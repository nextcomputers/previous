#include <SDL.h>
#include <SDL_thread.h>

#pragma once

#ifndef __ND_SDL_H__
#define __ND_SDL_H__

#ifdef __cplusplus
extern "C" {
#endif

    extern const int DISPLAY_VBL_MS;
    extern const int VIDEO_VBL_MS;
    extern const int BLANK_MS;

    typedef SDL_SpinLock       lock_t;
    typedef SDL_Thread         thread_t;
    typedef SDL_ThreadFunction thread_func_t;

    void      nd_sdl_init();
    void      nd_sdl_uninit();
    void      nd_sdl_destroy();

    void      lock(lock_t* lock);
    void      unlock(lock_t* lock);
    void      checklock(lock_t* lock);
    int       trylock(lock_t* lock);
    Uint32    time_ms();
    void      sleep_ms(Uint32 ms);
    thread_t* thread_create(thread_func_t, void* data);
    int       thread_wait(thread_t* thread);
    
#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
