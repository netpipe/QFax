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

#ifndef ADDRESSBOOK_H
#define ADDRESSBOOK_H

#include "prog_defs.h"

#include <vector>
#include <string>

#include <gtk/gtk.h>

#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/emitter.h>


namespace AddressBookCB {
  extern "C" {
    void addr_book_button_clicked(GtkWidget*, void*);
    void addr_book_drag_n_drop(GtkTreeModel*, GtkTreePath*, void*);
  }
}

class AddressBook: public WinBase {
  static int is_address_list;
  const int standard_size;
  std::string result;

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* add_button_p;
  GtkWidget* delete_button_p;
  GtkWidget* up_button_p;
  GtkWidget* down_button_p;

  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;

  void read_list(void);
  void save_list(void);
  bool ok_impl(void);
  void finish(void);
  GcharSharedHandle get_number(void);
  void add_address_prompt(void);
  void add_address_impl(const std::vector<std::string>& address);
  void delete_address_prompt(void);
  void delete_address_impl(void);
  void move_up_impl(void);
  void move_down_impl(void);
protected:
  virtual void on_delete_event(void);
public:
  friend void AddressBookCB::addr_book_button_clicked(GtkWidget*, void*);
  friend void AddressBookCB::addr_book_drag_n_drop(GtkTreeModel*, GtkTreePath*, void*);

  EmitterArg<const std::string&> accepted;
  static int get_is_address_list(void) {return is_address_list;}
  std::string get_result(void) const {return result;}
  AddressBook(const int standard_size, GtkWindow* parent_p);
  ~AddressBook(void);
};

namespace AddressDialogCB {
  extern "C" {
    void addr_dialog_selected(GtkWidget*, void*);
    gboolean addr_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}

class AddressDialog: public WinBase {

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* name_entry_p;
  GtkWidget* number_entry_p;

  bool selected_impl(void);
public:
  friend void AddressDialogCB::addr_dialog_selected(GtkWidget*, void*);
  friend gboolean AddressDialogCB::addr_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);

  EmitterArg<const std::vector<std::string>&> accepted;
  AddressDialog(const int standard_size, GtkWindow* parent_p);
};

#endif
