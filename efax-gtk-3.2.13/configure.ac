dnl Process this file with autoconf to produce a configure script.
AC_INIT([efax-gtk], [3.2.13])

AM_INIT_AUTOMAKE([1.11])

AC_CONFIG_HEADERS([config.h])

dnl use '--disable-silent-rules' or 'make V=1 ...' to override this
AM_SILENT_RULES([yes])

dnl check for working c and c++ compilers
AC_CHECK_COMPILERS
dnl Checks for programs.
AC_PROG_INSTALL

dnl Checks for libraries
AC_CHECK_GTK_VERSION
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.10.0)
PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.0.0)
AC_CHECK_X11_XLIB_H
AC_CHECK_HEADERS([tiffio.h],[],[AC_MSG_ERROR([tiffio.h in libtiff not found], 1)])
dnl this is probably best not checked for
dnl AC_CHECK_GTHREAD_HAS_PTHREADS

dnl Checks for typedefs, structures, and compiler characteristics.
AC_LANG_CPLUSPLUS dnl switch to C++
AC_TYPE_SIZE_T
AC_CHECK_TYPE([ssize_t], [:], AC_MSG_ERROR([POSIX ssize_t not available]))
AC_CHECK_SOCKLEN_T
AC_CHECK_IN_ADDR_T
dnl wchar_t is used in internal version of c++-gtk-utils
AC_CHECK_SIZEOF([wchar_t])

dnl set up gettext
AM_GNU_GETTEXT([external])

dnl Checks for library functions.
AC_CHECK_MKSTEMP
AC_CHECK_HAVE_IOS_NOCREATE
AC_CHECK_HAVE_STREAM_IMBUE
AC_CHECK_HEADERS([ ostream istream iostream sstream vector string fstream \
		   ios utility algorithm cstdlib cstring list iomanip \
		   set map memory ctime cmath functional iterator locale \
		   exception ],
                 [],[AC_MSG_ERROR([Standard C++ headers not found], 1)])

AC_CHECK_HEADERS([ unistd.h stdlib.h limits.h signal.h dirent.h fcntl.h \
		   errno.h netdb.h pthread.h sys/types.h sys/stat.h \
		   sys/wait.h sys/socket.h netinet/in.h arpa/inet.h ],
                 [:],[AC_MSG_ERROR([Standard POSIX headers not found], 1)])

dnl now check out the install directory
AC_INSTALL_DIRS

dnl enable source in subdirectories to be compiled
AC_PROG_RANLIB

dnl define MAKE for subdirectories
AC_PROG_MAKE_SET

AC_CONFIG_FILES([ Makefile src/Makefile efax/Makefile po/Makefile.in \
		  efax-gtk-faxfilter/Makefile src/utils/Makefile     \
		  efax-gtk.spec ])

AC_OUTPUT

AC_CLOSING_MESSAGE
