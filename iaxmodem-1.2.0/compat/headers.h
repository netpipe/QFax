/* Solaris headers are a rat's nest to figure out...
   These should be getting included already, but due
   to some missing define such as _XOPEN_SOURCE or 
   something else they get missed.  Attempts at defining
   _XOPEN_SOURCE result in spandsp compile failure. */

/* should be in strings.h */
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);

/* should be in string.h */
extern char *strdup(const char *);

/* should be in stdlib.h */
extern int posix_openpt(int);
extern int  grantpt(int);
extern int  unlockpt(int);
extern char *ptsname(int);

/* should be in signal.h */
extern int kill(pid_t, int);
