#include <ctype.h>		/* ANSI C */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h> 
#include <time.h>
#include <limits.h>

#include "../config.h"            /* For NLS */
#ifdef ENABLE_NLS
#include <libintl.h>
#include <glib.h>
#endif

#include "efaxmsg.h"

// there can be certain circumstances where PIPE_BUF is not
// defined in <limits.h>.  If so, just define the minimum
// required by POSIX
#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

#define MAXTSTAMP 80		/* maximum length of a time stamp */

#if PIPE_BUF>8192
#define MAXMSGBUF 4096		/* maximum status/error message bytes held */
#else
#define MAXMSGBUF PIPE_BUF/2
#endif

#define NLOG 2

char *verb[NLOG] = { "ewin", "" } ;
char *argv0 = "" ;

int nxtoptind = 1 ;		/* for communication with nextopt() */
char *nxtoptarg ;

int use_utf8 = 0 ;
int line_buffered = 0 ;

/* For systems without strerror(3) */

#ifdef NO_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];

extern char *strerror( int i )       
{
  return ( i >= 0 && i < sys_nerr ) ? sys_errlist[i] : "Unknown Error" ;
}
#endif


/* Provide dummy gettext() function if there is no internationalisation support */

#ifndef ENABLE_NLS
static const char *gettext ( const char *text )
{
  return text;
}
#endif

/* Print time stamp. */

time_t tstamp ( time_t last, FILE *f )
{
  time_t now ;
  char tbuf [ MAXTSTAMP ] ;

#ifdef ENABLE_NLS
  gsize written = 0 ;
  char *message ;
#endif

  now = time ( 0 ) ;

  strftime ( tbuf, MAXTSTAMP,  ( now - last > 600 ) ? "%c" : "%H:%M:%S",
	     localtime( &now ) ) ;
#ifdef ENABLE_NLS
  if ( use_utf8 ) {
    if ( g_utf8_validate ( tbuf, -1, NULL ) ) {
      fputs ( tbuf, f ) ;
    } else {
      message = g_locale_to_utf8 ( tbuf, -1, NULL, &written, NULL ) ;
      if ( message ) {
	fputs ( message, f ) ;
	g_free ( message ) ;
      }
    }
  }
  else fputs ( tbuf, f ) ;
#else
  fputs ( tbuf, f ) ;
#endif
  return now ;
}


/* Return string corresponding to character c. */

char *cname ( uchar c ) 
{
#define CNAMEFMT "<0x%02x>"
#define CNAMELEN 6+1
  static char *cnametab [ 256 ] = { /* character names */
  "<NUL>","<SOH>","<STX>","<ETX>", "<EOT>","<ENQ>","<ACK>","<BEL>",
  "<BS>", "<HT>", "<LF>", "<VT>",  "<FF>", "<CR>", "<SO>", "<SI>", 
  "<DLE>","<XON>","<DC2>","<XOFF>","<DC4>","<NAK>","<SYN>","<ETB>",
  "<CAN>","<EM>", "<SUB>","<ESC>", "<FS>", "<GS>", "<RS>", "<US>" } ;
  static char names[ (127-32)*2 + 129*(CNAMELEN) ] ;
  char *p=names ;
  static int i=0 ;
    
  if ( ! i ) {
    for ( i=32 ; i<256 ; i++ ) {
      cnametab [ i ] = p ;
      sprintf ( p, i<127 ? "%c" : CNAMEFMT, i ) ;
      p += strlen ( p ) + 1 ;
    }
  }

  return cnametab [ c ] ;
} 

/* Print a message with a variable number of printf()-type
   arguments if the first character appears in the global
   verb[ose] string.  Other leading characters and digits do
   additional actions: + allows the message to be continued on
   the same line, '-' buffers the message instead of printing it,
   E, and W expand into strings, S prints the error message for
   the most recent system error, a digit sets the return value, a
   space ends prefix but isn't printed.  Returns 0 if no prefix
   digit. */

enum  msgflags { E=0x01, W=0x02, S=0x04, NOFLSH=0x08, NOLF=0x10 } ;

