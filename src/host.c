#include "config.h"

#if HAVE_NANOSLEEP
#ifdef __MINGW32__
#include <unistd.h>
#else
#include <sys/time.h>
#endif
#endif
#include <errno.h>

#include "host.h"
#include "configuration.h"
#include "main.h"
#include "log.h"

/* NeXTdimension blank handling, see nd_sdl.c */
void nd_display_blank(int num);
void nd_video_blank(int num);

#define NUM_BLANKS 3
static const char* BLANKS[] = {
  "main","nd_main","nd_video"  
};

static volatile Uint32 blank[NUM_BLANKS];
static Uint32       vblCounter[NUM_BLANKS];
static Uint64       perfCounterStart;
static Sint64       cycleCounterStart;
static double       cycleSecsStart;
static bool         isRealtime;
static bool         currentIsRealtime;
static double       cycleDivisor;
static lock_t       timeLock;
static Uint32       ticksStart;
static bool         enableRealtime;
static Uint64       hardClockExpected;
static Uint64       hardClockActual;
static time_t       unixTimeStart;
static double       unixTimeOffset = 0;
static double       perfFrequency;
static Uint64       pauseTimeStamp;
static bool         osDarkmatter;

static inline double real_time() {
    double rt  = (SDL_GetPerformanceCounter() - perfCounterStart);
    rt        /= perfFrequency;
    return rt;
}

void host_reset() {
    perfCounterStart  = SDL_GetPerformanceCounter();
    pauseTimeStamp    = perfCounterStart;
    perfFrequency     = SDL_GetPerformanceFrequency();
    ticksStart        = SDL_GetTicks();
    unixTimeStart     = time(NULL);
    cycleCounterStart = 0;
    cycleSecsStart    = 0;
    isRealtime        = false;
    currentIsRealtime = false;
    hardClockExpected = 0;
    hardClockActual   = 0;
    enableRealtime    = ConfigureParams.System.bRealtime;
    osDarkmatter      = false;
    
    for(int i = NUM_BLANKS; --i >= 0;) {
        vblCounter[i] = 0;
        blank[i]      = 0;
    }
    
    cycleDivisor = ConfigureParams.System.nCpuFreq * 1000 * 1000;
    
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

extern Uint8* NEXTRam;

void host_blank(int slot, int src, bool state) {
    int bit = 1 << slot;
    if(state) {
        blank[src] |=  bit;
        vblCounter[src]++;
    }
    else
        blank[src] &= ~bit;
    switch (src) {
        case ND_DISPLAY:   nd_display_blank(slot); break;
        case ND_VIDEO:     nd_video_blank(slot);   break;
    }
    
    // check for darkmatter magic
    osDarkmatter =
    NEXTRam[0x246] == 'd' &&
    NEXTRam[0x247] == 'a' &&
    NEXTRam[0x248] == 'r' &&
    NEXTRam[0x249] == 'k';
}

bool host_blank_state(int slot, int src) {
    int bit = 1 << slot;
    return blank[src] & bit;
}

void host_hardclock(int expected, int actual) {
    if(abs(actual-expected) > 1000) {
        Log_Printf(LOG_WARN, "[Hardclock] expected:%dus actual:%dus\n", expected, actual);
    } else {
        hardClockExpected += expected;
        hardClockActual   += actual;
    }
}

extern Sint64 nCyclesMainCounter;
void host_realtime(bool state) {
    isRealtime = state;
}

double lastHostTime;

double host_time_sec() {
    double hostTime;
    
    host_lock(&timeLock);
    
    if(currentIsRealtime) {
        double rt = real_time();
        // last host time is in the future (compared to real time)
        // hold time until real time catches up
        // this will allow CPU to still run cycles and do some useful work until it
        // waits for some time dependent event
        hostTime = lastHostTime >= rt ? lastHostTime+(1.0/cycleDivisor) : rt;
    } else {
        hostTime  = nCyclesMainCounter - cycleCounterStart;
        hostTime /= cycleDivisor;
        hostTime += cycleSecsStart;
    }
    bool state = (osDarkmatter || isRealtime) && enableRealtime;
    if(currentIsRealtime != state) {
        if(currentIsRealtime) {
            // switching from real-time to cycle-time
            cycleSecsStart    = real_time();;
            cycleCounterStart = nCyclesMainCounter;
        }
        currentIsRealtime = state;
    }
    
    lastHostTime = hostTime;
    host_unlock(&timeLock);
    
    return hostTime;
}

void host_time(double* realTime, double* hostTime) {
    *hostTime = host_time_sec();
    *realTime = real_time();
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
    double rt;
    double vt;
    host_time(&rt, &vt);
    return vt-rt;
}

void host_pause_time(bool pausing) {
    if(pausing) {
        pauseTimeStamp = SDL_GetPerformanceCounter();
    } else {
        perfCounterStart += SDL_GetPerformanceCounter() - pauseTimeStamp;
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Sleep for a given number of micro seconds.
 */
void host_sleep_us(Uint64 us) {
#if HAVE_NANOSLEEP
    struct timespec	ts;
    int		ret;
    ts.tv_sec = us / 1000000LL;
    ts.tv_nsec = (us % 1000000LL) * 1000;	/* micro sec -> nano sec */
    /* wait until all the delay is elapsed, including possible interruptions by signals */
    do {
        errno = 0;
        ret = nanosleep(&ts, &ts);
    } while ( ret && ( errno == EINTR ) );		/* keep on sleeping if we were interrupted */
#else
    double timeout = us;
    timeout /= 1000000.0;
    timeout += real_time();
    host_sleep_ms(( (Uint32)(us / 1000LL)) );
    while(real_time() < timeout) {}
#endif
}

void host_sleep_ms(Uint32 ms) {
    SDL_Delay(ms);
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
  return SDL_CreateThread(func, "Thread", data);
}

int host_thread_wait(thread_t* thread) {
  int status;
  SDL_WaitThread(thread, &status);
  return status;
}
                
int host_num_cpus() {
  return  SDL_GetCPUCount();
}

static double lastVT;
static char   report[512];

const char* host_report(double realTime, double hostTime) {
    double dVT = hostTime - lastVT;

    double hardClock = hardClockExpected;
    hardClock /= hardClockActual == 0 ? 1 : hardClockActual;
    
    char* r = report;
    r += sprintf(r, "[%s] hostTime:%.1f hardClock:%.3fMHz", enableRealtime ? "Max.speed" : "CycleTime", hostTime, hardClock);

    for(int i = NUM_BLANKS; --i >= 0;) {
        r += sprintf(r, " %s:%.1fHz", BLANKS[i], (double)vblCounter[i]/dVT);
        vblCounter[i] = 0;
    }
    
    lastVT = hostTime;

    return report;
}

Uint8* host_malloc_aligned(size_t size) {
    void* result = NULL;
    posix_memalign(&result, 0x10000, size);
    return (Uint8*)result;
}
