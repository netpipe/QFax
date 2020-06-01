/* Copyright (C) 2008 to 2011 Chris Vine

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

#include <gtk/gtk.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <unistd.h>

#include <stdlib.h>
#include <cstring>

#include <tiffio.h>

#include <glib.h>

#include <prog_defs.h>

#include "mono_tiff_print_manager.h"
#include "tiff_handle.h"
#include "cairo_handle.h"

#include <c++-gtk-utils/gerror_handle.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// uncomment the following if the specialist write_error() function in
// prog_defs.h and definition of MEM_ERROR are not available in the
// application in question
/*
#include <iostream>
#ifdef HAVE_OSTREAM
#include <ostream>
#endif
inline void write_error(const char* message) {
  std::cerr << message;
}
#define MEM_ERROR 10
*/

inline uint32 cr_get_stride(uint32 width) {
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 6, 0)
  return cairo_format_stride_for_width (CAIRO_FORMAT_A1, width);
#else
  return ((width + 7)/8 + 3) & (~3);
#endif
}

// define this if asynchronous printing is permitted - this only works
// with GTK+2.10.13 or greater (printing to file doesn't work before
// GTK+2.10.14, whether synchronously or asynchronously)
#define MONO_TIFF_PRINT_MANAGER_ALLOW_ASYNC 1

#if !(GTK_CHECK_VERSION(2,10,13))
#undef MONO_TIFF_PRINT_MANAGER_ALLOW_ASYNC
#endif

GobjHandle<GtkPrintSettings> MonoTiffPrintManager::print_settings_h;
GobjHandle<GtkPageSetup> MonoTiffPrintManager::page_setup_h;

class MonoTiffPrintManager::CB {
public:
  static void begin_print(GtkPrintOperation* print_operation_p,
			  GtkPrintContext* context_p,
			  void* data) {
    static_cast<MonoTiffPrintManager*>(data)->begin_print_impl(print_operation_p, context_p);
  }

  static void draw_page(GtkPrintOperation* print_operation_p,
			GtkPrintContext* context_p,
			gint page_nr,
			void* data) {
    static_cast<MonoTiffPrintManager*>(data)->draw_page_impl(print_operation_p, context_p, page_nr);
  }

  static void done(GtkPrintOperation* print_operation_p,
		   GtkPrintOperationResult result,
		   void* data) {

    MonoTiffPrintManager* instance_p = static_cast<MonoTiffPrintManager*>(data);
    if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
      write_error("Print error in MonoTiffPrintManagerCB::mono_tiff_print_done()\n");
      GError* error_p;
      gtk_print_operation_get_error(print_operation_p, &error_p);
      GerrorScopedHandle handle_h(error_p);
      write_error(error_p->message);
      write_error("\n");
    }
    else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
      // save the print settings
      MonoTiffPrintManager::print_settings_h =
	GobjHandle<GtkPrintSettings>(gtk_print_operation_get_print_settings(print_operation_p));
      // and take ownership of the GtkPrintSettings object
      g_object_ref(G_OBJECT(MonoTiffPrintManager::print_settings_h.get()));
    }
    else if (result == GTK_PRINT_OPERATION_RESULT_CANCEL
	     && instance_p->parent_p
	     && !instance_p->cancelled_when_drawing) {
      // the parent will not be set sensitive if MonoTiffPrintManager::begin_print_impl()
      // has not been entered because the print job was cancelled from the print dialog
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->parent_p), true);
    }
    { // scope block for mutex lock
      Thread::Mutex::Lock lock(instance_p->mutex);
      instance_p->ready = true;
    }
    instance_p->unref();
  }

  static void page_setup_done(GtkPageSetup* page_setup_p, void* data) {

    if (page_setup_p) {
      // The documentation is unclear whether we get a new GtkPageSetup
      // object back if an existing GtkPageSetup object was passed to
      // gtk_print_run_page_setup_dialog_async().  However this is safe
      // even if we get the same object back again - see GobjHandle
      // implementation
      MonoTiffPrintManager::page_setup_h = GobjHandle<GtkPageSetup>(page_setup_p);
    }
    if (data) gtk_widget_set_sensitive(GTK_WIDGET(data), true);
  }

  static void request_page_setup(GtkPageSetup* page_setup_p) {
    // we want to remove the right and bottom margins, as these will
    // be established by the fax as sent.  If the recipient wants to
    // scale them to fit different printer margins, the user should
    // set scaling from the print dialog.  We need to keep the left
    // and top margins so that the top fax header line will print -
    // the draw_page_impl() method will automatically scale for these.
    gtk_page_setup_set_bottom_margin(page_setup_p, 0.0, GTK_UNIT_MM);
    gtk_page_setup_set_right_margin(page_setup_p, 0.0, GTK_UNIT_MM);
  }
};

