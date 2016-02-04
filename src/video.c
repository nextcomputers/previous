/*
  video.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/
const char Video_fileid[] = "Previous video.c : " __DATE__ " " __TIME__;

#include <stdbool.h>
#include <SDL_endian.h>

#include "configuration.h"
#include "cycInt.h"
#include "ioMem.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "screen.h"
#include "shortcut.h"
#include "nextMemory.h"
#include "video.h"
#include "avi_record.h"
#include "dma.h"
#include "sysReg.h"
#include "tmc.h"


/*--------------------------------------------------------------*/
/* Local functions prototypes                                   */
/*--------------------------------------------------------------*/



static void	Video_DrawScreen(void);

/*-----------------------------------------------------------------------*/
/**
 * Save/Restore snapshot of local variables('MemorySnapShot_Store' handles type)
 */
void Video_MemorySnapShot_Capture(bool bSave)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Reset video chip
 */
void Video_Reset(void)
{
	Video_StartInterrupts(0);
}


/*-----------------------------------------------------------------------*/
/**
 * Reset the GLUE chip responsible for generating the H/V sync signals.
 * When the 68000 RESET instruction is called, frequency and resolution
 * should be reset to 0.
 */
void Video_Reset_Glue(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Clear raster line table to store changes in palette/resolution on a line
 * basic. Called once on VBL interrupt.
 */
void Video_SetScreenRasters(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Draw screen (either with ST/STE shifter drawing functions or with
 * Videl drawing functions)
 */
static void Video_DrawScreen(void)
{
Screen_Draw();
}


#define NEXT_VBL_FREQ 68

/**
 * Start VBL interrupt
 */
void Video_StartInterrupts ( int PendingCyclesOver ) {
    CycInt_AddRelativeInterruptUs((1000*1000)/NEXT_VBL_FREQ, INTERRUPT_VIDEO_VBL);
}


/**
 * Generate vertical video retrace interrupt
 */
void Video_InterruptHandler(void)
{
	if (ConfigureParams.System.bTurbo) {
		tmc_video_interrupt();
	} else if (ConfigureParams.System.bColor) {
        color_video_interrupt();
    } else {
        dma_video_interrupt();
    }
}


/*-----------------------------------------------------------------------*/
/**
 * VBL interrupt : set new interrupts, draw screen, generate sound,
 * reset counters, ...
 */
void Video_InterruptHandler_VBL ( void )
{
	CycInt_AcknowledgeInterrupt();
    host_blank(0, MAIN_DISPLAY, true);
	Video_DrawScreen();
    Video_InterruptHandler();
    CycInt_AddRelativeInterruptUs((1000*1000)/NEXT_VBL_FREQ, INTERRUPT_VIDEO_VBL);
}




