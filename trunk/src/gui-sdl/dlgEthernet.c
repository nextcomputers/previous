/*
  Previous - dlgEthernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgEthernet_fileid[] = "Previous dlgEthernet.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"

Uint8 mac_addr[6];
char mac_addr_string[20] = "00:00:0f:00:00:00";

#if HAVE_PCAP
#define DLGENET_ENABLE      4
#define DLGENET_THIN        5
#define DLGENET_TWISTED     6

#define DLGENET_SLIRP       9
#define DLGENET_PCAP        10

#define DLGENET_MAC         14

#define DLGENET_EXIT        16

#define PCAP_INTERFACE_LEN  19

char pcap_interface[PCAP_INTERFACE_LEN] = "PCAP";


/* The Ethernet options dialog: */
static SGOBJ enetdlg[] =
{
    { SGBOX, 0, 0, 0,0, 51,23, NULL },
    { SGTEXT, 0, 0, 17,1, 16,1, "Ethernet options" },
    
    { SGBOX, 0, 0, 1,3, 24,9, NULL },
    { SGTEXT, 0, 0, 3,4, 15,1, "Guest interface" },
    { SGCHECKBOX, 0, 0, 3,6, 20,1, "Ethernet connected" },
    { SGRADIOBUT, 0, 0, 5,8, 11,1, "Thin wire" },
    { SGRADIOBUT, 0, 0, 5,10, 14,1, "Twisted pair" },
    
    { SGBOX, 0, 0, 26,3, 24,9, NULL },
    { SGTEXT, 0, 0, 28,4, 14,1, "Host interface" },
    { SGRADIOBUT, 0, 0, 29,6, 7,1, "SLiRP" },
    { SGRADIOBUT, 0, 0, 29,8, PCAP_INTERFACE_LEN,1, pcap_interface },
    
    { SGBOX, 0, 0, 1,13, 49,3, NULL },
    { SGTEXT, 0, 0, 3,14, 12,1, "MAC address:" },
    { SGTEXT, 0, 0, 16,14, 17,1, mac_addr_string },
    { SGBUTTON, 0, 0, 37,14, 10,1, "Select" },

    { SGTEXT, 0, 0, 4,17, 22,1, "Note: PCAP requires super user privileges." },
    
    { SGBUTTON, SG_DEFAULT, 0, 15,20, 21,1, "Back to main menu" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};
#else // !HAVE_PCAP
#define DLGENET_ENABLE      3
#define DLGENET_THIN        4
#define DLGENET_TWISTED     5
#define DLGENET_MAC         9
#define DLGENET_EXIT        10


/* The Ethernet options dialog: */
static SGOBJ enetdlg[] =
{
    { SGBOX, 0, 0, 0,0, 53,17, NULL },
    { SGTEXT, 0, 0, 18,1, 16,1, "Ethernet options" },
    
    { SGBOX, 0, 0, 1,3, 25,9, NULL },
    { SGCHECKBOX, 0, 0, 3,5, 20,1, "Ethernet connected" },
    { SGRADIOBUT, 0, 0, 5,7, 11,1, "Thin wire" },
    { SGRADIOBUT, 0, 0, 5,9, 14,1, "Twisted pair" },
    
    { SGBOX, 0, 0, 27,3, 25,9, NULL },
    { SGTEXT, 0, 0, 29,5, 12,1, "MAC address:" },
    { SGTEXT, 0, 0, 31,7, 17,1, mac_addr_string },
    { SGBUTTON, 0, 0, 42,5, 8,1, "Select" },
    
    { SGBUTTON, SG_DEFAULT, 0, 16,14, 21,1, "Back to main menu" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};
#endif


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Ethernet options dialog.
 */
void DlgEthernet_Main(void)
{
	int but;

	SDLGui_CenterDlg(enetdlg);

    /* Set up the dialog from actual values */
    
    enetdlg[DLGENET_ENABLE].state &= ~SG_SELECTED;
    enetdlg[DLGENET_THIN].state &= ~SG_SELECTED;
    enetdlg[DLGENET_TWISTED].state &= ~SG_SELECTED;

    if (ConfigureParams.Ethernet.bEthernetConnected) {
        enetdlg[DLGENET_ENABLE].state |= SG_SELECTED;
        if (ConfigureParams.Ethernet.bTwistedPair) {
            enetdlg[DLGENET_TWISTED].state |= SG_SELECTED;
        } else {
            enetdlg[DLGENET_THIN].state |= SG_SELECTED;
        }
    }
#if HAVE_PCAP
    enetdlg[DLGENET_SLIRP].state &= ~SG_SELECTED;
    enetdlg[DLGENET_PCAP].state &= ~SG_SELECTED;

    if (ConfigureParams.Ethernet.nHostInterface == ENET_PCAP) {
        enetdlg[DLGENET_PCAP].state |= SG_SELECTED;
        snprintf(pcap_interface, PCAP_INTERFACE_LEN, "PCAP: %s", ConfigureParams.Ethernet.szInterfaceName);
    } else {
        enetdlg[DLGENET_SLIRP].state |= SG_SELECTED;
        sprintf(pcap_interface, "PCAP");
    }
#endif
    
    /* Draw and process the dialog */
    
	do
	{
        DlgEthernetAdvancedGetMAC(mac_addr);
        
        sprintf(mac_addr_string, "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0],
                mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

		but = SDLGui_DoDialog(enetdlg, NULL);
		
		switch (but) {
			case DLGENET_ENABLE:
				if (enetdlg[DLGENET_ENABLE].state & SG_SELECTED) {
					if (ConfigureParams.Ethernet.bTwistedPair) {
						enetdlg[DLGENET_TWISTED].state |= SG_SELECTED;
					} else {
						enetdlg[DLGENET_THIN].state |= SG_SELECTED;
					}
				} else {
					enetdlg[DLGENET_THIN].state &= ~SG_SELECTED;
					enetdlg[DLGENET_TWISTED].state &= ~SG_SELECTED;
                }
				break;
			case DLGENET_THIN:
			case DLGENET_TWISTED:
				if (!(enetdlg[DLGENET_ENABLE].state & SG_SELECTED)) {
					enetdlg[DLGENET_ENABLE].state |= SG_SELECTED;
				}
				break;
#if HAVE_PCAP
            case DLGENET_PCAP:
                if (DlgEthernetAdvancedPCAP()) {
					snprintf(pcap_interface, PCAP_INTERFACE_LEN, "PCAP: %s", ConfigureParams.Ethernet.szInterfaceName);
                } else {
                    sprintf(pcap_interface, "PCAP");
                    enetdlg[DLGENET_PCAP].state &= ~SG_SELECTED;
                    enetdlg[DLGENET_SLIRP].state |= SG_SELECTED;
                }
                break;
            case DLGENET_SLIRP:
                sprintf(pcap_interface, "PCAP");
                break;
#endif
            case DLGENET_MAC:
                DlgEthernetAdvancedMAC(mac_addr);
                break;
                
			default:
				break;
		}
	}
    while (but != DLGENET_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog */
    ConfigureParams.Ethernet.bEthernetConnected = enetdlg[DLGENET_ENABLE].state & SG_SELECTED;
    ConfigureParams.Ethernet.bTwistedPair = enetdlg[DLGENET_TWISTED].state & SG_SELECTED;
#if HAVE_PCAP
    if (enetdlg[DLGENET_PCAP].state & SG_SELECTED) {
        ConfigureParams.Ethernet.nHostInterface = ENET_PCAP;
    } else {
        ConfigureParams.Ethernet.nHostInterface = ENET_SLIRP;
    }
#endif
}