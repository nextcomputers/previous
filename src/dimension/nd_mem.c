#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "dimension.h"
#include "nd_mem.h"
#include "nd_devs.h"
#include "nd_rom.h"

#if ENABLE_DIMENSION

#define write_log printf

/* --------- NEXTDIMENSION MEMORY ---------- */

/* NeXTdimension board memory */
#define ND_RAM_START	0xF8000000
#define ND_RAM_SIZE		0x04000000
#define ND_RAM_MASK		0x03FFFFFF

#define ND_VRAM_START	0xFE000000
#define ND_VRAM_SIZE	0x00400000
#define ND_VRAM_MASK	0x003FFFFF

#define ND_EEPROM_START	0xFFFE0000
#define ND_EEPROM_SIZE	0x00020000
#define ND_EEPROM_MASK	0x0001FFFF

Uint8 ND_ram[64*1024*1024];
Uint8 ND_vram[4*1024*1024];
Uint8 ND_rom[128*1024];


/* NeXTdimension board devices */
#define ND_IO_START		0xFF800000
#define ND_IO_SIZE		0x00004000
#define ND_IO_MASK		0x00003FFF

/* NeXTdimension board RAMDAC */
#define ND_RAMDAC_START	0xFF200000
#define ND_RAMDAC_SIZE  0x00001000
#define ND_RAMDAC_MASK  0x00000FFF

/* NeXTdimension EEPROM access */
#define ND_EEPROM2_STRT 0xFFF00000
#define ND_EEPROM2_SIZE 0x00080000
#define ND_EEPROM2_MASK 0x0007FFFC


/* Memory banks */

nd_addrbank *nd_mem_banks[65536];

void nd_map_banks (nd_addrbank *bank, int start, int size)
{
	int bnr;
	
	for (bnr = start; bnr < start + size; bnr++)
		nd_put_mem_bank (bnr << 16, bank);
	return;
}

/* NeXTdimension RAM */
static uae_u32 nd_ram_lget(uaecptr addr)
{
	addr &= ND_RAM_MASK;
	return do_get_mem_long(ND_ram + addr);
}

static uae_u32 nd_ram_wget(uaecptr addr)
{
	addr &= ND_RAM_MASK;
	return do_get_mem_word(ND_ram + addr);
}

static uae_u32 nd_ram_bget(uaecptr addr)
{
	addr &= ND_RAM_MASK;
	return ND_ram[addr];
}

static void nd_ram_lput(uaecptr addr, uae_u32 l)
{
	addr &= ND_RAM_MASK;
	do_put_mem_long(ND_ram + addr, l);
}

static void nd_ram_wput(uaecptr addr, uae_u32 w)
{
	addr &= ND_RAM_MASK;
	do_put_mem_word(ND_ram + addr, w);
}

static void nd_ram_bput(uaecptr addr, uae_u32 b)
{
	addr &= ND_RAM_MASK;
	ND_ram[addr] = b;
}

/* NeXTdimension VRAM */
static uae_u32 nd_vram_lget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return do_get_mem_long(ND_vram + addr);
}

static uae_u32 nd_vram_wget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return do_get_mem_word(ND_vram + addr);
}

static uae_u32 nd_vram_bget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return ND_vram[addr];
}

static void nd_vram_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_VRAM_MASK;
    do_put_mem_long(ND_vram + addr, l);
}

static void nd_vram_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_VRAM_MASK;
    do_put_mem_word(ND_vram + addr, w);
}

static void nd_vram_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_VRAM_MASK;
    ND_vram[addr] = b;
}

/* NeXTdimension ROM */
static uae_u32 nd_rom_lget(uaecptr addr)
{
	addr &= ND_EEPROM_MASK;
	return do_get_mem_long(ND_rom + addr);
}

static uae_u32 nd_rom_wget(uaecptr addr)
{
	addr &= ND_EEPROM_MASK;
	return do_get_mem_word(ND_rom + addr);
}

static uae_u32 nd_rom_bget(uaecptr addr)
{
	addr &= ND_EEPROM_MASK;
    return ND_rom[addr];
}

static void nd_rom_lput(uaecptr addr, uae_u32 l)
{
	addr &= ND_EEPROM_MASK;
	do_put_mem_long(ND_rom + addr, l);
}

static void nd_rom_wput(uaecptr addr, uae_u32 w)
{
	addr &= ND_EEPROM_MASK;
	do_put_mem_word(ND_rom + addr, w);
}

