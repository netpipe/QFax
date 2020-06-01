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

#ifndef REDIAL_QUEUE_H
#define REDIAL_QUEUE_H

#include "prog_defs.h"

#include <list>
#include <string>
#include <utility>

#include <gtk/gtk.h>
#include <glib.h>

#include <c++-gtk-utils/window.h>
#include <c++-gtk-utils/emitter.h>
#include <c++-gtk-utils/gobj_handle.h>

// first is a pointer to the source id of the timeout, second is the number
typedef std::pair<guint*, std::string> QueueItem;

class RedialQueueDialog;

class RedialQueue {
  const int standard_size;
  std::list<QueueItem> item_list;
  RedialQueueDialog* dialog_p;
public:
  friend class RedialQueueDialog;

  Emitter changed;

  // this method takes ownership of QueueItem::first and will
  // delete it when remove() or remove_all() is called
  void add(const QueueItem& item);
  bool remove(guint* id);
  void remove_all(void);
  void show_dialog(void);
  RedialQueue(const int size): standard_size(size) {}
  ~RedialQueue(void) {remove_all();}
};

namespace RedialQueueDialogCB {
  extern "C" {
    void redial_queue_dialog_button_clicked(GtkWidget*, void*);
    void redial_queue_dialog_set_button(GtkTreeSelection*, void*);
  }
}

class RedialQueueDialog: public WinBase {
  static int is_redial_queue_dialog;

  const int standard_size;
  GtkWidget* remove_button_p;
  GtkWidget* close_button_p;
  GobjHandle<GtkTreeModel> list_store_h;
  GtkTreeView* tree_view_p;
  RedialQueue* redial_queue_p;

  void populate_item(const QueueItem& item);
  void remove(void);
  void remove_prompt(void);
public:
  friend void RedialQueueDialogCB::redial_queue_dialog_button_clicked(GtkWidget*, void*);
  friend void RedialQueueDialogCB::redial_queue_dialog_set_button(GtkTreeSelection*, void*);

  Releaser releaser;

  void populate(void);
  void refresh(void);

  static int get_is_redial_queue_dialog(void) {return is_redial_queue_dialog;}

  RedialQueueDialog(const int standard_size, RedialQueue* redial_queue);
  ~RedialQueueDialog(void) {--is_redial_queue_dialog;}
};

#endif
