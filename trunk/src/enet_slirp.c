#include "m68000.h"
#include "ethernet.h"
#include "enet_slirp.h"
#include "queue.h"

#include <SDL.h>
#ifndef _WIN32
#include <signal.h>
#endif


/****************/
/* -- SLIRP -- */

#include <arpa/inet.h>

/* slirp prototypes */
extern  int slirp_init(void);
int slirp_redir(int is_udp, int host_port, struct in_addr guest_addr, int guest_port);
extern  void slirp_input(const uint8_t *pkt, int pkt_len);
extern  int slirp_select_fill(int *pnfds,
                              fd_set *readfds, fd_set *writefds, fd_set *xfds);
extern  void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds);
extern  void slirp_exit(int);
void slirp_debug_init(char*,int);
extern  void slirp_output(const unsigned char *pkt, int pkt_len);
extern  int slirp_can_output(void);

/* queue prototypes */
queueADT	slirpq;

int slirp_inited;
static SDL_mutex *slirp_mutex = NULL;
SDL_Thread *tick_func_handle;

//Is slirp initalized?
//Is set to true from the init, and false on ethernet disconnect
int slirp_can_output(void)
{
    return slirp_inited;
}

//This is a callback function for SLiRP that sends a packet
//to the calling library.  In this case I stuff
//it in q queue
void slirp_output (const unsigned char *pkt, int pkt_len)
{
    struct queuepacket *p;
    p=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    SDL_LockMutex(slirp_mutex);
    p->len=pkt_len;
    memcpy(p->data,pkt,pkt_len);
    QueueEnter(slirpq,p);
    SDL_UnlockMutex(slirp_mutex);
    Log_Printf(LOG_WARN, "[SLIRP] Output packet with %i bytes to queue",pkt_len);
}

//This function is to be periodically called
//to keep the internal packet state flowing.
void slirp_tick(void)
{
    int ret2,nfds;
    struct timeval tv;
    fd_set rfds, wfds, xfds;
    int timeout;
    nfds=-1;
    
    if (slirp_inited)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&xfds);
        timeout=slirp_select_fill(&nfds,&rfds,&wfds,&xfds); //this can crash
        
        if(timeout<0)
            timeout=500;
        tv.tv_sec=0;
        tv.tv_usec = timeout;    //basilisk default 10000
        
        ret2 = select(nfds + 1, &rfds, &wfds, &xfds, &tv);
        if(ret2>=0){
            SDL_LockMutex(slirp_mutex);
            slirp_select_poll(&rfds, &wfds, &xfds);
            SDL_UnlockMutex(slirp_mutex);
        }
    }
}

static int tick_func(void *arg)
{
    while(slirp_inited)
    {
        SDL_Delay(10);
        slirp_tick();
    }
    return 0;
}


void enet_slirp_queue_poll(void)
{
    SDL_LockMutex(slirp_mutex);
    if (QueuePeek(slirpq)>0)
    {
        struct queuepacket *qp;
        qp=QueueDelete(slirpq);
        Log_Printf(LOG_WARN, "[SLIRP] Getting packet from queue");
        enet_receive(qp->data,qp->len);
        free(qp);
    }
    SDL_UnlockMutex(slirp_mutex);
}

void enet_slirp_input(Uint8 *pkt, int pkt_len) {
    if (slirp_inited) {
        Log_Printf(LOG_WARN, "[SLIRP] Input packet with %i bytes",enet_tx_buffer.size);
        SDL_LockMutex(slirp_mutex);
        slirp_input(pkt,pkt_len);
        SDL_UnlockMutex(slirp_mutex);
    }
}

void enet_slirp_stop(void) {
    int ret;
    
    if(slirp_inited) {
        Log_Printf(LOG_WARN, "Stopping SLIRP");
        slirp_inited=0;
        //slirp_exit(0);
        QueueDestroy(slirpq);
        SDL_DestroyMutex(slirp_mutex);
        SDL_WaitThread(tick_func_handle, &ret);
    }
}

void enet_slirp_start(void) {
    struct in_addr guest_addr;
    
    if (!slirp_inited) {
        Log_Printf(LOG_WARN, "Starting SLIRP");
#ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
#endif
        slirp_init();
        slirpq = QueueCreate();
        slirp_inited=1;
        //SDL_Delay(500);
        slirp_mutex=SDL_CreateMutex();
        tick_func_handle=SDL_CreateThread(tick_func,"SLiRPTickThread", (void *)NULL);
        inet_aton("10.0.2.15", &guest_addr);
        slirp_redir(0, 42323, guest_addr, 23);
    }
}
