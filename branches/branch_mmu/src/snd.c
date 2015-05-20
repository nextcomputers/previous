#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "cycInt.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"


/* queue prototypes */
struct queuepacket{
    int len;
    unsigned char data[4096];
};

typedef struct queuepacket *queueElementT;
typedef struct queueCDT *queueADT;
extern  queueADT QueueCreate(void);
extern  void QueueDestroy(queueADT queue);
extern  void QueueEnter(queueADT queue, queueElementT element);
extern  queueElementT QueueDelete(queueADT queue);
extern  int QueueIsEmpty(queueADT queue);
extern  int QueueIsFull(queueADT queue);
extern  int QueuePeek(queueADT queue);

queueADT	soundq;


/* Initialize the audio system */
bool sound_inited;

void sound_init(void) {
    snd_buffer.limit=4096;
    if (!sound_inited) {
        Log_Printf(LOG_WARN, "Starting SDL_AudioDevice");
        Audio_Init();
        soundq = QueueCreate();
        sound_inited=true;
    }
}

void sound_uninit(void) {
    if(sound_inited) {
        Log_Printf(LOG_WARN, "Stopping SDL_AudioDevice");
        sound_inited=false;
        Audio_UnInit();
        QueueDestroy(soundq);
    }
}


/* Start and stop sound output */
bool sound_output_active = false;

void snd_start_output(void) {
    if (!sound_inited) {
        sound_init();//abort();
    }
    if (!sound_output_active) {
        Log_Printf(LOG_WARN, "[SDL Audio] Starting.");
        /* Starting SDL Audio and sound output loop */
        Audio_EnableAudio(true);
        sound_output_active = true;
        CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_SND_IO);
    }
}

void snd_stop_output(void) {
    if (sound_output_active) {
        sound_output_active=false;
    }
}


/* Sound IO loop (reads from via DMA from memory to queue) */
#define SND_DELAY   100000
int old_size;

void SND_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    if (QueuePeek(soundq)<4) {
        old_size = snd_buffer.size;
        dma_sndout_read_memory();

        if (snd_buffer.size==4096 || snd_buffer.size==old_size) {
            Log_Printf(LOG_WARN, "[Sound] %i samples ready.",snd_buffer.size/4);
            snd_buffer.limit = snd_buffer.size;
            snd_send_sound_out();
            snd_buffer.limit = 4096;
            snd_buffer.size = 0; /* Must be 0 */
        }

        if (!sound_output_active)
            return;
    } /* if queuepeek<4 */
    CycInt_AddRelativeInterrupt(SND_DELAY, INT_CPU_CYCLE, INTERRUPT_SND_IO);
}

void snd_send_sound_out(void) {
    int i;
    struct queuepacket *p;
    p=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    Audio_Lock();
    for (i=0; i<4096; i++) {
        if (snd_buffer.size>0) {
            p->data[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            p->data[i] = 0;
        }
        
    }
    p->len=4096;
    QueueEnter(soundq,p);
    Audio_Unlock();
    Log_Printf(LOG_WARN, "[Sound] Output 1024 samples to queue");
}


/* This function is called from the audio system to poll data */
void snd_queue_poll(Uint8 *buf, int len) {
    if (QueuePeek(soundq)>0) {
        struct queuepacket *qp;
        qp=QueueDelete(soundq);
        Log_Printf(LOG_WARN, "[SDL Audio] Reading 1024 samples from queue.");
        memcpy(buf,qp->data,len);
    } else if (!sound_output_active) {
        /* Last packet received, stop */
        Log_Printf(LOG_WARN, "[SDL Audio] Done. Stopping.");
        Audio_EnableAudio(false);
    } else {
        Log_Printf(LOG_WARN, "[SDL Audio] Not ready. No data on queue.");
        memset(buf, 0, len);
    }
}