static void nd_rom_bput(uaecptr addr, uae_u32 b)
{
	addr &= ND_EEPROM_MASK;
    ND_rom[addr] = b;
}

/* NeXTdimension ROM access */
static uae_u32 nd_rom_access_bget(uaecptr addr)
{
    addr &= ND_EEPROM2_MASK;
    addr |= (addr>>17)&3;
    addr &= ~(3<<17);
    return nd_rom_read(addr);
}

static void nd_rom_access_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_EEPROM2_MASK;
    addr |= (addr>>17)&3;
    addr &= ~(3<<17);
    nd_rom_write(addr, b);
}

/* NeXTdimension RAMDAC */
static uae_u32 nd_ramdac_lget(uaecptr addr)
{
    return 0;
}

static uae_u32 nd_ramdac_wget(uaecptr addr)
{
    return 0;
}

static uae_u32 nd_ramdac_bget(uaecptr addr)
{
    return 0;
}

static void nd_ramdac_lput(uaecptr addr, uae_u32 l)
{
}

static void nd_ramdac_wput(uaecptr addr, uae_u32 w)
{
}

static void nd_ramdac_bput(uaecptr addr, uae_u32 b)
{
}



static nd_addrbank nd_ram_bank =
{
	nd_ram_lget, nd_ram_wget, nd_ram_bget,
	nd_ram_lput, nd_ram_wput, nd_ram_bput,
	nd_ram_lget, nd_ram_wget
};

static nd_addrbank nd_vram_bank =
{
    nd_vram_lget, nd_vram_wget, nd_vram_bget,
    nd_vram_lput, nd_vram_wput, nd_vram_bput,
    nd_vram_lget, nd_vram_wget
};

static nd_addrbank nd_rom_bank =
{
	nd_rom_lget, nd_rom_wget, nd_rom_bget,
	nd_rom_lput, nd_rom_wput, nd_rom_bput,
	nd_rom_lget, nd_rom_wget
};

static nd_addrbank nd_rom_access_bank =
{
    NULL, NULL, nd_rom_access_bget,
    NULL, NULL, nd_rom_access_bput,
    NULL, NULL
};

static nd_addrbank nd_io_bank =
{
	nd_io_lget, nd_io_wget, nd_io_bget,
	nd_io_lput, nd_io_wput, nd_io_bput,
	nd_io_lget, nd_io_wget
};

static nd_addrbank nd_ramdac_bank =
{
    nd_ramdac_lget, nd_ramdac_wget, nd_ramdac_bget,
    nd_ramdac_lput, nd_ramdac_wput, nd_ramdac_bput,
    nd_ramdac_lget, nd_ramdac_wget
};


void nd_memory_init(void) {
	
	write_log("Mapping NeXTdimension Memory:\n");

	write_log("Mapping NeXTdimension Main Memory at $%08x: %iMB\n", ND_RAM_START, ND_RAM_SIZE/(1024*1024));
	nd_map_banks(&nd_ram_bank, ND_RAM_START>>16, ND_RAM_SIZE>>16);
    
    write_log("Mapping NeXTdimension Video Memory at $%08x: %iMB\n", ND_VRAM_START, ND_VRAM_SIZE/(1024*1024));
    nd_map_banks(&nd_vram_bank, ND_VRAM_START>>16, ND_VRAM_SIZE>>16);
	
	write_log("Mapping NeXTdimension ROM at $%08x: %ikB\n", ND_EEPROM_START, ND_EEPROM_SIZE/1024);
	nd_map_banks(&nd_rom_bank, ND_EEPROM_START>>16, ND_EEPROM_SIZE>>16);
    nd_map_banks(&nd_rom_access_bank, ND_EEPROM2_STRT>>16, ND_EEPROM2_SIZE>>16);
	
	write_log("Mapping NeXTdimension IO memory at $%08x\n", ND_IO_START);
	nd_map_banks(&nd_io_bank, ND_IO_START>>16, 1);
    
    write_log("Mapping NeXTdimension RAMDAC registers at $%08x\n", ND_RAMDAC_START);
    nd_map_banks(&nd_ramdac_bank, ND_RAMDAC_START>>16, 1);
    
	/* While we have no real ROM */
	ND_rom[0x1FFE0] = 0xA5; /* ROM signature */
	ND_rom[0x1FFD8] = 0x04; /* Byte lane ID */
	
	ND_rom[0x1FFB8] = 0x0F;
	ND_rom[0x1FFC0] = 0xFF;
	ND_rom[0x1FFC8] = 0xFF;
	ND_rom[0x1FFD0] = 0x00;
}

#endif
