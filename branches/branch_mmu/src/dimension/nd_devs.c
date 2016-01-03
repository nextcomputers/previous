#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "dimension.h"
#include "nd_devs.h"
#include "nd_nbic.h"

#if ENABLE_DIMENSION

/* --------- NEXTDIMENSION DEVICES ---------- */

/* Device registers */

/* Memory controller */
#define ND_MC_CSR0          0xFF800000
#define ND_MC_CSR1          0xFF800010
#define ND_MC_CSR2          0xFF800020
#define ND_MC_SID           0xFF800030

#define ND_MC_DMA_CSR       0xFF801000

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
#define CSR0_i860_CACHE_EN  0x00001000

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
    uae_u32 dma_csr;
    uae_u32 dma_start;
    uae_u32 dma_width;
    uae_u32 dma_pstart;
    uae_u32 dma_pwidth;
    uae_u32 dma_sstart;
    uae_u32 dma_swidth;
    uae_u32 dma_bsstart;
    uae_u32 dma_bswidth;
    uae_u32 dma_top;
    uae_u32 dma_bottom;
    uae_u32 dma_line_a;
    uae_u32 dma_curr_a;
    uae_u32 dma_scurr_a;
    uae_u32 dma_out_a;
    uae_u32 vram;
    uae_u32 dram;
} nd_mc;

const int I860_CYC            = 33 * 1000 * 1000;
const int VBL_CYC             = I860_CYC / 68;
const int HEIGHT              = 900; // guess
const int VIS_HEIGHT          = 832;
const int WIDTH               = 1152;
const int VIS_WIDTH           = 1120;
const int HBL_CYC             = I860_CYC / (68 * HEIGHT);
const int V_FRONT             = 1 * HBL_CYC;
const int V_BACK              = VBL_CYC - V_FRONT - (VIS_HEIGHT * HBL_CYC);

const int VIDEO_VBL_CYC       = I860_CYC / 60; // assume NTSC @ 60Hz
const int VIDEO_HEIGHT        = 525;
const int VIDEO_VIS_HEIGHT    = 480;
const int VIDEO_WIDTH         = 720;
const int VIDEO_VIS_WIDTH     = 640;
const int VIDEO_HBL_CYC       = I860_CYC / (60 * VIDEO_HEIGHT);
const int VIDEO_V_FRONT       = 5 * VIDEO_HBL_CYC;
const int VIDEO_V_BACK        = VIDEO_VBL_CYC - VIDEO_V_FRONT - (VIDEO_VIS_HEIGHT * VIDEO_HBL_CYC);

/* Cycle count for VBL interrupts */
int nd_vbl_cyc_count;
/* Cycle count for video interrupts */
int nd_video_cyc_count;


#define DP_IIC_MORE 0x20000000
#define DP_IIC_BUSY 0x80000000

struct {
    uae_u8  iic_addr;
    uae_u8  iic_msg[4096];
    uae_u32 iic_msgsz;
    int     iic_busy;
    uae_u32 csr;
    uae_u32 alpha;
    uae_u32 dma;
    uae_u32 x;
    uae_u32 y;
    uae_u32 iic_stat_addr;
    uae_u32 iic_data;
} nd_dp;

void nd_devs_init() {
    nd_mc.csr0          = 0;
    nd_mc.csr1          = 0;
    nd_mc.csr2          = 0;
    nd_mc.sid           = ND_SLOT;
    nd_mc.dma_csr       = 0x80000000;
    nd_mc.dma_start     = 0;
    nd_mc.dma_width     = 0;
    nd_mc.dma_pstart    = 0;
    nd_mc.dma_pwidth    = 0;
    nd_mc.dma_sstart    = 0;
    nd_mc.dma_swidth    = 0;
    nd_mc.dma_bsstart   = 0;
    nd_mc.dma_bswidth   = 0;
    nd_mc.dma_top       = 0;
    nd_mc.dma_bottom    = 0;
    nd_mc.dma_line_a    = 0;
    nd_mc.dma_curr_a    = 0;
    nd_mc.dma_scurr_a   = 0;
    nd_mc.dma_out_a     = 0;
    nd_mc.vram          = 0;
    nd_mc.dram          = 0;
    
    nd_vbl_cyc_count    = VBL_CYC;
    nd_video_cyc_count  = VIDEO_VBL_CYC;
    
    nd_dp.iic_msgsz     = 0;
    nd_dp.iic_addr      = 0;
    nd_dp.csr           = 0;
    nd_dp.alpha         = 0;
    nd_dp.dma           = 0;
    nd_dp.x             = 0;
    nd_dp.y             = 0;
    nd_dp.iic_stat_addr = 0;
    nd_dp.iic_data      = 0;
}

