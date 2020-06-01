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


#include <fstream>
#include <cstdlib>
#include <ios>
#include <ostream>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h> // the key codes are here

#include "addressbook.h"
#include "dialogs.h"
#include "utils/tree_path_handle.h"
#include "addressbook_icons.h"

#include <c++-gtk-utils/convert.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

#define ADDRESS_FILE ".efax-gtk_addressbook"
#define ADDRESS_DIVIDER 3

int AddressBook::is_address_list = 0;

namespace {
namespace ModelColumns {
  enum {name, number, cols_num};
} // namespace ModelColumns
} // anonymous namespace


void AddressBookCB::addr_book_button_clicked(GtkWidget* widget_p, void* data) {
  AddressBook* instance_p = static_cast<AddressBook*>(data);
  
  if (widget_p == instance_p->ok_button_p) {
    if (instance_p->ok_impl()) instance_p->close();
  }
  else if (widget_p == instance_p->cancel_button_p) {
    instance_p->result = "";
    instance_p->close();
  }
  else if (widget_p == instance_p->add_button_p) {
    instance_p->add_address_prompt();
  }
  else if (widget_p == instance_p->delete_button_p) {
    instance_p->delete_address_prompt();
  }
  else if (widget_p == instance_p->up_button_p) {
    instance_p->move_up_impl();
  }
  else if (widget_p == instance_p->down_button_p) {
    instance_p->move_down_impl();
  }
  else {
    write_error("Callback error in AddressBookCB::addr_book_button_clicked()\n");
    instance_p->close();
  }
}

void AddressBookCB::addr_book_drag_n_drop(GtkTreeModel*, GtkTreePath*, void* data) {
  static_cast<AddressBook*>(data)->save_list();
}


