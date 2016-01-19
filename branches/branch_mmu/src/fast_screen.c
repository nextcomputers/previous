/*
  Hatari - screen.c

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.

 (SC) Simon Schubiger - most of it rewritten for Previous NeXT emualtor
*/

#include <SDL.h>
#include <SDL_endian.h>
#include <SDL_blendmode.h>

#if 0
void blitDimension(SDL_Texture* tex) {}
#include "screen.c"
#else

const char Screen_fileid[] = "Previous fast_screen.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "m68000.h"
#include "dimension.h"
#include "paths.h"
#include "screen.h"
#include "control.h"
#include "convert/routines.h"
#include "resolution.h"
#include "statusbar.h"
#include "video.h"

/* extern for several purposes */
SDL_Thread*  repaintThread;
SDL_sem*     initLatch;
SDL_Window*  sdlWindow;
SDL_Surface* sdlscrn = NULL;   /* The SDL screen surface */
int nScreenZoomX, nScreenZoomY;/* Zooming factors, used for scaling mouse motions */
SDL_atomic_t blitUI;           /* When value = 1, the repaint thread will blit the sldscrn surface to the screen on the next redraw */
SDL_Rect     saveWindowBounds; /* Window bounds before going fullscreen. Used to restore window size & position. */

/* extern for shortcuts and falcon/hostscreen.c */
volatile bool bGrabMouse    = false;      /* Grab the mouse cursor in the window */
volatile bool bInFullScreen = false;   /* true if in full screen */

/* (SC) True for bulk rendering during Screen_DrawFrame() */
static volatile bool doRepaint  = true;

static Uint32 BW2RGB[0x400];
static Uint32 COL2RGB[0x10000];

static Uint32 bw2rgb(SDL_PixelFormat* format, int bw) {
    switch(bw & 3) {
        case 3:  return SDL_MapRGB(format, 0,   0,   0);
        case 2:  return SDL_MapRGB(format, 85,  85,  85);
        case 1:  return SDL_MapRGB(format, 170, 170, 170);
        case 0:  return SDL_MapRGB(format, 255, 255, 255);
        default: return 0;
    }
}

static Uint32 col2rgb(SDL_PixelFormat* format, int col) {
    int r = col & 0xF000; r >>= 12; r |= r << 4;
    int g = col & 0x0F00; g >>= 8;  g |= g << 4;
    int b = col & 0x00F0; b >>= 4;  b |= b << 4;
    return SDL_MapRGB(format, r,   g,   b);
}

/*
 BW format is 2bit per pixel
 */
static void blitBW(SDL_Texture* tex) {
    void* pixels;
    int   d;
    int   pitch = ConfigureParams.System.bTurbo ? 280 : 288;
    SDL_LockTexture(tex, NULL, &pixels, &d);
    Uint32* dst = (Uint32*)pixels;
    for(int y = 0; y < 832; y++) {
        int src     = y * pitch;
        for(int x = 0; x < 280; x++, src++) {
            int idx = NEXTVideo[src] * 4;
            *dst++  = BW2RGB[idx+0];
            *dst++  = BW2RGB[idx+1];
            *dst++  = BW2RGB[idx+2];
            *dst++  = BW2RGB[idx+3];
        }
    }
    SDL_UnlockTexture(tex);
}

/*
 Color format is 4bit per pixel, big-endian: RGBx
 */
static void blitColor(SDL_Texture* tex) {
    void* pixels;
    int   d;
    int pitch = ConfigureParams.System.bTurbo ? 1120 : 1152;
    SDL_LockTexture(tex, NULL, &pixels, &d);
    Uint32* dst = (Uint32*)pixels;
    for(int y = 0; y < 832; y++) {
        Uint16* src = (Uint16*)NEXTColorVideo + (y*pitch);
        for(int x = 0; x < 1120; x++) {
            *dst++ = COL2RGB[*src++];
        }
    }
    SDL_UnlockTexture(tex);
}

extern 

/*
 Dimension format is 8bit per pixel, big-endian: RRGGGBBBA
 */
