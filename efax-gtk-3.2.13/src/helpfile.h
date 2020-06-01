/* Copyright (C) 2001 to 2005 and 2010 Chris Vine

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

#ifndef HELPFILE_H
#define HELPFILE_H

#include "prog_defs.h"

#include <gtk/gtk.h>

#include <c++-gtk-utils/window.h>


namespace HelpDialogCB {
  extern "C" {
    void help_dialog_button_clicked(GtkWidget*, void*);
    gboolean help_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}

class HelpDialog: public WinBase {
  static int is_helpfile;
  GtkNotebook* notebook_p;

  const char* get_sending_help(void);
  const char* get_redial_help(void);
  const char* get_receiving_help(void);
  const char* get_addressbook_help(void);
  const char* get_fax_list_help(void);
  const char* get_settings_help(void);
  GtkWidget* make_text_view(const char* text);
  GtkWidget* make_scrolled_window(void);
public:
  friend void HelpDialogCB::help_dialog_button_clicked(GtkWidget*, void*);
  friend gboolean HelpDialogCB::help_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);

  static int get_is_helpfile(void) {return is_helpfile;}
  HelpDialog(const int size);
  ~HelpDialog(void);
};

#endif