AddressBook::AddressBook(const int size, GtkWindow* parent_p):
                             WinBase(gettext("efax-gtk: Address book"),
				     prog_config.window_icon_h,
				     true, parent_p),
			     standard_size(size) {

  // notify the existence of this object in case I later decide to use this dialog as modeless
  // by changing the modal parameter above to false
  is_address_list++;

  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  add_button_p = gtk_button_new();
  delete_button_p = gtk_button_new();
  up_button_p = gtk_button_new();
  down_button_p = gtk_button_new();

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));
  GtkTable* list_table_p = GTK_TABLE(gtk_table_new(5, 2, false));
  GtkScrolledWindow* scrolled_window_p = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box_p), standard_size/2);

  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);

  gtk_scrolled_window_set_shadow_type(scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scrolled_window_p, GTK_POLICY_ALWAYS,
				 GTK_POLICY_ALWAYS);

  // create the tree model and put it in a GobjHandle to handle its
  // lifetime, because it is not owned by a GTK+ container
  list_store_h =
    GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_list_store_new(ModelColumns::cols_num,
							       G_TYPE_STRING, G_TYPE_STRING)));
  // populate the model (containing the address list)
  read_list();

  // create the tree view
  tree_view_p = GTK_TREE_VIEW(gtk_tree_view_new_with_model(list_store_h));
  gtk_container_add(GTK_CONTAINER(scrolled_window_p), GTK_WIDGET(tree_view_p));

  // provide renderers for tree view, pack into tree view columns
  // and connect to the tree model columns
  GtkCellRenderer* renderer_p = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column_p =
    gtk_tree_view_column_new_with_attributes(gettext("Name"), renderer_p,
					     "text", ModelColumns::name,
					     static_cast<void*>(0));
  gtk_tree_view_append_column(tree_view_p, column_p);
  
  renderer_p = gtk_cell_renderer_text_new();
  column_p = gtk_tree_view_column_new_with_attributes(gettext("Number"), renderer_p,
						      "text", ModelColumns::number,
						      static_cast<void*>(0));
  gtk_tree_view_append_column(tree_view_p, column_p);

  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  // single line selection
  gtk_tree_selection_set_mode(selection_p, GTK_SELECTION_SINGLE);

  // make drag and drop work
  gtk_tree_view_set_reorderable(tree_view_p, true);

  // get any drag and drop to be saved to the addresses file
  g_signal_connect(G_OBJECT(list_store_h.get()), "row_deleted",
		   G_CALLBACK(AddressBookCB::addr_book_drag_n_drop), this);

  // bring up the icon size registered in MainWindow::MainWindow()
  GtkIconSize efax_gtk_button_size = gtk_icon_size_from_name("EFAX_GTK_BUTTON_SIZE");

  GtkWidget* image_p;
  { // provide a scope block for the GobjHandle

    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(add_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(add_button_p), image_p);
  }
  image_p = gtk_image_new_from_stock(GTK_STOCK_DELETE, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(delete_button_p), image_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_GO_UP, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(up_button_p), image_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(down_button_p), image_p);

  GtkWidget* dummy_p = gtk_label_new(0);

  gtk_table_attach(list_table_p, add_button_p,
		   0, 1, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, 0);

  gtk_table_attach(list_table_p, delete_button_p,
		   0, 1, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, 0);

  gtk_table_attach(list_table_p, up_button_p,
		   0, 1, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, 0);

  gtk_table_attach(list_table_p, down_button_p,
		   0, 1, 3, 4,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, 0);

  gtk_table_attach(list_table_p, dummy_p,
		   0, 1, 4, 5,
		   GTK_SHRINK, GTK_EXPAND,
		   standard_size/2, 0);

  gtk_table_attach(list_table_p, GTK_WIDGET(scrolled_window_p),
		   1, 2, 0, 5,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   0, 0);

  gtk_widget_set_tooltip_text(add_button_p, gettext("Add new address"));
  gtk_widget_set_tooltip_text(delete_button_p, gettext("Delete address"));
  gtk_widget_set_tooltip_text(up_button_p, gettext("Move address up"));
  gtk_widget_set_tooltip_text(down_button_p, gettext("Move address down"));

  gtk_table_attach(table_p, GTK_WIDGET(list_table_p),
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/3, standard_size/3);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  g_signal_connect(G_OBJECT(add_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  g_signal_connect(G_OBJECT(delete_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  g_signal_connect(G_OBJECT(up_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  g_signal_connect(G_OBJECT(down_button_p), "clicked",
		   G_CALLBACK(AddressBookCB::addr_book_button_clicked), this);
  
#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(ok_button_p, true);
  gtk_widget_set_can_default(cancel_button_p, true);
  gtk_widget_set_can_default(add_button_p, false);
  gtk_widget_set_can_default(delete_button_p, false);
  gtk_widget_set_can_focus(up_button_p, false);
  gtk_widget_set_can_focus(down_button_p, false);
#else
  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS(add_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS(delete_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS(up_button_p, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(down_button_p, GTK_CAN_FOCUS);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(table_p), standard_size/3);
  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_widget_grab_focus(cancel_button_p);
  gtk_window_set_default_size(get_win(), standard_size * 15, standard_size * 8);
  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

AddressBook::~AddressBook(void) {
  // notify the destruction of this object
  is_address_list--;
}

void AddressBook::on_delete_event(void) {
  gtk_widget_hide(GTK_WIDGET(get_win()));
  result = "";
  close();
}

bool AddressBook::ok_impl(void) {
  bool return_val = false;
  if (get_number().get()) result = get_number().get();
  if (!result.empty()) {
    accepted(result);
    return_val = true;
  }
  else beep();
  return return_val;
}


GcharSharedHandle AddressBook::get_number(void) {

  GtkTreeIter iter;
  GtkTreeModel* model_p;
  gchar* number_p = 0;
  GtkTreeSelection* selection_p(gtk_tree_view_get_selection(tree_view_p));

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &iter)) {
    gtk_tree_model_get(model_p, &iter,
		       ModelColumns::number, &number_p,
		       -1);
  }
  return GcharSharedHandle(number_p);
}

void AddressBook::add_address_prompt(void) {

  AddressDialog* dialog_p = new AddressDialog(standard_size, this->get_win());
  dialog_p->accepted.connect(Callback::make(*this, &AddressBook::add_address_impl));
  // there is no memory leak -- AddressDailog will delete its own memory
  // when it is closed
}

void AddressBook::add_address_impl(const std::vector<std::string>& address) {

  GtkTreeIter iter;
  gtk_list_store_append(GTK_LIST_STORE(list_store_h.get()), &iter);
  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &iter,
		     ModelColumns::name, address[0].c_str(),
		     ModelColumns::number, address[1].c_str(),
		     -1);
  save_list();
}

void AddressBook::delete_address_prompt(void) {
  GtkTreeIter iter;
  GtkTreeSelection* selection_p(gtk_tree_view_get_selection(tree_view_p));

  if (gtk_tree_selection_get_selected(selection_p, 0, &iter)) {
    PromptDialog* dialog_p = new PromptDialog(gettext("Delete selected address?"),
					      gettext("efax-gtk: Delete address"), standard_size,
					      get_win());
    dialog_p->accepted.connect(Callback::make(*this, &AddressBook::delete_address_impl));
    // there is no memory leak -- the memory will be deleted when PromptDialog closes
  }
  else beep();
}

void AddressBook::delete_address_impl(void) {

  GtkTreeIter iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p(gtk_tree_view_get_selection(tree_view_p));

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &iter)) {
    // delete the address by removing it from the list store
    gtk_list_store_remove(GTK_LIST_STORE(model_p), &iter);
    save_list();
  }
}

void AddressBook::read_list(void) {

  std::string filename(prog_config.working_dir);
  filename += "/" ADDRESS_FILE;

#ifdef HAVE_IOS_NOCREATE
  std::ifstream filein(filename.c_str(), std::ios::in | std::ios::nocreate);
#else
  // we must have Std C++ so we probably don't need a ios::nocreate
  // flag on a read open to ensure uniqueness
  std::ifstream filein(filename.c_str(), std::ios::in);
#endif

  if (filein) {

    gtk_list_store_clear(GTK_LIST_STORE(list_store_h.get()));

    std::string line;
    while (std::getline(filein, line)) {
      if (!line.empty()) {
	std::string::size_type pos = line.find(ADDRESS_DIVIDER, 0); // pos now is set to end of name value
	// get a list store row to insert the number and name in
	GtkTreeIter iter;
	gtk_list_store_append(GTK_LIST_STORE(list_store_h.get()), &iter);

	try {
	  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &iter,
			     ModelColumns::name,
			     Utf8::locale_to_utf8(line.substr(0, pos)).c_str(),
			     -1);
	}
	catch (Utf8::ConversionError&) {
	  write_error("UTF-8 conversion error in AddressBook::read_list()\n");
	}
	pos++; // pos now is set to the beginning of the number value
	try {
	  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &iter,
			     ModelColumns::number,
			     Utf8::locale_to_utf8(line.substr(pos, line.size() - pos)).c_str(),
			     -1);
	}
	catch (Utf8::ConversionError&) {
	  write_error("UTF-8 conversion error in AddressBook::read_list()\n");
	}
      }
    }
  }
}

void AddressBook::save_list(void) {

  std::string filename(prog_config.working_dir);
  filename += "/" ADDRESS_FILE;
  std::ofstream fileout(filename.c_str(), std::ios::out);

  if (fileout) {
    
    gchar* name_p;
    gchar* number_p;
    GtkTreeIter iter;
    bool not_at_end = gtk_tree_model_get_iter_first(list_store_h, &iter);
    while(not_at_end) {
      // the try()/catch() blocks here are ultra cautious - something must be
      // seriously wrong if model_columns.name and model_columns.number are not
      // in valid UTF-8 format, since they are tested where necessary at input
      gtk_tree_model_get(list_store_h, &iter,
			 ModelColumns::name, &name_p,
			 ModelColumns::number, &number_p,
			 -1);
      GcharScopedHandle name_h(name_p);
      GcharScopedHandle number_h(number_p);
      try {
	fileout << Utf8::locale_from_utf8(name_h.get());
	fileout << static_cast<char>(ADDRESS_DIVIDER);
	fileout << Utf8::locale_from_utf8(number_h.get());
	fileout << '\n';
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in AddressBook::save_list()\n");
	// end the line comprising whatever we have put there
	fileout << '\n';
      }
      not_at_end = gtk_tree_model_iter_next(list_store_h, &iter);
    }
  }
}

void AddressBook::move_up_impl(void) {

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p, &selected_iter));
    if (gtk_tree_path_prev(path_h)) {

      GtkTreeIter prev_iter;
      
      if (!gtk_tree_model_get_iter(model_p, &prev_iter, path_h)) {
	write_error("Iterator error in AddressBook::move_up_impl()\n");
	beep();
      }
      else {
	gchar* selected_name_p = 0;
	gchar* selected_number_p = 0;
	gchar* prev_name_p = 0;
	gchar* prev_number_p = 0;

	// just do a swap of values
	gtk_tree_model_get(model_p, &selected_iter,
			   ModelColumns::name, &selected_name_p,
			   ModelColumns::number, &selected_number_p,
			   -1);

	gtk_tree_model_get(model_p, &prev_iter,
			   ModelColumns::name, &prev_name_p,
			   ModelColumns::number, &prev_number_p,
			   -1);

	gtk_list_store_set(GTK_LIST_STORE(model_p), &selected_iter,
			   ModelColumns::name, prev_name_p,
			   ModelColumns::number, prev_number_p,
			   -1);
      
	gtk_list_store_set(GTK_LIST_STORE(model_p), &prev_iter,
			   ModelColumns::name, selected_name_p,
			   ModelColumns::number, selected_number_p,
			   -1);

	// we have not placed the gchar* strings given by gtk_tree_model_get()
	// in a GcharScopedHandle object so we need to free them by hand
	g_free(selected_name_p);
	g_free(selected_number_p);
	g_free(prev_name_p);
	g_free(prev_number_p);

	gtk_tree_selection_select_iter(selection_p, &prev_iter);
	save_list();
      }
    }
    else beep();
  }
  else beep();
}

