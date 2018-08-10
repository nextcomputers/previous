/*
 * cpummu.cpp -  MMU emulation
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 *
 * Inspired by UAE MMU patch
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "sysconfig.h"
#include "sysdeps.h"

#include "main.h"
#include "hatari-glue.h"


#include "options_cpu.h"
#include "memory.h"
#include "newcpu.h"
#include "cpummu.h"

#define MMUDUMP 0

#define DBG_MMU_VERBOSE	1
#define DBG_MMU_SANITY	1
#if 0
#define write_log printf
#endif

#ifdef FULLMMU


uae_u32 mmu_is_super;
uae_u32 mmu_tagmask, mmu_pagemask, mmu_pagemaski;
struct mmu_atc_line mmu_atc_array[ATC_TYPE][ATC_WAYS][ATC_SLOTS];
bool mmu_pagesize_8k;

int mmu060_state;
uae_u16 mmu_opcode;
bool mmu_restart;
static bool locked_rmw_cycle;
static bool ismoves;
int mmu_atc_ways[2];
int way_random;

int mmu040_movem;
uaecptr mmu040_movem_ea;
uae_u32 mmu040_move16[4];

static void mmu_dump_ttr(const TCHAR * label, uae_u32 ttr)
{
    DUNUSED(label);
#if MMUDEBUG > 0
    uae_u32 from_addr, to_addr;
    
    from_addr = ttr & MMU_TTR_LOGICAL_BASE;
    to_addr = (ttr & MMU_TTR_LOGICAL_MASK) << 8;
    
    write_log(_T("%s: [%08x] %08x - %08x enabled=%d supervisor=%d wp=%d cm=%02d\n"),
              label, ttr,
              from_addr, to_addr,
              ttr & MMU_TTR_BIT_ENABLED ? 1 : 0,
              (ttr & (MMU_TTR_BIT_SFIELD_ENABLED | MMU_TTR_BIT_SFIELD_SUPER)) >> MMU_TTR_SFIELD_SHIFT,
              ttr & MMU_TTR_BIT_WRITE_PROTECT ? 1 : 0,
              (ttr & MMU_TTR_CACHE_MASK) >> MMU_TTR_CACHE_SHIFT
              );
#endif
}

void mmu_make_transparent_region(uaecptr baseaddr, uae_u32 size, int datamode)
{
    uae_u32 * ttr;
    uae_u32 * ttr0 = datamode ? &regs.dtt0 : &regs.itt0;
    uae_u32 * ttr1 = datamode ? &regs.dtt1 : &regs.itt1;
    
    if ((*ttr1 & MMU_TTR_BIT_ENABLED) == 0)
        ttr = ttr1;
    else if ((*ttr0 & MMU_TTR_BIT_ENABLED) == 0)
        ttr = ttr0;
    else
        return;
    
    *ttr = baseaddr & MMU_TTR_LOGICAL_BASE;
    *ttr |= ((baseaddr + size - 1) & MMU_TTR_LOGICAL_BASE) >> 8;
    *ttr |= MMU_TTR_BIT_ENABLED;
    
#if MMUDEBUG > 0
    write_log(_T("MMU: map transparent mapping of %08x\n"), *ttr);
#endif
}

#if 0
#if MMUDUMP

/* This dump output makes much more sense than old one */

#define LEVELA_SIZE 7
#define LEVELB_SIZE 7
#define LEVELC_SIZE 6
#define PAGE_SIZE 12 // = 1 << 12 = 4096

#define LEVELA_VAL(x) ((((uae_u32)(x)) >> (32 - (LEVELA_SIZE                            ))) & ((1 << LEVELA_SIZE) - 1))
#define LEVELB_VAL(x) ((((uae_u32)(x)) >> (32 - (LEVELA_SIZE + LEVELB_SIZE              ))) & ((1 << LEVELB_SIZE) - 1))
#define LEVELC_VAL(x) ((((uae_u32)(x)) >> (32 - (LEVELA_SIZE + LEVELB_SIZE + LEVELC_SIZE))) & ((1 << LEVELC_SIZE) - 1))

#define LEVELA(root, x) (get_long(root + LEVELA_VAL(x) * 4))
#define LEVELB(a, x) (get_long((((uae_u32)a) & ~((1 << (LEVELB_SIZE + 2)) - 1)) + LEVELB_VAL(x) * 4))
#define LEVELC(b, x) (get_long((((uae_u32)b) & ~((1 << (LEVELC_SIZE + 2)) - 1)) + LEVELC_VAL(x) * 4))

#define ISINVALID(x) ((((ULONG)x) & 3) == 0)

static uae_u32 getdesc(uae_u32 root, uae_u32 addr)
{
    ULONG desc;
    
    desc = LEVELA(root, addr);
    if (ISINVALID(desc))
        return desc;
    desc = LEVELB(desc, addr);
    if (ISINVALID(desc))
        return desc;
    desc = LEVELC(desc, addr);
    return desc;
}
static void mmu_dump_table(const char * label, uaecptr root_ptr)
{
    ULONG i;
    ULONG startaddr;
    ULONG odesc;
    ULONG totalpages;
    ULONG pagemask = (1 << PAGE_SIZE) - 1;
    
    console_out_f(_T("MMU dump start. Root = %08x\n"), root_ptr);
    totalpages = 1 << (32 - PAGE_SIZE);
    startaddr = 0;
    odesc = getdesc(root_ptr, startaddr);
    for (i = 0; i <= totalpages; i++) {
        ULONG addr = i << PAGE_SIZE;
        ULONG desc = 0;
        if (i < totalpages)
            desc = getdesc(root_ptr, addr);
        if ((desc & pagemask) != (odesc & pagemask) || i == totalpages) {
            uae_u8 cm, sp;
            cm = (odesc >> 5) & 3;
            sp = (odesc >> 7) & 1;
            console_out_f(_T("%08x - %08x: %08x WP=%d S=%d CM=%d (%08x)\n"),
                          startaddr, addr - 1, odesc & ~((1 << PAGE_SIZE) - 1),
                          (odesc & 4) ? 1 : 0, sp, cm, odesc);
            startaddr = addr;
            odesc = desc;
        }
    }
    console_out_f(_T("MMU dump end\n"));
}

