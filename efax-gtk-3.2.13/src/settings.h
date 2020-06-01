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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "prog_defs.h"

#include <unistd.h>

#include <string>
#include <utility>

#include <gtk/gtk.h>

#include "settings_help.h"

#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/widget.h>
#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/convert.h>


namespace IdentityPageCB {
  extern "C" void identity_page_button_clicked(GtkWidget*, void*);
}

class IdentityPage: public MainWidgetBase {
  IdentityMessages help_messages;

  GtkWidget* name_entry_p;
  GtkWidget* number_entry_p;
  GtkWidget* name_help_button_p;
  GtkWidget* number_help_button_p;
public:
  friend void IdentityPageCB::identity_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_name(void) const;
  std::string get_number(void) const;
  void set_name(const std::string& name);
  void set_number(const std::string& number);
  void clear(void);

  IdentityPage(const int);
};

namespace ModemPageCB {
  extern "C" void modem_page_button_clicked(GtkWidget*, void*);
}

class ModemPage: public MainWidgetBase {
  ModemMessages help_messages;

  GtkWidget* device_entry_p;
  GtkWidget* lock_entry_p;
  GtkWidget* capabilities_entry_p;
  GtkWidget* rings_spin_button_p;
  GtkWidget* auto_button_p;
  GtkWidget* class1_button_p;
  GtkWidget* class2_button_p;
  GtkWidget* class20_button_p;
  GtkWidget* tone_button_p;
  GtkWidget* pulse_button_p;
  GtkWidget* device_help_button_p;
  GtkWidget* lock_help_button_p;
  GtkWidget* capabilities_help_button_p;
  GtkWidget* rings_help_button_p;
  GtkWidget* class_help_button_p;
  GtkWidget* dialmode_help_button_p;
public:
  friend void ModemPageCB::modem_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_device(void) const;
  std::string get_lock(void) const;
  std::string get_capabilities(void) const;
  std::string get_rings(void) const;
  std::string get_class(void) const;
  std::string get_dialmode(void) const;
  void set_device(const std::string& device);
  void set_lock(const std::string& lock);
  void set_capabilities(const std::string& capabilities);
  void set_rings(const std::string& rings);
  void set_class(const std::string& class_string);
  void set_dialmode(const std::string& dialmode);
  void clear(void);

  ModemPage(const int);
};

namespace ParmsPageCB {
  extern "C" void parms_page_button_clicked(GtkWidget*, void*);
}

class ParmsPage: public MainWidgetBase {
  ParmsMessages help_messages;

  GtkWidget* init_entry_p;
  GtkWidget* reset_entry_p;
  GtkWidget* parms_entry_p;
  GtkWidget* init_help_button_p;
  GtkWidget* reset_help_button_p;
  GtkWidget* parms_help_button_p;
public:
  friend void ParmsPageCB::parms_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_init(void) const;
  std::string get_reset(void) const;
  std::string get_parms(void) const;
  void set_init(const std::string& init);
  void set_reset(const std::string& reset);
  void set_parms(const std::string& parms);
  void clear(void);
  ParmsPage(const int);
};

namespace PrintPageCB {
  extern "C" void print_page_button_clicked(GtkWidget*, void*);
}

class PrintPage: public MainWidgetBase {
  PrintMessages help_messages;

  GtkWidget* command_label_p;
  GtkWidget* popup_label_p;
  GobjHandle<GtkWidget> gtkprint_check_button_h;
  GtkWidget* command_entry_p;
  GtkWidget* shrink_label_p;
  GtkWidget* shrink_spin_button_p;
  GtkWidget* popup_check_button_p;
  GobjHandle<GtkWidget> gtkprint_help_button_h;
  GtkWidget* command_help_button_p;
  GtkWidget* shrink_help_button_p;
  GtkWidget* popup_help_button_p;

  void gtkprint_button_toggled_impl(void);
public:
  friend void PrintPageCB::print_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_gtkprint(void) const;
  std::string get_command(void) const;
  std::string get_shrink(void) const;
  std::string get_popup(void) const;
  void set_gtkprint(const std::string& gtkprint_string);
  void set_command(const std::string& command);
  void set_shrink(const std::string& shrink);
  void set_popup(const std::string& popup_string);

  void clear(void);
  PrintPage(const int);
};

namespace ViewPageCB {
  extern "C" void view_page_button_clicked(GtkWidget*, void*);
}

class ViewPage: public MainWidgetBase {
  ViewMessages help_messages;

  GtkWidget* ps_view_command_entry_p;
public:
  friend void ViewPageCB::view_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_ps_view_command(void) const;
  void set_ps_view_command(const std::string& command);
  void clear(void);
  ViewPage(const int);
};

namespace SockPageCB {
  extern "C" void sock_page_button_clicked(GtkWidget*, void*);
}

class SockPage: public MainWidgetBase {
  SockMessages help_messages;

  GtkWidget* run_server_button_p;
  GtkWidget* popup_button_p;
  GtkWidget* localhost_button_p;
  GtkWidget* other_address_button_p;
  GtkWidget* port_spin_button_p;
  GtkWidget* other_addresses_entry_p;
  GtkWidget* ipv4_button_p;
  GtkWidget* ipv6_button_p;
  GtkWidget* run_server_help_button_p;
  GtkWidget* popup_help_button_p;
  GtkWidget* port_help_button_p;
  GtkWidget* client_address_help_button_p;
  GtkWidget* ip_family_help_button_p;

