#include <stdio.h>
#include <unistd.h>
#include <vector>

#include "nfsd.h"
#include "RPCServer.h"
#include "TCPServerSocket.h"
#include "UDPServerSocket.h"
#include "PortmapProg.h"
#include "NFSProg.h"
#include "MountProg.h"
#include "BootparamProg.h"
#include "configuration.h"

static bool         g_bLogOn = true;
static CPortmapProg g_PortmapProg;
static CRPCServer   g_RPCServer;
char                nfsd_hostname[_SC_HOST_NAME_MAX];
char                nfsd_export_path[MAXPATHLEN];

int mapped_udp_portmap_port = 0;
int udp_mount_port          = 0;
int mapped_udp_nfs_port     = 0;

int mapped_tcp_portmap_port = 0;
int tcp_mount_port          = 0;
int mapped_tcp_nfs_port     = 0;

static std::vector<UDPServerSocket*> SERVER_UDP;
static std::vector<TCPServerSocket*> SERVER_TCP;

static void add_program(CRPCProg *pRPCProg, uint16_t port = 0) {
    UDPServerSocket* udp = new UDPServerSocket();
    TCPServerSocket*   tcp = new TCPServerSocket();
    
    tcp->SetListener(&g_RPCServer);
    udp->SetListener(&g_RPCServer);
    g_RPCServer.Set(pRPCProg->GetProgNum(), pRPCProg);

    if (tcp->Open(pRPCProg->GetProgNum(), port) && udp->Open(pRPCProg->GetProgNum(), port)) {
        printf("[NFSD] %s started\n", pRPCProg->GetName());
        pRPCProg->Init(tcp->GetPort(), udp->GetPort());
        g_PortmapProg.Add(pRPCProg);
        SERVER_TCP.push_back(tcp);
        SERVER_UDP.push_back(udp);
    } else {
        printf("[NFSD] %s start failed.\n", pRPCProg->GetName());
    }
}

static void printAbout(void) {
    printf("[NFSD] Network File System server\n");
    printf("[NFSD] Copyright (C) 2005 Ming-Yang Kao\n");
    printf("[NFSD] Edited in 2011 by ZeWaren\n");
    printf("[NFSD] Edited in 2013 by Alexander Schneider (Jankowfsky AG)\n");
    printf("[NFSD] Edited in 2014 2015 by Yann Schepens\n");
    printf("[NFSD] Edited in 2016 by Peter Philipp (Cando Image GmbH), Marc Harding\n");
    printf("[NFSD] Mostly rewritten in 2019 by Simon Schubiger for Previous NeXT emulator\n");
}

extern "C" void nfsd_start(void) {
    if(access(ConfigureParams.Ethernet.szNFSroot, F_OK | R_OK | W_OK) < 0) {
        printf("[NFSD] can not access directory '%s'. nfsd startup canceled.", ConfigureParams.Ethernet.szNFSroot);
        return;
    }
    
    gethostname(nfsd_hostname, sizeof(nfsd_hostname));
    printf("[NFSD] starting local NFS daemon on '%s', exporting '%s'\n", nfsd_hostname, ConfigureParams.Ethernet.szNFSroot);
    printAbout();
    
    static CNFSProg       NFSProg;
    static CMountProg     MountProg;
    static CBootparamProg BootparamProg;
    
	NFSProg.SetUserID(0, 0);
    strcpy(nfsd_export_path, ConfigureParams.Ethernet.szNFSroot);
	MountProg.Export(nfsd_export_path, nfsd_export_path);
	g_RPCServer.SetLogOn(g_bLogOn);

    add_program(&g_PortmapProg, PORTMAP_PORT);
    add_program(&NFSProg,       NFS_PORT);
    add_program(&MountProg);
    add_program(&BootparamProg);
}

static void cleanup(void) {
    for (int i = 0; i < SERVER_TCP.size(); i++) SERVER_TCP[i]->Close();
    for (int i = 0; i < SERVER_UDP.size(); i++) SERVER_UDP[i]->Close();
}

extern "C" void nfsd_cleanup(void) {
    cleanup();
}

extern "C" int nfsd_match_addr(uint32_t addr) {
    return addr == INADDR_NFSD || (addr & CTL_NET_MASK) == (INADDR_NFSD & CTL_NET_MASK);
}

extern "C" void nfsd_not_implemented(const char* file, int line) {
    printf("[NFSD] not implemented file:%s, line %d.", file, line);
    pause();
}
