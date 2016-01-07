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
    
    void i860_Run(int nHostCycles) {
        if(i860_dbg_break(nd_i860.m_pc))
            nd_i860.debugger("BREAK at pc=%08X", nd_i860.m_pc);
        
        nd_i860.run_cycle(nHostCycles);
    }
    
    offs_t i860_Disasm(char* buffer, offs_t pc) {
        return nd_i860.disasm(buffer, pc);
    }
    
    void i860_reset() {
        nd_i860.i860_reset();
    }
}

void i860_cpu_device::run_cycle(int nHostCycles) {
    // Statusbar_Seti860Led(m_halt ? 0 : (GET_DIRBASE_CS8() ? 1 : 2));

    if(m_halt) return;
    
    UINT32 savepc = m_pc;
    m_pc_updated = 0;
    m_pending_trap = 0;
    
    savepc = m_pc;
    
    decode_exec (ifetch (m_pc), 1);
    
    if(nd_process_interrupts(nHostCycles))
        i860_gen_interrupt();
    else
        i860_clr_interrupt();

    m_exiting_ifetch = 0;
    m_exiting_readmem = 0;
    
    if (m_pending_trap)
    {
        static char buffer[128];
        if(!((GET_PSR_IAT() || GET_PSR_DAT() || GET_PSR_IN()))) {
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
            debugger(buffer);
        }

        /* If we need to trap, change PC to trap address.
         Also set supervisor mode, copy U and IM to their
         previous versions, clear IM.  */
        if ((m_pending_trap & TRAP_WAS_EXTERNAL) || (GET_EPSR_INT () && GET_PSR_IN ()))
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
        debugger(0);
}

void i860_cpu_device::init() {
    m_single_stepping = 0;
    m_lastcmd         = 0;

    // some sanity checks for endianess
    int    err    = 0;
    {
        UINT32 uint32 = 0x01234567;
        UINT8* uint8p = (UINT8*)&uint32;
        if(uint8p[3] != 0x01) err = 1;
        if(uint8p[2] != 0x23) err = 2;
        if(uint8p[1] != 0x45) err = 3;
        if(uint8p[0] != 0x67) err = 4;
        
        for(int i = 0; i < 32; i++) {
            uint8p[3] = i;
            set_fregval_s(i, *((float*)uint8p));
        }
        if(get_fregval_s(0) != 0)   err = 198;
        if(get_fregval_s(1) != 0)   err = 199;
        for(int i = 2; i < 32; i++) {
            uint8p[3] = i;
            if(get_fregval_s(i) != *((float*)uint8p))
                err = 100+i;
        }
        for(int i = 2; i < 32; i++) {
            if(m_frg[i*4+3] != i)    err = 200+i;
            if(m_frg[i*4+2] != 0x23) err = 200+i;
            if(m_frg[i*4+1] != 0x45) err = 200+i;
            if(m_frg[i*4+0] != 0x67) err = 200+i;
        }
    }
    
    {
        UINT64 uint64 = 0x0123456789ABCDEF;
        UINT8* uint8p = (UINT8*)&uint64;
        if(uint8p[7] != 0x01) err = 10001;
        if(uint8p[6] != 0x23) err = 10002;
        if(uint8p[5] != 0x45) err = 10003;
        if(uint8p[4] != 0x67) err = 10004;
        if(uint8p[3] != 0x89) err = 10005;
        if(uint8p[2] != 0xAB) err = 10006;
        if(uint8p[1] != 0xCD) err = 10007;
        if(uint8p[0] != 0xEF) err = 10008;
        
        for(int i = 0; i < 16; i++) {
            uint8p[7] = i;
            set_fregval_d(i*2, *((double*)uint8p));
        }
        if(get_fregval_d(0) != 0)
            err = 10199;
        for(int i = 1; i < 16; i++) {
            uint8p[7] = i;
            if(get_fregval_d(i*2) != *((double*)uint8p))
                err = 10100+i;
        }
        for(int i = 1; i < 16; i++) {
            if(m_frg[i*8+7] != i)    err = 10200+i;
            if(m_frg[i*8+6] != 0x23) err = 10200+i;
            if(m_frg[i*8+5] != 0x45) err = 10200+i;
            if(m_frg[i*8+4] != 0x67) err = 10200+i;
            if(m_frg[i*8+3] != 0x89) err = 10200+i;
            if(m_frg[i*8+2] != 0xAB) err = 10200+i;
            if(m_frg[i*8+1] != 0xCD) err = 10200+i;
            if(m_frg[i*8+0] != 0xEF) err = 10200+i;
        }
        
    }
    
    if(err) {
        fprintf(stderr, "NeXTdimension i860 emulator requires a little-endian host. This system seems to be big endian. Error %d. Exiting.\n", err);
        fflush(stderr);
        exit(err);
    }
    
    i860_reset();
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