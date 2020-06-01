/* Copyright (C) 2008 to 2010 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

*/

#ifndef MONO_TIFF_PRINTMANAGER_H
#define MONO_TIFF_PRINTMANAGER_H

#include <prog_defs.h>

#include <gtk/gtk.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <vector>
#include <string>

#include <cairo.h>

#include <glib-object.h>

#include <c++-gtk-utils/intrusive_ptr.h>
#include <c++-gtk-utils/gobj_handle.h>
#include <c++-gtk-utils/notifier.h>
#include <c++-gtk-utils/mutex.h>


/* The MonoTiffPrintManager class prints monochrome (1 bit per pixel)
   TIFF image files using the GTK+ printer interface provided with
   GTK+ 2.10.0 onwards, and libtiff.  Each page of a document to be
   printed must comprise a separate file, and the pages will print in
   the order in which their respective files are placed in the
   std::vector object passed to MonoTiffPrintManager::set_filenames().
   To obtain a MonoTiffPrintManager object, call
   MonoTiffPrintManager::create_manager(), and that method is thread
   safe provided that, if it is called in a thread other than the one
   in which the GTK+ event loop runs, Cgu::Notifier::init() has
   previously been called in the GTK+ event loop thread.
   MonoTiffPrintManager::set_filenames(), which as mentioned above
   passes the filenames to be printed for any one print job, can be
   called in any thread.  To print the files, call
   MonoTiffPrintManager::print(), which may also be done in any thread
   (assuming the gdk global lock is not used - if it is, it must be
   called in the main GUI thread).  The MonoTiffPrintManager::view()
   and MonoTiffPrintManager::print_to_file() methods are also
   available, which may be called in any thread (assuming as mentioned
   above).

   The MonoTiffPrintManager::page_setup() method can be used as a
   callback to display a document page setup dialog prior to printing,
   and this method should only be called in the thread in which the
   main GTK+ event loop runs.  It is a static method (a
   MonoTiffPrintManager object does not need to have been created
   before it is called).

   Once MonoTiffPrintManager::print(), MonoTiffPrintManager::view() or
   MonoTiffPrintManager::print_to_file() has been called, the
   MonoTiffPrintManager class owns a reference to itself and so
   manages its own lifetime - so once one of the methods has been
   called it doesn't matter if the IntrusivePtr returned by
   MonoTiffPrintManager::create_manager() goes out of scope (however,
   once MonoTiffPrintManager::print(), MonoTiffPrintManager::view() or
   MonoTiffPrintManager::print_to_file() has been called, the object
   will not be deleted until both printing has completed or failed AND
   the IntrusivePtr returned by MonoTiffPrintManager::create_manager()
   has gone out of scope or has been reset()).

   Normally, you would probably only want to use any one
   MonoTiffPrintManager object to print files forming a single print
   job (it has been designed with that in mind).  Nevertheless, if you
   keep a reference to the object alive via an active IntrusivePtr you
   can use it to print more than one print job.  However, if that is
   done it is not possible to call MonoTiffPrintManager::print()
   MonoTiffPrintManager::view(), MonoTiffPrintManager::print_to_file()
   or MonoTiffPrintManager::set_filenames() until the previous print
   job (if any) has been dispatched to the GTK+ print system (or
   cancelled).  If the MonoTiffPrintManager object is not ready
   because it is in the middle of handling an earlier print job, then
   the call to MonoTiffPrintManager::print(),
   MonoTiffPrintManager::view(), MonoTiffPrintManager::print_to_file()
   or MonoTiffPrintManager::set_filenames() will return false; if
   however the new print job was successfully started or filenames
   successfully set, they will return true.  You do not need to check
   the return value of these methods for the first print job
   despatched by any one MonoTiffPrintManager object.

   MonoTiffPrintManager::print_to_file() will also return false if no
   file name to which to print was specified.

*/

class MonoTiffPrintManager: public IntrusiveLockCounter {
  enum Mode {print_mode, view_mode, file_mode} mode;

  Thread::Mutex mutex;
  GtkWindow* parent_p;
  std::vector<std::string> filenames;
  std::string write_filename;
  Notifier print_notifier;
  bool ready;
  bool cancelled_when_drawing;
  cairo_user_data_key_t destroy_key;

  static GobjHandle<GtkPrintSettings> print_settings_h;
  static GobjHandle<GtkPageSetup> page_setup_h;

  void print_files(void);
  void begin_print_impl(GtkPrintOperation*, GtkPrintContext*);
  void draw_page_impl(GtkPrintOperation*, GtkPrintContext*, int);
  // private constructor
  MonoTiffPrintManager(void) {}
  // and this class may not be copied
  MonoTiffPrintManager(const MonoTiffPrintManager&);
  MonoTiffPrintManager& operator=(const MonoTiffPrintManager&);
public:
  // this helper class avoids exposing GObject callbacks with C
  // linkage to the global namespace
  class CB;
  friend class CB;

  // if a particular font is wanted, then this can be entered as the font_family and font_size
  // arguments to the create_manager() method..  If the default of an empty string for
  // font_family and 0 for font_size is used, then printing will use the previous font settings
  // (if any), or if not a font of "Mono" and a size of 10. The fonts passed in this method can
  // in any event be overridden from the "Print font" page in the print dialog.  If a font size is
  // passed as an argument, then the value must be 0 (default) or between 8 and 24 (actual).
  static IntrusivePtr<MonoTiffPrintManager> create_manager(GtkWindow* parent = 0);
  static void page_setup(GtkWindow* parent = 0);

  // for efficiency reasons (the string could hold a lot of text and we do not
  // want more copies than necessary hanging around) the text is passed by
  // reference (by std::auto_ptr<>).  We pass by std::auto_ptr so that the
  // string cannot be modified after it has been passed to the MonoTiffPrintManager
  // object - we take ownership of it
  bool set_filenames(const std::vector<std::string>&);
  bool print(void);
  bool view(void);
  bool print_to_file(const char* filename);
  ~MonoTiffPrintManager(void);
};

#endif // GTK_CHECK_VERSION
#endif // MONO_TIFF_PRINTMANAGER_H