const char* MC_RD_FORMAT = "[ND] Memory controller %s read %08X at %08X";

uae_u32 nd_mc_read_register(uaecptr addr) {
	switch (addr&0x3FFF) {
		case 0x0000:
			Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"csr0", nd_mc.csr0,addr);
			return nd_mc.csr0;
		case 0x0010:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"csr1", nd_mc.csr1,addr);
			return nd_mc.csr1;
		case 0x0020:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"csr2", nd_mc.csr2,addr);
			return nd_mc.csr2;
		case 0x0030:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"sid", nd_mc.sid,addr);
			return nd_mc.sid;
		case 0x1000:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_csr", nd_mc.dma_csr,addr);
            return nd_mc.dma_csr;
        case 0x1010:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_start", nd_mc.dma_start,addr);
            return nd_mc.dma_start;
        case 0x1020:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_width", nd_mc.dma_width,addr);
            return nd_mc.dma_width;
        case 0x1030:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_pstart", nd_mc.dma_pstart,addr);
            return nd_mc.dma_pstart;
        case 0x1040:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_pwidth", nd_mc.dma_pwidth,addr);
            return nd_mc.dma_pwidth;
        case 0x1050:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_sstart", nd_mc.dma_sstart,addr);
            return nd_mc.dma_sstart;
        case 0x1060:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_swidth", nd_mc.dma_swidth,addr);
            return nd_mc.dma_swidth;
        case 0x1070:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_bsstart", nd_mc.dma_bsstart,addr);
            return nd_mc.dma_bsstart;
        case 0x1080:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_bswidth", nd_mc.dma_bswidth,addr);
            return nd_mc.dma_bswidth;
        case 0x1090:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_top", nd_mc.dma_top,addr);
            return nd_mc.dma_top;
        case 0x10A0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_bottom", nd_mc.dma_bottom,addr);
            return nd_mc.dma_bottom;
        case 0x10B0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_line_a", nd_mc.dma_line_a,addr);
            return nd_mc.dma_line_a;
        case 0x10C0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_curr_a", nd_mc.dma_curr_a,addr);
            return nd_mc.dma_curr_a;
        case 0x10D0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_line_a", nd_mc.dma_line_a,addr);
            return nd_mc.dma_line_a;
        case 0x10E0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_scurr_a", nd_mc.dma_scurr_a,addr);
            return nd_mc.dma_scurr_a;
        case 0x10F0:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dma_out_a", nd_mc.dma_out_a,addr);
            return nd_mc.dma_out_a;
		case 0x2000:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"vram", nd_mc.vram,addr);
            return nd_mc.vram;
		case 0x3000:
            Log_Printf(ND_LOG_IO_RD, MC_RD_FORMAT,"dram", nd_mc.dram,addr);
            return nd_mc.dram;
		default:
			Log_Printf(LOG_WARN, "[ND] Memory controller UNKNOWN read at %08X",addr);
            break;
	}
	return 0;
}

