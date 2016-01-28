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

    typedef struct {
        const char* label;
        Uint64      total_count;
        double      time_mark;
        double      start_time;
        int         limit;
        int         count;
        int         frequency;
    } throttle_t;
    
    void        host_reset();
    void        host_realtime(bool state);
    bool        host_is_realtime();
    void        host_nd_blank(int slot, int src, bool state);
    bool        host_nd_blank_state(int slot, int src);
    Uint64      host_time_us();
    Uint32      host_time_ms();
    double      host_time_sec();
    void        host_sleep_sec(double sec);
    void        host_sleep_ms(Uint32 ms);
    void        host_sleep_us(Uint64 us);
    void        host_print_stat();
    throttle_t* host_create_throttle(const char* label, int frequencyHz, int eventLimit);
    void        host_throttle_add(throttle_t* throttle, int numEvents);
    void        host_destroy_throttle(throttle_t* throttle);
    
#ifdef __cplusplus
}
#endif

#endif /* __HOST_H__ */