void blitDimension(SDL_Texture* tex) {
    Uint32* src = (Uint32*)&ND_vram[ND_vram_off];
    void*   pixels;
    int     d;
    Uint32  format;
    SDL_QueryTexture(tex, &format, &d, &d, &d);
    SDL_LockTexture(tex, NULL, &pixels, &d);
    Uint32* dst = (Uint32*)pixels;
    if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        /* Add big-endian accelerated blit loops as needed */
        switch (format) {
            default: {
                /* fallback to SDL_MapRGB */
                SDL_PixelFormat* pformat = SDL_AllocFormat(format);
                for(int y = 832; --y >= 0;) {
                    for(int x = 1120; --x >= 0;) {
                        Uint32 v = *src++;
                        *dst++   = SDL_MapRGB(pformat, (v >> 24) & 0xFF, (v>>16) & 0xFF, (v>>8) & 0xFF);
                    }
                    src += 1152 - 1120;
                }
                SDL_FreeFormat(pformat);
                break;
            }
        }
    } else {
        /* Add little-endian accelerated blit loops as needed */
        switch (format) {
            case SDL_PIXELFORMAT_ARGB8888:
                for(int y = 832; --y >= 0;) {
                    for(int x = 1120; --x >= 0;) {
                        // Uint32 LE: AABBGGRR
                        // Target:    AARRGGBB
                        Uint32 v = *src++;
                        *dst++   = (v & 0xFF000000) | ((v<<16) &0x00FF0000) | (v &0x0000FF00) | ((v>>16) &0x000000FF);
                    }
                    src += 1152 - 1120;
                }
                break;
            default: {
                /* fallback to SDL_MapRGB */
                SDL_PixelFormat* pformat = SDL_AllocFormat(format);
                for(int y = 832; --y >= 0;) {
                    for(int x = 1120; --x >= 0;) {
                        Uint32 v = SDL_Swap32(*src++);
                        *dst++   = SDL_MapRGB(pformat, (v >> 24) & 0xFF, (v>>16) & 0xFF, (v>>8) & 0xFF);
                    }
                    src += 1152 - 1120;
                }
                SDL_FreeFormat(pformat);
                break;
            }
        }
    }
    SDL_UnlockTexture(tex);
}

/*
 Blit NeXT framebuffer to texture.
 */
static void blitScreen(SDL_Texture* tex) {
#if ENABLE_DIMENSION
    if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
        blitDimension(tex);
        return;
    }
#endif
    if(ConfigureParams.System.bColor) {
        blitColor(tex);
    } else {
        blitBW(tex);
    }
}

/*
 Initializes sdl graphics and the enter repaint loop.
 Loop: blits the NeXT framebuffer to the fbTexture, blends with the GUI surface and
 shows it.
 */
