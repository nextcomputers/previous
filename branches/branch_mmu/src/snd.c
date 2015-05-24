#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "cycInt.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"

#define LOG_SND_LEVEL   LOG_DEBUG
#define LOG_VOL_LEVEL   LOG_WARN

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

queueADT	sndout_q;


/* Initialize the audio system */
bool sndout_inited;

void sound_init(void) {
    snd_buffer.limit=4096;
    if (!sndout_inited && ConfigureParams.Sound.bEnableSound) {
        Log_Printf(LOG_WARN, "[Audio] Initializing audio device.");
        Audio_Output_Init();
        sndout_q = QueueCreate();
        sndout_inited=true;
    }
}

void sound_uninit(void) {
    if(sndout_inited) {
        Log_Printf(LOG_WARN, "[Audio] Uninitializing audio device.");
        sndout_inited=false;
        Audio_Output_UnInit();
        QueueDestroy(sndout_q);
    }
}

void Sound_Reset(void) {
    sound_uninit();
    sound_init();
}


/* Start and stop sound output */
struct {
    Uint8 mode;
    Uint8 mute;
    Uint8 lowpass;
    Uint8 volume[2]; /* 0 = left, 1 = right */
} sndout_state;

/* Maximum volume (really is attenuation) */
#define SND_MAX_VOL 43

/* Valid modes */
#define SND_MODE_NORMAL 0x00
#define SND_MODE_DBL_RP 0x10
#define SND_MODE_DBL_ZF 0x30

/* Function prototypes */
void snd_send_samples(void);
void snd_send_normal_samples(void);
void snd_send_double_samples(bool repeat);
void snd_adjust_volume(void);

bool sound_output_active = false;

void snd_start_output(Uint8 mode) {
    sndout_state.mode = mode;
    /* Starting SDL Audio */
    if (sndout_inited) {
        Audio_Output_Enable(true);
    } else {
        Log_Printf(LOG_SND_LEVEL, "[Audio] Not starting. Audio device not initialized.");
    }
    /* Starting sound output loop */
    if (!sound_output_active) {
        Log_Printf(LOG_SND_LEVEL, "[Audio] Starting.");
        sound_output_active = true;
        CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_SND_IO);
    }
}

void snd_stop_output(void) {
    if (sound_output_active) {
        sound_output_active=false;
    }
}


/* Sound IO loop (reads via DMA from memory to queue) */
#define SND_DELAY   100000
int old_size;

void SND_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    if (!sndout_inited || sndout_state.mute) {
        snd_buffer.limit = 4096;
        dma_sndout_read_memory();
        snd_buffer.size = 0;
    } else if (QueuePeek(sndout_q)<4) {
        old_size = snd_buffer.size;
        dma_sndout_read_memory();

        if (snd_buffer.size==4096 || snd_buffer.size==old_size) {
            Log_Printf(LOG_SND_LEVEL, "[Sound] %i samples ready.",snd_buffer.size/4);
            snd_buffer.limit = snd_buffer.size;
            snd_send_samples();
            snd_buffer.limit = 4096;
            snd_buffer.size = 0; /* Must be 0 */
        }

        if (!sound_output_active)
            return;
    } /* if queuepeek<4 */
    CycInt_AddRelativeInterrupt(SND_DELAY, INT_CPU_CYCLE, INTERRUPT_SND_IO);
}

/* These functions put samples to a queue for the audio system */
void snd_send_samples(void) {
    if (sndout_state.volume[0]!=0 || sndout_state.volume[1]!=0) {
        snd_adjust_volume();
    }
    switch (sndout_state.mode) {
        case SND_MODE_NORMAL: snd_send_normal_samples(); break;
        case SND_MODE_DBL_RP: snd_send_double_samples(true); break;
        case SND_MODE_DBL_ZF: snd_send_double_samples(false); break;
        default: break;
    }
}