#else
/* {{{ mmu_dump_table */
static void mmu_dump_table(const char * label, uaecptr root_ptr)
{
    DUNUSED(label);
    const int ROOT_TABLE_SIZE = 128,
    PTR_TABLE_SIZE = 128,
    PAGE_TABLE_SIZE = 64,
    ROOT_INDEX_SHIFT = 25,
    PTR_INDEX_SHIFT = 18;
    // const int PAGE_INDEX_SHIFT = 12;
    int root_idx, ptr_idx, page_idx;
    uae_u32 root_des, ptr_des, page_des;
    uaecptr ptr_des_addr, page_addr,
    root_log, ptr_log, page_log;
    
    console_out_f(_T("%s: root=%x\n"), label, root_ptr);
    
    for (root_idx = 0; root_idx < ROOT_TABLE_SIZE; root_idx++) {
        root_des = phys_get_long(root_ptr + root_idx);
        
        if ((root_des & 2) == 0)
            continue;	/* invalid */
        
        console_out_f(_T("ROOT: %03d U=%d W=%d UDT=%02d\n"), root_idx,
                      root_des & 8 ? 1 : 0,
                      root_des & 4 ? 1 : 0,
                      root_des & 3
                      );
        
        root_log = root_idx << ROOT_INDEX_SHIFT;
        
        ptr_des_addr = root_des & MMU_ROOT_PTR_ADDR_MASK;
        
        for (ptr_idx = 0; ptr_idx < PTR_TABLE_SIZE; ptr_idx++) {
            struct {
                uaecptr	log, phys;
                int start_idx, n_pages;	/* number of pages covered by this entry */
                uae_u32 match;
            } page_info[PAGE_TABLE_SIZE];
            int n_pages_used;
            
            ptr_des = phys_get_long(ptr_des_addr + ptr_idx);
            ptr_log = root_log | (ptr_idx << PTR_INDEX_SHIFT);
            
            if ((ptr_des & 2) == 0)
                continue; /* invalid */
            
            page_addr = ptr_des & (mmu_pagesize_8k ? MMU_PTR_PAGE_ADDR_MASK_8 : MMU_PTR_PAGE_ADDR_MASK_4);
            
            n_pages_used = -1;
            for (page_idx = 0; page_idx < PAGE_TABLE_SIZE; page_idx++) {
                
                page_des = phys_get_long(page_addr + page_idx);
                page_log = ptr_log | (page_idx << 2);		// ??? PAGE_INDEX_SHIFT
                
                switch (page_des & 3) {
                    case 0: /* invalid */
                        continue;
                    case 1: case 3: /* resident */
                    case 2: /* indirect */
                        if (n_pages_used == -1 || page_info[n_pages_used].match != page_des) {
                            /* use the next entry */
                            n_pages_used++;
                            
                            page_info[n_pages_used].match = page_des;
                            page_info[n_pages_used].n_pages = 1;
                            page_info[n_pages_used].start_idx = page_idx;
                            page_info[n_pages_used].log = page_log;
                        } else {
                            page_info[n_pages_used].n_pages++;
                        }
                        break;
                }
            }
            
            if (n_pages_used == -1)
                continue;
            
            console_out_f(_T(" PTR: %03d U=%d W=%d UDT=%02d\n"), ptr_idx,
                          ptr_des & 8 ? 1 : 0,
                          ptr_des & 4 ? 1 : 0,
                          ptr_des & 3
                          );
            
            
            for (page_idx = 0; page_idx <= n_pages_used; page_idx++) {
                page_des = page_info[page_idx].match;
                
                if ((page_des & MMU_PDT_MASK) == 2) {
                    console_out_f(_T("  PAGE: %03d-%03d log=%08x INDIRECT --> addr=%08x\n"),
                                  page_info[page_idx].start_idx,
                                  page_info[page_idx].start_idx + page_info[page_idx].n_pages - 1,
                                  page_info[page_idx].log,
                                  page_des & MMU_PAGE_INDIRECT_MASK
                                  );
                    
                } else {
                    console_out_f(_T("  PAGE: %03d-%03d log=%08x addr=%08x UR=%02d G=%d U1/0=%d S=%d CM=%d M=%d U=%d W=%d\n"),
                                  page_info[page_idx].start_idx,
                                  page_info[page_idx].start_idx + page_info[page_idx].n_pages - 1,
                                  page_info[page_idx].log,
                                  page_des & (mmu_pagesize_8k ? MMU_PAGE_ADDR_MASK_8 : MMU_PAGE_ADDR_MASK_4),
                                  (page_des & (mmu_pagesize_8k ? MMU_PAGE_UR_MASK_8 : MMU_PAGE_UR_MASK_4)) >> MMU_PAGE_UR_SHIFT,
                                  page_des & MMU_DES_GLOBAL ? 1 : 0,
                                  (page_des & MMU_TTR_UX_MASK) >> MMU_TTR_UX_SHIFT,
                                  page_des & MMU_DES_SUPER ? 1 : 0,
                                  (page_des & MMU_TTR_CACHE_MASK) >> MMU_TTR_CACHE_SHIFT,
                                  page_des & MMU_DES_MODIFIED ? 1 : 0,
                                  page_des & MMU_DES_USED ? 1 : 0,
                                  page_des & MMU_DES_WP ? 1 : 0
                                  );
                }
            }
        }
        
    }
}
/* }}} */
#endif
#endif

