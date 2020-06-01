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

#include <algorithm>

#include "selected_rows_handle.h"


class SelectedRowsHandle::CB {
public:
  static void fill(GtkTreePath* path_p,
		   void* data) {
    SelectedRowsHandle* instance_p = static_cast<SelectedRowsHandle*>(data);
    instance_p->path_list.push_back(TreePathSharedHandle(gtk_tree_path_copy(path_p)));
  }
};

// the GObject callback function with both C linkage
// specification and internal linkage
extern "C" {
  static void srh_fill_cb(GtkTreeModel*,
			  GtkTreePath* path_p,
			  GtkTreeIter*,
			  void* data) {
    SelectedRowsHandle::CB::fill(path_p, data);
  }
} // extern "C"


// this is a functor for converting a TreePathSharedHandle to a TreeRowRefSharedHandle
// and is suitable for use with std::transform()
class PathToRowRef {
  GtkTreeModel* model_p;
public:
  TreeRowRefSharedHandle operator()(const TreePathSharedHandle& path_h) {
    return TreeRowRefSharedHandle(gtk_tree_row_reference_new(model_p, path_h));
  }
  PathToRowRef(GtkTreeModel* model_p_): model_p(model_p_) {}
};

void SelectedRowsHandle::fill(const GtkTreeView* tree_view_p) {

  path_list.clear();

  GtkTreeSelection* selection_p =
    gtk_tree_view_get_selection(const_cast<GtkTreeView*>(tree_view_p));
  gtk_tree_selection_selected_foreach(selection_p,
				      srh_fill_cb,
				      this);
}

void SelectedRowsHandle::get_ref_list(GtkTreeModel* model_p,
				      RowRefList& row_ref_list) const {

  row_ref_list.clear();
  std::transform(path_list.begin(), path_list.end(),
		 std::back_inserter(row_ref_list), PathToRowRef(model_p));
}

