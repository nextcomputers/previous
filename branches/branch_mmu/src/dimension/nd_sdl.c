#include "main.h"
#include "nd_sdl.h"
#include "configuration.h"
#include "dimension.h"
#include "screen.h"

/* Because of SDL time (in)accuracy, timing is very approximative */
const int DISPLAY_VBL_MS = 1000 / 68; // main display at 68Hz, actually this is 71.42 Hz because (int)1000/(int)68Hz=14ms
const int VIDEO_VBL_MS   = 1000 / 60; // NTSC display at 60Hz, actually this is 62.5 Hz because (int)1000/(int)60Hz=16ms
const int BLANK_MS       = 10;        // Give some blank time for both

extern Uint32      nd_display_blank_start();
extern Uint32      nd_display_blank_end();
extern Uint32      nd_video_vbl(Uint32 interval, void* param);

static SDL_TimerID   videoVBL      = NULL;
static volatile bool doRepaint     = true;
static SDL_Thread*   repaintThread = NULL;
static SDL_Window*   ndWindow      = NULL;

extern void blitDimension(SDL_Texture* tex);

static int repainter(void* unused) {
    SDL_Texture*  ndTexture  = NULL;
    SDL_Renderer* ndRenderer = NULL;
    
    ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ndWindow || !ndRenderer) {
        fprintf(stderr,"[ND] Failed to create renderer!\n");
        exit(-1);
    }
    SDL_Rect r; r.x = 0; r.y = 0; r.w = 1120; r.h = 832;
    SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
    ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, r.w, r.h);
    
    while(doRepaint) {
        if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
            blitDimension(ndTexture);
            SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
            SDL_RenderPresent(ndRenderer);
        }
        // if this is a cube, then do ND blank emulation. Otherweise just sleep a bit.
        if(ConfigureParams.System.nMachineType == NEXT_CUBE030 || ConfigureParams.System.nMachineType == NEXT_CUBE040) {
            nd_display_blank_start();
            SDL_Delay(BLANK_MS);
            nd_display_blank_end();
        } else {
            SDL_Delay(BLANK_MS);
        }
    }

    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);

    return 0;
}

void nd_sdl_init() {
    if(!(repaintThread)) {
        int x, y, w, h;
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        SDL_GetWindowSize(sdlWindow, &w, &h);
        ndWindow      = SDL_CreateWindow("NeXT Dimension",(x-w)+1, y, 1120, 832, SDL_WINDOW_HIDDEN);
        repaintThread = SDL_CreateThread(repainter, "[ND] repainter", NULL);
    }
    if(videoVBL) SDL_RemoveTimer(videoVBL);
    // NTSC video at 60Hz
    videoVBL   = SDL_AddTimer(VIDEO_VBL_MS, nd_video_vbl, NULL);
    
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        SDL_ShowWindow(ndWindow);
    } else {
        SDL_HideWindow(ndWindow);
    }
}

void nd_sdl_uninit() {
    if(videoVBL) {
        SDL_RemoveTimer(videoVBL);
        videoVBL = NULL;
    }
    SDL_HideWindow(ndWindow);
}

void nd_sdl_destroy() {
    doRepaint = false; // stop repaint thread
    int s;
    SDL_WaitThread(repaintThread, &s);
    nd_sdl_uninit();
}

//----- SDL abstractions for ND implementation

void lock(lock_t* lock) {
    SDL_AtomicLock(lock);
}

int trylock(lock_t* lock) {
    return SDL_AtomicTryLock(lock);
}

void unlock(lock_t* lock) {
    SDL_AtomicUnlock(lock);
}

void checklock(lock_t* lock) {
    SDL_AtomicLock(lock);
    SDL_AtomicUnlock(lock);
}

Uint32 time_ms() {
    return SDL_GetTicks();
}

void sleep_ms(Uint32 ms) {
    SDL_Delay(ms);
}

thread_t* thread_create(thread_func_t func, void* data) {
    return SDL_CreateThread(func, "[ND] Thread", data);
}

int thread_wait(thread_t* thread) {
    int status;
    SDL_WaitThread(thread, &status);
    return status;
}