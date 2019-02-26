//
//  BootparamProg.cpp
//  Previous
//
//  Created by Simon Schubiger on 18.01.19.
//

#include <string.h>
#include <sys/syslimits.h>
#include <netinet/in.h>

#include "nfsd.h"
#include "BootparamProg.h"

CBootparamProg::CBootparamProg() : CRPCProg(PROG_BOOTPARAM, 1, "bootparamd") {
    #define RPC_PROG_CLASS CBootparamProg
    SetProc(1, WHOAMI);
    SetProc(2, GETFILE);
}

CBootparamProg::~CBootparamProg() {}

const int IP_ADDR_TYPE    = 1;

static void WriteInAddr(XDROutput* out, uint32_t inAddr) {
    out->Write(IP_ADDR_TYPE);
    out->Write(0xFF&(inAddr >> 24));
    out->Write(0xFF&(inAddr >> 16));
    out->Write(0xFF&(inAddr >> 8));
    out->Write(0xFF&(inAddr));
}

extern "C" struct in_addr special_addr;

int CBootparamProg::ProcedureWHOAMI(void) {
    uint32_t address_type;
    uint8_t  net;
    uint8_t  host;
    uint8_t  lh;
    uint8_t  impno;
    
    m_in->Read(&address_type);
    switch (address_type) {
        case IP_ADDR_TYPE:
            m_in->Read(&net);
            m_in->Read(&host);
            m_in->Read(&lh);
            m_in->Read(&impno);
            break;
        default:
            return PRC_FAIL;
    }
    char hostname[_SC_HOST_NAME_MAX];
    strcpy(hostname, NAME_HOST);
    char* client = hostname;
    char* domain = hostname;
    while(*++domain) {
        if(*domain == '.') {
            *domain = '\0';
            domain++;
            break;
        }
    }
    m_out->Write(_SC_HOST_NAME_MAX, client);
    m_out->Write(_SC_HOST_NAME_MAX, domain);
    WriteInAddr(m_out, ntohl(special_addr.s_addr) | CTL_GATEWAY);
    return PRC_OK;
}

const char* ROOT    = "/";
const char* PRIVATE = "/private";

int CBootparamProg::ProcedureGETFILE(void) {
    XDRString client;
    XDRString key;
    m_in->Read(client);
    m_in->Read(key);
    bool  deletePath = false;
    const char* path = NULL;
    if(!(strcmp("root", key.Get())))
        path = ROOT;
    else if(!(strcmp("private", key.Get()))) {
        path = PRIVATE;
    }
    if(path) {
        m_out->Write(_SC_HOST_NAME_MAX, nfsd_hostname);
        WriteInAddr(m_out, ntohl(special_addr.s_addr) | CTL_NFSD);
        m_out->Write(PATH_MAX, path);
        if(deletePath) delete[] path;
        return PRC_OK;
    } else {
        Log("Unknown key: %s", key.Get());
        return PRC_FAIL;
    }
}
