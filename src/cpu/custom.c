/*
* UAE - The Un*x Amiga Emulator
*
* Custom chip emulation
*
* Copyright 1995-2002 Bernd Schmidt
* Copyright 1995 Alessandro Bissacco
* Copyright 2000-2010 Toni Wilen
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "compat.h"
#include "hatari-glue.h"
#include "options_cpu.h"
#include "custom.h"
#include "newcpu.h"
#include "main.h"
#include "cpummu.h"
#include "cpu_prefetch.h"
#include "m68000.h"
#include "debugui.h"
#include "debugcpu.h"

#define WRITE_LOG_BUF_SIZE 4096

extern struct regstruct mmu_backup_regs;
static uae_u32 mmu_callback, mmu_regs;
static uae_u32 mmu_fault_bank_addr, mmu_fault_addr;
static int mmu_fault_size, mmu_fault_rw;
static struct regstruct mmur;
int qpcdivisor = 0;

unsigned long int event_cycles, nextevent, is_lastline, currcycle;
uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode);
void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v);
void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v);
uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode);

typedef struct _LARGE_INTEGER
{
     union
     {
          struct
          {
               unsigned long LowPart;
               long HighPart;
          };
          int64_t QuadPart;
     };
} LARGE_INTEGER, *PLARGE_INTEGER;

void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v)
{
	if (mode < 0)
		put_long (addr, v);
	else if (mode > 0)
		put_word (addr, v);
	else if (mode == 0)
		put_byte (addr, v);

	regs.ce020memcycles -= CYCLE_UNIT;
}

uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode)
{
	uae_u32 v = 0;

	if (mode < 0)
		v = get_long (addr);
	else if (mode > 0)
		v = get_word (addr);
	else if (mode == 0)
		v = get_byte (addr);

	regs.ce020memcycles -= CYCLE_UNIT;
	return v;
}

uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode)
{
	uae_u32 v = 0;

	if (mode < 0)
		v = get_long (addr);
	else if (mode > 0)
		v = get_word (addr);
	else if (mode == 0)
		v = get_byte (addr);

	return v;
}

void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v)
{
	if (mode < 0)
		put_long (addr, v);
	else if (mode > 0)
		put_word (addr, v);
	else if (mode == 0)
		put_byte (addr, v);
}

/* Code taken from main.cpp */
void fixup_cpu (struct uae_prefs *p)
{
	if (p->cpu_frequency == 1000000)
		p->cpu_frequency = 0;
	switch (p->cpu_model)
	{
	case 68000:
		p->address_space_24 = 1;
		if (p->cpu_compatible || p->cpu_cycle_exact)
			p->fpu_model = 0;
		break;
	case 68010:
		p->address_space_24 = 1;
		if (p->cpu_compatible || p->cpu_cycle_exact)
			p->fpu_model = 0;
		break;
	case 68020:
		break;
	case 68030:
		p->address_space_24 = 0;
		break;
	case 68040:
		p->address_space_24 = 0;
		if (p->fpu_model)
			p->fpu_model = 68040;
		break;
	case 68060:
		p->address_space_24 = 0;
		if (p->fpu_model)
			p->fpu_model = 68060;
		break;
	}
	if ((p->cpu_model != 68040) && (p->cpu_model != 68030))
		p->mmu_model = 0;
}

/* Code taken from main.cpp*/
void uae_reset (int hardreset)
{
	currprefs.quitstatefile[0] = changed_prefs.quitstatefile[0] = 0;

	if (quit_program == 0) {
		quit_program = -2;
		if (hardreset)
			quit_program = -3;
	}

}

/* Code taken from debug.cpp*/
void mmu_do_hit (void)
{
	int i;
	uaecptr p;
	uae_u32 pc;

	mmu_triggered = 0;
	pc = m68k_getpc ();
	p = mmu_regs + 18 * 4;
	put_long (p, pc);
	regs = mmu_backup_regs;
	regs.intmask = 7;
	regs.t0 = regs.t1 = 0;
	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
		if (currprefs.cpu_model >= 68020)
			m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
		else
			m68k_areg (regs, 7) = regs.isp;
		regs.s = 1;
	}
	MakeSR ();
	m68k_setpc (mmu_callback);
	fill_prefetch_slow ();

	if (currprefs.cpu_model > 68000) {
		for (i = 0 ; i < 9; i++) {
			m68k_areg (regs, 7) -= 4;
			put_long (m68k_areg (regs, 7), 0);
		}
		m68k_areg (regs, 7) -= 4;
		put_long (m68k_areg (regs, 7), mmu_fault_addr);
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB1S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB2S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB3S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7),
			(mmu_fault_rw ? 0 : 0x100) | (mmu_fault_size << 5)); /* SSW */
		m68k_areg (regs, 7) -= 4;
		put_long (m68k_areg (regs, 7), mmu_fault_bank_addr);
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0x7002);
	}
	m68k_areg (regs, 7) -= 4;
	put_long (m68k_areg (regs, 7), get_long (p - 4));
	m68k_areg (regs, 7) -= 2;
	put_word (m68k_areg (regs, 7), mmur.sr);
#ifdef JIT
	set_special(SPCFLAG_END_COMPILE);
#endif
}