/* Copyright (C) 2001 to 2011 Chris Vine

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

#include <vector>
#include <cstring>
#include <ios>
#include <ostream>
#include <ctime>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "fax_list_manager.h"
#include "fax_list_manager_icons.h"
#include "utils/tree_path_handle.h"
#if GTK_CHECK_VERSION(2,99,0)
#include "utils/cairo_handle.h"
#endif

#include <c++-gtk-utils/timeout.h>
#include <c++-gtk-utils/convert.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

#define PATH_DIVIDER '@'
#define DRAG_KEY_STRING "xXefax-gtkXx"

#define WRITE_PATH_TIMER_INTERVAL 300000    // number of milliseconds between flushing of logfile
                                            // = 5 minutes

// strptime() is a POSIX rather than standard C function so it may not
// be declared in <ctime>: so declare it here (with C linkage)
extern "C" char* strptime(const char*, const char*, struct std::tm*);

bool FaxListManager::fax_received_list_main_iteration = false;
bool FaxListManager::fax_sent_list_main_iteration = false;
GtkSortType FaxListManager::list_sort_type[FaxListEnum::sent + 1] = {GTK_SORT_ASCENDING, GTK_SORT_ASCENDING};

namespace {
namespace FolderListModelColumns {
  enum {icon, name, root_only, cols_num};
}

namespace FaxListModelColumns {
  enum {fax_date, fax_description, name, cols_num};
}
} // anonymous namespace

std::pair<bool, std::string> FolderNameValidator::validate(const std::string& folder_name) {

  std::pair<bool, std::string> return_val;

  if (folder_name.find(PATH_DIVIDER) != std::string::npos) {
    return_val.first = false;
    return_val.second = gettext("Folder name cannot contain character: ");
    return_val.second += PATH_DIVIDER;
  }    

  else {
    std::pair<std::set<std::string>::iterator, bool> result = folder_set.insert(folder_name);
    // we only want a return value - erase the element if the insert succeeded
    if ((return_val.first = result.second)) folder_set.erase(folder_name);
    else {
      return_val.second = gettext("The following folder already exists: ");
      return_val.second += folder_name;
    }
  }

  return return_val;
}

class FaxListManager::CB {
public:
  static void folder_tree_view_drag_begin(GdkDragContext* context_p,
					  void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);
    instance_p->drag_is_fax = false;

    GtkTreeModel* model_p;
    GtkTreeSelection* selection_p = gtk_tree_view_get_selection(instance_p->folder_tree_view_p);
    if (gtk_tree_selection_get_selected(selection_p, &model_p,
					&instance_p->folder_drag_source_row_iter)) {

      GtkTreeViewColumn* column_p = gtk_tree_view_get_column(instance_p->folder_tree_view_p, 0);
      gint x_off, y_off, cell_width, cell_height;
      GdkRectangle cell_area;
      gtk_tree_view_column_cell_get_size(column_p, &cell_area, &x_off, &y_off, &cell_width, &cell_height);

      if (cell_height < 0) cell_height = 0;
    
      TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p,
							  &instance_p->folder_drag_source_row_iter));
    
#if GTK_CHECK_VERSION(2,99,0)
      CairoSurfaceScopedHandle icon_h(gtk_tree_view_create_row_drag_icon(instance_p->folder_tree_view_p,
									 path_h));
      cairo_surface_set_device_offset(icon_h, -5, -cell_height/2);
      gtk_drag_set_icon_surface(context_p, icon_h);
#else
      // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
      GobjHandle<GdkPixmap> icon_h(gtk_tree_view_create_row_drag_icon(instance_p->folder_tree_view_p,
								      path_h));
      GdkColormap* colormap_p = gtk_widget_get_default_colormap();
      gtk_drag_set_icon_pixmap(context_p, colormap_p, icon_h, 0, 5, cell_height/2);
#endif
    }
  }

  static void fax_tree_view_drag_begin(GdkDragContext* context_p,
				       void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);
    instance_p->drag_is_fax = true;

    SelectedRowsHandle rows_handle(instance_p->fax_tree_view_p);
    rows_handle.get_ref_list(gtk_tree_view_get_model(instance_p->fax_tree_view_p),
			     instance_p->fax_drag_source_row_refs);

    // if we are dragging only one row, provide a row icon, but otherwise
    // use the generic GTK+ drag icon
    if (rows_handle.size() == 1 &&
	gtk_tree_row_reference_valid(instance_p->fax_drag_source_row_refs.front())) {

      GtkTreeViewColumn* column_p = gtk_tree_view_get_column(instance_p->fax_tree_view_p, 0);
      gint x_off, y_off, cell_width, cell_height;
      GdkRectangle cell_area;
      gtk_tree_view_column_cell_get_size(column_p, &cell_area, &x_off, &y_off, &cell_width, &cell_height);

      gint x_pos;
      gtk_widget_get_pointer(GTK_WIDGET(instance_p->fax_tree_view_p), &x_pos, 0);

      if (cell_height < 0) cell_height = 0;
      if (x_pos < 0) x_pos = 0;

#if GTK_CHECK_VERSION(2,99,0)
      CairoSurfaceScopedHandle icon_h(gtk_tree_view_create_row_drag_icon(instance_p->fax_tree_view_p,
									 rows_handle.front()));
      cairo_surface_set_device_offset(icon_h, -x_pos, -cell_height/2);
      gtk_drag_set_icon_surface(context_p, icon_h);
#else
      // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
      GobjHandle<GdkPixmap> icon_h(gtk_tree_view_create_row_drag_icon(instance_p->fax_tree_view_p,
								      rows_handle.front()));
      GdkColormap* colormap_p = gtk_widget_get_default_colormap();

      gtk_drag_set_icon_pixmap(context_p, colormap_p, icon_h, 0, x_pos, cell_height/2);
#endif
    }
  }

  static gboolean drag_motion(GdkDragContext* context_p,
			      gint x, gint y,
			      guint time,
			      void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    bool return_val;
    GtkTreePath* path_p = 0;
    GtkTreeViewDropPosition drop_pos;
    if (gtk_tree_view_get_dest_row_at_pos(instance_p->folder_tree_view_p, x, y, &path_p, &drop_pos)
	&& path_p) {

      // wrap path_p in a TreePathScopedHandle object
      // so we do not need to delete it by hand
      TreePathScopedHandle path_h(path_p);
      path_p = 0; // we don't need it any more
      if (instance_p->drag_is_fax) {
	gtk_tree_view_set_drag_dest_row(instance_p->folder_tree_view_p, path_h,
					GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
      }
      else {
	gtk_tree_view_set_drag_dest_row(instance_p->folder_tree_view_p, path_h, drop_pos);
      }
      gdk_drag_status(context_p, GDK_ACTION_MOVE, time);
      return_val = true;
    }
    else {
      gdk_drag_status(context_p, GdkDragAction(0), time);
      return_val = false;
    }
    return return_val;
  }

  static gboolean folder_tree_view_motion_notify(GdkEventMotion* event_p,
						 void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    gint x, y;
    gtk_tree_view_convert_bin_window_to_widget_coords(instance_p->folder_tree_view_p,
						      event_p->x, event_p->y,
						      &x, &y);
    if (x >= 0 && y >= 0) {
      bool result = gtk_tree_view_get_dest_row_at_pos(instance_p->folder_tree_view_p,
						      x, y, 0, 0);
      if (result && !instance_p->folder_drag_source_enabled) {
	gtk_drag_source_set(GTK_WIDGET(instance_p->folder_tree_view_p),
			    GDK_MODIFIER_MASK, instance_p->target_array,
			    instance_p->target_size, GDK_ACTION_MOVE);
	instance_p->folder_drag_source_enabled = true;
      }      
      else if (!result && instance_p->folder_drag_source_enabled) {
	gtk_drag_source_unset(GTK_WIDGET(instance_p->folder_tree_view_p));
	instance_p->folder_drag_source_enabled = false;
      }
    }
    return false;
  }

  static gboolean fax_tree_view_motion_notify(GdkEventMotion* event_p,
					      void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    gint x, y;
    gtk_tree_view_convert_bin_window_to_widget_coords(instance_p->fax_tree_view_p,
						      event_p->x, event_p->y,
						      &x, &y);
    if (x >= 0 && y >= 0) {
      bool result = gtk_tree_view_get_dest_row_at_pos(instance_p->fax_tree_view_p,
						      x, y, 0, 0);
      if (result && !instance_p->fax_drag_source_enabled) {
	gtk_drag_source_set(GTK_WIDGET(instance_p->fax_tree_view_p),
			    GDK_MODIFIER_MASK, instance_p->target_array,
			    instance_p->target_size, GDK_ACTION_MOVE);
	instance_p->fax_drag_source_enabled = true;
      }
      else if (!result && instance_p->fax_drag_source_enabled) {
	gtk_drag_source_unset(GTK_WIDGET(instance_p->fax_tree_view_p));
	instance_p->fax_drag_source_enabled = false;
      }
    }
    return false;
  }

  static gboolean fax_tree_view_mouse_click(GdkEventButton* event_p,
					    void* data) {
  
    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    // we can use fax_drag_source_enabled as a proxy for whether the pointer
    // is over a selectable (and so selected) row in the tree view when
    // a double click occurs
    if (event_p->type == GDK_2BUTTON_PRESS
	&& instance_p->fax_drag_source_enabled) {
      instance_p->fax_double_click_notify();
    }
    return false;
  }

  static void drag_data_get(GtkSelectionData* selection_data_p) {

    // all we do in this method is to set up an identification string (DRAG_KEY_STRING)
    // so that drags not originating in this program can be ignored.  The data which
    // is dragged is placed in FaxListManager::folder_drag_source_row_iter in method
    // FaxListManager::CB::folder_tree_view_drag_begin() (for a folder drag) and in
    // FaxListManager::fax_drag_source_row_refs in method
    // FaxListManager::CB::fax_tree_view_drag_begin() (for a fax drag), rather than in
    // this method.  We can therefore use this method for drags from both the folder tree
    // view and the fax tree view.  The method FaxListManager::CB::drag_data_received()
    // tests for this identification string when a drag is received by the folder tree
    // view (the fax tree view does not receive drags, it only sources them, whereas the
    // folder tree view both receives and sources drags).

    const std::string message(DRAG_KEY_STRING);

#if GTK_CHECK_VERSION(2,14,0)
    gtk_selection_data_set(selection_data_p, gtk_selection_data_get_target(selection_data_p),
			   8, (const guchar*)message.c_str(), message.size() + 1);
#else
    gtk_selection_data_set(selection_data_p, selection_data_p->target, 8,
			   (const guchar*)message.c_str(), message.size() + 1);
#endif
  }

  static void drag_data_received(GdkDragContext* context_p,
				 gint x, gint y,
				 GtkSelectionData* selection_data_p,
				 guint time,
				 void* data) {

    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    bool success = false;
    std::string message;

#if GTK_CHECK_VERSION(2,14,0)
    if(gtk_selection_data_get_length(selection_data_p) >= 0
       && gtk_selection_data_get_format(selection_data_p) == 8) {
      message = (const char*)gtk_selection_data_get_data(selection_data_p);
    }
#else
    if(selection_data_p->length >= 0 && selection_data_p->format == 8) {
      message = (const char*)selection_data_p->data;
    }
#endif

    if (message.find(DRAG_KEY_STRING) != std::string::npos) {
    
      GtkTreePath* path_p = 0;
      GtkTreeViewDropPosition drop_pos;
      if (gtk_tree_view_get_dest_row_at_pos(instance_p->folder_tree_view_p, x, y, &path_p, &drop_pos)
	  && path_p) {
	// wrap path_p in a TreePathScopedHandle object
	// so we do not need to delete it by hand
	TreePathScopedHandle path_h(path_p);
	path_p = 0; // we don't need it any more
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter(instance_p->folder_tree_store_h, &iter, path_h)) {
	  if (instance_p->drag_is_fax) instance_p->move_fax(&iter);
	  else instance_p->move_folder(&iter, drop_pos);
	  success = true;
	}
	else write_error("Invalid iterator in FaxListManager::CB::drag_data_received()\n");
      }
    }
    gtk_drag_finish(context_p, success, false, time);
  }

  static void fax_selected(void* data) {
    static_cast<FaxListManager*>(data)->selection_notify();
  }

  static void folder_selected(void* data) {
    FaxListManager* instance_p = static_cast<FaxListManager*>(data);

    // display_faxes() needs to be called first before selection_notify
    // emitter is emitted so that the correct fax tree model is in
    // position before the functor connected to the emitter is executed
    instance_p->display_faxes();
    instance_p->selection_notify();
  }

  static void toggle_sort_type(void* data) {
    FaxListManager* instance_p = static_cast<FaxListManager*>(data);
    if (instance_p->list_sort_type[instance_p->mode] == GTK_SORT_ASCENDING) 
      instance_p->list_sort_type[instance_p->mode] = GTK_SORT_DESCENDING;
    else instance_p->list_sort_type[instance_p->mode] = GTK_SORT_ASCENDING;

    FolderRowToFaxModelMap::const_iterator iter;
    for (iter = instance_p->folder_row_to_fax_model_map.begin();
	 iter != instance_p->folder_row_to_fax_model_map.end();
	 ++iter) {
      gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(iter->second.get()),
					   FaxListModelColumns::name,
					   instance_p->list_sort_type[instance_p->mode]);
    }
    // this doesn't control the sort order (the call to gtk_tree_sortable_set_sort_column_id()
    // did that) - it just sets the direction that the arrow in the tree view shows!
    gtk_tree_view_column_set_sort_order(instance_p->fax_date_column_p,
					instance_p->list_sort_type[instance_p->mode]);  
  }
};

extern "C" {

  static void folder_tree_view_drag_begin_cb(GtkWidget*,
					     GdkDragContext* context_p,
					     void* data) {
    FaxListManager::CB::folder_tree_view_drag_begin(context_p, data);
  }

  static void fax_tree_view_drag_begin_cb(GtkWidget*,
					  GdkDragContext* context_p,
					  void* data) {
    FaxListManager::CB::fax_tree_view_drag_begin(context_p, data);
  }

  static gboolean drag_motion_cb(GtkWidget*,
				 GdkDragContext* context_p,
				 gint x, gint y,
				 guint time,
				 void* data) {
    return FaxListManager::CB::drag_motion(context_p, x, y, time, data);
  }

  static gboolean folder_tree_view_motion_notify_cb(GtkWidget*,
						    GdkEventMotion* event_p,
						    void* data) {
    return FaxListManager::CB::folder_tree_view_motion_notify(event_p, data);
  }

  static gboolean fax_tree_view_motion_notify_cb(GtkWidget*,
					      GdkEventMotion* event_p,
					      void* data) {
    return FaxListManager::CB::fax_tree_view_motion_notify(event_p, data);
  }

  static gboolean fax_tree_view_mouse_click_cb(GtkWidget*,
					       GdkEventButton* event_p,
					       void* data) {
    return FaxListManager::CB::fax_tree_view_mouse_click(event_p, data);
  }

  static void drag_data_get_cb(GtkWidget*,
			       GdkDragContext*,
			       GtkSelectionData* selection_data_p,
			       guint,
			       guint,
			       void*) {
    FaxListManager::CB::drag_data_get(selection_data_p);
  }

  static void drag_data_received_cb(GtkWidget*,
				    GdkDragContext* context_p,
				    gint x, gint y,
				    GtkSelectionData* selection_data_p,
				    guint,
				    guint time,
				    void* data) {
    FaxListManager::CB::drag_data_received(context_p, x, y, selection_data_p, time, data);
  }

  static void fax_selected_cb(GtkTreeSelection*,
			      void* data) {
    FaxListManager::CB::fax_selected(data);
  }

  static void folder_selected_cb(GtkTreeSelection*,
				 void* data) {
    FaxListManager::CB::folder_selected(data);
  }

  static void toggle_sort_type_cb(GtkTreeViewColumn*,
			       void* data) {
    FaxListManager::CB::toggle_sort_type(data);
  }

} // extern "C"

FaxListManager::FaxListManager(FaxListEnum::Mode mode_):
                                                mode(mode_),
						target_0(new char[std::strlen("STRING") + 1]),
						target_1(new char[std::strlen("text/plain") + 1]),
                                                folder_drag_source_enabled(false),
                                                fax_drag_source_enabled(false) {
  

  // set up target_array for drag and drop
  // as we set the flags member to GTK_TARGET_SAME_APP we can probably omit the key
  // DRAG_KEY_STRING
  std::strcpy(target_0.get(), "STRING");
  std::strcpy(target_1.get(), "text/plain");
  target_array[0].target = (gchar*)target_0.get();
  target_array[0].flags = GTK_TARGET_SAME_APP;
  target_array[0].info = 0;
  
  target_array[1].target = (gchar*)target_1.get();
  target_array[1].flags = GTK_TARGET_SAME_APP;
  target_array[1].info = 0;

  // create icons
  // GdkPixbufs are not owned by a GTK+ container when passed to it so use GobjHandles
  folder_icon_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_xpm_data(folder_xpm));
  folder_icon_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_scale_simple(folder_icon_h,
								14, 14,
								GDK_INTERP_BILINEAR));
  trash_icon_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_new_from_xpm_data(trash_xpm));
  trash_icon_h = GobjHandle<GdkPixbuf>(gdk_pixbuf_scale_simple(trash_icon_h,
							       14, 16,
							       GDK_INTERP_BILINEAR));

  // create the tree model for the folders and put it in a GobjHandle to handle
  // its lifetime, because it is not owned by a GTK+ container
  folder_tree_store_h =
    GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_tree_store_new(FolderListModelColumns::cols_num,
							       GDK_TYPE_PIXBUF,
							       G_TYPE_STRING,
							       G_TYPE_BOOLEAN)));
  // make the folder tree view column, pack the two renderers into it
  // and connect them to the model columns
  GtkTreeViewColumn* column_p = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column_p, gettext("Folder"));

  GtkCellRenderer* renderer_p = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column_p, renderer_p, false);
  gtk_tree_view_column_set_attributes(column_p, renderer_p,
				      "pixbuf", FolderListModelColumns::icon,
				      static_cast<void*>(0));

  renderer_p = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column_p, renderer_p, true);
  gtk_tree_view_column_set_attributes(column_p, renderer_p,
				      "text", FolderListModelColumns::name,
				      static_cast<void*>(0));

  // create the folder tree view
  folder_tree_view_p =
    GTK_TREE_VIEW(gtk_tree_view_new_with_model(folder_tree_store_h));

  // add the folder tree view column to the folder tree view
  gtk_tree_view_append_column(folder_tree_view_p, column_p);

  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  // single line selection
  gtk_tree_selection_set_mode(selection_p, GTK_SELECTION_SINGLE);

  // create the fax tree view
  fax_tree_view_p = GTK_TREE_VIEW(gtk_tree_view_new());

  // provide renderers for fax tree view, pack into the fax tree view columns
  // and connect to the fax tree model columns
  renderer_p = gtk_cell_renderer_text_new();
  fax_date_column_p = gtk_tree_view_column_new_with_attributes(gettext("Date"), renderer_p,
							       "text", FaxListModelColumns::fax_date,
							       static_cast<void*>(0));
  // make the Date column clickable
  gtk_tree_view_column_set_clickable(fax_date_column_p, true);
  g_signal_connect(G_OBJECT(fax_date_column_p), "clicked",
		   G_CALLBACK(toggle_sort_type_cb), this);
  gtk_tree_view_append_column(fax_tree_view_p, fax_date_column_p);


  renderer_p = gtk_cell_renderer_text_new();
  column_p = gtk_tree_view_column_new_with_attributes(gettext("Description"), renderer_p,
						      "text", FaxListModelColumns::fax_description,
						      static_cast<void*>(0));
  gtk_tree_view_append_column(fax_tree_view_p, column_p);

  selection_p = gtk_tree_view_get_selection(fax_tree_view_p);
  // multiple line selection
  gtk_tree_selection_set_mode(selection_p, GTK_SELECTION_MULTIPLE);

  // populate the fax model columns - this will also default to showing
  // the base folder (Inbox or Sent box) and create all the necessary
  // tree models for the fax tree view (one for each folder) and create
  // the necessary mapping
  populate_fax_list();

  // now provide drag and drop from the fax tree view to the folder tree view
  // first connect slots to ensure that a drag only starts with a valid row to drag
  g_signal_connect(G_OBJECT(folder_tree_view_p), "motion_notify_event",
		   G_CALLBACK(folder_tree_view_motion_notify_cb), this);
  g_signal_connect(G_OBJECT(fax_tree_view_p), "motion_notify_event",
		   G_CALLBACK(fax_tree_view_motion_notify_cb), this);

  // connect slots to handle the drag source in the fax tree view
  g_signal_connect(G_OBJECT(fax_tree_view_p), "drag_data_get",
		   G_CALLBACK(drag_data_get_cb), this);
  g_signal_connect(G_OBJECT(fax_tree_view_p), "drag_begin",
		   G_CALLBACK(fax_tree_view_drag_begin_cb), this);

  // connect slots to handle the drag source in the folder tree view
  g_signal_connect(G_OBJECT(folder_tree_view_p), "drag_data_get",
		   G_CALLBACK(drag_data_get_cb), this);
  g_signal_connect(G_OBJECT(folder_tree_view_p), "drag_begin",
		   G_CALLBACK(folder_tree_view_drag_begin_cb), this);

  // connect slots to handle the drag destination in the folder tree view
  g_signal_connect(G_OBJECT(folder_tree_view_p), "drag_motion",
		   G_CALLBACK(drag_motion_cb), this);
  gtk_drag_dest_set(GTK_WIDGET(folder_tree_view_p), GTK_DEST_DEFAULT_ALL,
		    target_array, target_size, GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(folder_tree_view_p), "drag_data_received",
		   G_CALLBACK(drag_data_received_cb), this);

  // handle selection of a row in the tree views
  selection_p = gtk_tree_view_get_selection(fax_tree_view_p);
  g_signal_connect(G_OBJECT(selection_p), "changed",
		   G_CALLBACK(fax_selected_cb), this);
  selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  g_signal_connect(G_OBJECT(selection_p), "changed",
		   G_CALLBACK(folder_selected_cb), this);

  // capture a double click on the fax tree view for viewing a fax
  gtk_widget_add_events(GTK_WIDGET(fax_tree_view_p), GDK_BUTTON_PRESS_MASK);
  g_signal_connect(G_OBJECT(fax_tree_view_p), "button_press_event",
		   G_CALLBACK(fax_tree_view_mouse_click_cb), this);

  // set a timeout to call write_path() at 5 minute intervals
#if GLIB_CHECK_VERSION(2,14,0)
  timer_tag = start_timeout_seconds(WRITE_PATH_TIMER_INTERVAL/1000,
				    Callback::make(*this, &FaxListManager::write_path_timer_cb));
#else
  timer_tag = start_timeout(WRITE_PATH_TIMER_INTERVAL,
			    Callback::make(*this, &FaxListManager::write_path_timer_cb));
#endif
}

FaxListManager::~FaxListManager(void) {
  g_source_remove(timer_tag);
  write_path();
}

void FaxListManager::populate_fax_list(void) {

  std::string dir(prog_config.working_dir);
  if (mode == FaxListEnum::received) dir += "/faxin";
  else dir += "/faxsent";

  chdir(dir.c_str());

  DIR* dir_p;
  if ((dir_p = opendir(dir.c_str())) == 0) {
    std::string msg("Can't open directory ");
    msg += dir;
    msg += '\n';
    write_error(msg.c_str());
  }
  else {

    // before reading the faxin/faxsent directory for faxes set up the
    // folder items we always need

    /* - we don't need to do this as the program is now written
    gtk_tree_store_clear(GTK_TREE_STORE(folder_tree_store_h.get()));
    // keep the old tree stores (if any) alive, until we have reset the model
    // in the fax tree view later in this method, by copying the values
    FolderRowToFaxModelMap old_map = folder_row_to_fax_model_map;
    // now we can clear the fax list stores map
    folder_row_to_fax_model_map.clear();
    folder_name_validator.clear();
    */

    // we always have an 'Inbox' and 'Sent box' row items
    GtkTreeIter base_folder_iter;
    gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()), &base_folder_iter, 0);
    std::string base_path;
    base_path = PATH_DIVIDER;
    if (mode == FaxListEnum::received) {
      gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &base_folder_iter,
			 FolderListModelColumns::icon, folder_icon_h.get(),
			 FolderListModelColumns::name, gettext("Inbox"),
			 FolderListModelColumns::root_only, true,
			 -1);
      base_path += gettext("Inbox");
      folder_name_validator.insert_folder_name(gettext("Inbox"));
    }
    else {
      gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &base_folder_iter,
			 FolderListModelColumns::icon, folder_icon_h.get(),
			 FolderListModelColumns::name, gettext("Sent box"),
			 FolderListModelColumns::root_only, true,
			 -1);
      base_path += gettext("Sent box");
      folder_name_validator.insert_folder_name(gettext("Sent box"));
    }

    
    // we also always have a 'Trash' folder
    GtkTreeIter trash_folder_iter;
    gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()), &trash_folder_iter, 0);
    gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &trash_folder_iter,
		       FolderListModelColumns::icon, trash_icon_h.get(),
		       FolderListModelColumns::name, gettext("Trash"),
		       FolderListModelColumns::root_only, true,
		       -1);
    std::string trash_path;
    trash_path = PATH_DIVIDER;
    trash_path += gettext("Trash");
    folder_name_validator.insert_folder_name(gettext("Trash"));

    TreePathScopedHandle base_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							     &base_folder_iter));
    TreeRowRefSharedHandle base_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
								     base_path_h));

    TreePathScopedHandle trash_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							      &trash_folder_iter));
    trash_row_ref_h = TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
									trash_path_h));
    
    PathToFolderRowMap path_to_row_map;
    path_to_row_map.insert(PathToFolderRowMap::value_type(base_path, base_row_ref_h));
    path_to_row_map.insert(PathToFolderRowMap::value_type(trash_path, trash_row_ref_h));

    // now create a fax tree model for the base and trash folders
    // put them in a GobjHandle to handle their lifetime, because they are not
    // owned by a GTK+ container
    fax_base_model_h =
      GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
								 G_TYPE_STRING,
								 G_TYPE_STRING,
								 G_TYPE_STRING)));
    GobjHandle<GtkTreeModel> trash_model_h(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
									     G_TYPE_STRING,
									     G_TYPE_STRING,
									     G_TYPE_STRING)));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fax_base_model_h.get()),
					 FaxListModelColumns::name,
					 list_sort_type[mode]);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(trash_model_h.get()),
					 FaxListModelColumns::name,
					 list_sort_type[mode]);
    folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(base_row_ref_h, fax_base_model_h));
    folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(trash_row_ref_h, trash_model_h));

    // get the paths (folders) stored in PathList
    std::string line;
    std::ifstream file;
    std::string filename(dir);
    filename += "/PathList";

