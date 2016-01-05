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

#include "i860.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static i860_cpu_device nd_i860;

extern "C" void nd_i860_init() {
    nd_i860.i860_set_pin(DEC_PIN_BUS_HOLD, 1);
    nd_i860.i860_set_pin(DEC_PIN_RESET, 1);
    nd_i860.init();
}

extern "C" void i860_Run(int nHostCycles) {
    if(i860_dbg_break(nd_i860.m_pc))
       nd_i860.debugger("BREAK at pc=%08X", nd_i860.m_pc);
    
    if(nd_i860.m_halt) return;
    
    nd_i860.run_cycle(nHostCycles);
}

extern "C" offs_t i860_Disasm(char* buffer, offs_t pc) {
    return nd_i860.disasm(buffer, pc);
}

i860_cpu_device::i860_cpu_device() {
}

void i860_cpu_device::run_cycle(int nHostCycles) {
    UINT32 savepc = m_pc;
    m_pc_updated = 0;
    m_pending_trap = 0;
    
    savepc = m_pc;
    
    if(nd_process_interrupts(nHostCycles))
        i860_gen_interrupt();
    else
        i860_clr_interrupt();

    decode_exec (ifetch (m_pc), 1);
    
    m_exiting_ifetch = 0;
    m_exiting_readmem = 0;
    
    if (m_pending_trap)
    {
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
        debugger("SINGLESTEP");
}

void i860_cpu_device::init() {
    device_start();
    device_reset();
}

void i860_cpu_device::device_start()
{
	reset_i860();
	i860_set_pin(DEC_PIN_BUS_HOLD, 0);
	i860_set_pin(DEC_PIN_RESET, 0);
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
        fprintf(stderr, "NeXTdimension i860 emulator is only supported for little-endian hosts. Error %d\n", err);
        fflush(stderr);
        exit(err);
    }
    
	state( I860_PC,      "PC",      &m_pc).formatstr("%08X");
	state( I860_FIR,     "FIR",     &m_cregs[CR_FIR]).formatstr("%08X");
	state( I860_PSR,     "PSR",     &m_cregs[CR_PSR]).formatstr("%08X");
	state( I860_DIRBASE, "DIRBASE", &m_cregs[CR_DIRBASE]).formatstr("%08X");
	state( I860_DB,      "DB",      &m_cregs[CR_DB]).formatstr("%08X");
	state( I860_FSR,     "FSR",     &m_cregs[CR_FSR]).formatstr("%08X");
	state( I860_EPSR,    "EPSR",    &m_cregs[CR_EPSR]).formatstr("%08X");
	state( I860_R0,      "R0",      &m_iregs[0]).formatstr("%08X");
	state( I860_R1,      "R1",      &m_iregs[1]).formatstr("%08X");
	state( I860_R2,      "R2",      &m_iregs[2]).formatstr("%08X");
	state( I860_R3,      "R3",      &m_iregs[3]).formatstr("%08X");
	state( I860_R4,      "R4",      &m_iregs[4]).formatstr("%08X");
	state( I860_R5,      "R5",      &m_iregs[5]).formatstr("%08X");
	state( I860_R6,      "R6",      &m_iregs[6]).formatstr("%08X");
	state( I860_R7,      "R7",      &m_iregs[7]).formatstr("%08X");
	state( I860_R8,      "R8",      &m_iregs[8]).formatstr("%08X");
	state( I860_R9,      "R9",      &m_iregs[9]).formatstr("%08X");
	state( I860_R10,     "R10",     &m_iregs[10]).formatstr("%08X");
	state( I860_R11,     "R11",     &m_iregs[11]).formatstr("%08X");
	state( I860_R12,     "R12",     &m_iregs[12]).formatstr("%08X");
	state( I860_R13,     "R13",     &m_iregs[13]).formatstr("%08X");
	state( I860_R14,     "R14",     &m_iregs[14]).formatstr("%08X");
	state( I860_R15,     "R15",     &m_iregs[15]).formatstr("%08X");
	state( I860_R16,     "R16",     &m_iregs[16]).formatstr("%08X");
	state( I860_R17,     "R17",     &m_iregs[17]).formatstr("%08X");
	state( I860_R18,     "R18",     &m_iregs[18]).formatstr("%08X");
	state( I860_R19,     "R19",     &m_iregs[19]).formatstr("%08X");
	state( I860_R20,     "R20",     &m_iregs[20]).formatstr("%08X");
	state( I860_R21,     "R21",     &m_iregs[21]).formatstr("%08X");
	state( I860_R22,     "R22",     &m_iregs[22]).formatstr("%08X");
	state( I860_R23,     "R23",     &m_iregs[23]).formatstr("%08X");
	state( I860_R24,     "R24",     &m_iregs[24]).formatstr("%08X");
	state( I860_R25,     "R25",     &m_iregs[25]).formatstr("%08X");
	state( I860_R26,     "R26",     &m_iregs[26]).formatstr("%08X");
	state( I860_R27,     "R27",     &m_iregs[27]).formatstr("%08X");
	state( I860_R28,     "R28",     &m_iregs[28]).formatstr("%08X");
	state( I860_R29,     "R29",     &m_iregs[29]).formatstr("%08X");
	state( I860_R30,     "R30",     &m_iregs[30]).formatstr("%08X");
	state( I860_R31,     "R31",     &m_iregs[31]).formatstr("%08X");

	state( I860_F0,  "F0",  (UINT32*)&m_frg[0]).formatstr("%f");
	state( I860_F1,  "F1",  (UINT32*)&m_frg[4]).formatstr("%f");
	state( I860_F2,  "F2",  (UINT32*)&m_frg[8]).formatstr("%f");
	state( I860_F3,  "F3",  (UINT32*)&m_frg[12]).formatstr("%f");
	state( I860_F4,  "F4",  (UINT32*)&m_frg[16]).formatstr("%f");
	state( I860_F5,  "F5",  (UINT32*)&m_frg[20]).formatstr("%f");
	state( I860_F6,  "F6",  (UINT32*)&m_frg[24]).formatstr("%f");
	state( I860_F7,  "F7",  (UINT32*)&m_frg[28]).formatstr("%f");
	state( I860_F8,  "F8",  (UINT32*)&m_frg[32]).formatstr("%f");
	state( I860_F9,  "F9",  (UINT32*)&m_frg[26]).formatstr("%f");
	state( I860_F10, "F10", (UINT32*)&m_frg[40]).formatstr("%f");
	state( I860_F11, "F11", (UINT32*)&m_frg[44]).formatstr("%f");
	state( I860_F12, "F12", (UINT32*)&m_frg[48]).formatstr("%f");
	state( I860_F13, "F13", (UINT32*)&m_frg[52]).formatstr("%f");
	state( I860_F14, "F14", (UINT32*)&m_frg[56]).formatstr("%f");
	state( I860_F15, "F15", (UINT32*)&m_frg[60]).formatstr("%f");
	state( I860_F16, "F16", (UINT32*)&m_frg[64]).formatstr("%f");
	state( I860_F17, "F17", (UINT32*)&m_frg[68]).formatstr("%f");
	state( I860_F18, "F18", (UINT32*)&m_frg[72]).formatstr("%f");
	state( I860_F19, "F19", (UINT32*)&m_frg[76]).formatstr("%f");
	state( I860_F20, "F20", (UINT32*)&m_frg[80]).formatstr("%f");
	state( I860_F21, "F21", (UINT32*)&m_frg[84]).formatstr("%f");
	state( I860_F22, "F22", (UINT32*)&m_frg[88]).formatstr("%f");
	state( I860_F23, "F23", (UINT32*)&m_frg[92]).formatstr("%f");
	state( I860_F24, "F24", (UINT32*)&m_frg[96]).formatstr("%f");
	state( I860_F25, "F25", (UINT32*)&m_frg[100]).formatstr("%f");
	state( I860_F26, "F26", (UINT32*)&m_frg[104]).formatstr("%f");
	state( I860_F27, "F27", (UINT32*)&m_frg[108]).formatstr("%f");
	state( I860_F28, "F28", (UINT32*)&m_frg[112]).formatstr("%f");
	state( I860_F29, "F29", (UINT32*)&m_frg[116]).formatstr("%f");
	state( I860_F30, "F30", (UINT32*)&m_frg[120]).formatstr("%f");
	state( I860_F31, "F31", (UINT32*)&m_frg[124]).formatstr("%f");
}

