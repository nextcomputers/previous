/*  Previous - printer.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 NeXT Laser Printer emulation.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "printer.h"
#include "sysReg.h"
#include "dma.h"
#include "statusbar.h"
#include "file.h"

#include "png.h"

#define USE_PNG_PRINTING 1


#define IO_SEG_MASK 0x1FFFF

#define LOG_LP_REG_LEVEL    LOG_DEBUG
#define LOG_LP_LEVEL        LOG_WARN


struct {
    /* Registers */
    struct {
        Uint8 dma;
        Uint8 printer;
        Uint8 interface;
        Uint8 cmd;
    } csr;
    Uint32 data;
    
    /* Internal */
    Uint8 stat;
    Uint8 statmask;
    Uint32 margins;
} nlp;

void lp_power_on(void);
Uint32 lp_data_read(void);
void lp_boot_message(void);
void lp_interface_command(Uint8 cmd, Uint32 data);
void lp_gpo_access(Uint8 data);

void lp_png_setup(Uint32 data);
void lp_png_print(void);
void lp_png_finish(void);

bool lp_data_transfer = false;

/* Laser Printer control and status register (0x0200F000)
 *
 * x--- ---- ---- ---- ---- ---- ---- ----  dma out enable (r/w)
 * -x-- ---- ---- ---- ---- ---- ---- ----  dma out request (r)
 * --x- ---- ---- ---- ---- ---- ---- ----  dma out underrun detected (r/w)
 * ---- x--- ---- ---- ---- ---- ---- ----  dma in enable (r/w)
 * ---- -x-- ---- ---- ---- ---- ---- ----  dma in request (r)
 * ---- --x- ---- ---- ---- ---- ---- ----  dma in overrun detected (r/w)
 * ---- ---x ---- ---- ---- ---- ---- ----  increment buffer read pointer (r/w)
 *
 * ---- ---- x--- ---- ---- ---- ---- ----  printer on/off (r/w)
 * ---- ---- -x-- ---- ---- ---- ---- ----  behave like monitor interface (r/w)
 * ---- ---- ---- x--- ---- ---- ---- ----  printer interrupt (r)
 * ---- ---- ---- -x-- ---- ---- ---- ----  printer data available (r)
 * ---- ---- ---- --x- ---- ---- ---- ----  printer data overrun (r/w)
 *
 * ---- ---- ---- ---- x--- ---- ---- ----  dma out transmit pending (r)
 * ---- ---- ---- ---- -x-- ---- ---- ----  dma out transmit in progress (r)
 * ---- ---- ---- ---- --x- ---- ---- ----  cpu data transmit pending (r)
 * ---- ---- ---- ---- ---x ---- ---- ----  cpu data transmit in progress (r)
 * ---- ---- ---- ---- ---- --x- ---- ----  printer interface enable (return from reset state) (r/w)
 * ---- ---- ---- ---- ---- ---x ---- ----  loop back transmitter data (r/w)
 *
 * ---- ---- ---- ---- ---- ---- xxxx xxxx  command to append on printer data (r/w)
 *
 * ---x ---- --xx ---x ---- xx-- ---- ----  zero bits
 */

#define LP_DMA_OUT_EN   0x80
#define LP_DMA_OUT_REQ  0x40
#define LP_DMA_OUT_UNDR 0x20
#define LP_DMA_IN_EN    0x08
#define LP_DMA_IN_REQ   0x04
#define LP_DMA_IN_OVR   0x02
#define LP_DMA_INCR     0x01

#define LP_ON           0x80
#define LP_NDI          0x40
#define LP_INT          0x08
#define LP_DATA         0x04
#define LP_DATA_OVR     0x02

#define LP_TX_DMA_PEND  0x80
#define LP_TX_DMA       0x40
#define LP_TX_CPU_PEND  0x20
#define LP_TX_CPU       0x10
#define LP_TX_EN        0x02
#define LP_TX_LOOP      0x01


void LP_CSR0_Read(void) { // 0x0200F000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = nlp.csr.dma;
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void LP_CSR0_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (val&LP_DMA_OUT_UNDR) {
        nlp.csr.dma &= ~(LP_DMA_OUT_UNDR|LP_DMA_OUT_REQ);
        nlp.csr.printer &= ~LP_INT;
        set_interrupt(INT_PRINTER, RELEASE_INT);
    }
    if (val&LP_DMA_IN_OVR) {
        nlp.csr.dma &= ~(LP_DMA_IN_OVR|LP_DMA_IN_REQ);
        nlp.csr.printer &= ~LP_INT;
        set_interrupt(INT_PRINTER, RELEASE_INT);
    }
}

