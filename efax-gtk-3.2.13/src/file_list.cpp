/* Copyright (C) 2001 to 2005, 2009 and 2010 Chris Vine

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


#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cstring>

#include <gdk/gdk.h>

#include "file_list.h"
#include "dialogs.h"
#include "file_list_icons.h"

#include "utils/tree_path_handle.h"
#include "utils/toolbar_append_widget.h"

#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/mem_fun.h>
#include <c++-gtk-utils/convert.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

int FileListDialog::is_file_list = 0;


namespace {
namespace ModelColumns {
  enum {file_name, cols_num};
} // namespace ModelColumns
} // anonymous namespace

void FileListDialogCB::file_list_dialog_button_clicked(GtkWidget* widget_p, void* data) {
  FileListDialog* instance_p = static_cast<FileListDialog*>(data);

  if (widget_p == instance_p->ok_button_p) {
    instance_p->accepted(instance_p->get_files());
    instance_p->close();
  }
  else if (widget_p == instance_p->cancel_button_p) {
    instance_p->close();
  }
  else if (widget_p == instance_p->up_button_p) {
    instance_p->move_up();
  }
  else if (widget_p == instance_p->down_button_p) {
    instance_p->move_down();
  }
  else if (widget_p == instance_p->add_button_p) {
    instance_p->add_files();
  }
  else if (widget_p == instance_p->view_button_p) {
    instance_p->view_file();
  }
  else if (widget_p == instance_p->remove_button_p) {
    instance_p->remove_file_prompt();
  }
  else {
    write_error("Callback error in FileListDialogCB::file_list_dialog_button_clicked()\n");
    instance_p->close();
  }
}

void FileListDialogCB::file_list_dialog_set_buttons(GtkTreeSelection*, void* data) {
  FileListDialog* instance_p = static_cast<FileListDialog*>(data);

  // see if anything is selected
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(instance_p->tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, 0, 0)) {
    gtk_widget_set_sensitive(instance_p->view_button_p, true);
    gtk_widget_set_sensitive(instance_p->remove_button_p, true);
    gtk_button_set_relief(GTK_BUTTON(instance_p->view_button_p), GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NORMAL);
  }
    
  else {
    gtk_widget_set_sensitive(instance_p->view_button_p, false);
    gtk_widget_set_sensitive(instance_p->remove_button_p, false);
    gtk_button_set_relief(GTK_BUTTON(instance_p->view_button_p), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NONE);
  }
}


FileListDialog::FileListDialog(const int standard_size_, GtkWindow* parent_p):
                               WinBase(gettext("efax-gtk: File list"),
				       prog_config.window_icon_h,
				       true, parent_p),
			       standard_size(standard_size_) {

  // notify the existence of this object in case I later decide to use this dialog as modeless
  // by omitting the set_modal() call below
  is_file_list++;

  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  up_button_p = gtk_button_new();
  down_button_p = gtk_button_new();
  add_button_p = gtk_button_new();
  view_button_p = gtk_button_new();
  remove_button_p = gtk_button_new();

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* file_list_box_p = gtk_vbox_new(false, 0);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(5, 2, false));
  GtkScrolledWindow* file_list_scrolled_window_p
    = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  GtkToolbar* toolbar_p = (GTK_TOOLBAR(gtk_toolbar_new()));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box_p), standard_size/2);
  
  gtk_scrolled_window_set_shadow_type(file_list_scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(file_list_scrolled_window_p,
				 GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);

  // create the tree model and put it in a GobjHandle to handle its
  // lifetime, because it is not owned by a GTK+ container
  list_store_h =
    GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_list_store_new(ModelColumns::cols_num,
							       G_TYPE_STRING)));
  // create the tree view
  tree_view_p = GTK_TREE_VIEW(gtk_tree_view_new_with_model(list_store_h));
  gtk_container_add(GTK_CONTAINER(file_list_scrolled_window_p), GTK_WIDGET(tree_view_p));

  // provide a renderer for tree view, pack into tree view column
  // and connect to the tree model column
  GtkCellRenderer* renderer_p = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column_p =
    gtk_tree_view_column_new_with_attributes(gettext("Files to fax"), renderer_p,
					     "text", ModelColumns::file_name,
					     static_cast<void*>(0));
  gtk_tree_view_append_column(tree_view_p, column_p);

  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  // single line selection
  gtk_tree_selection_set_mode(selection_p, GTK_SELECTION_SINGLE);

  // make drag and drop work
  gtk_tree_view_set_reorderable(tree_view_p, true);

  // set up the tool bar
#if GTK_CHECK_VERSION(2,16,0)
  gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar_p), GTK_ORIENTATION_HORIZONTAL);
#else
  gtk_toolbar_set_orientation(toolbar_p, GTK_ORIENTATION_HORIZONTAL);
#endif
  gtk_toolbar_set_style(toolbar_p, GTK_TOOLBAR_ICONS);

  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(view_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(view_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, view_button_p,
			gettext("View selected file"));
  gtk_widget_set_sensitive(view_button_p, false);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(remove_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(remove_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, remove_button_p,
			gettext("Remove selected file from list"));
  gtk_widget_set_sensitive(remove_button_p, false);

  GtkWidget* add_hbox_p = gtk_hbox_new(false, 0);
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(add_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    GtkWidget* label_p = gtk_label_new(gettext("Add files to list"));
    gtk_box_pack_start(GTK_BOX(add_hbox_p), image_p, false, false, 4);
    gtk_box_pack_start(GTK_BOX(add_hbox_p), label_p, false, false, 4);
    gtk_container_add(GTK_CONTAINER(add_button_p), add_hbox_p);
  }
  toolbar_append_widget(toolbar_p, add_button_p,
			gettext("Add files to the file list for faxing"));

  // bring up the icon size registered in MainWindow::MainWindow()
  GtkIconSize efax_gtk_button_size = gtk_icon_size_from_name("EFAX_GTK_BUTTON_SIZE");
  
  image_p = gtk_image_new_from_stock(GTK_STOCK_GO_UP, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(up_button_p), image_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(down_button_p), image_p);

  gtk_widget_set_tooltip_text(up_button_p, gettext("Move file up"));
  gtk_widget_set_tooltip_text(down_button_p, gettext("Move file down"));

  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);
  
  GtkWidget* dummy1_p = gtk_label_new(0);
  GtkWidget* dummy2_p = gtk_label_new(0);

  gtk_table_attach(table_p, dummy1_p,
		   0, 1, 0, 1,
		   GTK_SHRINK, GTK_EXPAND,
		   0, 0);

  gtk_table_attach(table_p, up_button_p,
		   0, 1, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, standard_size/3);

  gtk_table_attach(table_p, down_button_p,
		   0, 1, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/2, standard_size/3);

  gtk_table_attach(table_p, dummy2_p,
		   0, 1, 3, 4,
		   GTK_SHRINK, GTK_EXPAND,
		   0, 0);

  gtk_table_attach(table_p, GTK_WIDGET(file_list_scrolled_window_p),
		   1, 2, 0, 4,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		    standard_size/3, standard_size/3);

  gtk_table_attach(table_p, button_box_p,
		   0, 2, 4, 5,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);

  gtk_box_pack_start(GTK_BOX(file_list_box_p), GTK_WIDGET(toolbar_p), false, false, 0);
  gtk_box_pack_start(GTK_BOX(file_list_box_p), GTK_WIDGET(table_p), true, true, 0);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(up_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(down_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(add_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(view_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(remove_button_p), "clicked",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_button_clicked), this);


  // now connect up the signal which indicates a selection has been made
  selection_p = gtk_tree_view_get_selection(tree_view_p);
  g_signal_connect(G_OBJECT(selection_p), "changed",
		   G_CALLBACK(FileListDialogCB::file_list_dialog_set_buttons), this);

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(ok_button_p, true);
  gtk_widget_set_can_default(cancel_button_p, true);
  gtk_widget_set_can_focus(up_button_p, false);
  gtk_widget_set_can_focus(down_button_p, false);
#else
  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS(up_button_p, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(down_button_p, GTK_CAN_FOCUS);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(table_p), standard_size/3);
  gtk_container_add(GTK_CONTAINER(get_win()), file_list_box_p);
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_default_size(get_win(), standard_size * 15, standard_size * 12);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar_p), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));

  gtk_button_set_relief(GTK_BUTTON(add_button_p), GTK_RELIEF_NORMAL);
  gtk_widget_set_sensitive(add_button_p, true);
  
  // not needed if gtk_toolbar_set_show_arrow() has been set false
  /*
  gtk_widget_set_size_request(GTK_WIDGET(toolbar_p), 
			      add_button_p->allocation.width + view_button_p->allocation.width
			      + remove_button_p->allocation.width + 12,
			      add_button_p->allocation.height);
  */
}

