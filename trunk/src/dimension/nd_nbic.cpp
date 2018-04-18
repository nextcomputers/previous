#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "sysReg.h"
#include "nd_nbic.hpp"
#include "dimension.hpp"
#include "log.h"

/* NeXTdimention NBIC */
#define ND_NBIC_INTR    0x80

NBIC::NBIC(int slot, int id) : slot(slot), id(id) {}

Uint8 NBIC::read(int addr) {
    switch(addr&0x1F) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC bus error read at %08X", slot,addr);
            M68000_BusError(addr, 1);
            return 0;

        case 0x08:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC Interrupt status read at %08X", slot,addr);
            return intstatus;
        case 0x0C:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC Interrupt mask read at %08X", slot,addr);
            return intmask;

        case 0x10:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC ID (byte 0) read at %08X", slot, addr);
            return (id>>24);
        case 0x14:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC ID (byte 1) read at %08X", slot,addr);
            return (id>>16);
        case 0x18:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC ID (byte 2) read at %08X", slot,addr);
            return (id>>8);
        case 0x1C:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC ID (byte 3) read at %08X", slot,addr);
            return id;
        default:
            Log_Printf(ND_LOG_IO_RD, "[ND] Slot %i: NBIC zero read at %08X", slot,addr);
            return 0;
    }
}

void  NBIC::write(int addr, Uint8 val) {
    switch(addr&0x1F) {
        case 0x0C:
            Log_Printf(ND_LOG_IO_WR, "[ND] Slot %i: NBIC Interrupt mask write %02X at %08X", slot,val,addr);
            intmask = val;
            if(val & ND_NBIC_INTR)
                remInterMask |= 1 << slot;
            else
                remInterMask &= ~(1 << slot);
            break;
        case 0x0D:
        case 0x0E:
        case 0x0F:
            Log_Printf(ND_LOG_IO_WR, "[ND] Slot %i: NBIC zero write %02X at %08X", slot,val,addr);
            break;
        default:
            Log_Printf(ND_LOG_IO_WR, "[ND] Slot %i: NBIC bus error write at %08X", slot,addr);
            M68000_BusError(addr, 0);
            break;
    }
}

/* NeXTdimension NBIC access */
Uint32 NBIC::lget(Uint32 addr) {
    Uint32 val = 0;
    
    if (addr&3) {
        Log_Printf(LOG_WARN, "[ND] Slot %i: NBIC Unaligned access at %08X.", slot, addr);
        abort();
    }
    val  = read(addr)<<24;
    val |= read(addr+1)<<16;
    val |= read(addr+2)<<8;
    val |= read(addr+3);
    
    return val;
}

Uint16 NBIC::wget(Uint32 addr) {
    Uint32 val = 0;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[ND] Slot %i: NBIC Unaligned access at %08X.", slot, addr);
        abort();
    }
    val  = read(addr)<<8;
    val |= read(addr+1)<<16;
    
    return val;
}

Uint8 NBIC::bget(Uint32 addr) {
    return read(addr);
}

void NBIC::lput(Uint32 addr, Uint32 l) {
    if (addr&3) {
        Log_Printf(LOG_WARN, "[ND] Slot %i: NBIC Unaligned access at %08X.", slot, addr);
        abort();
    }
    write(addr+0,l>>24);
    write(addr+1,l>>16);
    write(addr+2,l>>8);
    write(addr+2,l);
}

void NBIC::wput(Uint32 addr, Uint16 w) {
    if (addr&1) {
        Log_Printf(LOG_WARN, "[ND] Slot %i: NBIC Unaligned access at %08X.", slot, addr);
        abort();
    }
    write(addr+0,w>>8);
    write(addr+1,w);
}

void NBIC::bput(Uint32 addr, Uint8 b) {
    write(addr,b);
}

void NBIC::set_intstatus(bool set) {
	if (set) {
        intstatus |= ND_NBIC_INTR;
        remInter  |= 1 << slot;
	} else {
        intstatus &= ~ND_NBIC_INTR;
        remInter  &= ~(1 << slot);
	}
}


/* Reset function */

void NBIC::init(void) {
    /* Release any interrupt that may be pending */
    intmask      = 0;
    intstatus    = 0;
    remInter     = 0;
    remInterMask = 0;
    set_interrupt(INT_REMOTE, RELEASE_INT);
}

volatile Uint32 NBIC::remInter;
volatile Uint32 NBIC::remInterMask;

/* Interrupt functions */
void nd_nbic_interrupt(void) {
    if (NBIC::remInter&NBIC::remInterMask) {
        set_interrupt(INT_REMOTE, SET_INT);
    } else {
        set_interrupt(INT_REMOTE, RELEASE_INT);
    }
}