#ifdef HAVE_IOS_NOCREATE
    file.open(filename.c_str(), std::ios::in | std::ios::nocreate);
#else
    // we must have Std C++ so we probably don't need a ios::nocreate
    // flag on a read open to ensure uniqueness
    file.open(filename.c_str(), std::ios::in);
#endif
    if (file) {
      while (std::getline(file, line)) get_folders(path_to_row_map, line);
    }

    // now populate the tree store
    struct dirent* direntry;
    struct stat statinfo;

    while ((direntry = readdir(dir_p)) != 0) {
      stat(direntry->d_name, &statinfo);
      if (S_ISDIR(statinfo.st_mode)
	  // keep the check for the "oldfax" sub-directory even though it is not used
	  // in version 2.2.13 onwards (users may still have it lying around)
	  && std::strcmp(direntry->d_name, "oldfax")
	  && std::strcmp(direntry->d_name, ".")
	  && std::strcmp(direntry->d_name, "..")
	  && (mode == FaxListEnum::sent
	      || std::strcmp(direntry->d_name, "current"))) {
	// checking for "current" will prevent double entries if completion of a received
	// fax occurs while we are scanning the faxin directory - it is transferred to its
	// own directory in the EfaxController::child_ended_cb() callback and that callback
	// cannot be invoked until this method has returned (note also the checks for
	// FaxListManager::is_fax_received_list_main_iteration() and
	// FaxListManager::is_fax_sent_list_main_iteration() in EfaxController::child_ended_cb())
	// likewise a completed sent fax will not be stored in a directory in the faxsent
	// directory (if we are scanning that directory) until that is done via
	// EfaxController::child_ended_cb()

	std::string fax_name(direntry->d_name);

	// this while loop can be time intensive as it reads from disk
	// so clear any outstanding events in the main program loop
	if (mode == FaxListEnum::received) fax_received_list_main_iteration = true;
	else fax_sent_list_main_iteration = true;
	while (gtk_events_pending()) gtk_main_iteration();
	if (mode == FaxListEnum::received) fax_received_list_main_iteration = false;
	else fax_sent_list_main_iteration = false;

	line = "";
	file.close();
	file.clear();

	// get the fax list Path for each fax
	filename = dir + '/';
	filename += fax_name;
	filename += "/Path";

#ifdef HAVE_IOS_NOCREATE
	file.open(filename.c_str(), std::ios::in | std::ios::nocreate);
#else
	// we must have Std C++ so we probably don't need a ios::nocreate
	// flag on a read open to ensure uniqueness
	file.open(filename.c_str(), std::ios::in);
#endif

	if (file) {
	  while (std::getline(file, line) && line.empty());
	}
	file.close();
	file.clear();

	// now open up a file stream to read any fax description
	// to pass by reference to insert_fax_on_populate()
	filename = dir + '/';
	filename += fax_name;
	filename += "/Description";

#ifdef HAVE_IOS_NOCREATE
	file.open(filename.c_str(), std::ios::in | std::ios::nocreate);
#else
	// we must have Std C++ so we probably don't need a ios::nocreate
	// flag on a read open to ensure uniqueness
	file.open(filename.c_str(), std::ios::in);
#endif

	if (!Utf8::validate(line)) { // not valid UTF-8!
	  write_error("Invalid UTF-8 string in folder path name in FaxListManager::populate_fax_list()\n");
	  line = "";
	}

	// put the faxes in the relevant fax tree model for their folder
	if (line.size() == 1 && line[0] == PATH_DIVIDER) {
	  insert_fax_on_populate(path_to_row_map, file, base_path, fax_name);
	}
	// include a call to get_folders() in case something has gone wrong
	// and the fax belongs to a folder not listed in the PathList file
	else if (!line.empty() && get_folders(path_to_row_map, line)) {
	  insert_fax_on_populate(path_to_row_map, file, line, fax_name);
	}
	else {
	  insert_fax_on_populate(path_to_row_map, file, base_path, fax_name);
	}
      }
    }
    // close the directory search
    while (closedir(dir_p) == -1 && errno == EINTR);

    // select the main folder
    std::string base_folder;
    if (mode == FaxListEnum::received) base_folder = gettext("Inbox");
    else base_folder = gettext("Sent box");

    GtkTreeIter row_iter;
    bool found_it = false;
    bool result = gtk_tree_model_get_iter_first(folder_tree_store_h, &row_iter);
    while (!found_it && result) {
      gchar* name_p = 0;
      gtk_tree_model_get(folder_tree_store_h, &row_iter,
			 FolderListModelColumns::name, &name_p,
			 -1);
      if (name_p && base_folder == std::string(name_p)) found_it = true;
      else result = gtk_tree_model_iter_next(folder_tree_store_h, &row_iter);
      // we have not placed the gchar* string given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free it by hand
      g_free(name_p);
    }
    if (found_it) {
      GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
      gtk_tree_selection_select_iter(selection_p, &row_iter);
      display_faxes();
    }
  }
  // reset current directory
  std::string temp(prog_config.working_dir + "/faxin");
  chdir(temp.c_str());
}

