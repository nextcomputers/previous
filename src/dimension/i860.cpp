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
    
    void i860_Run(int nHostCycles) {
		if (nd_speed_hack) {
			while(nHostCycles) {
				if(i860_dbg_break(nd_i860.m_pc))
					nd_i860.debugger('d', "BREAK at pc=%08X", nd_i860.m_pc);
				
				nd_i860.run_cycle(1);
				nHostCycles -= 1;
			}
		} else {
			if(i860_dbg_break(nd_i860.m_pc))
				nd_i860.debugger('d', "BREAK at pc=%08X", nd_i860.m_pc);
			
			nd_i860.run_cycle(nHostCycles);
		}
	}
	
    offs_t i860_Disasm(char* buffer, offs_t pc) {
        return nd_i860.disasm(buffer, pc);
    }
    
    void i860_reset() {
        nd_i860.i860_reset();
    }
}

void i860_cpu_device::run_cycle(int nHostCycles) {

    if(m_halt) return;
    
    UINT32 savepc = m_pc;
    m_pc_updated = 0;
    m_pending_trap = 0;
    
    savepc = m_pc;
    
    decode_exec (ifetch (m_pc), 1);
    
    if(!(m_pending_trap)) {
        if(nd_process_interrupts(nHostCycles))
            i860_gen_interrupt();
        else
            i860_clr_interrupt();
    }
    
    m_exiting_ifetch = 0;
    m_exiting_readmem = 0;
    
    if (m_pending_trap) {
        static char buffer[128];
        buffer[0] = 0;
        strcat(buffer, "TRAP");
        if(m_pending_trap & TRAP_NORMAL)        strcat(buffer, " [Normal]");
        if(m_pending_trap & TRAP_IN_DELAY_SLOT) strcat(buffer, " [Delay Slot]");
        if(m_pending_trap & TRAP_WAS_EXTERNAL)  strcat(buffer, " [External]");
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
//            Log_Printf(LOG_WARN, "[ND] %s", buffer);
        }

        /* If we need to trap, change PC to trap address.
         Also set supervisor mode, copy U and IM to their
         previous versions, clear IM.  */
        if(m_pending_trap & TRAP_WAS_EXTERNAL)
        {
            if (!m_pc_updated)
                m_cregs[CR_FIR] = savepc + 4;
            else
                m_cregs[CR_FIR] = m_pc;
        }
        else if (m_pending_trap & TRAP_IN_DELAY_SLOT)
        {
            m_cregs[CR_FIR] = savepc + 4;
        }
        else
            m_cregs[CR_FIR] = savepc;
        
        m_fir_gets_trap_addr = 1;
        SET_PSR_PU (GET_PSR_U ());
        SET_PSR_PIM (GET_PSR_IM ());
        SET_PSR_U (0);
        SET_PSR_IM (0);
        SET_PSR_DIM (0);
        SET_PSR_DS (0);
        m_pc = 0xffffff00;
        m_pending_trap = 0;
    }
    else if (!m_pc_updated)
    {
        /* If the PC wasn't updated by a control flow instruction, just
         bump to next sequential instruction.  */
        m_pc += 4;
    }
    
    if(m_single_stepping)
        debugger(0,0);
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
    fp_writemem_emu(P_TEST_ADDR, 8, &m_frg[8], 0xff);
    fp_readmem_emu (P_TEST_ADDR, 8, &m_frg[8]);
    *((double*)&uint64) = get_fregval_d(2);
    if(uint64 != 0x0123456789ABCDEFLL) return err+4;
    
    UINT32 lo = rd32(P_TEST_ADDR+0);
    UINT32 hi = rd32(P_TEST_ADDR+4);
    
    if(lo != 0x01234567) return err+5;
    if(hi != 0x89ABCDEF) return err+6;
    
    return 0;
}

void i860_cpu_device::init() {
    m_single_stepping   = 0;
    m_lastcmd           = 0;
    m_console_idx       = 0;
    m_break_on_next_msg = false;
    
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
            if(m_frg[i*4+3] != i)    {err = 200+i; goto error;}
            if(m_frg[i*4+2] != 0x23) {err = 200+i; goto error;}
            if(m_frg[i*4+1] != 0x45) {err = 200+i; goto error;}
            if(m_frg[i*4+0] != 0x67) {err = 200+i; goto error;}
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
            if(m_frg[i*8+7] != i)    {err = 10200+i; goto error;}
            if(m_frg[i*8+6] != 0x23) {err = 10200+i; goto error;}
            if(m_frg[i*8+5] != 0x45) {err = 10200+i; goto error;}
            if(m_frg[i*8+4] != 0x67) {err = 10200+i; goto error;}
            if(m_frg[i*8+3] != 0x89) {err = 10200+i; goto error;}
            if(m_frg[i*8+2] != 0xAB) {err = 10200+i; goto error;}
            if(m_frg[i*8+1] != 0xCD) {err = 10200+i; goto error;}
            if(m_frg[i*8+0] != 0xEF) {err = 10200+i; goto error;}
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
    
    i860_reset();
}

void i860_cpu_device::uninit() {
	i860_halt(true);
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