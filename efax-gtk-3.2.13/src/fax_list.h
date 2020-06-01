/* Copyright (C) 2001 to 2007, 2009 and 2010 Chris Vine

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

#ifndef FAX_LIST_H
#define FAX_LIST_H

#include "prog_defs.h"

#include <string>
#include <utility>
#include <vector>

#include <gtk/gtk.h>

#include "fax_list_manager.h"
#include "utils/mono_tiff_print_manager.h"

#include <c++-gtk-utils/pipes.h>
#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/intrusive_ptr.h>
#include <c++-gtk-utils/async_queue.h>


namespace FaxListDialogCB {
  extern "C" void fax_list_dialog_button_clicked(GtkWidget*, void*);
}

class FaxListDialog: public WinBase {
  FaxListEnum::Mode mode;
  static int is_fax_received_list;
  static int is_fax_sent_list;
  const int standard_size;

  Thread::Mutex working_dir_mutex;
  std::string working_dir;

  std::vector<SharedHandle<char*> > view_files;

  FaxListManager fax_list_manager;
  AsyncQueue<IntrusivePtr<MonoTiffPrintManager> > async_queue;

  GtkWidget* close_button_p;
  GtkWidget* page_setup_button_p;
  GtkWidget* print_button_p;
  GtkWidget* view_button_p;
  GtkWidget* describe_button_p;
  GtkWidget* delete_fax_button_p;
  GtkWidget* empty_trash_button_p;
  GtkWidget* add_folder_button_p;
  GtkWidget* delete_folder_button_p;
  GtkWidget* reset_button_p;

  GtkWidget* new_fax_label_p;

  void set_buttons_cb(void);
  void describe_fax_prompt(void);
  void delete_fax(void);
  void add_folder_prompt(void);
  void add_folder(const std::string&);
  void empty_trash_prompt(void);
  void delete_folder_prompt(void);
  void write_pipe_to_file(PipeFifo*, int);
  std::pair<const char*, char* const*> get_print_from_stdin_parms(void);
  std::pair<const char*, char* const*> get_fax_to_ps_parms(const std::string&, bool);
  std::pair<const char*, char* const*> get_ps_viewer_parms(const char*);
  void print_fax_prompt(void);
  void print_fax(void);
  void print_fax_thread(std::string*, bool);
  void view_fax(void);
  void view_fax_thread(std::string*, std::pair<std::string*, int>);
  void delete_parms(std::pair<const char*, char* const*>);
public:
  friend void FaxListDialogCB::fax_list_dialog_button_clicked(GtkWidget*, void*);

  Releaser releaser;
  EmitterArg<int&> get_new_fax_count_sig;
  Emitter reset_sig;

  // display_new_fax_count() calls get_new_fax_count_sig, so until it has been
  // connected to a function which returns this, it will show 0 as the count
  void display_new_fax_count(void);
  static int get_is_fax_received_list(void) {return is_fax_received_list;}
  static int get_is_fax_sent_list(void) {return is_fax_sent_list;}
  void insert_new_fax_cb(const std::pair<std::string, std::string>& fax_info) {
    fax_list_manager.insert_new_fax_in_base(fax_info.first, fax_info.second);
    display_new_fax_count();
  }
  void set_page_setup_button(void);

  FaxListDialog(FaxListEnum::Mode, const int standard_size_);
  ~FaxListDialog(void);
};


namespace EntryDialogCB {
  extern "C" {
    void entry_dialog_selected(GtkWidget*, void*);
    gboolean entry_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}

class EntryDialog: public WinBase {

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* entry_p;

  bool selected_impl(void);
public:
  friend void EntryDialogCB::entry_dialog_selected(GtkWidget*, void*);
  friend gboolean EntryDialogCB::entry_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);

  EmitterArg<const std::string&> accepted;
  EntryDialog(const int standard_size, const char* entry_text,
	      const char* caption, const char* label_text,
	      GtkWindow* parent_p);
};


class DescriptionDialog: public EntryDialog {

public:
  DescriptionDialog(const int standard_size, const char* text, GtkWindow* parent_p);
};

class AddFolderDialog: public EntryDialog {

public:
  AddFolderDialog(const int standard_size, GtkWindow* parent_p);
};

#endif