void LP_CSR1_Read(void) { // 0x0200F001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = nlp.csr.printer;
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void LP_CSR1_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((val&LP_ON) != (nlp.csr.printer&LP_ON)) {
        if (val&LP_ON) {
            Statusbar_AddMessage("Switching Laser Printer ON.", 0);
            lp_power_on();
        } else {
            Statusbar_AddMessage("Switching Laser Printer OFF.", 0);
        }
    }
    nlp.csr.printer = val;
}

void LP_CSR2_Read(void) { // 0x0200F002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = nlp.csr.interface;
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void LP_CSR2_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((val&LP_TX_EN) != (nlp.csr.interface&LP_TX_EN)) {
        if (val&LP_TX_EN) {
            Log_Printf(LOG_LP_LEVEL,"[LP] Enable serial interface.");
            lp_boot_message();
        } else {
            Log_Printf(LOG_LP_LEVEL,"[LP] Disable serial interface.");
        }
    }
    nlp.csr.interface = val;
}

void LP_CSR3_Read(void) { // 0x0200F003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = nlp.csr.cmd;
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void LP_CSR3_Write(void) {
    nlp.csr.cmd=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] CSR3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void LP_Data_Read(void) { // 0x0200F004 (access must be 32-bit)
    nlp.data = lp_data_read();
    IoMem_WriteLong(IoAccessCurrentAddress&IO_SEG_MASK, nlp.data);
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] Data read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, nlp.data, m68k_getpc());
}

void LP_Data_Write(void) {
    nlp.data = IoMem_ReadLong(IoAccessCurrentAddress&IO_SEG_MASK);
    Log_Printf(LOG_LP_REG_LEVEL,"[LP] Data write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, nlp.data, m68k_getpc());

    lp_interface_command(nlp.csr.cmd, nlp.data);
}


/* Commands from CPU to Printer */
#define LP_CMD_RESET    0xff
#define LP_CMD_DATA_OUT 0xc7
#define LP_CMD_GPO      0xc4
#define LP_CMD_GPI_MASK 0xc5
#define LP_CMD_GPI_REQ  0x04
#define LP_CMD_MARGINS  0xc2
#define LP_CMD_COPY     0xe7

#define LP_CMD_MASK     0xc7
#define LP_CMD_NODATA   0x07
#define LP_CMD_DATA_EN  0x08
#define LP_CMD_300DPI   0x10
#define LP_CMD_NORMAL   0x20

/* Responses from Printer to CPU */
#define LP_RES_DATA_IN  0xc7
#define LP_RES_GPI      0xc4
#define LP_RES_COPY     0xe7
#define LP_RES_DATA_REQ 0x07
#define LP_RES_UNDERRUN 0x0f


/* Data for commands */
#define LP_GPI_PWR_RDY  0x10
#define LP_GPI_RDY      0x08
#define LP_GPI_VSREQ    0x04
#define LP_GPI_BUSY     0x02
#define LP_GPI_STAT_BIT 0x01

#define LP_GPO_DENSITY  0x40
#define LP_GPO_VSYNC    0x20
#define LP_GPO_ENABLE   0x10
#define LP_GPO_PWR_RDY  0x08
#define LP_GPO_CLOCK    0x04
#define LP_GPO_CMD_BIT  0x02
#define LP_GPO_BUSY     0x01

void lp_gpo(Uint8 cmd) {
    if (cmd&LP_GPO_DENSITY) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer 300 DPI mode");
    }
    if (cmd&LP_GPO_VSYNC) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer VSYNC enable");
    }
    if (cmd&LP_GPO_ENABLE) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer enable");
        nlp.stat |= LP_GPI_VSREQ;
    }
    if (cmd&LP_GPO_PWR_RDY) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer controller power ready");
    }
    if (cmd&LP_GPO_CLOCK) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer serial data clock");
    }
    if (cmd&LP_GPO_CMD_BIT) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer serial data command bits");
    }
    if (cmd&LP_GPO_BUSY) {
        Log_Printf(LOG_LP_LEVEL,"[LP] Printer controller busy");
    }
    lp_gpo_access(cmd);
}

