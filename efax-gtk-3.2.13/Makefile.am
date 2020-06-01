## Process this file with automake to produce Makefile.in

SUBDIRS = src efax po efax-gtk-faxfilter

desktopdir = $(datadir)/applications
desktop_DATA = efax-gtk.desktop

icondir = $(datadir)/pixmaps
icon_DATA = efax-gtk.png

sysconf_DATA = efax-gtkrc

dist_man_MANS = efax-gtk.1

release: dist
	mv @PACKAGE@-@VERSION@.tar.gz @PACKAGE@-@VERSION@.src.tgz 

snapshot: dist
	mv @PACKAGE@-@VERSION@.tar.gz @PACKAGE@3-`date +%y%m%d`.src.tgz 

EXTRA_DIST = m4/ChangeLog m4/*.m4 efax-gtkrc efax-gtk.spec config.rpath \
             BUGS efax-gtk.desktop efax-gtk.png mail_fax print_fax

ACLOCAL_AMFLAGS = -I m4
