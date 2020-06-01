/* Copyright (C) 2004 to 2010 Chris Vine

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

#ifndef SELECTED_ROWS_HANDLE_H
#define SELECTED_ROWS_HANDLE_H

#include <list>

#include <gtk/gtk.h>

#include "tree_row_reference_handle.h"
#include "tree_path_handle.h"

typedef std::list<TreePathSharedHandle> RowPathList;
typedef std::list<TreeRowRefSharedHandle> RowRefList;

class SelectedRowsHandle {
  RowPathList path_list;
public:
  // this helper class avoids exposing GObject callbacks with C
  // linkage to the global namespace
  class CB;
  friend class CB;

  void fill(const GtkTreeView* tree_view_p);
  RowPathList::size_type size(void) const {return path_list.size();}
  bool is_empty(void) const {return path_list.empty();}

  // get_ref_list() should be called and the result stored before anything is done to
  // the tree view which invalidates the selected paths it provides (row references
  // are not invalidated by things done to the tree view, but paths can be)
  // this method will change the RowRefList argument - it will empty it of
  // any previous selection and then fill it with the current one
  void get_ref_list(GtkTreeModel*, RowRefList&) const;

  // the method front() can also be invalidated by things done to the tree
  // view as it returns a path. If the tree view might change use get_ref_list()
  // first and then call RowRefList::front() with respect to that list
  TreePathSharedHandle front(void) const {return path_list.front();}

  SelectedRowsHandle() {}

  // if a Gtk::TreeView object is passed to the constructor, then the
  // path list will be filled immediately without the need to call
  // SelectedRowsHandle::fill()
  SelectedRowsHandle(const GtkTreeView* tree_view_p) {fill(tree_view_p);}
};

#endif
