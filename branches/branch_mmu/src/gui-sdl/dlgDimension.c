/*
  Previous - dlgDimension.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/

const char DlgDimension_fileid[] = "Previous dlgDimension.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"


#define DLGND_ENABLE        4
#define DLGND_CUSTOMIZE     5
#define DLGND_COLOR         7
#define DLGND_MONOCHROME    8
#define DLGND_BOTH          9
#define DLGND_MEMSIZE       16
#define DLGND_BROWSE        21
#define DLGND_NAME          22
#define DLGND_EXIT          24

/* Variable strings */
char dimension_memory[16] = "64 MB";

/* Additional functions */
void print_nd_overview(void);
void update_monitor_selection(void);
void get_nd_default_values(void);

#define ENABLE_DUAL_SCREEN  0


static SGOBJ dimensiondlg[] =
{
    { SGBOX, 0, 0, 0,0, 58,26, NULL },
    { SGTEXT, 0, 0, 18,1, 21,1, "NeXTdimension options" },
    
    { SGBOX, 0, 0, 2,3, 26,10, NULL },
    { SGTEXT, 0, 0, 3,4, 19,1, "NeXTdimension board" },
    { SGCHECKBOX, 0, 0, 4,6, 9,1, "Enabled" },
    { SGBUTTON, 0, 0, 15,6, 11,1, "Customize" },
    
    { SGTEXT, 0, 0, 3,9, 7,1, "System display" },
    { SGRADIOBUT, 0, 0, 4,11, 7,1, "Color" },
#if ENABLE_DUAL_SCREEN
    { SGRADIOBUT, 0, 0, 12,11, 6,1, "Mono" },
    { SGRADIOBUT, 0, 0, 19,11, 6,1, "Both" },
#else
    { SGRADIOBUT, 0, 0, 12,11, 12,1, "Monochrome" },
    { SGTEXT, 0, 0, 19,11, 6,1, "" },
#endif

    { SGTEXT, 0, 0, 30,4, 13,1, "System overview:" },
    { SGTEXT, 0, 0, 30,6, 13,1, "CPU type:" },
    { SGTEXT, 0, 0, 44,6, 13,1, "i860 XR" },
    { SGTEXT, 0, 0, 30,7, 13,1, "CPU clock:" },
    { SGTEXT, 0, 0, 44,7, 13,1, "33 MHz" },
    { SGTEXT, 0, 0, 30,8, 13,1, "Memory size:" },
    { SGTEXT, 0, 0, 44,8, 13,1, dimension_memory },
    { SGTEXT, 0, 0, 30,9, 13,1, "NBIC:" },
    { SGTEXT, 0, 0, 44,9, 13,1, "present" },

    { SGBOX, 0, 0, 2,14, 54,5, NULL },
    { SGTEXT, 0, 0, 3,15, 22,1, "ROM for NeXTdimension:" },
    { SGBUTTON, 0, 0, 27,15, 8,1, "Browse" },
    { SGTEXT, 0, 0, 4,17, 50,1, NULL },

    { SGTEXT, 0, 0, 3,20, 52,1, "Note: NeXTdimension does not work with NeXTstations." },
    
    { SGBUTTON, SG_DEFAULT, 0, 18,23, 21,1, "Back to main menu" },

    { -1, 0, 0, 0,0, 0,0, NULL }
};


/* Function to set NeXTdimension memory options */
void Dialog_NDMemDlg(int *membank);

/* Function to print system overview */
void print_nd_overview(void) {
    sprintf(dimension_memory, "%i MB", Configuration_CheckDimensionMemory(ConfigureParams.Dimension.nMemoryBankSize));
    
    update_monitor_selection();
}

/* Function to select and unselect system options */
void update_monitor_selection(void) {

}

/* Function to get default values for each system */
void get_nd_default_values(void) {
    int i;
    
    for (i = 0; i < 4; i++) {
        ConfigureParams.Dimension.nMemoryBankSize[i] = 4;
    }
}


