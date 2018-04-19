#pragma once

#ifndef __ND_MEM_H__
#define __ND_MEM_H__

#include <sysdeps.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* NeXTdimension board and slot memory */
#define ND_BOARD_SIZE    0x10000000
#define ND_BOARD_MASK    0x0FFFFFFF
#define ND_BOARD_BITS   0xF0000000
    
#define ND_SLOT_SIZE    0x01000000
#define ND_SLOT_MASK    0x00FFFFFF
#define ND_SLOT_BITS    0x0F000000
    
#define ND_NBIC_SPACE   0xFFFFFFE8

typedef uae_u32 (*nd_mem_get_func)(int, uaecptr) REGPARAM;
typedef void (*nd_mem_put_func)(int, uaecptr, uae_u32) REGPARAM;

#define nd_bankindex(addr) (((uaecptr)(addr)) >> 16)
#define nd_get_mem_bank(addr)    (nd->mem_banks[nd_bankindex(addr)])
#define nd68k_get_mem_bank(addr) (mem_banks[nd_bankindex(addr)])
#define nd_put_mem_bank(addr, b) (mem_banks[nd_bankindex(addr)] = (b))

#define nd_longget(addr)   (nd_get_mem_bank(addr)->lget(addr))
#define nd_wordget(addr)   (nd_get_mem_bank(addr)->wget(addr))
#define nd_byteget(addr)   (nd_get_mem_bank(addr)->bget(addr))
#define nd_longput(addr,l) (nd_get_mem_bank(addr)->lput(addr, l))
#define nd_wordput(addr,w) (nd_get_mem_bank(addr)->wput(addr, w))
#define nd_byteput(addr,b) (nd_get_mem_bank(addr)->bput(addr, b))
#define nd_cs8get(addr)    (nd_get_mem_bank(addr)->cs8get(addr))

#define nd68k_longget(addr)   (nd68k_get_mem_bank(addr)->lget(addr))
#define nd68k_wordget(addr)   (nd68k_get_mem_bank(addr)->wget(addr))
#define nd68k_byteget(addr)   (nd68k_get_mem_bank(addr)->bget(addr))
#define nd68k_longput(addr,l) (nd68k_get_mem_bank(addr)->lput(addr, l))
#define nd68k_wordput(addr,w) (nd68k_get_mem_bank(addr)->wput(addr, w))
#define nd68k_byteput(addr,b) (nd68k_get_mem_bank(addr)->bput(addr, b))
#define nd68k_cs8get(addr)    (nd68k_get_mem_bank(addr)->cs8geti(addr))

#ifdef __cplusplus
}

class ND_Addrbank {
protected:
    NextDimension* nd;
public:
    int flags;
    
    virtual Uint32 lget(Uint32 addr);
    virtual Uint32 wget(Uint32 addr);
    virtual Uint32 bget(Uint32 addr);
    virtual Uint32 cs8get(Uint32 addr);
    
    virtual void lput(Uint32 addr, Uint32 val);
    virtual void wput(Uint32 addr, Uint32 val);
    virtual void bput(Uint32 addr, Uint32 val);
    
    ND_Addrbank(NextDimension* nd);
};

#endif /* __cplusplus */

#endif /* __ND_MEM_H__ */
