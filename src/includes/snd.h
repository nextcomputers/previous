void SND_IO_Handler(void);

void snd_send_sound_out(void);
void snd_queue_poll(Uint8 *buf, int len);
void snd_start_output(void);
void snd_stop_output(void);


struct {
    Uint8 data[4096];
    Uint32 size;
    Uint32 limit;
} snd_buffer;
