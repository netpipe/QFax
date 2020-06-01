/* Copyright (C) 2003 Chris Vine

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

#include <iostream>
#include <ostream>
#include <string>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>


#define BUFFER_LENGTH 1024

void error_msg(const char*);

extern int mainEfaxsocket(int argc, char* argv[]) {

  if (argc < 3) {
    error_msg("Insufficient arguments\n"
	      "Usage efax-gtk-socket-client [hostname] [port]\n");
    return 1;
  }

  struct hostent* hostname_info;
  if ((hostname_info = gethostbyname(argv[1])) == 0) {
    std::string message("Cannot get address for hostname ");
    message += argv[1];
    message += '\n';
    error_msg(message.c_str());
    return 1;
  }
  
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    error_msg("Cannot create socket for socket server\n");
    return 1;
  }

  sockaddr_in address;
  std::memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_addr = *(in_addr*)hostname_info->h_addr_list[0];
  address.sin_port = htons(std::atoi(argv[2]));

  if (connect(sock_fd, (sockaddr*)&address, sizeof(address)) < 0) {
    error_msg("Cannot connect to efax-gtk server socket\n");
    close(sock_fd);
    return 1;
  }

  char buffer[BUFFER_LENGTH];
  ssize_t read_result;
  ssize_t write_result = 0;
  ssize_t written;

  while ((read_result = read(0, buffer, BUFFER_LENGTH)) > 0
	 && write_result != -1) {
    written = 0;
    do {
      write_result = write(sock_fd, buffer + written, read_result);
      written += write_result; // it doesn't matter if write_result is -1 as
                               // then the while test will fail
      read_result -= write_result;
    } while (read_result && write_result != -1);
  }

  if (write_result == -1) error_msg("Write error to socket\n");

  close(sock_fd);

  return 0;
}

void error_msg(const char* message) {

  std::cerr << "efax-gtk-socket-client: " << message << std::flush;
  syslog(LOG_ERR | LOG_USER, message);

}
