uae_u32 tmc_lget(uaecptr addr);
uae_u32 tmc_wget(uaecptr addr);
uae_u32 tmc_bget(uaecptr addr);

void tmc_lput(uaecptr addr, uae_u32 l);
void tmc_wput(uaecptr addr, uae_u32 w);
void tmc_bput(uaecptr addr, uae_u32 b);

void TMC_Reset(void);