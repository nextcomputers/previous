#ifndef _NFSD_H_
#define _NFSD_H_

#include <stdint.h>
#include <unistd.h>
#include "ctl.h"

#include "RPCProg.h"

static const uint32_t INADDR_NFSD    = CTL_BASE | CTL_NFSD;    // 10.0.2.254
static const uint32_t INADDR_GATEWAY = CTL_BASE | CTL_GATEWAY; // 10.0.2.1

extern int  mapped_udp_portmap_port;
extern int  udp_mount_port;
extern int  mapped_udp_nfs_port;

extern int  mapped_tcp_portmap_port;
extern int  tcp_mount_port;
extern int  mapped_tcp_nfs_port;

extern char nfsd_export_path[MAXPATHLEN];

#define PORTMAP_PORT 111
#define NFS_PORT     2049

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
