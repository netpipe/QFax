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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "prog_defs.h"

#include <unistd.h>
#include <sys/types.h>

#include <string>
#include <vector>
#include <list>
#include <utility>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "efax_controller.h"
#include "fax_list_manager.h"
#include "fax_list.h"
#include "socket_server.h"
#include "socket_list.h"
#include "tray_icon.h"
#include "helpfile.h"
#include "logger.h"

#include <c++-gtk-utils/pipes.h>
#include <c++-gtk-utils/shared_ptr.h>
#include <c++-gtk-utils/mutex.h>
#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/widget.h>
#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/reassembler.h>
#include <c++-gtk-utils/emitter.h>

#define MAINWIN_SAVE_FILE ".efax-gtk_mainwin_save"


class MessageText: public MainWidgetBase {

  GtkWidget* text_view_p;
  GtkTextMark* end_mark_p;
  GobjHandle<GtkTextTag> red_tag_h;
  Logger logger;

  void cleanify(std::string&);
public:
  void write_black_cb(const char* message);
  void write_red_cb(const char* message);
  void print_log(void) {logger.print_log();}
  static void print_page_setup(GtkWindow* parent_p) {Logger::print_page_setup(parent_p);}
  void view_log(const int standard_size) {logger.view_log(standard_size);}
  void reset_logfile(void) {logger.reset_logfile();}
  MessageText(GtkWindow* window_p);
};

class StatusLine: public MainWidgetBase {
  const int standard_size;
  GtkWidget* status_label_p;

  int get_max_status_text_height(const std::vector<std::string>&);
public:
  void set_status_line(const SharedPtr<std::vector<std::string> >&);
  void write_status_cb(const char* text);
  StatusLine(const int);
};

namespace MainWindowCB {
  extern "C" {
    void mainwin_button_clicked(GtkWidget*, void*);
    void mainwin_menuitem_activated(GtkMenuItem*, void*);
    gboolean mainwin_key_press_event(GtkWidget*, GdkEventKey*, void*);
    gboolean mainwin_visibility_notify_event(GtkWidget*, GdkEventVisibility*, void*);
    gboolean mainwin_window_state_event(GtkWidget*, GdkEventWindowState*, void*);
    gboolean mainwin_timer_event(void*);
#if GTK_CHECK_VERSION(2,99,0)
    void mainwin_button_style_updated(GtkWidget*, void*);
    gboolean mainwin_drawing_area_draw(GtkWidget*, cairo_t*, void*);
#else
    void mainwin_button_style_set(GtkWidget*, GtkStyle*, void*);
    gboolean mainwin_drawing_area_expose_event(GtkWidget*, GdkEventExpose*, void*);
#endif
  }
}

class MainWindow: public WinBase {

  const int standard_size;
  static PipeFifo error_pipe;
  static bool connected_to_stderr;
  bool obscured;
  bool minimised;
  pid_t prog_pid;
  std::string prog_fifo_name;
  guint error_pipe_tag;

  Emitter close_socket_list_dialog;
  EmitterArg<std::pair<SharedPtr<FilenamesList>, SharedPtr<Thread::Mutex::Lock> > > update_socket_list;

  GtkWidget* drawing_area_p;
  GtkWidget* file_button_p;
  GtkWidget* socket_button_p;
  GtkWidget* file_entry_p;
  GtkWidget* number_entry_p;
  GtkWidget* single_file_button_p;
  GtkWidget* multiple_file_button_p;
  GtkWidget* socket_list_button_p;
  GtkWidget* number_button_p;
  GtkWidget* send_button_p;
  GtkWidget* receive_answer_button_p;
  GtkWidget* receive_takeover_button_p;
  GtkWidget* receive_standby_button_p;
  GtkWidget* stop_button_p;

  GtkMenuItem* list_received_faxes_item_p;
  GtkMenuItem* list_sent_faxes_item_p;
  GtkMenuItem* socket_list_item_p;
  GtkMenuItem* single_file_item_p;
  GtkMenuItem* multiple_file_item_p;
  GtkMenuItem* address_book_item_p;
  GtkMenuItem* redial_queue_item_p;
  GtkMenuItem* settings_item_p;
  GtkMenuItem* quit_item_p;
  GtkMenuItem* print_log_item_p;
  GtkMenuItem* page_setup_log_item_p;
  GtkMenuItem* view_log_item_p;
  GtkMenuItem* about_efax_gtk_item_p;
  GtkMenuItem* about_efax_item_p; 
  GtkMenuItem* translations_item_p;
  GtkMenuItem* help_item_p;

