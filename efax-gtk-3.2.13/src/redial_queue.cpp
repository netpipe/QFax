/* Copyright (C) 2010 Chris Vine

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

#include <algorithm>
#include <functional>

#include <c++-gtk-utils/callback.h>
#include <c++-gtk-utils/mem_fun.h>

#include "redial_queue.h"
#include "dialogs.h"
#include "redial_queue_icons.h"
#include "utils/toolbar_append_widget.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

int RedialQueueDialog::is_redial_queue_dialog = 0;

void RedialQueue::add(const QueueItem& item) {
  item_list.push_back(item);
  changed.emit();
}

bool RedialQueue::remove(guint* id) {
  // we don't have lambdas in C++03, so use a struct in function scope
  struct Pred {
    // we can't pass const reference types as we bind with std::bind2nd below
    static bool pred(QueueItem item, guint* p) {
      return (item.first == p);
    }
  };
  
  std::list<QueueItem>::iterator iter =
    std::find_if(item_list.begin(), item_list.end(),
		 std::bind2nd(std::ptr_fun(Pred::pred), id));
  if (iter != item_list.end()) {
    item_list.erase(iter);
    g_source_remove(*id);
    delete id;
    changed.emit();
    return true;
  }
  return false;
}

void RedialQueue::remove_all(void) {
  // we don't have lambdas in C++03, so use a struct in function scope
  struct RemoveItem {
    static void exec(const QueueItem& item) {g_source_remove(*item.first);
                                             delete item.first;}
  };
  if (item_list.empty()) return; // we don't want to emit 'changed' unnecessarily
  std::for_each(item_list.begin(), item_list.end(),
                RemoveItem::exec);
  item_list.clear();
  changed.emit();
}

void RedialQueue::show_dialog(void) {
  if (!RedialQueueDialog::get_is_redial_queue_dialog()) {
    dialog_p = new RedialQueueDialog(standard_size, this);

    changed.connect(Callback::make(*dialog_p, &RedialQueueDialog::refresh), dialog_p->releaser);
    // there is no memory leak -- RedialQueueDialog is modeless and will delete its own memory
    // when it is closed
  }
  else gtk_window_present(GTK_WINDOW(dialog_p->get_win()));
}

void RedialQueueDialogCB::redial_queue_dialog_button_clicked(GtkWidget* widget_p, void* data) {
  RedialQueueDialog* instance_p = static_cast<RedialQueueDialog*>(data);
  
  if (widget_p == instance_p->remove_button_p) {
    instance_p->remove_prompt();
  }
  else if (widget_p == instance_p->close_button_p) {
    instance_p->close();
  }
  else {
    write_error("Callback error in RedialQueueDialogCB::redial_queue_dialog_button_clicked()\n");
  }
}

void RedialQueueDialogCB::redial_queue_dialog_set_button(GtkTreeSelection*, void* data) {
  RedialQueueDialog* instance_p = static_cast<RedialQueueDialog*>(data);

  // see if anything is selected
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(instance_p->tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, 0, 0)) {
    gtk_widget_set_sensitive(instance_p->remove_button_p, true);
    gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NORMAL);
  }
    
  else {
    gtk_widget_set_sensitive(instance_p->remove_button_p, false);
    gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NONE);
  }
}

namespace {
namespace ModelColumns {
  enum {dest_number, id, cols_num};
} // namespace ModelColumns
} // anonymous namespace

RedialQueueDialog::RedialQueueDialog(const int size, RedialQueue* redial_queue_p_):
                                       WinBase(gettext("efax-gtk: Redial queue"),
					       prog_config.window_icon_h),
				       standard_size(size),
				       redial_queue_p(redial_queue_p_) {

  ++is_redial_queue_dialog;

  remove_button_p = gtk_button_new();
  close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* queue_dialog_box_p = gtk_vbox_new(false, 0);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));
  GtkScrolledWindow* queue_dialog_scrolled_window_p
    = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  GtkToolbar* toolbar_p = (GTK_TOOLBAR(gtk_toolbar_new()));


  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);
  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  gtk_scrolled_window_set_shadow_type(queue_dialog_scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(queue_dialog_scrolled_window_p,
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  // create the tree model
  list_store_h =
    GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_list_store_new(ModelColumns::cols_num,
							       G_TYPE_STRING, G_TYPE_POINTER)));
  // populate the tree model
  populate();

  // create the tree view
  tree_view_p = GTK_TREE_VIEW(gtk_tree_view_new_with_model(list_store_h));
  gtk_container_add(GTK_CONTAINER(queue_dialog_scrolled_window_p), GTK_WIDGET(tree_view_p));

  // provide renderer for tree view, pack the fax_label tree view column
  // and connect to the tree model column(the fax_filename model column
  // is hidden and just contains data (a pointer to a GSource id) to which
  // the first column relates
  GtkCellRenderer* renderer_p = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column_p =
    gtk_tree_view_column_new_with_attributes(gettext("Number"), renderer_p,
					     "text", ModelColumns::dest_number,
					     static_cast<void*>(0));
  gtk_tree_view_append_column(tree_view_p, column_p);

  // single line selection
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  gtk_tree_selection_set_mode(selection_p, GTK_SELECTION_SINGLE);

  // set up the tool bar
#if GTK_CHECK_VERSION(2,16,0)
  gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar_p), GTK_ORIENTATION_HORIZONTAL);
#else
  gtk_toolbar_set_orientation(toolbar_p, GTK_ORIENTATION_HORIZONTAL);
#endif
  gtk_toolbar_set_style(toolbar_p, GTK_TOOLBAR_ICONS);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(remove_xpm));
    GtkWidget* image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(remove_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, remove_button_p,
			gettext("Remove fax from redial list"));
  gtk_widget_set_sensitive(remove_button_p, false);

  gtk_table_attach(table_p, GTK_WIDGET(queue_dialog_scrolled_window_p),
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/3, standard_size/3);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);

  gtk_box_pack_start(GTK_BOX(queue_dialog_box_p), GTK_WIDGET(toolbar_p), false, false, 0);
  gtk_box_pack_start(GTK_BOX(queue_dialog_box_p), GTK_WIDGET(table_p), true, true, 0);

  g_signal_connect(G_OBJECT(remove_button_p), "clicked",
		   G_CALLBACK(RedialQueueDialogCB::redial_queue_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(RedialQueueDialogCB::redial_queue_dialog_button_clicked), this);

  // now connect up the signal which indicates a selection has been made
  selection_p = gtk_tree_view_get_selection(tree_view_p);
  g_signal_connect(G_OBJECT(selection_p), "changed",
		   G_CALLBACK(RedialQueueDialogCB::redial_queue_dialog_set_button), this);


#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(close_button_p, true);
#else
  GTK_WIDGET_SET_FLAGS(close_button_p, GTK_CAN_DEFAULT);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(table_p), standard_size/3);
  gtk_container_add(GTK_CONTAINER(get_win()), queue_dialog_box_p);
  //gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(get_win(), standard_size * 10, standard_size * 10);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void RedialQueueDialog::populate_item(const QueueItem& item) {

  GtkTreeIter row_iter;
  gtk_list_store_append(GTK_LIST_STORE(list_store_h.get()), &row_iter);
  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &row_iter,
		     ModelColumns::dest_number, item.second.c_str(),
		     ModelColumns::id, item.first,
		     -1);
}

void RedialQueueDialog::populate(void) {

 std::for_each(redial_queue_p->item_list.begin(), redial_queue_p->item_list.end(),
               MemFun::make(*this, &RedialQueueDialog::populate_item));
}

void RedialQueueDialog::refresh(void) {

  gtk_list_store_clear(GTK_LIST_STORE(list_store_h.get()));
  populate();
}

void RedialQueueDialog::remove_prompt(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    gchar* number = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::dest_number, &number,
		       -1);
    if (number) {
      std::string msg(gettext("Remove fax to "));
      msg += number;
      msg += gettext(" from the redial queue?");
      // we have not placed the gchar* string given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free it by hand
      g_free(number);

      PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Remove fax for redialling?"),
						standard_size, get_win());
      dialog_p->accepted.connect(Callback::make(*this, &RedialQueueDialog::remove));
      // there is no memory leak -- the memory will be deleted when PromptDialog closes
    }
  }
}

void RedialQueueDialog::remove(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    guint* id = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::id, &id,
		       -1);
    if (id) redial_queue_p->remove(id);
  }
}
