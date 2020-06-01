/* Copyright (C) 2008 and 2010 Chris Vine

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

#ifndef CAIRO_HANDLE_H
#define CAIRO_HANDLE_H

#include <cairo.h>

#include <prog_defs.h>

#include <c++-gtk-utils/shared_handle.h>

class CairoSurfaceDestroy {
public:
  void operator()(cairo_surface_t* obj_p) {
    if (obj_p) {
      cairo_surface_destroy(obj_p);
    }
  }
};

class CairoPatternDestroy {
public:
  void operator()(cairo_pattern_t* obj_p) {
    if (obj_p) {
      cairo_pattern_destroy(obj_p);
    }
  }
};

class CairoContextDestroy {
public:
  void operator()(cairo_t* obj_p) {
    if (obj_p) {
      cairo_destroy(obj_p);
    }
  }
};

typedef ScopedHandle<cairo_surface_t*, CairoSurfaceDestroy> CairoSurfaceScopedHandle;
typedef ScopedHandle<cairo_pattern_t*, CairoPatternDestroy> CairoPatternScopedHandle;
typedef ScopedHandle<cairo_t*, CairoContextDestroy> CairoContextScopedHandle;

#endif