bool FaxListManager::get_folders(PathToFolderRowMap& path_to_row_map, const std::string& line) {

  bool return_val;
  if (!line.empty())  return_val = true;
  else return_val = false;

  std::vector<std::string> child_rowlist;
  std::string remaining_path(line);

  if (!Utf8::validate(line)) { // not valid UTF-8!
    write_error("Invalid UTF-8 string in folder path name in FaxListManager::get_folders()\n");
    remaining_path = "";
    return_val = false;
  }

  // keep going until we get to the root tree store or to a parent row in existence
  while (!remaining_path.empty() && path_to_row_map.find(remaining_path) == path_to_row_map.end()) {
    std::string::size_type pos = remaining_path.rfind(PATH_DIVIDER);
    if (pos == std::string::npos) {  // input error
      remaining_path = "";
      child_rowlist.clear();
      return_val = false;
    }
    else {
      child_rowlist.push_back(remaining_path.substr(pos + 1));
      folder_name_validator.insert_folder_name(remaining_path.substr(pos + 1));
      remaining_path.resize(pos);
    }
  }

  // now create the missing child level(s) stored in child_rowlist
  if (!child_rowlist.empty()) {

    std::vector<std::string>::reverse_iterator r_iter = child_rowlist.rbegin();
    TreeRowRefSharedHandle row_ref_h;
    std::string pathname(remaining_path);
    pathname += PATH_DIVIDER;
    pathname += *r_iter;

    if (remaining_path.empty()) { // we need to make a first level node
      GtkTreeIter iter;
      gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()), &iter, 0);

      // no need for a UTF8 conversion function here - we store folder
      // names in Path in UTF-8
      gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &iter,
			 FolderListModelColumns::icon, folder_icon_h.get(),
			 FolderListModelColumns::name, r_iter->c_str(),
			 FolderListModelColumns::root_only, false,
			 -1);
      
      TreePathScopedHandle path_h(gtk_tree_model_get_path(folder_tree_store_h,
							  &iter));
      row_ref_h = TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
								    path_h));
      path_to_row_map.insert(PathToFolderRowMap::value_type(pathname, row_ref_h));
      // create a fax tree model for the folder and put it in a GobjHandle to handle its
      // lifetime, because it is not owned by a GTK+ container
      GobjHandle<GtkTreeModel> model_h(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
									 G_TYPE_STRING,
									 G_TYPE_STRING,
									 G_TYPE_STRING)));
      gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model_h.get()),
					   FaxListModelColumns::name,
					   list_sort_type[mode]);
      folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(row_ref_h, model_h));
    }
    else {                        // build on the nearest ancestor already created
      row_ref_h = path_to_row_map[remaining_path];
      GtkTreeIter parent_iter;
      GtkTreeIter child_iter;
      TreePathScopedHandle parent_path_h(gtk_tree_row_reference_get_path(row_ref_h));
      if (parent_path_h.get() &&
	  gtk_tree_model_get_iter(folder_tree_store_h, &parent_iter, parent_path_h)) {
	gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()),
			      &child_iter, &parent_iter);
	// no need for a UTF8 conversion function here - we store folder
	// names in Path in UTF-8
	gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &child_iter,
			   FolderListModelColumns::icon, folder_icon_h.get(),
			   FolderListModelColumns::name, r_iter->c_str(),
			   FolderListModelColumns::root_only, false,
			   -1);
      
	TreePathScopedHandle child_path_h(gtk_tree_model_get_path(folder_tree_store_h,
								  &child_iter));
	row_ref_h = TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
								      child_path_h));
	path_to_row_map.insert(PathToFolderRowMap::value_type(pathname, row_ref_h));
	// create a fax tree model for the folder and put it in a GobjHandle to handle its
	// lifetime, because it is not owned by a GTK+ container
	GobjHandle<GtkTreeModel> model_h(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING)));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model_h.get()),
					     FaxListModelColumns::name,
					     list_sort_type[mode]);
	folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(row_ref_h, model_h));
      }
      else {
	write_error("Mapping error in FaxListManager::get_folders()\n");
	return_val = false;
	child_rowlist.clear();
	r_iter = child_rowlist.rbegin();
      }
    }
    // now deal with any further children remaining to be created
    for (++r_iter; r_iter != child_rowlist.rend(); ++r_iter) {
      GtkTreeIter parent_iter;
      GtkTreeIter child_iter;
      TreePathScopedHandle parent_path_h(gtk_tree_row_reference_get_path(row_ref_h));
      if (parent_path_h.get() &&
	  gtk_tree_model_get_iter(folder_tree_store_h, &parent_iter, parent_path_h)) {
	gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()),
			      &child_iter, &parent_iter);
	// no need for a UTF8 conversion function here - we store folder
	// names in Path in UTF-8
	gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &child_iter,
			   FolderListModelColumns::icon, folder_icon_h.get(),
			   FolderListModelColumns::name, r_iter->c_str(),
			   FolderListModelColumns::root_only, false,
			   -1);
      
	pathname += PATH_DIVIDER;
	pathname += *r_iter;
	TreePathScopedHandle child_path_h(gtk_tree_model_get_path(folder_tree_store_h,
								  &child_iter));
	row_ref_h = TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
								      child_path_h));
	path_to_row_map.insert(PathToFolderRowMap::value_type(pathname, row_ref_h));
	// create a fax tree model for the folder and put it in a GobjHandle to handle its
	// lifetime, because it is not owned by a GTK+ container
	GobjHandle<GtkTreeModel> model_h(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING)));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model_h.get()),
					     FaxListModelColumns::name,
					     list_sort_type[mode]);
	folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(row_ref_h, model_h));
      }
      else {
	write_error("Mapping error in FaxListManager::get_folders()\n");
	return_val = false;
	child_rowlist.clear();
	r_iter = child_rowlist.rbegin();
      }
    }
  }
  return return_val;
}