void i860_cpu_device::device_reset() {
	reset_i860();
}

offs_t i860_cpu_device::disasm(char* buffer, offs_t pc) {
    return pc + i860_disassembler(pc, ifetch_notrap(pc), buffer);
}

void i860_cpu_device::state_delta(char* buffer, const UINT32* oldstate, const UINT32* newstate) {
    for(int i = 0; i < STATE_SZ; i++)
        if(m_regs[i].valid() && oldstate[i] != newstate[i]) {
            buffer += strlen(buffer);
            sprintf(buffer, "%s=%08X ", m_regs[i].get_name(), m_regs[i].get());
            if(i == I860_PSR) {
                buffer += strlen(buffer);
            sprintf (buffer, "(CC = %d, LCC = %d, SC = %d, IM = %d, U = %d ",
                     GET_PSR_CC (), GET_PSR_LCC (), GET_PSR_SC (), GET_PSR_IM (), GET_PSR_U ());
            sprintf (buffer, "IT/FT/IAT/DAT/IN = %d/%d/%d/%d/%d) ",
                     GET_PSR_IT (), GET_PSR_FT (), GET_PSR_IAT (), GET_PSR_DAT (), GET_PSR_IN ());
            } else if(i == I860_EPSR) {
                buffer += strlen(buffer);
                sprintf (buffer, "(INT = %d, OF = %d, BE = %d) ", GET_EPSR_INT (), GET_EPSR_OF (), GET_EPSR_BE ());
            } else if(i == I860_DIRBASE) {
                buffer += strlen(buffer);
                sprintf (buffer, "(ATE = %d, CS8 = %d) ", GET_DIRBASE_ATE(), GET_DIRBASE_CS8());
            }
        }
}

/**************************************************************************
 * The actual decode and execute code.
 **************************************************************************/
#include "i860dec.cpp"
