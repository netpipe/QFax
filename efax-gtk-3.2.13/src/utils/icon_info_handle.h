/* Copyright (C) 2004 Chris Vine

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

#ifndef ICON_INFO_HANDLE_H
#define ICON_INFO_HANDLE_H

#include <gtk/gtk.h>

#include <prog_defs.h>

#include <c++-gtk-utils/shared_handle.h>

class GtkIconInfoFree {
public:
  void operator()(GtkIconInfo* obj_p) {
    if (obj_p) {
      gtk_icon_info_free(obj_p);
    }
  }
};

typedef ScopedHandle<GtkIconInfo*, GtkIconInfoFree> IconInfoScopedHandle;

#endif
