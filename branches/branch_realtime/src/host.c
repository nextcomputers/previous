#include "config.h"

#if HAVE_NANOSLEEP
#include <sys/time.h>
#endif
#include <errno.h>

#include "host.h"

/* NeXTdimension blank handling, see nd_sdl.c */
extern void nd_display_blank(void);
extern void nd_video_blank(void);

static volatile Uint32 nd_blank = 0;

void host_nd_blank(int slot, int src, bool state) {
    slot = 1 << (slot + src*16);
    if(state) nd_blank |=  slot;
    else      nd_blank &= ~slot;
    switch (src) {
        case ND_DISPLAY: nd_display_blank(); break;
        case ND_VIDEO:   nd_video_blank();   break;
    }
}

bool host_nd_blank_state(int slot, int src) {
    slot = 1 << (slot + src*16);
    return nd_blank & slot;
}

/* host time base in seconds */
static Uint64 perfCounterStart = 0;
static Uint32 ticksStart       = 0;

double host_time_sec() {
    if(perfCounterStart == 0) {
        ticksStart       = SDL_GetTicks();
        perfCounterStart = SDL_GetPerformanceCounter();
    }
    double t = (SDL_GetPerformanceCounter() - perfCounterStart);
    t /= SDL_GetPerformanceFrequency();
    return t;
}

// Return current time as micro seconds
Uint64 host_time_us() {
    return host_time_sec() * 1000.0 * 1000.0;
}

// Return current time as milliseconds
Uint32 host_time_ms() {
    return  host_time_us() / 1000LL;
}

/*-----------------------------------------------------------------------*/
/**
 * Sleep for a given number of micro seconds.
 * If nanosleep is available, we use it directly, else we use SDL_Delay
 * (which is portable, but less accurate as is uses milli-seconds)
 */

void host_sleep_us(Uint64 us) {
#if HAVE_NANOSLEEP
    struct timespec	ts;
    int		ret;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;	/* micro sec -> nano sec */
    /* wait until all the delay is elapsed, including possible interruptions by signals */
    do {
        errno = 0;
        ret = nanosleep(&ts, &ts);
    } while ( ret && ( errno == EINTR ) );		/* keep on sleeping if we were interrupted */
#else
    host_sleep_ms(( (Uint32)(ticks_micro / 1000) ) ;	/* micro sec -> milli sec */
#endif
}

void host_sleep_ms(Uint32 ms) {
    SDL_Delay(ms);
}

// --- utilities
void host_print_stat() {
    double t = host_time_sec();
    fprintf(stderr, "[host_time] time:%g ticks_drift:%g%%\n", t, fabs(100.0 - ((double)(SDL_GetTicks() - ticksStart) / (10.0*t))));
}