#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "bmap.h"


/* NeXT bmap chip emulation */


uae_u32  NEXTbmap[16];

uae_u32 bmap_get(uae_u32 addr);
void bmap_put(uae_u32 addr, uae_u32 val);


#define BMAP_DATA_RW    0xD

#define BMAP_TP         0x90000000

uae_u32 bmap_lget(uaecptr addr) {
    uae_u32 l;
    
    if (addr&3) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return 0;
    }
    
    l = bmap_get(addr>>2);

    return l;
}

uae_u32 bmap_wget(uaecptr addr) {
    uae_u32 w;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return 0;
    }

    w = bmap_get(addr>>2);
    w >>= (2 - (addr&2)) * 8;
    w &= 0xFFFF;
    
    return w;
}

uae_u32 bmap_bget(uaecptr addr) {
    uae_u32 b;
    
    b = bmap_get(addr>>2);
    b >>= (3 - (addr&3)) * 8;
    b &= 0xFF;
    
    return b;
}

void bmap_lput(uaecptr addr, uae_u32 l) {
    if (addr&3) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return;
    }
    
    bmap_put(addr>>2, l);
}

void bmap_wput(uaecptr addr, uae_u32 w) {
    uae_u32 val;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return;
    }

    val = NEXTbmap[addr>>2];
    
    val &= ~(0xFFFF << ((2 - (addr&2)) * 8));
    val |= w << ((2 - (addr&2)) * 8);
    
    bmap_put(addr>>2, val);
}

void bmap_bput(uaecptr addr, uae_u32 b) {
    uae_u32 val;
    
    val = NEXTbmap[addr>>2];

    val &= ~(0xFF << ((3 - (addr&3)) * 8));
    val |= b << ((3 - (addr&3)) * 8);
    
    bmap_put(addr>>2, val);
}


uae_u32 bmap_get(uae_u32 bmap_reg) {
    uae_u32 val;
    
    switch (bmap_reg) {
        case BMAP_DATA_RW:
            /* This is for detecing thin wire ethernet.
             * It prevents from switching ethernet
             * transceiver to loopback mode.
             */
            val = NEXTbmap[BMAP_DATA_RW]|0x20000000;
            break;
            
        default:
            val = NEXTbmap[bmap_reg];
            break;
    }
    
    return val;
}

void bmap_put(uae_u32 bmap_reg, uae_u32 val) {
    switch (bmap_reg) {
        case BMAP_DATA_RW:
            if ((val&BMAP_TP) != (NEXTbmap[bmap_reg]&BMAP_TP)) {
                if ((val&BMAP_TP)==BMAP_TP) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to twisted pair ethernet.");
                } else if ((val&BMAP_TP)==0) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to thin ethernet.");
                }
            }
            break;
            
        default:
            break;
    }
    
    NEXTbmap[bmap_reg] = val;
}