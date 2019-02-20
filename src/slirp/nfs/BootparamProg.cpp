//
//  BootparamProg.cpp
//  Previous
//
//  Created by Simon Schubiger on 18.01.19.
//

#include <string.h>
#include <sys/syslimits.h>

#include "nfsd.h"
#include "BootparamProg.h"

CBootparamProg::CBootparamProg() : CRPCProg(PROG_BOOTPARAM, 1, "bootparamd") {}

CBootparamProg::~CBootparamProg() {}

int CBootparamProg::Process(void) {
    static PPROC pf[] = {
        &CRPCProg::Null,
        (PPROC)&CBootparamProg::Whoami,
        (PPROC)&CBootparamProg::Getfile
    };
    
    if (m_param->proc >= sizeof(pf) / sizeof(PPROC))
        return PRC_NOTIMP;
    
    int result = (this->*pf[m_param->proc])();
    return result;
}

const int IP_ADDR_TYPE    = 1;

static void WriteInAddr(XDROutput* out, uint32_t inAddr) {
    out->Write(IP_ADDR_TYPE);
    out->Write(0xFF&(inAddr >> 24));
    out->Write(0xFF&(inAddr >> 16));
    out->Write(0xFF&(inAddr >> 8));
    out->Write(0xFF&(inAddr));
}

int CBootparamProg::Whoami(void) {
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
    strcpy(hostname, nfsd_hostname);
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
    WriteInAddr(m_out, INADDR_GATEWAY);
    return PRC_OK;
}

int CBootparamProg::Getfile(void) {
    XDRString client;
    XDRString key;
    m_in->Read(client);
    m_in->Read(key);
    bool  deletePath = false;
    char* path       = NULL;
    if(!(strcmp("root", key.Get())))
        path = nfsd_export_path;
    else if(!(strcmp("private", key.Get()))) {
        deletePath = true;
        const char* privateDir = "private";
        path = new char[strlen(nfsd_export_path) + strlen(privateDir) + 2];
        sprintf(path, "%s/%s", nfsd_export_path, privateDir);
    }
    if(path) {
        m_out->Write(_SC_HOST_NAME_MAX, nfsd_hostname);
        WriteInAddr(m_out, INADDR_NFSD);
        m_out->Write(PATH_MAX, path);
        if(deletePath) delete[] path;
        return PRC_OK;
    } else {
        Log("Unknown key: %s", key.Get());
        return PRC_FAIL;
    }
}
