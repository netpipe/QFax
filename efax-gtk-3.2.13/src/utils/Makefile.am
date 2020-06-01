## Process this file with automake to produce Makefile.in

noinst_LIBRARIES =	libutils.a

libutils_a_SOURCES =	selected_rows_handle.cpp     \
			utf8_utils.cpp               \
			toolbar_append_widget.cpp    \
			mono_tiff_print_manager.cpp

noinst_HEADERS =	icon_info_handle.h           \
			selected_rows_handle.h       \
			sem_sync.h utf8_utils.h      \
			tree_path_handle.h           \
			tree_row_reference_handle.h  \
			toolbar_append_widget.h      \
			mono_tiff_print_manager.h    \
			pango_layout_iter_handle.h   \
			tiff_handle.h cairo_handle.h

AM_CXXFLAGS = -D_XOPEN_SOURCE=600 @GTK_CFLAGS@ @GTHREAD_CFLAGS@ @CXX_GTK_UTILS_CFLAGS@ -I..

EXTRA_DIST = LGPL.TXT
