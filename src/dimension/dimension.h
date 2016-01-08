#pragma once

#ifndef __DIMENSION_H__
#define __DIMENSION_H__

#define ENABLE_DIMENSION    0
#define ND_SLOT 2

#if ENABLE_DIMENSION

Uint32 nd_slot_lget(Uint32 addr);
Uint16 nd_slot_wget(Uint32 addr);
Uint8 nd_slot_bget(Uint32 addr);
void nd_slot_lput(Uint32 addr, Uint32 l);
void nd_slot_wput(Uint32 addr, Uint16 w);
void nd_slot_bput(Uint32 addr, Uint8 b);

Uint32 nd_board_lget(Uint32 addr);
Uint16 nd_board_wget(Uint32 addr);
Uint8 nd_board_bget(Uint32 addr);
void nd_board_lput(Uint32 addr, Uint32 l);
void nd_board_wput(Uint32 addr, Uint16 w);
void nd_board_bput(Uint32 addr, Uint8 b);
Uint8 nd_board_cs8get(Uint32 addr);

Uint64 swap64(Uint64 val);
Uint32 swap32(Uint32 val);
Uint16 swap16(Uint16 val);

extern Uint8 ND_ram[64*1024*1024];
extern Uint8 ND_rom[128*1024];
extern Uint8 ND_vram[4*1024*1024];

void dimension_reset(void);
void nd_i860_init();
void i860_Run(int nHostCycles);
bool i860_dbg_break(Uint32 addr);
void i860_reset();
void nd_start_debugger(void);

#define ND_LOG_IO_RD LOG_NONE
#define ND_LOG_IO_WR LOG_NONE

#endif

#endif /* __DIMENSION_H__ */
