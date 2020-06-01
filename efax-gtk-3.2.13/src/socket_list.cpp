/* Copyright (C) 2001 to 2006, 2009 and 2010 Chris Vine

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
#include <cstring>
#include <vector>
#include <algorithm>
#include <ios>
#include <ostream>
#include <sstream>

#include <gdk/gdk.h>

#include "socket_list.h"
#include "dialogs.h"
#include "socket_list_icons.h"
#include "utils/toolbar_append_widget.h"

#include <c++-gtk-utils/mem_fun.h>
#include <c++-gtk-utils/convert.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

int SocketListDialog::is_socket_list = 0;

namespace {
namespace ModelColumns {
  enum {fax_label, fax_filename, cols_num};
} // namespace ModelColumns
} // anonymous namespace

class SocketListDialog::CB {
public:
  static void button_clicked(GtkWidget* widget_p,
			     void* data) {
    SocketListDialog* instance_p = static_cast<SocketListDialog*>(data);

    if (widget_p == instance_p->send_button_p) {
      instance_p->send_fax_impl();
    }
    else if (widget_p == instance_p->view_button_p) {
      instance_p->view_file();
    }
    else if (widget_p == instance_p->remove_button_p) {
      instance_p->remove_file_prompt();
    }
    else if (widget_p == instance_p->close_button_p) {
      instance_p->close();
    }
    else {
      write_error("Callback error in SocketListDialog::CB::button_clicked()\n");
      instance_p->close();
    }
  }

  static void set_buttons(void* data) {
    SocketListDialog* instance_p = static_cast<SocketListDialog*>(data);

    // see if anything is selected
    GtkTreeSelection* selection_p = gtk_tree_view_get_selection(instance_p->tree_view_p);
    if (gtk_tree_selection_get_selected(selection_p, 0, 0)) {
      gtk_widget_set_sensitive(instance_p->view_button_p, true);
      gtk_widget_set_sensitive(instance_p->remove_button_p, true);
      gtk_widget_set_sensitive(instance_p->send_button_p, true);
      gtk_button_set_relief(GTK_BUTTON(instance_p->view_button_p), GTK_RELIEF_NORMAL);
      gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NORMAL);
      gtk_button_set_relief(GTK_BUTTON(instance_p->send_button_p), GTK_RELIEF_NORMAL);
    }
    
    else {
      gtk_widget_set_sensitive(instance_p->view_button_p, false);
      gtk_widget_set_sensitive(instance_p->remove_button_p, false);
      gtk_widget_set_sensitive(instance_p->send_button_p, false);
      gtk_button_set_relief(GTK_BUTTON(instance_p->view_button_p), GTK_RELIEF_NONE);
      gtk_button_set_relief(GTK_BUTTON(instance_p->remove_button_p), GTK_RELIEF_NONE);
      gtk_button_set_relief(GTK_BUTTON(instance_p->send_button_p), GTK_RELIEF_NONE);
    }
  }

  static gboolean tree_view_mouse_click(GdkEventButton* event_p,
					void* data) {
    if (event_p->type == GDK_2BUTTON_PRESS) {
      SocketListDialog* instance_p = static_cast<SocketListDialog*>(data);
      int x, y;
      gtk_widget_get_pointer(GTK_WIDGET(instance_p->tree_view_p), &x, &y);
      if (x >= 0 && y >= 0
	  && gtk_tree_view_get_dest_row_at_pos(instance_p->tree_view_p,
					       x, y, 0, 0)) {
	instance_p->view_file();
      }
    }
    return false;
  }
};

extern "C" {
  static void button_clicked_cb(GtkWidget* widget_p,
				void* data) {
    SocketListDialog::CB::button_clicked(widget_p, data);
  }

  static void set_buttons_cb(GtkTreeSelection*,
			     void* data) {
    SocketListDialog::CB::set_buttons(data);
  }

  static gboolean tree_view_mouse_click_cb(GtkWidget*,
					   GdkEventButton* event_p,
					   void* data) {
    return SocketListDialog::CB::tree_view_mouse_click(event_p, data);
  }

} // extern "C"

SocketListDialog::SocketListDialog(std::pair<SharedPtr<FilenamesList>,
		                             SharedPtr<Thread::Mutex::Lock> > filenames_pair,
				   const int standard_size_):
                               WinBase(gettext("efax-gtk: Queued faxes from socket"),
				       prog_config.window_icon_h),
			       standard_size(standard_size_) {

  // notify the existence of this object
  is_socket_list++;

  send_button_p = gtk_button_new();
  view_button_p = gtk_button_new();
  remove_button_p = gtk_button_new();
  close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* socket_list_box_p = gtk_vbox_new(false, 0);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));
  GtkScrolledWindow* socket_list_scrolled_window_p
    = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  GtkToolbar* toolbar_p = (GTK_TOOLBAR(gtk_toolbar_new()));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);
  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  gtk_scrolled_window_set_shadow_type(socket_list_scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(socket_list_scrolled_window_p,
				 GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);

  // create the tree model and put it in a GobjHandle to handle its
  // lifetime, because it is not owned by a GTK+ container
  list_store_h =
    GobjHandle<GtkTreeModel>(GTK_TREE_MODEL(gtk_list_store_new(ModelColumns::cols_num,
							       G_TYPE_STRING, G_TYPE_STRING)));
  // populate the tree model
  set_socket_list_rows(filenames_pair);

  // create the tree view
  tree_view_p = GTK_TREE_VIEW(gtk_tree_view_new_with_model(list_store_h));
  gtk_container_add(GTK_CONTAINER(socket_list_scrolled_window_p), GTK_WIDGET(tree_view_p));

  // provide renderer for tree view, pack the fax_label tree view column
  // and connect to the tree model column (the fax_filename model column
  // is hidden and just contains data (a file name) to which the first
  // column relates
  GtkCellRenderer* renderer_p = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column_p =
    gtk_tree_view_column_new_with_attributes(gettext("Queued print jobs"), renderer_p,
					     "text", ModelColumns::fax_label,
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

  GtkWidget* send_hbox_p = gtk_hbox_new(false, 0);
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(send_fax_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    GtkWidget* label_p = gtk_label_new(gettext("Enter selected fax to send"));
    gtk_box_pack_start(GTK_BOX(send_hbox_p), image_p, false, false, 4);
    gtk_box_pack_start(GTK_BOX(send_hbox_p), label_p, false, false, 4);
    gtk_container_add(GTK_CONTAINER(send_button_p), send_hbox_p);
  }
  toolbar_append_widget(toolbar_p, send_button_p,
			gettext("Choose the selected fax for sending"));
  gtk_widget_set_sensitive(send_button_p, false);

  gtk_table_attach(table_p, GTK_WIDGET(socket_list_scrolled_window_p),
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/3, standard_size/3);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);

  gtk_box_pack_start(GTK_BOX(socket_list_box_p), GTK_WIDGET(toolbar_p), false, false, 0);
  gtk_box_pack_start(GTK_BOX(socket_list_box_p), GTK_WIDGET(table_p), true, true, 0);

  g_signal_connect(G_OBJECT(send_button_p), "clicked",
		   G_CALLBACK(button_clicked_cb), this);
  g_signal_connect(G_OBJECT(view_button_p), "clicked",
		   G_CALLBACK(button_clicked_cb), this);
  g_signal_connect(G_OBJECT(remove_button_p), "clicked",
		   G_CALLBACK(button_clicked_cb), this);
  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(button_clicked_cb), this);

  // now connect up the signal which indicates a selection has been made
  selection_p = gtk_tree_view_get_selection(tree_view_p);
  g_signal_connect(G_OBJECT(selection_p), "changed",
		   G_CALLBACK(set_buttons_cb), this);

  // capture a double click on the tree view for viewing a fax for sending
  gtk_widget_add_events(GTK_WIDGET(tree_view_p), GDK_BUTTON_PRESS_MASK);
  g_signal_connect(G_OBJECT(tree_view_p), "button_press_event",
		   G_CALLBACK(tree_view_mouse_click_cb), this);

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(close_button_p, true);
#else
  GTK_WIDGET_SET_FLAGS(close_button_p, GTK_CAN_DEFAULT);
#endif

  gtk_container_set_border_width(GTK_CONTAINER(table_p), standard_size/3);
  gtk_container_add(GTK_CONTAINER(get_win()), socket_list_box_p);
  //gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(get_win(), standard_size * 15, standard_size * 12);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar_p), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));

  // not needed if gtk_toolbar_set_show_arrow() has been set false
  /*
  gtk_widget_set_size_request(GTK_WIDGET(toolbar_p), 
			      send_button_p->allocation.width + view_button_p->allocation.width
			      + remove_button_p->allocation.width + 12,
			      send_button_p->allocation.height);
  */
}