void snd_send_normal_samples(void) {
    int i;
    struct queuepacket *p;
    p=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    Audio_Output_Lock();
    for (i=0; i<4096; i++) {
        if (snd_buffer.size>0) {
            p->data[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            p->data[i] = 0;
        }
    }
    p->len=4096;
    QueueEnter(sndout_q,p);
    Audio_Output_Unlock();
    Log_Printf(LOG_SND_LEVEL, "[Sound] Output 1024 samples to queue");
}

void snd_send_double_samples(bool repeat) {
    int i;
    struct queuepacket *p1, *p2;
    p1=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    p2=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    Audio_Output_Lock();
    
    for (i=0; i<4096; i++) {
        if (snd_buffer.size>0) {
            p1->data[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            p1->data[i+4] = repeat ? p1->data[i] : 0; /* repeat or zero-fill */
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            p1->data[i] = p1->data[i+4] = 0;
        }
        if ((i&3)==3) i+=4;
    }
    p1->len=4096;
    QueueEnter(sndout_q,p1);
    for (i=0; i<4096; i++) {
        if (snd_buffer.size>0) {
            p2->data[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            p2->data[i+4] = repeat ? p2->data[i] : 0; /* repeat or zero-fill */
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            p2->data[i] = p2->data[i+4] = 0;
        }
        if ((i&3)==3) i+=4;
    }
    p2->len=4096;
    QueueEnter(sndout_q,p2);
    Audio_Output_Unlock();
    Log_Printf(LOG_SND_LEVEL, "[Sound] Output 2048 samples to queue");
}


/* This function is called from the audio system to poll data */
bool audio_flushed=false;

void snd_queue_poll(Uint8 *buf, int len) {
    if (QueuePeek(sndout_q)>0) {
        struct queuepacket *qp;
        audio_flushed = false;
        qp=QueueDelete(sndout_q);
        Log_Printf(LOG_SND_LEVEL, "[Audio] Reading 1024 samples from queue.");
        memcpy(buf,qp->data,len);
        free(qp);
    } else if (!sound_output_active) {
        /* Last packet received, stop */
        if (audio_flushed) {
            Log_Printf(LOG_SND_LEVEL, "[Audio] Done. Stopping.");
            audio_flushed = false;
            Audio_Output_Enable(false);
        } else { /* Flush residual audio from device (required for SDL) */
            Log_Printf(LOG_SND_LEVEL, "[Audio] Done. Flushing.");
            audio_flushed = true;
            memset(buf, 0, len);
        }
    } else {
        Log_Printf(LOG_WARN, "[Audio] Not ready. No data on queue.");
        memset(buf, 0, len);
    }
}


/* This function adjusts sound output volume */
void snd_adjust_volume(void) {
    int i;
    Sint16 ldata, rdata;
    float ladjust = (sndout_state.volume[0]==0)?1:(1-log(sndout_state.volume[0])/log(SND_MAX_VOL));
    float radjust = (sndout_state.volume[1]==0)?1:(1-log(sndout_state.volume[1])/log(SND_MAX_VOL));

    for (i=0; i<4096; i+=4) {
        ldata = (Sint16)((snd_buffer.data[i]<<8)|snd_buffer.data[i+1]);
        rdata = (Sint16)((snd_buffer.data[i+2]<<8)|snd_buffer.data[i+3]);
        ldata = ldata*ladjust;
        rdata = rdata*radjust;
        snd_buffer.data[i] = ldata>>8;
        snd_buffer.data[i+1] = ldata;
        snd_buffer.data[i+2] = rdata>>8;
        snd_buffer.data[i+3] = rdata;
    }
}


/* Internal volume control register access (shifted in left to right)
 *
 * xxx ---- ----  unused bits
 * --- xx-- ----  channel (0x80 = right, 0x40 = left)
 * --- --xx xxxx  volume
 */

Uint8 tmp_vol;
Uint8 chan_lr;
int bit_num;

void snd_access_volume_reg(Uint8 databit) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface shift bit %i (%i).",bit_num,databit?1:0);
    
    if (bit_num<3) {
        /* nothing to do */
    } else if (bit_num<5) {
        chan_lr = (chan_lr<<1)|(databit?1:0);
    } else if (bit_num<11) {
        tmp_vol = (tmp_vol<<1)|(databit?1:0);
    }
    bit_num++;
}

void snd_volume_interface_reset(void) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface reset.");
    
    bit_num = 0;
    chan_lr = 0;
    tmp_vol = 0;
}

void snd_save_volume_reg(void) {
    if (bit_num!=11) {
        Log_Printf(LOG_WARN, "[Sound] Incomplete volume transfer (%i bits).",bit_num);
        return;
    }
    if (tmp_vol>SND_MAX_VOL) {
        Log_Printf(LOG_WARN, "[Sound] Volume limit exceeded (%i).",tmp_vol);
        tmp_vol=SND_MAX_VOL;
    }
    if (chan_lr&1) {
        Log_Printf(LOG_WARN, "[Sound] Setting volume of left channel to %i",tmp_vol);
        sndout_state.volume[0] = tmp_vol;
    }
    if (chan_lr&2) {
        Log_Printf(LOG_WARN, "[Sound] Setting volume of right channel to %i",tmp_vol);
        sndout_state.volume[1] = tmp_vol;
    }
}

/* This function fills the internal volume register */
#define SND_SPEAKER_ENABLE  0x10
#define SND_LOWPASS_ENABLE  0x08

#define SND_INTFC_CLOCK     0x04
#define SND_INTFC_DATA      0x02
#define SND_INTFC_STROBE    0x01

Uint8 old_data;

void snd_gpo_access(Uint8 data) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] GPO access: %02X",data);
    
    sndout_state.mute = data&SND_SPEAKER_ENABLE;
    sndout_state.lowpass = data&SND_LOWPASS_ENABLE;
    
    if (data&SND_INTFC_STROBE) {
        snd_save_volume_reg();
    } else if ((data&SND_INTFC_CLOCK) && !(old_data&SND_INTFC_CLOCK)) {
        snd_access_volume_reg(data&SND_INTFC_DATA);
    } else if ((data&SND_INTFC_CLOCK) == (old_data&SND_INTFC_CLOCK)) {
        snd_volume_interface_reset();
    }
    old_data = data;
}
