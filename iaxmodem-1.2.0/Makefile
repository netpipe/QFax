MODEMVER=	1.2.0
DSPVER=		spandsp-0.0.5-snapshot-20080725
IAXVER=		libiax2-0.2.3-SVN-20071223+

CFLAGS:=	-DMODEMVER=\"${MODEMVER}\" -DDSPVER=\"${DSPVER}\" -DIAXVER=\"${IAXVER}\" \
		-Wall -g -DSTATICLIBS -DUSE_UNIX98_PTY \
		-std=c99 -Ilib/libiax2/src -Ilib/spandsp/src \
		${CFLAGS}
OUR_CFLAGS=	-Wall -g -DSTATICLIBS -DUSE_UNIX98_PTY \
		-std=c99 -Ilib/libiax2/src -Ilib/spandsp/src
OBJS=		iaxmodem.o lib/spandsp/src/.libs/libspandsp.a lib/libiax2/src/.libs/libiax.a
LDFLAGS+=	-lm -lutil -ltiff

all: iaxmodem

iaxmodem: build-libiax build-libspandsp iaxmodem.o
	${CC} ${CFLAGS} ${OBJS} -o iaxmodem ${LDFLAGS}

iaxmodem.o: iaxmodem.c
	${CC} ${CFLAGS} -c iaxmodem.c

build-libiax:
	cd lib/libiax2 && ${MAKE}

build-libspandsp:
	cd lib/spandsp && ${MAKE}

clean:
	rm -f *.o && \
	cd lib/libiax2 && ${MAKE} clean && \
	cd ../../lib/spandsp && ${MAKE} clean

