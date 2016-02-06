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

    extern void    nd_sdl_init();
    extern void    nd_sdl_uninit();
    extern void    nd_sdl_destroy();
    extern void    nd_start_interrupts();
    extern void    nd_vbl_handler();
    extern void    nd_video_vbl_handler();

#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
