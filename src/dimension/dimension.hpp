#pragma once

#ifndef __DIMENSION_H__
#define __DIMENSION_H__

#include <SDL_stdinc.h>
#include "nd_sdl.hpp"

/* NeXTdimension memory controller revision (0 and 1 allowed) */
#define ND_STEP 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
typedef void (*i860_run_func)(int);
extern i860_run_func i860_Run;


void nd_start_debugger(void);
const char* nd_reports(int num, double realTime, double hostTime);

#define ND_LOG_IO_RD LOG_NONE
#define ND_LOG_IO_WR LOG_NONE

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#include "NextBus.hpp"
#include "i860.hpp"
#include "nd_nbic.hpp"
#include "nd_mem.hpp"
#include "ramdac.h"

class NextDimension;

class MC {
    NextDimension* nd;
public:
    Uint32 csr0;
    Uint32 csr1;
    Uint32 csr2;
    Uint32 sid;
    Uint32 dma_csr;
    Uint32 dma_start;
    Uint32 dma_width;
    Uint32 dma_pstart;
    Uint32 dma_pwidth;
    Uint32 dma_sstart;
    Uint32 dma_swidth;
    Uint32 dma_bsstart;
    Uint32 dma_bswidth;
    Uint32 dma_top;
    Uint32 dma_bottom;
    Uint32 dma_line_a;
    Uint32 dma_curr_a;
    Uint32 dma_scurr_a;
    Uint32 dma_out_a;
    Uint32 vram;
    Uint32 dram;
    
    MC(NextDimension* nd);
    void init(void);
    Uint32 read(Uint32 addr);
    void   write(Uint32 addr, Uint32 val);
};

class DP {
    NextDimension* nd;
public:
    Uint8  iic_addr;
    Uint8  iic_msg;
    Uint32 iic_msgsz;
    int    iic_busy;
    Uint32 doff; // (SC) wild guess - vram offset in pixels?
    Uint32 csr;
    Uint32 alpha;
    Uint32 dma;
    Uint32 cpu_x;
    Uint32 cpu_y;
    Uint32 dma_x;
    Uint32 dma_y;
    Uint32 iic_stat_addr;
    Uint32 iic_data;
    
    DP(NextDimension* nd);
    void init(void);
    Uint32 lget(Uint32 addr);
    void   lput(Uint32 addr, Uint32 val);
    void   iicmsg(void);
};

#define ND_DMCD_NUM_REG 25
class DMCD {
    NextDimension* nd;
public:
    Uint8 addr;
    Uint8 reg[ND_DMCD_NUM_REG];

    DMCD(NextDimension* nd);
    
    void write(Uint32 step, Uint8 data);
};

class DCSC {
    NextDimension* nd;
    int dev;
public:
    Uint8 addr;
    Uint8 ctrl;
    Uint8 lut[256];
    
    DCSC(NextDimension* nd, int dev);
    
    void write(Uint32 step, Uint8 data);
};

class NextDimension : public NextBusBoard {
    /* Message port for host->dimension communication */
    volatile int    m_port;
    lock_t          m_port_lock;
public:
    NDSDL           sdl;
    i860_cpu_device i860;
    NBIC            nbic;
    MC              mc;
    DP              dp;
    DMCD            dmcd;
    DCSC            dcsc0;
    DCSC            dcsc1;
    bt463           ramdac;

    ND_Addrbank*    mem_banks[65536];
    Uint8           ram[64*1024*1024];
    Uint8           vram[4*1024*1024];
    Uint8           rom[128*1024];
    Uint8           rom_command;
    Uint32          rom_last_addr;;
    Uint8           dmem[512];
    Uint32          bankmask[4];

    NextDimension(int slot);
    void mem_init(void);
    void init_mem_banks(void);
    void map_banks (ND_Addrbank *bank, int start, int size);

    virtual Uint32 board_lget(Uint32 addr);
    virtual Uint16 board_wget(Uint32 addr);
    virtual Uint8  board_bget(Uint32 addr);
    virtual void   board_lput(Uint32 addr, Uint32 val);
    virtual void   board_wput(Uint32 addr, Uint16 val);
    virtual void   board_bput(Uint32 addr, Uint8 val);

    virtual Uint32 slot_lget(Uint32 addr);
    virtual Uint16 slot_wget(Uint32 addr);
    virtual Uint8  slot_bget(Uint32 addr);
    virtual void   slot_lput(Uint32 addr, Uint32 val);
    virtual void   slot_wput(Uint32 addr, Uint16 val);
    virtual void   slot_bput(Uint32 addr, Uint8 val);

    virtual void   reset(void);
    virtual void   pause(bool pause);

    static Uint8  i860_cs8get  (NextDimension* nd, Uint32 addr);
    static void   i860_rd8_be  (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd16_be (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd32_be (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd64_be (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd128_be(NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_wr8_be  (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr16_be (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr32_be (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr64_be (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr128_be(NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_rd8_le  (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd16_le (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd32_le (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd64_le (NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_rd128_le(NextDimension* nd, Uint32 addr, Uint32* val);
    static void   i860_wr8_le  (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr16_le (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr32_le (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr64_le (NextDimension* nd, Uint32 addr, const Uint32* val);
    static void   i860_wr128_le(NextDimension* nd, Uint32 addr, const Uint32* val);

    Uint8  rom_read(Uint32 addr);
    void   rom_write(Uint32 addr, Uint8 val);
    void   rom_load();
        
    bool   handle_msgs(void);  /* i860 thread message handler */
    void   send_msg(int msg);

    void   set_blank_state(int src, bool state);
    void   video_dev_write(Uint8 addr, Uint32 step, Uint8 data);

    bool   dbg_cmd(const char* buf);

    virtual ~NextDimension();
};

#endif /* __cplusplus */

#endif /* __DIMENSION_H__ */