/*-----------------------------------------------------------------------*/
/**
  * Show and process the "System" dialog (specific to winUAE cpu).
  */
void Dialog_DimensionDlg(void)
{
    int but;
    char dlgname_ndrom[64];
 
 	SDLGui_CenterDlg(dimensiondlg);
 
 	/* Set up dialog from actual values: */
    
    if (ConfigureParams.Dimension.bEnabled) {
        dimensiondlg[DLGND_ENABLE].state |= SG_SELECTED;
    } else {
        dimensiondlg[DLGND_ENABLE].state &= ~SG_SELECTED;
    }
    
    dimensiondlg[DLGND_COLOR].state &= ~SG_SELECTED;
    dimensiondlg[DLGND_MONOCHROME].state &= ~SG_SELECTED;
    dimensiondlg[DLGND_BOTH].state &= ~SG_SELECTED;
    
    switch (ConfigureParams.Screen.nMonitorType) {
#if ENABLE_DUAL_SCREEN
        case MONITOR_TYPE_DUAL:
            dimensiondlg[DLGND_BOTH].state |= SG_SELECTED;
            break;
#endif
        case MONITOR_TYPE_CPU:
            dimensiondlg[DLGND_MONOCHROME].state |= SG_SELECTED;
            break;
        case MONITOR_TYPE_DIMENSION:
            dimensiondlg[DLGND_COLOR].state |= SG_SELECTED;
            break;
            
        default:
            break;
    }
    
    File_ShrinkName(dlgname_ndrom, ConfigureParams.Dimension.szRomFileName,
                        dimensiondlg[DLGND_NAME].w);
    dimensiondlg[DLGND_NAME].txt = dlgname_ndrom;

    /* System overview */
    print_nd_overview();
     
 		
 	/* Draw and process the dialog: */

    do
	{
        but = SDLGui_DoDialog(dimensiondlg, NULL);
        
        switch (but) {
            case DLGND_BROWSE:
                SDLGui_FileConfSelect(dlgname_ndrom,
                                      ConfigureParams.Dimension.szRomFileName,
                                      dimensiondlg[DLGND_NAME].w,
                                      false);
                break;
                
            case DLGND_CUSTOMIZE:
                Dialog_NDMemDlg(ConfigureParams.Dimension.nMemoryBankSize);
                print_nd_overview();
                break;
                
            default:
                break;
        }
    }
    while (but != DLGND_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog */
    ConfigureParams.Dimension.bEnabled = (dimensiondlg[DLGND_ENABLE].state&SG_SELECTED)?true:false;
    if (ConfigureParams.Dimension.bEnabled) {
        ConfigureParams.System.bNBIC = true;
    }
    
    if (dimensiondlg[DLGND_COLOR].state&SG_SELECTED) {
        ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_DIMENSION;
#if ENABLE_DUAL_SCREEN
    } else if (dimensiondlg[DLGND_BOTH].state&SG_SELECTED) {
        ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_DUAL;
#endif
    } else {
        ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_CPU;
    }
}


#define DLGNDMEM_BANK0_4MB     4
#define DLGNDMEM_BANK0_16MB    5
#define DLGNDMEM_BANK0_NONE    6

#define DLGNDMEM_BANK1_4MB     9
#define DLGNDMEM_BANK1_16MB    10
#define DLGNDMEM_BANK1_NONE    11

#define DLGNDMEM_BANK2_4MB     14
#define DLGNDMEM_BANK2_16MB    15
#define DLGNDMEM_BANK2_NONE    16

#define DLGNDMEM_BANK3_4MB     19
#define DLGNDMEM_BANK3_16MB    20
#define DLGNDMEM_BANK3_NONE    21

#define DLGNDMEM_DEFAULT       23
#define DLGNDMEM_EXIT          24


void Dialog_NDMemDraw(int *memorybank);
void Dialog_NDMemRead(int *memorybank);

static SGOBJ ndmemdlg[] =
{
    { SGBOX, 0, 0, 0,0, 55,18, NULL },
    { SGTEXT, 0, 0, 18,1, 19,1, "Custom memory setup" },
    
    { SGBOX, 0, 0, 2,4, 12,7, NULL },
    { SGTEXT, 0, 0, 3,5, 14,1, "Bank0" },
    { SGRADIOBUT, 0, 0, 4,7, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 4,8, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 4,9, 6,1, "none" },
    
    { SGBOX, 0, 0, 15,4, 12,7, NULL },
    { SGTEXT, 0, 0, 16,5, 14,1, "Bank1" },
    { SGRADIOBUT, 0, 0, 17,7, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 17,8, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 17,9, 6,1, "none" },
    
    { SGBOX, 0, 0, 28,4, 12,7, NULL },
    { SGTEXT, 0, 0, 29,5, 14,1, "Bank2" },
    { SGRADIOBUT, 0, 0, 30,7, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 30,8, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 30,9, 6,1, "none" },
    
    { SGBOX, 0, 0, 41,4, 12,7, NULL },
    { SGTEXT, 0, 0, 42,5, 14,1, "Bank3" },
    { SGRADIOBUT, 0, 0, 43,7, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 43,8, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 43,9, 6,1, "none" },
    
    { SGTEXT, 0, 0, 5,12, 14,1, "Bank0 must contain at least 4 MB of memory." },
    
    { SGBUTTON, 0, 0, 15,15, 10,1, "Defaults" },
    { SGBUTTON, SG_DEFAULT, 0, 30,15, 10,1, "OK" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the "Memory Advanced" dialog.
 */
void Dialog_NDMemDlg(int *membank) {
    
    int but;
    
    SDLGui_CenterDlg(ndmemdlg);
    
    /* Set up dialog from actual values: */
    Dialog_NDMemDraw(membank);
    
    /* Draw and process the dialog: */
    do
    {
        but = SDLGui_DoDialog(ndmemdlg, NULL);

        switch (but) {
            case DLGNDMEM_DEFAULT:
                membank[0] = membank[1] = membank[2] = membank[3] = 4;
                Configuration_CheckDimensionMemory(membank);
                Dialog_NDMemDraw(membank);
                break;
            default:
                break;
        }
    }
    while (but != DLGNDMEM_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog: */
    Dialog_NDMemRead(membank);
}


/* Function to set up the dialog from the actual values */
void Dialog_NDMemDraw(int *bank) {
    int i, j;
    for (i = 0; i<4; i++) {
        
        for (j = (DLGNDMEM_BANK0_4MB+(5*i)); j <= (DLGNDMEM_BANK0_NONE+(5*i)); j++)
        {
            ndmemdlg[j].state &= ~SG_SELECTED;
        }
        
        switch (bank[i])
        {
            case 0:
                ndmemdlg[DLGNDMEM_BANK0_NONE+(i*5)].state |= SG_SELECTED;
                break;
            case 4:
                ndmemdlg[DLGNDMEM_BANK0_4MB+(i*5)].state |= SG_SELECTED;
                break;
            case 16:
                ndmemdlg[DLGNDMEM_BANK0_16MB+(i*5)].state |= SG_SELECTED;
                break;
                
            default:
                break;
        }
    }
}


/* Function to read the actual values from the dialog */
void Dialog_NDMemRead(int *bank) {
    int i;
    for (i = 0; i<4; i++) {
        if (ndmemdlg[(DLGNDMEM_BANK0_4MB)+(i*5)].state & SG_SELECTED)
            bank[i] = 4;
        else if (ndmemdlg[(DLGNDMEM_BANK0_16MB)+(i*5)].state & SG_SELECTED)
            bank[i] = 16;
        else
            bank[i] = 0;
    }
}