void AddressBook::move_down_impl(void) {

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    GtkTreeIter next_iter = selected_iter;;
    if (gtk_tree_model_iter_next(model_p, &next_iter)) {
      
      gchar* selected_name_p = 0;
      gchar* selected_number_p = 0;
      gchar* next_name_p = 0;
      gchar* next_number_p = 0;

      // just do a swap of values
      gtk_tree_model_get(model_p, &selected_iter,
			 ModelColumns::name, &selected_name_p,
			 ModelColumns::number, &selected_number_p,
			 -1);

      gtk_tree_model_get(model_p, &next_iter,
			 ModelColumns::name, &next_name_p,
			 ModelColumns::number, &next_number_p,
			 -1);

      gtk_list_store_set(GTK_LIST_STORE(model_p), &selected_iter,
			 ModelColumns::name, next_name_p,
			 ModelColumns::number, next_number_p,
			 -1);
      
      gtk_list_store_set(GTK_LIST_STORE(model_p), &next_iter,
			 ModelColumns::name, selected_name_p,
			 ModelColumns::number, selected_number_p,
			 -1);

      // we have not placed the gchar* strings given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free them by hand
      g_free(selected_name_p);
      g_free(selected_number_p);
      g_free(next_name_p);
      g_free(next_number_p);

      gtk_tree_selection_select_iter(selection_p, &next_iter);
      save_list();
    }
    else beep();
  }
  else beep();
}