/* {{{ mmu_dump_atc */
static void mmu_dump_atc(void)
{
    
}
/* }}} */

/* {{{ mmu_dump_tables */
void mmu_dump_tables(void)
{
    write_log(_T("URP: %08x   SRP: %08x  MMUSR: %x  TC: %x\n"), regs.urp, regs.srp, regs.mmusr, regs.tcr);
    mmu_dump_ttr(_T("DTT0"), regs.dtt0);
    mmu_dump_ttr(_T("DTT1"), regs.dtt1);
    mmu_dump_ttr(_T("ITT0"), regs.itt0);
    mmu_dump_ttr(_T("ITT1"), regs.itt1);
    mmu_dump_atc();
#if MMUDUMP
    mmu_dump_table("SRP", regs.srp);
#endif
}
/* }}} */

void mmu_bus_error(uaecptr addr, uae_u32 val, int fc, bool write, int size, bool nonmmu)
{
    uae_u16 ssw = 0;
    
    if (ismoves) {
        ismoves = false;
        // MOVES special behavior
        int fc2 = write ? regs.dfc : regs.sfc;
        if (fc2 == 0 || fc2 == 3 || fc2 == 4 || fc2 == 7)
            ssw |= MMU_SSW_TT1;
        if (fc2 == 2 || fc2 == 6)
            fc2--;
#if MMUDEBUGMISC > 0
        write_log (_T("040 MMU MOVES fc=%d -> %d\n"), fc, fc2);
#endif
        fc = fc2;
    }
    
    ssw |= fc & MMU_SSW_TM;				/* TM = FC */
    
    switch (size) {
        case sz_byte:
            ssw |= MMU_SSW_SIZE_B;
            break;
        case sz_word:
            ssw |= MMU_SSW_SIZE_W;
            break;
        case sz_long:
            ssw |= MMU_SSW_SIZE_L;
            break;
    }
    
    regs.wb3_status = write ? 0x80 | (ssw & 0x7f) : 0;
    regs.wb3_data = val;
    regs.wb2_status = 0;
    if (!write)
        ssw |= MMU_SSW_RW;
    
    if (size == 16) { // MOVE16
        ssw |= MMU_SSW_SIZE_CL;
        ssw |= MMU_SSW_TT0;
        regs.mmu_effective_addr &= ~15;
        if (write) {
            // clear normal writeback if MOVE16 write
            regs.wb3_status &= ~0x80;
            // wb2 = cacheline size writeback
            regs.wb2_status = 0x80 | MMU_SSW_SIZE_CL | (ssw & 0x1f);
            regs.wb2_address = regs.mmu_effective_addr;
            write_log (_T("040 MMU MOVE16 WRITE FAULT!\n"));
        }
    }
    
    if (mmu040_movem) {
        ssw |= MMU_SSW_CM;
        regs.mmu_effective_addr = mmu040_movem_ea;
        mmu040_movem = 0;
#if MMUDEBUGMISC > 0
        write_log (_T("040 MMU_SSW_CM EA=%08X\n"), mmu040_movem_ea);
#endif
    }
    if (locked_rmw_cycle) {
        ssw |= MMU_SSW_LK;
        ssw &= ~MMU_SSW_RW;
        locked_rmw_cycle = false;
#if MMUDEBUGMISC > 0
        write_log (_T("040 MMU_SSW_LK!\n"));
#endif
    }
    
    if (!nonmmu)
        ssw |= MMU_SSW_ATC;
    regs.mmu_ssw = ssw;
    
#if MMUDEBUG > 0
    write_log(_T("BF: fc=%d w=%d logical=%08x ssw=%04x PC=%08x INS=%04X\n"), fc, write, addr, ssw, m68k_getpc(), mmu_opcode);
#endif
    
    regs.mmu_fault_addr = addr;
    
    THROW(2);
}

/*
 * Lookup the address by walking the page table and updating
 * the page descriptors accordingly. Returns the found descriptor
 * or produces a bus error.
 */
