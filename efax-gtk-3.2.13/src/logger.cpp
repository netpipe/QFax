/* Copyright (C) 2001 to 2009 and 2011 Chris Vine

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


#include <ctime>
#include <memory>
#include <cstring>
#include <ios>
#include <ostream>
#include <istream>

#include <pango/pango-font.h>

#include <gdk/gdkkeysyms.h> // the key codes are here

#include "logger.h"
#include "dialogs.h"

#include <c++-gtk-utils/shared_handle.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/intrusive_ptr.h>
#include <c++-gtk-utils/text_print_manager.h>
#include <c++-gtk-utils/reassembler.h>
#include <c++-gtk-utils/timeout.h>
#include <c++-gtk-utils/convert.h>
#include <c++-gtk-utils/cgu_config.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif


#define LOGFILE_TIMER_INTERVAL 60000    // number of milliseconds between flushing of logfile
#define LOGFILE_MARK_COUNT 15*60000/LOGFILE_TIMER_INTERVAL // to mark the logfile at 15 minute intervals


Logger::Logger(const char* default_dir, GtkWindow* parent_p_):
                                         logfile_count(0),
                                         default_dirname(default_dir),
                                         parent_p(parent_p_) {


  default_dirname += '/';

  // open the log file if required
  if (!prog_config.logfile_name.empty()) {

    try {
      std::string temp(Utf8::filename_from_utf8(prog_config.logfile_name));
      if (temp[0] != '/') temp.insert(0, default_dirname); // provide an absolute path name
      logfile.open(temp.c_str(), std::ios::app | std::ios::out);
      if (!logfile) {
	std::string message("Can't open logfile ");
	message += temp;
	message += '\n';
	write_error(message.c_str());
	temp = "";
	// lock the Prog_config object while we modify it
	Thread::Mutex::Lock lock(*prog_config.mutex_p);
	prog_config.logfile_name = "";
      }
      else {
	struct std::tm* time_p;
	std::time_t time_count;
	std::time(&time_count);
	time_p = std::localtime(&time_count);
      
	const char date_description_format[] = "%H%M %Z %d %b %Y";
	const int max_description_datesize = 126;
	char date_description[max_description_datesize];
	std::strftime(date_description, max_description_datesize, date_description_format, time_p);

	std::string date_desc(date_description);
	if (!Utf8::validate(date_desc)) {
	  try {
	    date_desc = Utf8::locale_to_utf8(date_desc);
	  }
	  catch (Utf8::ConversionError&) {
	    write_error("UTF-8 conversion error in Logger::Logger()\n");
	    date_desc = "";
	  }
	}
	  
	logfile << "\n***** "
		<< gettext("Beginning fax log: ") << date_desc
		<< " *****\n" << std::endl;
	// set a timeout to flush every one minute
#if GLIB_CHECK_VERSION(2,14,0)
	timer_tag = start_timeout_seconds(LOGFILE_TIMER_INTERVAL/1000,
					  Callback::make(*this, &Logger::logfile_timer_cb));
#else
	timer_tag = start_timeout(LOGFILE_TIMER_INTERVAL,
				  Callback::make(*this, &Logger::logfile_timer_cb));
#endif
      }

      // save the logfile name in the local version of logfile_name 
      // in case reset_logfile() is called later so we can test whether
      // it has changed, and so we can print and view the file
      logfile_name = temp;
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in Logger::Logger()\n");
      beep();
      // lock the Prog_config object while we modify it
      Thread::Mutex::Lock lock(*prog_config.mutex_p);
      prog_config.logfile_name = "";
    }
  }
}

Logger::~Logger(void) {
  if (!logfile_name.empty()) {

    g_source_remove(timer_tag);
    
    struct std::tm* time_p;
    std::time_t time_count;
    std::time(&time_count);
    time_p = std::localtime(&time_count);
  
    const char date_description_format[] = "%H%M %Z %d %b %Y";
    const int max_description_datesize = 126;
    char date_description[max_description_datesize];
    std::strftime(date_description, max_description_datesize, date_description_format, time_p);
  
    std::string date_desc(date_description);
    if (!Utf8::validate(date_desc)) {
      try {
	date_desc = Utf8::locale_to_utf8(date_desc);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in Logger::~Logger()\n");
	date_desc = "";
      }
    }
	  
    logfile << "\n***** "
	    << gettext("Ending fax log: ") << date_desc
	    << " *****\n" << std::endl;
    logfile.close();
    logfile.clear();
  }
}

void Logger::write_to_log(const char* text) {
  if (!logfile_name.empty()) logfile << text;
}

void Logger::reset_logfile(void) {

  // check pre-conditions
  try {
    if (logfile_name == Utf8::filename_from_utf8(prog_config.logfile_name)) return; // no change!
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in Logger::reset_logfile()\n");
    beep();
    // lock the Prog_config object while we modify it
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    prog_config.logfile_name = "";
    return;
  }

  // proceed
  // first close the log file if required
  if (!logfile_name.empty()) {

    struct std::tm* time_p;
    std::time_t time_count;
    std::time(&time_count);
    time_p = std::localtime(&time_count);
  
    const char date_description_format[] = "%H%M %Z %d %b %Y";
    const int max_description_datesize = 126;
    char date_description[max_description_datesize];
    std::strftime(date_description, max_description_datesize, date_description_format, time_p);
  
    std::string date_desc(date_description);
    if (!Utf8::validate(date_desc)) {
      try {
	date_desc = Utf8::locale_to_utf8(date_desc);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in Logger::reset_logfile()\n");
	date_desc = "";
      }
    }
	  
    logfile << "\n***** "
	    << gettext("Ending fax log: ") << date_desc
	    << " *****\n" << std::endl;
    logfile.close();
    logfile.clear();

    logfile_name = "";
    logfile_count = 0;
    // and now disconnect the old timer connection
    g_source_remove(timer_tag);
  }

  // now open the new log file if required
  if (!prog_config.logfile_name.empty()) {
    try {
      std::string temp(Utf8::filename_from_utf8(prog_config.logfile_name));
      if (temp[0] != '/') temp.insert(0, default_dirname); // provide an absolute path name
      logfile.open(temp.c_str(), std::ios::app | std::ios::out);
      if (!logfile) {
	std::string message("Can't open logfile ");
	message += temp;
	message += '\n';
	write_error(message.c_str());
	temp = "";
	// lock the Prog_config object while we modify it
	Thread::Mutex::Lock lock(*prog_config.mutex_p);
	prog_config.logfile_name = "";
      }
      else {
	struct std::tm* time_p;
	std::time_t time_count;
	std::time(&time_count);
	time_p = std::localtime(&time_count);
      
	const char date_description_format[] = "%H%M %Z %d %b %Y";
	const int max_description_datesize = 126;
	char date_description[max_description_datesize];
	std::strftime(date_description, max_description_datesize, date_description_format, time_p);
	std::string date_desc(date_description);
	if (!Utf8::validate(date_desc)) {
	  try {
	    date_desc = Utf8::locale_to_utf8(date_desc);
	  }
	  catch (Utf8::ConversionError&) {
	    write_error("UTF-8 conversion error in Logger::reset_logfile()\n");
	    date_desc = "";
	  }
	}
	  
	logfile << "\n***** "
		<< gettext("Beginning fax log: ") << date_desc
		<< " *****\n" << std::endl;
	// set a timeout to flush every one minute
#if GLIB_CHECK_VERSION(2,14,0)
	timer_tag = start_timeout_seconds(LOGFILE_TIMER_INTERVAL/1000,
					  Callback::make(*this, &Logger::logfile_timer_cb));
#else
	timer_tag = start_timeout(LOGFILE_TIMER_INTERVAL,
				  Callback::make(*this, &Logger::logfile_timer_cb));
#endif
      }
      logfile_count = 0;
      // save the logfile name in the local version of logfile_name 
      // in case reset_logfile() is called later so we can test whether
      // it has changed, and so we can print and view the file
      logfile_name = temp;
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in Logger::reset_logfile()\n");
      beep();
      // lock the Prog_config object while we modify it
      Thread::Mutex::Lock lock(*prog_config.mutex_p);
      prog_config.logfile_name = "";
    }
  }
}

bool Logger::log_to_string(std::string& text) {

  if (logfile_name.empty()) return false;

  logfile.flush();
  text = "";
  std::ifstream infile;
  
  infile.open(logfile_name.c_str(), std::ios::in | std::ios:: binary);
  if (!infile) {
    new InfoDialog(gettext("Cannot open log file for reading"),
		   gettext("efax-gtk: print logfile"),
		   GTK_MESSAGE_ERROR,
		   parent_p,
		   true);
    // there is no memory leak - the exec() method has not been called so the dialog
    // is self-owning and will delete itself when it is closed
    return false;
  }
  const int BLOCKSIZE = 1024;
  char block[BLOCKSIZE];
  while (infile) {
    infile.read(block, BLOCKSIZE - 1);
    std::streamsize extracted = infile.gcount();
    if (extracted > 0) {
      block[extracted] = 0;
      text += block;
    }
  }
  return true;
}

void Logger::print_log(void) {

#if !defined(CGU_USE_AUTO_PTR) && CGU_API_VERSION>=20
  std::unique_ptr<std::string> text(new std::string);
#else
  std::auto_ptr<std::string> text(new std::string);
#endif
  if (log_to_string(*text)) {
    IntrusivePtr<TextPrintManager> print_manager_i(TextPrintManager::create_manager(parent_p));
    print_manager_i->set_text(text);
    print_manager_i->print();
  }
}

void Logger::print_page_setup(GtkWindow* parent_p) {

  TextPrintManager::page_setup(parent_p);
}

void Logger::view_log(const int standard_size) {

  if (logfile_name.empty()) return;

  logfile.flush();
  try {
    new LogViewDialog(standard_size, logfile_name, parent_p);
    // there is no memory leak - the exec() method has not been called so the dialog
    // is self-owning and will delete itself when it is closed
  }
  catch (LogViewFileError&) {
    new InfoDialog(gettext("Cannot open log file for viewing"),
		   gettext("efax-gtk: view logfile"),
		   GTK_MESSAGE_ERROR,
		   parent_p,
		   true);
    // there is no memory leak - the exec() method has not been called so the dialog
    // is self-owning and will delete itself when it is closed
  }
}

void Logger::logfile_timer_cb(bool&) {
  
  logfile_count++;
  if (logfile_count >= LOGFILE_MARK_COUNT) {
    logfile_count = 0;

    struct std::tm* time_p;
    std::time_t time_count;
    std::time(&time_count);
    time_p = std::localtime(&time_count);
      
    const char date_description_format[] = "%H%M %Z %d %b %Y";
    const int max_description_datesize = 126;
    char date_description[max_description_datesize];
    std::strftime(date_description, max_description_datesize, date_description_format, time_p);

    std::string date_desc(date_description);
    if (!Utf8::validate(date_desc)) {
      try {
	date_desc = Utf8::locale_to_utf8(date_desc);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in Logger::logfile_timer_cb()\n");
	date_desc = "";
      }
    }
	  
    logfile << "\n** " << date_desc << " **\n" << std::endl;
  }
  logfile.flush();
}

void LogViewDialogCB::log_view_dialog_button_clicked(GtkWidget*, void* data) {
  static_cast<LogViewDialog*>(data)->close();
}

gboolean LogViewDialogCB::log_view_dialog_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  LogViewDialog* instance_p = static_cast<LogViewDialog*>(data);
  int keycode = event_p->keyval;
  
#if GTK_CHECK_VERSION(2,99,0)
  if (keycode == GDK_KEY_Home || keycode == GDK_KEY_End
      || keycode == GDK_KEY_Up || keycode == GDK_KEY_Down
      || keycode == GDK_KEY_Page_Up || keycode == GDK_KEY_Page_Down) {
    gtk_widget_event(GTK_WIDGET(instance_p->text_view_p), (GdkEvent*)event_p);
    return true;    // stop processing here
  }
#else
  if (keycode == GDK_Home || keycode == GDK_End
      || keycode == GDK_Up || keycode == GDK_Down
      || keycode == GDK_Page_Up || keycode == GDK_Page_Down) {
    gtk_widget_event(GTK_WIDGET(instance_p->text_view_p), (GdkEvent*)event_p);
    return true;    // stop processing here
  }
#endif
  return false;     // pass on the key event
}

LogViewDialog::LogViewDialog(const int standard_size,
			     const std::string& logfile_name,
			     GtkWindow* parent_p):
                                   WinBase(gettext("efax-gtk: View log"),
					   prog_config.window_icon_h,
					   true,
					   parent_p) {

  GtkBox* vbox_p = GTK_BOX(gtk_vbox_new(false, 0));
  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(vbox_p));

  GtkWidget* scrolled_window_p = gtk_scrolled_window_new(0, 0);
  gtk_box_pack_start(vbox_p, scrolled_window_p, true, true, standard_size/3);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window_p),
				      GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_p),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  gtk_box_pack_start(vbox_p, button_box_p, false, false, standard_size/3);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);

  GtkWidget* close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  text_view_p = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p), text_view_p);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view_p), GTK_WRAP_WORD);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_p), false);
  PangoFontDescription* font_description = 
    pango_font_description_from_string(prog_config.fixed_font.c_str());
#if GTK_CHECK_VERSION(2,99,0)
  gtk_widget_override_font(text_view_p, font_description);
#else
  gtk_widget_modify_font(text_view_p, font_description);
#endif
  pango_font_description_free(font_description);

  std::ifstream infile;
  infile.open(logfile_name.c_str(), std::ios::in | std::ios:: binary);
  if (!infile) {
    throw LogViewFileError();
  }

  GtkTextIter end_iter;
  GtkTextBuffer* buffer_p = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_p));
  gtk_text_buffer_get_end_iter(buffer_p, &end_iter);
  Utf8::Reassembler reassembler;
  const int BLOCKSIZE = 1024;
  char block[BLOCKSIZE];
  while (infile) {
    infile.read(block, BLOCKSIZE);
    std::streamsize extracted = infile.gcount();
    if (extracted > 0) {
      SharedHandle<char*> read_text_h(reassembler(block, extracted));
      if (!read_text_h.get()) {
	const char message[] = "Invalid UTF-8 text found in logfile\n";
	write_error(message);
	gtk_text_buffer_insert(buffer_p, &end_iter, message, sizeof(message) - 1);
      }
      else {
	gtk_text_buffer_insert(buffer_p, &end_iter, read_text_h.get(), std::strlen(read_text_h));
      }
    }
  }

  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(LogViewDialogCB::log_view_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(LogViewDialogCB::log_view_dialog_key_press_event), this);

  gtk_window_set_default_size(get_win(), standard_size * 25, standard_size * 16);
  
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/4);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  
  gtk_widget_grab_focus(GTK_WIDGET(get_win()));
#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_focus(text_view_p, false);
#else
  GTK_WIDGET_UNSET_FLAGS(text_view_p, GTK_CAN_FOCUS);
#endif

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}