FileListDialog::~FileListDialog(void) {
  // notify the destruction of this object
  is_file_list--;
}

std::string FileListDialog::get_files(void) {
  std::string text;
  
  GtkTreeIter row_iter;
  bool result = gtk_tree_model_get_iter_first(list_store_h, &row_iter);
  while (result) {
    gchar* file_name_p = 0;
    gtk_tree_model_get(list_store_h, &row_iter,
		       ModelColumns::file_name, &file_name_p,
		       -1);
    if (file_name_p) text += file_name_p;
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(file_name_p);
    result = gtk_tree_model_iter_next(list_store_h, &row_iter);
    if (result) text += ", ";
  }
  return text;
}

void FileListDialog::move_up(void) {

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    TreePathScopedHandle path_h(gtk_tree_model_get_path(model_p, &selected_iter));
    if (gtk_tree_path_prev(path_h)) {

      GtkTreeIter prev_iter;
      
      if (!gtk_tree_model_get_iter(model_p, &prev_iter, path_h)) {
	write_error("Iterator error in FileListDialog::move_up()\n");
	beep();
      }
      else {
	gchar* selected_name_p;
	gchar* prev_name_p;

	// just do a swap of values
	gtk_tree_model_get(model_p, &selected_iter,
			   ModelColumns::file_name, &selected_name_p,
			   -1);

	gtk_tree_model_get(model_p, &prev_iter,
			   ModelColumns::file_name, &prev_name_p,
			   -1);

	gtk_list_store_set(GTK_LIST_STORE(model_p), &selected_iter,
			   ModelColumns::file_name, prev_name_p,
			   -1);
      
	gtk_list_store_set(GTK_LIST_STORE(model_p), &prev_iter,
			   ModelColumns::file_name, selected_name_p,
			   -1);

	// we have not placed the gchar* strings given by gtk_tree_model_get()
	// in a GcharScopedHandle object so we need to free them by hand
	g_free(selected_name_p);
	g_free(prev_name_p);

	gtk_tree_selection_select_iter(selection_p, &prev_iter);
      }
    }
    else beep();
  }
  else beep();
}

