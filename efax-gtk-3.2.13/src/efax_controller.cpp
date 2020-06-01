/* Copyright (C) 2001 to 2010 Chris Vine

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

#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <fstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <ios>
#include <ostream>
#include <sstream>

#include <glib.h>

#include "efax_controller.h"
#include "fax_list_manager.h"
#include "settings.h"

#include <c++-gtk-utils/thread.h>
#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/callback.h>
#include <c++-gtk-utils/mem_fun.h>
#include <c++-gtk-utils/convert.h>
#include <c++-gtk-utils/timeout.h>
#include <c++-gtk-utils/cgu_config.h>

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// there can be certain circumstances where PIPE_BUF is not
// defined in <limits.h>.  If so, just define the minimum
// required by POSIX
#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

// the headers on one of my machines does not properly include the declaration
// of getpgid() for some reason, so declare it here explicitly
extern "C" pid_t getpgid(pid_t);


EfaxController::EfaxController(const int size): standard_size(size),
						state(inactive),
						close_down(false),
						received_fax_count(0),
						redial_queue(standard_size) {
  // set up state_messages

  state_messages.push_back(gettext("Inactive"));
  state_messages.push_back(gettext("Sending fax"));
  state_messages.push_back(gettext("Answering call"));
  state_messages.push_back(gettext("Answering call"));
  state_messages.push_back(gettext("Standing by to receive calls"));
  state_messages.push_back(gettext("Sending fax"));
  state_messages.push_back(gettext("Sending fax"));
}

void EfaxController::efax_closedown(void) {
  if (state == inactive) ready_to_quit_notify();
  else if (!close_down) {
    close_down = true;
    stop();
  }
}

void EfaxController::init_sendfax_parms(void) {

  // we do not need a mutex when accessing efax_parms_vec.  It is only
  // accessed in init_sendfax_parms() and init_receive_parms() (in the
  // GUI thread) followed by sendfax_thread() or receive_thread() (in
  // a worker thread) and cannot then be accessed by any thread until
  // child_ended_cb() has been called via Callback::post(), which will
  // have forced memory synchronisation
  efax_parms_vec = prog_config.parms;

  // now add the first set of arguments to the copy of prog_config.parms
  // in efax_parms_vec
     
  struct std::tm* time_p;
  std::time_t time_count;

  std::time(&time_count);
  time_p = std::localtime(&time_count);

  char date_string[150];
  // create the date and time in an internationally acceptable
  // numeric (ASCII) only character format which efax can make sense of
  const char format[] = "%Y-%m-%d %H:%M";
  std::strftime(date_string, sizeof(date_string), format, time_p);

  std::string temp("-h");
  temp += date_string;
  temp += "    ";
  temp += prog_config.my_name;

  temp += " (";
  temp += prog_config.my_number + ") --> ";
  if (prog_config.addr_in_header) temp += last_fax_item_sent.number;
  temp += "      %d/%d";
  efax_parms_vec.push_back(temp);

  if (last_fax_item_sent.number.empty()) {
    efax_parms_vec.push_back("-jX3");
    efax_parms_vec.push_back("-t");
    efax_parms_vec.push_back("");
  }
  else {
    temp = "-t";
    if (prog_config.tone_dial) temp += 'T';
    else temp += 'P';
    temp += last_fax_item_sent.number;
    efax_parms_vec.push_back(temp);
  }
}

void EfaxController::sendfax_impl(const Fax_item& fax_item,
				  bool in_child_ended_cb,
				  bool in_redial_cb) {

  if (state == receive_standby) {
    if (!is_receiving_fax()) {
      last_fax_item_sent = fax_item;
      state = start_send_on_standby;
      kill_child();
    }
    else {
      if (in_redial_cb) { // just start another timeout
	start_redial_timeout(fax_item);
      }
      else {
	if (prog_config.redial) {
	  write_error(gettext("Cannot send fax - the modem is in use.\n"
			      "Adding the fax to the redial queue\n"));
	  start_redial_timeout(fax_item);
	}
	else {
	  write_error(gettext("Cannot send fax - a fax is being received\n"));
	}
      }
    }
  }

  // the in_child_ended_cb argument is a guard.  It stops a user, by pressing the
  // Send button, from starting a new fax send in start_send_on_standby mode
  // before the listening child process has terminated and been cleaned up: we
  // we can only do that via EfaxController::child_ended_cb()
  else if (state == inactive || (in_child_ended_cb && state == start_send_on_standby)) {
    stdout_pipe.open(PipeFifo::non_block);
    // efax is sensitive to timing, so set pipe write to non-block also
    stdout_pipe.make_write_non_block();
    
    if (state == inactive) {
      state = sending;
      last_fax_item_sent = fax_item;
    }
    // else state == start_send_on_standby - we don't need to assign to
    // last_fax_item_sent in this case as it has already been done in the
    // 'if' block opening this function on the previous call to this function
    else state = send_on_standby;

    display_state();

    // get the first set of arguments for the exec() call in sendfax_thread (because
    // this is a multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    // the arguments are loaded into the EfaxController object member efax_parms_vec
    init_sendfax_parms();

    // now launch a worker thread to make up the fax pages in tiffg3 format
    // for sending by efax - if the fax is correctly made up, the worker thread
    // will invoke efax and sends the fax, and when the fax has been sent (or
    // sending has failed) it will post child_ended_cb() to clean up, which will
    // execute in this initial (GUI) thread.  If the fax is not correctly make
    // up, the worker thread will post no_fax_made_cb() which will execute in
    // this initial (GUI) thread so as to clean up correctly

    // first block off the signals for which we have set handlers so that the worker
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the worker thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    if (!Thread::Thread::start(Callback::make(*this, &EfaxController::sendfax_thread, in_redial_cb),
			       false).get()) {
      write_error("Cannot start thread to make fax for sending\n");
      stdout_pipe.close();
      state = inactive;
      display_state();
    }
    // now unblock signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
  else {
    beep();
    if (prog_config.redial) { // we cannot reach here via redial_cb()
                              // (state must be 'receive_standby' or
                              // 'inactive' to do that) so this is the
                              // first attempt to send
      write_error(gettext("Cannot send fax - the modem is in use.\n"
			  "Adding the fax to the redial queue\n"));
      start_redial_timeout(fax_item);
    }
  }
}

void EfaxController::no_fax_made_cb(void) {

  cleanup_fax_send_fail();
  stdout_pipe.close();
  State state_val = state;
  state = inactive;
  if (state_val == send_on_standby) receive(receive_standby);
  // we don't need to call display_state() if we have called receive(), as
  // receive() will call display_state() itself
  else display_state();
}

std::pair<const char*, char* const*> EfaxController::get_gs_parms(const std::string& basename) {

  std::vector<std::string> parms;

  { // scope block for mutex lock
    // lock the Prog_config object to stop it being modified in the intial (GUI) thread
    // while we are accessing it here
    Thread::Mutex::Lock lock(*prog_config.mutex_p);

    std::string temp;
    parms.push_back("gs");
    parms.push_back("-q");
    parms.push_back("-sDEVICE=tiffg3");
    parms.push_back("-dMaxStripSize=0");
    temp = "-r";
    temp += prog_config.resolution;
    parms.push_back(temp);
    parms.push_back("-dNOPAUSE");
    parms.push_back("-dSAFER");
    temp = "-sOutputFile=";
    temp += basename + ".%03d";
    parms.push_back(temp);
    temp = "-sPAPERSIZE=";
    temp += prog_config.page_size;
    parms.push_back(temp);
    parms.push_back(basename);
  }

  char** exec_parms = new char*[parms.size() + 1];

  std::vector<std::string>::const_iterator iter;
  char**  temp_pp = exec_parms;
  for (iter = parms.begin(); iter != parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }

  *temp_pp = 0;
  
  char* prog_name = new char[std::strlen("gs") + 1];
  std::strcpy(prog_name, "gs");

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void EfaxController::sendfax_thread(bool redialling) {
  // convert the postscript file(s) into tiffg3 fax files, beginning at [filename].001
  // we will use ghostscript.  we will also load the results into efax_parms_vec
  try {
    std::for_each(last_fax_item_sent.file_list.begin(),
		  last_fax_item_sent.file_list.end(),
		  MemFun::make(*this, &EfaxController::ps_file_to_tiff_files));
  }
  catch (const std::string& msg) {
    // cause no_fax_made_cb to be executed in the initial (GUI) thread
    // for clean up and then end this worker thread

    // it is safe not to use a Releaser object here because the MainWindow
    // object, and so this object, exists throughout execution of the glib
    // main loop

    // if we are redialling a fax received from the print system
    // and that fax has been deleted via the socket list dialog,
    // then just treat it as cancelled and don't print a diagnostic
    if (!redialling || !last_fax_item_sent.is_socket_file)
      write_error(msg.c_str());
    Callback::post(Callback::make(*this, &EfaxController::no_fax_made_cb));
    return;
  }
  // we have now made the fax pages in tiffg3 format and entered
  // them into efax_parms_vec.

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> exec_parms(get_efax_parms());

  // set up a synchronising pipe in case the child process finishes before
  // fork() in parent space has returned (yes, with an exec() error that can
  // happen with Linux kernel 2.6)
  SyncPipe sync_pipe;

  pid_t pid = fork();
  if (pid == -1) {
    write_error("Fork error - exiting\n");
    std::exit(FORK_ERROR);
  }
  if (!pid) {  // child process - as soon as everything is set up we are going to do an exec()

    // unblock signals as these are blocked for all worker threads
    // (the child process inherits the signal mask of the thread
    // creating it with the fork() call)
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    // this child process is single threaded, so we can use sigprocmask()
    // rather than pthread_sigmask() (and should do so as sigprocmask()
    // is guaranteed to be async-signal-safe)
    // this process will not be receiving interrupts so we do not need
    // to test for EINTR on the call to sigprocmask()
    sigprocmask(SIG_UNBLOCK, &sig_mask, 0);

    // now we have forked, we can connect stdout_pipe to stdout
    // and connect MainWindow::error_pipe to stderr
    stdout_pipe.connect_to_stdout();
    connect_to_stderr();

    // wait before we call execvp() until the parent process has set itself up
    // and releases this process
    sync_pipe.wait();

    execvp(exec_parms.first, exec_parms.second);

    // if we reached this point, then the execvp() call must have failed
    // report error and exit - uses _exit() and not exit()
    write_error("Can't find the efax-0.9a program - please check your installation\n"
		"and the PATH environmental variable\n");
    _exit(EXEC_ERROR); 
  } // end of child process
    
  // this is the parent process

  { // use a mutex because we may access prog_config.efax_controller_child_pid
    // in kill_child() before a call to Callback::post() has forced memory
    // synchronisation
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    prog_config.efax_controller_child_pid = pid;
  }

  stdout_pipe.make_readonly();   // since the pipe is unidirectional, we can close the write fd
  join_child();

  // now we have set up, release the child process
  sync_pipe.release();
    
  // release the memory allocated on the heap for
  // the redundant exec_parms pair
  // we are in the main parent process here - no worries about
  // only being able to use async-signal-safe functions
  delete_parms(exec_parms);

  // now wait for the child process to terminate so we can post to child_ended_cb()
  pid_t ret;
  int status;
  do {
    ret = waitpid(pid, &status, 0);
  } while (ret == -1 && errno == EINTR);

  int exit_code = -1;
  if (ret != -1 && WIFEXITED(status))
    exit_code = WEXITSTATUS(status);
  
  Callback::post(Callback::make(*this, &EfaxController::child_ended_cb, exit_code));
}

// throws bool if there is an error
void EfaxController::ps_file_to_tiff_files(const std::string& filename) {

  std::string::size_type pos = filename.find_last_of('/');

  if (pos == std::string::npos || pos + 2 > filename.size()) {
    throw std::string(gettext("Not valid file name\n"));
  }
    
  else if (access(filename.c_str(), F_OK)) {
    throw std::string(gettext("File does not exist\n"));
  }

  else if (access(filename.c_str(), R_OK)) {
    throw std::string(gettext("User does not have read permission on the file\n"));
  }

  // unfortunately ghostscript does not handle long file names
  // so we need to separate the file name from the full path (we will chdir() to the directory later)
  // pos is already set to the position of the last '/' character
  std::string dirname(filename.substr(0, pos));
  pos++;
  std::string basename(filename.substr(pos));

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> gs_parms(get_gs_parms(basename));

  pid_t pid = fork();
    
  if (pid == -1) {
    write_error("Fork error\n");
    std::exit(FORK_ERROR);
  }
  if (!pid) { // child process

    // unblock signals as these are blocked for all worker threads
    // (the child process inherits the signal mask of the thread
    // creating it with the fork() call)
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    // this child process is single threaded, so we can use sigprocmask()
    // rather than pthread_sigmask() (and should do so as sigprocmask()
    // is guaranteed to be async-signal-safe)
    // this process will not be receiving interrupts so we do not need
    // to test for EINTR on the call to sigprocmask()
    sigprocmask(SIG_UNBLOCK, &sig_mask, 0);

    connect_to_stderr();

    int fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
      write_error("Cannot open /dev/null in EfaxController::ps_file_to_tiff_files()\n");
      // in case of error end child process here
      _exit(FILEOPEN_ERROR);
    }
    dup2(fd, 0);
    // now close stdout
    dup2(fd, 1);
    close(fd); // now stdin and stdout read/write to /dev/null, we can close the /dev/null file descriptor

    // unfortunately ghostscript does not handle long file names
    // so we need to chdir()
    chdir(dirname.c_str());
    execvp(gs_parms.first, gs_parms.second);

    // if we reached this point, then the execvp() call must have failed
    write_error("Can't find the ghostscript program - please check your installation\n"
		"and the PATH environmental variable\n");
    // this child process must end here - use _exit() not exit()
    _exit(EXEC_ERROR); 
  } // end of child process

  // this is the parent process

  // release the memory allocated on the heap for
  // the redundant gs_parms
  // we are in the main parent process here - no worries about
  // only being able to use async-signal-safe functions
  delete_parms(gs_parms);

  // wait for the child process to terminate
  pid_t ret;
  int status;
  do {
    ret = waitpid(pid, &status, 0);
  } while (ret == -1 && errno == EINTR);

  if (ret == -1 || !WIFEXITED(status)) {
    throw std::string(gettext("Not valid postscript/PDF file\n"));
  }

  // now enter the names of the created files in efax_parms_vec
  bool valid_file = false;
  int partnumber = 1;
  std::ostringstream strm;

#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

  strm << filename << '.' << std::setfill('0')
       << std::setw(3) << partnumber;
  int result = access(strm.str().c_str(), R_OK);

  while (!result) {  // file OK
    // valid_file only needs to be set true once, but it is more
    // convenient to do it in this loop
    valid_file = true;

    // we do not need a mutex when accessing efax_parms_vec.  It is only
    // accessed in init_sendfax_parms() and init_receive_parms() (in the
    // GUI thread) followed by sendfax_thread() or receive_thread() (in
    // a worker thread) and cannot then be accessed by any thread until
    // child_ended_cb() has been called via Callback::post(), which will
    // have forced memory synchronisation
    efax_parms_vec.push_back(strm.str());

    partnumber++;
    strm.str("");
    strm << filename << '.' << std::setfill('0')
	 << std::setw(3) << partnumber;
    result = access(strm.str().c_str(), R_OK);
  }
    
  if (!valid_file) {
    throw std::string(gettext("Not valid postscript/PDF file\n"));
  }
}

void EfaxController::init_receive_parms(State mode) {

  // we do not need a mutex when accessing efax_parms_vec.  It is only
  // accessed in init_sendfax_parms() and init_receive_parms() (in the
  // GUI thread) followed by sendfax_thread() or receive_thread() (in
  // a worker thread) and cannot then be accessed by any thread until
  // child_ended_cb() has been called via Callback::post(), which will
  // have forced memory synchronisation
  efax_parms_vec = prog_config.parms;

  // now add the first set of arguments to the copy of prog_config.parms
  // in efax_parms_vec
     
  efax_parms_vec.push_back("-rcurrent");

  if (mode == receive_takeover) efax_parms_vec.push_back("-w");
      
  else if (mode == receive_standby) {
    std::string temp = "-jS0=";
    temp += prog_config.rings;
    efax_parms_vec.push_back(temp);
    efax_parms_vec.push_back("-s");
    efax_parms_vec.push_back("-w");
  }
}

void EfaxController::receive(State mode) {

  // check pre-conditions
  if (state != inactive) {
    beep();
    return;
  }
  if (prog_config.working_dir.empty()) {
    write_error(gettext("You don't have the $HOME environmental variable set\n"));
    beep();
    return;
  }

  // now proceed to put the program in receive mode
  std::string fax_pathname(prog_config.working_dir);
  fax_pathname += "/faxin/current";
  ScopedHandle<char*> fax_pathname_h(new char[fax_pathname.size() + 1]);
  std::strcpy(fax_pathname_h.get(), fax_pathname.c_str());

  if (mkdir(fax_pathname_h.get(), S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save received faxes.\n"
			"Faxes cannot be received!\n"));
  }
  else {

    stdout_pipe.open(PipeFifo::non_block);
    // efax is sensitive to timing, so set pipe write to non-block also
    stdout_pipe.make_write_non_block();
  
    state = mode;
    display_state();

    // get the first set of arguments for the exec() call in receive_thread (because
    // this is a multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    // the arguments are loaded into the EfaxController object member efax_parms_vec
    init_receive_parms(mode);

    // now launch a worker thread to invoke efax and receive a fax.  When a fax
    // has been sent (or a receiving session has been stopped or failed) it will
    // post child_ended_cb() to clean up, which will execute in this initial (GUI)
    // thread.

    // first block off the signals for which we have set handlers so that the worker
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the worker thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    const char* thread_arg = fax_pathname_h.release();
    if (!Thread::Thread::start(Callback::make(*this, &EfaxController::receive_thread,
					      thread_arg),
			       false).get()) {
      write_error("Cannot start thread to receive fax\n");
      stdout_pipe.close();
      state = inactive;
      delete thread_arg;
      display_state();
    }
    // now unblock signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
}

void EfaxController::receive_thread(const char* pathname_p) {

  ScopedHandle<const char*> pathname_h(pathname_p);

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> exec_parms(get_efax_parms());

  // set up a synchronising pipe  in case the child process finishes before
  // fork() in parent space has returned (yes, with an exec() error that can
  // happen with Linux kernel 2.6)
  SyncPipe sync_pipe;

  pid_t pid = fork();
  if (pid == -1) {
    write_error("Fork error - exiting\n");
    std::exit(FORK_ERROR);
  }
  if (!pid) {  // child process - as soon as everything is set up we are going to do an exec()

    // unblock signals as these are blocked for all worker threads
    // (the child process inherits the signal mask of the thread
    // creating it with the fork() call)
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    // this child process is single threaded, so we can use sigprocmask()
    // rather than pthread_sigmask() (and should do so as sigprocmask()
    // is guaranteed to be async-signal-safe)
    // this process will not be receiving interrupts so we do not need
    // to test for EINTR on the call to sigprocmask()
    sigprocmask(SIG_UNBLOCK, &sig_mask, 0);

    // now we have forked, we can connect stdout_pipe to stdout
    // and connect MainWindow::error_pipe to stderr
    stdout_pipe.connect_to_stdout();
    connect_to_stderr();

    chdir(pathname_h.get());

    // wait before we call execvp() until the parent process has set itself up
    // and releases this process
    sync_pipe.wait();

    execvp(exec_parms.first, exec_parms.second);

    // if we reached this point, then the execvp() call must have failed
    // report the error and end this process - use _exit() and not exit()
    write_error("Can't find the efax-0.9a program - please check your installation\n"
		"and the PATH environmental variable\n");
    _exit(EXEC_ERROR); 
  } // end of child process

  // this is the parent process

  { // use a mutex because we may access prog_config.efax_controller_child_pid
    // in kill_child() before a call to Callback::post() has forced memory
    // synchronisation
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    prog_config.efax_controller_child_pid = pid;
  }

  stdout_pipe.make_readonly();   // since the pipe is unidirectional, we can close the write fd
  join_child();

  // now we have set up, release the child process
  sync_pipe.release();

  // release the memory allocated on the heap for
  // the redundant exec_parms pair
  // we are in the main parent process here - no worries about
  // only being able to use async-signal-safe functions
  delete_parms(exec_parms);

  // now wait for the child process to terminate so we can post to child_ended_cb()
  pid_t ret;
  int status;
  do {
    ret = waitpid(pid, &status, 0);
  } while (ret == -1 && errno == EINTR);

  int exit_code = -1;
  if (ret != -1 && WIFEXITED(status))
    exit_code = WEXITSTATUS(status);
  
  Callback::post(Callback::make(*this, &EfaxController::child_ended_cb, exit_code));
}

std::pair<const char*, char* const*> EfaxController::get_efax_parms(void) {

  // efax_parms_vec has already been filled by init_sendfax_parms() and
  // ps_file_to_tiff_files() if sending, or by init_receive_parms() if
  // receiving - so make up the C style arrays for the execvp() call in
  // sendfax_thread() and receive_thread()
  char** exec_parms = new char*[efax_parms_vec.size() + 1];

  std::vector<std::string>::const_iterator iter;
  char**  temp_pp = exec_parms;
  for (iter = efax_parms_vec.begin(); iter != efax_parms_vec.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }

  *temp_pp = 0;

  char* prog_name = new char[std::strlen("efax-0.9a") + 1];
  std::strcpy(prog_name, "efax-0.9a");

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void EfaxController::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

void EfaxController::stop(void) {
 if (state != inactive) {
   stdout_message(gettext("\n*** Stopping send/receive session ***\n\n"));
   kill_child();
 }
  else beep();
}

void EfaxController::child_ended_cb(int exit_code) {

  // if a fax list is in the process of being constructed, we need to wait until
  // construction has completed before acting on efax exit status, so reload the
  // event in the glib main loop (we might have reached here through the call to
  // gtk_main_iteration() in FaxListManager::populate_fax_list())
  if (FaxListManager::is_fax_received_list_main_iteration()
      || FaxListManager::is_fax_sent_list_main_iteration()) {
    Callback::post(Callback::make(*this, &EfaxController::child_ended_cb, exit_code));
    return;
  }

  // preconditions are OK - proceed

  // we don't need a mutex here - memory will have synchronised through the
  // call to Callback::post() and we don't now have a worker thread running
  prog_config.efax_controller_child_pid = 0;

  // this won't work if efax is suid root and we are not root
  if (!prog_config.lock_file.empty()) unlink(prog_config.lock_file.c_str());
  
  bool restart_standby = false;
  bool end_receive = false;

  switch(state) {

  case EfaxController::sending:
  case EfaxController::send_on_standby:
    if (!exit_code) {
      std::pair<std::string, std::string> fax_info(save_sent_fax());
      // notify that the fax has been sent
      if (!fax_info.first.empty()) fax_sent_notify(fax_info);

      if (last_fax_item_sent.is_socket_file) {
	// if we are sending a print job via the socket server, notify the
	// SocketServer object which created the temporary file (the server
	// object will clean up by deleting it and also cause the socket
	// file list to be updated - we do not need to do that here)
	remove_from_socket_server_filelist(last_fax_item_sent.file_list[0]);
      }
    }
    else {
      cleanup_fax_send_fail();
      if (exit_code == 1 && prog_config.redial) start_redial_timeout(last_fax_item_sent);
    }
    unjoin_child();
    {
      State state_val = state;
      state = inactive;
      if (state_val == send_on_standby
	  && !close_down) {
	receive(receive_standby);
      }
      // we do not need to call display_state() if we have called receive(), as
      // receive() will call display_state() itself
      else display_state();
    }
    break;
  case EfaxController::start_send_on_standby:
    receive_cleanup();
    unjoin_child();
    if (!close_down) sendfax_impl(last_fax_item_sent, true, false);
    else {
      state = inactive;
      display_state();
    }
    break;
  case EfaxController::receive_standby:
    if ((!exit_code || exit_code == 3)
	&& !close_down) {
      restart_standby = true;
    }
    else end_receive = true;
    break;
  default:
    if (state != inactive) end_receive = true;
    break;
  }

  if (end_receive || restart_standby) {
    receive_cleanup();
    unjoin_child();
    state = inactive; // this is needed even if we are going to call receive()
    // now restart if in standby mode
    if (restart_standby) receive(receive_standby);
    // we do not need to call display_state() if we have called receive(), as
    // receive() will call display_state() itself (calling display_state() will
    // also update the received fax count displayed in the tray icon tooltip if we are
    // in receive_standby mode - this will appear in the tray_item tooltip, because
    // TrayIcon::set_tooltip_cb() is connected to the write_state signal in the
    // MainWindow::MainWindow() constructor)
    else display_state();
    restart_standby = false;
    end_receive = false;
  }

  if (close_down && state == inactive) ready_to_quit_notify();
}

void EfaxController::receive_cleanup(void) {
  // delete the "current" receive directory if it is empty
  std::string full_dirname(prog_config.working_dir);
  full_dirname += "/faxin/current";
  int result = rmdir(full_dirname.c_str()); // only deletes the directory if it is empty

  if (result == -1 && errno == ENOTEMPTY) {
    // before assuming a fax has been successfully received, check to see
    // if current.001 is of 0 size (if it is delete it)
    struct stat statinfo;
    std::string file_name(full_dirname);
    file_name += "/current.001";
    if (!stat(file_name.c_str(), &statinfo)
	&& !statinfo.st_size) {
      unlink(file_name.c_str());
      rmdir(full_dirname.c_str());
    }
    else { // we have received a fax - save it
      std::pair<std::string, std::string> fax_info(save_received_fax());
      // notify that the fax has been received
      if (!fax_info.first.empty()) {
	received_fax_count++;
	fax_received_notify(fax_info);
      }
    }
  }
}

std::pair<std::string, std::string> EfaxController::save_received_fax(void) {

  char faxname[18];
  *faxname = 0;

  // get a time value to create the directory name into which the fax is to be saved
  struct std::tm* time_p;
  std::time_t time_count;
    
  std::time(&time_count);
  time_p = std::localtime(&time_count);

  std::string new_dirname(prog_config.working_dir);
  new_dirname += "/faxin/";

  const char format[] = "%Y%m%d%H%M%S";
  std::strftime(faxname, sizeof(faxname), format, time_p);
  std::string temp(new_dirname);
  temp += faxname;

  // check whether directory already exists or can't be created
  int count;
  for (count = 0; count < 4 && mkdir(temp.c_str(), S_IRUSR | S_IWUSR | S_IXUSR); count++) {
    // it is not worth checking for a short sleep because of a signal as we will
    // repeat the attempt four times anyway
    sleep(1); // wait a second to get a different time
    std::time(&time_count);
    time_p = std::localtime(&time_count);
    std::strftime(faxname, sizeof(faxname), format, time_p);
    temp = new_dirname + faxname;
  }
  if (count == 4) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save received fax.\n"
			"This fax will not be saved!\n"));
    *faxname = 0;
  }

  else { // move the faxes into the new directory

    // make new_dirname the same as the directory name for the saved faxes
    // that we have just created
    new_dirname += faxname;

    std::string old_dirname(prog_config.working_dir);
    old_dirname += "/faxin/current";

    std::vector<std::string> filelist;
    struct dirent* direntry;
    struct stat statinfo;

    DIR* dir_p;
    if ((dir_p = opendir(old_dirname.c_str())) == 0) {
      write_error(gettext("SERIOUS SYSTEM ERROR: Can't open received fax directory.\n"
			  "This fax will not be saved!\n"));
    }

    else {
      chdir(old_dirname.c_str());
      while ((direntry = readdir(dir_p)) != 0) {
	stat(direntry->d_name, &statinfo);
	if (S_ISREG(statinfo.st_mode)) {
	  filelist.push_back(direntry->d_name);
	}
      }

      closedir(dir_p);

      bool failure = false;
      std::vector<std::string>::const_iterator iter;
      std::string old_filename;
      std::string new_filename;

      for (iter = filelist.begin(); iter != filelist.end(); ++iter) {
	old_filename = old_dirname;
	old_filename += '/';
	old_filename += *iter;
	if (!iter->substr(0, std::strlen("current.")).compare("current.")) {
	  new_filename = new_dirname;
	  new_filename += '/';
	  new_filename += faxname;
	  new_filename += iter->substr(std::strlen("current"));  // add the suffix (".001" etc.)
	  if (link(old_filename.c_str(), new_filename.c_str())) {
	    failure = true;
	    break;
	  }
	}
      }
      if (failure) {
	write_error(gettext("SERIOUS SYSTEM ERROR: Cannot save all of the received fax.\n"
			    "All or part of the fax will be missing!\n"));
      }
      else {
	for (iter = filelist.begin(); iter != filelist.end(); ++iter) {
	  old_filename = old_dirname;
	  old_filename += '/';
	  old_filename += *iter;
	  unlink(old_filename.c_str());
	}
	if (rmdir(old_dirname.c_str())) {
	  std::string msg("Can't delete directory ");
	  msg += old_dirname;
	  msg += "\nThe contents should have been moved to ";
	  msg += new_dirname;
	  msg += "\nand it should now be empty -- please check\n";
	  write_error(msg.c_str());
	}
      }	
    }

    // reset current directory
    std::string temp(prog_config.working_dir + "/faxin");
    chdir(temp.c_str());
  }
  // this program does not provide an automatic fax description at the moment
  // so pass an empty string as the secong member of the pair
  return std::pair<std::string, std::string>(std::string(faxname), std::string());
}

void EfaxController::read_pipe_cb(bool&) {

  char pipe_buffer[PIPE_BUF + 1];
  ssize_t result;

  while ((result = stdout_pipe.read(pipe_buffer, PIPE_BUF)) > 0) {
    SharedHandle<char*> output_h(stdout_pipe_reassembler(pipe_buffer, result));
    if (output_h.get()) stdout_message(output_h.get());
    else write_error(gettext("Invalid Utf8 received in EfaxController::read_pipe_cb()\n"));
  }
}

void EfaxController::join_child(void) {
  // we do not need a mutex for this - join_child() is only ever called by
  // a worker thread in sendfax_thread() or receive_thread() and unjoin_child()
  // is only called in the initial (GUI) thread in child_ended_cb() after a
  // call to Callback::post() has forced memory synchronisation

  iowatch_tag = start_iowatch(stdout_pipe.get_read_fd(),
			      Callback::make(*this, &EfaxController::read_pipe_cb),
			      G_IO_IN);
}

void EfaxController::unjoin_child(void) {
  // we do not need a mutex for this - join_child() is only ever called by
  // a worker thread in sendfax_thread() or receive_thread() and unjoin_child()
  // is only called in the initial (GUI) thread in child_ended_cb() after a
  // call to Callback::post() has forced memory synchronisation

  g_source_remove(iowatch_tag);
  stdout_pipe.close();
  stdout_pipe_reassembler.reset();
}

void EfaxController::kill_child(void) {
  // it shouldn't be necessary to check whether the process group id of
  // the process whose pid is in prog_config.efax_controller_child_pid
  // is the same as efax-gtk (that is, prog_config.efax_controller_child_pid
  // is not out of date) because it shouldn't happen - even if efax has ended
  // we cannot have reaped its exit value yet if prog_config.efax_controller_child_pid
  // is not 0 so it should still be a zombie in the process table, but for
  // safety's sake let's just check it

  // use a mutex as we set prog_config.efax_controller_child_pid in
  // sendfax_thread() or receive_thread() and memory may not yet
  // have synchronised
  pid_t pid;
  {
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    pid = prog_config.efax_controller_child_pid;
  }

  if (pid > 0 && getpgid(0) == getpgid(pid)) {
    kill(pid, SIGTERM);
    // one second delay
    g_usleep(1000000);
    // now really make sure (we don't need to check
    // prog_config.efax_controller_child_pid again in this
    // implementation because the child exit handling is in this
    // thread, via EfaxController::child_ended_cb(), but we would need
    // to check it again (and protect it with a mutex) if the program
    // is modified to put child exit handling in a signal handling
    // thread waiting on sigwait() which resets the value of
    // prog_config.efax_controller_child_pid)
    kill(pid, SIGKILL);
  }
}

std::pair<std::string, std::string> EfaxController::save_sent_fax(void) {

  char faxname[18];
  *faxname = 0;
  std::string description;

  // get a time value to create the directory name into which the fax is to be saved
  struct std::tm* time_p;
  std::time_t time_count;
    
  std::time(&time_count);
  time_p = std::localtime(&time_count);

  // now create the directory into which the fax files to be saved
  std::string fileout_name(prog_config.working_dir);
  fileout_name += "/faxsent/";

  const char dirname_format[] = "%Y%m%d%H%M%S";
  std::strftime(faxname, sizeof(faxname), dirname_format, time_p);
  std::string temp(fileout_name);
  temp += faxname;

  // check whether directory already exists or can't be created
  int count;
  for (count = 0; count < 4 && mkdir(temp.c_str(), S_IRUSR | S_IWUSR | S_IXUSR); count++) {
    // it is not worth checking for a short sleep because of a signal as we will
    // repeat the attempt four times anyway
    sleep(1); // wait a second to get a different time
    std::time(&time_count);
    time_p = std::localtime(&time_count);
    std::strftime(faxname, sizeof(faxname), dirname_format, time_p);
    temp = fileout_name + faxname;
  }
  if (count == 4) {
    write_error(gettext("SERIOUS SYSTEM ERROR: Can't create directory to save sent fax.\n"
			"This fax will not be saved!\n"));
    *faxname = 0;
  }

  else {
    // now make fileout_name the same as the directory name for the saved faxes
    // that we have just created
    fileout_name += faxname;
    const std::string fileout_dirname(fileout_name); // keep for later to enter a description

    // and now complete the unsuffixed file name of the destination files
    fileout_name += '/';
    fileout_name += faxname;

    // now create a string containing the date for the fax description
    // glibc has const struct tm* as last param of strftime()
    // but the const does not appear to be guaranteed by POSIX
    // so do localtime() again just in case
    time_p = std::localtime(&time_count);
    const char date_description_format[] = "(%H%M %Z %d %b %Y)";
    const int max_description_datesize = 126;
    char date_description[max_description_datesize];
    std::strftime(date_description, max_description_datesize, date_description_format, time_p);

    // now copy files into the relevant directory for the fax pages to be saved

    std::vector<std::string> file_basename_vec; // this is used in making the fax description
    int out_partnumber = 1; // in order to generate the suffix for each fax page as saved
    
    std::vector<std::string>::const_iterator sentname_iter;
    for (sentname_iter = last_fax_item_sent.file_list.begin();
	 sentname_iter != last_fax_item_sent.file_list.end(); ++sentname_iter) {

      std::string::size_type pos = sentname_iter->find_last_of('/');
      if (pos == std::string::npos || pos + 2 > sentname_iter->size()) {
	write_error("Not valid file name to save -- can't save sent fax\n");
      }

      else {
	// save the file base name for later use in making the fax description
	pos++;
	file_basename_vec.push_back(sentname_iter->substr(pos));

	// make the suffixes
	int in_partnumber = 1;
	std::ostringstream in_suffix_strm;
#ifdef HAVE_STREAM_IMBUE
	in_suffix_strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

	in_suffix_strm << '.' << std::setfill('0')
		       << std::setw(3) << in_partnumber;

	std::ostringstream out_suffix_strm;
#ifdef HAVE_STREAM_IMBUE
	out_suffix_strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

	out_suffix_strm << '.' << std::setfill('0')
			<< std::setw(3) << out_partnumber;

	// make the suffixed source and destination files
	std::string suffixed_inname = *sentname_iter + in_suffix_strm.str();
	std::string suffixed_outname = fileout_name + out_suffix_strm.str();

	std::ifstream filein;
	std::ofstream fileout;
	const int BLOCKSIZE = 1024;
	char block[BLOCKSIZE];

	while (!access(suffixed_inname.c_str(), R_OK)
	       && (filein.open(suffixed_inname.c_str(), std::ios::in), filein) // use comma operator to check filein
	       && (fileout.open(suffixed_outname.c_str(), std::ios::out), fileout)) { // ditto for fileout
	  while (filein) {
	    filein.read(block, BLOCKSIZE);
	    fileout.write(block, filein.gcount());
	  }
	  filein.close();
	  filein.clear();
	  fileout.close();
	  fileout.clear();
	  unlink(suffixed_inname.c_str());
     
	  in_partnumber++;
	  out_partnumber++;
	  in_suffix_strm.str("");
	  in_suffix_strm << '.' << std::setfill('0')
			 << std::setw(3) << in_partnumber;

	  out_suffix_strm.str("");
	  out_suffix_strm << '.' << std::setfill('0')
			  << std::setw(3) << out_partnumber;

	  // make the suffixed source and destination files
	  suffixed_inname = *sentname_iter + in_suffix_strm.str();
	  suffixed_outname = fileout_name + out_suffix_strm.str();
	}
	fileout.clear();
	if (in_partnumber < 2) write_error("There was a problem saving all or part of the sent fax\n");
      }
    }

    // now save the sent fax description
    if (out_partnumber > 1) {
      const std::string description_filename(fileout_dirname + "/Description");
      std::ofstream fileout(description_filename.c_str(), std::ios::out);
      if (fileout) {
	if (last_fax_item_sent.is_socket_file) description = gettext("PRINT JOB");
	else {
	  std::vector<std::string>::const_iterator filename_iter;
	  for (filename_iter = file_basename_vec.begin();
	       filename_iter != file_basename_vec.end(); ++filename_iter) {
      
	    if (filename_iter != file_basename_vec.begin()) description += '+';
	    try {
	      description += Utf8::filename_to_utf8(*filename_iter);
	    }
	    catch (Utf8::ConversionError&) {
	      write_error("UTF-8 conversion error in EfaxController::save_sent_fax()\n");
	    }
	  }
	}
	if (!last_fax_item_sent.number.empty()) {
	  description += " --> ";
	  description += last_fax_item_sent.number;
	}
	description += ' ';
	try {
	  description += Utf8::locale_to_utf8(date_description);
	}
	catch (Utf8::ConversionError&) {
	  write_error("UTF-8 conversion error in EfaxController::save_sent_fax()\n");
	}
	fileout << description;
      }
      else write_error("Cannot save sent fax description\n");
    }
  }
  return std::pair<std::string, std::string>(std::string(faxname), description);
}

void EfaxController::cleanup_fax_send_fail(void) {

  // delete the tiff g3 files for each ps file prepared for sending
  std::for_each(last_fax_item_sent.file_list.begin(),
		last_fax_item_sent.file_list.end(),
		MemFun::make(*this, &EfaxController::cleanup_fail_item));
}

void EfaxController::cleanup_fail_item(const std::string& base_filename) {

  // make the suffix
  int partnumber = 1;
  std::ostringstream strm;

#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE

  strm << '.' << std::setfill('0') << std::setw(3) << partnumber;

  // make the suffixed file name
  std::string filename = base_filename + strm.str();

  while (!access(filename.c_str(), R_OK)) {
    unlink(filename.c_str());
    partnumber++;
    strm.str("");
    strm << '.' << std::setfill('0') << std::setw(3) << partnumber;
    filename = base_filename + strm.str();
  }
}

bool EfaxController::is_receiving_fax(void) const {

  bool return_val = false;

  std::string file_name(prog_config.working_dir);
  file_name += "/faxin/current/current.001";

  struct stat statinfo;
  if (!stat(file_name.c_str(), &statinfo)) return_val = true;

  return return_val;
}

void EfaxController::start_redial_timeout(const Fax_item& fax_item) {
  int seconds = std::atoi(prog_config.redial_interval.c_str()) * 60;
  if (seconds > 60) {
    guint* id = new guint;
#if GLIB_CHECK_VERSION(2,14,0)
    *id = start_timeout_seconds(seconds,
				Callback::make(*this, &EfaxController::redial_cb, fax_item, id));
#else
    *id = start_timeout(seconds * 1000,
			Callback::make(*this, &EfaxController::redial_cb, fax_item, id));
#endif
    // redial_queue takes ownership of the id argument
    redial_queue.add(QueueItem(id, fax_item.number));
  }
  else write_error("Incorrect timeout value in EfaxController::child_ended_cb\n");
}

void EfaxController::redial_cb(Fax_item fax_item, guint* id, bool&) {
  // check preconditions
  // if the user has decided to change settings since the timeout was set
  // and we no longer automatically redial, just remove the timeout
  if (!prog_config.redial) {
    redial_queue.remove(id);
    return;
  }
  // we don't want to start a fax when the settings dialog is open
  // so have a one minute delay
  if (SettingsDialog::is_dialog()) {
    redial_queue.remove(id);
    id = new guint;
#if GLIB_CHECK_VERSION(2,14,0)
    *id = start_timeout_seconds(60, Callback::make(*this, &EfaxController::redial_cb, fax_item, id));
#else
    *id = start_timeout(60 * 1000, Callback::make(*this, &EfaxController::redial_cb, fax_item, id));
#endif
    // redial_queue takes ownership of the new id argument
    redial_queue.add(QueueItem(id, fax_item.number));
    return;
  }
  // try again later if we are not inactive or receive_standby mode
  if (state != inactive && state != receive_standby) return;


  // all OK
  // now proceed
  redial_queue.remove(id); // we set another timeout in child_ended_cb()
                           // or sendfax_cb() if this attempt fails
  sendfax_impl(fax_item, false, true);
}