static uae_u32 mmu_fill_atc(uaecptr addr, bool super, uae_u32 tag, bool write, struct mmu_atc_line *l)
{
    uae_u32 desc, desc_addr, wp, status;
    int i;
    
    wp = 0;
    desc = super ? regs.srp : regs.urp;
    
    /* fetch root table descriptor */
    i = (addr >> 23) & 0x1fc;
    desc_addr = (desc & MMU_ROOT_PTR_ADDR_MASK) | i;
    
    SAVE_EXCEPTION;
    TRY(prb) {
        desc = phys_get_long(desc_addr);
        if ((desc & 2) == 0) {
#if MMUDEBUG > 1
            write_log(_T("MMU: invalid root descriptor %s for %x desc at %x desc=%x\n"), super ? _T("srp"):_T("urp"),
                      addr, desc_addr, desc);
#endif
            goto fail;
        }
        
        wp |= desc;
        if ((desc & MMU_DES_USED) == 0)
            phys_put_long(desc_addr, desc | MMU_DES_USED);
        
        /* fetch pointer table descriptor */
        i = (addr >> 16) & 0x1fc;
        desc_addr = (desc & MMU_ROOT_PTR_ADDR_MASK) | i;
        desc = phys_get_long(desc_addr);
        if ((desc & 2) == 0) {
#if MMUDEBUG > 1
            write_log(_T("MMU: invalid ptr descriptor %s for %x desc at %x desc=%x\n"), super ? _T("srp"):_T("urp"),
                      addr, desc_addr, desc);
#endif
            goto fail;
        }
        wp |= desc;
        if ((desc & MMU_DES_USED) == 0)
            phys_put_long(desc_addr, desc | MMU_DES_USED);
        
        /* fetch page table descriptor */
        if (mmu_pagesize_8k) {
            i = (addr >> 11) & 0x7c;
            desc_addr = (desc & MMU_PTR_PAGE_ADDR_MASK_8) + i;
        } else {
            i = (addr >> 10) & 0xfc;
            desc_addr = (desc & MMU_PTR_PAGE_ADDR_MASK_4) + i;
        }
        
        desc = phys_get_long(desc_addr);
        if ((desc & 3) == 2) {
            /* indirect */
            desc_addr = desc & MMU_PAGE_INDIRECT_MASK;
            desc = phys_get_long(desc_addr);
        }
        if ((desc & 1) == 1) {
            wp |= desc;
            if (write) {
                if ((wp & MMU_DES_WP) || ((desc & MMU_DES_SUPER) && !super)) {
                    if ((desc & MMU_DES_USED) == 0) {
                        desc |= MMU_DES_USED;
                        phys_put_long(desc_addr, desc);
                    }
                } else if ((desc & (MMU_DES_USED|MMU_DES_MODIFIED)) !=
                           (MMU_DES_USED|MMU_DES_MODIFIED)) {
                    desc |= MMU_DES_USED|MMU_DES_MODIFIED;
                    phys_put_long(desc_addr, desc);
                }
            } else {
                if ((desc & MMU_DES_USED) == 0) {
                    desc |= MMU_DES_USED;
                    phys_put_long(desc_addr, desc);
                }
            }
            desc |= wp & MMU_DES_WP;
        } else {
#if MMUDEBUG > 2
            write_log(_T("MMU: invalid page descriptor log=%0x desc=%08x @%08x\n"), addr, desc, desc_addr);
#endif
            if ((desc & 3) == 2) {
#if MMUDEBUG > 1
                write_log(_T("MMU: double indirect descriptor log=%0x desc=%08x @%08x\n"), addr, desc, desc_addr);
#endif
            }
fail:
            desc = 0;
        }
        
        /* Create new ATC entry and return status */
        l->status = desc & (MMU_MMUSR_G|MMU_MMUSR_Ux|MMU_MMUSR_S|MMU_MMUSR_CM|MMU_MMUSR_M|MMU_MMUSR_W|MMU_MMUSR_R);
        l->phys = desc & mmu_pagemaski;
        status = l->phys | l->status;
        RESTORE_EXCEPTION;
    } CATCH(prb) {
        RESTORE_EXCEPTION;
        /* bus error during table search */
        l->status = 0;
        l->phys = 0;
        status = MMU_MMUSR_B;
#if MMUDEBUG > 0
        write_log(_T("MMU: bus error during table search.\n"));
#endif
    } ENDTRY
    
    l->valid = 1;
    l->tag = tag;
    
#if MMUDEBUG > 2
    write_log(_T("translate: %x,%u,%u -> %x\n"), addr, super, write, desc);
#endif

    return status;
}

uaecptr mmu_translate(uaecptr addr, uae_u32 val, uae_u32 flags)
{
    int way, i, index, way_invalid;
    uae_u32 data = flags & TRANS_DATA;
    uae_u32 tag = ((flags & TRANS_SUPER) | (addr >> 1)) & mmu_tagmask;
    struct mmu_atc_line *l;
    
    if (mmu_pagesize_8k)
        index=(addr & 0x0001E000)>>13;
    else
        index=(addr & 0x0000F000)>>12;
    
    way_invalid = ATC_WAYS;
    way_random++;
    way = mmu_atc_ways[data];
    
    for (i = 0; i < ATC_WAYS; i++) {
        // if we have this
        l = &mmu_atc_array[data][way][index];
        if (l->valid) {
            if (tag == l->tag) {
atc_retry:
                // check if we need to cause a page fault
                if (((l->status&(MMU_MMUSR_W|MMU_MMUSR_S|MMU_MMUSR_R))!=MMU_MMUSR_R)) {
                    if (((l->status&MMU_MMUSR_W) && (flags & TRANS_WRITE)) ||
                        ((l->status&MMU_MMUSR_S) && !(flags & TRANS_SUPER)) ||
                        !(l->status&MMU_MMUSR_R)) {
                        mmu_bus_error(addr, val, mmu_get_fc(flags & TRANS_SUPER, flags & TRANS_DATA), flags & TRANS_WRITE, (flags & TRANS_SIZE)>>1, false);
                        return 0; // never reach, bus error longjumps out of the function
                    }
                }
                // if first write to this page initiate table search to set M bit (but modify this slot)
                if (!(l->status&MMU_MMUSR_M) && (flags & TRANS_WRITE)) {
                    way_invalid = way;
                    break;
                }
                // save way for next access (likely in same page)
                mmu_atc_ways[data] = way;
                
                // return translated addr
                return l->phys | (addr & mmu_pagemask);
            }
        } else {
            way_invalid = way;
        }
        way++;
        way %= ATC_WAYS;
    }
    // no entry found, we need to create a new one, first find an atc line to replace
    way_random %= ATC_WAYS;
    way = (way_invalid < ATC_WAYS) ? way_invalid : way_random;

    // then initiate table search and create a new entry
    l = &mmu_atc_array[data][way][index];
    mmu_fill_atc(addr, flags & TRANS_SUPER, tag, flags & TRANS_WRITE, l);
    
    // and retry the ATC search
    way_random++;
    goto atc_retry;
    
    // never reach
    return 0;
}