void FaxListManager::insert_fax_on_populate(const PathToFolderRowMap& path_to_row_map,
					    std::ifstream& file,
					    const std::string& fax_path,
					    const std::string& fax_name) {
  
  PathToFolderRowMap::const_iterator path_map_iter = path_to_row_map.find(fax_path);
  if (path_map_iter == path_to_row_map.end()) {
    write_error("Mapping error (no folder row) in FaxListManager::insert_fax_on_populate()\n");
  }
  else {
    FolderRowToFaxModelMap::const_iterator row_map_iter =
      folder_row_to_fax_model_map.find(path_map_iter->second);
    if (row_map_iter == folder_row_to_fax_model_map.end()) {
      write_error("Mapping error in FaxListManager::insert_fax_on_populate()\n");
    }
    else {
	
      GobjHandle<GtkTreeModel> fax_model_h(row_map_iter->second);

      GtkTreeIter row_iter;
      gtk_list_store_append(GTK_LIST_STORE(fax_model_h.get()), &row_iter);
      gtk_list_store_set(GTK_LIST_STORE(fax_model_h.get()), &row_iter,
			 FaxListModelColumns::name, fax_name.c_str(),
			 -1);

      // convert faxname to locale date-time format for fax date
      std::string date(convert_faxname_to_date(fax_name));
  
      try {
	gtk_list_store_set(GTK_LIST_STORE(fax_model_h.get()), &row_iter,
			   FaxListModelColumns::fax_date,
			   Utf8::locale_to_utf8(date).c_str(),
			   -1);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in FaxListManager::insert_fax_on_populate()\n");
      }

      // now see if there is a description
      std::string line;
      if (file) {
	while (std::getline(file, line) && line.empty());
      }
      if (!line.empty()) {
	if (Utf8::validate(line)) {
	  // no filename to UTF-8 conversion is required as the fax description is
	  // stored in UTF-8 (since version 3.0.7 of efax-gtk)
	  gtk_list_store_set(GTK_LIST_STORE(fax_model_h.get()), &row_iter,
			     FaxListModelColumns::fax_description,
			     line.c_str(),
			     -1);
	}
	else {
	  //write_error("Invalid UTF-8 in fax description in FaxListManager::insert_fax_on_populate()\n");
	}
      }
    }
  }
}

