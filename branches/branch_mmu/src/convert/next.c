/*
  Fast screen updates for NeXT systems by (SC) Simon Schubiger
*/

#include "nd_sdl.h"

static void InvalidateScreenBuffer(void) {}

static Uint32 bwcolors[1024];

static Uint32 bw2color(int bw) {
    switch(bw & 3) {
        case 3:  return SDL_MapRGB(sdlscrn->format, 0,   0,   0);
        case 2:  return SDL_MapRGB(sdlscrn->format, 85,  85,  85);
        case 1:  return SDL_MapRGB(sdlscrn->format, 170, 170, 170);
        case 0:  return SDL_MapRGB(sdlscrn->format, 255, 255, 255);
        default: return 0;
    }
}

static void ConvertHighRes_640x8Bit(void) {
    static int          first=1;
    static SDL_Texture* nextColorFB;
    static SDL_Texture* nextColorTFB;
    static Uint8        buffer[sizeof(NEXTColorVideo)];
    int                 y, x;
    static SDL_Rect     boundsNeXT;
    static SDL_Point    ZERO;
    
	if (first) {
		first=0;
        for(int i = 0; i < 256; i++) {
            bwcolors[i*4+0] = bw2color(i>>6);
            bwcolors[i*4+1] = bw2color(i>>4);
            bwcolors[i*4+2] = bw2color(i>>2);
            bwcolors[i*4+3] = bw2color(i>>0);
        }
        nextColorTFB = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STREAMING, 1120, sdlscrn->h);
        nextColorFB  = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STREAMING, 1152, sdlscrn->h);
        boundsNeXT.x = 0;
        boundsNeXT.y = 0;
        boundsNeXT.w = 1120;
        boundsNeXT.h = 832;
        ZERO.x       = 0;
        ZERO.y       = 0;
    }

#if ENABLE_DIMENSION
    if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
        SDL_UpdateTexture(sdlTexture, NULL, (void*)&ND_vram[16], 1152*4);
        return;
    }
#endif
    if(ConfigureParams.System.bColor) {
        SDL_SetRenderTarget(sdlRenderer, sdlTexture);
        Uint16* dst;
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            dst = (Uint16*)NEXTColorVideo;
        } else {
            Uint16* src = (Uint16*)NEXTColorVideo;
            dst         = (Uint16*)buffer;
            for(int i = 1152*832; --i >= 0;)
                dst[i] = SDL_Swap16(src[i]);
        }
        if(ConfigureParams.System.bTurbo) {
            SDL_UpdateTexture(nextColorTFB, NULL, dst, 1120 * 2);
            SDL_RenderCopy(sdlRenderer, nextColorTFB, NULL, NULL);
        } else {
            SDL_UpdateTexture(nextColorFB, NULL, dst, 1152 * 2);
            SDL_RenderCopyEx(sdlRenderer, nextColorFB, &boundsNeXT, &boundsNeXT, 0, &ZERO, SDL_FLIP_NONE);
        }
        SDL_RenderPresent(sdlRenderer);
        SDL_SetRenderTarget(sdlRenderer, NULL);
    } else {
        int pitch = ConfigureParams.System.bTurbo ? 280 : 288;
        for(y = 0; y < 832; y++) {
            int adr = y * pitch;
            Uint32* p = &((Uint32*)sdlscrn->pixels)[y*sdlscrn->w];
            for (x = 0; x < 280; x++, adr++) {
                int idx = NEXTVideo[adr] * 4;
                *p++ = bwcolors[idx++];
                *p++ = bwcolors[idx++];
                *p++ = bwcolors[idx++];
                *p++ = bwcolors[idx];
            }
        }
        SDL_UpdateTexture(sdlTexture, NULL, sdlscrn->pixels, 1120*4);
    }
}
