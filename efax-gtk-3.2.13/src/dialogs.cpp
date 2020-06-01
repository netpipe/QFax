/* Copyright (C) 2001 to 2014 Chris Vine

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

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <gdk/gdkkeysyms.h> // the key codes are here

#include <pango/pango-font.h>
#include <pango/pango-layout.h>

#include "dialogs.h"

#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/convert.h>

#include "gpl.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

enum FileReadSelectDialogResponse {
  FRS_VIEW_FILE = 1,
};

void FileReadSelectDialogCB::frs_dialog_response(GtkDialog*, gint id, void* data) {
  FileReadSelectDialog* instance_p = static_cast<FileReadSelectDialog*>(data);

  if (id == FRS_VIEW_FILE) {
    instance_p->view_file_impl();
    return;
  }

  if (id == GTK_RESPONSE_CANCEL || id == GTK_RESPONSE_DELETE_EVENT)
    instance_p->result.clear();
  else if (id == GTK_RESPONSE_OK)
    instance_p->set_result();
  else {
    instance_p->result.clear();
    write_error("Callback error in FileReadSelectDialogCB::frs_dialog_response()\n");
  }
  instance_p->close();
}

FileReadSelectDialog::FileReadSelectDialog(int size, bool multi_files, GtkWindow* parent_p):
            WinBase(gettext("efax-gtk: File to fax"),
		    prog_config.window_icon_h,
		    true, parent_p,
		    GTK_WINDOW(gtk_file_chooser_dialog_new(0, 0,
					GTK_FILE_CHOOSER_ACTION_OPEN,
				        gettext("View"), FRS_VIEW_FILE,
 					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK, GTK_RESPONSE_OK,
					static_cast<void*>(0)))),
	    standard_size(size) {

  GtkFileChooser* file_object_p = GTK_FILE_CHOOSER(get_win());

  gtk_file_chooser_set_select_multiple(file_object_p, multi_files);
  if (!prog_config.working_dir.empty()) {
    std::string temp(prog_config.working_dir);
    temp +=  "/faxout";
    gtk_file_chooser_set_current_folder(file_object_p, temp.c_str());
  }

#if !(GTK_CHECK_VERSION(3,12,0))
# if GTK_CHECK_VERSION(2,14,0)
  GtkBox* hbox_p = GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(get_win())));
# else
  GtkBox* hbox_p = GTK_BOX(GTK_DIALOG(get_win())->action_area);
# endif
  gtk_box_set_homogeneous(hbox_p, true);
#endif

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(FileReadSelectDialogCB::frs_dialog_response), this);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void FileReadSelectDialog::on_delete_event(void) {
  // override to do nothing - delete events are dealt with in the
  // response handler
}

std::pair<const char*, char* const*> FileReadSelectDialog::get_view_file_parms(void) {

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
    write_error("UTF-8 conversion error in FileReadSelectDialog::get_view_file_parms()\n");
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

  view_parms.push_back(get_filename_string());

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

void FileReadSelectDialog::view_file_impl(void) {

  // check pre-conditions
  std::string filename(get_filename_string());
  if (filename.empty()
      || filename[filename.size() - 1] == '/'
      || access(filename.c_str(), R_OK)) {
    beep();
    return;
  }

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> view_file_parms(get_view_file_parms());

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

void FileReadSelectDialog::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

std::string FileReadSelectDialog::get_filename_string(void) {
  GcharScopedHandle file_name_h(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(get_win())));
  if (file_name_h.get()) return std::string((const char*)file_name_h.get());
  return std::string();
}

void FileReadSelectDialog::set_result(void) {

  result.clear();

  GSList* file_list_p = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(get_win()));
  for (GSList* elt = file_list_p; elt; elt = elt->next) {
    result.push_back((const char*)elt->data);
    g_free(elt->data);
  }
  g_slist_free(file_list_p);

  try {
    std::transform(result.begin(), result.end(), result.begin(),
		   Utf8::filename_to_utf8);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in FileReadSelectDialog::set_result()\n");
  }
}


void GplDialogCB::gpl_dialog_selected(GtkWidget* widget_p, void* data) {
  GplDialog* instance_p = static_cast<GplDialog*>(data);
  gtk_widget_hide(GTK_WIDGET(instance_p->get_win()));
  if (widget_p == instance_p->accept_button_p) instance_p->result = GplDialog::accepted;
  else if (widget_p == instance_p->reject_button_p) instance_p->result = GplDialog::rejected;
  else write_error("Callback error in GplDialogCB::gpl_dialog_selected()\n");
  instance_p->close();
}

gboolean GplDialogCB::gpl_dialog_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  GplDialog* instance_p = static_cast<GplDialog*>(data);
  int keycode = event_p->keyval;
  
#if GTK_CHECK_VERSION(2,99,0)
  if (keycode == GDK_KEY_Escape) instance_p->on_delete_event();
  else if (keycode == GDK_KEY_Home || keycode == GDK_KEY_End
	   || keycode == GDK_KEY_Up || keycode == GDK_KEY_Down
	   || keycode == GDK_KEY_Page_Up || keycode == GDK_KEY_Page_Down) {
    gtk_widget_event(GTK_WIDGET(instance_p->text_view_p), (GdkEvent*)event_p);
  }
#else
  if (keycode == GDK_Escape) instance_p->on_delete_event();
  else if (keycode == GDK_Home || keycode == GDK_End
	   || keycode == GDK_Up || keycode == GDK_Down
	   || keycode == GDK_Page_Up || keycode == GDK_Page_Down) {
    gtk_widget_event(GTK_WIDGET(instance_p->text_view_p), (GdkEvent*)event_p);
  }
#endif
  return true;    // stop processing here
}

GplDialog::GplDialog(int size): WinBase(gettext("efax-gtk: Conditions, Notices and Disclaimers"),
				        prog_config.window_icon_h,
					true),
				standard_size(size),
				result(rejected) {

  accept_button_p = gtk_button_new_with_label(gettext("Accept"));
  reject_button_p = gtk_button_new_with_label(gettext("Reject"));
  
  GtkWidget* label_p = gtk_label_new(gettext("Do you accept the Conditions, Notices "
					     "and Disclaimers shown above?"));

  GtkTable* table_p = GTK_TABLE(gtk_table_new(3, 2, false));

  text_view_p = GTK_TEXT_VIEW(gtk_text_view_new());
  gtk_text_view_set_editable(text_view_p, false);

  PangoFontDescription* font_description = 
    pango_font_description_from_string(prog_config.fixed_font.c_str());
#if GTK_CHECK_VERSION(2,99,0)
  gtk_widget_override_font(GTK_WIDGET(text_view_p), font_description);
#else
  gtk_widget_modify_font(GTK_WIDGET(text_view_p), font_description);
#endif
  pango_font_description_free(font_description);

  gtk_text_buffer_set_text(gtk_text_view_get_buffer(text_view_p),
			   gpl_copyright_msg()->c_str(),
			   -1);

  GtkScrolledWindow* scrolled_window_p = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  gtk_scrolled_window_set_shadow_type(scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scrolled_window_p, GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled_window_p), GTK_WIDGET(text_view_p));

  gtk_table_attach(table_p, GTK_WIDGET(scrolled_window_p),
		   0, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   0, 0);

  gtk_table_attach(table_p, label_p,
		   0, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   0, standard_size/3);

  gtk_table_attach(table_p, accept_button_p,
		   0, 1, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   0, standard_size/3);

  gtk_table_attach(table_p, reject_button_p,
		   1, 2, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   0, standard_size/3);

  GobjHandle<GtkSizeGroup> group_h(gtk_size_group_new(GTK_SIZE_GROUP_BOTH));
  gtk_size_group_add_widget(group_h, accept_button_p);
  gtk_size_group_add_widget(group_h, reject_button_p);

  g_signal_connect(G_OBJECT(accept_button_p), "clicked",
		   G_CALLBACK(GplDialogCB::gpl_dialog_selected), this);
  g_signal_connect(G_OBJECT(reject_button_p), "clicked",
		   G_CALLBACK(GplDialogCB::gpl_dialog_selected), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(GplDialogCB::gpl_dialog_key_press_event), this);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  
  gtk_window_set_default_size(get_win(), standard_size * 25, standard_size * 16);
  
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/4);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  
  gtk_widget_grab_focus(GTK_WIDGET(get_win()));

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_focus(GTK_WIDGET(text_view_p), false);
  gtk_widget_set_can_focus(accept_button_p, false);
  gtk_widget_set_can_focus(reject_button_p, false);
#else
  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(text_view_p), GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(accept_button_p, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(reject_button_p, GTK_CAN_FOCUS);
#endif

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

int GplDialog::get_exec_val(void) const {
  return result;
}

void GplDialog::on_delete_event(void) {
  gtk_widget_hide(GTK_WIDGET(get_win()));
  result = rejected;
  close();
}


void InfoDialogCB::info_dialog_selected(GtkDialog*, int, void* data) {
  static_cast<InfoDialog*>(data)->close();
}

InfoDialog::InfoDialog(const char* text, const char* caption,
                       GtkMessageType message_type, GtkWindow* parent_p, bool modal):
                       WinBase(caption, prog_config.window_icon_h,
			       modal, parent_p,
			       GTK_WINDOW(gtk_message_dialog_new(0, GtkDialogFlags(0),
								 message_type,
								 GTK_BUTTONS_CLOSE,
								 "%s", text))) {

  // make further specialisations for a message dialog object

#if GTK_CHECK_VERSION(2,14,0)
  GtkWidget* action_area_p = gtk_dialog_get_action_area(GTK_DIALOG(get_win()));
#else
  GtkWidget* action_area_p = GTK_DIALOG(get_win())->action_area;
#endif
  gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area_p),
			    GTK_BUTTONBOX_SPREAD);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(InfoDialogCB::info_dialog_selected), this);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void InfoDialog::on_delete_event(void) {
  // the response event handler will cause close() to be called for
  // the GTK_RESPONSE_DELETE_EVENT id, so just return when we
  // subsequently get the delete event
  return;
}

void PromptDialogCB::prompt_dialog_selected(GtkDialog*, int id, void* data) {
  PromptDialog* instance_p = static_cast<PromptDialog*>(data);
  if (id == GTK_RESPONSE_ACCEPT) {
    instance_p->result = true;
    instance_p->accepted();
    instance_p->close();
  }
  else if (id == GTK_RESPONSE_REJECT) {
    instance_p->result = false;
    instance_p->rejected();
    instance_p->close();
  }
}

PromptDialog::PromptDialog(const char* text, const char* caption,
			   int standard_size, GtkWindow* parent_p, bool modal):
                           WinBase(caption, prog_config.window_icon_h,
				   modal, parent_p,
				   GTK_WINDOW(gtk_dialog_new_with_buttons(0, 0,
#if GTK_CHECK_VERSION(2,99,0)
						  GtkDialogFlags(0),
#else
						  GTK_DIALOG_NO_SEPARATOR,
#endif
						  GTK_STOCK_CANCEL,
						  GTK_RESPONSE_REJECT,
						  GTK_STOCK_OK,
						  GTK_RESPONSE_ACCEPT,
						  static_cast<void*>(0)))),
			   result(false) {

  GtkWidget* label_p = gtk_label_new(text);
  gtk_label_set_line_wrap(GTK_LABEL(label_p), true);
  gtk_misc_set_padding(GTK_MISC(label_p), standard_size/2, standard_size/2);
#if GTK_CHECK_VERSION(2,14,0)
  GtkWidget* content_area_p = gtk_dialog_get_content_area(GTK_DIALOG(get_win()));
#else
  GtkWidget* content_area_p = GTK_DIALOG(get_win())->vbox;
#endif
  gtk_container_add(GTK_CONTAINER(content_area_p), 
		    label_p);

  gtk_dialog_set_default_response(GTK_DIALOG(get_win()), GTK_RESPONSE_ACCEPT);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(PromptDialogCB::prompt_dialog_selected), this);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

int PromptDialog::get_exec_val(void) const {
  return result;
}

void PromptDialog::on_delete_event(void) {
  result = false;
  rejected();
  close();
}

void AboutEfaxGtkDialogCB::about_efax_gtk_dialog_selected(GtkDialog*, int, void* data) {
  static_cast<AboutEfaxGtkDialog*>(data)->close();
}

AboutEfaxGtkDialog::AboutEfaxGtkDialog(GtkWindow* parent_p, bool modal):
                                       WinBase(gettext("About efax-gtk"), prog_config.window_icon_h,
					       modal, parent_p,
					       GTK_WINDOW(gtk_about_dialog_new())) {

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(AboutEfaxGtkDialogCB::about_efax_gtk_dialog_selected), this);

  gtk_window_set_resizable(get_win(), false);

  if (parent_p && modal) gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(get_win()), "efax-gtk");
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(get_win()), VERSION);
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(get_win()),
				 gettext("Copyright (C) 2001 - 2014 Chris Vine"));
  gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(get_win()),
			       gpl_copyright_msg()->c_str()); // this function calls g_strdup()
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(get_win()),
				gettext("This program was written by Chris Vine "
					"and is released under the GNU General "
					"Public License, version 2. It is a front "
					"end for efax. efax was written by Ed Casas"));
  gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(get_win()), prog_config.window_icon_h);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void AboutEfaxGtkDialog::on_delete_event(void) {
  // the response event handler will cause close() to be called for
  // the GTK_RESPONSE_DELETE_EVENT id, so just return when we
  // subsequently get the delete event
  return;
}

void AboutEfaxDialogCB::about_efax_dialog_selected(GtkDialog*, int, void* data) {
  static_cast<AboutEfaxDialog*>(data)->close();
}

AboutEfaxDialog::AboutEfaxDialog(GtkWindow* parent_p, bool modal):
                                 WinBase(gettext("About efax"), prog_config.window_icon_h,
					 modal, parent_p,
					 GTK_WINDOW(gtk_about_dialog_new())) {

#if GTK_CHECK_VERSION(2,14,0)
  GtkWidget* action_area_p = gtk_dialog_get_action_area(GTK_DIALOG(get_win()));
#else
  GtkWidget* action_area_p = GTK_DIALOG(get_win())->action_area;
#endif
  gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area_p),
			    GTK_BUTTONBOX_SPREAD);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(AboutEfaxDialogCB::about_efax_dialog_selected), this);

  gtk_window_set_resizable(get_win(), false);

  if (parent_p && modal) gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(get_win()), "efax");
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(get_win()),
				 gettext("efax is copyright (C) 1999 Ed Casas"));
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(get_win()),
				gettext("This program (efax-gtk) is a front end "
					"for efax. efax is a program released "
					"under the GNU General Public License, "
					"version 2 by Ed Casas"));
  gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(get_win()), prog_config.window_icon_h);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

void AboutEfaxDialog::on_delete_event(void) {
  // the response event handler will cause close() to be called for
  // the GTK_RESPONSE_DELETE_EVENT id, so just return when we
  // subsequently get the delete event
  return;
}