const char* ND_CSR0_BITS[] = {
    "i860PIN_RESET",   "i860PIN_CS8",     "i860_IMASK",  "i860_INT",
    "BE_IMASK",        "BE_INT",          "VBL_IMASK",   "VBL_INT",
    "VBLANK",          "VIOVBL_IMASK",    "VIOVBL_INT",  "VIOBLANK",
    "i860_CACHE_EN",   "00002000",        "00004000",    "00008000",
    "00010000",        "00020000",        "00040000",    "00080000",
    "00100000",        "00200000",        "00400000",    "00800000",
    "01000000",        "02000000",        "04000000",    "08000000",
    "10000000",        "20000000",        "40000000",    "80000000",
};

const char* ND_CSR1_BITS[] = {
    "CPU_INT",         "00000002",        "00000004",    "00000008",
    "00000010",        "00000020",        "00000040",    "00000080",
    "00000100",        "00000200",        "00000400",    "00000800",
    "00001000",        "00002000",        "00004000",    "00008000",
    "00010000",        "00020000",        "00040000",    "00080000",
    "00100000",        "00200000",        "00400000",    "00800000",
    "01000000",        "02000000",        "04000000",    "08000000",
    "10000000",        "20000000",        "40000000",    "80000000",
};

const char* ND_CSR2_BITS[] = {
    "GLOBAL_ACCESS",   "00000002",        "00000004",    "00000008",
    "00000010",        "00000020",        "00000040",    "00000080",
    "00000100",        "00000200",        "00000400",    "00000800",
    "00001000",        "00002000",        "00004000",    "00008000",
    "00010000",        "00020000",        "00040000",    "00080000",
    "00100000",        "00200000",        "00400000",    "00800000",
    "01000000",        "02000000",        "04000000",    "08000000",
    "10000000",        "20000000",        "40000000",    "80000000",
};

const char* decodeBits(const char** strs, uae_u32 val) {
    static char buffer[512];
    char* result = buffer;
    *result = 0;
    for(int i = 0; i < 32; i++) {
        if(val & (1 << i)) {
            const char* str = strs[i];
            while(*str) *result++ = *str++;
            *result++ = '|';
        }
    }
    if(result != buffer)
        *--result = 0;
    return buffer;
}

const char* MC_WR_FORMAT   = "[ND] Memory controller %s write %08X at %08X";
const char* MC_WR_FORMAT_S = "[ND] Memory controller %s write (%s) at %08X";

void nd_mc_write_register(uaecptr addr, uae_u32 val) {
    switch (addr&0x3FFF) {
        case 0x0000:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT_S,"csr0", decodeBits(ND_CSR0_BITS, val), addr);
            nd_mc.csr0 = val;
            break;
        case 0x0010:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT_S,"csr1", decodeBits(ND_CSR1_BITS, val),addr);
            uae_u32 change = nd_mc.csr1 ^ val;
            nd_mc.csr1 = val;
            if((change & CSR1_CPU_INT) && (nd_mc.csr1 & CSR1_CPU_INT))
                nd_nbic_interrupt(true);
            else
                nd_nbic_interrupt(false);
            break;
        case 0x0020:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT_S,"csr2", decodeBits(ND_CSR2_BITS, val),addr);
            nd_mc.csr2 = val;
            break;
        case 0x0030:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"sid", val,addr);
            nd_mc.sid = val;
            break;
        case 0x1000:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_csr", val,addr);
            nd_mc.dma_csr = val;
            break;
        case 0x1010:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_start", val,addr);
            nd_mc.dma_start = val;
            break;
        case 0x1020:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_width", val,addr);
            nd_mc.dma_width = val;
            break;
        case 0x1030:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_pstart", val,addr);
            nd_mc.dma_pstart = val;
            break;
        case 0x1040:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_pwidth", val,addr);
            nd_mc.dma_pwidth = val;
            break;
        case 0x1050:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_sstart", val,addr);
            nd_mc.dma_sstart = val;
            break;
        case 0x1060:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_swidth", val,addr);
            nd_mc.dma_swidth = val;
            break;
        case 0x1070:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_bsstart", val,addr);
            nd_mc.dma_bsstart = val;
            break;
        case 0x1080:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_bswidth", val,addr);
            nd_mc.dma_bswidth = val;
            break;
        case 0x1090:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_top", val,addr);
            nd_mc.dma_top = val;
            break;
        case 0x10A0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_bottom", val,addr);
            nd_mc.dma_bottom = val;
            break;
        case 0x10B0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_line_a", val,addr);
            nd_mc.dma_line_a = val;
            break;
        case 0x10C0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_curr_a", val,addr);
            nd_mc.dma_curr_a = val;
            break;
        case 0x10D0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_line_a", val,addr);
            nd_mc.dma_line_a = val;
            break;
        case 0x10E0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_scurr_a", val,addr);
            nd_mc.dma_scurr_a = val;
            break;
        case 0x10F0:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dma_out_a", val,addr);
            nd_mc.dma_out_a = val;
            break;
        case 0x2000:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"vram", val,addr);
            nd_mc.vram = val;
            break;
        case 0x3000:
            Log_Printf(ND_LOG_IO_WR, MC_WR_FORMAT,"dram", val,addr);
            nd_mc.dram = val;
            break;
        default:
            Log_Printf(LOG_WARN, "[ND] Memory controller UNKNOWN write at %08X",addr);
            break;
	}
}

