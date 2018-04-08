/*
  Hatari - m68000.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

/* 2007/11/10	[NP]	Add pairing for lsr / dbcc (and all variants	*/
/*			working on register, not on memory).		*/
/* 2008/01/07	[NP]	Use PairingArray to store all valid pairing	*/
/*			combinations (in m68000.c)			*/
/* 2010/04/05	[NP]	Rework the pairing code to take BusCyclePenalty	*/
/*			into account when using d8(an,ix).		*/
/* 2010/05/07	[NP]	Add BusCyclePenalty to LastInstrCycles to detect*/
/*			a possible pairing between add.l (a5,d1.w),d0	*/
/*			and move.b 7(a5,d1.w),d5.			*/


#ifndef HATARI_M68000_H
#define HATARI_M68000_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
#include "sysdeps.h"
#include "memory.h"
#include "newcpu.h"     /* for regs */
#include "cycInt.h"
#include "log.h"
#include "host.h"
#include "configuration.h"

/* 68000 Register defines */
enum {
  REG_D0,    /* D0.. */
  REG_D1,
  REG_D2,
  REG_D3,
  REG_D4,
  REG_D5,
  REG_D6,
  REG_D7,    /* ..D7 */
  REG_A0,    /* A0.. */
  REG_A1,
  REG_A2,
  REG_A3,
  REG_A4,
  REG_A5,
  REG_A6,
  REG_A7,    /* ..A7 (also SP) */
};

/* 68000 Condition code's */
#define SR_AUX              0x0010
#define SR_NEG              0x0008
#define SR_ZERO             0x0004
#define SR_OVERFLOW         0x0002
#define SR_CARRY            0x0001

#define SR_CLEAR_AUX        0xffef
#define SR_CLEAR_NEG        0xfff7
#define SR_CLEAR_ZERO       0xfffb
#define SR_CLEAR_OVERFLOW   0xfffd
#define SR_CLEAR_CARRY      0xfffe

#define SR_CCODE_MASK       (SR_AUX|SR_NEG|SR_ZERO|SR_OVERFLOW|SR_CARRY)
#define SR_MASK             0xFFE0

#define SR_TRACEMODE        0x8000
#define SR_SUPERMODE        0x2000
#define SR_IPL              0x0700

#define SR_CLEAR_IPL        0xf8ff
#define SR_CLEAR_TRACEMODE  0x7fff
#define SR_CLEAR_SUPERMODE  0xdfff

/* Exception vectors */
#define  EXCEPTION_BUSERROR   0x00000008
#define  EXCEPTION_ADDRERROR  0x0000000c
#define  EXCEPTION_ILLEGALINS 0x00000010
#define  EXCEPTION_DIVZERO    0x00000014
#define  EXCEPTION_CHK        0x00000018
#define  EXCEPTION_TRAPV      0x0000001c
#define  EXCEPTION_TRACE      0x00000024
#define  EXCEPTION_LINE_A     0x00000028
#define  EXCEPTION_LINE_F     0x0000002c
#define  EXCEPTION_TRAP0      0x00000080
#define  EXCEPTION_TRAP1      0x00000084
#define  EXCEPTION_TRAP2      0x00000088
#define  EXCEPTION_TRAP13     0x000000B4
#define  EXCEPTION_TRAP14     0x000000B8


/* Size of 68000 instructions */
#define MAX_68000_INSTRUCTION_SIZE  10  /* Longest 68000 instruction is 10 bytes(6+4) */
#define MIN_68000_INSTRUCTION_SIZE  2   /* Smallest 68000 instruction is 2 bytes(ie NOP) */

/* Illegal Opcode used to help emulation. eg. free entries are 8 to 15 inc' */
#define  GEMDOS_OPCODE        8  /* Free op-code to intercept GemDOS trap */
#define  SYSINIT_OPCODE      10  /* Free op-code to initialize system (connected drives etc.) */
#define  VDI_OPCODE          12  /* Free op-code to call VDI handlers AFTER Trap#2 */



/* Ugly hacks to adapt the main code to the different CPU cores: */

#define Regs regs.regs

# define M68000_GetPC()     m68k_getpc()
# define M68000_SetPC(val)  m68k_setpc(val)

static inline Uint16 M68000_GetSR(void)
{
	MakeSR();
	return regs.sr;
}
static inline void M68000_SetSR(Uint16 v)
{
	regs.sr = v;
	MakeFromSR();
}

# define M68000_SetSpecial(flags)   set_special(flags)
# define M68000_UnsetSpecial(flags) unset_special(flags)


/* bus error mode */
#define BUS_ERROR_WRITE 0
#define BUS_ERROR_READ 1


/* bus access mode */
#define	BUS_MODE_CPU		0			/* bus is owned by the cpu */
#define	BUS_MODE_BLITTER	1			/* bus is owned by the blitter */


extern Uint32 BusErrorAddress;
extern Uint32 BusErrorPC;
extern bool bBusErrorReadWrite;
extern int BusMode;

extern int	LastOpcodeFamily;
extern int	LastInstrCycles;
extern int	Pairing;
extern char	PairingArray[ MAX_OPCODE_FAMILY ][ MAX_OPCODE_FAMILY ];
extern const char *OpcodeName[];


/*-----------------------------------------------------------------------*/
/**
 * Add CPU cycles.
 */
static inline void M68000_AddCycles(int cycles) {
    nCyclesOver += cycles;
    
    if(PendingInterrupt.type == CYC_INT_CPU)
        PendingInterrupt.time -= cycles;

    if(usCheckCycles < 0) {
        if(!(CycInt_SetNewInterruptUs())) {
            usCheckCycles = 100 * ConfigureParams.System.nCpuFreq;
        }
    } else
        usCheckCycles -= cycles;
    nCyclesMainCounter += cycles;
}

void M68000_Init(void);
void M68000_Reset(bool bCold);
void M68000_Stop(void);
void M68000_Start(void);
void M68000_CheckCpuSettings(void);
void M68000_BusError(Uint32 addr, bool bReadWrite);
void M68000_Exception(Uint32 ExceptionVector , int ExceptionSource);

#ifdef __cplusplus
}
#endif /* __cplusplus */
        
#endif