void FaxListManager::insert_new_fax_in_base(const std::string& fax_name,
					    const std::string& description) {

  GtkTreeIter row_iter;
  gtk_list_store_append(GTK_LIST_STORE(fax_base_model_h.get()), &row_iter);
  gtk_list_store_set(GTK_LIST_STORE(fax_base_model_h.get()), &row_iter,
		     FaxListModelColumns::name, fax_name.c_str(),
		     -1);

  // convert faxname to locale date-time format for fax date
  std::string date(convert_faxname_to_date(fax_name));
  
  try {
    gtk_list_store_set(GTK_LIST_STORE(fax_base_model_h.get()), &row_iter,
		       FaxListModelColumns::fax_date,
		       Utf8::locale_to_utf8(date).c_str(),
		       -1);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in FaxListManager::insert_new_fax_in_base()\n");
  }

  // now insert any description
  if (!description.empty()) {
    // there is no need to convert the fax description to UTF-8 as it is
    // already in that codeset - but let's check
    if (Utf8::validate(description)) {
      gtk_list_store_set(GTK_LIST_STORE(fax_base_model_h.get()), &row_iter,
			 FaxListModelColumns::fax_description,
			 description.c_str(),
			 -1);
    }
    else {
      write_error("Invalid UTF-8 in fax description in FaxListManager::insert_new_fax_in_base()\n");
    }
  }
}

void FaxListManager::move_fax(const GtkTreeIter* dest_folder_row_iter_p) {

  // check pre-conditions
  if (fax_drag_source_row_refs.empty()) return;

  // compare the destination fax tree model with the source fax tree model
  TreePathScopedHandle dest_folder_path_h(gtk_tree_model_get_path(folder_tree_store_h,
					     const_cast<GtkTreeIter*>(dest_folder_row_iter_p)));
  TreeRowRefSharedHandle dest_folder_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
									  dest_folder_path_h));
  FolderRowToFaxModelMap::const_iterator row_map_iter =
    folder_row_to_fax_model_map.find(dest_folder_row_ref_h);
  if (row_map_iter == folder_row_to_fax_model_map.end()) {
    write_error("Mapping error in FaxListManager::move_fax()\n");
    return;
  }
  GobjHandle<GtkTreeModel> dest_model_h = row_map_iter->second;
  // the source tree model must be the one currently embedded in the fax tree view
  GtkTreeModel* source_model_p = gtk_tree_view_get_model(fax_tree_view_p);

  if (source_model_p != dest_model_h.get()) {
  
    RowRefList::iterator refs_iter;
    for (refs_iter = fax_drag_source_row_refs.begin();
	 refs_iter != fax_drag_source_row_refs.end(); ++refs_iter) {

      GtkTreeIter old_fax_row_iter;
      TreePathScopedHandle old_fax_path_h(gtk_tree_row_reference_get_path(*refs_iter));
      if (old_fax_path_h.get() &&
	  gtk_tree_model_get_iter(source_model_p, &old_fax_row_iter, old_fax_path_h)) {
	
	gchar* date_p = 0;
	gchar* description_p = 0;
	gchar* name_p = 0;
	gtk_tree_model_get(source_model_p, &old_fax_row_iter,
			   FaxListModelColumns::fax_date, &date_p,
			   FaxListModelColumns::fax_description, &description_p,
			   FaxListModelColumns::name, &name_p,
			   -1);
	std::string description;
	if (description_p) description = description_p;
	std::string date;
	if (date_p) date = date_p;
	std::string faxname;
	if (name_p) faxname = name_p;
	
	// we have not placed the gchar* strings given by gtk_tree_model_get()
	// in a GcharScopedHandle object so we need to free them by hand
	g_free(date_p);
	g_free(description_p);
	g_free(name_p);

	GtkTreeIter new_fax_row_iter;
	gtk_list_store_append(GTK_LIST_STORE(dest_model_h.get()), &new_fax_row_iter);
	gtk_list_store_set(GTK_LIST_STORE(dest_model_h.get()), &new_fax_row_iter,
			   FaxListModelColumns::fax_date, date.c_str(),
			   FaxListModelColumns::fax_description, description.c_str(),
			   FaxListModelColumns::name, faxname.c_str(),
			   -1);
	gtk_list_store_remove(GTK_LIST_STORE(gtk_tree_view_get_model(fax_tree_view_p)),
			      &old_fax_row_iter);
      }
      else {
	write_error("Database mapping error in FaxListManager::move_fax()\n");
      }
    }
    fax_drag_source_row_refs.clear();
  }
}