static void misalignednotfirst(uaecptr addr)
{
#if MMUDEBUGMISC > 0
    write_log (_T("misalignednotfirst %08x -> %08x %08X\n"), regs.mmu_fault_addr, addr, regs.instruction_pc);
#endif
    regs.mmu_fault_addr = addr;
    regs.mmu_ssw |= MMU_SSW_MA;
}

static void misalignednotfirstcheck(uaecptr addr)
{
    if (regs.mmu_fault_addr == addr)
        return;
    misalignednotfirst (addr);
}

uae_u16 REGPARAM2 mmu_get_word_unaligned(uaecptr addr)
{
    uae_u16 res;
    
    res = (uae_u16)mmu_get_byte(addr, sz_word) << 8;
    SAVE_EXCEPTION;
    TRY(prb) {
        res |= mmu_get_byte(addr + 1, sz_word);
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        misalignednotfirst(addr);
        THROW_AGAIN(prb);
    } ENDTRY
    return res;
}

uae_u32 REGPARAM2 mmu_get_long_unaligned(uaecptr addr)
{
    uae_u32 res;
    
    if (likely(!(addr & 1))) {
        res = (uae_u32)mmu_get_word(addr, sz_long) << 16;
        SAVE_EXCEPTION;
        TRY(prb) {
            res |= mmu_get_word(addr + 2, sz_long);
            RESTORE_EXCEPTION;
        }
        CATCH(prb) {
            RESTORE_EXCEPTION;
            misalignednotfirst(addr);
            THROW_AGAIN(prb);
        } ENDTRY
    } else {
        res = (uae_u32)mmu_get_byte(addr, sz_long) << 8;
        SAVE_EXCEPTION;
        TRY(prb) {
            res = (res | mmu_get_byte(addr + 1, sz_long)) << 8;
            res = (res | mmu_get_byte(addr + 2, sz_long)) << 8;
            res |= mmu_get_byte(addr + 3, sz_long);
            RESTORE_EXCEPTION;
        }
        CATCH(prb) {
            RESTORE_EXCEPTION;
            misalignednotfirst(addr);
            THROW_AGAIN(prb);
        } ENDTRY
    }
    return res;
}

uae_u32 REGPARAM2 mmu_get_ilong_unaligned(uaecptr addr)
{
    uae_u32 res;
    
    res = (uae_u32)mmu_get_iword(addr, sz_long) << 16;
    SAVE_EXCEPTION;
    TRY(prb) {
        res |= mmu_get_iword(addr + 2, sz_long);
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        misalignednotfirst(addr);
        THROW_AGAIN(prb);
    } ENDTRY
    return res;
}

static uae_u16 REGPARAM2 mmu_get_lrmw_word_unaligned(uaecptr addr)
{
    uae_u16 res;
    
    res = (uae_u16)mmu_get_user_byte(addr, regs.s != 0, true, sz_word) << 8;
    SAVE_EXCEPTION;
    TRY(prb) {
        res |= mmu_get_user_byte(addr + 1, regs.s != 0, true, sz_word);
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        misalignednotfirst(addr);
        THROW_AGAIN(prb);
    } ENDTRY
    return res;
}

static uae_u32 REGPARAM2 mmu_get_lrmw_long_unaligned(uaecptr addr)
{
    uae_u32 res;
    
    if (likely(!(addr & 1))) {
        res = (uae_u32)mmu_get_user_word(addr, regs.s != 0, true, sz_long) << 16;
        SAVE_EXCEPTION;
        TRY(prb) {
            res |= mmu_get_user_word(addr + 2, regs.s != 0, true, sz_long);
            RESTORE_EXCEPTION;
        }
        CATCH(prb) {
            RESTORE_EXCEPTION;
            misalignednotfirst(addr);
            THROW_AGAIN(prb);
        } ENDTRY
    } else {
        res = (uae_u32)mmu_get_user_byte(addr, regs.s != 0, true, sz_long) << 8;
        SAVE_EXCEPTION;
        TRY(prb) {
            res = (res | mmu_get_user_byte(addr + 1, regs.s != 0, true, sz_long)) << 8;
            res = (res | mmu_get_user_byte(addr + 2, regs.s != 0, true, sz_long)) << 8;
            res |= mmu_get_user_byte(addr + 3, regs.s != 0, true, sz_long);
            RESTORE_EXCEPTION;
        }
        CATCH(prb) {
            RESTORE_EXCEPTION;
            misalignednotfirst(addr);
            THROW_AGAIN(prb);
        } ENDTRY
    }
    return res;
}

void REGPARAM2 mmu_put_long_unaligned(uaecptr addr, uae_u32 val)
{
    SAVE_EXCEPTION;
    TRY(prb) {
        if (likely(!(addr & 1))) {
            mmu_put_word(addr, val >> 16, sz_long);
            mmu_put_word(addr + 2, val, sz_long);
        } else {
            mmu_put_byte(addr, val >> 24, sz_long);
            mmu_put_byte(addr + 1, val >> 16, sz_long);
            mmu_put_byte(addr + 2, val >> 8, sz_long);
            mmu_put_byte(addr + 3, val, sz_long);
        }
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        regs.wb3_data = val;
        misalignednotfirstcheck(addr);
        THROW_AGAIN(prb);
    } ENDTRY
}

