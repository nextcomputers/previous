void SND_IO_Handler(void);
void Sound_Reset(void);

void snd_send_sound_out(void);
void snd_queue_poll(Uint8 *buf, int len);
void snd_start_output(void);
void snd_stop_output(void);
void snd_change_output_freq(int frequency);


struct {
    Uint8 data[4096];
    Uint32 size;
    Uint32 limit;
} snd_buffer;

/* Valid frequencies */
#define SND_FREQ_DOUBLE 22050
#define SND_FREQ_NORMAL 44100
