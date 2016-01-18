#include "main.h"
#include "nd_sdl.h"
#include "configuration.h"
#include "dimension.h"

/* Because of SDL time (in)accuracy, timing is very approximative */
const int DISPLAY_VBL_MS = 1000 / 68; // main display at 68Hz, actually this is 71.42 Hz because (int)1000/(int)68Hz=14ms
const int VIDEO_VBL_MS   = 1000 / 60; // NTSC display at 60Hz, actually this is 62.5 Hz because (int)1000/(int)60Hz=16ms
const int BLANK_MS       = 10;        // Give some blank time for both

extern Uint32      nd_display_vbl(Uint32 interval, void* param);
extern Uint32      nd_video_vbl(Uint32 interval, void* param);
extern SDL_Window* sdlWindow;

static SDL_TimerID displayVBL = NULL;
static SDL_TimerID videoVBL   = NULL;

SDL_Texture*        ndTexture  = NULL;
SDL_Renderer*       ndRenderer = NULL;
static SDL_Window*  ndWindow   = NULL;

static Uint32 nd_window_vbl(Uint32 interval, void* param) {
    interval = nd_display_vbl(interval, param);
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL && interval == BLANK_MS) {
        SDL_UpdateTexture(ndTexture, NULL, (void*)&ND_vram[16], 1152*4);
        SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
        SDL_RenderPresent(ndRenderer);
    }
    return interval;
}

void nd_sdl_init() {
    SDL_Rect r; r.x = 0; r.y = 0; r.w = 1120; r.h = 832;
    
    if(!(ndWindow)) {
        int x, y, w, h;
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        SDL_GetWindowSize(sdlWindow, &w, &h);
        ndWindow   = SDL_CreateWindow("NeXT Dimension",(x-w)+1, y, r.w, r.h, SDL_WINDOW_HIDDEN);
        ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED);
        if (!ndWindow || !ndRenderer) {
            fprintf(stderr,"[ND] Failed to create window or renderer!\n");
            exit(-1);
        }
        SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, r.w, r.h);
        } else {
            ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, r.w, r.h);
        }
    }
    
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL)
        SDL_ShowWindow(ndWindow);
    else
        SDL_HideWindow(ndWindow);
    
    if(displayVBL) SDL_RemoveTimer(displayVBL);
    // main display at 68Hz
    displayVBL = SDL_AddTimer(DISPLAY_VBL_MS,  nd_window_vbl, NULL);
    
    if(videoVBL) SDL_RemoveTimer(videoVBL);
    // NTSC video at 60Hz
    videoVBL   = SDL_AddTimer(VIDEO_VBL_MS, nd_video_vbl, NULL);
}

void nd_sdl_uninit() {
    if(displayVBL) {
        SDL_RemoveTimer(displayVBL);
        displayVBL = NULL;
    }
    if(videoVBL) {
        SDL_RemoveTimer(videoVBL);
        videoVBL = NULL;
    }
}

void nd_sdl_destroy() {
    nd_sdl_uninit();
    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);
}

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