// the GObject callback functions with both C linkage
// specification and internal linkage
extern "C" {

  static void mtp_begin_print(GtkPrintOperation* print_operation_p,
			      GtkPrintContext* context_p,
			      void* data) {
    MonoTiffPrintManager::CB::begin_print(print_operation_p, context_p, data);
  }

  static void mtp_draw_page(GtkPrintOperation* print_operation_p,
			    GtkPrintContext* context_p,
			    gint page_nr,
			    void* data) {
    MonoTiffPrintManager::CB::draw_page(print_operation_p, context_p, page_nr, data);
  }

  static void mtp_done(GtkPrintOperation* print_operation_p,
		       GtkPrintOperationResult result,
		       void* data) {
    MonoTiffPrintManager::CB::done(print_operation_p, result, data);
  }

  static void mtp_page_setup_done(GtkPageSetup* page_setup_p, void* data) {
    MonoTiffPrintManager::CB::page_setup_done(page_setup_p, data);
  }

  static void mtp_request_page_setup(GtkPrintOperation*, GtkPrintContext*,
				     gint, GtkPageSetup* page_setup_p, void*) {
    MonoTiffPrintManager::CB::request_page_setup(page_setup_p);
  }

} // extern "C"


MonoTiffPrintManager::~MonoTiffPrintManager(void) {

  // clear the filenames vector through a mutex to synchronise memory
  Thread::Mutex::Lock lock(mutex);
  filenames.clear();
}

IntrusivePtr<MonoTiffPrintManager> MonoTiffPrintManager::create_manager(GtkWindow* parent) {

  MonoTiffPrintManager* instance_p = new MonoTiffPrintManager;
  instance_p->print_notifier.connect(Callback::make(*instance_p, &MonoTiffPrintManager::print_files));

  Thread::Mutex::Lock lock(instance_p->mutex);
  std::memset(&(instance_p->destroy_key), 0, sizeof(cairo_user_data_key_t));
  instance_p->parent_p = parent;
  instance_p->cancelled_when_drawing = false;
  instance_p->ready = true;
  
  return IntrusivePtr<MonoTiffPrintManager>(instance_p);
}

void MonoTiffPrintManager::page_setup(GtkWindow* parent) {

  if (!print_settings_h.get()) {
    print_settings_h = GobjHandle<GtkPrintSettings>(gtk_print_settings_new());
  }

  if (parent) gtk_widget_set_sensitive(GTK_WIDGET(parent), false);
  gtk_print_run_page_setup_dialog_async(parent, page_setup_h, print_settings_h, 
					mtp_page_setup_done,
					parent);
}

bool MonoTiffPrintManager::set_filenames(const std::vector<std::string>& filenames_) {
  Thread::Mutex::Lock lock(mutex);
  if (!ready) return false;
  filenames = filenames_;
  return true;
}

bool MonoTiffPrintManager::print(void) {

  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    mode = print_mode;
    // protect our innards
    ready = false;
  }

  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

bool MonoTiffPrintManager::view(void) {
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    mode = view_mode;
    // protect our innards
    ready = false;
  }

  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

bool MonoTiffPrintManager::print_to_file(const char* filename) {

  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    write_filename = filename;
    if (write_filename.empty()) {
      write_error(gettext("No file to print specified"));
      return false;
    }

    mode = file_mode;
    // protect our innards
    ready = false;
  }
  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

