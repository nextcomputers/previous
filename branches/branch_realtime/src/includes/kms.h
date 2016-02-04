void KMS_Reset();

void KMS_Ctrl_Snd_Write();
void KMS_Stat_Snd_Read();
void KMS_Ctrl_KM_Write();
void KMS_Stat_KM_Read();
void KMS_Ctrl_TX_Write();
void KMS_Stat_TX_Read();
void KMS_Ctrl_Cmd_Write();
void KMS_Stat_Cmd_Read();

void KMS_Data_Write();
void KMS_Data_Read();

void KMS_KM_Data_Read();

void kms_keydown(Uint8 modkeys, Uint8 keycode);
void kms_keyup(Uint8 modkeys, Uint8 keycode);
void kms_mouse_move(int x, bool left, int y, bool up);
void kms_mouse_button(bool left, bool down);

void kms_response();

void kms_sndout_underrun();
void kms_sndin_overrun();

void Mouse_Handler();

