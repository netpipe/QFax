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

#ifndef TOOLBAR_APPEND_WIDGET_H
#define TOOLBAR_APPEND_WIDGET_H

/* This is a utility function which mimics gtk_toolbar_append_widget()
   but uses the GtkToolItem interface introduced in GTK+-2.4 instead
   of that function (which was deprecated in GTK+-2.4).  If compiled
   with GTK+-2.0 or GTK+-2.2, a call to toolbar_append_widget() will
   just pass it on to gtk_toolbar_append_widget(); otherwise it will
   insert an appropriate GtkToolItem object into the toolbar.

   The parameters passed are the same as those in
   gtk_toolbar_append_widget(), so the third argument is a tooltip and
   the fourth a private tooltip (if any).
*/

#include <gtk/gtk.h>

void toolbar_append_widget(GtkToolbar*, GtkWidget*, const char*);

#endif
