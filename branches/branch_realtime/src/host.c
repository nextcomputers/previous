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

static const size_t NUM_BLANKS = 3;
static const char* BLANKS[] = {
  "main","nd_main","nd_video"  
};

static volatile Uint32 blank[NUM_BLANKS];
static Uint32 vblCounter[NUM_BLANKS];

void host_blank(int slot, int src, bool state) {
    slot = 1 << slot;
    if(state) {
        blank[src] |=  slot;
        vblCounter[src]++;
    }
    else
        blank[src] &= ~slot;
    switch (src) {
        case ND_DISPLAY:   nd_display_blank(); break;
        case ND_VIDEO:     nd_video_blank();   break;
    }
}

bool host_blank_state(int slot, int src) {
    slot = 1 << slot;
    return blank[src] & slot;
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
static Uint32 ticksStart;
static bool   enableRealtime;

void host_reset() {
    perfCounterStart  = SDL_GetPerformanceCounter();
    ticksStart        = SDL_GetTicks();
    cycleCounterStart = 0;
    secsStart         = 0;
    isRealtime        = false;
    oldIsRealtime     = false;
    realTimeAcc       = 0;
    cycleTimeAcc      = 0;
    enableRealtime    = ConfigureParams.System.bRealtime;
    
    for(int i = NUM_BLANKS; --i >= 0;) {
        vblCounter[i] = 0;
        blank[i]      = 0;
    }
    
    cycleDivisor = ConfigureParams.System.nCpuFreq * 1000 * 1000;
}

extern Sint64 nCyclesMainCounter;

void host_realtime(bool state) {
    isRealtime = state & enableRealtime;
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
        if(rt < -0.001) {
            rt *= -1000 * 1000;
            host_sleep_us(rt);
        }
    }
    bool state = isRealtime;
    if(oldIsRealtime != state) {
        if(oldIsRealtime)   realTimeAcc += t;
        else                cycleTimeAcc += t;
        perfCounterStart  = SDL_GetPerformanceCounter();
        cycleCounterStart = nCyclesMainCounter;
        oldIsRealtime     = state;
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
    
    double ticks = SDL_GetTicks() - ticksStart;
    ticks /= 1000;
    
    int rtFactor = (100 * t) / ticks;
    
    fprintf(stderr, "[time] host_time:%g rt:%d%% cycleTime:%g realTime:%g", t, rtFactor, cycleTimeAcc, realTimeAcc);
    
    for(int i = NUM_BLANKS; --i >= 0;)
        fprintf(stderr, " %s:%gHz", BLANKS[i], (double)vblCounter[i]/t);
    
    fprintf(stderr, "\n");
}