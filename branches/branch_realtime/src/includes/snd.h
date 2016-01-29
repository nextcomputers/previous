#define AUDIO_FREQUENCY  44100;            /* Sound playback frequency */

void SND_IO_Handler(void);
void Sound_Reset(void);

void snd_start_output(Uint8 mode);
void snd_stop_output(void);
void snd_gpo_access(Uint8 data);
