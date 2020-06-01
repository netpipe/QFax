/* Copyright (C) 2001 to 2013 Chris Vine

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

#include <unistd.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <ios>
#include <ostream>
#include <sstream>
#include <cstdlib>
#include <cstring>

#include <gdk/gdk.h>

#include "settings.h"
#include "settings_icons.h"
#include "dialogs.h"
#include "utils/utf8_utils.h"

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

static inline bool not_ascii_char(char c) {
  if (static_cast<signed char>(c) < 0) return true;
  return false;
}

int SettingsDialog::dialog_count = 0;


void IdentityPageCB::identity_page_button_clicked(GtkWidget* widget_p, void* data) {
  IdentityPage* instance_p = static_cast<IdentityPage*>(data);
  int message_index;
  if (widget_p == instance_p->name_help_button_p) {
    message_index = IdentityMessages::name;
  }
  else if (widget_p == instance_p->number_help_button_p) {
    message_index = IdentityMessages::number;
  }
  else {
    write_error("Callback error in IdentityPageCB::identity_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

IdentityPage::IdentityPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* name_label_p = gtk_label_new(gettext("Name: "));
  GtkWidget* number_label_p = gtk_label_new(gettext("Number: "));

  gtk_label_set_justify(GTK_LABEL(name_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(number_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(name_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(number_label_p), 1, 0.5);

  name_entry_p = gtk_entry_new();
  number_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(name_entry_p, standard_size * 7, -1);
  gtk_widget_set_size_request(number_entry_p, standard_size * 7, -1);

  name_help_button_p = gtk_button_new();
  number_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(name_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(number_help_button_p), image_p);
  }

  gtk_table_attach(table_p, name_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, name_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, name_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  gtk_table_attach(table_p, number_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, number_entry_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, number_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  g_signal_connect(G_OBJECT(name_help_button_p), "clicked",
		   G_CALLBACK(IdentityPageCB::identity_page_button_clicked), this);
  g_signal_connect(G_OBJECT(number_help_button_p), "clicked",
		   G_CALLBACK(IdentityPageCB::identity_page_button_clicked), this);
  
  gtk_widget_set_tooltip_text(name_help_button_p,
			      help_messages.get_message(IdentityMessages::name).c_str());
  gtk_widget_set_tooltip_text(number_help_button_p,
			      help_messages.get_message(IdentityMessages::number).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string IdentityPage::get_name(void) const {
  return gtk_entry_get_text(GTK_ENTRY(name_entry_p));
}

std::string IdentityPage::get_number(void) const {
  return gtk_entry_get_text(GTK_ENTRY(number_entry_p));
}

void IdentityPage::set_name(const std::string& name) {
  gtk_entry_set_text(GTK_ENTRY(name_entry_p), name.c_str());
}

void IdentityPage::set_number(const std::string& number) {
  gtk_entry_set_text(GTK_ENTRY(number_entry_p), number.c_str());
}

void IdentityPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(name_entry_p), "");
  gtk_entry_set_text(GTK_ENTRY(number_entry_p), "");
}

void ModemPageCB::modem_page_button_clicked(GtkWidget* widget_p, void* data) {
  ModemPage* instance_p = static_cast<ModemPage*>(data);
  int message_index;
  if (widget_p == instance_p->device_help_button_p) {
    message_index = ModemMessages::device;
  }
  else if (widget_p == instance_p->lock_help_button_p) {
    message_index = ModemMessages::lock;
  }
  else if (widget_p == instance_p->capabilities_help_button_p) {
    message_index = ModemMessages::capabilities;
  }
  else if (widget_p == instance_p->rings_help_button_p) {
    message_index = ModemMessages::rings;
  }
  else if (widget_p == instance_p->class_help_button_p) {
    message_index = ModemMessages::modem_class;
  }
  else if (widget_p == instance_p->dialmode_help_button_p) {
    message_index = ModemMessages::dialmode;
  }
  else {
    write_error("Callback error in ModemPageCB::modem_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

ModemPage::ModemPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(6, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* device_label_p = gtk_label_new(gettext("Serial Device: "));
  GtkWidget* lock_label_p = gtk_label_new(gettext("Lock File: "));
  GtkWidget* capabilities_label_p = gtk_label_new(gettext("Capabilities: "));
  GtkWidget* rings_label_p = gtk_label_new(gettext("Rings (1-9): "));
  GtkWidget* class_label_p = gtk_label_new(gettext("Modem Class: "));
  GtkWidget* dialmode_label_p = gtk_label_new(gettext("Dial Mode: "));

  gtk_label_set_justify(GTK_LABEL(device_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(lock_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(capabilities_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(rings_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(class_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(dialmode_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(device_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(lock_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(capabilities_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(rings_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(class_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(dialmode_label_p), 1, 0.5);

  auto_button_p = gtk_radio_button_new_with_label(0, gettext("Auto"));
  class1_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(auto_button_p),
								gettext("Class 1"));
  class2_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(auto_button_p),
								gettext("Class 2"));
  class20_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(auto_button_p),
								 gettext("Class 2.0"));
  GtkBox* class_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(class_box_p, auto_button_p,
		     false, false, 0);
  gtk_box_pack_start(class_box_p, class1_button_p,
		     false, false, 0);
  gtk_box_pack_start(class_box_p, class2_button_p,
		     false, false, 0);
  gtk_box_pack_start(class_box_p, class20_button_p,
		     false, false, 0);
  GtkWidget* class_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(class_box_alignment_p), GTK_WIDGET(class_box_p));
  GtkWidget* class_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(class_frame_p), class_box_alignment_p);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_button_p), true);

  tone_button_p = gtk_radio_button_new_with_label(0, gettext("Tone"));
  pulse_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tone_button_p),
							       gettext("Pulse"));
  GtkBox* dialmode_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(dialmode_box_p, tone_button_p,
		     false, false, 0);
  gtk_box_pack_start(dialmode_box_p, pulse_button_p,
		     false, false, 0);
  GtkWidget* dialmode_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(dialmode_box_alignment_p), GTK_WIDGET(dialmode_box_p));
  GtkWidget* dialmode_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(dialmode_frame_p), dialmode_box_alignment_p);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tone_button_p), true);

  device_entry_p = gtk_entry_new();
  lock_entry_p = gtk_entry_new();
  capabilities_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(device_entry_p, standard_size * 7, -1);
  gtk_widget_set_size_request(lock_entry_p, standard_size * 7, -1);
  gtk_widget_set_size_request(capabilities_entry_p, standard_size * 7, -1);

  rings_spin_button_p = gtk_spin_button_new_with_range(1, 9, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(rings_spin_button_p), true);
  GtkWidget* rings_spin_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(rings_spin_button_alignment_p), rings_spin_button_p);

  device_help_button_p = gtk_button_new();
  lock_help_button_p = gtk_button_new();
  capabilities_help_button_p = gtk_button_new();
  rings_help_button_p = gtk_button_new();
  class_help_button_p = gtk_button_new();
  dialmode_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(device_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(lock_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(capabilities_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(rings_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(class_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(dialmode_help_button_p), image_p);
  }

  gtk_table_attach(table_p, device_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, device_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, device_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  gtk_table_attach(table_p, lock_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, lock_entry_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, lock_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  gtk_table_attach(table_p, capabilities_label_p,
		   0, 1, 2, 3,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, capabilities_entry_p,
		   1, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, capabilities_help_button_p,
		   2, 3, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  gtk_table_attach(table_p, rings_label_p,
		   0, 1, 3, 4,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, rings_spin_button_alignment_p,
		   1, 2, 3, 4,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, rings_help_button_p,
		   2, 3, 3, 4,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  gtk_table_attach(table_p, class_label_p,
		   0, 1, 4, 5,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, class_frame_p,
		   1, 2, 4, 5,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, class_help_button_p,
		   2, 3, 4, 5,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  gtk_table_attach(table_p, dialmode_label_p,
		   0, 1, 5, 6,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, dialmode_frame_p,
		   1, 2, 5, 6,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/5, standard_size/5);
  gtk_table_attach(table_p, dialmode_help_button_p,
		   2, 3, 5, 6,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/5);

  g_signal_connect(G_OBJECT(device_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);
  g_signal_connect(G_OBJECT(lock_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);
  g_signal_connect(G_OBJECT(capabilities_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);
  g_signal_connect(G_OBJECT(rings_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);
  g_signal_connect(G_OBJECT(class_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);
  g_signal_connect(G_OBJECT(dialmode_help_button_p), "clicked",
		   G_CALLBACK(ModemPageCB::modem_page_button_clicked), this);

  gtk_widget_set_tooltip_text(device_help_button_p,
			      help_messages.get_message(ModemMessages::device).c_str());
  gtk_widget_set_tooltip_text(lock_help_button_p,
			      help_messages.get_message(ModemMessages::lock).c_str());
  gtk_widget_set_tooltip_text(capabilities_help_button_p,
			      help_messages.get_message(ModemMessages::capabilities).c_str());
  gtk_widget_set_tooltip_text(rings_help_button_p,
			      help_messages.get_message(ModemMessages::rings).c_str());
  gtk_widget_set_tooltip_text(class_help_button_p,
			      help_messages.get_message(ModemMessages::modem_class).c_str());
  gtk_widget_set_tooltip_text(dialmode_help_button_p,
			      help_messages.get_message(ModemMessages::dialmode).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string ModemPage::get_device(void) const {
  return gtk_entry_get_text(GTK_ENTRY(device_entry_p));
}

std::string ModemPage::get_lock(void) const {
  return gtk_entry_get_text(GTK_ENTRY(lock_entry_p));
}

std::string ModemPage::get_capabilities(void) const {
  return gtk_entry_get_text(GTK_ENTRY(capabilities_entry_p));
}

std::string ModemPage::get_rings(void) const {
  std::ostringstream strm;
#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE 
  strm << gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rings_spin_button_p));
  return strm.str();
}

std::string ModemPage::get_class(void) const {

  std::string return_val; // an empty string will indicate an "Auto" setting
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(class1_button_p))) return_val = "1";
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(class2_button_p))) return_val = "2";
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(class20_button_p))) return_val = "2.0";
  return return_val;
}

std::string ModemPage::get_dialmode(void) const {
  
  std::string return_val("tone");
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pulse_button_p))) return_val = "pulse";
  return return_val;
}

void ModemPage::set_device(const std::string& device) {
  gtk_entry_set_text(GTK_ENTRY(device_entry_p), device.c_str());
}

void ModemPage::set_lock(const std::string& lock) {
  gtk_entry_set_text(GTK_ENTRY(lock_entry_p), lock.c_str());
}

void ModemPage::set_capabilities(const std::string& capabilities) {
  gtk_entry_set_text(GTK_ENTRY(capabilities_entry_p), capabilities.c_str());
}

void ModemPage::set_rings(const std::string& rings) {

  int val = std::atoi(rings.c_str());
  if (val <= 0) val = 1;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rings_spin_button_p), val);
}

void ModemPage::set_class(const std::string& class_string) {

  if (!class_string.compare("1")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(class1_button_p), true);
  }
  else if (!class_string.compare("2")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(class2_button_p), true);
  }
  else if (!class_string.compare("2.0")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(class20_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_button_p), true); // "Auto" setting
}

void ModemPage::set_dialmode(const std::string& mode_string) {

  std::string temp(utf8_to_lower(mode_string));
  if (!temp.compare("tone")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tone_button_p), true);
  }
  else if (!temp.compare("pulse")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pulse_button_p), true);
  }
}

void ModemPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(device_entry_p), "");
  gtk_entry_set_text(GTK_ENTRY(lock_entry_p), "");
  gtk_entry_set_text(GTK_ENTRY(capabilities_entry_p), "");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rings_spin_button_p), 1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_button_p), true);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tone_button_p), true);
}

void ParmsPageCB::parms_page_button_clicked(GtkWidget* widget_p, void* data) {
  ParmsPage* instance_p = static_cast<ParmsPage*>(data);
  int message_index;
  if (widget_p == instance_p->init_help_button_p) {
    message_index = ParmsMessages::init;
  }
  else if (widget_p == instance_p->reset_help_button_p) {
    message_index = ParmsMessages::reset;
  }
  else if (widget_p == instance_p->parms_help_button_p) {
    message_index = ParmsMessages::extra_parms;
  }
  else {
    write_error("Callback error in ParmsPageCB::parms_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

ParmsPage::ParmsPage(const int standard_size): 
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(3, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* init_label_p = gtk_label_new(gettext("Initialization Params:"));
  GtkWidget* reset_label_p = gtk_label_new(gettext("Reset Params: "));
  GtkWidget* parms_label_p = gtk_label_new(gettext("Other Params: "));

  gtk_label_set_justify(GTK_LABEL(init_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(reset_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(parms_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(init_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(reset_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(parms_label_p), 1, 0.5);

  init_entry_p = gtk_entry_new();
  reset_entry_p = gtk_entry_new();
  parms_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(init_entry_p, standard_size * 7, -1);
  gtk_widget_set_size_request(reset_entry_p, standard_size * 7, -1);
  gtk_widget_set_size_request(parms_entry_p, standard_size * 7, -1);

  init_help_button_p = gtk_button_new();
  reset_help_button_p = gtk_button_new();
  parms_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(init_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(reset_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(parms_help_button_p), image_p);
  }

  gtk_table_attach(table_p, init_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, init_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, init_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  gtk_table_attach(table_p, reset_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, reset_entry_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, reset_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  gtk_table_attach(table_p, parms_label_p,
		   0, 1, 2, 3,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, parms_entry_p,
		   1, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, parms_help_button_p,
		   2, 3, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  g_signal_connect(G_OBJECT(init_help_button_p), "clicked",
		   G_CALLBACK(ParmsPageCB::parms_page_button_clicked), this);
  g_signal_connect(G_OBJECT(reset_help_button_p), "clicked",
		   G_CALLBACK(ParmsPageCB::parms_page_button_clicked), this);
  g_signal_connect(G_OBJECT(parms_help_button_p), "clicked",
		   G_CALLBACK(ParmsPageCB::parms_page_button_clicked), this);

  gtk_widget_set_tooltip_text(init_help_button_p,
			      help_messages.get_message(ParmsMessages::init).c_str());
  gtk_widget_set_tooltip_text(reset_help_button_p,
			      help_messages.get_message(ParmsMessages::reset).c_str());
  gtk_widget_set_tooltip_text(parms_help_button_p,
			      help_messages.get_message(ParmsMessages::extra_parms).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string ParmsPage::get_init(void) const {
  return gtk_entry_get_text(GTK_ENTRY(init_entry_p));
}

std::string ParmsPage::get_reset(void) const {
  return gtk_entry_get_text(GTK_ENTRY(reset_entry_p));
}

std::string ParmsPage::get_parms(void) const {
  return gtk_entry_get_text(GTK_ENTRY(parms_entry_p));
}

void ParmsPage::set_init(const std::string& init) {
  gtk_entry_set_text(GTK_ENTRY(init_entry_p), init.c_str());
}

void ParmsPage::set_reset(const std::string& reset) {
  gtk_entry_set_text(GTK_ENTRY(reset_entry_p), reset.c_str());
}

void ParmsPage::set_parms(const std::string& parms) {
  gtk_entry_set_text(GTK_ENTRY(parms_entry_p), parms.c_str());
}

void ParmsPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(init_entry_p), "");
  gtk_entry_set_text(GTK_ENTRY(reset_entry_p), "");
  gtk_entry_set_text(GTK_ENTRY(parms_entry_p), "");
}

void PrintPageCB::print_page_button_clicked(GtkWidget* widget_p, void* data) {
  PrintPage* instance_p = static_cast<PrintPage*>(data);
  int message_index;
  if (widget_p == instance_p->gtkprint_check_button_h.get()) {
    instance_p->gtkprint_button_toggled_impl();
    return;
  }
  else if (widget_p == instance_p->gtkprint_help_button_h.get()) {
    message_index = PrintMessages::gtkprint;
  }
  else if (widget_p == instance_p->command_help_button_p) {
    message_index = PrintMessages::command;
  }
  else if (widget_p == instance_p->shrink_help_button_p) {
    message_index = PrintMessages::shrink;
  }
  else if (widget_p == instance_p->popup_help_button_p) {
    message_index = PrintMessages::popup;
  }
  else {
    write_error("Callback error in PrintPageCB::print_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

PrintPage::PrintPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(4, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));
  
  GobjHandle<GtkWidget> gtkprint_label_h(gtk_label_new(gettext("Use GTK+ Print System")));
  command_label_p = gtk_label_new(gettext("Print Program: "));
  shrink_label_p = gtk_label_new(gettext("Print Shrink (50-100):"));
  popup_label_p = gtk_label_new(gettext("Popup confirmation dialog\nbefore printing"));
  
  gtk_label_set_justify(GTK_LABEL(gtkprint_label_h.get()), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(command_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(shrink_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(popup_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(gtkprint_label_h.get()), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(command_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(shrink_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(popup_label_p), 1, 0.5);
  
  GobjHandle<GtkWidget> gtkprint_button_alignment_h(gtk_alignment_new(0, 0.5, 0, 1));
  gtkprint_check_button_h = GobjHandle<GtkWidget>(gtk_check_button_new());
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkprint_check_button_h.get()), false); // this is the default
  gtk_container_add(GTK_CONTAINER(gtkprint_button_alignment_h.get()),
		    gtkprint_check_button_h);

  command_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(command_entry_p, standard_size * 7, -1);

  shrink_spin_button_p = gtk_spin_button_new_with_range(50, 100, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(shrink_spin_button_p), true);
  GtkWidget* shrink_spin_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(shrink_spin_button_alignment_p), shrink_spin_button_p);

  GtkWidget* popup_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  popup_check_button_p = gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_check_button_p), true); // this is the default
  gtk_container_add(GTK_CONTAINER(popup_button_alignment_p),
		    popup_check_button_p);

  gtkprint_help_button_h = GobjHandle<GtkWidget>(gtk_button_new());
  command_help_button_p = gtk_button_new();
  shrink_help_button_p = gtk_button_new();
  popup_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(gtkprint_help_button_h.get()), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(command_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(shrink_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(popup_help_button_p), image_p);
  }

  gtk_table_attach(table_p, gtkprint_label_h,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, gtkprint_button_alignment_h,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, gtkprint_help_button_h,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/2);

  gtk_table_attach(table_p, command_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, command_entry_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, command_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  gtk_table_attach(table_p, shrink_label_p,
		   0, 1, 2, 3,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, shrink_spin_button_alignment_p,
		   1, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, shrink_help_button_p,
		   2, 3, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  gtk_table_attach(table_p, popup_label_p,
		   0, 1, 3, 4,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, popup_button_alignment_p,
		   1, 2, 3, 4,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, popup_help_button_p,
		   2, 3, 3, 4,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/2);

  g_signal_connect(G_OBJECT(gtkprint_check_button_h.get()), "clicked",
		   G_CALLBACK(PrintPageCB::print_page_button_clicked), this);
  g_signal_connect(G_OBJECT(gtkprint_help_button_h.get()), "clicked",
		   G_CALLBACK(PrintPageCB::print_page_button_clicked), this);
  g_signal_connect(G_OBJECT(command_help_button_p), "clicked",
		   G_CALLBACK(PrintPageCB::print_page_button_clicked), this);
  g_signal_connect(G_OBJECT(shrink_help_button_p), "clicked",
		   G_CALLBACK(PrintPageCB::print_page_button_clicked), this);
  g_signal_connect(G_OBJECT(popup_help_button_p), "clicked",
		   G_CALLBACK(PrintPageCB::print_page_button_clicked), this);

  gtk_widget_set_tooltip_text(gtkprint_help_button_h,
			      help_messages.get_message(PrintMessages::gtkprint).c_str());
  gtk_widget_set_tooltip_text(command_help_button_p,
			      help_messages.get_message(PrintMessages::command).c_str());
  gtk_widget_set_tooltip_text(shrink_help_button_p,
			      help_messages.get_message(PrintMessages::shrink).c_str());
  gtk_widget_set_tooltip_text(popup_help_button_p,
			      help_messages.get_message(PrintMessages::popup).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string PrintPage::get_gtkprint(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtkprint_check_button_h.get()))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string PrintPage::get_command(void) const {
  return gtk_entry_get_text(GTK_ENTRY(command_entry_p));
}

std::string PrintPage::get_shrink(void) const {
  std::ostringstream strm;
#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE
  strm << gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(shrink_spin_button_p));
  return strm.str();
}

std::string PrintPage::get_popup(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(popup_check_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

void PrintPage::set_gtkprint(const std::string& gtkprint_string) {
  std::string temp(utf8_to_lower(gtkprint_string));
  if (!temp.compare("no")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkprint_check_button_h.get()), false);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkprint_check_button_h.get()), true);
}

void PrintPage::set_command(const std::string& command) {
  gtk_entry_set_text(GTK_ENTRY(command_entry_p), command.c_str());
}

void PrintPage::set_shrink(const std::string& shrink) {

  int val = std::atoi(shrink.c_str());
  if (val < 50) val = 100;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(shrink_spin_button_p), val);
}

void PrintPage::set_popup(const std::string& popup_string) {
  std::string temp(utf8_to_lower(popup_string));
  if (!temp.compare("no")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_check_button_p), false);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_check_button_p), true);
}

void PrintPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(command_entry_p), "");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(shrink_spin_button_p), 100);
}

void PrintPage::gtkprint_button_toggled_impl(void) {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtkprint_check_button_h.get()))) {
    gtk_widget_set_sensitive(GTK_WIDGET(command_label_p), false);
    gtk_editable_set_editable(GTK_EDITABLE(command_entry_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(command_entry_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(shrink_label_p), false);
    gtk_editable_set_editable(GTK_EDITABLE(shrink_spin_button_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(shrink_spin_button_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(popup_label_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(popup_check_button_p), false);
  }
  else {
    gtk_widget_set_sensitive(GTK_WIDGET(command_label_p), true);
    gtk_editable_set_editable(GTK_EDITABLE(command_entry_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(command_entry_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(shrink_label_p), true);
    gtk_editable_set_editable(GTK_EDITABLE(shrink_spin_button_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(shrink_spin_button_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(popup_label_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(popup_check_button_p), true);
  }
}

void ViewPageCB::view_page_button_clicked(GtkWidget*, void* data) {
  ViewPage* instance_p = static_cast<ViewPage*>(data);

  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(ViewMessages::ps_view_command),
				      instance_p->help_messages.get_caption(ViewMessages::ps_view_command)));
}

ViewPage::ViewPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(1, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));
   
  GtkWidget* ps_view_command_label_p = gtk_label_new(gettext("Postscript viewer\nprogram:"));
  gtk_label_set_justify(GTK_LABEL(ps_view_command_label_p), GTK_JUSTIFY_RIGHT);

  ps_view_command_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(ps_view_command_entry_p, standard_size * 7, -1);

  GtkWidget* ps_view_command_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(ps_view_command_help_button_p), image_p);
  }

  gtk_table_attach(table_p, ps_view_command_label_p,
		   0, 1, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, ps_view_command_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, ps_view_command_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  g_signal_connect(G_OBJECT(ps_view_command_help_button_p), "clicked",
		   G_CALLBACK(ViewPageCB::view_page_button_clicked), this);

  gtk_widget_set_tooltip_text(ps_view_command_help_button_p,
			      help_messages.get_message(ViewMessages::ps_view_command).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string ViewPage::get_ps_view_command(void) const {
  return gtk_entry_get_text(GTK_ENTRY(ps_view_command_entry_p));
}

void ViewPage::set_ps_view_command(const std::string& command) {
  gtk_entry_set_text(GTK_ENTRY(ps_view_command_entry_p), command.c_str());
}

void ViewPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(ps_view_command_entry_p), "");
}

void SockPageCB::sock_page_button_clicked(GtkWidget* widget_p, void* data) {
  SockPage* instance_p = static_cast<SockPage*>(data);
  int message_index;
  if (widget_p == instance_p->other_address_button_p) {
    instance_p->other_address_button_toggled_impl();
    return;
  }
  else if (widget_p == instance_p->run_server_help_button_p) {
    message_index = SockMessages::run_server;
  }
  else if (widget_p == instance_p->popup_help_button_p) {
    message_index = SockMessages::popup;
  }
  else if (widget_p == instance_p->port_help_button_p) {
    message_index = SockMessages::port;
  }
  else if (widget_p == instance_p->client_address_help_button_p) {
    message_index = SockMessages::client_address;
  }
  else if (widget_p == instance_p->ip_family_help_button_p) {
    message_index = SockMessages::ip_family;
  }
  else {
    write_error("Callback error in SockPageCB::sock_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

SockPage::SockPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(5, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* run_server_label_p = gtk_label_new(gettext("Run socket server"));
  GtkWidget* popup_label_p = gtk_label_new(gettext("Popup dialog when fax\nreceived from socket"));
  GtkWidget* port_label_p = gtk_label_new(gettext("Port to which faxes to\nbe sent:"));
  GtkWidget* client_address_label_p = gtk_label_new(gettext("Addresses allowed to\nconnect"));
  GtkWidget* ip_family_label_p = gtk_label_new(gettext("IP family for socket\nserver"));

  gtk_label_set_justify(GTK_LABEL(run_server_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(popup_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(port_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(client_address_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(ip_family_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(run_server_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(popup_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(port_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(client_address_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(ip_family_label_p), 1, 0.5);

  GtkWidget* run_server_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  GtkWidget* popup_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  run_server_button_p = gtk_check_button_new();
  popup_button_p = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(run_server_button_alignment_p),
		    run_server_button_p);
  gtk_container_add(GTK_CONTAINER(popup_button_alignment_p),
		    popup_button_p);

  port_spin_button_p = gtk_spin_button_new_with_range(1024, 65535, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(port_spin_button_p), true);
  GtkWidget* port_spin_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(port_spin_button_alignment_p), port_spin_button_p);

  other_addresses_entry_p = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(other_addresses_entry_p), false);
  gtk_widget_set_sensitive(GTK_WIDGET(other_addresses_entry_p), false);

  localhost_button_p = gtk_radio_button_new_with_label(0, gettext("localhost   "));
  other_address_button_p =
    gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(localhost_button_p),
						gettext("other"));

  GtkBox* client_address_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(client_address_box_p, localhost_button_p,
		     false, false, 0);
  gtk_box_pack_start(client_address_box_p, other_address_button_p,
		     false, false, 0);
  GtkWidget* client_address_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(client_address_box_alignment_p),
		    GTK_WIDGET(client_address_box_p));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(localhost_button_p), true);

  GtkBox* client_address_frame_box_p = GTK_BOX(gtk_vbox_new(false, 0));
  gtk_box_pack_start(client_address_frame_box_p, client_address_box_alignment_p,
		     false, false, 0);
  gtk_box_pack_start(client_address_frame_box_p, other_addresses_entry_p,
		     false, false, 0);
  gtk_container_set_border_width(GTK_CONTAINER(client_address_frame_box_p), 4);

  GtkWidget* client_address_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(client_address_frame_p),
		    GTK_WIDGET(client_address_frame_box_p));

  ipv4_button_p = gtk_radio_button_new_with_label(0, gettext("IPv4   "));
  ipv6_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ipv4_button_p),
							      gettext("IPv6"));
  GtkBox* ip_family_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(ip_family_box_p, ipv4_button_p,
		     false, false, 0);
  gtk_box_pack_start(ip_family_box_p, ipv6_button_p,
		     false, false, 0);
  GtkWidget* ip_family_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(ip_family_box_alignment_p), GTK_WIDGET(ip_family_box_p));
  GtkWidget* ip_family_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(ip_family_frame_p), ip_family_box_alignment_p);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ipv4_button_p), true);

  run_server_help_button_p = gtk_button_new();
  popup_help_button_p = gtk_button_new();
  port_help_button_p = gtk_button_new();
  client_address_help_button_p = gtk_button_new();
  ip_family_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(run_server_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(popup_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(port_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(client_address_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(ip_family_help_button_p), image_p);
  }

  gtk_table_attach(table_p, run_server_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, run_server_button_alignment_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, run_server_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/5);

  gtk_table_attach(table_p, popup_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, popup_button_alignment_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, popup_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/5);

  gtk_table_attach(table_p, port_label_p,
		   0, 1, 2, 3,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, port_spin_button_alignment_p,
		   1, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, port_help_button_p,
		   2, 3, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/5);

  gtk_table_attach(table_p, client_address_label_p,
		   0, 1, 3, 4,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, client_address_frame_p,
		   1, 2, 3, 4,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, client_address_help_button_p,
		   2, 3, 3, 4,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/5);

  gtk_table_attach(table_p, ip_family_label_p,
		   0, 1, 4, 5,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, ip_family_frame_p,
		   1, 2, 4, 5,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/5);
  gtk_table_attach(table_p, ip_family_help_button_p,
		   2, 3, 4, 5,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/5);

  g_signal_connect(G_OBJECT(other_address_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);
  g_signal_connect(G_OBJECT(run_server_help_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);
  g_signal_connect(G_OBJECT(popup_help_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);
  g_signal_connect(G_OBJECT(port_help_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);
  g_signal_connect(G_OBJECT(client_address_help_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);
  g_signal_connect(G_OBJECT(ip_family_help_button_p), "clicked",
		   G_CALLBACK(SockPageCB::sock_page_button_clicked), this);

  gtk_widget_set_tooltip_text(run_server_help_button_p,
			      help_messages.get_message(SockMessages::run_server).c_str());
  gtk_widget_set_tooltip_text(popup_help_button_p,
			      help_messages.get_message(SockMessages::popup).c_str());
  gtk_widget_set_tooltip_text(port_help_button_p,
			      help_messages.get_message(SockMessages::port).c_str());
  gtk_widget_set_tooltip_text(client_address_help_button_p,
			      help_messages.get_message(SockMessages::client_address).c_str());
  gtk_widget_set_tooltip_text(ip_family_help_button_p,
			      help_messages.get_message(SockMessages::ip_family).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string SockPage::get_run_server(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(run_server_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string SockPage::get_popup(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(popup_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string SockPage::get_port(void) const {
  std::ostringstream strm;
#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE
  strm << gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(port_spin_button_p));
  return strm.str();
}

std::string SockPage::get_if_other_address(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(other_address_button_p))) {
    return std::string("other");;
  }
  return std::string("localhost");
}

std::string SockPage::get_other_addresses(void) const {
  return gtk_entry_get_text(GTK_ENTRY(other_addresses_entry_p));
}

std::string SockPage::get_ip_family(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ipv6_button_p))) {
    return std::string("ipv6");;
  }
  return std::string("ipv4");
}

void SockPage::set_run_server(const std::string& run_server_string) {

  std::string temp(utf8_to_lower(run_server_string));
  if (!temp.compare("yes")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(run_server_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(run_server_button_p), false);
}

void SockPage::set_popup(const std::string& popup_string) {

  std::string temp(utf8_to_lower(popup_string));
  if (!temp.compare("yes")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), false);
}

void SockPage::set_port(const std::string& port_string) {

  int val = std::atoi(port_string.c_str());
  if (val < 1024) val = 9900;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin_button_p), val);
}

void SockPage::set_if_other_address(const std::string& if_other_address_string) {

  std::string temp(utf8_to_lower(if_other_address_string));
  if (!temp.compare("other")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other_address_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(localhost_button_p), true);
}

void SockPage::set_other_addresses(const std::string& other_addresses_string) {
  gtk_entry_set_text(GTK_ENTRY(other_addresses_entry_p), other_addresses_string.c_str());
}

void SockPage::set_ip_family(const std::string& ip_family_string) {

  std::string temp(utf8_to_lower(ip_family_string));
  if (!temp.compare("ipv6")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ipv6_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ipv4_button_p), true);
}

void SockPage::clear(void) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(run_server_button_p), false);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), false);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other_address_button_p), false);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ipv6_button_p), false);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin_button_p), 9900);
  gtk_entry_set_text(GTK_ENTRY(other_addresses_entry_p), "");
}

void SockPage::other_address_button_toggled_impl(void) {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(other_address_button_p))) {
    gtk_editable_set_editable(GTK_EDITABLE(other_addresses_entry_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(other_addresses_entry_p), true);
  }
  else {
    gtk_editable_set_editable(GTK_EDITABLE(other_addresses_entry_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(other_addresses_entry_p), false);
  }
}

void ReceivePageCB::receive_page_button_clicked(GtkWidget* widget_p, void* data) {
  ReceivePage* instance_p = static_cast<ReceivePage*>(data);
  int message_index;
  if (widget_p == instance_p->exec_button_p) {
    instance_p->exec_button_toggled_impl();
    return;
  }
  else if (widget_p == instance_p->popup_help_button_p) {
    message_index = ReceiveMessages::popup;
  }
  else if (widget_p == instance_p->exec_help_button_p) {
    message_index = ReceiveMessages::exec;
  }
  else {
    write_error("Callback error in ReceivePageCB::receive_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

ReceivePage::ReceivePage(const int standard_size):
                      MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* popup_label_p = gtk_label_new(gettext("Popup dialog when fax\nreceived by modem"));
  GtkWidget* exec_label_p = gtk_label_new(gettext("Execute a program or script\nwhen fax received by modem"));

  gtk_label_set_justify(GTK_LABEL(popup_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(exec_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(popup_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(exec_label_p), 1, 0.5);

  GtkWidget* popup_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  GtkWidget* exec_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  popup_button_p = gtk_check_button_new();
  exec_button_p = gtk_check_button_new();
  
  gtk_container_add(GTK_CONTAINER(popup_button_alignment_p),
		    popup_button_p);
  gtk_container_add(GTK_CONTAINER(exec_button_alignment_p),
		    exec_button_p);

  prog_entry_p = gtk_entry_new();
  gtk_editable_set_editable(GTK_EDITABLE(prog_entry_p), false);
  gtk_widget_set_sensitive(GTK_WIDGET(prog_entry_p), false);
  gtk_widget_set_size_request(prog_entry_p, standard_size * 7, -1);

  GtkBox* exec_frame_box_p = GTK_BOX(gtk_vbox_new(false, 0));
  gtk_box_pack_start(exec_frame_box_p, exec_button_alignment_p,
		     false, false, 0);
  gtk_box_pack_start(exec_frame_box_p, prog_entry_p,
		     false, false, 0);
  gtk_container_set_border_width(GTK_CONTAINER(exec_frame_box_p), 4);

  GtkWidget* exec_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(exec_frame_p),
		    GTK_WIDGET(exec_frame_box_p));

  // set the defaults
  gtk_editable_set_editable(GTK_EDITABLE(prog_entry_p), false);
  gtk_widget_set_sensitive(GTK_WIDGET(prog_entry_p), false);

  popup_help_button_p = gtk_button_new();
  exec_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(popup_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(exec_help_button_p), image_p);
  }

  gtk_table_attach(table_p, popup_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, popup_button_alignment_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, popup_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  gtk_table_attach(table_p, exec_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, exec_frame_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, exec_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  g_signal_connect(G_OBJECT(exec_button_p), "clicked",
		   G_CALLBACK(ReceivePageCB::receive_page_button_clicked), this);
  g_signal_connect(G_OBJECT(popup_help_button_p), "clicked",
		   G_CALLBACK(ReceivePageCB::receive_page_button_clicked), this);
  g_signal_connect(G_OBJECT(exec_help_button_p), "clicked",
		   G_CALLBACK(ReceivePageCB::receive_page_button_clicked), this);

  gtk_widget_set_tooltip_text(popup_help_button_p,
			      help_messages.get_message(ReceiveMessages::popup).c_str());
  gtk_widget_set_tooltip_text(exec_help_button_p,
			      help_messages.get_message(ReceiveMessages::exec).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string ReceivePage::get_popup(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(popup_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string ReceivePage::get_exec(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(exec_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string ReceivePage::get_prog(void) const {
  return gtk_entry_get_text(GTK_ENTRY(prog_entry_p));
}

void ReceivePage::set_popup(const std::string& popup_string) {

  std::string temp(utf8_to_lower(popup_string));
  if (!temp.compare("yes")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), false);
}

void ReceivePage::set_exec(const std::string& exec_string) {

  std::string temp(utf8_to_lower(exec_string));
  if (!temp.compare("yes")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exec_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exec_button_p), false);
}

void ReceivePage::set_prog(const std::string& prog_string) {
  gtk_entry_set_text(GTK_ENTRY(prog_entry_p), prog_string.c_str());
}

void ReceivePage::clear(void) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popup_button_p), false);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exec_button_p), false);

  gtk_entry_set_text(GTK_ENTRY(prog_entry_p), "");
}

void ReceivePage::exec_button_toggled_impl(void) {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(exec_button_p))) {
    gtk_editable_set_editable(GTK_EDITABLE(prog_entry_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(prog_entry_p), true);
  }
  else {
    gtk_editable_set_editable(GTK_EDITABLE(prog_entry_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(prog_entry_p), false);
  }
}

void SendPageCB::send_page_button_clicked(GtkWidget* widget_p, void* data) {
  SendPage* instance_p = static_cast<SendPage*>(data);
  int message_index;
  if (widget_p == instance_p->redial_check_button_p) {
    instance_p->redial_check_button_toggled_impl();
    return;
  }
  else if (widget_p == instance_p->res_help_button_p) {
    message_index = SendMessages::res;
  }
  else if (widget_p == instance_p->header_help_button_p) {
    message_index = SendMessages::header;
  }
  else if (widget_p == instance_p->redial_help_button_p) {
    message_index = SendMessages::redial;
  }
  else if (widget_p == instance_p->dial_prefix_help_button_p) {
    message_index = SendMessages::dial_prefix;
  }
  else {
    write_error("Callback error in SendPageCB::send_page_button_clicked()\n");
    return;
  }
  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(message_index),
				      instance_p->help_messages.get_caption(message_index)));
}

SendPage::SendPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(4, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* res_label_p = gtk_label_new(gettext("Sent Fax Resolution:"));
  GtkWidget* header_label_p = gtk_label_new(gettext("Include destination\nfax number on fax\npage top header line"));
  GtkWidget* redial_label_p = gtk_label_new(gettext("Automatic redial"));
  redial_spin_label_p = gtk_label_new(gettext("Interval (minutes):"));
  GtkWidget* dial_prefix_label_p = gtk_label_new(gettext("Dial Prefix:"));

  gtk_label_set_justify(GTK_LABEL(res_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(header_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(redial_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(redial_spin_label_p), GTK_JUSTIFY_RIGHT);
  gtk_label_set_justify(GTK_LABEL(dial_prefix_label_p), GTK_JUSTIFY_RIGHT);

  gtk_misc_set_alignment(GTK_MISC(res_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(header_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(redial_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(redial_spin_label_p), 1, 0.5);
  gtk_misc_set_alignment(GTK_MISC(dial_prefix_label_p), 1, 0.5);

  standard_button_p = gtk_radio_button_new_with_label(0, gettext("Standard"));
  fine_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(standard_button_p),
							      gettext("Fine"));
  GtkWidget* res_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  GtkBox* res_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(res_box_p, standard_button_p,
		     false, false, 10);
  gtk_box_pack_start(res_box_p, fine_button_p,
		     false, false, 10);
  gtk_container_add(GTK_CONTAINER(res_box_alignment_p), GTK_WIDGET(res_box_p));
  GtkWidget* res_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(res_frame_p), res_box_alignment_p);

  header_check_button_p = gtk_check_button_new();
  GtkWidget* header_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(header_button_alignment_p),
		    header_check_button_p);

  redial_check_button_p = gtk_check_button_new();
  GtkWidget* redial_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  GtkBox* redial_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(redial_box_p, redial_check_button_p,
		     false, false, 10);
  gtk_box_pack_start(redial_box_p, redial_spin_label_p,
		     false, false, 0);
  redial_spin_button_p = gtk_spin_button_new_with_range(5, 1440, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(redial_spin_button_p), true);
  gtk_box_pack_start(redial_box_p, redial_spin_button_p,
		     false, false, 0);

  gtk_container_add(GTK_CONTAINER(redial_box_alignment_p), GTK_WIDGET(redial_box_p));
  GtkWidget* redial_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(redial_frame_p), redial_box_alignment_p);

  // set the defaults
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fine_button_p), true);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(header_check_button_p), true);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(redial_spin_button_p), 15);
  gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_label_p), false);
  gtk_editable_set_editable(GTK_EDITABLE(redial_spin_button_p), false);
  gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_button_p), false);

  dial_prefix_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(dial_prefix_entry_p, standard_size * 7, -1);

  res_help_button_p = gtk_button_new();
  header_help_button_p = gtk_button_new();
  redial_help_button_p = gtk_button_new();
  dial_prefix_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(res_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(header_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(redial_help_button_p), image_p);
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(dial_prefix_help_button_p), image_p);
  }

  gtk_table_attach(table_p, res_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, res_frame_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, res_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  
  gtk_table_attach(table_p, header_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, header_button_alignment_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/4, standard_size/2);
  gtk_table_attach(table_p, header_help_button_p,
		   2, 3, 1, 2,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/4, standard_size/2);

  gtk_table_attach(table_p, redial_label_p,
		   0, 1, 2, 3,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, redial_frame_p,
		   1, 2, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, redial_help_button_p,
		   2, 3, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);

  gtk_table_attach(table_p, dial_prefix_label_p,
		   0, 1, 3, 4,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, dial_prefix_entry_p,
		   1, 2, 3, 4,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/2);
  gtk_table_attach(table_p, dial_prefix_help_button_p,
		   2, 3, 3, 4,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size/2);
  
  g_signal_connect(G_OBJECT(redial_check_button_p), "clicked",
		   G_CALLBACK(SendPageCB::send_page_button_clicked), this);
  g_signal_connect(G_OBJECT(res_help_button_p), "clicked",
		   G_CALLBACK(SendPageCB::send_page_button_clicked), this);
  g_signal_connect(G_OBJECT(header_help_button_p), "clicked",
		   G_CALLBACK(SendPageCB::send_page_button_clicked), this);
  g_signal_connect(G_OBJECT(redial_help_button_p), "clicked",
		   G_CALLBACK(SendPageCB::send_page_button_clicked), this);
  g_signal_connect(G_OBJECT(dial_prefix_help_button_p), "clicked",
		   G_CALLBACK(SendPageCB::send_page_button_clicked), this);

  gtk_widget_set_tooltip_text(res_help_button_p,
			      help_messages.get_message(SendMessages::res).c_str());
  gtk_widget_set_tooltip_text(header_help_button_p,
			      help_messages.get_message(SendMessages::header).c_str());
  gtk_widget_set_tooltip_text(redial_help_button_p,
			      help_messages.get_message(SendMessages::redial).c_str());
  gtk_widget_set_tooltip_text(dial_prefix_help_button_p,
			      help_messages.get_message(SendMessages::dial_prefix).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string SendPage::get_res(void) const {
  
  std::string return_val("fine");
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(standard_button_p))) return_val = "standard";
  return return_val;
}

std::string SendPage::get_addr_in_header(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(header_check_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string SendPage::get_redial(void) const {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(redial_check_button_p))) {
    return std::string("Yes");
  }
  return std::string("No");
}

std::string SendPage::get_redial_interval(void) const {

  std::ostringstream strm;
#ifdef HAVE_STREAM_IMBUE
  strm.imbue(std::locale::classic());
#endif // HAVE_STREAM_IMBUE
  strm << gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(redial_spin_button_p));
  return strm.str();
}

std::string SendPage::get_dial_prefix(void) const {
  return gtk_entry_get_text(GTK_ENTRY(dial_prefix_entry_p));
}

void SendPage::set_res(const std::string& res_string) {

  std::string temp(utf8_to_lower(res_string));
  if (!temp.compare("fine")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fine_button_p), true);
  }
  else if (!temp.compare("standard")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(standard_button_p), true);
  }
}

void SendPage::set_addr_in_header(const std::string& header_string) {

  std::string temp(utf8_to_lower(header_string));
  if (!temp.compare("no")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(header_check_button_p), false);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(header_check_button_p), true);
}

void SendPage::set_redial(const std::string& redial) {

  std::string temp(utf8_to_lower(redial));
  if (!temp.compare("yes")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(redial_check_button_p), true);
  }
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(redial_check_button_p), false);
}

void SendPage::set_redial_interval(const std::string& interval) {

  int val = std::atoi(interval.c_str());
  if (val < 5) val = 15;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(redial_spin_button_p), val);
}

void SendPage::set_dial_prefix(const std::string& prefix) {
  gtk_entry_set_text(GTK_ENTRY(dial_prefix_entry_p), prefix.c_str());
}

void SendPage::clear(void) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fine_button_p), true);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(header_check_button_p), true);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(redial_check_button_p), false);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(redial_spin_button_p), 15);
  gtk_entry_set_text(GTK_ENTRY(dial_prefix_entry_p), "");
  gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_label_p), false);
  gtk_editable_set_editable(GTK_EDITABLE(redial_spin_button_p), false);
  gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_button_p), false);
}

void SendPage::redial_check_button_toggled_impl(void) {

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(redial_check_button_p))) {
    gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_label_p), true);
    gtk_editable_set_editable(GTK_EDITABLE(redial_spin_button_p), true);
    gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_button_p), true);
  }
  else {
    gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_label_p), false);
    gtk_editable_set_editable(GTK_EDITABLE(redial_spin_button_p), false);
    gtk_widget_set_sensitive(GTK_WIDGET(redial_spin_button_p), false);
  }
}

void LoggingPageCB::logging_page_button_clicked(GtkWidget*, void* data) {
  LoggingPage* instance_p = static_cast<LoggingPage*>(data);

  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(LoggingMessages::logfile),
				      instance_p->help_messages.get_caption(LoggingMessages::logfile)));
}

LoggingPage::LoggingPage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(1, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* logfile_label_p = gtk_label_new(gettext("Log File: "));
  gtk_label_set_justify(GTK_LABEL(logfile_label_p), GTK_JUSTIFY_RIGHT);

  logfile_entry_p = gtk_entry_new();
  gtk_widget_set_size_request(logfile_entry_p, standard_size * 7, -1);

  GtkWidget* logfile_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(logfile_help_button_p), image_p);
  }
  
  gtk_table_attach(table_p, logfile_label_p,
		   0, 1, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, logfile_entry_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, logfile_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);

  g_signal_connect(G_OBJECT(logfile_help_button_p), "clicked",
		   G_CALLBACK(LoggingPageCB::logging_page_button_clicked), this);

  gtk_widget_set_tooltip_text(logfile_help_button_p,
			      help_messages.get_message(LoggingMessages::logfile).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string LoggingPage::get_logfile(void) const {
  return gtk_entry_get_text(GTK_ENTRY(logfile_entry_p));
}

void LoggingPage::set_logfile(const std::string& logfile) {
  gtk_entry_set_text(GTK_ENTRY(logfile_entry_p), logfile.c_str());
}

void LoggingPage::clear(void) {
  gtk_entry_set_text(GTK_ENTRY(logfile_entry_p), "");
}

void PagePageCB::page_page_button_clicked(GtkWidget* widget_p, void* data) {
  PagePage* instance_p = static_cast<PagePage*>(data);

  instance_p->show_help_sig(std::pair<std::string, std::string>(
				      instance_p->help_messages.get_message(PageMessages::page),
				      instance_p->help_messages.get_caption(PageMessages::page)));
}

PagePage::PagePage(const int standard_size):
                       MainWidgetBase(gtk_alignment_new(0.5, 0.5, 1, 0)) {

  GtkTable* table_p = GTK_TABLE(gtk_table_new(1, 3, false));
  gtk_container_add(GTK_CONTAINER(get_main_widget()), GTK_WIDGET(table_p));

  GtkWidget* page_label_p = gtk_label_new(gettext("Page Size: "));
  gtk_label_set_justify(GTK_LABEL(page_label_p), GTK_JUSTIFY_RIGHT);

  a4_button_p = gtk_radio_button_new_with_label(0, "A4");
  letter_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(a4_button_p),
								"Letter");
  legal_button_p = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(a4_button_p),
							       "Legal");

  GtkWidget* page_box_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  GtkBox* page_box_p = GTK_BOX(gtk_hbox_new(false, 2));
  gtk_box_pack_start(page_box_p, a4_button_p,
		     false, false, 10);
  gtk_box_pack_start(page_box_p, letter_button_p,
		     false, false, 10);
  gtk_box_pack_start(page_box_p, legal_button_p,
		     false, false, 10);
  
  gtk_container_add(GTK_CONTAINER(page_box_alignment_p), GTK_WIDGET(page_box_p));
  GtkWidget* page_frame_p = gtk_frame_new(0);
  gtk_container_add(GTK_CONTAINER(page_frame_p), page_box_alignment_p);

  // set the defaults
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a4_button_p), true);

  page_help_button_p = gtk_button_new();
  GtkWidget* image_p;
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(help_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(page_help_button_p), image_p);
  }

  gtk_table_attach(table_p, page_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, page_frame_p,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size);
  gtk_table_attach(table_p, page_help_button_p,
		   2, 3, 0, 1,
		   GTK_SHRINK, GTK_SHRINK,
		   standard_size/3, standard_size);
  
  g_signal_connect(G_OBJECT(page_help_button_p), "clicked",
		   G_CALLBACK(PagePageCB::page_page_button_clicked), this);

  gtk_widget_set_tooltip_text(page_help_button_p,
			      help_messages.get_message(PageMessages::page).c_str());

  gtk_container_set_border_width(GTK_CONTAINER(get_main_widget()), standard_size/3);
}

std::string PagePage::get_page(void) const {
  
  std::string return_val("a4");
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(letter_button_p))) return_val = "letter";
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(legal_button_p))) return_val = "legal";
  return return_val;
}

void PagePage::set_page(const std::string& page_string) {

  std::string temp(utf8_to_lower(page_string));
  if (!temp.compare("a4")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a4_button_p), true);
  }
  else if (!temp.compare("letter")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(letter_button_p), true);
  }
  else if (!temp.compare("legal")) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legal_button_p), true);
  }
}

void PagePage::clear(void) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a4_button_p), true);
}

void SettingsDialogCB::settings_dialog_button_clicked(GtkWidget* widget_p, void* data) {
  SettingsDialog* instance_p = static_cast<SettingsDialog*>(data);
  
  if (widget_p == instance_p->ok_button_p) {
    if (instance_p->write_config()) {
      gtk_widget_hide(GTK_WIDGET(instance_p->get_win()));
      instance_p->accepted(configure_prog(true));
      instance_p->close();
    }
  }
  else if (widget_p == instance_p->cancel_button_p) {
    instance_p->close();
  }
  else if (widget_p == instance_p->reset_button_p) {
    instance_p->get_reset_settings_prompt();
  }
  else {
    write_error("Callback error in SettingsDialogCB::settings_dialog_button_clicked()\n");
    instance_p->close();
  }
}

SettingsDialog::SettingsDialog(const int size, GtkWindow* parent_p, bool skip_old_settings):
                                           // skip_old_settings has a default value of false
                                           WinBase(gettext("efax-gtk: settings"),
						   prog_config.window_icon_h,
						   true, parent_p),
					   standard_size(size),
					   is_home_config(false),
					   identity_page(standard_size),
					   modem_page(standard_size),
					   parms_page(standard_size),
					   print_page(standard_size),
					   view_page(standard_size),
					   sock_page(standard_size),
					   receive_page(standard_size),
					   send_page(standard_size),
					   logging_page(standard_size),
					   page_page(standard_size) {

  ++dialog_count;

  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  reset_button_p = 0;
   
  GtkWidget* label_p = gtk_label_new(0);
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkTable* window_table_p = GTK_TABLE(gtk_table_new(2, 2, false));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box_p), standard_size/2);
   
  // we set skip_old_settings as true in MainWindow::MainWindow(), where no config file has
  // been found, to prevent that fact being reported twice
  if (!skip_old_settings) read_config();

  if (!is_home_config) { // if skip_old_settings is true, then is_home_config is always false

    gtk_label_set_line_wrap(GTK_LABEL(label_p), true);
    std::string label_text(gettext("Note: pressing the OK button will save the "
				     "settings in file"));
    label_text += " ~/." RC_FILE;
    gtk_label_set_text(GTK_LABEL(label_p), label_text.c_str());
  }

  else {
    reset_button_p = gtk_button_new_with_label(gettext("Reset"));
    g_signal_connect(G_OBJECT(reset_button_p), "clicked",
		     G_CALLBACK(SettingsDialogCB::settings_dialog_button_clicked), this);
#if GTK_CHECK_VERSION(2,20,0)
    gtk_widget_set_can_default(reset_button_p, true);
#else
    GTK_WIDGET_SET_FLAGS(reset_button_p, GTK_CAN_DEFAULT);
#endif
    gtk_container_add(GTK_CONTAINER(button_box_p), reset_button_p);
  }
    
  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);

  GtkNotebook* notebook_p = GTK_NOTEBOOK(gtk_notebook_new());

  gtk_table_attach(window_table_p, GTK_WIDGET(notebook_p),
		   0, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/2, standard_size/4);
  gtk_table_attach(window_table_p, label_p,
		   0, 1, 1, 2,
		   GTK_EXPAND, GTK_SHRINK,
		   standard_size/2, standard_size/4);
  gtk_table_attach(window_table_p, button_box_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  gtk_notebook_set_tab_pos(notebook_p, GTK_POS_TOP);
  gtk_notebook_set_scrollable(notebook_p, true);

  GtkWidget* tab_label_p;
  tab_label_p = gtk_label_new(gettext("Identity"));
  gtk_notebook_append_page(notebook_p, identity_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Modem"));
  gtk_notebook_append_page(notebook_p, modem_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Params"));
  gtk_notebook_append_page(notebook_p, parms_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Print"));
  gtk_notebook_append_page(notebook_p, print_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("View"));
  gtk_notebook_append_page(notebook_p, view_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Socket"));
  gtk_notebook_append_page(notebook_p, sock_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Receive"));
  gtk_notebook_append_page(notebook_p, receive_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Send"));
  gtk_notebook_append_page(notebook_p, send_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Logging"));
  gtk_notebook_append_page(notebook_p, logging_page.get_main_widget(), tab_label_p);

  tab_label_p = gtk_label_new(gettext("Page"));
  gtk_notebook_append_page(notebook_p, page_page.get_main_widget(), tab_label_p);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(SettingsDialogCB::settings_dialog_button_clicked), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(SettingsDialogCB::settings_dialog_button_clicked), this);

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_can_default(ok_button_p, true);
  gtk_widget_set_can_default(cancel_button_p, true);
#else
  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);
#endif

  identity_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  modem_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  parms_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  print_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  view_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  sock_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  receive_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  send_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  logging_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));
  page_page.show_help_sig.connect(Callback::make(*this, &SettingsDialog::show_help));

  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/2);
  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(window_table_p));
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_widget_set_size_request(GTK_WIDGET(notebook_p), standard_size * 25, standard_size * 12);

  gtk_widget_grab_focus(cancel_button_p);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

bool SettingsDialog::write_config(void) {
  // returns false if any of the entered parameters is invalid, otherwise it returns true

  bool return_val = true;
  std::ifstream filein;

  if (!rcfile.empty()) {
    filein.open(rcfile.c_str(), std::ios::in);
    if (!filein) {
      std::string message("Can't open file ");
      message += rcfile;
      message += "\n\n";
      write_error(message.c_str());
    }
  }

  std::vector<std::string> file_list;
  std::string file_read;
  
  std::string temp;

  std::string name_line("NAME: ");
  temp = identity_page.get_name();
  strip(temp);
  if (!is_ascii(temp)) {
    return_val = false;
    InfoDialog dialog(gettext("Invalid user name specified in \"Identity\" tab - "
			      "it must be in plain ASCII characters.  If this "
			      "is a problem, leave it blank as the fax station "
			      "number will always be given on the top header"),
		      gettext("Config Error"),
		      GTK_MESSAGE_ERROR, get_win());
    dialog.exec();
  }
  else name_line += temp;
  
  std::string number_line("NUMBER: ");
  temp =  identity_page.get_number();
  strip(temp);
  number_line += temp;
  
  std::string device_line;
  temp = modem_page.get_device();
  strip(temp);
  if (temp.empty()) device_line = "#DEVICE: modem";
  else {
    device_line = "DEVICE: ";
    device_line += temp;
  }
  
  std::string lock_line;
  temp = modem_page.get_lock();
  strip(temp);
  if (temp.empty()) lock_line = "#LOCK: /var/lock";
  else {
    lock_line = "LOCK: ";
    lock_line += temp;
  }
  
  std::string class_line;
  temp = modem_page.get_class();
  strip(temp);
  if (temp.empty()) class_line = "#CLASS: 2";
  else {
    if (temp.compare("1") && temp.compare("2") && temp.compare("2.0")) {
      class_line = "#CLASS: 2";
      return_val = false;
      InfoDialog dialog(gettext("Invalid modem class specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {    
      class_line = "CLASS: ";
      class_line += temp;
    }
  }
  
  std::string page_line;
  temp = page_page.get_page();
  strip(temp);
  if (temp.empty()) page_line = "#PAGE: a4";
  else {
    if (temp.compare("a4") && temp.compare("letter") && temp.compare("legal")) {
      page_line = "#PAGE: a4";
      return_val = false;
      InfoDialog dialog(gettext("Invalid page size specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      page_line = "PAGE: ";
      page_line += temp;
    }
  }
  
  std::string res_line;
  temp = send_page.get_res();
  strip(temp);
  if (temp.empty()) res_line = "#RES: fine";
  else {
    if (temp.compare("fine") && temp.compare("standard")) {
      res_line = "#RES: fine";
      return_val = false;
      InfoDialog dialog(gettext("Invalid sent fax resolution specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      res_line = "RES: ";
      res_line += temp;
    }
  }
  
  std::string dial_prefix_line;
  temp = send_page.get_dial_prefix();
  strip(temp);
  if (temp.empty()) dial_prefix_line = "#DIAL_PREFIX: ";
  else {
    dial_prefix_line = "DIAL_PREFIX: ";
    dial_prefix_line += temp;
  }
  
  std::string rings_line;
  temp = modem_page.get_rings();
  strip(temp);
  if (temp.empty()) rings_line = "#RINGS: 1";
  else {
    if (temp.size() > 1 || temp[0] < '1' ||temp[0] > '9') {
      rings_line = "#RINGS: 1";
      return_val = false;
      InfoDialog dialog(gettext("Invalid rings number specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      rings_line = "RINGS: ";
      rings_line += temp;
    }
  }
  
  std::string dialmode_line;
  temp = modem_page.get_dialmode();
  strip(temp);
  if (temp.empty()) dialmode_line = "#DIALMODE: tone";
  else {
    if (temp.compare("tone") && temp.compare("pulse")) {
      dialmode_line = "#DIALMODE: tone";
      return_val = false;
      InfoDialog dialog(gettext("Invalid dial mode specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      dialmode_line = "DIALMODE: ";
      dialmode_line += temp;
    }
  }
  
  std::string init_line;
  temp = parms_page.get_init();
  strip(temp);
  if (temp.empty()) init_line = "#INIT: Z &FE&D2S7=120 &C0 M1L0";
  else {
    init_line = "INIT: ";
    init_line += temp;
  }
  
  std::string reset_line;
  temp = parms_page.get_reset();
  strip(temp);
  if (temp.empty()) reset_line = "#RESET: Z";
  else {
    reset_line = "RESET: ";
    reset_line += temp;
  }
  
  std::string capabilities_line;
  temp = modem_page.get_capabilities();
  strip(temp);
  if (temp.empty()) capabilities_line = "#CAPABILITIES: 1,5,0,2,0,0,0,0";
  else {
    capabilities_line = "CAPABILITIES: ";
    capabilities_line += temp;
  }
  
  std::string extra_parms_line;
  temp = parms_page.get_parms();
  strip(temp);
  if (temp.empty()) extra_parms_line = "#PARMS: ";
  else {
    extra_parms_line = "PARMS: ";
    extra_parms_line += temp;
  }
  
  std::string print_cmd_line;
  temp = print_page.get_command();
  strip(temp);
  if (temp.empty()) print_cmd_line = "#PRINT_CMD: lpr";
  else {
    print_cmd_line = "PRINT_CMD: ";
    print_cmd_line += temp;
  }
  
  std::string gtkprint_line;
  temp = print_page.get_gtkprint();
  strip(temp);
  if (temp.empty()) gtkprint_line = "#USE_GTKPRINT: No";
  else {
    gtkprint_line = "USE_GTKPRINT: ";
    gtkprint_line += temp;
  }

  std::string print_shrink_line;
  temp = print_page.get_shrink();
  strip(temp);
  if (temp.empty()) print_shrink_line = "#PRINT_SHRINK: 100";
  else {
    if (std::atoi(temp.c_str()) < 50 || std::atoi(temp.c_str()) > 100) {
      print_shrink_line = "#PRINT_SHRINK: 100";
      return_val = false;
      InfoDialog dialog(gettext("Invalid print shrink parameter specified"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      print_shrink_line = "PRINT_SHRINK: ";
      print_shrink_line += temp;
    }
  }
  
  std::string print_popup_line;
  temp = print_page.get_popup();
  strip(temp);
  if (temp.empty()) print_popup_line = "#PRINT_POPUP: Yes";
  else {
    print_popup_line = "PRINT_POPUP: ";
    print_popup_line += temp;
  }

  std::string ps_view_cmd_line;
  temp = view_page.get_ps_view_command();
  strip(temp);
  if (temp.empty()) ps_view_cmd_line = "#PS_VIEWER: gv";
  else {
    ps_view_cmd_line = "PS_VIEWER: ";
    ps_view_cmd_line += temp;
  }
  
  std::string run_server_line;
  temp = sock_page.get_run_server();
  strip(temp);
  if (temp.empty()) run_server_line = "#SOCK_SERVER: No";
  else {
    run_server_line = "SOCK_SERVER: ";
    run_server_line += temp;
  }

  std::string sock_popup_line;
  temp = sock_page.get_popup();
  strip(temp);
  if (temp.empty()) sock_popup_line = "#SOCK_POPUP: No";
  else {
    sock_popup_line = "SOCK_POPUP: ";
    sock_popup_line += temp;
  }

  std::string sock_port_line;
  temp = sock_page.get_port();
  strip(temp);
  if (temp.empty()) sock_port_line = "#SOCK_SERVER_PORT: 9900";
  else {
    if (std::atoi(temp.c_str()) < 1024 || std::atoi(temp.c_str()) > 65535) {
      sock_port_line = "#SOCK_SERVER_PORT: 9900";
      return_val = false;
      InfoDialog dialog(gettext("Invalid socket port number specified. "
				"It must be between 1024 and 65535"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      sock_port_line = "SOCK_SERVER_PORT: ";
      sock_port_line += temp;
    }
  }

  std::string sock_client_address_line;
  temp = sock_page.get_if_other_address();
  strip(temp);
  if (temp.empty()) sock_client_address_line = "#SOCK_CLIENT_ADDRESS: localhost";
  else {
    sock_client_address_line = "SOCK_CLIENT_ADDRESS: ";
    sock_client_address_line += temp;
  }

  std::string sock_other_addresses_line;
  temp = sock_page.get_other_addresses();
  strip(temp);
  if (temp.empty()) sock_other_addresses_line = "#SOCK_OTHER_ADDRESSES:";
  else {
    sock_other_addresses_line = "SOCK_OTHER_ADDRESSES: ";
    sock_other_addresses_line += temp;
  }
  
  std::string ip_family_line;
  temp = sock_page.get_ip_family();
  strip(temp);
  if (temp.empty()) sock_other_addresses_line = "#IP_FAMILY:";
  else {
    ip_family_line = "IP_FAMILY: ";
    ip_family_line += temp;
  }
  
  std::string fax_received_popup_line;
  temp = receive_page.get_popup();
  strip(temp);
  if (temp.empty()) fax_received_popup_line = "#FAX_RECEIVED_POPUP: No";
  else {
    fax_received_popup_line = "FAX_RECEIVED_POPUP: ";
    fax_received_popup_line += temp;
  }

  std::string fax_received_exec_line;
  temp = receive_page.get_exec();
  strip(temp);
  if (temp.empty()) fax_received_exec_line = "#FAX_RECEIVED_EXEC: No";
  else {
    fax_received_exec_line = "FAX_RECEIVED_EXEC: ";
    fax_received_exec_line += temp;
  }

  std::string fax_received_prog_line;
  temp = receive_page.get_prog();
  strip(temp);
  if (temp.empty()) fax_received_prog_line = "#FAX_RECEIVED_PROG: ";
  else {
    fax_received_prog_line = "FAX_RECEIVED_PROG: ";
    fax_received_prog_line += temp;
  }
  
  std::string addr_in_header_line;
  temp = send_page.get_addr_in_header();
  strip(temp);
  if (temp.empty()) addr_in_header_line = "#ADDR_IN_HEADER: Yes";
  else {
    addr_in_header_line = "ADDR_IN_HEADER: ";
    addr_in_header_line += temp;
  }

  std::string redial_line;
  temp = send_page.get_redial();
  strip(temp);
  if (temp.empty()) redial_line = "#REDIAL: No";
  else {
    redial_line = "REDIAL: ";
    redial_line += temp;
  }

  std::string redial_interval_line;
  temp = send_page.get_redial_interval();
  strip(temp);
  if (temp.empty()) redial_interval_line = "#REDIAL_INTERVAL: 15";
  else {
    if (std::atoi(temp.c_str()) < 5 || std::atoi(temp.c_str()) > 1440) {
      redial_interval_line = "#REDIAL_INTERVAL: 15";
      return_val = false;
      InfoDialog dialog(gettext("Invalid redial interval specified. "
				"It must be between 5 and 1440 minutes"),
			gettext("Config Error"),
			GTK_MESSAGE_ERROR, get_win());
      dialog.exec();
    }
    else {
      redial_interval_line = "REDIAL_INTERVAL: ";
      redial_interval_line += temp;
    }
  }

  std::string logfile_line;
  temp = logging_page.get_logfile();
  strip(temp);
  if (temp.empty()) logfile_line = "#LOG_FILE: logfile";
  else {
    logfile_line = "LOG_FILE: ";
    logfile_line += temp;
  }
  
  if (return_val) { // no errors -- write out the configuration file
    
    bool found_name = false;
    bool found_number = false;
    bool found_device = false;
    bool found_lock = false;
    bool found_class = false;
    bool found_page = false;
    bool found_res = false;
    bool found_dial_prefix = false;
    bool found_rings = false;
    bool found_dialmode = false;
    bool found_init = false;
    bool found_reset = false;
    bool found_capabilities = false;
    bool found_parms = false;
    bool found_print_cmd = false;
    bool found_gtkprint = false;
    bool found_print_shrink = false;
    bool found_print_popup = false;
    bool found_ps_view_cmd = false;
    bool found_run_server = false;
    bool found_sock_popup = false;
    bool found_sock_port = false;
    bool found_sock_client_address = false;
    bool found_sock_other_addresses = false;
    bool found_ip_family = false;
    bool found_fax_received_popup = false;
    bool found_fax_received_exec = false;
    bool found_fax_received_prog = false;
    bool found_addr_in_header = false;
    bool found_redial = false;
    bool found_redial_interval = false;
    bool found_log_file = false;
    
    const char terminating_line[] = "## end of " RC_FILE " ##";
    
    while (std::getline(filein, file_read)) {

      try {
	// look for "NAME:"
	if (find_prog_parm("NAME:", file_read) || find_prog_parm("#NAME:", file_read)) {
	  if (!found_name) {
	    found_name = true;
	    file_list.push_back(Utf8::locale_from_utf8(name_line));
	  }
	}
      
	// look for "NUMBER:"
	else if (find_prog_parm("NUMBER:", file_read) || find_prog_parm("#NUMBER:", file_read)) {
	  if (!found_number) {
	    found_number = true;
	    file_list.push_back(number_line);
	  }
	}
      
	// look for "DEVICE:"
	else if (find_prog_parm("DEVICE:", file_read) || find_prog_parm("#DEVICE:", file_read)) {
	  if (!found_device) {
	    found_device = true;
	    file_list.push_back(Utf8::locale_from_utf8(device_line));
	  }
	}
      
	// look for "LOCK:"
	else if (find_prog_parm("LOCK:", file_read)  || find_prog_parm("#LOCK:", file_read)) {
	  if (!found_lock) {
	    found_lock = true;
	    file_list.push_back(Utf8::filename_from_utf8(lock_line));
	  }
	}
      
	// look for "CLASS:"
	else if (find_prog_parm("CLASS:", file_read) || find_prog_parm("#CLASS:", file_read))  {
	  if (!found_class) {
	    found_class = true;
	    file_list.push_back(class_line);
	  }
	}
      
	// look for "PAGE:"
	else if (find_prog_parm("PAGE:", file_read) || find_prog_parm("#PAGE:", file_read))  {
	  if (!found_page) {
	    found_page = true;
	    file_list.push_back(page_line);
	  }
	}
      
	// look for "RES:"
	else if (find_prog_parm("RES:", file_read) || find_prog_parm("#RES:", file_read))  {
	  if (!found_res) {
	    found_res = true;
	    file_list.push_back(res_line);
	  }
	}
      
	// look for "DIAL_PREFIX:"
	else if (find_prog_parm("DIAL_PREFIX:", file_read) || find_prog_parm("#DIAL_PREFIX:", file_read))  {
	  if (!found_dial_prefix) {
	    found_dial_prefix = true;
	    file_list.push_back(Utf8::locale_from_utf8(dial_prefix_line));
	  }
	}

	// look for "RINGS:"
	else if (find_prog_parm("RINGS:", file_read) || find_prog_parm("#RINGS:", file_read)) {
	  if (!found_rings) {
	    found_rings = true;
	    file_list.push_back(rings_line);
	  }
	}
      
	// look for "DIALMODE:"
	else if (find_prog_parm("DIALMODE:", file_read) || find_prog_parm("#DIALMODE:", file_read)) {
	  if (!found_dialmode) {
	    found_dialmode = true;
	    file_list.push_back(dialmode_line);
	  }
	}
      
	// look for "INIT:"
	else if (find_prog_parm("INIT:", file_read) || find_prog_parm("#INIT:", file_read)) {
	  if (!found_init) {
	    found_init = true;
	    file_list.push_back(init_line);
	  }
	}
      
	// look for "RESET:"
	else if (find_prog_parm("RESET:", file_read) || find_prog_parm("#RESET:", file_read)) {
	  if (!found_reset) {
	    found_reset = true;
	    file_list.push_back(reset_line);
	  }
	}
      
	// look for "CAPABILITIES:"
	else if (find_prog_parm("CAPABILITIES:", file_read) || find_prog_parm("#CAPABILITIES:", file_read)) {
	  if (!found_capabilities) {
	    found_capabilities = true;
	    file_list.push_back(capabilities_line);
	  }
	}
      
	// look for "PARMS:"
	else if (find_prog_parm("PARMS:", file_read) || find_prog_parm("#PARMS:", file_read)) {
	  if (!found_parms) {
	    found_parms = true;
	    file_list.push_back(extra_parms_line);
	  }
	}
      
	// look for "PRINT_CMD:"
	else if (find_prog_parm("PRINT_CMD:", file_read) || find_prog_parm("#PRINT_CMD:", file_read)) {
	  if (!found_print_cmd) {
	    found_print_cmd = true;
	    file_list.push_back(Utf8::filename_from_utf8(print_cmd_line));
	  }
	}
      
	// look for "USE_GTKPRINT:"
	else if (find_prog_parm("USE_GTKPRINT:", file_read) || find_prog_parm("#USE_GTKPRINT:", file_read)) {
	  if (!found_gtkprint) {
	    found_gtkprint = true;
	    file_list.push_back(gtkprint_line);
	  }
	}
      
	// look for "PRINT_SHRINK:"
	else if (find_prog_parm("PRINT_SHRINK:", file_read) || find_prog_parm("#PRINT_SHRINK:", file_read)) {
	  if (!found_print_shrink) {
	    found_print_shrink = true;
	    file_list.push_back(print_shrink_line);
	  }
	}
      
	// look for "PRINT_POPUP:"
	else if (find_prog_parm("PRINT_POPUP:", file_read) || find_prog_parm("#PRINT_POPUP:", file_read)) {
	  if (!found_print_popup) {
	    found_print_popup = true;
	    file_list.push_back(print_popup_line);
	  }
	}
      
	// look for "PS_VIEWER:"
	else if (find_prog_parm("PS_VIEWER:", file_read) || find_prog_parm("#PS_VIEWER:", file_read)) {
	  if (!found_ps_view_cmd) {
	    found_ps_view_cmd = true;
	    file_list.push_back(Utf8::filename_from_utf8(ps_view_cmd_line));
	  }
	}
      
	// look for "SOCK_SERVER:"
	else if (find_prog_parm("SOCK_SERVER:", file_read) || find_prog_parm("#SOCK_SERVER:", file_read)) {
	  if (!found_run_server) {
	    found_run_server = true;
	    file_list.push_back(run_server_line);
	  }
	}
      
	// look for "SOCK_POPUP:"
	else if (find_prog_parm("SOCK_POPUP:", file_read) || find_prog_parm("#SOCK_POPUP:", file_read)) {
	  if (!found_sock_popup) {
	    found_sock_popup = true;
	    file_list.push_back(sock_popup_line);
	  }
	}
      
	// look for "SOCK_SERVER_PORT:"
	else if (find_prog_parm("SOCK_SERVER_PORT:", file_read) || find_prog_parm("#SOCK_SERVER_PORT:", file_read)) {
	  if (!found_sock_port) {
	    found_sock_port = true;
	    file_list.push_back(sock_port_line);
	  }
	}
      
	// look for "SOCK_CLIENT_ADDRESS:"
	else if (find_prog_parm("SOCK_CLIENT_ADDRESS:", file_read) || find_prog_parm("#SOCK_CLIENT_ADDRESS:", file_read)) {
	  if (!found_sock_client_address) {
	    found_sock_client_address = true;
	    file_list.push_back(sock_client_address_line);
	  }
	}
      
	// look for "SOCK_OTHER_ADDRESSES:"
	else if (find_prog_parm("SOCK_OTHER_ADDRESSES:", file_read) || find_prog_parm("#SOCK_OTHER_ADDRESSES:", file_read)) {
	  if (!found_sock_other_addresses) {
	    found_sock_other_addresses = true;
	    file_list.push_back(sock_other_addresses_line);
	  }
	}

	// look for "IP_FAMILY:"
	else if (find_prog_parm("IP_FAMILY:", file_read) || find_prog_parm("#IP_FAMILY:", file_read)) {
	  if (!found_ip_family) {
	    found_ip_family = true;
	    file_list.push_back(ip_family_line);
	  }
	}

	// look for "FAX_RECEIVED_POPUP:"
	else if (find_prog_parm("FAX_RECEIVED_POPUP:", file_read) || find_prog_parm("#FAX_RECEIVED_POPUP:", file_read)) {
	  if (!found_fax_received_popup) {
	    found_fax_received_popup = true;
	    file_list.push_back(fax_received_popup_line);
	  }
	}
      
	// look for "FAX_RECEIVED_EXEC:"
	else if (find_prog_parm("FAX_RECEIVED_EXEC:", file_read) || find_prog_parm("#FAX_RECEIVED_EXEC:", file_read)) {
	  if (!found_fax_received_exec) {
	    found_fax_received_exec = true;
	    file_list.push_back(fax_received_exec_line);
	  }
	}
      
	// look for "FAX_RECEIVED_PROG:"
	else if (find_prog_parm("FAX_RECEIVED_PROG:", file_read) || find_prog_parm("#FAX_RECEIVED_PROG:", file_read)) {
	  if (!found_fax_received_prog) {
	    found_fax_received_prog = true;
	    file_list.push_back(Utf8::filename_from_utf8(fax_received_prog_line));
	  }
	}
      
	// look for "ADDR_IN_HEADER:"
	else if (find_prog_parm("ADDR_IN_HEADER:", file_read) || find_prog_parm("#ADDR_IN_HEADER:", file_read)) {
	  if (!found_addr_in_header) {
	    found_addr_in_header = true;
	    file_list.push_back(addr_in_header_line);
	  }
	}
      
	// look for "REDIAL:"
	else if (find_prog_parm("REDIAL:", file_read) || find_prog_parm("#REDIAL:", file_read)) {
	  if (!found_redial) {
	    found_redial = true;
	    file_list.push_back(redial_line);
	  }
	}

	// look for "REDIAL_INTERVAL:"
	else if (find_prog_parm("REDIAL_INTERVAL:", file_read) || find_prog_parm("#REDIAL_INTERVAL:", file_read)) {
	  if (!found_redial_interval) {
	    found_redial_interval = true;
	    file_list.push_back(redial_interval_line);
	  }
	}

	// look for "LOG_FILE:"
	else if (find_prog_parm("LOG_FILE:", file_read) || find_prog_parm("#LOG_FILE:", file_read)) {
	  if (!found_log_file) {
	    found_log_file = true;
	    file_list.push_back(Utf8::filename_from_utf8(logfile_line));
	  }
	}
      
	// add any residual lines to the list, except the terminating line
	else if (!find_prog_parm(terminating_line, file_read)) file_list.push_back(file_read);
      }
      catch (Utf8::ConversionError&) {
	write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
      }
    }


    if (found_number == false) file_list.push_back(number_line);
    if (found_device == false) file_list.push_back(device_line);
    if (found_class == false) file_list.push_back(class_line);
    if (found_page == false) file_list.push_back(page_line);
    if (found_res == false) file_list.push_back(res_line);
    if (found_rings == false) file_list.push_back(rings_line);
    if (found_dialmode == false) file_list.push_back(dialmode_line);
    if (found_init == false) file_list.push_back(init_line);
    if (found_reset == false) file_list.push_back(reset_line);
    if (found_capabilities == false) file_list.push_back(capabilities_line);
    if (found_parms == false) file_list.push_back(extra_parms_line);
    if (found_gtkprint == false) file_list.push_back(gtkprint_line);
    if (found_print_shrink == false) file_list.push_back(print_shrink_line);
    if (found_print_popup == false) file_list.push_back(print_popup_line);
    if (found_run_server == false) file_list.push_back(run_server_line);
    if (found_sock_popup == false) file_list.push_back(sock_popup_line);
    if (found_sock_port == false) file_list.push_back(sock_port_line);
    if (found_sock_client_address == false) file_list.push_back(sock_client_address_line);
    if (found_sock_other_addresses == false) file_list.push_back(sock_other_addresses_line);
    if (found_ip_family == false) file_list.push_back(ip_family_line);
    if (found_fax_received_popup == false) file_list.push_back(fax_received_popup_line);
    if (found_fax_received_exec == false) file_list.push_back(fax_received_exec_line);
    if (found_addr_in_header == false) file_list.push_back(addr_in_header_line);
    if (found_redial == false) file_list.push_back(redial_line);
    if (found_redial_interval == false) file_list.push_back(redial_interval_line);

    try {
      if (found_name == false) file_list.push_back(Utf8::locale_from_utf8(name_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_lock == false) file_list.push_back(Utf8::filename_from_utf8(lock_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_dial_prefix == false) file_list.push_back(Utf8::locale_from_utf8(dial_prefix_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_print_cmd == false) file_list.push_back(Utf8::filename_from_utf8(print_cmd_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_ps_view_cmd == false) file_list.push_back(Utf8::filename_from_utf8(ps_view_cmd_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_fax_received_prog == false) file_list.push_back(Utf8::filename_from_utf8(fax_received_prog_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    try {
      if (found_log_file == false) file_list.push_back(Utf8::filename_from_utf8(logfile_line));
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in SettingsDialog::write_config()\n");
    }

    // add the terminating line
    file_list.push_back(terminating_line);
    
    // now write out the new config file
    
    rcfile = prog_config.homedir;
    rcfile += "/." RC_FILE;
    
    std::ofstream fileout(rcfile.c_str(), std::ios::out);
    if (!fileout) {
      std::string message("Can't open file");
      message += rcfile;
      message += '\n';
      write_error(message.c_str());
      return_val = false;
    }
    else {
      std::copy(file_list.begin(), file_list.end(),
		std::ostream_iterator<std::string>(fileout, "\n"));
    }
  }
  return return_val;
}

void SettingsDialog::read_config(bool search_localfile) {
// search_localfile has a default value of true
// get rc file
  if (!get_rcfile_path(search_localfile)) {
    std::string message;
    if (search_localfile) {
      message =  "Can't find or open file " RC_DIR "/"  RC_FILE ",\n";
      if (std::strcmp(RC_DIR, "/etc")) {
	message += "/etc/" RC_FILE ", ";
      }
      message += "or ";
      message += prog_config.homedir + "/." RC_FILE "\n";
    }
    else {
      message =  "Can't find or open file " RC_DIR "/"  RC_FILE "\n";
      if (std::strcmp(RC_DIR, "/etc")) {
	message += "or /etc/" RC_FILE "\n";
      }
    }
    write_error(message.c_str());
  }

  else {
// now extract settings from file

    std::ifstream filein(rcfile.c_str(), std::ios::in);
    if (!filein) {
      std::string message("Can't open file ");
      message += rcfile;
      write_error(message.c_str());
    }

    else {
      std::string file_read;
      std::string result;
      while (std::getline(filein, file_read)) {

	if (!file_read.empty() && file_read[0] != '#') { // valid line to check
	  // now check for other comment markers
	  std::string::size_type pos = file_read.find_first_of('#');
	  if (pos != std::string::npos) file_read.resize(pos); // truncate
	
	  // look for "NAME:"
	  if (get_prog_parm("NAME:", file_read, result)) {
	    if (is_ascii(result)) identity_page.set_name(result);
	  }
	
	  // look for "NUMBER:"
	  else if (get_prog_parm("NUMBER:", file_read, result)) {
	    identity_page.set_number(result);
	  }
	
	  // look for "DEVICE:"
	  else if (get_prog_parm("DEVICE:", file_read, result)) {
	    modem_page.set_device(result);
	  }
	
	  // look for "LOCK:"
	  else if (get_prog_parm("LOCK:", file_read, result, Utf8::filename_to_utf8)) {
	    modem_page.set_lock(result);
	  }

	  // look for "CLASS:"
	  else if (get_prog_parm("CLASS:", file_read, result)) {
	    if (!result.compare("1")
		|| !result.compare("2")
		|| !result.compare("2.0")) {
	      modem_page.set_class(result);
	    }
	    else modem_page.set_class("2");
	  }

	  // look for "PAGE:"
	  else if (get_prog_parm("PAGE:", file_read, result)) {
	    page_page.set_page(result);
	  }

	  // look for "RES:"
	  else if (get_prog_parm("RES:", file_read, result)) {
	    send_page.set_res(result);
	  }
	
	  // look for "DIAL_PREFIX:"
	  else if (get_prog_parm("DIAL_PREFIX:", file_read, result)) {
	    send_page.set_dial_prefix(result);
	  }

	  // look for "RINGS:"
	  else if (get_prog_parm("RINGS:", file_read, result)) {
	    if (std::atoi(result.c_str()) < 1
		|| std::atoi(result.c_str()) > 9) {
	      modem_page.set_rings("1");
	    }
	    else modem_page.set_rings(result);
	  }
	
	  // look for "DIALMODE:"
	  else if (get_prog_parm("DIALMODE:", file_read, result)) {
	    modem_page.set_dialmode(result);
	  }
	
	  // look for "INIT:"
	  else if (get_prog_parm("INIT:", file_read, result)) {
	    parms_page.set_init(result);
	  }
	
	  // look for "RESET:"
	  else if (get_prog_parm("RESET:", file_read, result)) {
	    parms_page.set_reset(result);
	  }

	  // look for "CAPABILITIES:"
	  else if (get_prog_parm("CAPABILITIES:", file_read, result)) {
	    modem_page.set_capabilities(result);
	  }

	  // look for "PARMS:"
	  else if (get_prog_parm("PARMS:", file_read, result)) {
	    parms_page.set_parms(result);
	  }

	  // look for "PRINT_CMD:"
	  else if (get_prog_parm("PRINT_CMD:", file_read, result)) {
	    print_page.set_command(result);
	  }

	  // look for "USE_GTKPRINT:"
	  else if (get_prog_parm("USE_GTKPRINT:", file_read, result)) {
	    print_page.set_gtkprint(result);
	  }

	  // look for "PRINT_SHRINK:"
	  else if (get_prog_parm("PRINT_SHRINK:", file_read, result)) {
	    if (std::atoi(result.c_str()) < 50
		|| std::atoi(result.c_str()) > 100) {
	      print_page.set_shrink("100");
	    }
	    else print_page.set_shrink(result);
	  }

	  // look for "PRINT_POPUP:"
	  else if (get_prog_parm("PRINT_POPUP:", file_read, result)) {
	    print_page.set_popup(result);
	  }

	  // look for "PS_VIEWER:"
	  else if (get_prog_parm("PS_VIEWER:", file_read, result)) {
	    view_page.set_ps_view_command(result);
	  }

	  // look for "SOCK_SERVER:"
	  else if (get_prog_parm("SOCK_SERVER:", file_read, result)) {
	    sock_page.set_run_server(result);
	  }

	  // look for "SOCK_POPUP:"
	  else if (get_prog_parm("SOCK_POPUP:", file_read, result)) {
	    sock_page.set_popup(result);
	  }

	  // look for "SOCK_SERVER_PORT:"
	  else if (get_prog_parm("SOCK_SERVER_PORT:", file_read, result)) {
	    if (std::atoi(result.c_str()) < 1024
		|| std::atoi(result.c_str()) > 65535) {
	      sock_page.set_port("9900");
	    }
	    else sock_page.set_port(result);
	  }

	  // look for "SOCK_CLIENT_ADDRESS:"
	  else if (get_prog_parm("SOCK_CLIENT_ADDRESS:", file_read, result)) {
	    sock_page.set_if_other_address(result);
	  }

	  // look for "SOCK_OTHER_ADDRESSES:"
	  else if (get_prog_parm("SOCK_OTHER_ADDRESSES:", file_read, result)) {
	    sock_page.set_other_addresses(result);
	  }

	  // look for "IP_FAMILY:"
	  else if (get_prog_parm("IP_FAMILY:", file_read, result)) {
	    sock_page.set_ip_family(result);
	  }

	  // look for "FAX_RECEIVED_POPUP:"
	  else if (get_prog_parm("FAX_RECEIVED_POPUP:", file_read, result)) {
	    receive_page.set_popup(result);
	  }

	  // look for "FAX_RECEIVED_EXEC:"
	  else if (get_prog_parm("FAX_RECEIVED_EXEC:", file_read, result)) {
	    receive_page.set_exec(result);
	  }

	  // look for "FAX_RECEIVED_PROG:"
	  else if (get_prog_parm("FAX_RECEIVED_PROG:", file_read, result)) {
	    receive_page.set_prog(result);
	  }

	  // look for "ADDR_IN_HEADER:"
	  else if (get_prog_parm("ADDR_IN_HEADER:", file_read, result)) {
	    send_page.set_addr_in_header(result);
	  }

	  // look for "REDIAL:"
	  else if (get_prog_parm("REDIAL:", file_read, result)) {
	    send_page.set_redial(result);
	  }

	  // look for "REDIAL_INTERVAL:"
	  else if (get_prog_parm("REDIAL_INTERVAL:", file_read, result)) {
	    send_page.set_redial_interval(result);
	  }

	  // look for "LOG_FILE:"
	  else if (get_prog_parm("LOG_FILE:", file_read, result, Utf8::filename_to_utf8)) {
	    logging_page.set_logfile(result);
	  }
	}
      }
    }
  }
}

void SettingsDialog::get_reset_settings_prompt(void) {
  std::string message(gettext("Enter settings from "));
  message += RC_DIR "/"  RC_FILE;
  if (std::strcmp(RC_DIR, "/etc")) {
    message += gettext(" or\n");
    message += "/etc/" RC_FILE;
  }
  message += gettext("?");

  PromptDialog* dialog_p = new PromptDialog(message.c_str(), gettext("Reset settings"),
					    standard_size, get_win());
  dialog_p->accepted.connect(Callback::make(*this, &SettingsDialog::get_reset_settings));
  // there is no memory leak -- the memory will be deleted when PromptDialog closes
}

void SettingsDialog::get_reset_settings(void) {

  // clear all the existing settings
  identity_page.clear();
  modem_page.clear();
  parms_page.clear();
  print_page.clear();
  view_page.clear();
  sock_page.clear();
  receive_page.clear();
  send_page.clear();
  logging_page.clear();
  page_page.clear();

  read_config(false); // read settings without searching for config file in home directory
}

bool SettingsDialog::get_prog_parm(const char* name, std::string& line, std::string& result,
				   std::string(*convert_func)(const std::string&)) {
// This function looks for a setting named `name' in the string `line'
// and returns the values stated after it in string `result'.  It returns
// `true' if the setting was found.  If there are trailing spaces or tabs,
// string `line' will be modified.  string `result' is only modified if
// the `name' setting is found.  Anything extracted from `line' will be
// converted (when placed into `result') to UTF-8 as maintained by
// std::string, using the function assigned to function pointer
// convert_func (you would normally use Utf8::locale_to_utf8() or
// Utf8::filename_to_utf8(), and there is a default inline method
// using Utf8::locale_to_utf8()

  const std::string::size_type length = std::strlen(name);
  // we have to use std::string::substr() because libstdc++-2
  // doesn't support the Std-C++ std::string::compare() functions
  if (!line.substr(0, length).compare(name)) {
    // erase any trailing space or tab
    while (line.find_last_of(" \t") == line.size() - 1) line.resize(line.size() - 1);
    if (line.size() > length) {
      // ignore any preceding space or tab from the setting value given
      std::string::size_type pos = line.find_first_not_of(" \t", length); // pos now is set to beginning of setting value
      if (pos != std::string::npos) {
	try {
	  result.assign(convert_func(line.substr(pos)));
	}
	catch (Utf8::ConversionError&) {
	  result = "";
	  write_error("UTF-8 conversion error in SettingsDialog::get_prog_parm()\n");
	}
      }
    }
    return true;
  }
  return false;
}

bool SettingsDialog::find_prog_parm(const char* name, const std::string& line) {
// this function looks for a setting named `name' in the string `line'
// it returns `true' if the setting was found or false otherwise

  const std::string::size_type length = std::strlen(name);
  // we have to use std::string::substr() because libstdc++-2
  // doesn't support the Std-C++ std::string::compare() functions
  if (!line.substr(0, length).compare(name)) return true;
  return false;
}

bool SettingsDialog::get_rcfile_path(bool search_localfile) {
// search_localfile has a default value of true

  bool found_rcfile = false;

  if (search_localfile) {
    rcfile = prog_config.homedir;
    rcfile += "/." RC_FILE;

    if (!access(rcfile.c_str(), F_OK)) {
      found_rcfile = true;
      is_home_config = true;
    }
  }

  if (!found_rcfile) {

    rcfile = RC_DIR "/" RC_FILE;
    if (!access(rcfile.c_str(), F_OK)) found_rcfile = true;
  }

  if (!found_rcfile && std::strcmp(RC_DIR, "/etc")) {

    rcfile = "/etc/" RC_FILE;
    if (!access(rcfile.c_str(), F_OK)) found_rcfile = true;
  }
  if (!found_rcfile) rcfile = "";
  return found_rcfile;
}

void SettingsDialog::show_help(const std::pair<std::string, std::string>& text) {
  new SettingsHelpDialog(standard_size,
			 text.first, text.second, get_win());
  // there is no memory leak
  // the SettingsHelpDialog object will delete its own memory when it is closed
}

void SettingsDialog::strip(std::string& setting) {

  // erase any trailing space or tab
  while (!setting.empty() && setting.find_last_of(" \t") == setting.size() - 1) {
    setting.resize(setting.size() - 1);
  }
  // erase any leading space or tab
  while (!setting.empty() && (setting[0] == ' ' || setting[0] == '\t')) {
    setting.erase(0, 1);
  }
}

bool SettingsDialog::is_ascii(const std::string& text) {
  
  std::string::const_iterator end_iter = text.end();
  if (std::find_if(text.begin(), end_iter, not_ascii_char) == end_iter) {
    return true;
  }
  return false;
}