static int repainter(void* unused) {
    /*
    //(SC) Use this code for Screen_DrawFrame relative time measurments
    double elapsed = 0;
    double total   = 0;
    Uint64 count   = 0;
    double min     = 100000;
    double max     = 0;
*/
    int Width;
    int Height;

    SDL_GetWindowSize(sdlWindow, &Width, &Height);
    
    SDL_Renderer* sdlRenderer;
    SDL_Texture*  uiTexture;
    SDL_Texture*  fbTexture;
    
    Uint32 r, g, b, a;
    Uint32 mask;
    
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(sdlRenderer, Width, Height);
    
    uiTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, Width, Height);
    SDL_SetTextureBlendMode(uiTexture, SDL_BLENDMODE_BLEND);
    
    fbTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, Width, Height);
    SDL_SetTextureBlendMode(fbTexture, SDL_BLENDMODE_NONE);
    
    Uint32 format;
    int    d;
    SDL_QueryTexture(uiTexture, &format, &d, &d, &d);
    SDL_PixelFormatEnumToMasks(format, &d, &r, &g, &b, &a);
    mask = g | a;
    sdlscrn = SDL_CreateRGBSurface(SDL_SWSURFACE, Width, Height, 32, r, g, b, a);
    // clear UI with mask
    SDL_FillRect(sdlscrn, NULL, mask);
    
    /* Exit if we can not open a screen */
    if (!sdlscrn) {
        fprintf(stderr, "Could not set video mode:\n %s\n", SDL_GetError() );
        SDL_Quit();
        exit(-2);
    }
    
    if (!bInFullScreen) {
        /* re-embed the new SDL window */
        Control_ReparentWindow(Width, Height, bInFullScreen);
    }
    
    Statusbar_Init(sdlscrn);
    
	if (bGrabMouse) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
    }

	/* Configure some SDL stuff: */
	SDL_ShowCursor(SDL_DISABLE);
    
    /* Setup lookup tables */
    SDL_PixelFormat* pformat = SDL_AllocFormat(format);
    /* initialize BW lookup table */
    for(int i = 0; i < 0x100; i++) {
        BW2RGB[i*4+0] = bw2rgb(pformat, i>>6);
        BW2RGB[i*4+1] = bw2rgb(pformat, i>>4);
        BW2RGB[i*4+2] = bw2rgb(pformat, i>>2);
        BW2RGB[i*4+3] = bw2rgb(pformat, i>>0);
    }
    /* initialize color lookup table */
    for(int i = 0; i < 0x10000; i++)
        COL2RGB[SDL_BYTEORDER == SDL_BIG_ENDIAN ? i : SDL_Swap16(i)] = col2rgb(pformat, i);
    
    /* Initialization done -> signal */
    SDL_SemPost(initLatch);
    
    /* Enter repaint loop */
    while(doRepaint) {
        /*
        // (SC) Screen_DrawFrame time measurment
        Uint64 before = SDL_GetPerformanceCounter();
        */
        
        SDL_RenderClear(sdlRenderer);
        
        // draw the NeXT framebuffer
        blitScreen(fbTexture);
        
        // draw statusbar or overlay led(s)
        Statusbar_OverlayBackup(sdlscrn);
        Statusbar_Update(sdlscrn);
        
        // Render NeXT framebuffer
        SDL_RenderCopy(sdlRenderer, fbTexture, NULL, NULL);
        // Copy UI to screen
        if(SDL_AtomicSet(&blitUI, 0)) {
            // poor man's green-screen - would be nice if SDL had more blending modes...
            int     count = Width*Height;
            void*   tmp   = malloc(count*4);
            Uint32* dst   = (Uint32*)tmp;
            Uint32* src   = (Uint32*)sdlscrn->pixels;
            for(int i = Width*Height; --i >= 0; src++)
                *dst++ = *src == mask ? 0 : *src;
            // update UI texture and render it
            SDL_UpdateTexture(uiTexture, NULL, tmp, sdlscrn->pitch);
            SDL_RenderCopy(sdlRenderer, uiTexture, NULL, NULL);
            free(tmp);
        }
        
        /*
        elapsed += SDL_GetPerformanceCounter() - before;
        
        elapsed /= (SDL_GetPerformanceFrequency() / 1000.0);
        total += elapsed;
        if(elapsed < min) min = elapsed;
        if(elapsed > max) max = elapsed;
        count++;
        
        if((count % 10) == 0)
            Log_Printf(LOG_WARN, "Screen_DrawFrame (ms): %g min=%g avg=%g max=%g", elapsed, min, total / count, max);
        */
         
        // SDL_RenderPresent sleeps until next VSYNC because of SDL_RENDERER_PRESENTVSYNC in ScreenInit
        SDL_RenderPresent(sdlRenderer);
    }
    return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Init Screen bitmap and buffers/tables needed for ST to PC screen conversion
 */
