/* Copyright (C) 2003 to 2011 Chris Vine

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301 USA

*/

#include <stdlib.h>  // include <stdlib.h> rather than <cstdlib> for mkstemp
#include <fstream>
#include <vector>
#include <algorithm>
#include <ios>
#include <ostream>
#include <locale>
#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#include <glib.h>

#include "socket_server.h"

#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/callback.h>
#include <c++-gtk-utils/mem_fun.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

#define SAVED_FAX_FILENAME  ".efax-gtk_queued_server_files"

#define BUFFER_LENGTH 1024


SocketServer::SocketServer(void): server_running(false),
				  serve_sock_fd(-1), count (0),
                                  working_dir(prog_config.working_dir),
				  filenames_s(new(FilenamesList)) {

  // we have included std::string working_dir member so that we can access it in the
  // socket server thread created by SocketServer::start() without having to
  // introduce locking of accesses to prog_config.working_dir by the main GUI thread
  // it is fine to initialise it in the initialisation list of this constructor
  // as prog_config.working_dir is only set once, on the first call to configure_prog()
  // (that is, when configure_prog() is passed false as its argument)


  fax_to_send.second = 0;

  stdout_notify.connect(Callback::make(*this, &SocketServer::write_stdout_dispatcher_cb));
  socket_error_notify.connect(Callback::make(*this, &SocketServer::cleanup));

  // make sure that we have the $HOME/efax-gtk-server directory
  // (we do not need to lock working_dir_mutex as the socket server thread cannot
  // be running at this point)
  std::string dir_name(working_dir);
  dir_name += "/efax-gtk-server";
  mkdir(dir_name.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

  // now populate the queued faxes from server list  
  read_queued_faxes();
}

void SocketServer::start(const std::string& port_, bool other_sock_client_address_, bool ipv6_) {

  if (!server_running) {
    port = port_;
    other_sock_client_address = other_sock_client_address_;
    ipv6 = ipv6_;
    server_running = true;

    // now block off the signals for which we have set handlers so that the socket server
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the socket server thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    thread =
      Thread::Thread::start(Callback::make(*this, &SocketServer::socket_thread),
			    true);
    if (!thread.get()) {
      write_error("Cannot start new socket thread, fax socket will not run\n");
      server_running = false;
    }
    // now unblock the signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
}

void SocketServer::socket_thread(void) {

  bool running;

  // we don't want this thread to be cancelled until we reach accept_on_client()
  // make scope block for Thread::CancelBlock object
  {
    Thread::CancelBlock cancel_block;
    if (ipv6) running = make_ipv6_socket();
    else running = make_ipv4_socket();

    if (running) {
      std::string message(gettext("Socket running on port "));
      message += port;
      message += '\n';
      write_stdout_from_thread(message.c_str());
    }
  } // end of CancelBlock block - thread can now be cancelled
    // wait for clients to connect, and continue
  while (accept_on_client());
}

bool SocketServer::make_ipv4_socket(void) {

  if ((serve_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    write_error("Cannot create IPv4 socket for socket server\n");
    socket_error_notify();
    return false;
  }

  sockaddr_in serve_address;
  std::memset(&serve_address, 0, sizeof(serve_address));

  serve_address.sin_family = AF_INET;
  if (other_sock_client_address) serve_address.sin_addr.s_addr = htonl(INADDR_ANY);
  else {
    int res = inet_pton(AF_INET, "127.0.0.1", &(serve_address.sin_addr));
    if (res <= 0) {
      write_error("Cannot create IPv4 network address for localhost\n");
      socket_error_notify();
      return false;
    }
  }
  serve_address.sin_port = htons(std::atoi(port.c_str()));
    
  if ((bind(serve_sock_fd, (sockaddr*)&serve_address, sizeof(serve_address))) < 0) {
    write_error("Cannot bind to socket for socket server\n");
    socket_error_notify();
    return false;
  }

  if ((listen(serve_sock_fd, 5)) < 0) {
    write_error("Cannot listen on socket of socket server\n");
    socket_error_notify();
    return false;
  }
  return true;
}

bool SocketServer::make_ipv6_socket(void) {

  if ((serve_sock_fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
    write_error("Cannot create IPv6 socket for socket server\n");
    socket_error_notify();
    return false;
  }

  sockaddr_in6 serve_address;
  std::memset(&serve_address, 0, sizeof(serve_address));

  serve_address.sin6_family = AF_INET6;
  if (other_sock_client_address) serve_address.sin6_addr = in6addr_any;
  else {
    int res = inet_pton(AF_INET6, "::1", &(serve_address.sin6_addr));
    if (res <= 0) {
      write_error("Cannot create IPv6 network address for localhost\n");
      socket_error_notify();
      return false;
    }
  }
  serve_address.sin6_port = htons(std::atoi(port.c_str()));
    
  if ((bind(serve_sock_fd, (sockaddr*)&serve_address, sizeof(serve_address))) < 0) {
    write_error("Cannot bind to socket for socket server\n");
    socket_error_notify();
    return false;
  }

  if ((listen(serve_sock_fd, 5)) < 0) {
    write_error("Cannot listen on socket of socket server\n");
    socket_error_notify();
    return false;
  }
  return true;
}

bool SocketServer::accept_on_client(void) {

  bool return_val = false;

  sockaddr_storage connect_address;
  std::memset(&connect_address, 0, sizeof(connect_address));
  socklen_t client_len = sizeof(connect_address);

  // the calls to pthread_testcancel() are a workaround for Linuxthreads -
  // with POSIX conforming threads the calls are unnecessary but harmless
  pthread_testcancel();
  // don't test for EINTR here - it is emitted by Linuxthreads if a thread cancellation
  // request is received and we do not use signals in this program any more now
  int connect_sock_fd = accept(serve_sock_fd, (sockaddr*)&connect_address, &client_len);
  pthread_testcancel();

  // we only want thread cancellation to occur while accept() is blocking above
  Thread::CancelBlock cancel_block;
  if (connect_sock_fd >= 0
      && is_valid_peer(connect_address, client_len)) {

    std::string filename;
    {
      Thread::Mutex::Lock lock(working_dir_mutex);
      filename = working_dir;
    }
    filename += "/efax-gtk-server/efax-gtk-server-XXXXXX";

    std::string::size_type size = filename.size() + 1;
    ScopedHandle<char*> tempfile_h(new char[size]);
    std::memcpy(tempfile_h.get(), filename.c_str(), size); // this will include the terminating '\0' in the copy
    int file_fd = mkstemp(tempfile_h.get());

    if (file_fd == -1) {
      write_error("Can't open temporary file for socket server\n"
		  "This fax will not be processed.  Is the disk full?\n");
      return_val = false;
    }
    else {
      read_socket(file_fd, connect_sock_fd, tempfile_h.get());
      while (close(file_fd) == -1 && errno == EINTR);
      return_val = true;
    }
    while (close(connect_sock_fd) == -1 && errno == EINTR);
  }

  else if (connect_sock_fd >= 0) {
    // if we have got here then the peer is invalid - reject it by
    // closing connect_sock_fd
    write_error("Invalid host tried to connect to the socket server\n");
    while (close(connect_sock_fd) == -1 && errno == EINTR);
    return_val = true;
  }

  else {
    // if we have got here, connect_sock_fd is in error condition (otherwise it
    // would have been picked up in the previous else_if blocks) - report the
    // problem and close the thread
    write_error("There is a problem with receiving a connection on the socket server\n"
		"Closing the socket\n");
    return_val = false;
  }
  if (!return_val) socket_error_notify();
  return return_val;
  // the CancelBlock object will release and allow thread cancellation
  // at this point
}

void SocketServer::read_socket(int file_fd, int connect_sock_fd, const char* file) {

  char buffer[BUFFER_LENGTH];
  ssize_t read_result;
  ssize_t write_result;
  ssize_t written;

  while ((read_result = read(connect_sock_fd, buffer, BUFFER_LENGTH)) > 0
	 || (read_result == -1 && errno == EINTR)) {
    if (read_result > 0) {
      written = 0;
      do {
	write_result = write(file_fd, buffer + written, read_result);
	if (write_result > 0) {
	  written += write_result;
	  read_result -= write_result;
	}
      } while (read_result && (write_result != -1 || errno == EINTR));
    }
  }

  if (!read_result) {                                   // normal close by client
  
    write_stdout_from_thread(gettext("Print job received on socket\n"));
    if (count < 9999) count++;
    else count = 1;
    
    add_file(file);
    filelist_changed_notify();

    Thread::Mutex::Lock lock(fax_to_send_mutex);
    // use a Thread::Cond object to avoid overwriting an earlier fax to send entry
    while (fax_to_send.second) fax_to_send_cond.wait(fax_to_send_mutex);

    fax_to_send.first = file;
    fax_to_send.second = count;
    fax_to_send_notify();
  }
  else write_error("Error in receiving print job on socket\n");
}

void SocketServer::stop(void) {

  // this should only be run in the main GUI thread - check (we can
  // use any convenient Notifier object for this)
  if (!socket_error_notify.in_main_thread()) {
    write_error("Call to SocketServer::stop() in other than the main thread\n");
    return;
  }

  if (server_running) {
    thread->cancel();
    cleanup();
  }
}

void SocketServer::cleanup(void) {

  // this should only be run in the main GUI thread, either by a call
  // by the Notifier object socket_error_notify or by a call from
  // SocketServer::stop() - check (we can use any convenient Notifier
  // object for this)
  if (!socket_error_notify.in_main_thread()) {
    write_error("Call to SocketServer::cleanup() in other than the main thread\n");
    return;
  }

  thread->join();

  // this method is running in the main GUI thread so use stdout_message directly
  stdout_message(gettext("Closing the socket\n"));

  if (serve_sock_fd >= 0) {
    shutdown(serve_sock_fd, SHUT_RDWR);
    while (close(serve_sock_fd) == -1 && errno == EINTR);
    serve_sock_fd = -1;
  }
  server_running = false;

  // the thread has ended so destroy the Thread object held by thread
  thread.reset();
}

void SocketServer::add_file(const std::string& filename) {

  Thread::Mutex::Lock lock(filenames_mutex);
  std::pair<std::string, unsigned int> file_item;
  file_item.first = filename;
  file_item.second = count;
  filenames_s->push_back(file_item);

  save_queued_faxes();
}
 
std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> > SocketServer::get_filenames(void) const {

  // we will use a SharedPtr, which automatically controls object life, to hold a Thread::Mutex::Lock
  // this tunnelling lock will automatically be released once the object using the FilenamesList
  // object has finished with it and the lock is deleted by the destructor of the last SharedPtr
  // holding it (namely when SocketListDialog::set_socket_list_rows() has completed its task)
  SharedPtr<Thread::Mutex::Lock> lock_s(new Thread::Mutex::Lock(filenames_mutex));
  return std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> >(filenames_s, lock_s);
}

std::pair<std::string, unsigned int> SocketServer::get_fax_to_send(void) {

  Thread::Mutex::Lock lock(fax_to_send_mutex);
  
  std::pair<std::string, unsigned int> return_val(fax_to_send);

  fax_to_send.second = 0;
  // release the Thread::Cond object if it is blocking
  fax_to_send_cond.signal();
  return return_val;
}  

int SocketServer::remove_file(const std::string& filename) {

  int return_val = 0;
  bool found_file = false;
  { // mutex lock scope block
    Thread::Mutex::Lock lock(filenames_mutex);
    FilenamesList::iterator iter;
    for (iter = filenames_s->begin(); !found_file && iter != filenames_s->end(); ++iter) {
      if (iter->first == filename) {
	filenames_s->erase(iter);
	found_file = true;
      }
    }
  } // end of mutex lock scope block
  if (!found_file) return_val = -1;
  else {
    // update the display in any socket list object
    filelist_changed_notify();
    // clean up by deleting the temporary file we created earlier
    // in accept_on_client() above
    unlink(filename.c_str());
    // write out the new list of queued fax files
    save_queued_faxes();
  }
  return return_val;
}

void SocketServer::read_queued_faxes(void) {

  // we do not need a mutex here as it is only called in
  // SocketServer::SocketServer() before a new thread
  // has been launched

  // we do not need to use working_dir_mutex here either, for the same reason
  std::string filename(working_dir);
  filename += "/" SAVED_FAX_FILENAME;
  
  std::ifstream filein(filename.c_str(), std::ios::in);

  if (filein) {

    filenames_s->clear(); // may sure the files list is empty

    std::string file_read;
    
    while (std::getline(filein, file_read)) {
      if (!file_read.empty() && file_read[0] != '#' && file_read[0] != '!') {

	// find the last '?' (in theory, mkstemp() could use it in the filename,
	// so there may be a preceding one, so use a reverse find)
	std::string::size_type pos = file_read.rfind('?');
	if (pos != std::string::npos
	    && file_read.size() > pos + 1) {
	  if (!access(file_read.substr(0, pos).c_str(), R_OK)) {    // file exists and can be read
	    std::pair<std::string, unsigned int> file_item;
	    // file_item.first is the filename
	    // file_item second is the print job number
	    file_item.first = file_read.substr(0, pos);
	    file_item.second = std::atoi(file_read.substr(pos + 1).c_str());
	    filenames_s->push_back(file_item);
	  }
	}
      }
      else if (file_read[0] == '!' && file_read.size() > 1) {
	count = std::atoi(file_read.substr(1).c_str());
      }
    }
  }
}

void SocketServer::save_queued_faxes(void) {

  // we do not need a filenames mutex here as this method is only called by
  // SocketServer::add_file() and SocketServer::remove_file()
  // which are already protected by a mutex

  std::string filename;
  {
    Thread::Mutex::Lock lock(working_dir_mutex);
    filename = working_dir;
  }
  filename += "/" SAVED_FAX_FILENAME;
  
  std::ofstream fileout(filename.c_str(), std::ios::out);
  if (!fileout) {
    std::string message("Can't open file ");
    message += filename;
    message += ", so the list of queued files can't be saved\n";
    write_error(message.c_str());
  }
  else {

#ifdef HAVE_STREAM_IMBUE
    fileout.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

    FilenamesList::const_iterator iter;
    for (iter = filenames_s->begin(); iter != filenames_s->end(); ++iter) {
      fileout << iter->first << '?' << iter->second << '\n';
    }
    fileout << '!' << count << '\n';
  }
}

void SocketServer::write_stdout_from_thread(const char* message) {

  // this should only be run in a thread other than the main GUI thread, or
  // we can have deadlocks - check
  if (stdout_notify.in_main_thread()) {
    write_error("Call to SocketServer::write_stdout_from_thread() from "
		"the main GUI thread\n");
    return;
  }

  Thread::Mutex::Lock lock(stdout_mutex);

  // use a Thread::Cond object to avoid overwriting an earlier message
  while (!stdout_text.empty()) stdout_cond.wait(stdout_mutex);

  stdout_text = message;
  stdout_notify();
}

void SocketServer::write_stdout_dispatcher_cb(void) {

  Thread::Mutex::Lock lock(stdout_mutex);

  stdout_message(stdout_text.c_str());
  stdout_text = "";

  // release the Thread::Cond object if it is blocking
  stdout_cond.signal();
}

bool SocketServer::is_valid_peer(const sockaddr_storage& address, socklen_t len) {

  bool return_val = false;
  std::vector<std::string> peer_names;

  char buffer[INET6_ADDRSTRLEN + 512]; // general buffer which must be at least INET6_ADDRSTRLEN in
                                       // size because we use it in inet_ntop() (INET6_ADDRSTRLEN
                                       // is bigger than INET_ADDRSTRLEN)

  // deal with the peer

  // get unqualified domain name of peer (if any)
  if (!getnameinfo((sockaddr*)&address, len,
		   buffer, sizeof(buffer),
		   0, 0, NI_NAMEREQD | NI_NOFQDN)) {
    peer_names.push_back(buffer);
  }
  // get fully qualified domain name of peer
  if (!getnameinfo((sockaddr*)&address, len,
		   buffer, sizeof(buffer),
		   0, 0, NI_NAMEREQD)) {
    peer_names.push_back(buffer);
  }
  // get full list of peer's addresses in numeric string format
  if (!getnameinfo((sockaddr*)&address, len,
		   buffer, sizeof(buffer),
		   0, 0, NI_NUMERICHOST)) {
    // now obtain all the numeric addresses of the connecting peer
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    if (ipv6)
      hints.ai_family = AF_INET6;
    else
      hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res;
    if (getaddrinfo(buffer, 0, &hints, &res)) {
      write_error("Cannot look up peer's numeric addresses\n");
    }
    else {
      for (addrinfo* tmp = res; tmp; tmp = tmp->ai_next) {
	void* addr;
	if (ipv6) {
	  sockaddr_in6* in = (sockaddr_in6*)tmp->ai_addr;
	  addr = &(in->sin6_addr);
        }
	else {
	  sockaddr_in* in = (sockaddr_in*)tmp->ai_addr;
	  addr = &(in->sin_addr);
        }
	if (inet_ntop(tmp->ai_family, addr, buffer, sizeof(buffer))) {
	  std::string tmp(normalise(buffer));
	  if (!tmp.empty()) peer_names.push_back(tmp);
	}
      }
      freeaddrinfo(res);
    }   
  }
  else {
    write_error("Cannot obtain numeric address of peer\n");
  }

  // now generate the valid names

  std::vector<std::string> valid_names;
  valid_names.push_back("localhost");           // localhost is always valid
  if (ipv6) {
    valid_names.push_back("0000:0000:0000:0000:0000:0000:0000:0001"); // we compare on expanded addresses
  }
  else {
    valid_names.push_back("127.0.0.1");         // ditto
  }

  if (gethostname(buffer, sizeof(buffer)) < 0) {
    // we probably don't need to emit a warning with IPv6
    if (!ipv6) write_error("Cannot get our hostname\n");
  }
  else {
    buffer[sizeof(buffer) - 1] = 0;  // make sure it is null terminated
    valid_names.push_back(buffer);   // our own hostname is also always valid
    
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    if (ipv6) 
      hints.ai_family = AF_INET6;
    else
      hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res;
    if (!getaddrinfo(buffer, 0, &hints, &res)) {
      for (addrinfo* tmp = res; tmp; tmp = tmp->ai_next) {
	void* addr;
	if (tmp->ai_family == AF_INET6) {
	  sockaddr_in6* in = (sockaddr_in6*)tmp->ai_addr;
	  addr = &(in->sin6_addr);
        }
	else {
	  sockaddr_in* in = (sockaddr_in*)tmp->ai_addr;
	  addr = &(in->sin_addr);
        }
	if (inet_ntop(tmp->ai_family, addr, buffer, sizeof(buffer))) {
	  std::string tmp(normalise(buffer));
	  if (!tmp.empty()) valid_names.push_back(tmp);
	}
      }
      freeaddrinfo(res);
    }
  }

  // and now add the permitted clients from the settings dialog

  // lock the Prog_config object to stop it being modified or accessed in the initial
  // (GUI) thread while we are accessing it here
  Thread::Mutex::Lock lock(*prog_config.mutex_p);

  // if there is a bad IPv6 address this will cause an empty string to be inserted
  // but that's OK as addr_compare() checks for that
  std::transform(prog_config.permitted_clients_list.begin(),
		 prog_config.permitted_clients_list.end(),
		 std::back_inserter(valid_names),
		 Cgu::MemFun::make(*this, &SocketServer::normalise_checked));

  std::vector<std::string>::const_iterator peer_names_iter;
  std::vector<std::string>::const_iterator valid_names_iter;
  for (peer_names_iter = peer_names.begin();
       !return_val && peer_names_iter != peer_names.end();
       ++peer_names_iter) {
    for (valid_names_iter = valid_names.begin();
	 !return_val && valid_names_iter != valid_names.end();
	 ++valid_names_iter) {
      return_val = addr_compare(*valid_names_iter, *peer_names_iter);
    }
  }
  return return_val;
}

bool SocketServer::addr_compare(const std::string& wild_str, const std::string& in_str) {

  // check pre-conditions
  if (wild_str.empty()) return false;

  if (wild_str[wild_str.size() - 1] == '*'
      && wild_str.find_first_not_of("0123456789abcdefABCDEF.:*") == std::string::npos) {

    // take trailing '*' characters out of the comparison
    std::string::size_type length = wild_str.size();
    do {
      length--;
    } while (length > 0 && wild_str[length - 1] == '*');

    return wild_str.substr(0, length) == in_str.substr(0, length);
  }
  return wild_str == in_str;
}

std::string SocketServer::ascii_to_lower(const std::string& in) {
  std::locale loc("C");
  Cgu::GcharScopedHandle str(g_strdup(in.c_str()));
  std::use_facet<std::ctype<char> >(loc).tolower((char*)str.get(),
						 (char*)str.get() + in.size());
  return std::string((char*)str.get());
}

std::string SocketServer::normalise(const std::string& in) {

  if (!ipv6) return in;

  // if there is a wildcard, the '::' compression feature may not be used (but
  // other forms of 0 suppression are OK)

  // cursory first-sift check whether it appears to be a valid ipv6 address
  // (this is mainly to ensure that the following implementation doesn't do
  // anything too stupid, such as pass non-ASCII characters to ascii_to_lower(),
  // than as a real check)
  if (in.empty()
      || in.size() > 45  // big enough for IPv4 mapped IPv6 addresses
      || in.find_first_not_of("0123456789abcdefABCDEF:.*") != std::string::npos) {
    write_error("Invalid IPv6 address entered\n");
    return std::string();
  }

  std::size_t partition = in.find("::");
  bool wildcard = in[in.size() - 1] == '*';
  if (partition != std::string::npos && wildcard) {
    write_error("Invalid wildcard supplied: cannot use wildcard with '::'\n");
    return std::string();
  }

  // preconditions checked: now proceed

  // deal with first part (this will never be entered if we have a
  //  wildcard, begin with '::' or have no '::')
  std::size_t start = 0; // first nibble of address word
  std::size_t end;       // one past last nibble of address word

  std::string part1;
  if (partition                            // the address doesn't begin with '::'
      && partition != std::string::npos) { // but we have a '::'
    for (;;) {
      bool finished = false;
      end = in.find(':', start);
      if (end == partition) {
	finished = true;
      }
      for (int missing = 4 - static_cast<int>(end - start);
	   missing > 0; --missing) {
	part1 += '0';
      }
      for (; start < end; ++start) {
	part1 += in[start];
      }
      if (!finished) {
	part1 += ':';
	start = end;
	++start;
      }
      else {
	start = end + 2;           // jump over '::' for next if block
	break;
      }
    }
  }

  // deal with second part
  int ipv4_count = 0; // IPv4-mapped IPv6 addresses consume the last two 16 bit
                      // words for the IPv4 address and are of indeterminate length
  std::string part2;
  if (!partition) start = 2;       // address begins with '::' - jump over it
  if (start != in.size()) {        // the address doesn't end with '::'
    for (;;) {
      bool finished = false;
      end = in.find(':', start);
      if (end == std::string::npos) {
	finished = true;
	end = in.size();           // one past last character
	if (in.find('.', start) != std::string::npos)
	  ipv4_count = end - start;    // number of characters in IPv4 address
      }
      if (!wildcard || !finished) {    // don't expand last element before wild card
	for (int missing = 4 - static_cast<int>(end - start);
	     missing > 0; --missing) { // The IPv4 address of IPv4-mapped IPv6 addresses will
	                               // always occupy more than 4 characters so this will
                                       // be passed over without a further test being necessary
	  part2 += '0';
	}
      }
      for (; start < end; ++start) {
	part2 += in[start];
      }
      if (!finished) {
	part2 += ':';
	start = end;
	++start;
      }
      else break;
    }
  }

  // insert part1 in result
  std::string result;
  if (!part1.empty()) {
    result += ascii_to_lower(part1);
    result += ':'; // this will always be correct: if there is no '::' then part1 is empty.
                   // If there is a '::', there is more to come
  }

  // now deal with the '::' expansion
  if (!wildcard) {
    int total;
    // 40 not 39 - if the address ends with :: we only have part 1, which is an 
    // exact multiple of 5 in size, and an empty part 2.  Discarding any remainder
    // (which will be 1 if part 2 has something in it) is therefore correct.  (It
    // could in fact be anything between 40 and 43.)
    if (!ipv4_count) total = 40;
    // for IPv4 mapping we substitute the IPv4 address for the last 2 words, and
    // have 30 not 29 because the IPv6 element in part1 and part2 will always sum
    // to a whole multiple of 5 in size with or without '::'
    else total = 30;
    for (int missing = (total - static_cast<int>(result.size() + part2.size() - ipv4_count))/5;
	 missing > 0;
       --missing) {
      result += "0000";
      if (missing > 1
	  || !part2.empty())  // part2 cannot be empty for IPv4 mapped addresses
	result += ':';
    }
  }

  // add part2 in result
  result += ascii_to_lower(part2);
  return result;
}

// for IPv6 addresses which could be in either numeric or domain name
// form (we might get these from prog_config.permitted_clients_list)
std::string SocketServer::normalise_checked(const std::string& in) {
  // check to see if 'in' is in domain name format - if so just return
  // it (as it happens, this will also sift out ipv4 addresses with no
  // wildcard): check also for '*' because if there is a wildcard
  // there might only be one or no ':' in a valid IPv6 address -
  // addresses in domain name format should not have wildcards
  if (std::count(in.begin(), in.end(), ':') < 2
      && in.find('*') == std::string::npos) return in;
  return normalise(in);
}
