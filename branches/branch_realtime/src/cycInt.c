/*
  Hatari - cycInt.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This code handles our table with callbacks for cycle accurate program
  interruption. We add any pending callback handler into a table so that we do
  not need to test for every possible interrupt event. We then scan
  the list if used entries in the table and copy the one with the least cycle
  count into the global 'PendingInterrupt' variable. This is then
  decremented by the execution loop - rather than decrement each and every
  entry (as the others cannot occur before this one).
  We have two methods of adding interrupts; Absolute and Relative.
  Absolute will set values from the time of the previous interrupt (e.g., add
  HBL every 512 cycles), and Relative will add from the current cycle time.
  Note that interrupt may occur 'late'. I.e., if an interrupt is due in 4
  cycles time but the current instruction takes 20 cycles we will be 16 cycles
  late - this is handled in the adjust functions.
*/

const char CycInt_fileid[] = "Previous cycInt.c : " __DATE__ " " __TIME__;

#include <stdint.h>
#include <assert.h>
#include "main.h"
#include "cycInt.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "screen.h"
#include "video.h"
#include "sysReg.h"
#include "esp.h"
#include "mo.h"
#include "ethernet.h"
#include "dma.h"
#include "floppy.h"
#include "snd.h"
#include "printer.h"
#include "kms.h"
#include "configuration.h"


void (*PendingInterruptFunction)(void);
Sint64 PendingInterruptCounter;
int    timeCheckCycles = 0;

static Sint64 nCyclesOver;
Sint64 nCyclesMainCounter;				/* Main cycles counter */

/* List of possible interrupt handlers to be store in 'PendingInterruptTable',
 * used for 'MemorySnapShot' */
static void (* const pIntHandlerFunctions[MAX_INTERRUPTS])(void) =
{
	NULL,
	Video_InterruptHandler_VBL,
	Hardclock_InterruptHandler,
    Mouse_Handler,
    ESP_InterruptHandler,
    ESP_IO_Handler,
    M2RDMA_InterruptHandler,
    R2MDMA_InterruptHandler,
    MO_InterruptHandler,
    MO_IO_Handler,
    ECC_IO_Handler,
    ENET_IO_Handler,
    FLP_IO_Handler,
    SND_IO_Handler,
    Printer_IO_Handler
};

static INTERRUPTHANDLER InterruptHandlers[MAX_INTERRUPTS];
INTERRUPTHANDLER        PendingInterrupt;
static int ActiveInterrupt=0;

static void CycInt_SetNewInterrupt(void);

/*-----------------------------------------------------------------------*/
/**
 * Reset interrupts, handlers
 */
