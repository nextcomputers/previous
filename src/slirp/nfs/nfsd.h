#ifndef _NFSD_H_
#define _NFSD_H_

#include <stdint.h>
#include <unistd.h>
#include "ctl.h"

#include "RPCProg.h"

extern int  mapped_udp_portmap_port;
extern int  udp_mount_port;
extern int  mapped_udp_nfs_port;

extern int  mapped_tcp_portmap_port;
extern int  tcp_mount_port;
extern int  mapped_tcp_nfs_port;

extern char nfsd_export_path[MAXPATHLEN];

#define PORT_PORTMAP  111
#define PORT_NFS      2049

enum {
    PROG_PORTMAP   = 100000,
    PROG_NFS       = 100003,
    PROG_MOUNT     = 100005,
    PROG_BOOTPARAM = 100026,
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