/* interrupt processing */
int nd_process_interrupts(int nHostCycles) {
    nd_vbl_cyc_count   -= nHostCycles;
    nd_video_cyc_count -= nHostCycles;
    
    int result = 0;

    if(nd_vbl_cyc_count >= V_FRONT && nd_vbl_cyc_count < V_BACK)
        nd_mc.csr0 &= ~CSR0_VBLANK;
    else
        nd_mc.csr0 |= CSR0_VBLANK;

    if(nd_vbl_cyc_count <= 0) {
        nd_mc.csr0 |= CSR0_VBL_INT;
        if(nd_mc.csr0 & CSR0_VBL_IMASK)
            result = 1;
        nd_vbl_cyc_count = VBL_CYC;
    }

    if(nd_video_cyc_count >= VIDEO_V_FRONT && nd_video_cyc_count < VIDEO_V_BACK)
        nd_mc.csr0 &= ~CSR0_VIOBLANK;
    else
        nd_mc.csr0 |= CSR0_VIOBLANK;

    
    if(nd_video_cyc_count <= 0) {
        nd_mc.csr0 |= CSR0_VIOVBL_INT;
        if(nd_mc.csr0 & CSR0_VIOVBL_IMASK)
            result = 1;
        nd_video_cyc_count = VIDEO_VBL_CYC;
    }
    
    if (nd_mc.csr0 & CSR0_i860_INT) {
        if(nd_mc.csr0 & CSR0_i860_IMASK)
            result = 1;
    }
    
    if (nd_mc.csr0 & CSR0_BE_INT) {
        if(nd_mc.csr0 & CSR0_BE_IMASK)
            result = 1;
    }
    
    if(nd_mc.csr0 & CSR0_i860PIN_RESET) {
        nd_i860_init();
        nd_mc.csr0 &= ~CSR0_i860PIN_RESET;
        result = 0;
    }

    return result;
}

/* NeXTdimension device space */
inline uae_u32 nd_io_lget(uaecptr addr) {
	return nd_mc_read_register(addr);
}

inline uae_u32 nd_io_wget(uaecptr addr) {
	return 0;
}

inline uae_u32 nd_io_bget(uaecptr addr) {
	return 0;
}

inline void nd_io_lput(uaecptr addr, uae_u32 l) {
	nd_mc_write_register(addr, l);
}

inline void nd_io_wput(uaecptr addr, uae_u32 w) {
    
}

inline void nd_io_bput(uaecptr addr, uae_u32 b) {
    
}

struct {
    int   addr;
    int   idx;
    Uint8 regs[0x1000];
} nd_ramdac;

static void nd_ramdac_autoinc() {
    nd_ramdac.idx++;
    if(nd_ramdac.idx == 3) {
        nd_ramdac.idx = 0;
        nd_ramdac.addr++;
    }
}