void Screen_Init(void) {
    /* Set initial window resolution */
    bInFullScreen = ConfigureParams.Screen.bFullScreen;
    nScreenZoomX  = 1;
    nScreenZoomY  = 1;

    int Width  = 1120;
    int Height = 832;
    int SBarHeight, BitCount, maxW, maxH;
    
    /* Statusbar height for doubled screen size */
    SBarHeight = Statusbar_GetHeightForSize(1120, 832);
    Resolution_GetLimits(&maxW, &maxH, &BitCount);
    Height += Statusbar_SetHeight(Width, Height);
    
    if (bInFullScreen) {
        /* unhide the WM window for fullscreen */
        Control_ReparentWindow(Width, Height, bInFullScreen);
    }
    
    /* Set new video mode */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    
    fprintf(stderr, "SDL screen request: %d x %d @ %d (%s)\n", Width, Height, BitCount, bInFullScreen ? "fullscreen" : "windowed");
    
    int x = SDL_WINDOWPOS_UNDEFINED;
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        for(int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            SDL_Rect r;
            SDL_GetDisplayBounds(i, &r);
            if(r.w >= Width * 2) {
                x = r.x + Width + ((r.w - Width * 2) / 2);
                break;
            }
            if(r.x >= 0 && SDL_GetNumVideoDisplays() == 1) x = r.x + 8;
        }
    }
    sdlWindow  = SDL_CreateWindow(PROG_NAME, x, SDL_WINDOWPOS_UNDEFINED, Width, Height, 0);
    if (!sdlWindow) {
        fprintf(stderr,"Failed to create window: %s!\n", SDL_GetError());
        exit(-1);
    }

    initLatch     = SDL_CreateSemaphore(0);
    repaintThread = SDL_CreateThread(repainter, "[Previous] screen repaint", NULL);
    SDL_SemWait(initLatch);
}

extern void nd_sdl_destroy();

/*-----------------------------------------------------------------------*/
/**
 * Free screen bitmap and allocated resources
 */
void Screen_UnInit(void) {
    doRepaint = false; // stop repaint thread
    int s;
    SDL_WaitThread(repaintThread, &s);
    nd_sdl_destroy();
}

/*-----------------------------------------------------------------------*/
/**
 * Enter Full screen mode
 */
void Screen_EnterFullScreen(void) {
	bool bWasRunning;

	if (!bInFullScreen) {
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = true;

        SDL_GetWindowPosition(sdlWindow, &saveWindowBounds.x, &saveWindowBounds.y);
        SDL_GetWindowSize(sdlWindow, &saveWindowBounds.w, &saveWindowBounds.h);
        SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_Delay(20);                  /* To give monitor time to change to new resolution */
		
		if (bWasRunning) {
			/* And off we go... */
			Main_UnPauseEmulation();
		}
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Return from Full screen mode back to a window
 */
void Screen_ReturnFromFullScreen(void) {
	bool bWasRunning;

	if (bInFullScreen) {
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = false;

        SDL_SetWindowFullscreen(sdlWindow, 0);
		SDL_Delay(20);                /* To give monitor time to switch resolution */
        SDL_SetWindowPosition(sdlWindow, saveWindowBounds.x, saveWindowBounds.y);
        SDL_SetWindowSize(sdlWindow, saveWindowBounds.w, saveWindowBounds.h);
        
		if (bWasRunning) {
			/* And off we go... */
			Main_UnPauseEmulation();
		}

		if (!bGrabMouse) {
			/* Un-grab mouse pointer in windowed mode */
			SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
		}
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Force things associated with changing between low/medium/high res.
 */
void Screen_ModeChanged(void) {
	if (!sdlscrn) {
		/* screen not yet initialized */
		return;
	}
	if (bInFullScreen || bGrabMouse) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Draw screen to window/full-screen - (SC) empty. Screen re-draw is done in repaint thread.
 */
bool Screen_Draw(void) {
    return !bQuitProgram;
}

void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
    SDL_AtomicSet(&blitUI, 1);
}

void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h) {
    SDL_AtomicSet(&blitUI, 1);
}

/*-----------------------------------------------------------------------*/
/**
 * Reset screen - (SC) unused
 */
void Screen_Reset(void) {}

/*-----------------------------------------------------------------------*/
/**
 * Set flags so screen will be TOTALLY re-drawn (clears whole of full-screen)
 * next time around - (SC) unused
 */
void Screen_SetFullUpdate(void) {}

#endif