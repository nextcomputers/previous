#pragma once

#ifndef __DIMENSION_H__
#define __DIMENSION_H__

#define ND_SLOT 2

extern Uint32 nd_slot_lget(Uint32 addr);
extern Uint16 nd_slot_wget(Uint32 addr);
extern Uint8  nd_slot_bget(Uint32 addr);
extern void   nd_slot_lput(Uint32 addr, Uint32 l);
extern void   nd_slot_wput(Uint32 addr, Uint16 w);
extern void   nd_slot_bput(Uint32 addr, Uint8 b);

extern Uint32 nd_board_lget(Uint32 addr);
extern Uint16 nd_board_wget(Uint32 addr);
extern Uint8  nd_board_bget(Uint32 addr);
extern void   nd_board_lput(Uint32 addr, Uint32 l);
extern void   nd_board_wput(Uint32 addr, Uint16 w);
extern void   nd_board_bput(Uint32 addr, Uint8 b);
extern Uint8  nd_board_cs8get(Uint32 addr);

extern Uint8  ND_ram[64*1024*1024];
extern Uint8  ND_rom[128*1024];
extern Uint32 ND_vram_off;
extern Uint8  ND_vram[4*1024*1024];
extern void (*i860_Run)(int);

extern void dimension_init();
extern void dimension_uninit();
extern void nd_i860_init();
extern void nd_i860_uninit();
extern void i860_reset();
extern void i860_interrupt();
extern void nd_start_debugger();
extern const char* nd_reports(double realTime, double hostTime);

#define ND_LOG_IO_RD LOG_NONE
#define ND_LOG_IO_WR LOG_NONE

#endif /* __DIMENSION_H__ */
