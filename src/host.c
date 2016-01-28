#include "config.h"

#if HAVE_NANOSLEEP
#include <sys/time.h>
#endif
#include <errno.h>

#include "host.h"
#include "configuration.h"

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
static Uint64 perfCounterStart;
static Sint64 cycleCounterStart;
static double secsStart;
static bool   isRealtime;
static bool   oldIsRealtime;
static double cycleDivisor;
static double realTimeAcc;
static double cycleTimeAcc;
static lock_t timeLock;

extern Uint8 NEXTRom[0x20000];

void host_reset() {
    perfCounterStart  = SDL_GetPerformanceCounter();
    cycleCounterStart = 0;
    secsStart         = 0;
    isRealtime        = false;
    oldIsRealtime     = false;
    realTimeAcc       = 0;
    cycleTimeAcc      = 0;
    
    cycleDivisor      = ConfigureParams.System.nCpuFreq;
    if(ConfigureParams.System.nCpuLevel == 3) {
        if(NEXTRom[0xFFAB]==0x04) //  HACK for ROM version 0.8.31 power-on test
            cycleDivisor *= 1000 * 1000 * 1;
        else
            cycleDivisor *= 1000 * 1000 * 2;
    } else
        cycleDivisor *= 1000 * 1000 * 3;
}

extern Sint64 nCyclesMainCounter;

void host_realtime(bool state) {
    isRealtime = state;
}

bool host_is_realtime() {
    return isRealtime;
}

double host_time_sec() {
    
    host_lock(&timeLock);
    double rt, t;
    rt  = (SDL_GetPerformanceCounter() - perfCounterStart);
    rt /= SDL_GetPerformanceFrequency();
    if(oldIsRealtime) {
        t = rt;
    } else {
        t  = nCyclesMainCounter - cycleCounterStart;
        t /= cycleDivisor;
        rt -= t;
        if(rt < 0) {
            rt *= -1000 * 1000;
            host_sleep_us(rt);
        }
    }
    if(oldIsRealtime != isRealtime) {
        if(oldIsRealtime)   realTimeAcc += t;
        else                cycleTimeAcc += t;
        perfCounterStart  = SDL_GetPerformanceCounter();
        cycleCounterStart = nCyclesMainCounter;
        oldIsRealtime     = isRealtime;
        secsStart        += t;
        t                 = 0;
    }
    host_unlock(&timeLock);

    return t + secsStart;
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
 * together with a wait-loop.
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
    Uint64 timeout = host_time_us() + us;
    host_sleep_ms(( (Uint32)(ticks_micro / 1000) ) ;	/* micro sec -> milli sec */
    while(host_time_us() < timeout) {}
#endif
}

void host_sleep_ms(Uint32 ms) {
    Uint64 sleep = ms;
    sleep *= 1000;
    host_sleep_us(sleep);
}

void host_sleep_sec(double sec) {
    sec *= 1000 * 1000;
    host_sleep_us((Uint64)sec);
}

void host_lock(lock_t* lock) {
  SDL_AtomicLock(lock);
}

int host_trylock(lock_t* lock) {
  return SDL_AtomicTryLock(lock);
}

void host_unlock(lock_t* lock) {
  SDL_AtomicUnlock(lock);
}

void host_checklock(lock_t* lock) {
  SDL_AtomicLock(lock);
  SDL_AtomicUnlock(lock);
}

thread_t* host_thread_create(thread_func_t func, void* data) {
  return SDL_CreateThread(func, "[ND] Thread", data);
}

int host_thread_wait(thread_t* thread) {
  int status;
  SDL_WaitThread(thread, &status);
  return status;
}
                  
// --- utilities
                  
void host_print_stat() {
    double t = host_time_sec();
    fprintf(stderr, "[host_time] time:%g cycleTime:%g realTime:%g\n", t, cycleTimeAcc, realTimeAcc);
}