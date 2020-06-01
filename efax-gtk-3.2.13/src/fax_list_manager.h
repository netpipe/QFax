/* Copyright (C) 2001 to 2010 Chris Vine

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

#ifndef FAX_LIST_MANAGER_H
#define FAX_LIST_MANAGER_H

#include "prog_defs.h"

#include <string>
#include <map>
#include <set>
#include <fstream>
#include <utility>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

#include "utils/tree_row_reference_handle.h"
#include "utils/selected_rows_handle.h"

#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/emitter.h>


typedef std::map<TreeRowRefSharedHandle, GobjHandle<GtkTreeModel> > FolderRowToFaxModelMap;
typedef std::map<std::string, TreeRowRefSharedHandle> PathToFolderRowMap;

/* The following typedefs are in selected_rows_handle.h
typedef std::list<TreePathSharedHandle> RowPathList;
typedef std::list<TreeRowRefSharedHandle> RowRefList;
*/

namespace FaxListEnum {
  enum Mode {received = 0, sent};
}

class FolderNameValidator {
  std::set<std::string> folder_set;
public:
  void clear(void) {folder_set.clear();}
  void insert_folder_name(const std::string& name) {folder_set.insert(name);}
  void erase_folder_name(const std::string& name) {folder_set.erase(name);}
  std::pair<bool, std::string> validate(const std::string&);
};

class FaxListManager {

  static bool fax_received_list_main_iteration;
  static bool fax_sent_list_main_iteration;

  static GtkSortType list_sort_type[FaxListEnum::sent + 1];

  FaxListEnum::Mode mode;
  guint timer_tag;

  static const int target_size = 2;
  ScopedHandle<char*> target_0;
  ScopedHandle<char*> target_1;
  GtkTargetEntry target_array[target_size];

  bool folder_drag_source_enabled;
  bool fax_drag_source_enabled;
  GtkTreeIter folder_drag_source_row_iter;
  RowRefList fax_drag_source_row_refs;
  bool drag_is_fax;
  
  GtkTreeView* folder_tree_view_p;
  GtkTreeView* fax_tree_view_p;
  GtkTreeViewColumn* fax_date_column_p;
  GobjHandle<GtkTreeModel> folder_tree_store_h;
  GobjHandle<GtkTreeModel> fax_base_model_h;
  FolderRowToFaxModelMap folder_row_to_fax_model_map;

  GobjHandle<GdkPixbuf> folder_icon_h;
  GobjHandle<GdkPixbuf> trash_icon_h;

  TreeRowRefSharedHandle trash_row_ref_h;

  FolderNameValidator folder_name_validator;

  std::string get_pathname_for_folder(const GtkTreeIter*);
  void populate_fax_list(void);
  bool get_folders(PathToFolderRowMap&, const std::string&);
  void insert_fax_on_populate(const PathToFolderRowMap&, std::ifstream&,
			      const std::string&, const std::string&);
  void move_fax(const GtkTreeIter*);
  void move_folder(GtkTreeIter*, GtkTreeViewDropPosition);
  void move_child_folders_for_level(GtkTreeIter*, GtkTreeIter*);
  bool is_valid_drop_path(const std::string&, const std::string&);
  void display_faxes(void);
  void write_path(void);
  void write_paths_for_level(const GtkTreeIter*, std::ofstream&);
  std::string convert_faxname_to_date(const std::string&);
  void write_path_timer_cb(bool&) {write_path();}

public:
  class CB;
  friend class CB;

  Emitter selection_notify;
  Emitter fax_double_click_notify;

  void insert_folder_tree_view(GtkContainer* container_p) {
    gtk_container_add(container_p, GTK_WIDGET(folder_tree_view_p));
  }
  void insert_fax_tree_view(GtkContainer* container_p) {
    gtk_container_add(container_p, GTK_WIDGET(fax_tree_view_p));
  }

  void delete_fax(void);
  void delete_folder(void);
  std::pair<bool, std::string> is_folder_name_valid(const std::string& folder_name) {
    return folder_name_validator.validate(folder_name);
  }
  void make_folder(const std::string&, bool test_valid = true);
  void describe_fax(const std::string&);
  void empty_trash_folder(void);
  void insert_new_fax_in_base(const std::string&, const std::string&);

  RowPathList::size_type is_fax_selected(void);
  bool is_folder_selected(void);
  bool is_selected_folder_empty(void);
  bool is_selected_folder_permanent(void);
  bool show_trash_folder_icon(void);

  bool are_selected_faxes_in_trash_folder(void);
  void move_selected_faxes_to_trash_folder(void);

  static bool is_fax_received_list_main_iteration(void) {return fax_received_list_main_iteration;}
  static bool is_fax_sent_list_main_iteration(void) {return fax_sent_list_main_iteration;}

  static GtkSortType get_received_list_sort_type(void) {return list_sort_type[FaxListEnum::received];}
  static GtkSortType get_sent_list_sort_type(void) {return list_sort_type[FaxListEnum::sent];}
  static void set_received_list_sort_type(GtkSortType val) {list_sort_type[FaxListEnum::received] = val;}
  static void set_sent_list_sort_type(GtkSortType val) {list_sort_type[FaxListEnum::sent] = val;}

  GcharSharedHandle get_fax_number(void);
  GcharSharedHandle get_fax_description(void);
  GcharSharedHandle get_folder_name(void);

  FaxListManager(FaxListEnum::Mode);
  ~FaxListManager(void);
};

#endif