void lp_interface_status(void) {
    Log_Printf(LOG_LP_LEVEL,"[LP] Interface status: %02X (mask: %02X)",nlp.stat,nlp.statmask);

    nlp.data = ((~(nlp.stat/*&nlp.statmask*/))&0xFF)<<24;

    nlp.csr.cmd = LP_RES_GPI;
    
    nlp.csr.printer |= (LP_INT|LP_DATA);
    set_interrupt(INT_PRINTER, SET_INT);
}

void lp_interface_command(Uint8 cmd, Uint32 data) {
    switch (cmd) {
        case LP_CMD_RESET:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: Reset (%08X)",data);
            break;
        case LP_CMD_DATA_OUT:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: Data out (%08X)",data);
            break;
        case LP_CMD_GPO:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: General purpose out (%02X)",(~data)>>24);
            lp_gpo((~data)>>24);
            break;
        case LP_CMD_GPI_MASK:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: General purpose input mask (%02X)",(~data)>>24);
            nlp.statmask = (~data)>>24;
            break;
        case LP_CMD_GPI_REQ:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: General purpose input request");
            lp_interface_status();
            break;
        case LP_CMD_MARGINS:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: Margins (%08X)",data);
            nlp.margins = data;
            break;
        case LP_CMD_COPY:
            Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: Copyright (%08X)",data);
            break;
            
        default: /* Commands with no data */
            if ((cmd&LP_CMD_MASK)==LP_CMD_NODATA) {
                Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: No data command:");

                if (cmd&LP_CMD_DATA_EN) {
                    Log_Printf(LOG_LP_LEVEL,"[LP] Enable printer data transfer");
                    /* Setup printing buffer */
                    lp_png_setup(nlp.margins);
                    lp_data_transfer = true;
                    CycInt_AddRelativeInterrupt(1000, INT_CPU_CYCLE, INTERRUPT_LP_IO);
                } else {
                    Log_Printf(LOG_LP_LEVEL,"[LP] Disable printer data transfer");
                    if (lp_data_transfer) {
                        /* Save buffered printing data to image file */
                        lp_png_finish();
                    }
                    lp_data_transfer = false;
                }
                if (cmd&LP_CMD_300DPI) {
                    Log_Printf(LOG_LP_LEVEL,"[LP] 300 DPI mode");
                } else {
                    Log_Printf(LOG_LP_LEVEL,"[LP] 400 DPI mode");
                }
                if (cmd&LP_CMD_NORMAL) {
                    Log_Printf(LOG_LP_LEVEL,"[LP] Normal requests for data transfer");
                } else {
                    Log_Printf(LOG_LP_LEVEL,"[LP] Early requests for data transfer");
                }
            } else {
                Log_Printf(LOG_LP_LEVEL,"[LP] Interface command: Unknown command!");
            }
            break;
    }
}

void lp_power_on(void) {
    nlp.stat |= (LP_GPI_PWR_RDY|LP_GPI_RDY);
}

/* COPY. NeXT 1987 */
Uint32 lp_copyright_message[4] = { 0x00434f50, 0x522e204e, 0x65585420, 0x31393837 };
int lp_copyright_sequence = 0;

void lp_boot_message(void) {
    lp_copyright_sequence = 4;
    
    nlp.csr.cmd = LP_RES_COPY;
    
    nlp.csr.printer |= LP_DATA;
}

Uint32 lp_data_read(void) {
    Uint32 val = 0;
    
    if (lp_copyright_sequence>0) {
        val = lp_copyright_message[4-lp_copyright_sequence];
        lp_copyright_sequence--;
    } else {
        val = nlp.data;
    }
    
    if (lp_copyright_sequence==0) {
        nlp.csr.printer &= ~(LP_INT|LP_DATA);
        set_interrupt(INT_PRINTER, RELEASE_INT);
    }

    return val;
}


/* Printer internal command and status via serial interface */
#define CMD_STATUS0     0x01
#define CMD_STATUS1     0x02
#define CMD_STATUS2     0x04
#define CMD_STATUS4     0x08
#define CMD_STATUS5     0x0b
#define CMD_STATUS15    0x1f

#define CMD_EXT_CLK     0x40
#define CMD_PRINTER_CLK 0x43
#define CMD_PAUSE       0x45
#define CMD_UNPAUSE     0x46
#define CMD_DRUM_ON     0x49
#define CMD_DRUM_OFF    0x4a
#define CMD_CASSFEED    0x4c
#define CMD_HANDFEED    0x4f
#define CMD_RETRANSCANC 0x5d

