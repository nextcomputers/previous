#ifndef _NFSD_H_
#define _NFSD_H_

#include <stdint.h>
#include <unistd.h>
#include "ctl.h"

#include "RPCProg.h"

typedef struct {
    int portmap;
    int nfs;
    int mount;
    int dns;
} nfsd_mapped_ports;

typedef struct {
    nfsd_mapped_ports tcp;
    nfsd_mapped_ports udp;
} nfsd_NAT;

extern nfsd_NAT nfsd_ports;

extern char nfsd_export_path[MAXPATHLEN];

#define PORT_DNS      53
#define PORT_PORTMAP  111
#define PORT_NFS      2049

enum {
    PROG_PORTMAP   = 100000,
    PROG_NFS       = 100003,
    PROG_MOUNT     = 100005,
    PROG_BOOTPARAM = 100026,
    PROG_VDNS      = 200053, // virtual DNS
};

extern char nfsd_hostname[_SC_HOST_NAME_MAX];

#ifdef __cplusplus
extern "C" {
#endif
    void nfsd_start(void);
    void nfsd_cleanup(void);
    int  nfsd_match_addr(uint32_t addr);
    void nfsd_not_implemented(const char* file, int line);
#ifdef __cplusplus
}
#endif

#define NFSD_NOTIMPL nfsd_not_implemented(__FILE__, __LINE__);

#endif
