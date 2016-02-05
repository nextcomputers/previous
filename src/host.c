#include "config.h"

#if HAVE_NANOSLEEP
#include <sys/time.h>
#endif
#include <errno.h>

#include "host.h"
#include "configuration.h"
#include "main.h"

/* NeXTdimension blank handling, see nd_sdl.c */
extern void nd_display_blank(void);
extern void nd_video_blank(void);

static const size_t NUM_BLANKS = 3;
static const char* BLANKS[] = {
  "main","nd_main","nd_video"  
};

static volatile Uint32 blank[NUM_BLANKS];
static Uint32       vblCounter[NUM_BLANKS];
static Uint64       perfCounterStart;
static Sint64       cycleCounterStart;
static double       secsStart;
static bool         isRealtime;
static bool         oldIsRealtime;
static double       cycleDivisor;
static double       realTimeAcc;
static double       cycleTimeAcc;
static lock_t       timeLock;
static Uint32       ticksStart;
static bool         enableRealtime;
static Uint64       hardClockExpected;
static Uint64       hardClockActual;
static time_t       unixTimeStart;
static double       unixTimeOffset = 0;
static double       perfFrequency;
static double       realTimeOffset;
static Uint64       pauseTimeStamp;

void host_reset() {
    perfCounterStart  = SDL_GetPerformanceCounter();
    perfFrequency     = SDL_GetPerformanceFrequency();
    ticksStart        = SDL_GetTicks();
    unixTimeStart     = time(NULL);
    cycleCounterStart = 0;
    secsStart         = 0;
    isRealtime        = false;
    oldIsRealtime     = false;
    realTimeAcc       = 0;
    cycleTimeAcc      = 0;
    hardClockExpected = 0;
    hardClockActual   = 0;
    enableRealtime    = ConfigureParams.System.bRealtime;
    realTimeOffset    = 0;
    
    for(int i = NUM_BLANKS; --i >= 0;) {
        vblCounter[i] = 0;
        blank[i]      = 0;
    }
    
    cycleDivisor = ConfigureParams.System.nCpuFreq * 1000 * 1000;
    
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

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

void host_hardclock(int expected, int actual) {
    if(abs(actual-expected) > 1000)
        fprintf(stderr, "[Hardclock] expected:%dus actual:%dus\n", expected, actual);
    else {
        hardClockExpected += expected;
        hardClockActual   += actual;
    }
}

extern Sint64 nCyclesMainCounter;

void host_realtime(bool state) {
    isRealtime = state & enableRealtime;
}

bool host_is_realtime() {
    return isRealtime;
}

extern void Statusbar_SetCPULed(double virtualTime, double realTime);

double host_time_sec() {
    
    host_lock(&timeLock);
    double rt;
    double t;
    rt  = (SDL_GetPerformanceCounter() - perfCounterStart);
    rt /= perfFrequency;
    if(oldIsRealtime) {
        t = rt;
    } else {
        t  = nCyclesMainCounter - cycleCounterStart;
        t /= cycleDivisor;
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
    
    realTimeOffset = t - rt;

    t += secsStart;
    Statusbar_SetCPULed(t,rt);
    host_unlock(&timeLock);
    
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

time_t host_unix_time() {
    return unixTimeStart + unixTimeOffset + host_time_sec();
}

void host_set_unix_time(time_t now) {
    unixTimeOffset += difftime(now, host_unix_time());
}

double host_real_time_offset() {
    host_lock(&timeLock);
    double result = realTimeOffset;
    host_unlock(&timeLock);
    return result;
}

void host_pause_time(bool pausing) {
    if(pausing) {
        pauseTimeStamp = SDL_GetPerformanceCounter();
    } else {
        perfCounterStart -= SDL_GetPerformanceCounter() - pauseTimeStamp;
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Sleep for a given number of micro seconds. We burn cycles by running
 * the event loop.
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

thread_t* host_thread_create(thread_func_t func, void* data) {
  return SDL_CreateThread(func, "[ND] Thread", data);
}

int host_thread_wait(thread_t* thread) {
  int status;
  SDL_WaitThread(thread, &status);
  return status;
}
                
int host_num_cpus() {
  return  SDL_GetCPUCount();
}
                  
// --- utilities
                  
void host_print_stat() {
    double t = host_time_sec();
    
    double ticks = SDL_GetTicks() - ticksStart;
    ticks /= 1000;
    
    int rtFactor = (100 * t) / ticks;
    double hardClock = hardClockExpected;
    hardClock /= hardClockActual;
    
    fprintf(stderr, "[%s] hostTime:%g rt:%d%% cycleTime:%g realTime:%g hardClock:%gMHz", enableRealtime ? "Realtime" : "CycleTime", t, rtFactor, cycleTimeAcc, realTimeAcc, hardClock);
    
    for(int i = NUM_BLANKS; --i >= 0;)
        fprintf(stderr, " %s:%gHz", BLANKS[i], (double)vblCounter[i]/t);
    
    fprintf(stderr, "\n");
}