void REGPARAM2 mmu_put_word_unaligned(uaecptr addr, uae_u16 val)
{
    SAVE_EXCEPTION;
    TRY(prb) {
        mmu_put_byte(addr, val >> 8, sz_word);
        mmu_put_byte(addr + 1, val, sz_word);
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        regs.wb3_data = val;
        misalignednotfirstcheck(addr);
        THROW_AGAIN(prb);
    } ENDTRY
}

uae_u32 REGPARAM2 sfc_get_long(uaecptr addr)
{
    bool super = (regs.sfc & 4) != 0;
    uae_u32 res;
    
    ismoves = true;
    if (likely(!is_unaligned(addr, 4))) {
        res = mmu_get_user_long(addr, super, false, sz_long);
    } else {
        if (likely(!(addr & 1))) {
            res = (uae_u32)mmu_get_user_word(addr, super, false, sz_long) << 16;
            SAVE_EXCEPTION;
            TRY(prb) {
                res |= mmu_get_user_word(addr + 2, super, false, sz_long);
                RESTORE_EXCEPTION;
            }
            CATCH(prb) {
                RESTORE_EXCEPTION;
                misalignednotfirst(addr);
                THROW_AGAIN(prb);
            } ENDTRY
        } else {
            res = (uae_u32)mmu_get_user_byte(addr, super, false, sz_long) << 8;
            SAVE_EXCEPTION;
            TRY(prb) {
                res = (res | mmu_get_user_byte(addr + 1, super, false, sz_long)) << 8;
                res = (res | mmu_get_user_byte(addr + 2, super, false, sz_long)) << 8;
                res |= mmu_get_user_byte(addr + 3, super, false, sz_long);
                RESTORE_EXCEPTION;
            }
            CATCH(prb) {
                RESTORE_EXCEPTION;
                misalignednotfirst(addr);
                THROW_AGAIN(prb);
            } ENDTRY
        }
    }
    
    ismoves = false;
    return res;
}

uae_u16 REGPARAM2 sfc_get_word(uaecptr addr)
{
    bool super = (regs.sfc & 4) != 0;
    uae_u16 res;
    
    ismoves = true;
    if (likely(!is_unaligned(addr, 2))) {
        res = mmu_get_user_word(addr, super, false, sz_word);
    } else {
        res = (uae_u16)mmu_get_user_byte(addr, super, false, sz_word) << 8;
        SAVE_EXCEPTION;
        TRY(prb) {
            res |= mmu_get_user_byte(addr + 1, super, false, sz_word);
            RESTORE_EXCEPTION;
        }
        CATCH(prb) {
            RESTORE_EXCEPTION;
            misalignednotfirst(addr);
            THROW_AGAIN(prb);
        } ENDTRY
    }
    ismoves = false;
    return res;
}

uae_u8 REGPARAM2 sfc_get_byte(uaecptr addr)
{
    bool super = (regs.sfc & 4) != 0;
    uae_u8 res;
    
    ismoves = true;
    res = mmu_get_user_byte(addr, super, false, sz_byte);
    ismoves = false;
    return res;
}

void REGPARAM2 dfc_put_long(uaecptr addr, uae_u32 val)
{
    bool super = (regs.dfc & 4) != 0;
    
    ismoves = true;
    SAVE_EXCEPTION;
    TRY(prb) {
        if (likely(!is_unaligned(addr, 4))) {
            mmu_put_user_long(addr, val, super, sz_long);
        } else if (likely(!(addr & 1))) {
            mmu_put_user_word(addr, val >> 16, super, sz_long);
            mmu_put_user_word(addr + 2, val, super, sz_long);
        } else {
            mmu_put_user_byte(addr, val >> 24, super, sz_long);
            mmu_put_user_byte(addr + 1, val >> 16, super, sz_long);
            mmu_put_user_byte(addr + 2, val >> 8, super, sz_long);
            mmu_put_user_byte(addr + 3, val, super, sz_long);
        }
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        regs.wb3_data = val;
        misalignednotfirstcheck(addr);
        THROW_AGAIN(prb);
    } ENDTRY
    ismoves = false;
}

void REGPARAM2 dfc_put_word(uaecptr addr, uae_u16 val)
{
    bool super = (regs.dfc & 4) != 0;
    
    ismoves = true;
    SAVE_EXCEPTION;
    TRY(prb) {
        if (likely(!is_unaligned(addr, 2))) {
            mmu_put_user_word(addr, val, super, sz_word);
        } else {
            mmu_put_user_byte(addr, val >> 8, super, sz_word);
            mmu_put_user_byte(addr + 1, val, super, sz_word);
        }
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        regs.wb3_data = val;
        misalignednotfirstcheck(addr);
        THROW_AGAIN(prb);
    } ENDTRY
    ismoves = false;
}

void REGPARAM2 dfc_put_byte(uaecptr addr, uae_u8 val)
{
    bool super = (regs.dfc & 4) != 0;
    
    ismoves = true;
    SAVE_EXCEPTION;
    TRY(prb) {
        mmu_put_user_byte(addr, val, super, sz_byte);
        RESTORE_EXCEPTION;
    }
    CATCH(prb) {
        RESTORE_EXCEPTION;
        regs.wb3_data = val;
        THROW_AGAIN(prb);
    } ENDTRY
    ismoves = false;
}