SocketListDialog::~SocketListDialog(void) {
  // notify the destruction of this object
  is_socket_list--;
}

void SocketListDialog::send_fax_impl(void) {

  std::pair<std::string, std::string> fax_to_send;

  GtkTreeIter selected_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);

  if (gtk_tree_selection_get_selected(selection_p, &model_p, &selected_iter)) {
    gchar* fax_label_p = 0;
    gchar* fax_filename_p = 0;
    gtk_tree_model_get(model_p, &selected_iter,
		       ModelColumns::fax_label, &fax_label_p,
		       ModelColumns::fax_filename, &fax_filename_p,
		       -1);
    if (fax_label_p) fax_to_send.first = fax_label_p;
    if (fax_filename_p) fax_to_send.second = fax_filename_p;
    
    // we have not placed the gchar* strings given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free them by hand
    g_free(fax_label_p);
    g_free(fax_filename_p);
    
    selected_fax(fax_to_send);
  }
}

void SocketListDialog::close_cb(void) {
  close();
}

void SocketListDialog::set_socket_list_rows(std::pair<SharedPtr<FilenamesList>,
					              SharedPtr<Thread::Mutex::Lock> > filenames_pair) {

  gtk_list_store_clear(GTK_LIST_STORE(list_store_h.get()));

  // filenames_pair.first references a std::list object containing
  // another pair as value type, the first member of which is a
  // temporary file name created by mkstemp() in
  // SocketServer::accept_client(), and the second member of which
  // contains the print job number assigned by
  // SocketServer::add_file()

  // filenames_pair.second is just a tunnelling lock, which gives us
  // exclusive access to the FilenamesList object maintained by the
  // SocketServer object while we are iterating through it in this
  // function

  std::for_each(filenames_pair.first->begin(), filenames_pair.first->end(),
		MemFun::make(*this, &SocketListDialog::set_socket_list_item));
}

