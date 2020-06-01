/* Copyright (C) 2001 to 2014 Chris Vine

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

#ifndef DIALOGS_H
#define DIALOGS_H

#include "prog_defs.h"

#include <string>
#include <vector>
#include <utility>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/emitter.h>


namespace FileReadSelectDialogCB {
  extern "C" {
    void frs_dialog_response(GtkDialog*, gint, void*);
  }
}

class FileReadSelectDialog: public WinBase {
  int standard_size;

  std::vector<std::string> result;
  std::pair<const char*, char* const*> get_view_file_parms(void);
  void delete_parms(std::pair<const char*, char* const*>);
  std::string get_filename_string(void);
  void set_result(void);
  void view_file_impl(void);
protected:
  virtual void on_delete_event(void);
public:
  friend void FileReadSelectDialogCB::frs_dialog_response(GtkDialog*, gint, void*);

  // get_result() returns the filenames in UTF-8 (not filesystem) encoding
  std::vector<std::string> get_result(void) const {return result;}
  FileReadSelectDialog(int standard_size, bool multi_files, GtkWindow* parent_p);
};

namespace GplDialogCB {
  extern "C" {
    void gpl_dialog_selected(GtkWidget*, void*);
    gboolean gpl_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);
  }
}

class GplDialog: public WinBase {
public:
  enum Result {rejected, accepted};
private:
  int standard_size;
  Result result;
  GtkWidget* accept_button_p;
  GtkWidget* reject_button_p;
  GtkTextView* text_view_p;
protected:
  virtual int get_exec_val(void) const;
  virtual void on_delete_event(void);
public:
  friend void GplDialogCB::gpl_dialog_selected(GtkWidget*, void*);
  friend gboolean GplDialogCB::gpl_dialog_key_press_event(GtkWidget*, GdkEventKey*, void*);

  GplDialog(int standard_size);
};

namespace InfoDialogCB {
  extern "C" void info_dialog_selected(GtkDialog*, int, void*);
}

class InfoDialog: public WinBase {
protected:
  virtual void on_delete_event(void);
public:
  friend void InfoDialogCB::info_dialog_selected(GtkDialog*, int, void*);

  InfoDialog(const char* text, const char* caption,
	     GtkMessageType message_type, GtkWindow* parent_p, bool modal = true);
};

namespace PromptDialogCB {
  extern "C" void prompt_dialog_selected(GtkDialog*, int, void*);
}

class PromptDialog: public WinBase {
  bool result;
protected:
  virtual int get_exec_val(void) const;
  virtual void on_delete_event(void);
public:
  friend void PromptDialogCB::prompt_dialog_selected(GtkDialog*, int, void*);

  Emitter accepted;
  Emitter rejected;

  PromptDialog(const char* text, const char* caption,
	       int standard_size, GtkWindow* parent_p, bool modal = true);
};

namespace AboutEfaxGtkDialogCB {
  extern "C" void about_efax_gtk_dialog_selected(GtkDialog*, int, void*);
}

class AboutEfaxGtkDialog: public WinBase {
protected:
  virtual void on_delete_event(void);
public:
  friend void AboutEfaxGtkDialogCB::about_efax_gtk_dialog_selected(GtkDialog*, int, void*);

  AboutEfaxGtkDialog(GtkWindow* parent_p = 0, bool modal = false);
};

namespace AboutEfaxDialogCB {
  extern "C" void about_efax_dialog_selected(GtkDialog*, int, void*);
}

class AboutEfaxDialog: public WinBase {
protected:
  virtual void on_delete_event(void);
public:
  friend void AboutEfaxDialogCB::about_efax_dialog_selected(GtkDialog*, int, void*);

  AboutEfaxDialog(GtkWindow* parent_p = 0, bool modal = false);
};

#endif
