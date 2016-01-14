/***************************************************************************

    i860.c

    Interface file for the Intel i860 emulator.

    Copyright (C) 1995-present Jason Eckhardt (jle@rice.edu)
    Released for general non-commercial use under the MAME license
    with the additional requirement that you are free to use and
    redistribute this code in modified or unmodified form, provided
    you list me in the credits.
    Visit http://mamedev.org for licensing and usage restrictions.

    Changes for previous/NeXTdimension by Simon Schubiger (SC)

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "i860.hpp"

static i860_cpu_device nd_i860;

extern "C" {
    void nd_i860_init() {
        nd_i860.init();
    }
	
	void nd_i860_uninit() {
        nd_i860.uninit();
	}
	
	int nd_speed_hack;
	
	void nd_set_speed_hack(int state) {
		nd_speed_hack = state;
	}
    
    void nd_start_debugger(void) {
        nd_i860.send_msg(MSG_DBG_BREAK);
    }

    void i860_Run(int nHostCycles) {
#if ENABLE_I860_THREAD
        nd_i860.check_debug_lock();
#else
        nd_i860.handle_msgs();

        if(nd_i860.is_halted()) return;

        if (nd_speed_hack) {
            while(nHostCycles) {
                nd_i860.run_cycle(2);
                nHostCycles -= 2;
            }
        } else
            nd_i860.run_cycle(nHostCycles);
#endif
        nd_nbic_interrupt();
	}
    
    int i860_thread(void* data) {
        ((i860_cpu_device*)data)->run();
        return 0;
    }
    
    void i860_reset() {
        nd_i860.send_msg(MSG_I860_RESET);
    }
}

i860_cpu_device::i860_cpu_device() {
#if ENABLE_I860_THREAD
    m_thread = NULL;
#endif
    m_halt = true;
}

inline UINT32 i860_cpu_device::rd32i(UINT32 addr) {
    return nd_board_lget(addr^4);
}

inline void i860_cpu_device::wr32i(UINT32 addr, UINT32 val) {
    nd_board_lput(addr^4, val);
}

inline UINT8 i860_cpu_device::rdcs8(UINT32 addr) {
    return nd_board_cs8get(addr);
}

inline UINT8 i860_cpu_device::rd8(UINT32 addr) {
    if (GET_EPSR_BE())  return nd_board_bget(addr);
    else                return nd_board_bget(addr^7);
}

inline UINT16 i860_cpu_device::rd16(UINT32 addr) {
    if (GET_EPSR_BE())  return nd_board_wget(addr);
    else                return nd_board_wget(addr^6);
}

inline UINT32 i860_cpu_device::rd32(UINT32 addr) {
    if(GET_EPSR_BE())   return nd_board_lget(addr);
    else                return nd_board_lget(addr^4);
}

inline void i860_cpu_device::wr8(UINT32 addr, UINT8 val) {
    if (GET_EPSR_BE())  nd_board_bput(addr,   val);
    else                nd_board_bput(addr^7, val);
}

inline void i860_cpu_device::wr16(UINT32 addr, UINT16 val) {
    if (GET_EPSR_BE())  nd_board_wput(addr,   val);
    else                nd_board_wput(addr^6, val);
}

inline void i860_cpu_device::wr32(UINT32 addr, UINT32 val) {
    if (GET_EPSR_BE())  nd_board_lput(addr  , val);
    else                nd_board_lput(addr^4, val);
}

inline void i860_cpu_device::rddata(UINT32 addr, int size, UINT8* data) {
    switch(size) {
        case 4:
            *((UINT32*)data) = rd32(addr);
            break;
        case 8:
            ((UINT32*)data)[0] = rd32(addr+4);
            ((UINT32*)data)[1] = rd32(addr+0);
            break;
        case 16:
            ((UINT32*)data)[0] = rd32(addr+4);
            ((UINT32*)data)[1] = rd32(addr+0);
            ((UINT32*)data)[2] = rd32(addr+12);
            ((UINT32*)data)[3] = rd32(addr+8);
            break;
    }
}

inline void i860_cpu_device::wrdata(UINT32 addr, int size, UINT8* data) {
    switch(size) {
        case 4:
            wr32(addr, *((UINT32*)data));
            break;
        case 8:
            wr32(addr+4, ((UINT32*)data)[0]);
            wr32(addr+0, ((UINT32*)data)[1]);
            break;
        case 16:
            wr32(addr+4,  ((UINT32*)data)[0]);
            wr32(addr+0,  ((UINT32*)data)[1]);
            wr32(addr+12, ((UINT32*)data)[2]);
            wr32(addr+8,  ((UINT32*)data)[3]);
            break;
    }
}

inline UINT32 i860_cpu_device::get_iregval(int gr) {
    return m_iregs[gr];
}

inline void i860_cpu_device::set_iregval(int gr, UINT32 val) {
    if(gr > 0)
        m_iregs[gr] = val;
}

inline float i860_cpu_device::get_fregval_s (int fr) {
    return *(float*)(&m_fregs[fr * 4]);
}

inline void i860_cpu_device::set_fregval_s (int fr, float s) {
    if(fr > 1)
        *(float*)(&m_fregs[fr * 4]) = s;
}

inline double i860_cpu_device::get_fregval_d (int fr) {
    return *(double*)(&m_fregs[fr * 4]);
}

inline void i860_cpu_device::set_fregval_d (int fr, double d) {
    if(fr > 1)
        *(double*)(&m_fregs[fr * 4]) = d;
}

inline void i860_cpu_device::SET_PSR_CC(int val) {
    if(!(m_dim_cc_valid))
        m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 2)) | ((val & 1) << 2);
}

void i860_cpu_device::send_msg(int msg) {
    /* Get pending messages */
    int pmsg = SDL_AtomicGet(&m_port);
    /* If this message is not already pending, add it. */
    if(!(pmsg & msg))
        SDL_AtomicAdd(&m_port, msg);
}

