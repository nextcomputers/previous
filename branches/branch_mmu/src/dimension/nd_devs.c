#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "dimension.h"
#include "nd_devs.h"

#if ENABLE_DIMENSION

/* --------- NEXTDIMENSION DEVICES ---------- */

/* Device registers */

/* Memory controller */
#define ND_MC_CSR0          0xFF800000
#define ND_MC_CSR1          0xFF800010
#define ND_MC_CSR2          0xFF800020
#define ND_MC_SID           0xFF800030

#define ND_MC_DMA_CSR       0xFF801000
#define ND_MC_DMA_VSTART    0xFF801010  /* Visible */
#define ND_MC_DMA_VWIDTH    0xFF801020
#define ND_MC_DMA_VMSTART   0xFF801030  /* Visible pixel mask */
#define ND_MC_DMA_VMWIDTH   0xFF801040
#define ND_MC_DMA_BSTART    0xFF801050  /* Blank */
#define ND_MC_DMA_BWIDTH    0xFF801060
#define ND_MC_DMA_BMSTART   0xFF801070  /* Blank pixel mask */
#define ND_MC_DMA_BMWIDTH   0xFF801080
#define ND_MC_DMA_VTOP      0xFF801090
#define ND_MC_DMA_VBOTTOM   0xFF8010A0
#define ND_MC_DMA_LINE      0xFF8010B0
#define ND_MC_DMA_CUR       0xFF8010C0
#define ND_MC_DMA_MLINE     0xFF8010D0
#define ND_MC_DMA_MCUR      0xFF8010E0
#define ND_MC_DMA_OUT       0xFF8010F0

#define ND_MC_VRAM_TIMING	0xFF802000
#define ND_MC_DRAM_SIZE		0xFF803000


/* CSR bits */
#define CSR0_i860PIN_RESET  0x00000001
#define CSR0_i860PIN_CS8    0x00000002
#define CSR0_i860_IMASK     0x00000004
#define CSR0_i860_INT       0x00000008
#define CSR0_BE_IMASK       0x00000010
#define CSR0_BE_INT         0x00000020
#define CSR0_VBL_IMASK      0x00000040
#define CSR0_VBL_INT        0x00000080
#define CSR0_VBLANK         0x00000100 /* ro */
#define CSR0_VIOVBL_IMASK   0x00000200
#define CSR0_VIOVBL_INT     0x00000400
#define CSR0_VIOBLANK       0x00000800 /* ro */
#define CSR0_i860PIN_KEN    0x00001000

#define CSR1_CPU_INT        0x00000001

#define CSR2_GLOBAL_ACCESS  0x00000001

#define CSRDMA_VISIBLE_EN   0x00000001
#define CSRDMA_BLANKED_EN   0x00000002
#define CSRDMA_READ_EN      0x00000004

#define CSRVRAM_VBLANK      0x00000001
#define CSRVRAM_60HZ        0x00000002
#define CSRVRAM_EXT_SYNC    0x00000004

#define CSRDRAM_4MBIT       0x00000001


struct {
	uae_u32 csr0;
	uae_u32 csr1;
	uae_u32 csr2;
	uae_u32 sid;
    uae_u32 dmacsr;
    uae_u32 vram;
    uae_u32 dram;
} nd_mc;

uae_u32 nd_mc_read_register(uaecptr addr) {
	switch (addr&0x3FFF) {
		case 0x0000:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR0 read %08X at %08X",nd_mc.csr0,addr);
			return nd_mc.csr0;
		case 0x0010:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR1 read %08X at %08X",nd_mc.csr1,addr);
			return nd_mc.csr1;
		case 0x0020:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR2 read %08X at %08X",nd_mc.csr2,addr);
			return nd_mc.csr2;
		case 0x0030:
			Log_Printf(LOG_WARN, "[ND] Memory controller SID read %08X at %08X",nd_mc.sid,addr);
			return nd_mc.sid;
		case 0x1000:
			Log_Printf(LOG_WARN, "[ND] Memory controller DMA CSR read at %08X",addr);
			return nd_mc.dmacsr;
		case 0x2000:
			Log_Printf(LOG_WARN, "[ND] Memory controller VRAM timing read at %08X",addr);
			return nd_mc.vram;
		case 0x3000:
			Log_Printf(LOG_WARN, "[ND] Memory controller DRAM size read at %08X",addr);
			return nd_mc.dram;

		default:
			Log_Printf(LOG_WARN, "[ND] Memory controller UNKNOWN read at %08X",addr);
			break;
	}
	return 0;
}

void nd_mc_write_register(uaecptr addr, uae_u32 val) {
	switch (addr&0x3FFF) {
		case 0x0000:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR0 write %08X at %08X",val,addr);
			nd_mc.csr0 = val;
			break;
		case 0x0010:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR1 write %08X at %08X",val,addr);
			nd_mc.csr1 = val;
			break;
		case 0x0020:
			Log_Printf(LOG_WARN, "[ND] Memory controller CSR2 write %08X at %08X",val,addr);
			nd_mc.csr2 = val;
			break;
		case 0x0030:
			Log_Printf(LOG_WARN, "[ND] Memory controller SID write %08X at %08X",val,addr);
			nd_mc.sid = val;
			break;
		case 0x1000:
			Log_Printf(LOG_WARN, "[ND] Memory controller DMA CSR write at %08X",addr);
            nd_mc.dmacsr = val;
			break;
		case 0x2000:
			Log_Printf(LOG_WARN, "[ND] Memory controller VRAM timing write at %08X",addr);
            nd_mc.vram = val;
			break;
		case 0x3000:
			Log_Printf(LOG_WARN, "[ND] Memory controller DRAM size write at %08X",addr);
            nd_mc.dram = val;
			break;
			
		default:
			Log_Printf(LOG_WARN, "[ND] Memory controller UNKNOWN write at %08X",addr);
			break;
	}
}


/* NeXTdimension device space */
inline uae_u32 nd_io_lget(uaecptr addr)
{
	return nd_mc_read_register(addr);
}

inline uae_u32 nd_io_wget(uaecptr addr)
{
	return 0;
}

inline uae_u32 nd_io_bget(uaecptr addr)
{
	return 0;
}

inline void nd_io_lput(uaecptr addr, uae_u32 l)
{
	nd_mc_write_register(addr, l);
}

inline void nd_io_wput(uaecptr addr, uae_u32 w)
{
}

inline void nd_io_bput(uaecptr addr, uae_u32 b)
{
}

#endif