void FaxListManager::move_folder(GtkTreeIter* dest_folder_row_iter_p,
				 GtkTreeViewDropPosition drop_pos) {
  bool dragging_trash_folder = false;

  { // scope block for path handles
    TreePathScopedHandle source_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							       &folder_drag_source_row_iter));
    TreePathScopedHandle trash_path_h(gtk_tree_row_reference_get_path(trash_row_ref_h));
    if (source_path_h == trash_path_h) dragging_trash_folder = true;
  }
  
  gboolean is_root_only = false;
  gchar* name_p = 0;
  gtk_tree_model_get(folder_tree_store_h, &folder_drag_source_row_iter,
		     FolderListModelColumns::root_only, &is_root_only,
		     FolderListModelColumns::name, &name_p,
		     -1);
  std::string foldername;
  if (name_p) foldername = name_p;
  // we have not placed the gchar* string given by gtk_tree_model_get()
  // in a GcharScopedHandle object so we need to free it by hand
  g_free(name_p);
  
  std::string source_pathname(get_pathname_for_folder(&folder_drag_source_row_iter));
  std::string drop_pathname(get_pathname_for_folder(dest_folder_row_iter_p));

  if (source_pathname != drop_pathname) {
    // get the new pathname for the folder and put it in new_pathname
    std::string new_pathname(drop_pathname);
    std::string::size_type pos;
    if (drop_pos == GTK_TREE_VIEW_DROP_BEFORE
	|| drop_pos == GTK_TREE_VIEW_DROP_AFTER) {
      if ((pos = new_pathname.rfind(PATH_DIVIDER)) != std::string::npos) {
	new_pathname.resize(pos);
      }
      else {
	write_error("Folder with no pathname in FaxListManager::move_folder()\n");
	new_pathname = "";
      }
    }
    new_pathname += PATH_DIVIDER;
    new_pathname += foldername;

    if (is_valid_drop_path(source_pathname, new_pathname)
	&& (!is_root_only
	    || new_pathname.rfind(PATH_DIVIDER) == 0)) {
      GtkTreeIter new_row_iter;
      switch (drop_pos) {
      case GTK_TREE_VIEW_DROP_BEFORE:
	gtk_tree_store_insert_before(GTK_TREE_STORE(folder_tree_store_h.get()),
				     &new_row_iter, 0,
				     dest_folder_row_iter_p);
	break;
      case GTK_TREE_VIEW_DROP_AFTER:
	gtk_tree_store_insert_after(GTK_TREE_STORE(folder_tree_store_h.get()),
				    &new_row_iter, 0,
				    dest_folder_row_iter_p);
	break;
      default:
	gtk_tree_store_prepend(GTK_TREE_STORE(folder_tree_store_h.get()),
			       &new_row_iter, dest_folder_row_iter_p);
	break;
      }

      if (dragging_trash_folder) {
	// although the Trash folder is root only, it can still be dragged
	// within the root level (as can the Inbox/Sent faxes boxes)
	gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &new_row_iter,
			   FolderListModelColumns::icon, trash_icon_h.get(),
			   FolderListModelColumns::name, foldername.c_str(),
			   FolderListModelColumns::root_only, true,
			   -1);
	TreePathScopedHandle new_path_h(gtk_tree_model_get_path(folder_tree_store_h,
								&new_row_iter));
	trash_row_ref_h = TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
									    new_path_h));
      }
      else {
	gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &new_row_iter,
			   FolderListModelColumns::icon, folder_icon_h.get(),
			   FolderListModelColumns::name, foldername.c_str(),
			   FolderListModelColumns::root_only, is_root_only,
			   -1);
      }

      TreePathScopedHandle source_path_h(gtk_tree_model_get_path(folder_tree_store_h,
								 &folder_drag_source_row_iter));
      TreeRowRefSharedHandle source_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
									 source_path_h));
      TreePathScopedHandle new_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							      &new_row_iter));
      TreeRowRefSharedHandle new_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
								      new_path_h));
      FolderRowToFaxModelMap::iterator row_map_iter =
	folder_row_to_fax_model_map.find(source_row_ref_h);
      if (row_map_iter == folder_row_to_fax_model_map.end()) {
	write_error("Mapping error in FaxListManager::move_folder()\n");
	return;
      }

      // assigning the tree model to a new handle will keep it alive
      // when we erase it from the map
      GobjHandle<GtkTreeModel> fax_model_h = row_map_iter->second;
      // remap folder_row_to_fax_model_map
      folder_row_to_fax_model_map.erase(row_map_iter);
      folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(new_row_ref_h,
									    fax_model_h));

      // get the moved folder to show to its dropped level (do this before the call
      // to move_child_folders_for_level() or it may be invalidated)
      if (gtk_tree_path_up(new_path_h)) {
	gtk_tree_view_expand_to_path(folder_tree_view_p, new_path_h);
      }

      // now move all the children of the moved folder to rejoin their parent
      move_child_folders_for_level(&folder_drag_source_row_iter, &new_row_iter);

      GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
      gtk_tree_selection_select_iter(selection_p, &new_row_iter);
      
      gtk_tree_store_remove(GTK_TREE_STORE(folder_tree_store_h.get()),
			    &folder_drag_source_row_iter);
    }
  }
}

bool FaxListManager::is_valid_drop_path(const std::string& old_pathname,
					const std::string& new_pathname) {

  // check that the folder is not being dropped on one of its children -
  // with drag and drop syncing issues this can happen
  if (new_pathname.find(old_pathname) == 0
      && new_pathname.size() > old_pathname.size()) { // we can have the new pathname the same
                                                      // as the old pathname - say if folder
                                                      // is being moved at same level
    return false;
  }
  return true;
}

void FaxListManager::move_child_folders_for_level(GtkTreeIter* source_level_p,
						  GtkTreeIter* dest_level_p) {

  GtkTreeIter source_child_iter;
  bool valid = gtk_tree_model_iter_children(folder_tree_store_h, &source_child_iter, source_level_p);

  while (valid) {
    gchar* name_p = 0;
    gtk_tree_model_get(folder_tree_store_h, &source_child_iter,
		       FolderListModelColumns::name, &name_p,
		       -1);
    std::string node_name;      
    if (name_p) node_name = name_p;
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(name_p);

    GtkTreeIter dest_row_iter;
    gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()),
			  &dest_row_iter, dest_level_p);

    gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &dest_row_iter,
		       FolderListModelColumns::icon, folder_icon_h.get(),
		       FolderListModelColumns::name, node_name.c_str(),
		       // as we are not at the root level we know the value of
		       // FolderListModelColumns::root_only must be false
		       FolderListModelColumns::root_only, false,
		       -1);

    TreePathScopedHandle source_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							       &source_child_iter));
    TreeRowRefSharedHandle source_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
								       source_path_h));

    TreePathScopedHandle dest_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							      &dest_row_iter));
    TreeRowRefSharedHandle dest_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
								     dest_path_h));

    FolderRowToFaxModelMap::iterator row_map_iter =
      folder_row_to_fax_model_map.find(source_row_ref_h);
    if (row_map_iter == folder_row_to_fax_model_map.end()) {
      write_error("Mapping error in FaxListManager::move_child_folders_for_level()\n");
      return;
    }

    // assigning the tree model to a new handle will keep it alive
    // when we erase it from the map
    GobjHandle<GtkTreeModel> fax_model_h = row_map_iter->second;
    // remap folder_row_to_fax_model_map
    folder_row_to_fax_model_map.erase(row_map_iter);
    folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(dest_row_ref_h,
									  fax_model_h));
      
    // now recursively work the way up children of this node (if any)
    move_child_folders_for_level(&source_child_iter, &dest_row_iter);
      
    valid = gtk_tree_model_iter_next(folder_tree_store_h, &source_child_iter);
  }
}

std::string FaxListManager::get_pathname_for_folder(const GtkTreeIter* folder_iter) {

  GtkTreeIter child_iter = *folder_iter;
  GtkTreeIter parent_iter;
  std::string pathname;
  std::string temp;
  bool result = true;

  while (result) {
    temp = PATH_DIVIDER;
    gchar* name_p = 0;
    // we don't need a Glib conversion function here - we will use/store the Path to file
    // in UTF-8 - the std::string is just a transparent container for the encoding
    gtk_tree_model_get(folder_tree_store_h, &child_iter,
		       FolderListModelColumns::name, &name_p,
		       -1);
    if (name_p) temp += name_p;
    pathname.insert(0, temp);
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(name_p);

    result = gtk_tree_model_iter_parent(folder_tree_store_h, &parent_iter, &child_iter);
    child_iter = parent_iter;
  }
  return pathname;
}

void FaxListManager::display_faxes(void) {

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (!gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    return;
  }
  TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p, &selected_iter));
  TreeRowRefSharedHandle folder_row_ref_h(gtk_tree_row_reference_new(model_p,
								     path_h));
  FolderRowToFaxModelMap::const_iterator row_map_iter =
    folder_row_to_fax_model_map.find(folder_row_ref_h);
  if (row_map_iter == folder_row_to_fax_model_map.end()) {
    write_error("Mapping error in FaxListManager::display_faxes()\n");
  }

  else {
    gtk_tree_view_set_model(fax_tree_view_p, row_map_iter->second);
    // provide a sort indicator - we need to do it here as setting
    // the tree model will unset the sort indicator (a bug in GTK+?)
    gtk_tree_view_column_set_sort_indicator(fax_date_column_p, true);
    // this doesn't control the sort order (the previous calls to
    // gtk_tree_sortable_set_sort_column_id() on the tree models did that)
    // - it just sets the direction that the arrow in the tree view shows!
    gtk_tree_view_column_set_sort_order(fax_date_column_p, list_sort_type[mode]);
  }
}