void i860_cpu_device::handle_trap(UINT32 savepc, bool dim) {
    static char buffer[128];
    buffer[0] = 0;
    strcat(buffer, "TRAP");
    if(m_flow & TRAP_NORMAL)        strcat(buffer, " [Normal]");
    if(m_flow & TRAP_IN_DELAY_SLOT) strcat(buffer, " [Delay Slot]");
    if(m_flow & TRAP_WAS_EXTERNAL)  strcat(buffer, " [External]");
    if(!(GET_PSR_IT() || GET_PSR_FT() || GET_PSR_IAT() || GET_PSR_DAT() || GET_PSR_IN()))
        strcat(buffer, " >Reset<");
    else {
        if(GET_PSR_IT())  strcat(buffer, " >Instruction Fault<");
        if(GET_PSR_FT())  strcat(buffer, " >Floating Point Fault<");
        if(GET_PSR_IAT()) strcat(buffer, " >Instruction Access Fault<");
        if(GET_PSR_DAT()) strcat(buffer, " >Data Access Fault<");
        if(GET_PSR_IN())  strcat(buffer, " >Interrupt<");
    }
    
    if(!((GET_PSR_IAT() || GET_PSR_DAT() || GET_PSR_IN()))) {
        debugger('d', buffer);
    } else {
        //            Log_Printf(LOG_WARN, "[i860] %s", buffer);
    }
    
    if(m_dim)
        Log_Printf(LOG_WARN, "[i860] Trap while DIM %s", buffer);
    

    /* If we need to trap, change PC to trap address.
     Also set supervisor mode, copy U and IM to their
     previous versions, clear IM.  */
    if(m_flow & TRAP_WAS_EXTERNAL) {
        if (GET_PC_UPDATED()) {
            m_cregs[CR_FIR] = m_pc;
        } else {
            m_cregs[CR_FIR] = savepc + 4;
        }
    }
    else if (m_flow & TRAP_IN_DELAY_SLOT) {
        m_cregs[CR_FIR] = savepc + 4;
    }
    else
        m_cregs[CR_FIR] = savepc;
    
    m_flow |= FIR_GETS_TRAP;
    SET_PSR_PU (GET_PSR_U ());
    SET_PSR_PIM (GET_PSR_IM ());
    SET_PSR_U (0);
    SET_PSR_IM (0);
    // (SC) we don't emulate DIM traps for now
    //SET_PSR_DIM (m_dim != DIM_NONE);
    //m_dim = false;
    //SET_PSR_DS ((m_dim != DIM_NONE) != dim);
    SET_PSR_DIM (0);
    m_save_dim = m_dim;
    m_dim      = DIM_NONE;
    SET_PSR_DS (0);
    m_pc = 0xffffff00;
}

