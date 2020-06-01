## Process this file with automake to produce Makefile.in

spooldir = @SPOOLDIR@
spool_SCRIPTS = efax-gtk-faxfilter
spool_PROGRAMS = efax-gtk-socket-client

efax_gtk_socket_client_SOURCES = efax-gtk-socket-client.cpp

install-data-hook:
	-chown lp $(spooldir) 2> /dev/null
	-chgrp lp $(spooldir) 2> /dev/null

EXTRA_DIST = efax-gtk-faxfilter
