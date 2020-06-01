/* Copyright (C) 2007 Chris Vine

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

#ifndef PANGO_LAYOUT_ITER_HANDLE_H
#define PANGO_LAYOUT_ITER_HANDLE_H

#include <pango/pango-layout.h>

#include <prog_defs.h>

#include <c++-gtk-utils/shared_handle.h>

class PangoLayoutIterFree {
public:
  void operator()(PangoLayoutIter* obj_p) {
    if (obj_p) {
      pango_layout_iter_free(obj_p);
    }
  }
};

typedef SharedHandle<PangoLayoutIter*, PangoLayoutIterFree> PangoLayoutIterSharedHandle;

#endif