void FaxListManager::empty_trash_folder(void) {

  FolderRowToFaxModelMap::iterator row_map_iter =
    folder_row_to_fax_model_map.find(trash_row_ref_h);
  if (row_map_iter == folder_row_to_fax_model_map.end()) {
    write_error("Mapping error in FaxListManager::empty_trash_folder()\n");
    return;
  }

  GobjHandle<GtkTreeModel> fax_model_h = row_map_iter->second;
  GtkTreeIter trash_iter;
  if (!gtk_tree_model_get_iter_first(fax_model_h, &trash_iter)) return; // empty folder

  std::string dirname(prog_config.working_dir);
  if (mode == FaxListEnum::received) dirname += "/faxin/";
  else dirname += "/faxsent/";

  bool valid = true;  // we have already tested for the first element above

  while (valid) {
    
    std::string faxdir(dirname);
    gchar* name_p = 0;
    gtk_tree_model_get(fax_model_h, &trash_iter,
		       FaxListModelColumns::name, &name_p,
		       -1);
    // we don't need to use a Glib conversion function here - we know the
    // fax name is just plain ASCII numbers
    if (name_p) faxdir += name_p;
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(name_p);

    struct dirent* direntry;
    struct stat statinfo;

    DIR* dir_p;
    if ((dir_p = opendir(faxdir.c_str())) == 0) {
      std::string msg("Can't open directory ");
      msg += faxdir;
      msg += '\n';
      write_error(msg.c_str());
    }

    else {
      chdir(faxdir.c_str());
      while ((direntry = readdir(dir_p)) != 0) {
	stat(direntry->d_name, &statinfo);
	if (S_ISREG(statinfo.st_mode)) unlink(direntry->d_name);
      }

      while (closedir(dir_p) == -1 && errno == EINTR);
      // reset current directory
      std::string temp(prog_config.working_dir + "/faxin");
      chdir(temp.c_str());

      if (rmdir(faxdir.c_str())) {
	std::string msg("Can't delete directory ");
	msg += faxdir;
	msg += "\nThe contents should have been deleted\n"
	       "and it should now be empty -- please check\n";
	write_error(msg.c_str());
      }
    }
    
    valid = gtk_tree_model_iter_next(fax_model_h, &trash_iter);
  }

  gtk_list_store_clear(GTK_LIST_STORE(fax_model_h.get()));

  // we don't need to write out the paths: the Path files for all the faxes
  // previously in the trash folder will have been deleted and the PathList
  // file will remain the same
}

RowPathList::size_type FaxListManager::is_fax_selected(void) {

  SelectedRowsHandle rows_handle(fax_tree_view_p);
  return rows_handle.size();
}

bool FaxListManager::is_selected_folder_empty(void) {
  bool return_val = false;
  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p, &row_iter));
    if (!gtk_tree_model_iter_has_child(model_p, &row_iter)) { // check if there is a child folder

      TreeRowRefSharedHandle folder_row_ref_h(gtk_tree_row_reference_new(model_p,
									 path_h));
      FolderRowToFaxModelMap::const_iterator row_map_iter =
	folder_row_to_fax_model_map.find(folder_row_ref_h);
      if (row_map_iter == folder_row_to_fax_model_map.end()) {
	write_error("Mapping error in FaxListManager::is_selected_folder_empty()\n");
      }
      else if (!gtk_tree_model_get_iter_first(row_map_iter->second, &row_iter)) { // empty folder
	return_val = true;
      }
    }    
  }
  return return_val;
}

bool FaxListManager::is_selected_folder_permanent(void) {

  gboolean is_root_only = false;
  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    gtk_tree_model_get(model_p, &row_iter,
		       FolderListModelColumns::root_only, &is_root_only,
		       -1);
  }
  return is_root_only;
}

bool FaxListManager::show_trash_folder_icon(void) {

  // we want to show the relevant button icon if the trash folder is selected
  // and it is not empty
  bool return_val = false;

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    TreePathScopedHandle row_path_h(gtk_tree_model_get_path(model_p, &row_iter));
    TreePathScopedHandle trash_path_h(gtk_tree_row_reference_get_path(trash_row_ref_h));

    if (trash_path_h.get() && row_path_h == trash_path_h) {
      // trash folder selected
      // now check if it has faxes in it
      FolderRowToFaxModelMap::iterator row_map_iter =
	folder_row_to_fax_model_map.find(trash_row_ref_h);
      if (row_map_iter == folder_row_to_fax_model_map.end()) {
	write_error("Mapping error in FaxListManager::show_trash_folder_icon()\n");
      }
      else if (gtk_tree_model_get_iter_first(row_map_iter->second, &row_iter)) { // faxes in folder
	return_val = true;
      }
    }
  }
  return return_val;
}

void FaxListManager::make_folder(const std::string& folder_name, bool test_valid) {

  if (!test_valid || folder_name_validator.validate(folder_name).first) {

    GtkTreeIter iter;
    gtk_tree_store_append(GTK_TREE_STORE(folder_tree_store_h.get()), &iter, 0);

    gtk_tree_store_set(GTK_TREE_STORE(folder_tree_store_h.get()), &iter,
		       FolderListModelColumns::icon, folder_icon_h.get(),
		       FolderListModelColumns::name, folder_name.c_str(),
		       // since we are creating a new folder it is not root only
		       // (only Inbox, Sent box and Trash are root only)
		       FolderListModelColumns::root_only, false,
		       -1);
    folder_name_validator.insert_folder_name(folder_name);

    TreePathScopedHandle path_h(gtk_tree_model_get_path(folder_tree_store_h,
							&iter));
    TreeRowRefSharedHandle row_ref_h =
      TreeRowRefSharedHandle(gtk_tree_row_reference_new(folder_tree_store_h,
							path_h));
    // create a fax tree model for the folder and put it in a GobjHandle to handle its
    // lifetime, because it is not owned by a GTK+ container
    GobjHandle<GtkTreeModel> model_h(GTK_TREE_MODEL(gtk_list_store_new(FaxListModelColumns::cols_num,
								       G_TYPE_STRING,
								       G_TYPE_STRING,
								       G_TYPE_STRING)));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model_h.get()),
					 FaxListModelColumns::name,
					 list_sort_type[mode]);
    folder_row_to_fax_model_map.insert(FolderRowToFaxModelMap::value_type(row_ref_h, model_h));

    write_path();
  }
}

void FaxListManager::delete_folder(void) {

  // we should have called is_folder_empty() and is_selected_folder_permanent()
  // before getting here, so all we need to do is to delete the row in the
  // folder tree store and the entry in the map

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    gchar* name_p = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       FolderListModelColumns::name, &name_p,
		       -1);
    if (name_p) folder_name_validator.erase_folder_name(name_p);
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(name_p);

    TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p, &row_iter));
    TreeRowRefSharedHandle row_ref_h(gtk_tree_row_reference_new(model_p, path_h));
    FolderRowToFaxModelMap::iterator row_map_iter =
      folder_row_to_fax_model_map.find(row_ref_h);
    if (row_map_iter == folder_row_to_fax_model_map.end()) {
      write_error("Mapping error in FaxListManager::delete_folder()\n");
      return;
    }
    // this will also destroy the fax tree model for this folder
    folder_row_to_fax_model_map.erase(row_map_iter);

    // now remove the row in the folder tree model
    gtk_tree_store_remove(GTK_TREE_STORE(model_p), &row_iter);
    write_path();
  }
}

void FaxListManager::delete_fax(void) {

  GtkTreeModel* fax_list_store_p = gtk_tree_view_get_model(fax_tree_view_p);
  RowRefList row_ref_list;
  SelectedRowsHandle rows_handle(fax_tree_view_p);
  rows_handle.get_ref_list(fax_list_store_p, row_ref_list);

  if (!row_ref_list.empty() && gtk_tree_row_reference_valid(row_ref_list.front())) {
    
    std::string dirname(prog_config.working_dir);
    if (mode == FaxListEnum::received) dirname += "/faxin/";
    else dirname += "/faxsent/";

    RowRefList::iterator refs_iter;
    for (refs_iter = row_ref_list.begin();
	 refs_iter != row_ref_list.end(); ++refs_iter) {

      GtkTreeIter row_iter;
      TreePathScopedHandle path_h(gtk_tree_row_reference_get_path(*refs_iter));
      if (path_h.get() &&
	  gtk_tree_model_get_iter(fax_list_store_p, &row_iter, path_h)) {

	gchar* name_p = 0;
	// get the name of the fax to be deleted
	// we don't need to use a Glib conversion function here - we know the
	// fax name is just plain ASCII numbers
	gtk_tree_model_get(fax_list_store_p, &row_iter,
			   FaxListModelColumns::name, &name_p,
			   -1);

	std::string faxname;
	if (name_p) faxname = name_p;
	// we have not placed the gchar* string given by gtk_tree_model_get()
	// in a GcharScopedHandle object so we need to free it by hand
	g_free(name_p);

	std::string faxdir(dirname);
	faxdir += faxname;

	struct dirent* direntry;
	struct stat statinfo;

	DIR* dir_p;
	if ((dir_p = opendir(faxdir.c_str())) == 0) {
	  std::string msg("Can't open directory ");
	  msg += faxdir;
	  msg += '\n';
	  write_error(msg.c_str());
	}

	else {
	  chdir(faxdir.c_str());
	  while ((direntry = readdir(dir_p)) != 0) {
	    stat(direntry->d_name, &statinfo);
	    if (S_ISREG(statinfo.st_mode)) unlink(direntry->d_name);
	  }

	  while (closedir(dir_p) == -1 && errno == EINTR);
	  // reset current directory
	  std::string temp(prog_config.working_dir + "/faxin");
	  chdir(temp.c_str());

	  if (rmdir(faxdir.c_str())) {
	    std::string msg("Can't delete directory ");
	    msg += faxdir;
	    msg += "\nThe contents should have been deleted\n"
	           "and it should now be empty -- please check\n";
	    write_error(msg.c_str());
	  }
	  else {
	    // delete the fax in the fax list
	    gtk_list_store_remove(GTK_LIST_STORE(fax_list_store_p), &row_iter);
	  }
	}
      }
    }
    // and write out the paths
    write_path();
  }
}