void MonoTiffPrintManager::print_files(void) {

  // hand back ownership to local scope so that if there is a problem
  // or an exception is thrown this method cleans itself up, and it also
  // automatically cleans up if we do not do an asynchronous print (we will
  // ref() again later in this method if we are going to do an asynchronous
  // print and everything is set up)
  IntrusivePtr<MonoTiffPrintManager> temp(this);
  unref();

  GobjHandle<GtkPrintOperation> print_operation_h(gtk_print_operation_new());

  if (print_settings_h.get()) {
    gtk_print_operation_set_print_settings(print_operation_h, print_settings_h);
  }

  if (page_setup_h.get()) {
    gtk_print_operation_set_default_page_setup(print_operation_h, page_setup_h);
  }

  g_signal_connect(G_OBJECT(print_operation_h.get()), "begin_print",
		   G_CALLBACK(mtp_begin_print), this);
  g_signal_connect(G_OBJECT(print_operation_h.get()), "draw_page",
		   G_CALLBACK(mtp_draw_page), this);
  g_signal_connect(G_OBJECT(print_operation_h.get()), "request_page_setup",
		   G_CALLBACK(mtp_request_page_setup), 0);
#ifdef MONO_TIFF_PRINT_MANAGER_ALLOW_ASYNC
  g_signal_connect(G_OBJECT(print_operation_h.get()), "done",
		   G_CALLBACK(mtp_done), this);
  gtk_print_operation_set_allow_async(print_operation_h, true);
  // regain ownership of ourselves (the print system will do the final
  // unreference in the MonoTiffPrintManagerCB::mono_tiff_print_done() callback)
  ref();
#endif

  GError* error_p = 0;
  GtkPrintOperationResult result;
  Mode mode_chosen;
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    mode_chosen = mode;
  }

  if (parent_p) gtk_widget_set_sensitive(GTK_WIDGET(parent_p), false);

  if (mode_chosen == file_mode) {
    { // scope block for mutex lock
      Thread::Mutex::Lock lock(mutex);
      gtk_print_operation_set_export_filename(print_operation_h, write_filename.c_str());
    }
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_EXPORT,
				     parent_p, &error_p);
  }
  else if (mode_chosen == view_mode) {
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_PREVIEW,
				     parent_p, &error_p);
  }
  else {
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				     parent_p, &error_p);
  }

  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    write_error("GtkPrintOperation error in MonoTiffPrintManager::print_text()\n");
    if (error_p) {
      GerrorScopedHandle handle_h(error_p);
      write_error(error_p->message);
      write_error("\n");
    }
  }
#ifndef MONO_TIFF_PRINT_MANAGER_ALLOW_ASYNC
  else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    print_settings_h =
      GobjHandle<GtkPrintSettings>(gtk_print_operation_get_print_settings(print_operation_h.get()));
    // take ownership of the GtkPrintSettings object
    g_object_ref(G_OBJECT(print_settings_h.get()));
  }
  else if (result == GTK_PRINT_OPERATION_RESULT_CANCEL
	   && parent_p
	   && !cancelled_when_drawing) {
    // the parent will not be set sensitive if MonoTiffPrintManager::begin_print_impl()
    // has not been entered because the print job was cancelled from the print dialog
    gtk_widget_set_sensitive(GTK_WIDGET(parent_p), true);
  }
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    ready = true;
  }
#endif
}

void MonoTiffPrintManager::begin_print_impl(GtkPrintOperation* print_operation_p,
					    GtkPrintContext*) {

  // The 'filenames' variable is protected by a mutex in
  // set_filenames().  In the print(), view() and print_file() methods
  // the 'ready' variable is unset and the 'mode' variable written to,
  // under a lock of the same mutex.  The unsetting of the 'ready'
  // variable in those methods will prevent any further actions on
  // 'filenames' until the current print job has completed.
  // Furthermore, the call to print_files() always occurs in the GUI
  // thread, and access to the 'mode' variable in print_files() is
  // protected by the same mutex.  This means that no read access to
  // 'filenames' in this method can "happen before" the release of the
  // mutex in set_filenames(), and no subsequent write access to
  // 'filenames' in set_filenames() can "happen before" completion of
  // the current print job, and there will be memory visibility
  // between the respective threads, according to both the POSIX and
  // proposed C++-0x standards.  No further mutex locking is therefore
  // required here.
  gtk_print_operation_set_n_pages(print_operation_p, filenames.size());

  if (parent_p) gtk_widget_set_sensitive(GTK_WIDGET(parent_p), true);
}