void i860_cpu_device::run_cycle(int nHostCycles) {
    m_dim_cc_valid = false;
    CLEAR_FLOW();
    bool   dim_insn = false;
    UINT64 insn64   = ifetch64(m_pc);
    
    if(!(m_pc & 4)) {
        UINT32 savepc  = m_pc;
        
        if(m_single_stepping) debugger(0,0);
        
        UINT32 insnLow = insn64;
        if(insnLow == INSN_FNOP || insnLow == INSN_FNOP_DIM)
            dim_insn = m_dim != DIM_NONE;
        else if((insnLow & INSN_MASK_DIM) == INSN_FP_DIM)
            dim_insn = true;
        
        decode_exec(insnLow);
        
        if (PENDING_TRAP()) {
            handle_trap(savepc, dim_insn);
            goto done;
        } else if(GET_PC_UPDATED()) {
            goto done;
        } else {
            // If the PC wasn't updated by a control flow instruction, just bump to next sequential instruction.
            m_pc   += 4;
            CLEAR_FLOW();
        }
    }
    
    if(m_pc & 4) {
        UINT32 savepc  = m_pc;
        
        if(m_single_stepping) debugger(0,0);

        UINT32 insnHigh= insn64 >> 32;
        decode_exec(insnHigh);
        
        // only check for external interrupts
        // - on high-word (speedup)
        // - not DIM (safety :-)
        // - when no other traps are pending
        if(!(m_dim) && !(PENDING_TRAP())) {
            if(nd_process_interrupts(nHostCycles))
                gen_interrupt();
            else
                clr_interrupt();
        }
        
        if (PENDING_TRAP()) {
            handle_trap(savepc, dim_insn);
        } else if (GET_PC_UPDATED()) {
            goto done;
        } else {
            // If the PC wasn't updated by a control flow instruction, just bump to next sequential instruction.
            m_pc += 4;
        }
    }
done:
    switch (m_dim) {
        case DIM_NONE:
            if(dim_insn)
                m_dim = DIM_TEMP;
            break;
        case DIM_TEMP:
            m_dim = dim_insn ? DIM_FULL : DIM_NONE;
            break;
        case DIM_FULL:
            if(!(dim_insn))
                m_dim = DIM_TEMP;
            break;
    }
}

