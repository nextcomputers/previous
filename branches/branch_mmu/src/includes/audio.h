void Audio_EnableAudio(bool bEnable);
void Audio_SetOutputAudioFreq(int nNewFrequency);
void Audio_Init(void);
void Audio_UnInit(void);

struct {
    Uint8 data[32768];
    Sint32 size;
    Uint32 limit;
} snd_buffer;