  void other_address_button_toggled_impl(void);
public:
  friend void SockPageCB::sock_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_run_server(void) const ;
  std::string get_popup(void) const;
  std::string get_port(void) const;
  std::string get_if_other_address(void) const;
  std::string get_other_addresses(void) const;
  std::string get_ip_family(void) const;
  void set_run_server(const std::string& run_server_string);
  void set_popup(const std::string& popup_string);
  void set_port(const std::string& port_string);
  void set_if_other_address(const std::string& if_other_address_string);
  void set_other_addresses(const std::string& other_addresses_string);
  void set_ip_family(const std::string& ip_family_string);

  void clear(void);
  SockPage(const int);
};

namespace ReceivePageCB {
  extern "C" void receive_page_button_clicked(GtkWidget*, void*);
}

class ReceivePage: public MainWidgetBase {
  ReceiveMessages help_messages;

  GtkWidget* popup_button_p;
  GtkWidget* exec_button_p;
  GtkWidget* prog_entry_p;
  GtkWidget* popup_help_button_p;
  GtkWidget* exec_help_button_p;

  void exec_button_toggled_impl(void);
public:
  friend void ReceivePageCB::receive_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_popup(void) const;
  std::string get_exec(void) const;
  std::string get_prog(void) const;
  void set_popup(const std::string& popup_string);
  void set_exec(const std::string& exec_string);
  void set_prog(const std::string& prog_string);
  void clear(void);
  ReceivePage(const int);
};

namespace SendPageCB {
  extern "C" void send_page_button_clicked(GtkWidget*, void*);
}

class SendPage: public MainWidgetBase {
  SendMessages help_messages;

  GtkWidget* standard_button_p;
  GtkWidget* fine_button_p;
  GtkWidget* header_check_button_p;
  GtkWidget* redial_check_button_p;
  GtkWidget* redial_spin_label_p;
  GtkWidget* redial_spin_button_p;
  GtkWidget* dial_prefix_entry_p;
  GtkWidget* res_help_button_p;
  GtkWidget* header_help_button_p;
  GtkWidget* redial_help_button_p;
  GtkWidget* dial_prefix_help_button_p;

  void redial_check_button_toggled_impl(void);
public:
  friend void SendPageCB::send_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_res(void) const;
  std::string get_addr_in_header(void) const;
  std::string get_redial(void) const;
  std::string get_redial_interval(void) const;
  std::string get_dial_prefix(void) const;
  void set_res(const std::string& res);
  void set_addr_in_header(const std::string& res);
  void set_redial(const std::string& redial);
  void set_redial_interval(const std::string& interval);
  void set_dial_prefix(const std::string& prefix);
  void clear(void);
  SendPage(const int);
};

namespace LoggingPageCB {
  extern "C" void logging_page_button_clicked(GtkWidget*, void*);
}

class LoggingPage: public MainWidgetBase {
  LoggingMessages help_messages;

  GtkWidget* logfile_entry_p;
public:
  friend void LoggingPageCB::logging_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_logfile(void) const;
  void set_logfile(const std::string& logfile);
  void clear(void);
  LoggingPage(const int);
};

namespace PagePageCB {
  extern "C" void page_page_button_clicked(GtkWidget*, void*);
}

class PagePage: public MainWidgetBase {
  PageMessages help_messages;

  GtkWidget* a4_button_p;
  GtkWidget* letter_button_p;
  GtkWidget* legal_button_p;
  GtkWidget* page_help_button_p;
public:
  friend void PagePageCB::page_page_button_clicked(GtkWidget*, void*);

  //sigc::signal2<void, const std::string&, const std::string&> show_help_sig;
  EmitterArg<const std::pair<std::string, std::string>&> show_help_sig;
  std::string get_page(void) const;
  void set_page(const std::string& page);
  void clear(void);
  PagePage(const int);
};

namespace SettingsDialogCB {
  extern "C" void settings_dialog_button_clicked(GtkWidget*, void*);
}

class SettingsDialog: public WinBase {

  static int dialog_count;

  const int standard_size;
  bool is_home_config;
  std::string rcfile;

  GtkWidget* ok_button_p;
  GtkWidget* cancel_button_p;
  GtkWidget* reset_button_p;

  IdentityPage identity_page;
  ModemPage modem_page;
  ParmsPage parms_page;
  PrintPage print_page;
  ViewPage view_page;
  SockPage sock_page;
  ReceivePage receive_page;
  SendPage send_page;
  LoggingPage logging_page;
  PagePage page_page;

  void read_config(bool search_localfile = true);
  bool write_config(void);
  bool get_prog_parm(const char*, std::string&, std::string&, std::string(*)(const std::string&));
  bool get_prog_parm(const char* name, std::string& line, std::string& result)
    {return get_prog_parm(name, line, result, Utf8::locale_to_utf8);}
  bool find_prog_parm(const char*, const std::string&);
  bool get_rcfile_path(bool search_localfile = true);
  void get_reset_settings_prompt(void);
  void get_reset_settings(void);
  void show_help(const std::pair<std::string, std::string>&);
  void strip(std::string&);
  bool is_ascii(const std::string&);
public:
  friend void SettingsDialogCB::settings_dialog_button_clicked(GtkWidget*, void*);

  static bool is_dialog(void) {return dialog_count;}

  // this signal can be used to indicate that settings have changed
  // it is emitted after this dialog has rewritten the ~/.efax-gtkrc
  // and has called configure_prog() and passes the messages
  // returned by configure_prog()
  EmitterArg<const std::string&> accepted;
  SettingsDialog(const int standard_size, GtkWindow* parent_p, bool skip_old_settings = false);
  ~SettingsDialog(void) {--dialog_count;}
};

#endif