int i860_cpu_device::memtest(bool be) {
    const UINT32 P_TEST_ADDR = 0x28000000; // assume ND in slot 2
    
    m_cregs[CR_DIRBASE] = 0; // turn VM off

    UINT8  uint8  = 0x01;
    UINT16 uint16 = 0x0123;
    UINT32 uint32 = 0x01234567;
    UINT64 uint64 = 0x0123456789ABCDEFLL;
    
    int err = be ? 20000 : 30000;
    
    // intel manual example
    SET_EPSR_BE(0);
    
    wr8(P_TEST_ADDR+0, 'A');
    wr8(P_TEST_ADDR+1, 'B');
    wr8(P_TEST_ADDR+2, 'C');
    wr8(P_TEST_ADDR+3, 'D');
    wr8(P_TEST_ADDR+4, 'E');
    wr8(P_TEST_ADDR+5, 'F');
    wr8(P_TEST_ADDR+6, 'G');
    wr8(P_TEST_ADDR+7, 'H');
    
    if(rd8(P_TEST_ADDR+0) != 'A') return err + 100;
    if(rd8(P_TEST_ADDR+1) != 'B') return err + 101;
    if(rd8(P_TEST_ADDR+2) != 'C') return err + 102;
    if(rd8(P_TEST_ADDR+3) != 'D') return err + 103;
    if(rd8(P_TEST_ADDR+4) != 'E') return err + 104;
    if(rd8(P_TEST_ADDR+5) != 'F') return err + 105;
    if(rd8(P_TEST_ADDR+6) != 'G') return err + 106;
    if(rd8(P_TEST_ADDR+7) != 'H') return err + 107;
    
    if(rd16(P_TEST_ADDR+0) != (('B'<<8)|('A'))) return err + 110;
    if(rd16(P_TEST_ADDR+2) != (('D'<<8)|('C'))) return err + 111;
    if(rd16(P_TEST_ADDR+4) != (('F'<<8)|('E'))) return err + 112;
    if(rd16(P_TEST_ADDR+6) != (('H'<<8)|('G'))) return err + 113;

    if(rd32(P_TEST_ADDR+0) != (('D'<<24)|('C'<<16)|('B'<<8)|('A'))) return err + 120;
    if(rd32(P_TEST_ADDR+4) != (('H'<<24)|('G'<<16)|('F'<<8)|('E'))) return err + 121;

    SET_EPSR_BE(1);

    if(rd8(P_TEST_ADDR+0) != 'H') return err + 200;
    if(rd8(P_TEST_ADDR+1) != 'G') return err + 201;
    if(rd8(P_TEST_ADDR+2) != 'F') return err + 202;
    if(rd8(P_TEST_ADDR+3) != 'E') return err + 203;
    if(rd8(P_TEST_ADDR+4) != 'D') return err + 204;
    if(rd8(P_TEST_ADDR+5) != 'C') return err + 205;
    if(rd8(P_TEST_ADDR+6) != 'B') return err + 206;
    if(rd8(P_TEST_ADDR+7) != 'A') return err + 207;
    
    if(rd16(P_TEST_ADDR+0) != (('H'<<8)|('G'))) return err + 210;
    if(rd16(P_TEST_ADDR+2) != (('F'<<8)|('E'))) return err + 211;
    if(rd16(P_TEST_ADDR+4) != (('D'<<8)|('C'))) return err + 212;
    if(rd16(P_TEST_ADDR+6) != (('B'<<8)|('A'))) return err + 213;
    
    if(rd32(P_TEST_ADDR+0) != (('H'<<24)|('G'<<16)|('F'<<8)|('E'))) return err + 220;
    if(rd32(P_TEST_ADDR+4) != (('D'<<24)|('C'<<16)|('B'<<8)|('A'))) return err + 221;
    
    // some register and mem r/w tests
    
    SET_EPSR_BE(be);
    
    wr8(P_TEST_ADDR, uint8);
    if(rd8(P_TEST_ADDR) != 0x01) return err;
    
    wr16(P_TEST_ADDR, uint16);
    if(rd16(P_TEST_ADDR) != 0x0123) return err+1;
    
    wr32(P_TEST_ADDR, uint32);
    if(rd32(P_TEST_ADDR) != 0x01234567) return err+2;
    
    fp_readmem_emu(P_TEST_ADDR, 4, (UINT8*)&uint32);
    if(uint32 != 0x01234567) return 20003;
    
    fp_writemem_emu(P_TEST_ADDR, 4, (UINT8*)&uint32, 0xff);
    if(rd32(P_TEST_ADDR) != 0x01234567) return err+3;
    
    UINT8* uint8p = (UINT8*)&uint64;
    set_fregval_d(2, *((double*)uint8p));
    fp_writemem_emu(P_TEST_ADDR, 8, &m_fregs[8], 0xff);
    fp_readmem_emu (P_TEST_ADDR, 8, &m_fregs[8]);
    *((double*)&uint64) = get_fregval_d(2);
    if(uint64 != 0x0123456789ABCDEFLL) return err+4;
    
    UINT32 lo = rd32(P_TEST_ADDR+0);
    UINT32 hi = rd32(P_TEST_ADDR+4);
    
    if(lo != 0x01234567) return err+5;
    if(hi != 0x89ABCDEF) return err+6;
    
    return 0;
}

