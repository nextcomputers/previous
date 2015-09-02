#define ENABLE_DIMENSION    0

#if ENABLE_DIMENSION
Uint32 nd_slot_lget(Uint32 addr);
Uint16 nd_slot_wget(Uint32 addr);
Uint8 nd_slot_bget(Uint32 addr);
void nd_slot_lput(Uint32 addr, Uint32 l);
void nd_slot_wput(Uint32 addr, Uint16 w);
void nd_slot_bput(Uint32 addr, Uint8 b);

Uint32 nd_board_lget(Uint32 addr);
Uint16 nd_board_wget(Uint32 addr);
Uint8 nd_board_bget(Uint32 addr);
void nd_board_lput(Uint32 addr, Uint32 l);
void nd_board_wput(Uint32 addr, Uint16 w);
void nd_board_bput(Uint32 addr, Uint8 b);


extern Uint8 ND_vram[4*1024*1024];
extern bool enable_dimension_screen;

void dimension_reset(void);

#endif