/* NeXTdimension RAMDAC */
inline uae_u32 nd_ramdac_bget(uaecptr addr) {
    uae_u32 result = 0;
    switch(addr & 0xF) {
        case 0:
            return nd_ramdac.addr & 0xFF;
        case 0x4:
            return (nd_ramdac.addr >> 8) & 0xFF;
        case 0x8:
            result = nd_ramdac.regs[nd_ramdac.addr*3];
            if(nd_ramdac.addr == 0x100 || nd_ramdac.addr == 0x101)
                nd_ramdac_autoinc();
            break;
        case 0xC:
            result = nd_ramdac.regs[nd_ramdac.addr*3+nd_ramdac.idx];
            nd_ramdac_autoinc();
            break;
    }
    return result;
}

inline void nd_ramdac_bput(uaecptr addr, uae_u32 b) {
    switch(addr & 0xF) {
        case 0x0:
            nd_ramdac.addr &= 0xFF00;
            nd_ramdac.addr |= b & 0xFF;
            nd_ramdac.idx = 0;
            break;
        case 0x4:
            nd_ramdac.addr &= 0x000F;
            nd_ramdac.addr |= (b & 0x0F) << 8;
            nd_ramdac.idx = 0;
            break;
        case 0x8:
            nd_ramdac.regs[nd_ramdac.addr*3] = b;
            if(nd_ramdac.addr == 0x100 || nd_ramdac.addr == 0x101)
                nd_ramdac_autoinc();
            break;
        case 0xC:
            nd_ramdac.regs[nd_ramdac.addr*3+nd_ramdac.idx] = b;
            nd_ramdac_autoinc();
            break;
    }
}

void nd_dp_iicmsg() {
    Log_Printf(LOG_WARN, "[ND] data path IIC msg addr:%02X msg[%d]=%02X", nd_dp.iic_addr, nd_dp.iic_msgsz-1, nd_dp.iic_msg[nd_dp.iic_msgsz-1]);
}

/* NeXTdimension data path */
inline uae_u32 nd_dp_lget(uaecptr addr) {
    switch(addr) {
        case 0x340:
            return nd_dp.csr;
        case 0x344:
            return nd_dp.alpha;
        case 0x348:
            return nd_dp.dma;
        case 0x350:
            return nd_dp.x;
        case 0x354:
            return nd_dp.y;
        case 0x360:
            if(nd_dp.iic_busy <= 0)
                nd_dp.iic_stat_addr &= ~DP_IIC_BUSY;
            else
                nd_dp.iic_busy--;
            return nd_dp.iic_stat_addr;
        case 0x364:
            return 0;
        default:
            Log_Printf(LOG_WARN, "[ND] data path UNKNOWN read at %08X",addr);
    }
    return 0;
}

inline void nd_dp_lput(uaecptr addr, uae_u32 v) {
    switch(addr) {
        case 0x340:
            nd_dp.csr = v;
            break;
        case 0x344:
            nd_dp.alpha = v;
            break;
        case 0x348:
            nd_dp.dma = v;
            break;
        case 0x350:
            nd_dp.x = v;
            break;
        case 0x354:
            nd_dp.y = v;
            break;
        case 0x360:
            nd_dp.iic_msgsz = 0;
            nd_dp.iic_addr  = v >> 8;
            nd_dp.iic_msg[nd_dp.iic_msgsz++] = v;
            nd_dp.iic_stat_addr |= DP_IIC_BUSY;
            nd_dp.iic_busy       = 10;
            nd_dp_iicmsg();
            break;
        case 0x364:
            nd_dp.iic_msg[nd_dp.iic_msgsz++] = v;
            nd_dp.iic_stat_addr |= DP_IIC_BUSY;
            nd_dp.iic_busy       = 10;
            nd_dp_iicmsg();
            break;
        default:
            Log_Printf(LOG_WARN, "[ND] data path UNKNOWN write at %08X %08X",addr,v);
    }
}

#endif
