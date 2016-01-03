/***************************************************************************

    i860.c

    Interface file for the Intel i860 emulator.

    Copyright (C) 1995-present Jason Eckhardt (jle@rice.edu)
    Released for general non-commercial use under the MAME license
    with the additional requirement that you are free to use and
    redistribute this code in modified or unmodified form, provided
    you list me in the credits.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include "i860.hpp"
#include <stdio.h>
#include <string.h>

static i860_cpu_device nd_i860;

extern "C" void nd_i860_init() {
    nd_i860.i860_set_pin(DEC_PIN_BUS_HOLD, 1);
    nd_i860.i860_set_pin(DEC_PIN_RESET, 1);
    nd_i860.init();
}

extern "C" void i860_Run(int nHostCycles) {
    if(nd_i860.m_halt) return;
    
    if(nd_process_interrupts(nHostCycles))
        nd_i860.i860_gen_interrupt();
    else
        nd_i860.i860_clr_interrupt();
    nd_i860.run_cycle();
}

extern "C" offs_t i860_Disasm(char* buffer, offs_t pc) {
    return nd_i860.disasm(buffer, pc);
}

i860_cpu_device::i860_cpu_device() {
}

void i860_cpu_device::run_cycle() {
    UINT32 savepc = m_pc;
    m_pc_updated = 0;
    m_pending_trap = 0;
    
    savepc = m_pc;
    //debugger_instruction_hook(this, m_pc);
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
    
    /*if (m_single_stepping)
     debugger (cpustate); */
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

    /*
	save_item(NAME(m_iregs));
	save_item(NAME(m_cregs));
	save_item(NAME(m_frg));
	save_item(NAME(m_pc));
*/
    
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

	state( I860_F0,  "F0",  &m_freg[0]).formatstr("%08X");
	state( I860_F1,  "F1",  &m_freg[1]).formatstr("%08X");
	state( I860_F2,  "F2",  &m_freg[2]).formatstr("%08X");
	state( I860_F3,  "F3",  &m_freg[3]).formatstr("%08X");
	state( I860_F4,  "F4",  &m_freg[4]).formatstr("%08X");
	state( I860_F5,  "F5",  &m_freg[5]).formatstr("%08X");
	state( I860_F6,  "F6",  &m_freg[6]).formatstr("%08X");
	state( I860_F7,  "F7",  &m_freg[7]).formatstr("%08X");
	state( I860_F8,  "F8",  &m_freg[8]).formatstr("%08X");
	state( I860_F9,  "F9",  &m_freg[9]).formatstr("%08X");
	state( I860_F10, "F10", &m_freg[10]).formatstr("%08X");
	state( I860_F11, "F11", &m_freg[11]).formatstr("%08X");
	state( I860_F12, "F12", &m_freg[12]).formatstr("%08X");
	state( I860_F13, "F13", &m_freg[13]).formatstr("%08X");
	state( I860_F14, "F14", &m_freg[14]).formatstr("%08X");
	state( I860_F15, "F15", &m_freg[15]).formatstr("%08X");
	state( I860_F16, "F16", &m_freg[16]).formatstr("%08X");
	state( I860_F17, "F17", &m_freg[17]).formatstr("%08X");
	state( I860_F18, "F18", &m_freg[18]).formatstr("%08X");
	state( I860_F19, "F19", &m_freg[19]).formatstr("%08X");
	state( I860_F20, "F20", &m_freg[20]).formatstr("%08X");
	state( I860_F21, "F21", &m_freg[21]).formatstr("%08X");
	state( I860_F22, "F22", &m_freg[22]).formatstr("%08X");
	state( I860_F23, "F23", &m_freg[23]).formatstr("%08X");
	state( I860_F24, "F24", &m_freg[24]).formatstr("%08X");
	state( I860_F25, "F25", &m_freg[25]).formatstr("%08X");
	state( I860_F26, "F26", &m_freg[26]).formatstr("%08X");
	state( I860_F27, "F27", &m_freg[27]).formatstr("%08X");
	state( I860_F28, "F28", &m_freg[28]).formatstr("%08X");
	state( I860_F29, "F29", &m_freg[29]).formatstr("%08X");
	state( I860_F30, "F30", &m_freg[30]).formatstr("%08X");
	state( I860_F31, "F31", &m_freg[31]).formatstr("%08X");
    
	m_icountptr = &m_icount;
}

void i860_cpu_device::device_reset() {
	reset_i860();
}

offs_t i860_cpu_device::disasm(char* buffer, offs_t pc) {
    return pc + i860_disassembler(pc, ifetch(pc), buffer);
}

void i860_cpu_device::state_delta(char* buffer, const UINT32* oldstate, const UINT32* newstate) {
    for(int i = 0; i < STATE_SZ; i++)
        if(m_regs[i].valid() && oldstate[i] != newstate[i]) {
            buffer += strlen(buffer);
            sprintf(buffer, "%s=%08X ", m_regs[i].get_name(), m_regs[i].get());
            if(i == I860_PSR) {
                buffer += strlen(buffer);
            sprintf (buffer, "(CC = %d, LCC = %d, SC = %d, IM = %d, U = %d ",
                     GET_PSR_CC (), GET_PSR_LCC (), GET_PSR_SC (), GET_PSR_IM (),
                     GET_PSR_U ());
            sprintf (buffer, "IT/FT/IAT/DAT/IN = %d/%d/%d/%d/%d) ",
                     GET_PSR_IT (), GET_PSR_FT (), GET_PSR_IAT (),
                     GET_PSR_DAT (), GET_PSR_IN ());
                
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