void FileListDialog::move_down(void) {

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    GtkTreeIter next_iter = selected_iter;;
    if (gtk_tree_model_iter_next(model_p, &next_iter)) {
      
      gchar* selected_name_p = 0;
      gchar* next_name_p = 0;

      // just do a swap of values
      gtk_tree_model_get(model_p, &selected_iter,
			 ModelColumns::file_name, &selected_name_p,
			 -1);

      gtk_tree_model_get(model_p, &next_iter,
			 ModelColumns::file_name, &next_name_p,
			 -1);

      gtk_list_store_set(GTK_LIST_STORE(model_p), &selected_iter,
			 ModelColumns::file_name, next_name_p,
			 -1);
      
      gtk_list_store_set(GTK_LIST_STORE(model_p), &next_iter,
			 ModelColumns::file_name, selected_name_p,
			 -1);

      // we have not placed the gchar* strings given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free them by hand
      g_free(selected_name_p);
      g_free(next_name_p);

      gtk_tree_selection_select_iter(selection_p, &next_iter);
    }
    else beep();
  }
  else beep();
}

void FileListDialog::add_files(void) {

  FileReadSelectDialog file_dialog(standard_size, true, get_win());
  file_dialog.exec();
  std::vector<std::string> file_result = file_dialog.get_result();
  if (!file_result.empty()) {
    std::for_each(file_result.begin(), file_result.end(),
		  MemFun::make(*this, &FileListDialog::add_file_item));
  }
}

void FileListDialog::add_file_item(const std::string& item) {

  GtkTreeIter row_iter;
  gtk_list_store_append(GTK_LIST_STORE(list_store_h.get()), &row_iter);
  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &row_iter,
		     ModelColumns::file_name, item.c_str(),
		     -1);
}