void CycInt_Reset(void) {
	int i;

	/* Reset counts */
    PendingInterrupt.time = 0;
	ActiveInterrupt       = 0;
	nCyclesOver           = 0;

	/* Reset interrupt table */
	for (i=0; i<MAX_INTERRUPTS; i++) {
		InterruptHandlers[i].type      = CYC_INT_NONE;
		InterruptHandlers[i].time      = INT64_MAX;
		InterruptHandlers[i].pFunction = pIntHandlerFunctions[i];
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Convert interrupt handler function pointer to ID, used for saving
 */
static int CycInt_HandlerFunctionToID(void (*pHandlerFunction)(void))
{
	int i;

	/* Scan for function match */
	for (i=0; i<MAX_INTERRUPTS; i++)
	{
		if (pIntHandlerFunctions[i]==pHandlerFunction)
			return i;
	}

	/* Didn't find one! Oops */
	fprintf(stderr, "\nError: didn't find interrupt function matching 0x%p\n",
	        pHandlerFunction);
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Convert ID back into interrupt handler function, used for restoring
 */
static void *CycInt_IDToHandlerFunction(int ID)
{
	/* Get function pointer */
	return pIntHandlerFunctions[ID];
}


/*-----------------------------------------------------------------------*/
/**
 * Save/Restore snapshot of local variables('MemorySnapShot_Store' handles type)
 */
void CycInt_MemorySnapShot_Capture(bool bSave)
{
	int i,ID;

	/* Save/Restore details */
	for (i=0; i<MAX_INTERRUPTS; i++)
	{
		MemorySnapShot_Store(&InterruptHandlers[i].type, sizeof(InterruptHandlers[i].type));
		MemorySnapShot_Store(&InterruptHandlers[i].time, sizeof(InterruptHandlers[i].time));
		if (bSave)
		{
			/* Convert function to ID */
			ID = CycInt_HandlerFunctionToID(InterruptHandlers[i].pFunction);
			MemorySnapShot_Store(&ID, sizeof(int));
		}
		else
		{
			/* Convert ID to function */
			MemorySnapShot_Store(&ID, sizeof(int));
			InterruptHandlers[i].pFunction = CycInt_IDToHandlerFunction(ID);
		}
	}
	MemorySnapShot_Store(&nCyclesOver, sizeof(nCyclesOver));
    MemorySnapShot_Store(&PendingInterrupt.type, sizeof(PendingInterrupt.type));
	MemorySnapShot_Store(&PendingInterrupt.time, sizeof(PendingInterrupt.time));
	if (bSave)
	{
		/* Convert function to ID */
		ID = CycInt_HandlerFunctionToID(PendingInterruptFunction);
		MemorySnapShot_Store(&ID, sizeof(int));
	}
	else
	{
		/* Convert ID to function */
		MemorySnapShot_Store(&ID, sizeof(int));
		PendingInterruptFunction = CycInt_IDToHandlerFunction(ID);
	}


	if (!bSave)
		CycInt_SetNewInterrupt();	/* when restoring snapshot, compute current state after */
}


/*-----------------------------------------------------------------------*/
/**
 * Find next interrupt to occur, and store to global variables for decrement
 * in instruction decode loop.
 * (SC) We assume here that the emulated CPU runs at least as fast as the 
 * real CPU. Thus we simply convert microseconds to clock cycles for
 * finding the next interrupt. We deal with the possible difference later
 * when we check for pending interrupts in the decode loop.
 */
static void CycInt_SetNewInterrupt(void) {
	Sint64       LowestCycleCount = INT64_MAX;
	interrupt_id LowestInterrupt  = INTERRUPT_NULL, i;
    Sint64       CPUFreq          = ConfigureParams.System.nCpuFreq;
    Sint64       now              = 0;
    
	/* Find next interrupt to go off */
	for (i = INTERRUPT_NULL+1; i < MAX_INTERRUPTS; i++) {
        /* Is interrupt pending? */
        switch (InterruptHandlers[i].type) {
            case CYC_INT_CPU:
                if(InterruptHandlers[i].time < LowestCycleCount) {
                    LowestCycleCount = InterruptHandlers[i].time;
                    LowestInterrupt  = i;
                }
                break;
            case CYC_INT_US:
                if(!(now)) now = host_time_us();
                Sint64 dt = (InterruptHandlers[i].time - now) * CPUFreq;
                if(dt < LowestCycleCount) {
                    LowestCycleCount = dt * CPUFreq;
                    LowestInterrupt  = i;
                }
                break;
        }
	}

	/* Set new counts, active interrupt */
    PendingInterrupt = InterruptHandlers[LowestInterrupt];
	ActiveInterrupt  = LowestInterrupt;
}


/*-----------------------------------------------------------------------*/
/**
 * Adjust all interrupt timings, MUST call CycInt_SetNewInterrupt after this.
 */
static void CycInt_UpdateInterrupt(void)
{
	Sint64 CycleSubtract;
	int i;

	/* Find out how many cycles we went over (<=0) */
	nCyclesOver = PendingInterrupt.time;
	/* Calculate how many cycles have passed, included time we went over */
	CycleSubtract = InterruptHandlers[ActiveInterrupt].time - nCyclesOver;

	/* Adjust table */
	for (i = 0; i < MAX_INTERRUPTS; i++) {
		if (InterruptHandlers[i].type == CYC_INT_CPU)
			InterruptHandlers[i].time -= CycleSubtract;
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Adjust all interrupt timings as 'ActiveInterrupt' has occured, and
 * remove from active list.
 */
void CycInt_AcknowledgeInterrupt(void)
{
	/* Update list cycle counts */
	CycInt_UpdateInterrupt();

	/* Disable interrupt entry which has just occured */
	InterruptHandlers[ActiveInterrupt].type = CYC_INT_NONE;

	/* Set new */
	CycInt_SetNewInterrupt();
}

/*-----------------------------------------------------------------------*/
/**
 * Add interrupt to occur from now.
 */
void CycInt_AddRelativeInterrupt(Sint64 CycleTime, interrupt_id Handler)
{
	assert(CycleTime >= 0);

	/* Update list cycle counts with current PendingInterruptCount before adding a new int, */
	/* because CycInt_SetNewInterrupt can change the active int / PendingInterruptCount */
	if ( ActiveInterrupt > 0 )
		CycInt_UpdateInterrupt();

	InterruptHandlers[Handler].type = CYC_INT_CPU;
	InterruptHandlers[Handler].time = CycleTime;

	/* Set new active int and compute a new value for PendingInterruptCount*/
	CycInt_SetNewInterrupt();
}


/*-----------------------------------------------------------------------*/
/**
 * Add interrupt to occur us microsencods from now or if repeat=true relative to
 * time if time is no more than 1 ms in the past.
 */
void CycInt_AddRelativeInterruptUs(Sint64 us, bool repeat, interrupt_id Handler) {
    assert(us >= 0);
    
    /* Update list cycle counts with current PendingInterruptCount before adding a new int, */
    /* because CycInt_SetNewInterrupt can change the active int / PendingInterruptCount */
    if ( ActiveInterrupt > 0 )
        CycInt_UpdateInterrupt();
    
    Uint64 now = host_time_us();
    InterruptHandlers[Handler].type = CYC_INT_US;
    if(repeat && (now - InterruptHandlers[Handler].time) < 1000)
        InterruptHandlers[Handler].time += us;
    else
        InterruptHandlers[Handler].time = now + us;
    
    /* Set new active int and compute a new value for PendingInterruptCount*/
    CycInt_SetNewInterrupt();
}

/*-----------------------------------------------------------------------*/
/**
 * Remove a pending interrupt from our table
 */
void CycInt_RemovePendingInterrupt(interrupt_id Handler)
{
	/* Update list cycle counts, including the handler we want to remove */
	/* to be able to resume it later (for MFP timers) */
	CycInt_UpdateInterrupt();

	/* Stop interrupt after CycInt_UpdateInterrupt */
	InterruptHandlers[Handler].type = CYC_INT_NONE;

	/* Set new */
	CycInt_SetNewInterrupt();
}


/*-----------------------------------------------------------------------*/
/**
 * Return true if interrupt is active in list
 */
bool CycInt_InterruptActive(interrupt_id Handler)
{
    return InterruptHandlers[Handler].type != CYC_INT_NONE;
}
