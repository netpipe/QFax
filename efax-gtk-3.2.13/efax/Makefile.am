## Process this file with automake to produce Makefile.in

bin_PROGRAMS = efax-0.9a efix-0.9a

efax_0_9a_SOURCES = efax.c efaxio.c efaxos.c efaxlib.c efaxmsg.c

efix_0_9a_SOURCES = efix.c efaxlib.c efaxmsg.c

noinst_HEADERS = efaxlib.h efaxio.h efaxos.h efaxmsg.h

dist_man_MANS = efax.1 efix.1

AM_CPPFLAGS = -DDATADIR=\"$(datadir)\"

AM_CFLAGS = @GLIB_CFLAGS@

efax_0_9a_LDADD = @GLIB_LIBS@

efix_0_9a_LDADD = @GLIB_LIBS@

EXTRA_DIST = PATCHES Makefile.orig efax.c.orig efix.c.orig efaxlib.c.orig efaxmsg.c.orig efaxio.c.orig efaxos.c.orig fax efax.1.orig
