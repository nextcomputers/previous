#define AUDIO_FREQUENCY  44100;            /* Sound playback frequency */
#define SND_BUFFER_LIMIT 4096

void SND_IO_Handler(void);
void Sound_Reset(void);

void snd_start_output(Uint8 mode);
void snd_stop_output(void);
void snd_gpo_access(Uint8 data);


struct {
    Uint8  data[SND_BUFFER_LIMIT];
    Uint32 size;
} snd_buffer;
