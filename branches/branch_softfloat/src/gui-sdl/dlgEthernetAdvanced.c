/*
  Previous - dlgEthernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgEthernetAdvanced_fileid[] = "Previous dlgEthernetAdvanced.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"

#if HAVE_PCAP
#include <pcap.h>

#define DLGENETPCAP_CANCEL  3
#define DLGENETPCAP_SELECT  4
#define DLGENETPCAP_NEXT    5

#define PCAP_LIST_LEN       29

char pcap_list[PCAP_LIST_LEN] = "";

/* The Boot options dialog: */
static SGOBJ enetpcapdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 36,8, NULL },
	{ SGTEXT, 0, 0, 2,1, 31,1, "Select Host Ethernet Interface:" },
    { SGTEXT, 0, 0, 3,3, PCAP_LIST_LEN,1, pcap_list },
    
	{ SGBUTTON, 0, 0, 4,6, 8,1, "Cancel" },
    { SGBUTTON, SG_DEFAULT, 0, 14,6, 8,1, "Select" },
    { SGBUTTON, 0, 0, 24,6, 8,1, "Next" },

	{ -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the PCAP Ethernet options dialog.
 */
bool DlgEthernetAdvanced(void)
{
	int but;
    pcap_if_t *alldevs;
    pcap_if_t *dev;
    char name[FILENAME_MAX];
    char errbuf[PCAP_ERRBUF_SIZE];
    bool bNone;
    
	SDLGui_CenterDlg(enetpcapdlg);
    
    /* Set up the variables */
    if (pcap_findalldevs(&alldevs, errbuf) == -1)
    {
        printf("Error in pcap_findalldevs: %s\n", errbuf);
    }
    dev = alldevs;
    
    /* Draw and process the dialog */
    
	do
	{
        /* on windows use dev->description instead of dev->name */
#ifdef _WIN32
        if (dev && dev->name && dev->description) {
			strcpy(name, dev->description);
#else
        if (dev && dev->name) {
			strcpy(name, dev->name);
#endif
            File_ShrinkName(pcap_list, name, PCAP_LIST_LEN);
            bNone = false;
        } else {
            sprintf(pcap_list, "no interface found");
            bNone = true;
        }

		but = SDLGui_DoDialog(enetpcapdlg, NULL);
		
		switch (but) {
            case DLGENETPCAP_SELECT:
                if (!bNone) {
                    snprintf(ConfigureParams.Ethernet.szInterfaceName, FILENAME_MAX, "%s", dev->name);
                }
				break;
			case DLGENETPCAP_NEXT:
                if (!bNone) {
                    dev = dev->next;
                }
				break;
            case DLGENETPCAP_CANCEL:
                bNone = true;
                break;
				
			default:
				break;
		}
	}
    while (but != DLGENETPCAP_SELECT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram && !bNone);
    
    pcap_freealldevs(alldevs);
    
    return !bNone;
}
#endif
