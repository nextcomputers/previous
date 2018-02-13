/* CMake config.h for Hatari */

/* Define if you have a PNG compatible library */
#cmakedefine HAVE_LIBPNG 1

/* Define if you have a PCAP compatible library */
#cmakedefine HAVE_PCAP 1

/* Define if you have a readline compatible library */
#cmakedefine HAVE_LIBREADLINE 1

/* Define if you have a X11 environment */
#cmakedefine HAVE_X11 1

/* Define to 1 if you have the `z' library (-lz). */
#cmakedefine HAVE_LIBZ 1

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H 1

/* Define to 1 if you have unix domain sockets */
#cmakedefine HAVE_UNIX_DOMAIN_SOCKETS 1

/* Define to 1 if you have the 'setenv' function. */
#cmakedefine HAVE_SETENV 1

/* Define to 1 if you have the `select' function. */
#cmakedefine HAVE_SELECT 1

/* Define to 1 if you have the 'gettimeofday' function. */
#cmakedefine HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the 'nanosleep' function. */
#cmakedefine HAVE_NANOSLEEP 1

/* Define to 1 if you have the 'alphasort' function. */
#cmakedefine HAVE_ALPHASORT 1

/* Define to 1 if you have the 'scandir' function. */
#cmakedefine HAVE_SCANDIR 1

/* Define to 1 if you have the 'strdup' function */
#cmakedefine HAVE_STRDUP 1


/* Relative path from bindir to datadir */
#define BIN2DATADIR "@BIN2DATADIR@"

/* Define to 1 to enable trace logs - undefine to slightly increase speed */
#cmakedefine ENABLE_TRACING 1
