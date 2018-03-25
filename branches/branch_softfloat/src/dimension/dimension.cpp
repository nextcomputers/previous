#include "configuration.h"
#include "m68000.h"
#include "dimension.hpp"
#include "nd_nbic.hpp"
#include "nd_mem.hpp"
#include "nd_sdl.hpp"

NextDimension::NextDimension(int slot) :
    NextBusBoard(slot),
    sdl(slot, (Uint32*)vram),
    i860(this),
    nbic(slot, ND_NBIC_ID),
    mc(this),
    dp(this),
    dmcd(this),
    dcsc0(this, 0),
    dcsc1(this, 1),
    rom_command(0),
    rom_last_addr(0)
{
    i860.uninit();
    nbic.init();
    mc.init();
    dp.init();
    mem_init();
    i860.init();
    sdl.init();
}

NextDimension::~NextDimension() {
    i860.uninit();
    sdl.destroy();
}

void NextDimension::reset(void) {
    i860.set_run_func();
    switch (slot) {
        case 2: sdl.start_interrupts(INTERRUPT_ND0_VBL, INTERRUPT_ND0_VIDEO_VBL); break;
        case 4: sdl.start_interrupts(INTERRUPT_ND1_VBL, INTERRUPT_ND1_VIDEO_VBL); break;
        case 6: sdl.start_interrupts(INTERRUPT_ND2_VBL, INTERRUPT_ND2_VIDEO_VBL); break;
    }
}

void NextDimension::interrupt(interrupt_id intr) {
    switch(intr) {
        case INTERRUPT_ND0_VBL:
        case INTERRUPT_ND1_VBL:
        case INTERRUPT_ND2_VBL:
            sdl.vbl_handler(intr);
            break;
        case INTERRUPT_ND0_VIDEO_VBL:
        case INTERRUPT_ND1_VIDEO_VBL:
        case INTERRUPT_ND2_VIDEO_VBL:
            sdl.video_vbl_handler(intr);
            break;
        default:
            break;
    }
}

void NextDimension::pause(bool pause) {
    i860.pause(pause);
    sdl.pause(pause);
}

/* NeXTdimension board memory access (m68k) */

 Uint32 NextDimension::board_lget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    Uint32 result = nd68k_longget(addr);
    // (SC) delay m68k read on csr0 while in ROM (CS8=1)to give ND some time to start up.
    if(addr == 0xFF800000 && (result & 0x02))
        M68000_AddCycles(ConfigureParams.System.nCpuFreq * 100);
    return result;
}

 Uint16 NextDimension::board_wget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd68k_wordget(addr);
}

 Uint8 NextDimension::board_bget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd68k_byteget(addr);
}

 void NextDimension::board_lput(Uint32 addr, Uint32 l) {
    addr |= ND_BOARD_BITS;
    nd68k_longput(addr, l);
}

 void NextDimension::board_wput(Uint32 addr, Uint16 w) {
    addr |= ND_BOARD_BITS;
    nd68k_wordput(addr, w);
}

 void NextDimension::board_bput(Uint32 addr, Uint8 b) {
    addr |= ND_BOARD_BITS;
    nd68k_byteput(addr, b);
}

/* NeXTdimension slot memory access */
Uint32 NextDimension::slot_lget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd68k_longget(addr);
    } else {
        return nbic.lget(addr);
    }
}

Uint16 NextDimension::slot_wget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd68k_wordget(addr);
    } else {
        return nbic.wget(addr);
    }
}

Uint8 NextDimension::slot_bget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd68k_byteget(addr);
    } else {
        return nbic.bget(addr);
    }
}

void NextDimension::slot_lput(Uint32 addr, Uint32 l) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd68k_longput(addr, l);
    } else {
        nbic.lput(addr, l);
    }
}

void NextDimension::slot_wput(Uint32 addr, Uint16 w) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd68k_wordput(addr, w);
    } else {
        nbic.wput(addr, w);
    }
}

void NextDimension::slot_bput(Uint32 addr, Uint8 b) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd68k_byteput(addr, b);
    } else {
        nbic.bput(addr, b);
    }
}

void NextDimension::send_msg(int msg) {
    host_lock(&m_port_lock);
    m_port |= msg;
    host_unlock(&m_port_lock);
}

