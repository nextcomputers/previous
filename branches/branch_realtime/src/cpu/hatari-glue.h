/*
  Hatari - hatari-glue.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_GLUE_H
#define HATARI_GLUE_H

#include "sysdeps.h"
#include "options_cpu.h"

extern int Init680x0(void);
extern void Exit680x0(void);
extern int intlev (void);

#define write_log printf


#endif /* HATARI_GLUE_H */