int msg ( char *fmt, ... ) 
{ 
  static int init=0 ;
  static FILE *logfile [ NLOG ] ;
  static char msgbuf [ NLOG ] [ MAXMSGBUF ] ;
  static time_t logtime [ NLOG ] = { 0, 0 } ;
  static int atcol1 [ NLOG ] = { 1, 1 } ;
  
  int err=0, i, flags=0 ;
  char *p;

  va_list ap ;

#ifdef ENABLE_NLS
  gsize written = 0 ;
  char *message ;
#endif

  if ( ! init ) {
    logfile[0] = stderr ;
    logfile[1] = stdout ;
    for ( i=0 ; i<NLOG ; i++ ) {

      /* have stderr line buffered so that shared error reporting
	 mechanisms, such as that used by efax-gtk, do not cause
	 partial writes of a UTF-8 character (in the unlikely event
	 that the buffer for stderr maintained by this program would
	 otherwise fill up before being flushed) - we need UTF-8
	 validation checking at the read end anyway, but with line
	 buffering of error messages we can have more than one process
	 per pipe */
      if ( !i && setvbuf ( logfile[i], msgbuf[i], _IOLBF, MAXMSGBUF ) )
	setvbuf ( logfile[i], msgbuf[i], _IOFBF, MAXMSGBUF ) ; /* fallback */

      /* stdout is busy, so unless the -n option is used, applications
	 monitoring stdout must be able to handle partial UTF-8
	 character writes from efax when the buffer flushes on filling
	 up (which also entails having one pipe dedicated to efax as
	 well as validation checking at the read end) - adopt full
	 buffering for stdout unless -n option used for terminal
	 output */
      if ( i && ( !line_buffered || setvbuf ( logfile[i], msgbuf[i], _IOLBF, MAXMSGBUF ) ) )
	   setvbuf ( logfile[i], msgbuf[i], _IOFBF, MAXMSGBUF ) ;

    }
    cname ( 0 ) ;
    init = 1 ;
  }
  
  for ( i=0 ; i<NLOG ; i++ ) {
    va_start ( ap, fmt ) ;

    for ( p=fmt ; *p ; p++ ) {
      switch ( *p ) {
      case ' ': p++ ; goto print ;
      case 'E': flags |= E ; break ;
      case 'W': flags |= W ; break ;
      case 'S': flags |= S ; break ;
      case '+': flags |= NOLF ; break ;
      case '-': flags |= NOFLSH ; break ;
      default: 
	if ( isdigit ( (unsigned char) *p ) ) {
	  err = *p - '0' ; 
	} else if ( ! isupper ( (unsigned char) *p ) ) {
	  goto print ;
	}
      }
    }
      
    print:

    if ( strchr ( verb[i], tolower ( (unsigned char) *fmt ) ) ) {
      
      if ( atcol1[i] ) {
	fprintf ( logfile[i], "%s: ", argv0 ) ;
	logtime[i] = tstamp ( logtime[i], logfile[i] ) ; 
	fputs ( ( flags & E ) ? gettext ( " Error: " ) : 
		( flags & W ) ? gettext ( " Warning: " ) : 
		" ",
		logfile[i] ) ;
      }
      vfprintf( logfile[i], p, ap ) ;
      if ( flags & S ) {
#ifdef ENABLE_NLS
	message = strerror ( errno ) ;
	if ( use_utf8 ) {
	  if ( g_utf8_validate ( message, -1, NULL ) ) {
	    fprintf ( logfile[i], " %s", message ) ;
	  } else {
	    message = g_locale_to_utf8 ( message, -1, NULL, &written, NULL ) ;
	    if ( message ) {
	      fprintf ( logfile[i], " %s", message ) ;
	      g_free ( message ) ;
	    }
	  }
	}
	else fprintf ( logfile[i], " %s", message ) ;
#else
	fprintf ( logfile[i], " %s", strerror ( errno ) ) ;
#endif
      }
      if ( ! ( flags & NOLF ) ) fputs ( "\n", logfile[i] ) ;
      atcol1[i] = flags & NOLF ? 0 : 1 ;
      if ( ! ( flags & NOFLSH ) ) fflush ( logfile[i] ) ;
      
    }
    
    va_end ( ap ) ;
  }
  
  return err ;
}


/* Simple (one option per argument) version of getopt(3). */

int nextopt( int argc, char **argv, char *args )
{
  char *a, *p ;

  if ( nxtoptind >= argc || *(a = argv[nxtoptind]) != '-' ) return -1 ;
  nxtoptind++ ;

  if ( ! *(a+1) || ( ( p = strchr ( args, *(a+1) ) ) == 0 ) )
    return msg ( "Eunknown option (%s)", a ), '?' ; 

  if ( *(p+1) != ':' ) nxtoptarg = 0 ;
  else
    if ( *(a+2) ) nxtoptarg = a+2 ;
    else
      if ( nxtoptind >= argc ) return msg ( "Eno argument for (%s)", a ), '?' ;
      else nxtoptarg = argv [ nxtoptind++ ] ;
  return *(a+1) ;
}

