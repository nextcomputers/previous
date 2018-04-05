 /*
  * UAE - The Un*x Amiga Emulator - CPU core
  *
  * Big endian memory access functions.
  *
  * Copyright 1996 Bernd Schmidt
  *
  * Adaptation to Hatari by Thomas Huth, Eero Tamminen
  *
  * This file is distributed under the GNU Public License, version 2 or at
  * your option any later version. Read the file gpl.txt for details.
  */

#ifndef UAE_MACCESS_H
#define UAE_MACCESS_H


/* Can the actual CPU access unaligned memory? */
#ifndef CPU_CAN_ACCESS_UNALIGNED
# if defined(__i386__) || defined(powerpc) || defined(__mc68020__)
#  define CPU_CAN_ACCESS_UNALIGNED 1
# else
#  define CPU_CAN_ACCESS_UNALIGNED 0
# endif
#endif


/* If the CPU can access unaligned memory, use these accelerated functions: */
#if CPU_CAN_ACCESS_UNALIGNED

#include <SDL_endian.h>


static inline uae_u32 do_get_mem_long(void *a)
{
	return SDL_SwapBE32(*(uae_u32 *)a);
}

static inline uae_u16 do_get_mem_word(void *a)
{
	return SDL_SwapBE16(*(uae_u16 *)a);
}


static inline void do_put_mem_long(void *a, uae_u32 v)
{
	*(uae_u32 *)a = SDL_SwapBE32(v);
}

static inline void do_put_mem_word(void *a, uae_u16 v)
{
	*(uae_u16 *)a = SDL_SwapBE16(v);
}


#else  /* Cpu can not access unaligned memory: */

#define do_get_mem_long(a) __builtin_bswap32(*((uae_u32 *)(a)))
#define do_get_mem_word(a) __builtin_bswap16(*((uae_u16 *)(a)))
#define do_put_mem_long(a, v) *((uae_u32 *)(a)) = __builtin_bswap32(v)
#define do_put_mem_word(a, v) *((uae_u16 *)(a)) = __builtin_bswap16(v)

#endif  /* CPU_CAN_ACCESS_UNALIGNED */


/* These are same for all architectures: */

static inline uae_u8 do_get_mem_byte(uae_u8 *a)
{
	return *a;
}

static inline void do_put_mem_byte(uae_u8 *a, uae_u8 v)
{
	*a = v;
}


#endif /* UAE_MACCESS_H */