std::pair<const char*, char* const*> FileListDialog::get_view_file_parms(const std::string& file_name) {

  std::vector<std::string> view_parms;
  std::string view_cmd;
  std::string view_name;
  std::string::size_type end_pos;
  try {
    // lock the Prog_config object to stop it being accessed in
    // FaxListDialog::get_ps_viewer_parms() while we are accessing it here
    // (this is ultra cautious as it is only copied/checked for emptiness
    // there, and the GUI interface is insensitive if we are here)
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    view_cmd = Utf8::filename_from_utf8(prog_config.ps_view_cmd);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in FileListDialog::get_view_file_parms()\n");
    return std::pair<const char*, char* const*>(static_cast<char*>(0),
						static_cast<char* const*>(0));
  }

  if ((end_pos = view_cmd.find_first_of(' ')) != std::string::npos) { // we have parms
    view_name.assign(view_cmd, 0, end_pos);
    view_parms.push_back(view_name);
    // find start of next parm
    std::string::size_type start_pos = view_cmd.find_first_not_of(' ', end_pos);
    while (start_pos != std::string::npos) {
      end_pos = view_cmd.find_first_of(' ', start_pos);
      if (end_pos != std::string::npos) {
	view_parms.push_back(view_cmd.substr(start_pos, end_pos - start_pos));
	start_pos = view_cmd.find_first_not_of(' ', end_pos); // prepare for next interation
      }
      else {
	view_parms.push_back(view_cmd.substr(start_pos, 
					     view_cmd.size() - start_pos));
	start_pos = end_pos;
      }
    }
  }

  else { // just a view command without parameters to be passed
    view_name = view_cmd;
    view_parms.push_back(view_name);
  }

  view_parms.push_back(file_name);

  char** exec_parms = new char*[view_parms.size() + 1];

  char**  temp_pp = exec_parms;
  std::vector<std::string>::const_iterator iter;
  for (iter = view_parms.begin(); iter != view_parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }
  
  *temp_pp = 0;

  char* prog_name = new char[view_name.size() + 1];
  std::strcpy(prog_name, view_name.c_str());

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void FileListDialog::view_file(void) {

  bool is_ps_view_cmd_empty;
  { // lock the Prog_config object to stop it being accessed in
    // FaxListDialog::get_ps_viewer_parms() while we are accessing it here
    // (this is ultra cautious as it is only copied/checked for emptiness
    // there, and the GUI interface is insensitive if we are here)
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    is_ps_view_cmd_empty = prog_config.ps_view_cmd.empty();
  }

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (!is_ps_view_cmd_empty
      && gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    
    std::string file_name;
    try {
      gchar* name_p = 0;
      gtk_tree_model_get(model_p, &row_iter,
			 ModelColumns::file_name, &name_p,
			 -1);
      // place name_p in a handle class in case Utf8::filename_from_utf8() throws
      GcharScopedHandle name_h(name_p);
      name_p = 0; // we don't need it any more
      if (name_h.get()) file_name = Utf8::filename_from_utf8(name_h.get());
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in FileListDialog::view_file()\n");
      beep();
      return;
    }

    // get the arguments for the exec() call below (because this is a
    // multi-threaded program, we must do this before fork()ing because
    // we use functions to get the arguments which are not async-signal-safe)
    std::pair<const char*, char* const*> view_file_parms(get_view_file_parms(file_name));

    if (view_file_parms.first) { // this will be 0 if get_view_file_parms()
                                 // threw a Utf8::ConversionError)

      pid_t pid = fork();

      if (pid == -1) {
	write_error("Fork error - exiting\n");
	std::exit(FORK_ERROR);
      }
      if (!pid) {  // child process - as soon as everything is set up we are going to do an exec()

	connect_to_stderr();

	execvp(view_file_parms.first, view_file_parms.second);

	// if we reached this point, then the execvp() call must have failed
	// report error and end process - use _exit() and not exit()
	write_error("Can't find the postscript viewer program - please check your installation\n"
		    "and the PATH environmental variable\n");
      _exit(0);
      } // end of view program process
      // release the memory allocated on the heap for
      // the redundant view_file_parms
      // we are in the main parent process here - no worries about
      // only being able to use async-signal-safe functions
      delete_parms(view_file_parms);
    }
  }
}

void FileListDialog::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

void FileListDialog::remove_file_prompt(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    gchar* name_p = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::file_name, &name_p,
		       -1);
    
    std::string msg(gettext("Remove file "));
    if (name_p) msg += name_p;
    msg += gettext(" from the list?");

    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free them by hand
    g_free(name_p);
    
    PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("Remove file"),
					      standard_size, get_win());
    dialog_p->accepted.connect(Callback::make(*this, &FileListDialog::remove_file));
    // there is no memory leak -- the memory will be deleted when PromptDialog closes
  }
}

void FileListDialog::remove_file(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {
    gtk_list_store_remove(GTK_LIST_STORE(model_p), &row_iter);
  }
}
