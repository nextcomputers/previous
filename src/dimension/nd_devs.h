extern void    nd_devs_init();
extern uae_u32 nd_io_lget(uaecptr addr);
extern uae_u32 nd_io_wget(uaecptr addr);
extern uae_u32 nd_io_bget(uaecptr addr);
extern void    nd_io_lput(uaecptr addr, uae_u32 l);
extern void    nd_io_wput(uaecptr addr, uae_u32 w);
extern void    nd_io_bput(uaecptr addr, uae_u32 b);
extern int     nd_process_interrupts(int nHostCycles);
extern uae_u32 nd_ramdac_bget(uaecptr addr);
extern void    nd_ramdac_bput(uaecptr addr, uae_u32 b);
extern uae_u32 nd_dp_lget(uaecptr addr);
extern void    nd_dp_lput(uaecptr addr, uae_u32 b);