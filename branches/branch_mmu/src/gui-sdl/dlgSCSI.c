/*
  Hatari - dlgHardDisk.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgHardDisk_fileid[] = "Hatari dlgHardDisk.c : " __DATE__ " " __TIME__;

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"


#define SCSIDLG_OFFSET      2
#define SCSIDLG_INTERVAL    6

#define SCSIDLG_LEFT        1
#define SCSIDLG_DEVTYPE     2
#define SCSIDLG_RIGHT       3
#define SCSIDLG_SELECT      4
#define SCSIDLG_NAME        5

#define GET_BUTTON(x)       (((x)-(SCSIDLG_OFFSET))%(SCSIDLG_INTERVAL))
#define GET_TARGET(x)       (((x)-(SCSIDLG_OFFSET))/(SCSIDLG_INTERVAL))
#define PUT_BUTTON(x,y)     (((x)*(SCSIDLG_INTERVAL))+(SCSIDLG_OFFSET)+(y))

#define SCSIDLG_EXIT        44


/* The SCSI dialog: */
static SGOBJ diskdlg[] =
{
    { SGBOX, 0, 0, 0,0, 64,29, NULL },
	{ SGTEXT, 0, 0, 27,1, 10,1, "SCSI disks" },

	{ SGTEXT, 0, 0, 2,3, 14,1, "SCSI Disk 0:" },
    { SGBUTTON, 0, 0, 36, 3, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 3, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 3, 3, 1, "\x03" },
	{ SGBUTTON, 0, 0, 54,3, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,4, 58,1, NULL },
    
    { SGTEXT, 0, 0, 2,6, 14,1, "SCSI Disk 1:" },
    { SGBUTTON, 0, 0, 36, 6, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 6, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 6, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,6, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,7, 58,1, NULL },
    
    { SGTEXT, 0, 0, 2,9, 14,1, "SCSI Disk 2:" },
    { SGBUTTON, 0, 0, 36, 9, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 9, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 9, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,9, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,10, 58,1, NULL },

    { SGTEXT, 0, 0, 2,12, 14,1, "SCSI Disk 3:" },
    { SGBUTTON, 0, 0, 36, 12, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 12, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 12, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,12, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,13, 58,1, NULL },

    { SGTEXT, 0, 0, 2,15, 14,1, "SCSI Disk 4:" },
    { SGBUTTON, 0, 0, 36, 15, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 15, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 15, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,15, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,16, 58,1, NULL },

    { SGTEXT, 0, 0, 2,18, 14,1, "SCSI Disk 5:" },
    { SGBUTTON, 0, 0, 36, 18, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 18, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 18, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,18, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,19, 58,1, NULL },

    { SGTEXT, 0, 0, 2,21, 14,1, "SCSI Disk 6:" },
    { SGBUTTON, 0, 0, 36, 21, 3, 1, "\x04" },
    { SGTEXT, 0, 0, 40, 21, 8, 1, NULL },
    { SGBUTTON, 0, 0, 49, 21, 3, 1, "\x03" },
    { SGBUTTON, 0, 0, 54,21, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,22, 58,1, NULL },

    { SGBUTTON, SG_DEFAULT, 0, 22,26, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


#define SCSIDLG_EJECT_WARNING   "WARNING: Don't eject manually if a guest system is running. Risk of data loss. Eject now?"
#define SCSIDLG_NODEV_NOTICE    "No image selected for harddisk drive. Ignoring drive."

/**
 * Let user browse given directory, set directory if one selected.
 * return false if none selected, otherwise return true.
 */
/*static bool DlgDisk_BrowseDir(char *dlgname, char *confname, int maxlen)
{
	char *str, *selname;

	selname = SDLGui_FileSelect(confname, NULL, false);
	if (selname)
	{
		strcpy(confname, selname);
		free(selname);

		str = strrchr(confname, PATHSEP);
		if (str != NULL)
			str[1] = 0;
		File_CleanFileName(confname);
		File_ShrinkName(dlgname, confname, maxlen);
		return true;
	}
	return false;
}*/


/* Draw device type selector */

void DlgSCSI_DrawDevtypeSelect(void) {
    int i;
    
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        switch (ConfigureParams.SCSI.target[i].nDeviceType) {
            case DEVTYPE_HARDDISK:
                diskdlg[PUT_BUTTON(i,SCSIDLG_DEVTYPE)].txt = "Harddisk";
                diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Select";
                break;
            case DEVTYPE_CD:
                diskdlg[PUT_BUTTON(i,SCSIDLG_DEVTYPE)].txt = " CD-ROM ";
                if (ConfigureParams.SCSI.target[i].bDiskInserted) {
                    diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Eject";
                } else {
                    diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Insert";
                }
                break;
            case DEVTYPE_FLOPPY:
                diskdlg[PUT_BUTTON(i,SCSIDLG_DEVTYPE)].txt = " Floppy ";
                diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Insert";
                if (ConfigureParams.SCSI.target[i].bDiskInserted) {
                    diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Eject";
                } else {
                    diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Insert";
                }
                break;
            case DEVTYPE_NONE:
                diskdlg[PUT_BUTTON(i,SCSIDLG_DEVTYPE)].txt = "        ";
                diskdlg[PUT_BUTTON(i,SCSIDLG_SELECT)].txt = "Select";
                break;
                
            default:
                diskdlg[PUT_BUTTON(i,SCSIDLG_DEVTYPE)].txt = " Error! ";
                break;
        }
    }
}

/**
 * Show and process the hard disk dialog.
 */
void DlgHardDisk_Main(void)
{
    int but;
    int i;
    char dlgname_scsi[ESP_MAX_DEVS][64];

	SDLGui_CenterDlg(diskdlg);

	/* Set up dialog to actual values: */
    DlgSCSI_DrawDevtypeSelect();

    /* SCSI hard disk image: */
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        if (ConfigureParams.SCSI.target[i].bDiskInserted) {
            File_ShrinkName(dlgname_scsi[i], ConfigureParams.SCSI.target[i].szImageName,
                            diskdlg[PUT_BUTTON(i,SCSIDLG_NAME)].w);
        } else {
            dlgname_scsi[i][0] = '\0';
        }
        diskdlg[PUT_BUTTON(i,SCSIDLG_NAME)].txt = dlgname_scsi[i];
    }
    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(diskdlg, NULL);
        
        switch (GET_BUTTON(but)) {
            case SCSIDLG_LEFT:
                ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType+=NUM_DEVTYPES-1;
                ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType%=NUM_DEVTYPES;
                DlgSCSI_DrawDevtypeSelect();
                break;
            case SCSIDLG_RIGHT:
                ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType++;
                ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType%=NUM_DEVTYPES;
                DlgSCSI_DrawDevtypeSelect();
                break;
            case SCSIDLG_SELECT:
                if ((ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType == DEVTYPE_CD ||
                     ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType == DEVTYPE_FLOPPY) &&
                    ConfigureParams.SCSI.target[GET_TARGET(but)].bDiskInserted) {
                    if (DlgAlert_Query(SCSIDLG_EJECT_WARNING)) {
                        ConfigureParams.SCSI.target[GET_TARGET(but)].bDiskInserted = false;
                        ConfigureParams.SCSI.target[GET_TARGET(but)].szImageName[0] = '\0';
                        diskdlg[PUT_BUTTON(GET_TARGET(but),SCSIDLG_NAME)].txt[0] = '\0';
                    }
                } else if (SDLGui_FileConfSelect(dlgname_scsi[GET_TARGET(but)],
                                                 ConfigureParams.SCSI.target[GET_TARGET(but)].szImageName,
                                                 diskdlg[PUT_BUTTON(GET_TARGET(but),SCSIDLG_NAME)].w, false)) {
                    ConfigureParams.SCSI.target[GET_TARGET(but)].bDiskInserted = true;
                    if (ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType == DEVTYPE_NONE) {
                        ConfigureParams.SCSI.target[GET_TARGET(but)].nDeviceType = DEVTYPE_HARDDISK;
                    }
                }
                DlgSCSI_DrawDevtypeSelect();
                break;

            default:
                break;
        }
	}
	while (but != SCSIDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Check for invalid combinations */
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        if (ConfigureParams.SCSI.target[i].nDeviceType==DEVTYPE_NONE) {
            ConfigureParams.SCSI.target[i].bDiskInserted = false;
            ConfigureParams.SCSI.target[i].szImageName[0] = '\0';
        }
        if (ConfigureParams.SCSI.target[i].nDeviceType==DEVTYPE_HARDDISK &&
            ConfigureParams.SCSI.target[i].bDiskInserted == false) {
            DlgAlert_Notice(SCSIDLG_NODEV_NOTICE);
            ConfigureParams.SCSI.target[i].nDeviceType = DEVTYPE_NONE;
        }
    }
}