#define STAT0_PRINTREQ  0x40
#define STAT0_PAPERDLVR 0x20
#define STAT0_DATARETR  0x10
#define STAT0_WAIT      0x08
#define STAT0_PAUSE     0x04
#define STAT0_CALL      0x02

#define STAT1_NOCART    0x40
#define STAT1_NOPAPER   0x10
#define STAT1_JAM       0x08
#define STAT1_DOOROPEN  0x04
#define STAT1_TESTPRINT 0x02

#define STAT2_FIXINGASM 0x40
#define STAT2_POORBDSIG 0x20
#define STAT2_SCANMOTOR 0x10

#define STAT5_NOCASS    0x01
#define STAT5_A4        0x02
#define STAT5_LETTER    0x08
#define STAT5_B5        0x13
#define STAT5_LEGAL     0x19

#define STAT15_NOTONER  0x04

Uint8 lp_serial_status[16] = {
    0,0,0,0,
    0,STAT5_A4,0,0,
    0,0,0,0,
    0,0,0,0
};

Uint8 lp_serial_phase = 0;

Uint8 lp_printer_status(Uint8 num) {
    int i;
    Uint8 val;
    
    lp_serial_phase = 8;
    nlp.stat |= LP_GPI_BUSY;
    nlp.csr.cmd = LP_RES_GPI;
    
    if (num<16) {
        val = lp_serial_status[num];
    } else {
        val = 0x00;
    }
    
    /* odd parity */
    val |= 1;
    for (i = 1; i < 8; i++) {
        if (val & (1 << i)) {
            val ^= 1;
        }
    }
    return val;
}

Uint8 lp_printer_command(Uint8 cmd) {
    switch (cmd) {
        case CMD_STATUS0:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 0");
            return lp_printer_status(0);
        case CMD_STATUS1:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 1");
            return lp_printer_status(1);
        case CMD_STATUS2:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 2");
            return lp_printer_status(2);
        case CMD_STATUS4:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 4");
            return lp_printer_status(4);
        case CMD_STATUS5:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 5");
            return lp_printer_status(5);
        case CMD_STATUS15:
            Log_Printf(LOG_LP_LEVEL, "[LP] Read status register 15");
            return lp_printer_status(15);
        /* Commands with no status */
        case CMD_EXT_CLK:
            Log_Printf(LOG_LP_LEVEL, "[LP] External clock");
            return lp_printer_status(16);
        case CMD_PRINTER_CLK:
            Log_Printf(LOG_LP_LEVEL, "[LP] Printer clock");
            return lp_printer_status(16);
        case CMD_PAUSE:
            Log_Printf(LOG_LP_LEVEL, "[LP] Pause");
            return lp_printer_status(16);
        case CMD_UNPAUSE:
            Log_Printf(LOG_LP_LEVEL, "[LP] Unpause");
            return lp_printer_status(16);
        case CMD_DRUM_ON:
            Log_Printf(LOG_LP_LEVEL, "[LP] Drum on");
            return lp_printer_status(16);
        case CMD_DRUM_OFF:
            Log_Printf(LOG_LP_LEVEL, "[LP] Drum off");
            return lp_printer_status(16);
        case CMD_CASSFEED:
            Log_Printf(LOG_LP_LEVEL, "[LP] Cassette feed");
            return lp_printer_status(16);
        case CMD_HANDFEED:
            Log_Printf(LOG_LP_LEVEL, "[LP] Hand feed");
            return lp_printer_status(16);
        case CMD_RETRANSCANC:
            Log_Printf(LOG_LP_LEVEL, "[LP] Cancel retransmission");
            return lp_printer_status(16);

        default:
            Log_Printf(LOG_LP_LEVEL, "[LP] Unknown command!");
            return lp_printer_status(16);
    }
}

