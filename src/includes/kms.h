void KMS_Ctrl_Snd_Write(void);
void KMS_Stat_Snd_Read(void);
void KMS_Ctrl_KM_Write(void);
void KMS_Stat_KM_Read(void);
void KMS_Ctrl_TX_Write(void);
void KMS_Stat_TX_Read(void);
void KMS_Ctrl_Cmd_Write(void);
void KMS_Stat_Cmd_Read(void);

void KMS_Data_Write(void);
void KMS_Data_Read(void);

void KMS_KM_Data_Read(void);

void kms_keydown(Uint8 modkeys, Uint8 keycode);
void kms_keyup(Uint8 modkeys, Uint8 keycode);
void kms_mouse_move(int x, bool left, int y, bool up);
void kms_mouse_button(bool left, bool down);

void kms_response(void);

void Mouse_Handler(void);

#define SNDOUT_DMA_ENABLE   0x80
#define SNDOUT_DMA_REQUEST  0x40
#define SNDOUT_DMA_UNDERRUN 0x20
#define SNDIN_DMA_ENABLE    0x08
#define SNDIN_DMA_REQUEST   0x04
#define SNDIN_DMA_OVERRUN   0x02

void kms_snd_dma_or(Uint8 val);