void MonoTiffPrintManager::draw_page_impl(GtkPrintOperation* print_operation_p,
					  GtkPrintContext* context_p, int page_nr) {

  // With respect to the 'filenames' variable, no further locking is
  // required in this method for the reasons given in the comments in
  // the begin_print_impl() method.

  // check initial conditions
  if (page_nr >= static_cast<int>(filenames.size()) || page_nr < 0) {
    write_error("Yikes, invalid page number passed to MonoTiffPrintManager::draw_page_impl()\n");
    cancelled_when_drawing = true;
    gtk_print_operation_cancel(print_operation_p);    
    return;
  }
  TiffFileScopedHandle tif_h(TIFFOpen(filenames[page_nr].c_str(), "r"));
  if (!tif_h.get()) {
    write_error("Cannot open file ");
    write_error(filenames[page_nr].c_str());
    write_error(" for reading\n");
    cancelled_when_drawing = true;
    gtk_print_operation_cancel(print_operation_p);    
    return;
  }
  uint16 samples_per_pixel;
  uint16 bits_per_sample;
  TIFFGetField(tif_h, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetField(tif_h, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  if (samples_per_pixel != 1 || bits_per_sample != 1) {
    write_error("Error: file ");
    write_error(filenames[page_nr].c_str());
    write_error(" does not have 1 bit per pixel monochrome format\n");
    cancelled_when_drawing = true;
    gtk_print_operation_cancel(print_operation_p);    
    return;
  }
  // initial conditions OK - now process the tiff file

  uint32 height;
  uint32 width;
  TIFFGetField(tif_h, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tif_h, TIFFTAG_IMAGEWIDTH, &width);

  // get a stride acceptable for both libtiff and cairo
  const uint32 tiff_stride = TIFFScanlineSize(tif_h);
  const uint32 cr_stride = cr_get_stride(tiff_stride * 8);
  if (static_cast<int32>(cr_stride) == -1) {
    write_error("Cannot calculate cairo stride width in MonoTiffPrintManager::draw_page_impl()\n");
    cancelled_when_drawing = true;
    gtk_print_operation_cancel(print_operation_p);    
    return;
  }

  // provide a buffer for the image raster
  unsigned char* buf = static_cast<unsigned char*>(calloc(cr_stride * height, 1));
  if (!buf) {
    write_error("Memory allocation error in MonoTiffPrintManager::draw_page_impl()\n");
    exit(MEM_ERROR);
  }

  // fill up the image raster buffer
  uint32 row;
  unsigned char* tmp = buf;
  for (row = 0; row < height; ++row, tmp += cr_stride) {
    TIFFReadScanline(tif_h, tmp, row);
  }

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  TIFFReverseBits(buf, cr_stride * height);
#endif

  // now form the image surface
  CairoSurfaceScopedHandle surface_h(cairo_image_surface_create_for_data(buf, CAIRO_FORMAT_A1,
									 width, height,
									 cr_stride));

  if (cairo_surface_status(surface_h) != CAIRO_STATUS_SUCCESS) {
    write_error("Cairo surface error in MonoTiffPrintManager::draw_page_impl()\n");
    cancelled_when_drawing = true;
    gtk_print_operation_cancel(print_operation_p);
    free(buf);
  }
  else {
    // the gtk+ print system keeps a reference to the surface after
    // this method returns so set up a callback to free buf
    cairo_surface_set_user_data(surface_h, &destroy_key, buf, (cairo_destroy_func_t)free);

    // get the cairo context and scale it so that the pixel size of
    // the image pattern matches the pixel size of the print context
    double width_ratio = gtk_print_context_get_width(context_p)/width;
    double height_ratio = gtk_print_context_get_height(context_p)/height;
    double scale = (width_ratio < height_ratio) ? width_ratio : height_ratio;
    cairo_t* cairo_p = gtk_print_context_get_cairo_context(context_p);
    cairo_scale(cairo_p, scale, scale);

    // and paint the image in the source surface to the destination surface
    // maintained by the GtkPrint system
    CairoPatternScopedHandle pattern_h(cairo_pattern_create_for_surface(surface_h));
    // does setting the filter for the pattern have any effect in this usage and
    // is it necessary?
    cairo_pattern_set_filter(pattern_h, CAIRO_FILTER_BILINEAR);
    cairo_set_source(cairo_p, pattern_h);
    cairo_paint(cairo_p);
  }
}

#endif // GTK_CHECK_VERSION