  MessageText text_window;
  StatusLine status_line;
  EfaxController efax_controller;
  SocketServer socket_server;

  std::pair<std::string, unsigned int> notified_fax;
  std::string selected_socket_list_file;

  FaxListDialog* received_fax_list_p;
  FaxListDialog* sent_fax_list_p;
  SocketListDialog* socket_list_p;
  HelpDialog* helpfile_p;

  TrayIcon tray_icon;

  Utf8::Reassembler error_pipe_reassembler;

  // these are called by handlers of GTK+ signals
  void close_impl(void);
  void get_file_impl(void);
  void file_list_impl(void);
  void socket_list_impl(void);
  void addressbook_impl(void);
  void redial_queue_impl(void);
  void receive_impl(EfaxController::State);
  void fax_list_impl(FaxListEnum::Mode);
  void settings_impl(void);
  void about_impl(bool);
  void translations_impl(void);
  void helpfile_impl(void);
  void set_file_items_sensitive_impl(void);
  void set_socket_items_sensitive_impl(void);

  // these are connected to Emitter or Notifier objects, or connected
  // to the callback of a WatchSource (iowatch) object, and might also be
  // called by the handlers of GTK+ signals
  void sendfax_cb(void);
  void set_files_cb(const std::string&);
  void fax_received_notify_cb(const std::pair<std::string, std::string>&);
  void enter_socket_file_cb(const std::pair<std::string, std::string>&);
  void settings_changed_cb(const std::string&);
  void socket_filelist_changed_notify_cb(void);
  void fax_to_send_notify_cb(void);
  void nullify_notified_fax_cb(void);
  void set_number_cb(const std::string&);
  void tray_icon_menu_cb(int);
  void tray_icon_activated_cb(void);
  void quit_cb(void);
  void read_error_pipe_cb(bool&);
  void sig_event_cb(bool&);
  void start_hidden_check_cb(bool&);

  void fax_to_send_dialog(const std::pair<std::string, unsigned int>&);
  std::pair<int, int> get_max_button_text_extents(void);
  void get_window_settings(void);
  void save_window_settings(void);
  void strip(std::string&);

#if GTK_CHECK_VERSION(2,99,0)
  bool draw_fax_from_socket_notifier(cairo_t*);
#else
  bool draw_fax_from_socket_notifier(GdkEventExpose*);
  void button_style_set_impl(GtkStyle*);
#endif
  void button_style_updated_impl(void);

protected:
  virtual void on_delete_event(void);
public:
  friend void MainWindowCB::mainwin_button_clicked(GtkWidget*, void*);
  friend void MainWindowCB::mainwin_menuitem_activated(GtkMenuItem*, void*);
  friend gboolean MainWindowCB::mainwin_key_press_event(GtkWidget*, GdkEventKey*, void*);
  friend gboolean MainWindowCB::mainwin_visibility_notify_event(GtkWidget*,
								GdkEventVisibility*, void*);
  friend gboolean MainWindowCB::mainwin_window_state_event(GtkWidget*,
							   GdkEventWindowState*, void*);
#if GTK_CHECK_VERSION(2,99,0)
  friend void MainWindowCB::mainwin_button_style_updated(GtkWidget*, void*);
  friend gboolean MainWindowCB::mainwin_drawing_area_draw(GtkWidget*,
							  cairo_t*, void*);
#else
  friend void MainWindowCB::mainwin_button_style_set(GtkWidget*, GtkStyle*, void*);
  friend gboolean MainWindowCB::mainwin_drawing_area_expose_event(GtkWidget*,
								  GdkEventExpose*, void*);
#endif

  friend ssize_t write_error(const char*);
  friend int connect_to_stderr(void);

  void remove_from_socket_server_filelist(const std::string& file);
  void present_window(void);
  MainWindow(const std::string&, bool start_hidden = false,
	     bool start_in_standby = false, const char* filename = 0);
  ~MainWindow(void);
};

#endif