void AddressDialogCB::addr_dialog_selected(GtkWidget* widget_p, void* data) {
  AddressDialog* instance_p = static_cast<AddressDialog*>(data);

  if (widget_p == instance_p->ok_button_p) {
    if (instance_p->selected_impl()) instance_p->close();
  }
  else if (widget_p == instance_p->cancel_button_p) instance_p->close();
  else {
    write_error("Callback error in AddressDialogCB::addr_dialog_selected()\n");
    instance_p->close();
  }
}

gboolean AddressDialogCB::addr_dialog_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  AddressDialog* instance_p = static_cast<AddressDialog*>(data);
  int keycode = event_p->keyval;
#if GTK_CHECK_VERSION(2,20,0)
  bool cancel_focus = gtk_widget_has_focus(instance_p->cancel_button_p);
#else
  bool cancel_focus = GTK_WIDGET_HAS_FOCUS(instance_p->cancel_button_p);
#endif

#if GTK_CHECK_VERSION(2,99,0)
  if (keycode == GDK_KEY_Return && !cancel_focus) {
    if (instance_p->selected_impl()) instance_p->close();
    return true;  // stop processing here
  }
#else
  if (keycode == GDK_Return && !cancel_focus) {
    if (instance_p->selected_impl()) instance_p->close();
    return true;  // stop processing here
  }
#endif
  return false;   // continue processing key events
}

AddressDialog::AddressDialog(const int standard_size, GtkWindow* parent_p):
                             WinBase(gettext("efax-gtk: Add address"),
				     prog_config.window_icon_h,
				     true, parent_p) {
                             
  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

  GtkWidget* name_label_p = gtk_label_new(gettext("Name:"));
  GtkWidget* number_label_p = gtk_label_new(gettext("Number:"));

  gtk_label_set_justify(GTK_LABEL(name_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(number_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(name_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(number_label_p), 1, 0.5);

  name_entry_p = gtk_entry_new();
  number_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(name_entry_p, standard_size * 8, standard_size);
  gtk_widget_set_size_request(number_entry_p, standard_size * 8, standard_size);

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkTable* table_p = GTK_TABLE(gtk_table_new(3, 2, false));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box_p), standard_size/2);

  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);

  gtk_table_attach(table_p, name_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/2, standard_size/4);
  gtk_table_attach(table_p, name_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, number_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/2, standard_size/4);
  gtk_table_attach(table_p, number_entry_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, button_box_p,
		   0, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(AddressDialogCB::addr_dialog_selected), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(AddressDialogCB::addr_dialog_selected), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(AddressDialogCB::addr_dialog_key_press_event), this);

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(ok_button_p, true);
  gtk_widget_set_can_default(cancel_button_p, true);
#else
  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);
#endif

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/2);

  gtk_widget_grab_focus(name_entry_p);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

bool AddressDialog::selected_impl(void) {

  bool return_val = false;

  const gchar* name_text = gtk_entry_get_text(GTK_ENTRY(name_entry_p));
  const gchar* number_text = gtk_entry_get_text(GTK_ENTRY(number_entry_p));

  if (!*name_text || !*number_text) beep();
  
  else {
    std::vector<std::string> out_val;
    out_val.push_back((const char*)name_text);
    out_val.push_back((const char*)number_text);
    accepted(out_val);
    return_val = true;
  }
  return return_val;
}
