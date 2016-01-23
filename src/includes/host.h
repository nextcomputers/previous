/*
 Host system dependencies such as (real)time synchronizaton and events that
 occur on host threads (e.g. VBL & timers) which need to be synchronized
 to Previous CPU threads.
 */

#pragma once

#ifndef __HOST_H__
#define __HOST_H__

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
    enum {
        ND_DISPLAY,
        ND_VIDEO,
    };
    
    void   host_nd_blank(int slot, int src, bool state);
    bool   host_nd_blank_state(int slot, int src);
    Uint64 host_time_us();
    Uint32 host_time_ms();
    double host_time_sec();
    void   host_sleep_ms(Uint32 ms);
    void   host_sleep_us(Uint64 us);
    void   host_print_stat();
     
#ifdef __cplusplus
}
#endif

#endif /* __HOST_H__ */
