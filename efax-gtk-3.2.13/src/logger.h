/* Copyright (C) 2001 to 2007 Chris Vine

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

#ifndef LOGGER_H
#define LOGGER_H

#include "prog_defs.h"

#include <string>
#include <fstream>
#include <exception>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

#include <c++-gtk-utils/window.h>


class Logger {

  int logfile_count;
  std::ofstream logfile;
  std::string logfile_name;
  std::string default_dirname;
  guint timer_tag;
  GtkWindow* parent_p;

  void logfile_timer_cb(bool&);
  bool log_to_string(std::string&);
public:
  void write_to_log(const char*);
  void reset_logfile(void);

  // only call print_log(), print_page_setup() and view_log() in the
  // main GUI thread.  print_log() and print_page_setup() will compile
  // but won't do anything unless this program is compiled against
  // GTK+-2.10.0 or higher, so that the GTK+ printing API is available
  void print_log(void);
  static void print_page_setup(GtkWindow* parent_p);
  void view_log(const int standard_size);
  
  Logger(const char* default_dir, GtkWindow* parent_p = 0);
  ~Logger(void);
};

namespace LogViewDialogCB {
  extern "C" {
    void log_view_dialog_button_clicked(GtkWidget*, void*);
    gboolean log_view_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}

struct LogViewFileError: public std::exception {
  virtual const char* what() const throw() {return "Cannot open log file for viewing";}
};

class LogViewDialog: public WinBase {
  GtkWidget* text_view_p;
public:
  friend void LogViewDialogCB::log_view_dialog_button_clicked(GtkWidget*, void*);
  friend gboolean LogViewDialogCB::log_view_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  LogViewDialog(const int standard_size, const std::string& logfile_name, GtkWindow* parent_p = 0);
};


#endif