void REGPARAM2 mmu_op_real(uae_u32 opcode, uae_u16 extra)
{
    bool super = (regs.dfc & 4) != 0;
    DUNUSED(extra);
    if ((opcode & 0xFE0) == 0x0500) { // PFLUSH
        bool glob;
        int regno;
        //D(didflush = 0);
        uae_u32 addr;
        /* PFLUSH */
        regno = opcode & 7;
        glob = (opcode & 8) != 0;
        
        if (opcode & 16) {
#if MMUINSDEBUG > 1
            write_log(_T("pflusha(%u,%u) PC=%08x\n"), glob, regs.dfc, m68k_getpc ());
#endif
            mmu_flush_atc_all(glob);
        } else {
            addr = m68k_areg(regs, regno);
#if MMUINSDEBUG > 1
            write_log(_T("pflush(%u,%u,%x) PC=%08x\n"), glob, regs.dfc, addr, m68k_getpc ());
#endif
            mmu_flush_atc(addr, super, glob);
        }
        flush_internals();
#ifdef USE_JIT
        flush_icache(0);
#endif
    } else if ((opcode & 0x0FD8) == 0x0548) { // PTEST (68040)
        bool write;
        int regno;
        uae_u32 addr;
        
        regno = opcode & 7;
        write = (opcode & 32) == 0;
        addr = m68k_areg(regs, regno);
#if MMUINSDEBUG > 0
        write_log(_T("PTEST%c (A%d) %08x DFC=%d\n"), write ? 'W' : 'R', regno, addr, regs.dfc);
#endif
        mmu_flush_atc(addr, super, true);
        
        bool data = (regs.dfc & 3) != 2;
        int ttr_match = mmu_match_ttr(addr,super,data);
        
        if (ttr_match != TTR_NO_MATCH) {
            if (ttr_match == TTR_NO_WRITE && write) {
                regs.mmusr = MMU_MMUSR_B;
            } else {
                regs.mmusr = MMU_MMUSR_T | MMU_MMUSR_R;
            }
        } else {
            int way;
            uae_u32 index;
            uae_u32 tag = ((super ? 0x80000000 : 0x00000000) | (addr >> 1)) & mmu_tagmask;
            if (mmu_pagesize_8k)
                index=(addr & 0x0001E000)>>13;
            else
                index=(addr & 0x0000F000)>>12;
            
            for (way = 0; way < ATC_WAYS; way++) {
                if (!mmu_atc_array[data][way][index].valid)
                    break;
            }
            if (way >= ATC_WAYS) {
                way = way_random % ATC_WAYS;
            }
            regs.mmusr = mmu_fill_atc(addr, super, tag, write, &mmu_atc_array[data][way][index]);
        }
#if MMUINSDEBUG > 0
        write_log(_T("PTEST result: mmusr %08x\n"), regs.mmusr);
#endif
    }
#if 0
    else if ((opcode & 0xFFB8) == 0xF588) { // PLPA (68060)
        int write = (opcode & 0x40) == 0;
        int regno = opcode & 7;
        uae_u32 addr = m68k_areg (regs, regno);
        bool data = (regs.dfc & 3) != 2;
        
#if MMUINSDEBUG > 0
        write_log(_T("PLPA%c param: %08x\n"), write ? 'W' : 'R', addr);
#endif
        if (mmu_match_ttr_write(addr,super,data,0,1,write)==TTR_NO_MATCH) {
            m68k_areg (regs, regno) = mmu_translate(addr, 0, super, data, write, 1);
        }
#if MMUINSDEBUG > 0
        write_log(_T("PLPA%c result: %08x\n"), write ? 'W' : 'R', m68k_areg (regs, regno));
#endif
    }
#endif
    else {
        op_illg (opcode);
    }
}

// fixme : global parameter?
void REGPARAM2 mmu_flush_atc(uaecptr addr, bool super, bool global)
{
    int way,type,index;
    
    uaecptr tag = ((super ? 0x80000000 : 0) | (addr >> 1)) & mmu_tagmask;
    if (mmu_pagesize_8k)
        index=(addr & 0x0001E000)>>13;
    else
        index=(addr & 0x0000F000)>>12;
    for (type=0;type<ATC_TYPE;type++) {
        for (way=0;way<ATC_WAYS;way++) {
            if (!global && mmu_atc_array[type][way][index].status&MMU_MMUSR_G)
                continue;
            // if we have this
            if ((tag == mmu_atc_array[type][way][index].tag) && (mmu_atc_array[type][way][index].valid)) {
                mmu_atc_array[type][way][index].valid=false;
            }
        }
    }
}

void REGPARAM2 mmu_flush_atc_all(bool global)
{
    unsigned int way,slot,type;
    for (type=0;type<ATC_TYPE;type++) {
        for (way=0;way<ATC_WAYS;way++) {
            for (slot=0;slot<ATC_SLOTS;slot++) {
                if (!global && mmu_atc_array[type][way][slot].status&MMU_MMUSR_G)
                    continue;
                mmu_atc_array[type][way][slot].valid=false;
            }
        }
    }
}

void REGPARAM2 mmu_set_funcs(void)
{
}

void REGPARAM2 mmu_reset(void)
{
    mmu_flush_atc_all(true);
    mmu_set_funcs();
}

void REGPARAM2 mmu_set_tc(uae_u16 tc)
{
    regs.mmu_enabled = (tc & 0x8000) != 0;
    mmu_pagesize_8k = (tc & 0x4000) != 0;
    mmu_tagmask  = mmu_pagesize_8k ? 0xFFFF0000 : 0xFFFF8000;
    mmu_pagemask = mmu_pagesize_8k ? 0x00001FFF : 0x00000FFF;
    mmu_pagemaski = ~mmu_pagemask;
    regs.mmu_page_size = mmu_pagesize_8k ? 8192 : 4096;
    
    mmu_flush_atc_all(true);
    
    write_log(_T("%d MMU: enabled=%d page8k=%d PC=%08x\n"), currprefs.mmu_model, regs.mmu_enabled, mmu_pagesize_8k, m68k_getpc());
#if MMUDUMP
    if (regs.mmu_enabled)
        mmu_dump_tables();
#endif
}