void SocketListDialog::set_socket_list_item(const std::pair<std::string, unsigned int>& item) {

  // get a row to insert the file name in
  GtkTreeIter row_iter;
  gtk_list_store_append(GTK_LIST_STORE(list_store_h.get()), &row_iter);

  // insert fax label and fax filename
  std::ostringstream strm;
  strm << gettext("PRINT JOB: ") << item.second;
  gtk_list_store_set(GTK_LIST_STORE(list_store_h.get()), &row_iter,
		     ModelColumns::fax_label, strm.str().c_str(),
		     ModelColumns::fax_filename, item.first.c_str(),
		     -1);
}

std::pair<const char*, char* const*> SocketListDialog::get_view_file_parms(const std::string& file_name) {

  std::vector<std::string> view_parms;
  std::string view_cmd;
  std::string view_name;
  std::string::size_type end_pos;
  try {
    // lock the Prog_config object to stop it being accessed in
    // FaxListDialog::get_ps_viewer_parms() while we are accessing it here
    // (this is ultra cautious as it is only copied/checked for emptiness
    // there)
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    view_cmd = Utf8::filename_from_utf8(prog_config.ps_view_cmd);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in SocketListDialog::get_view_file_parms()\n");
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

void SocketListDialog::view_file(void) {

  bool is_ps_view_cmd_empty;
  { // lock the Prog_config object to stop it being accessed in
    // FaxListDialog::get_ps_viewer_parms() while we are accessing it here
    // (this is ultra cautious as it is only copied/checked for emptiness
    // there)
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    is_ps_view_cmd_empty = prog_config.ps_view_cmd.empty();
  }

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p(gtk_tree_view_get_selection(tree_view_p));

  if (!is_ps_view_cmd_empty
      && gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    gchar* file_name_p = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::fax_filename, &file_name_p,
		       -1);
    std::string file_name;
    if (file_name_p) file_name = file_name_p;
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(file_name_p);

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

void SocketListDialog::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

void SocketListDialog::remove_file_prompt(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    gchar* fax_label_p = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::fax_label, &fax_label_p,
		       -1);
    if (fax_label_p) {
      std::string msg(gettext("Remove "));
      msg += fax_label_p;
      msg += gettext(" from the list?");
      // we have not placed the gchar* string given by gtk_tree_model_get()
      // in a GcharScopedHandle object so we need to free it by hand
      g_free(fax_label_p);

      PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Remove queued fax"),
						standard_size, get_win());
      dialog_p->accepted.connect(Callback::make(*this, &SocketListDialog::remove_file));
      // there is no memory leak -- the memory will be deleted when PromptDialog closes
    }
  }
}

void SocketListDialog::remove_file(void) {

  GtkTreeIter row_iter;
  GtkTreeModel* model_p;
  GtkTreeSelection* selection_p = gtk_tree_view_get_selection(tree_view_p);
  if (gtk_tree_selection_get_selected(selection_p, &model_p, &row_iter)) {

    gchar* file_name_p = 0;
    gtk_tree_model_get(model_p, &row_iter,
		       ModelColumns::fax_filename, &file_name_p,
		       -1);
    if (file_name_p) remove_from_socket_server_filelist(file_name_p);
    // we have not placed the gchar* string given by gtk_tree_model_get()
    // in a GcharScopedHandle object so we need to free it by hand
    g_free(file_name_p);

    // the emission of the remove_from_socket_server_filelist signal
    // will invoke MainWindow::remove_from_socket_server_filelist()
    // which will call SocketServer::remove_file() which will then
    // invoke the SocketServer::filelist_changed_notify Notifier
    // object which will in turn invoke the
    // MainWindow::socket_filelist_changed_notify_cb() which will
    // then call SocketListDialog::set_socket_list_rows() and so
    // refresh the socket file list.  The server object will also
    // clean up by deleting the temporary file it created.  We do not
    // therefore need to do anything else here.
  }
}
