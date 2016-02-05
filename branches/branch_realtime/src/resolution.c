/*
  Hatari - resolution.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  SDL resolution limitation and selection routines.
*/
const char Resolution_fileid[] = "Hatari resolution.c : " __DATE__ " " __TIME__;

#include <SDL.h>
#include "main.h"
#include "configuration.h"
#include "resolution.h"
#include "screen.h"

#define RESOLUTION_DEBUG 0

#if RESOLUTION_DEBUG
#define Dprintf(a) printf a
#else
#define Dprintf(a)
#endif

static int DesktopWidth, DesktopHeight;

/**
  * Initilizes resolution settings (gets current desktop
  * resolution, sets max Falcon/TT Videl zooming resolution).
  */
void Resolution_Init(void)
{
 	/* Needs to be called after SDL video and configuration
 	 * initialization, but before Hatari Screen init is called
 	 * for the first time!
 	 */
	fprintf(stderr,"FIXME: Resolution init!\n");
	DesktopWidth = 800;
	DesktopHeight = 600;

 	Dprintf(("Desktop resolution: %dx%d\n",DesktopWidth, DesktopHeight));
 	Dprintf(("Configured Max res: %dx%d\n",ConfigureParams.Screen.nMaxWidth,ConfigureParams.Screen.nMaxHeight));
}

/**
  * Get current desktop resolution
  */
void Resolution_GetDesktopSize(int *width, int *height)
{
 	*width = DesktopWidth;
 	*height = DesktopHeight;
}

/**
 * Search video mode size that best suits the given width/height/bpp
 * constraints and set them into given arguments.  With zeroed arguments,
 * return largest video mode.
 */
void Resolution_Search(int *width, int *height, int *bpp)
{
	fprintf(stderr,"FIXME: Resolution_Search\n");
	Resolution_GetDesktopSize(width, height);
}


/**
  * Set given width & height arguments to maximum size allowed in the
  * configuration, or if that's too large for the requested bit depth,
  * to the largest available video mode size.
 */
void Resolution_GetLimits(int *width, int *height, int *bpp)
{
	*width = *height = 0;
	/* constrain max size to what HW/SDL offers */
    Dprintf(("resolution: request limits for: %dx%dx%d\n", *width, *height, *bpp));
	Resolution_Search(width, height, bpp);

 	if (bInFullScreen) {
 		/* resolution change not allowed */
 		Dprintf(("resolution: limit to desktop size\n"));
 		Resolution_GetDesktopSize(width, height);
 		return;
 	}
}
