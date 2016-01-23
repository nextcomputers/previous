typedef struct {
    int   addr;
    int   idx;
    Uint8 regs[0x1000];
} bt463;

uae_u32 bt463_bget(bt463* ramdac, uaecptr addr);
void bt463_bput(bt463* ramdac, uaecptr addr, uae_u32 b);