void lp_gpo_access(Uint8 data) {
    static Uint8 lp_cmd = 0;
    static Uint8 lp_stat = 0;
    static Uint8 lp_old_data = 0;

    Log_Printf(LOG_LP_LEVEL, "[LP] Control logic access: %02X",data);
    
    if ((lp_old_data&LP_GPO_BUSY)!=(data&LP_GPO_BUSY)) {
        if (!(data&LP_GPO_BUSY)) {
            Log_Printf(LOG_LP_LEVEL, "[LP] Printer command: %02X",lp_cmd);
            lp_stat = lp_printer_command(lp_cmd);
        } else {
            lp_cmd = 0;
        }
    }
    if (data&LP_GPO_BUSY) {
        if ((data&LP_GPO_CLOCK) && !(lp_old_data&LP_GPO_CLOCK)) {
            lp_cmd <<= 1;
            lp_cmd |= (data&LP_GPO_CMD_BIT)?1:0;
        }
    } else if (nlp.stat&LP_GPI_BUSY) {
        if ((data&LP_GPO_CLOCK) && !(lp_old_data&LP_GPO_CLOCK)) {
            lp_serial_phase--;
            if (lp_serial_phase==0) {
                nlp.stat &= ~LP_GPI_BUSY;
                Log_Printf(LOG_LP_LEVEL, "[LP] Printer status: %02X",lp_stat);
            }
            nlp.stat &= ~LP_GPI_STAT_BIT;
            nlp.stat |= (lp_stat&(1<<lp_serial_phase))?LP_GPI_STAT_BIT:0;
            
            nlp.csr.printer |= (LP_INT|LP_DATA); /* CHECK: true? */
        }
    }
    
    lp_old_data = data;
}

/* Printer DMA and printing function */
void Printer_IO_Handler(void) {
    int i;
    
    CycInt_AcknowledgeInterrupt();
    
    if (lp_data_transfer) {
        lp_buffer.limit = 4096;
        lp_buffer.size = 0;
        dma_printer_read_memory();
        
        if (lp_buffer.size==0) {
            Log_Printf(LOG_LP_LEVEL,"[LP] Printing done.");
            nlp.csr.dma |= LP_DMA_OUT_UNDR;
            set_interrupt(INT_PRINTER, SET_INT);
            return;
        }
        /* Save data to printing buffer */
        lp_png_print();
        
        for (i = 0; i < lp_buffer.size; i++) {
            printf("%02X",lp_buffer.data[i]);
        }
        printf("\n");
        
        CycInt_AddRelativeInterrupt(200000, INT_CPU_CYCLE, INTERRUPT_LP_IO);
    }
}


/* PNG printing functions */
#if USE_PNG_PRINTING
const int MAX_PAGE_LEN = 400 * 14; // 14 inches is the length of US legal paper, longest paper that fits into the NeXT printer cartridge
png_structp png_ptr          = NULL;
png_infop   png_info_ptr     = NULL;
png_byte**  png_row_pointers = NULL;
int         png_width;
int         png_height;
int         png_count;
int         png_page_count   = 0;
char        png_path[PATH_MAX+1];
#endif

void lp_png_setup(Uint32 data) {
#if USE_PNG_PRINTING
    int i;
    png_width = ((data >> 16) & 0x7F) * 32;
    
    if (png_ptr) {
        for (i = 0; i < MAX_PAGE_LEN; i++) {
            png_free(png_ptr, png_row_pointers[i]);
        }
        png_free(png_ptr, png_row_pointers);
        png_destroy_write_struct(&png_ptr, &png_info_ptr);
    }
    
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        return;
    }
    png_info_ptr = png_create_info_struct(png_ptr);
    if (png_info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, &png_info_ptr);
        png_ptr = NULL;
        return;
    }
    
    png_row_pointers = png_malloc(png_ptr, MAX_PAGE_LEN * sizeof (png_byte *));
    for (i = 0; i < MAX_PAGE_LEN; i++) {
        png_row_pointers[i] = png_malloc(png_ptr, sizeof (uint8_t) * (png_width / 8));
    }
    png_count  = 0;
    png_height = 0;
#endif
}

void lp_png_print(void) {
#if USE_PNG_PRINTING
    int i;
    
    for (i = 0; i < lp_buffer.size; i++) {
        png_row_pointers[png_count/png_width][(png_count%png_width)/8] = ~lp_buffer.data[i];
        png_count += 8;
    }
#endif
}

void lp_png_finish(void) {
#if USE_PNG_PRINTING
    png_set_IHDR(png_ptr,
                 png_info_ptr,
                 png_width,
                 png_count / png_width,
                 1,
                 PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    
    sprintf(png_path, "%05d_next_printer.png", png_page_count);
    FILE* png_fp = File_Open(png_path, "wb");
    
    png_init_io(png_ptr, png_fp);
    png_set_rows(png_ptr, png_info_ptr, png_row_pointers);
    png_write_png(png_ptr, png_info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    
    File_Close(png_fp);
    png_page_count++;
#endif
}
