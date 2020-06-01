/* Copyright (C) 2006 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

*/

#include "toolbar_append_widget.h"


void toolbar_append_widget(GtkToolbar* toolbar_p, GtkWidget* widget_p,
			   const char* tooltip_text_p) {

  GtkToolItem* toolitem_p = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(toolitem_p), widget_p);
  gtk_tool_item_set_tooltip_text(toolitem_p, tooltip_text_p);
  gtk_toolbar_insert(toolbar_p, toolitem_p, -1);
}
