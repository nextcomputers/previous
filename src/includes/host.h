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
    
    typedef SDL_SpinLock       lock_t;
    typedef SDL_Thread         thread_t;
    typedef SDL_ThreadFunction thread_func_t;

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
    
    void        host_lock(lock_t* lock);
    void        host_unlock(lock_t* lock);
    void        host_checklock(lock_t* lock);
    int         host_trylock(lock_t* lock);
    thread_t*   host_thread_create(thread_func_t, void* data);
    int         host_thread_wait(thread_t* thread);

#ifdef __cplusplus
}
#endif

#endif /* __HOST_H__ */
