# Makefile for efax

# Change the following to the name of your ANSI C compiler
# (normally gcc).

CC=gcc

# Compile/load options. Add -DNO_STRERROR to CFLAGS if _strerror
# is undefined

CFLAGS=
LDFLAGS=

# Change the following to the destination directories for
# binaries and man pages. Probably /usr/bin and /usr/man on
# Linux, /usr/local/{bin,man} on other systems.

BINDIR=/usr/bin
MANDIR=/usr/man

.c.o:
	$(CC) $(CFLAGS) -c $<

all:	efax efix

efax:	efax.o efaxlib.o efaxio.o efaxos.o efaxmsg.o
	$(CC) -o efax $(LDFLAGS) efax.o efaxlib.o efaxio.o efaxos.o efaxmsg.o
	strip efax

efix:	efix.o efaxlib.o efaxmsg.o
	$(CC) -o efix $(LDFLAGS) efix.o efaxlib.o efaxmsg.o
	strip efix

install:
	cp fax efax efix $(BINDIR)
	chmod 755 $(BINDIR)/fax $(BINDIR)/efax $(BINDIR)/efix
	cp fax.1 efax.1 efix.1 $(MANDIR)/man1
	chmod 644 $(MANDIR)/man1/fax.1 $(MANDIR)/man1/efax.1  \
		$(MANDIR)/man1/efix.1

clean:	
	rm -f efax efix efax.o efix.o efaxlib.o efaxio.o efaxos.o efaxmsg.o

efax.o:		efax.c    efaxmsg.h efaxlib.h efaxio.h efaxos.h
efaxio.o:	efaxio.c  efaxmsg.h           efaxio.h efaxos.h
efaxos.o:	efaxos.c  efaxmsg.h efaxlib.h          efaxos.h
efix.o:		efix.c    efaxmsg.h efaxlib.h           
efaxlib.o:	efaxlib.c efaxmsg.h efaxlib.h           
efaxmsg.o:	efaxmsg.c efaxmsg.h