void i860_cpu_device::init() {
#if ENABLE_I860_THREAD
    if(!(is_halted())) uninit();
#endif
    
    m_single_stepping   = 0;
    m_lastcmd           = 0;
    m_console_idx       = 0;
    m_break_on_next_msg = false;
    m_dim               = DIM_NONE;

    // some sanity checks for endianess
    int    err    = 0;
    {
        UINT32 uint32 = 0x01234567;
        UINT8* uint8p = (UINT8*)&uint32;
        if(uint8p[3] != 0x01) {err = 1; goto error;}
        if(uint8p[2] != 0x23) {err = 2; goto error;}
        if(uint8p[1] != 0x45) {err = 3; goto error;}
        if(uint8p[0] != 0x67) {err = 4; goto error;}
        
        for(int i = 0; i < 32; i++) {
            uint8p[3] = i;
            set_fregval_s(i, *((float*)uint8p));
        }
        if(get_fregval_s(0) != 0)   {err = 198; goto error;}
        if(get_fregval_s(1) != 0)   {err = 199; goto error;}
        for(int i = 2; i < 32; i++) {
            uint8p[3] = i;
            if(get_fregval_s(i) != *((float*)uint8p))
                {err = 100+i; goto error;}
        }
        for(int i = 2; i < 32; i++) {
            if(m_fregs[i*4+3] != i)    {err = 200+i; goto error;}
            if(m_fregs[i*4+2] != 0x23) {err = 200+i; goto error;}
            if(m_fregs[i*4+1] != 0x45) {err = 200+i; goto error;}
            if(m_fregs[i*4+0] != 0x67) {err = 200+i; goto error;}
        }
    }
    
    {
        UINT64 uint64 = 0x0123456789ABCDEFLL;
        UINT8* uint8p = (UINT8*)&uint64;
        if(uint8p[7] != 0x01) {err = 10001; goto error;}
        if(uint8p[6] != 0x23) {err = 10002; goto error;}
        if(uint8p[5] != 0x45) {err = 10003; goto error;}
        if(uint8p[4] != 0x67) {err = 10004; goto error;}
        if(uint8p[3] != 0x89) {err = 10005; goto error;}
        if(uint8p[2] != 0xAB) {err = 10006; goto error;}
        if(uint8p[1] != 0xCD) {err = 10007; goto error;}
        if(uint8p[0] != 0xEF) {err = 10008; goto error;}
        
        for(int i = 0; i < 16; i++) {
            uint8p[7] = i;
            set_fregval_d(i*2, *((double*)uint8p));
        }
        if(get_fregval_d(0) != 0)
            {err = 10199; goto error;}
        for(int i = 1; i < 16; i++) {
            uint8p[7] = i;
            if(get_fregval_d(i*2) != *((double*)uint8p))
                {err = 10100+i; goto error;}
        }
        for(int i = 2; i < 32; i += 2) {
            float hi = get_fregval_s(i+1);
            float lo = get_fregval_s(i+0);
            if((*(UINT32*)&hi) != (0x00234567 | (i<<23))) {err = 10100+i; goto error;}
            if((*(UINT32*)&lo) !=  0x89ABCDEF)            {err = 10100+i; goto error;}
        }
        for(int i = 1; i < 16; i++) {
            if(m_fregs[i*8+7] != i)    {err = 10200+i; goto error;}
            if(m_fregs[i*8+6] != 0x23) {err = 10200+i; goto error;}
            if(m_fregs[i*8+5] != 0x45) {err = 10200+i; goto error;}
            if(m_fregs[i*8+4] != 0x67) {err = 10200+i; goto error;}
            if(m_fregs[i*8+3] != 0x89) {err = 10200+i; goto error;}
            if(m_fregs[i*8+2] != 0xAB) {err = 10200+i; goto error;}
            if(m_fregs[i*8+1] != 0xCD) {err = 10200+i; goto error;}
            if(m_fregs[i*8+0] != 0xEF) {err = 10200+i; goto error;}
        }
    }
    
    err = memtest(true); if(err) goto error;
    err = memtest(false); if(err) goto error;
    
error:
    if(err) {
        fprintf(stderr, "NeXTdimension i860 emulator requires a little-endian host. This system seems to be big endian. Error %d. Exiting.\n", err);
        fflush(stderr);
        exit(err);
    }

    send_msg(MSG_I860_RESET);
#if ENABLE_I860_THREAD
    m_thread = SDL_CreateThread(i860_thread, "[ND] i860", this);
#endif
}

void i860_cpu_device::uninit() {
	halt(true);
    send_msg(MSG_I860_KILL);
#if ENABLE_I860_THREAD
    int status;
    if(m_thread) {
        SDL_WaitThread(m_thread, &status);
        m_thread = NULL;
    }
    send_msg(MSG_NONE);
#endif
}

/* Message disaptcher */
bool i860_cpu_device::handle_msgs() {
    int msg = SDL_AtomicSet(&m_port, 0);
    if(msg & MSG_I860_KILL)
        return false;
    if(msg & MSG_I860_RESET)
        reset();
    if(msg & MSG_DBG_BREAK)
        debugger('d', "BREAK at pc=%08X", m_pc);
    return true;
}

void i860_cpu_device::run() {
    while(handle_msgs()) {
        
        /* Sleep a bit if halted */
        if(m_halt) {
            SDL_Delay(100);
            continue;
        }
        
        /* Run i860 */
        run_cycle(2);
    }
}

offs_t i860_cpu_device::disasm(char* buffer, offs_t pc) {
    return pc + i860_disassembler(pc, ifetch_notrap(pc), buffer);
}

/**************************************************************************
 * The actual decode and execute code.
 **************************************************************************/
#include "i860dec.cpp"

/**************************************************************************
 * The debugger code.
 **************************************************************************/
#include "i860dbg.cpp"