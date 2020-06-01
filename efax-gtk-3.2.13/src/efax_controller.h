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

#ifndef EFAX_CONTROLLER_H
#define EFAX_CONTROLLER_H

#include "prog_defs.h"

#include <string>
#include <vector>
#include <list>
#include <utility>

#include <unistd.h>
#include <sys/types.h>

#include <c++-gtk-utils/pipes.h>
#include <c++-gtk-utils/notifier.h>
#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/reassembler.h>

#include "redial_queue.h"

struct Fax_item {
  std::vector<std::string> file_list;
  std::string number;
  bool is_socket_file;
};

class EfaxController {

public:
  enum State {inactive = 0, sending, receive_answer, receive_takeover, receive_standby, start_send_on_standby, send_on_standby};

private:
  const int standard_size;
  State state;
  bool close_down;

  int received_fax_count;

  std::vector<std::string> efax_parms_vec;
  Fax_item last_fax_item_sent;

  PipeFifo stdout_pipe;
  guint iowatch_tag;
  Utf8::Reassembler stdout_pipe_reassembler;
  RedialQueue redial_queue;

  void receive_cleanup(void);
  std::pair<std::string, std::string> save_received_fax(void);
  std::pair<std::string, std::string> save_sent_fax(void);
  void join_child(void);
  void unjoin_child(void);
  void kill_child(void);
  void cleanup_fax_send_fail(void);
  void cleanup_fail_item(const std::string&);
  std::vector<std::string> state_messages;
  void sendfax_thread(bool);
  void receive_thread(const char*);
  void ps_file_to_tiff_files(const std::string&);
  void init_sendfax_parms(void);
  void init_receive_parms(State);
  void no_fax_made_cb(void);
  void read_pipe_cb(bool&);
  void sendfax_impl(const Fax_item&, bool, bool);
  void child_ended_cb(int);
  void start_redial_timeout(const Fax_item&);
  void redial_cb(Fax_item, guint*, bool&);

  std::pair<const char*, char* const*> get_gs_parms(const std::string&);
  std::pair<const char*, char* const*> get_efax_parms(void);
  void delete_parms(std::pair<const char*, char* const*>);

  // we don't want to permit copies of this class
  EfaxController(const EfaxController&);
  void operator=(const EfaxController&);

public:
  EmitterArg<const std::pair<std::string, std::string>&> fax_received_notify;
  EmitterArg<const std::pair<std::string, std::string>&> fax_sent_notify;
  Emitter ready_to_quit_notify;
  EmitterArg<const char*> stdout_message;
  EmitterArg<const char*> write_state;
  EmitterArg<const std::string&> remove_from_socket_server_filelist;

  void display_state(void) {write_state(state_messages[state].c_str());}
  void get_state(int& arg) const {arg = state;}
  bool is_receiving_fax(void) const;
  // get_state_messages() is used in order to set text widgets
  // to the size necessary to display the largest messages
  std::vector<std::string> get_state_messages(void) const {return state_messages;}

  void get_count(int& arg) {arg = received_fax_count;}
  void reset_count(void) {received_fax_count = 0; display_state();}

  void stop(void);

  void show_redial_queue(void) {redial_queue.show_dialog();}

  void efax_closedown(void);
  void sendfax(const Fax_item& fax_item) {sendfax_impl(fax_item, false, false);}
  void receive(State);
  EfaxController(const int);
};

#endif
