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


char pcap_list[20] = "";

/* The Boot options dialog: */
static SGOBJ enetpcapdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 36,8, NULL },
	{ SGTEXT, 0, 0, 2,1, 31,1, "Select Host Ethernet Interface:" },
    { SGTEXT, 0, 0, 3,3, 12,1, pcap_list },
    
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
    char errbuf[PCAP_ERRBUF_SIZE];
    bool bDone;
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
        if (dev == NULL) {
            sprintf(pcap_list, "no interface found");
            bNone = true;
        } else {
            if (strlen(dev->name) < 12) {
                sprintf(pcap_list, "%s", dev->name);
            } else {
                sprintf(pcap_list, "string is too long");
            }
            bNone = false;
        }
        bDone = false;

		but = SDLGui_DoDialog(enetpcapdlg, NULL);
		
		switch (but) {
            case DLGENETPCAP_SELECT:
                if (!bNone) {
                    if (strlen(dev->name) < 12) {
                        sprintf(ConfigureParams.Ethernet.szInterfaceName, "%s", dev->name);
                    } else {
                        printf("Error: Ethernet interface string too long\n");
                        bNone = true;
                    }
                }
                bDone = true;
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
    while (but != DLGENETPCAP_CANCEL && but != DLGENETPCAP_SELECT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram && !bNone);
    
    pcap_freealldevs(alldevs);
    
    return !bNone;
}
#endif