/* NeXTdimension board memory access (i860) */

Uint8  NextDimension::i860_cs8get(NextDimension* nd, Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_cs8get(addr);
}

void   NextDimension::i860_rd8_be(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    *((Uint8*)val) = nd_byteget(addr);
}

void   NextDimension::i860_rd16_be(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr |= ND_BOARD_BITS;
    *((Uint16*)val) = nd_wordget(addr);
}

void   NextDimension::i860_rd32_be(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr);
}

void   NextDimension::i860_rd64_be(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr+4);
    val[1] = nd_longget(addr+0);
}

void   NextDimension::i860_rd128_be(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr   |= ND_BOARD_BITS;
    val[0]  = nd_longget(addr+4);
    val[1]  = nd_longget(addr+0);
    val[2]  = nd_longget(addr+12);
    val[3]  = nd_longget(addr+8);
}

void   NextDimension::i860_wr8_be(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr, *((const Uint8*)val));
}

void   NextDimension::i860_wr16_be(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr, *((const Uint16*)val));
}

void   NextDimension::i860_wr32_be(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr, val[0]);
}

void   NextDimension::i860_wr64_be(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+4, val[0]);
    nd_longput(addr+0, val[1]);
}

void   NextDimension::i860_wr128_be(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+4,  val[0]);
    nd_longput(addr+0,  val[1]);
    nd_longput(addr+12, val[2]);
    nd_longput(addr+8,  val[3]);
}

void   NextDimension::i860_rd8_le(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    *((Uint8*)val) = nd_byteget(addr^7);
}

void   NextDimension::i860_rd16_le(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr |= ND_BOARD_BITS;
    *((Uint16*)val) = nd_wordget(addr^6);
}

void   NextDimension::i860_rd32_le(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr^4);
}

void   NextDimension::i860_rd64_le(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr+0);
    val[1] = nd_longget(addr+4);
}

void   NextDimension::i860_rd128_le(NextDimension* nd, Uint32 addr, Uint32* val) {
    addr   |= ND_BOARD_BITS;
    val[0]  = nd_longget(addr+0);
    val[1]  = nd_longget(addr+4);
    val[2]  = nd_longget(addr+8);
    val[3]  = nd_longget(addr+12);
}

void   NextDimension::i860_wr8_le(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr^7, *((const Uint8*)val));
}

void   NextDimension::i860_wr16_le(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr^6, *((const Uint16*)val));
}

void   NextDimension::i860_wr32_le(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr^4, val[0]);
}

void   NextDimension::i860_wr64_le(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+0, val[0]);
    nd_longput(addr+4, val[1]);
}

void   NextDimension::i860_wr128_le(NextDimension* nd, Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+0,  val[0]);
    nd_longput(addr+4,  val[1]);
    nd_longput(addr+8,  val[2]);
    nd_longput(addr+12, val[3]);
}

/* Message disaptcher - executed on i860 thread, safe to call i860 methods */
bool NextDimension::handle_msgs(void) {
    host_lock(&m_port_lock);
    int msg = m_port;
    m_port = 0;
    host_unlock(&m_port_lock);
    
    if(msg & MSG_DISPLAY_BLANK)
        set_blank_state(ND_DISPLAY, host_blank_state(slot, ND_DISPLAY));
    if(msg & MSG_VIDEO_BLANK)
        set_blank_state(ND_VIDEO, host_blank_state(slot, ND_VIDEO));

    return i860.handle_msgs(msg);
}

void nd_start_debugger(void) {
    for(int slot = 2; slot < 16; slot += 2) {
        if(NextDimension* d = dynamic_cast<NextDimension*>(nextbus[slot])) {
            d->send_msg(MSG_DBG_BREAK);
        }
    }
}

extern "C" {
    void nd_display_blank(int slot) {
        ((NextDimension*)nextbus[slot])->send_msg(MSG_DISPLAY_BLANK);
    }

    void nd_video_blank(int slot) {
        ((NextDimension*)nextbus[slot])->send_msg(MSG_VIDEO_BLANK);
    }

    const char* nd_reports(int slot, double realTime, double hostTime) {
        return ((NextDimension*)nextbus[slot])->i860.reports(realTime, hostTime);
    }
}