void FaxListManager::write_path(void) {

  std::string dir(prog_config.working_dir);
  if (mode == FaxListEnum::received) dir += "/faxin/";
  else dir += "/faxsent/";

  // first write the path list file for the folders
  std::string filename(dir);
  filename += "PathList";
  std::ofstream path_list_file(filename.c_str(), std::ios::out);

  if (path_list_file) {
    GtkTreeIter row_iter;
    if (gtk_tree_model_get_iter_first(folder_tree_store_h, &row_iter)) {
      write_paths_for_level(&row_iter, path_list_file);
    }
    else write_error("Tree model error in FaxListManager::write_path()\n");
  }
  else write_error("Cannot open PathList file for writing in FaxListManager::write_path()\n");
}

void FaxListManager::write_paths_for_level(const GtkTreeIter* level_iter_p,
					   std::ofstream& path_list_file) {

  std::string dir(prog_config.working_dir);
  if (mode == FaxListEnum::received) dir += "/faxin/";
  else dir += "/faxsent/";
  std::ofstream fax_path_file;

  GtkTreeIter folder_row_iter = *level_iter_p;
  bool folder_valid = true; // level_iter_p must be valid when first called - we
                            // cannot sensibly check it here
  while (folder_valid) {
    // write out PathList file
    std::string pathname = get_pathname_for_folder(&folder_row_iter);
    path_list_file << pathname << '\n';

    // now write out individual fax Path files for this folder/pathname
    // - first get the tree model for the faxes
    TreePathScopedHandle folder_path_h(gtk_tree_model_get_path(folder_tree_store_h,
							       &folder_row_iter));
    TreeRowRefSharedHandle folder_row_ref_h(gtk_tree_row_reference_new(folder_tree_store_h,
								       folder_path_h));
    FolderRowToFaxModelMap::const_iterator row_map_iter =
      folder_row_to_fax_model_map.find(folder_row_ref_h);
    if (row_map_iter == folder_row_to_fax_model_map.end()) {
      write_error("Mapping error in FaxListManager::write_paths_for_level()\n");
    }
    else {
      GobjHandle<GtkTreeModel> fax_model_h = row_map_iter->second;

      GtkTreeIter fax_row_iter;
      bool fax_valid = gtk_tree_model_get_iter_first(fax_model_h, &fax_row_iter);
      while (fax_valid) {

	fax_path_file.close();
	fax_path_file.clear();

	std::string filename(dir);
	gchar* name_p = 0;
	gtk_tree_model_get(fax_model_h, &fax_row_iter,
			   FaxListModelColumns::name, &name_p,
			   -1);
	// we don't need to use a Glib conversion function here - we know the
	// fax name is just plain ASCII numbers
	if (name_p) filename += name_p;
	filename += "/Path";
	// we have not placed the gchar* string given by gtk_tree_model_get()
	// in a GcharScopedHandle object so we need to free it by hand
	g_free(name_p);

	fax_path_file.open(filename.c_str(), std::ios::out);
	if (fax_path_file) fax_path_file << pathname << std::endl;
	else {
	  std::string message("Cannot open file ");
	  message += filename;
	  message += '\n';
	  write_error(message.c_str());
	}

	fax_valid = gtk_tree_model_iter_next(fax_model_h, &fax_row_iter);
      }
    }

    // now recursively work the way up children of this node (if any) of the folder tree model
    GtkTreeIter child_iter;
    if (gtk_tree_model_iter_children(folder_tree_store_h,
				     &child_iter,
				     &folder_row_iter)) {
      write_paths_for_level(&child_iter, path_list_file);
    }
    folder_valid = gtk_tree_model_iter_next(folder_tree_store_h, &folder_row_iter);
  }
}

void FaxListManager::describe_fax(const std::string& description) {

  SelectedRowsHandle rows_handle(fax_tree_view_p);

  if (!rows_handle.is_empty()) {

    GtkTreeIter row_iter;
    GtkTreeModel* fax_list_store_p = gtk_tree_view_get_model(fax_tree_view_p);
    if (gtk_tree_model_get_iter(fax_list_store_p, &row_iter,
				rows_handle.front())) {

      gtk_list_store_set(GTK_LIST_STORE(fax_list_store_p), &row_iter,
			 FaxListModelColumns::fax_description, description.c_str(),
			 -1);

      std::string filename(prog_config.working_dir);
      if (mode == FaxListEnum::received) filename += "/faxin/";
      else filename += "/faxsent/";

      gchar* name_p = 0;
      gtk_tree_model_get(fax_list_store_p, &row_iter,
			 FaxListModelColumns::name, &name_p,
			 -1);
      // we don't need to use a Glib conversion function here - we know the
      // fax name is just plain ASCII numbers
      if (name_p) filename += name_p;
      // we have not placed the gchar* string given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free it by hand
      g_free(name_p);

      filename += "/Description";
      std::ofstream file(filename.c_str(), std::ios::out);
      // do not convert the description to the locale codeset - save it in UTF-8
      // (with effect from version 3.0.7)
      if (file) file << description;
    
      else {
	std::string msg("Can't open file ");
	msg += filename;
	msg += '\n';
	write_error(msg.c_str());
      }
    }
  }
}

bool FaxListManager::are_selected_faxes_in_trash_folder(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    TreePathScopedHandle row_path_h(gtk_tree_model_get_path(model_p, &row_iter));
    TreePathScopedHandle trash_path_h(gtk_tree_row_reference_get_path(trash_row_ref_h));

    if (trash_path_h.get() && row_path_h == trash_path_h) {
      return true;
    }
  }
  return false;
}

void FaxListManager::move_selected_faxes_to_trash_folder(void) {

  // pretend we are doing a drag and drop

  // get the destination of the drop
  GtkTreeIter trash_iter;
  TreePathScopedHandle trash_path_h(gtk_tree_row_reference_get_path(trash_row_ref_h));
  if (trash_path_h.get() &&
      gtk_tree_model_get_iter(folder_tree_store_h, &trash_iter, trash_path_h)) {

    // these are the faxes to be dropped
    SelectedRowsHandle rows_handle(fax_tree_view_p);

    rows_handle.get_ref_list(gtk_tree_view_get_model(fax_tree_view_p),
			     fax_drag_source_row_refs);

    // now move the faxes using the pretend drop
    move_fax(&trash_iter);
  }
  else write_error("Selection error in FaxListManager::move_selected_fax_to_trash_folder()\n");
}

GcharSharedHandle FaxListManager::get_fax_number(void) {

  gchar* number_p = 0;
  SelectedRowsHandle rows_handle(fax_tree_view_p);

  if (!rows_handle.is_empty()) {
    GtkTreeModel* fax_list_store_p = gtk_tree_view_get_model(fax_tree_view_p);
    GtkTreeIter row_iter;
    if (gtk_tree_model_get_iter(fax_list_store_p, &row_iter,
				rows_handle.front())) {
      gtk_tree_model_get(fax_list_store_p, &row_iter,
			 FaxListModelColumns::name, &number_p,
			 -1);
    }
  }
  return GcharSharedHandle(number_p);
}

GcharSharedHandle FaxListManager::get_fax_description(void) {

  gchar* description_p = 0;
  SelectedRowsHandle rows_handle(fax_tree_view_p);

  if (!rows_handle.is_empty()) {
    GtkTreeModel* fax_list_store_p = gtk_tree_view_get_model(fax_tree_view_p);
    GtkTreeIter row_iter;
    if (gtk_tree_model_get_iter(fax_list_store_p, &row_iter,
				rows_handle.front())) {
      gtk_tree_model_get(fax_list_store_p, &row_iter,
			 FaxListModelColumns::fax_description, &description_p,
			 -1);
    }
  }
  return GcharSharedHandle(description_p);
}


GcharSharedHandle FaxListManager::get_folder_name(void) {

  GtkTreeIter iter;
  GtkTreeModel* model_p;
  gchar* name_p = 0;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &iter)) {
    gtk_tree_model_get(model_p, &iter,
		       FolderListModelColumns::name, &name_p,
		       -1);
  }
  return GcharSharedHandle(name_p);
}

bool FaxListManager::is_folder_selected(void) {

  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(folder_tree_view_p);
  return gtk_tree_selection_get_selected(selection_p, 0, 0);
}

std::string FaxListManager::convert_faxname_to_date(const std::string& faxname) {

  struct std::tm t;
  char str[150];
  
  // init
  std::memset(&t, 0, sizeof(struct std::tm));
  std::memset(str, 0, sizeof(str));
  // convert date --> struct tm
  if (faxname.size() > 12) { // the "new" format for creating fax directories
    strptime(faxname.c_str(), "%Y%m%d%H%M%S", &t);
  }
  else {                     // the "old" format for creating fax directories
    strptime(faxname.c_str(), "%y%m%d%H%M%S", &t);
  }

  // convert tm to date and time in current locale
  std::strftime(str, sizeof(str), "%x %H:%M", &t);
  return std::string(str);
}
