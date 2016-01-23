/*  Previous - ramdac.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Brooktree Bt463 RAMDAC emulation.
 
 This chip was is used in color NeXTstations and NeXTdimension.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "ramdac.h"
#include "sysReg.h"

#define IO_SEG_MASK 0x1FFFF

#define LOG_RAMDAC_LEVEL    LOG_DEBUG


/* Bt 463 RAMDAC */

static void bt463_autoinc(bt463* ramdac) {
    ramdac->idx++;
    if(ramdac->idx == 3) {
        ramdac->idx = 0;
        ramdac->addr++;
    }
}

uae_u32 bt463_bget(bt463* ramdac, uaecptr addr) {
    uae_u32 result = 0;
    switch(addr & 0xF) {
        case 0:
            return ramdac->addr & 0xFF;
        case 0x4:
            return (ramdac->addr >> 8) & 0xFF;
        case 0x8:
            result = ramdac->regs[ramdac->addr*3];
            if(ramdac->addr == 0x100 || ramdac->addr == 0x101)
                bt463_autoinc(ramdac);
            break;
        case 0xC:
            result = ramdac->regs[ramdac->addr*3+ramdac->idx];
            bt463_autoinc(ramdac);
            break;
    }
    return result;
}

void bt463_bput(bt463* ramdac, uaecptr addr, uae_u32 b) {
    switch(addr & 0xF) {
        case 0x0:
            ramdac->addr &= 0xFF00;
            ramdac->addr |= b & 0xFF;
            ramdac->idx = 0;
            break;
        case 0x4:
            ramdac->addr &= 0x000F;
            ramdac->addr |= (b & 0x0F) << 8;
            ramdac->idx = 0;
            break;
        case 0x8:
            ramdac->regs[ramdac->addr*3] = b;
            if(ramdac->addr == 0x100 || ramdac->addr == 0x101)
                bt463_autoinc(ramdac);
            break;
        case 0xC:
            ramdac->regs[ramdac->addr*3+ramdac->idx] = b;
            bt463_autoinc(ramdac);
            break;
    }
}
