/* Copyright (C) 2001 to 2005, 2009 and 2010 Chris Vine

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

#ifndef SOCKET_LIST_H
#define SOCKET_LIST_H

#include "prog_defs.h"

#include <string>
#include <list>
#include <utility>

#include <gtk/gtk.h>

#include "socket_server.h"

#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/shared_ptr.h>


class SocketListDialog: public WinBase {
  static int is_socket_list;
  const int standard_size;

  GtkWidget* send_button_p;
  GtkWidget* view_button_p;
  GtkWidget* remove_button_p;
  GtkWidget* close_button_p;

  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;

  void send_fax_impl(void);
  void view_file(void);
  void remove_file_prompt(void);
  void remove_file(void);
  std::pair<const char*, char* const*> get_view_file_parms(const std::string&);
  void delete_parms(std::pair<const char*, char* const*>);
  void set_socket_list_item(const std::pair<std::string, unsigned int>&);
public:
  class CB;
  friend class CB;

  Releaser releaser; // for MainWindow object

  EmitterArg<const std::pair<std::string, std::string>&> selected_fax;
  EmitterArg<const std::string&> remove_from_socket_server_filelist;

  void close_cb(void);

  void set_socket_list_rows(std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> >);
  static int get_is_socket_list(void) {return is_socket_list;}

  SocketListDialog(std::pair<SharedPtr<FilenamesList>,
		             SharedPtr<Thread::Mutex::Lock> > filenames_pair,
		   const int standard_size_);
  ~SocketListDialog(void);
};

#endif
