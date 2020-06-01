/* Copyright (C) 2004, 2005, 2009 and 2010 Chris Vine

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

#ifndef SOCKET_NOTIFY_H
#define SOCKET_NOTIFY_H

#include "prog_defs.h"

#include <string>
#include <utility>

#include <gtk/gtk.h>

#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/emitter.h>


namespace SocketNotifyDialogCB {
  extern "C"  void socket_notify_dialog_button_clicked(GtkWidget*, void*);
}

class SocketNotifyDialog: public WinBase {
  const int standard_size;
  std::pair<std::string, std::string> fax_name;

  GtkWidget* number_button_p;
  GtkWidget* send_button_p;
  GtkWidget* queue_button_p;

  GtkWidget* number_entry_p;

  void set_number_cb(const std::string&);
  void send_signals(void);
protected:
  virtual void on_delete_event(void);
public:
  friend void SocketNotifyDialogCB::socket_notify_dialog_button_clicked(GtkWidget*, void*);

  EmitterArg<const std::pair<std::string, std::string>&> fax_name_sig;
  EmitterArg<const std::string&> fax_number_sig;
  Emitter sendfax_sig;
  Emitter queue_sig;

  SocketNotifyDialog(const int standard_size, const std::pair<std::string, unsigned int>&);
};

#endif
