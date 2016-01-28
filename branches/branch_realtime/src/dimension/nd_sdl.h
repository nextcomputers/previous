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

    void      nd_sdl_init();
    void      nd_sdl_uninit();
    void      nd_sdl_destroy();
    
#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
