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

#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "prog_defs.h"

#include <string>
#include <utility>

#include <gtk/gtk.h>

#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/emitter.h>


namespace FileListDialogCB {
  extern "C" {
    void file_list_dialog_button_clicked(GtkWidget*, void*);
    void file_list_dialog_set_buttons(GtkTreeSelection*, void* data);
  }
}

class FileListDialog: public WinBase {
  static int is_file_list;
  const int standard_size;

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* up_button_p;
  GtkWidget* down_button_p;
  GtkWidget* add_button_p;
  GtkWidget* view_button_p;
  GtkWidget* remove_button_p;

  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;

  std::string get_files(void);
  void add_files(void);
  void add_file_item(const std::string&);
  void view_file(void);
  void remove_file_prompt(void);
  void remove_file(void);
  void move_up(void);
  void move_down(void);
  std::pair<const char*, char* const*> get_view_file_parms(const std::string&);
  void delete_parms(std::pair<const char*, char* const*>);
public:
  friend void FileListDialogCB::file_list_dialog_button_clicked(GtkWidget*, void*);
  friend void FileListDialogCB::file_list_dialog_set_buttons(GtkTreeSelection*, void*);

  EmitterArg<const std::string&> accepted;

  static int get_is_file_list(void) {return is_file_list;}

  FileListDialog(const int standard_size, GtkWindow* parent_p);
  ~FileListDialog(void);
};

#endif
