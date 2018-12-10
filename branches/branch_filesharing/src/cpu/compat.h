/*
  Hatari - compat.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains all the includes and defines specific to windows (such as TCHAR)
  needed by WinUae CPU core. 
  The aim is to have minimum changes in WinUae CPU core for next updates
*/

#ifndef HATARI_COMPAT_H
#define HATARI_COMPAT_H

#include <stdbool.h>

#include "sysconfig.h"

/* this defione is here for newcpu.c compatibility.
 * In WinUae, it's defined in debug.h" */
#ifndef MAX_LINEWIDTH
#define MAX_LINEWIDTH 100
#endif

#ifndef REGPARAM
#define REGPARAM
#endif

#ifndef REGPARAM2
#define REGPARAM2
#endif

#ifndef REGPARAM3
#define REGPARAM3
#endif

#ifndef TCHAR
typedef char TCHAR;
#endif

#ifndef STATIC_INLINE
#define STATIC_INLINE static inline
#endif

#define _vsnprintf vsnprintf
#define _tcsncmp strncmp
#define _istspace isspace
#define _tcscmp strcmp
#define _tcslen strlen
#define _tcsstr strstr
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _stprintf sprintf

#define _vsntprintf printf

#define f_out fprintf

#endif
