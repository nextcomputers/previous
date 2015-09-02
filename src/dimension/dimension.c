#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "dimension.h"
#include "sysdeps.h"
#include "nd_mem.h"
#include "nd_devs.h"
#include "nd_nbic.h"

#if ENABLE_DIMENSION

/* NeXTdimension board and slot memory */
#define ND_BOARD_SIZE	0x10000000
#define ND_BOARD_MASK	0x0FFFFFFF
#define ND_BOARD_BITS   0xF0000000

#define ND_SLOT_SIZE	0x01000000
#define ND_SLOT_MASK	0x00FFFFFF
#define ND_SLOT_BITS    0x0F000000

#define ND_NBIC_SPACE   0xFFFFFFE8

/* NeXTdimension screen */
bool enable_dimension_screen = false;

/* NeXTdimension slot memory access */
Uint32 nd_slot_lget(Uint32 addr) {
    
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd_longget(addr);
    } else {
        return nd_nbic_lget(addr);
    }
}

Uint16 nd_slot_wget(Uint32 addr) {
    
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd_wordget(addr);
    } else {
        return nd_nbic_wget(addr);
    }
}

Uint8 nd_slot_bget(Uint32 addr) {
    
    addr |= ND_SLOT_BITS;

    if (addr<ND_NBIC_SPACE) {
        return nd_byteget(addr);
    } else {
        return nd_nbic_bget(addr);
    }
}

void nd_slot_lput(Uint32 addr, Uint32 l) {

    addr |= ND_SLOT_BITS;

    if (addr<ND_NBIC_SPACE) {
        nd_longput(addr, l);
    } else {
        nd_nbic_lput(addr, l);
    }
}

void nd_slot_wput(Uint32 addr, Uint16 w) {

    addr |= ND_SLOT_BITS;

    if (addr<ND_NBIC_SPACE) {
        nd_wordput(addr, w);
    } else {
        nd_nbic_wput(addr, w);
    }
}

void nd_slot_bput(Uint32 addr, Uint8 b) {
    
    addr |= ND_SLOT_BITS;

    if (addr<ND_NBIC_SPACE) {
        nd_byteput(addr, b);
    } else {
        nd_nbic_bput(addr, b);
    }
}

/* NeXTdimension board memory access */
Uint32 nd_board_lget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_longget(addr);
}

Uint16 nd_board_wget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_wordget(addr);
}

Uint8 nd_board_bget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_byteget(addr);
}

void nd_board_lput(Uint32 addr, Uint32 l) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr, l);
}

void nd_board_wput(Uint32 addr, Uint16 w) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr, w);
}

void nd_board_bput(Uint32 addr, Uint8 b) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr, b);
}


/* Reset function */

void dimension_reset(void) {
    nd_nbic_init();
    nd_memory_init();
}

#endif
