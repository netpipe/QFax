## Process this file with automake to produce Makefile.in

SUBDIRS = utils

bin_PROGRAMS = efax-gtk

efax_gtk_SOURCES = main.cpp mainwindow.cpp tray_icon.cpp	      \
		   efax_controller.cpp settings.cpp socket_list.cpp   \
		   socket_notify.cpp fax_list.cpp addressbook.cpp     \
		   file_list.cpp logger.cpp redial_queue.cpp	      \
		   fax_list_manager.cpp dialogs.cpp helpfile.cpp      \
		   settings_help.cpp socket_server.cpp

noinst_HEADERS =   mainwindow.h dialogs.h fax_list.h                  \
	           fax_list_manager.h file_list.h efax_controller.h   \
                   addressbook.h settings.h settings_help.h           \
                   helpfile.h socket_server.h socket_list.h           \
                   socket_notify.h logger.h tray_icon.h		      \
		   redial_queue.h gpl.h prog_defs.h menu_icons.h      \
		   addressbook_icons.h settings_icons.h window_icon.h \
		   socket_list_icons.h file_list_icons.h	      \
		   fax_list_icons.h fax_list_manager_icons.h	      \
		   redial_queue_icons.h

AM_CPPFLAGS = -DDATADIR=\"$(datadir)\" -DRC_DIR=\"$(sysconfdir)\"

AM_CXXFLAGS = -D_XOPEN_SOURCE=600 @CXX_GTK_UTILS_CFLAGS@ @GTK_CFLAGS@ \
	      @GTHREAD_CFLAGS@ @GLIB_CFLAGS@ @X11_CFLAGS@ -I./utils

efax_gtk_LDADD = utils/libutils.a @CXX_GTK_UTILS_LIBS@ @GTK_LIBS@     \
		 @GTHREAD_LIBS@ @GLIB_LIBS@ @X11_LIBS@ @LIBINTL@ -ltiff
