/* Copyright (C) 2004, 2006, 2009 and 2010 Chris Vine

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

#include <string>
#include <ios>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "tray_icon.h"
#include "efax_controller.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

class TrayIcon::CB {
public:
  static void tray_icon_activated(void* data) {
    static_cast<TrayIcon*>(data)->activated();
  }

  static void tray_icon_popup(guint button,
			      guint time,
			      void* data) {

    TrayIcon* instance_p = static_cast<TrayIcon*>(data);
  
    int state;
    instance_p->get_state(state);
    if (state == EfaxController::inactive) {
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->stop_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_takeover_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_answer_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_standby_item_p), true);
    }
    else  {
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->stop_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_takeover_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_answer_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_standby_item_p), false);
    }
    gtk_menu_popup(GTK_MENU(instance_p->menu_h.get()), 0, 0, 0, 0,
		   button, time);
  }

  static void menu_item_activated(GtkMenuItem* item_p, void* data) {
    TrayIcon* instance_p = static_cast<TrayIcon*>(data);

    if (item_p == instance_p->list_received_faxes_item_p) {
      instance_p->menu_item_chosen(TrayIcon::list_received_faxes);
    }
    else if (item_p == instance_p->list_sent_faxes_item_p) {
      instance_p->menu_item_chosen(TrayIcon::list_sent_faxes);
    }
    else if (item_p == instance_p->redial_queue_item_p) {
      instance_p->menu_item_chosen(TrayIcon::redial_queue);
    }
    else if (item_p == instance_p->receive_takeover_item_p) {
      instance_p->menu_item_chosen(TrayIcon::receive_takeover);
    }
    else if (item_p == instance_p->receive_answer_item_p) {
      instance_p->menu_item_chosen(TrayIcon::receive_answer);
    }
    else if (item_p == instance_p->receive_standby_item_p) {
      instance_p->menu_item_chosen(TrayIcon::receive_standby);
    }
    else if (item_p == instance_p->stop_item_p) {
      instance_p->menu_item_chosen(TrayIcon::stop);
    }
    else if (item_p == instance_p->quit_item_p) {
      instance_p->menu_item_chosen(TrayIcon::quit);
    }
    else {
      write_error("Callback error in TrayIcon::CB::menu_item_activated()\n");
    }
  }
};

extern "C" {
  static void tray_icon_activated_cb(GtkStatusIcon*,
				     void* data) {
    TrayIcon::CB::tray_icon_activated(data);
  }

  static void tray_icon_popup_cb(GtkStatusIcon*,
				 guint button,
				 guint time,
				 void* data) {
    TrayIcon::CB::tray_icon_popup(button, time, data);
  }

  static void menu_item_activated_cb(GtkMenuItem* item_p,
				     void* data) {
    TrayIcon::CB::menu_item_activated(item_p, data);
  }
} // extern "C"

TrayIcon::TrayIcon(void): status_icon_h(gtk_status_icon_new()),
			  menu_h(gtk_menu_new()) {

  GtkWidget* image_p;

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU);
  list_received_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("List received faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_received_faxes_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(list_received_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_received_faxes_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU);
  list_sent_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("List sent faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_sent_faxes_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(list_sent_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_sent_faxes_item_p));

  // insert separator
  GtkWidget* separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU);
  redial_queue_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("Redial queue")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(redial_queue_item_p), image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(redial_queue_item_p));
  gtk_widget_show(GTK_WIDGET(redial_queue_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), separator_item_p);
  gtk_widget_show(separator_item_p);

  receive_takeover_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Take over call")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(receive_takeover_item_p));
  gtk_widget_show(GTK_WIDGET(receive_takeover_item_p));

  receive_answer_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Answer call")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(receive_answer_item_p));
  gtk_widget_show(GTK_WIDGET(receive_answer_item_p));

  receive_standby_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Receive standby")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(receive_standby_item_p));
  gtk_widget_show(GTK_WIDGET(receive_standby_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_STOP,  GTK_ICON_SIZE_MENU);
  stop_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("Stop")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(stop_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(stop_item_p));
  gtk_widget_show(GTK_WIDGET(stop_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_QUIT,  GTK_ICON_SIZE_MENU);
  quit_item_p = 
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("Quit")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(quit_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_h.get()), GTK_WIDGET(quit_item_p));
  gtk_widget_show(GTK_WIDGET(quit_item_p));

  g_signal_connect(G_OBJECT(list_received_faxes_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(list_sent_faxes_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(redial_queue_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_takeover_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_answer_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_standby_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(stop_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);
  g_signal_connect(G_OBJECT(quit_item_p), "activate",
		   G_CALLBACK(menu_item_activated_cb), this);

  // have an icon size of between 22 and 38 pixels
  int icon_size = std::max(std::min(gtk_status_icon_get_size(status_icon_h), 40) - 2, 22);
  { // scope block for the GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_scale_simple(prog_config.window_icon_h,
							   icon_size, icon_size, GDK_INTERP_HYPER));
    gtk_status_icon_set_from_pixbuf(status_icon_h, pixbuf_h);
  }

  set_tooltip_cb(gettext("Inactive"));
  
  g_signal_connect(G_OBJECT(status_icon_h.get()), "activate",
                   G_CALLBACK(tray_icon_activated_cb), this);

  g_signal_connect(G_OBJECT(status_icon_h.get()), "popup-menu",
                   G_CALLBACK(tray_icon_popup_cb), this);

}

void TrayIcon::set_tooltip_cb(const char* text) {

  int state;
  get_state(state);

  std::ostringstream strm;
  strm << gettext("efax-gtk: ") << text;
  if (state == EfaxController::receive_standby) {
    int count;
    get_new_fax_count(count);
    strm << '\n' << gettext("New faxes:") << ' ' << count;
  }

#if GTK_CHECK_VERSION(2,16,0)
  gtk_status_icon_set_tooltip_text(status_icon_h, strm.str().c_str());
#else
  gtk_status_icon_set_tooltip(status_icon_h, strm.str().c_str());
#endif
}

bool TrayIcon::is_embedded(void) {

  gboolean embedded;
  g_object_get(status_icon_h.get(),
	       "embedded", &embedded,
	       static_cast<void*>(0));

  return embedded;
}