void REGPARAM2 mmu_set_super(bool super)
{
    mmu_is_super = super ? 0x80000000 : 0;
}

void m68k_do_rte_mmu040 (uaecptr a7)
{
    uae_u16 ssr = get_word_mmu040 (a7 + 8 + 4);
    if (ssr & MMU_SSW_CT) {
        uaecptr src_a7 = a7 + 8 - 8;
        uaecptr dst_a7 = a7 + 8 + 52;
        put_word_mmu040 (dst_a7 + 0, get_word_mmu040 (src_a7 + 0));
        put_long_mmu040 (dst_a7 + 2, get_long_mmu040 (src_a7 + 2));
        // skip this word
        put_long_mmu040 (dst_a7 + 8, get_long_mmu040 (src_a7 + 8));
    }
    if (ssr & MMU_SSW_CM) {
        mmu040_movem = 1;
        mmu040_movem_ea = get_long_mmu040 (a7 + 8);
#if MMUDEBUGMISC > 0
        write_log (_T("MMU restarted MOVEM EA=%08X\n"), mmu040_movem_ea);
#endif
    }
}

void m68k_do_rte_mmu060 (uaecptr a7)
{
#if 0
    mmu060_state = 2;
#endif
}

void flush_mmu040 (uaecptr addr, int n)
{
}
void m68k_do_rts_mmu040 (void)
{
    uaecptr stack = m68k_areg (regs, 7);
    uaecptr newpc = get_long_mmu040 (stack);
    m68k_areg (regs, 7) += 4;
    m68k_setpc (newpc);
}
void m68k_do_bsr_mmu040 (uaecptr oldpc, uae_s32 offset)
{
    uaecptr newstack = m68k_areg (regs, 7) - 4;
    put_long_mmu040 (newstack, oldpc);
    m68k_areg (regs, 7) -= 4;
    m68k_incpci (offset);
}
void uae_mmu_put_lrmw (uaecptr addr, uae_u32 v, int size, int type)
{
    locked_rmw_cycle = true;
    if (size == sz_byte) {
        mmu_put_byte(addr, v, sz_byte);
    } else if (size == sz_word) {
        if (unlikely(is_unaligned(addr, 2))) {
            mmu_put_word_unaligned(addr, v);
        } else {
            mmu_put_word(addr, v, sz_word);
        }
    } else {
        if (unlikely(is_unaligned(addr, 4)))
            mmu_put_long_unaligned(addr, v);
        else
            mmu_put_long(addr, v, sz_long);
    }
    locked_rmw_cycle = false;
}
uae_u32 uae_mmu_get_lrmw (uaecptr addr, int size, int type)
{
    uae_u32 v;
    locked_rmw_cycle = true;
    if (size == sz_byte) {
        v = mmu_get_user_byte(addr, regs.s != 0, true, sz_byte);
    } else if (size == sz_word) {
        if (unlikely(is_unaligned(addr, 2))) {
            v = mmu_get_lrmw_word_unaligned(addr);
        } else {
            v = mmu_get_user_word(addr, regs.s != 0, true, sz_word);
        }
    } else {
        if (unlikely(is_unaligned(addr, 4)))
            v = mmu_get_lrmw_long_unaligned(addr);
        else
            v = mmu_get_user_long(addr, regs.s != 0, true, sz_long);
    }
    locked_rmw_cycle = false;
    return v;
}


#ifndef __cplusplus
sigjmp_buf __exbuf;
int     __exvalue;
#define MAX_TRY_STACK 256
static int s_try_stack_size=0;
static sigjmp_buf s_try_stack[MAX_TRY_STACK];
sigjmp_buf* __poptry(void) {
    if (s_try_stack_size>0) {
        s_try_stack_size--;
        if (s_try_stack_size == 0)
            return NULL;
        memcpy(&__exbuf,&s_try_stack[s_try_stack_size-1],sizeof(sigjmp_buf));
        // fprintf(stderr,"pop %d jmpbuf=%08x\n",s_try_stack_size, s_try_stack[s_try_stack_size][0]);
        return &s_try_stack[s_try_stack_size-1];
    }
    else {
        fprintf(stderr,"try stack underflow...\n");
        // return (NULL);
        abort();
    }
}
void __pushtry(sigjmp_buf* j) {
    if (s_try_stack_size<MAX_TRY_STACK) {
        // fprintf(stderr,"push %d jmpbuf=%08x\n",s_try_stack_size, (*j)[0]);
        memcpy(&s_try_stack[s_try_stack_size],j,sizeof(sigjmp_buf));
        s_try_stack_size++;
    } else {
        fprintf(stderr,"try stack overflow...\n");
        abort();
    }
}
int __is_catched(void) {return (s_try_stack_size>0); }
#endif

#else

void mmu_op(uae_u32 opcode, uae_u16 /*extra*/)
{
    if ((opcode & 0xFE0) == 0x0500) {
        /* PFLUSH instruction */
        flush_internals();
    } else if ((opcode & 0x0FD8) == 0x548) {
        /* PTEST instruction */
    } else
        op_illg(opcode);
}

#endif


/*
 vim:ts=4:sw=4:
 */
