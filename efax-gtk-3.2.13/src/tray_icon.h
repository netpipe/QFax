/* Copyright (C) 2004, 2006, 2009 and 2010 Chris Vine

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

#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include "prog_defs.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/gobj_handle.h>

class TrayIcon {

  GobjHandle<GtkStatusIcon> status_icon_h;
  GobjHandle<GtkWidget> menu_h;

  GtkMenuItem* list_received_faxes_item_p;
  GtkMenuItem* list_sent_faxes_item_p;
  GtkMenuItem* redial_queue_item_p;
  GtkMenuItem* receive_takeover_item_p;
  GtkMenuItem* receive_answer_item_p;
  GtkMenuItem* receive_standby_item_p;
  GtkMenuItem* stop_item_p;
  GtkMenuItem* quit_item_p;

  // not to be copied or assigned
  void operator=(const TrayIcon&);
  TrayIcon(const TrayIcon&);
public:
  enum MenuItem {quit, stop, receive_standby, receive_answer, receive_takeover,
		 list_received_faxes, list_sent_faxes, redial_queue};

  class CB;
  friend class CB;

  Emitter activated;
  EmitterArg<int> menu_item_chosen;
  EmitterArg<int&> get_state;
  EmitterArg<int&> get_new_fax_count;

  void set_tooltip_cb(const char* text);
  bool is_embedded(void);

  TrayIcon(void);
};

#endif
