#include "main.h"
#include "nd_sdl.hpp"
#include "configuration.h"
#include "dimension.hpp"
#include "screen.h"
#include "host.h"
#include "cycInt.h"
#include "NextBus.hpp"

/* Because of SDL time (in)accuracy, timing is very approximative */
const int DISPLAY_VBL_MS = 1000 / 68; // main display at 68Hz, actually this is 71.42 Hz because (int)1000/(int)68Hz=14ms
const int VIDEO_VBL_MS   = 1000 / 60; // NTSC display at 60Hz, actually this is 62.5 Hz because (int)1000/(int)60Hz=16ms
const int BLANK_MS       = 2;         // Give some blank time for both

NDSDL::NDSDL(int slot, Uint32* vram) : slot(slot), doRepaint(true), repaintThread(NULL), ndWindow(NULL), ndRenderer(NULL), vram(vram) {}


int NDSDL::repainter(void *_this) {
    return ((NDSDL*)_this)->repainter();
}

int NDSDL::repainter(void) {
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
    
    SDL_Texture*  ndTexture  = NULL;
    
    SDL_Rect r = {0,0,1120,832};
    
    ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!ndRenderer) {
        fprintf(stderr,"[ND] Slot %i: Failed to create renderer!\n", slot);
        exit(-1);
    }
    
    SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
    ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, r.w, r.h);
    
    SDL_AtomicSet(&blitNDFB, 1);
    
    while(doRepaint) {
        if (SDL_AtomicGet(&blitNDFB)) {
            blitDimension(vram, ndTexture);
            SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
            SDL_RenderPresent(ndRenderer);
        } else {
            host_sleep_ms(100);
        }
    }

    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);

    return 0;
}

void NDSDL::init(void) {
    if(!(repaintThread)) {
        int x, y, w, h;
        char title[32];
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        SDL_GetWindowSize(sdlWindow, &w, &h);
        sprintf(title, "NeXTdimension (Slot %i)", slot);
        ndWindow = SDL_CreateWindow(title, (x-w)+1, y, 1120, 832, SDL_WINDOW_HIDDEN);
        
        if (!ndWindow) {
            fprintf(stderr,"[ND] Slot %i: Failed to create window!\n", slot);
            exit(-1);
        }
    }
    
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        SDL_ShowWindow(ndWindow);
    } else {
        SDL_HideWindow(ndWindow);
    }
}

void NDSDL::start_interrupts() {
    char name[32];
    
    if (!(repaintThread) && ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        sprintf(name, "[ND] Slot %i: Repainter", slot);
        repaintThread = SDL_CreateThread(NDSDL::repainter, name, this);
    }
    
    CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_ND_VBL);
    CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_ND_VIDEO_VBL);
}

void nd_vbl_handler(void)       {
    CycInt_AcknowledgeInterrupt();

    for(int slot = 2; slot < 16; slot += 2) {
        if(NextDimension* nd = dynamic_cast<NextDimension*>(nextbus[slot])) {
            host_blank(nd->slot, ND_DISPLAY, nd->sdl.ndVBLtoggle);
            nd->sdl.ndVBLtoggle = !nd->sdl.ndVBLtoggle;
        }
    }
    // 136Hz with toggle gives 68Hz, blank time is 1/2 frame time
    CycInt_AddRelativeInterruptUs((1000*1000)/136, 0, INTERRUPT_ND_VBL);
}
void nd_video_vbl_handler(void) {
    CycInt_AcknowledgeInterrupt();

    for(int slot = 2; slot < 16; slot += 2) {
        if(NextDimension* nd = dynamic_cast<NextDimension*>(nextbus[slot])) {
            host_blank(slot, ND_VIDEO, nd->sdl.ndVideoVBLtoggle);
            nd->sdl.ndVideoVBLtoggle = !nd->sdl.ndVideoVBLtoggle;
        }
    }
    
    // 120Hz with toggle gives 60Hz NTSC, blank time is 1/2 frame time
    CycInt_AddRelativeInterruptUs((1000*1000)/120, 0, INTERRUPT_ND_VIDEO_VBL);
}

void NDSDL::uninit(void) {
    SDL_HideWindow(ndWindow);
}

void NDSDL::pause(bool pause) {
    SDL_AtomicSet(&blitNDFB, pause ? 0 : 1);
}

void nd_sdl_destroy(void) {
    for(int slot = 2; slot < 16; slot += 2) {
        if(NextDimension* d = dynamic_cast<NextDimension*>(nextbus[slot])) {
            d->sdl.destroy();
        }
    }
}

void NDSDL::destroy(void) {
    doRepaint = false; // stop repaint thread
    int s;
    SDL_WaitThread(repaintThread, &s);
    uninit();
}
