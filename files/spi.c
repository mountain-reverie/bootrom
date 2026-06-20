/* This file come from fwprg.c providing Bootloader SPI read feature only for code size reason */
#include "board.h"
#include "spi.h"
#include "pff.h"

#ifdef DEVICE_SPI2_ADDR

/* spi2 implementation — used when board.h defines DEVICE_SPI2_ADDR */
#define SPI2_CS0    0x01u   /* bit0: cs[0] output (0=SD selected) */
#define SPI2_START  0x02u   /* bit1 write: start transaction */
#define SPI2_BUSY   0x02u   /* bit1 read:  busy */
#define SPI2_DIV(x) ((unsigned)(x) << 27)  /* bits31:27: speed divisor */

static unsigned spi2_ctrl;  /* shadow: current CS + speed state */

void spi_setspeed(int fast)
{
    spi2_ctrl = (spi2_ctrl & SPI2_CS0) |
                (fast ? SPI2_DIV(0) : SPI2_DIV(31));
    DEVICE_SPI2->ctrl = spi2_ctrl;
}

/* chip: SpiData_Chip(1)=select SD, 0=deselect */
void spi_cs(int chip)
{
    /* drain any in-progress transfer */
    while (DEVICE_SPI2->ctrl & SPI2_BUSY)
        ;
    if (chip)
        spi2_ctrl &= ~SPI2_CS0;   /* cs[0]=0 → SD selected (active-low) */
    else
        spi2_ctrl |= SPI2_CS0;    /* cs[0]=1 → SD deselected */
    DEVICE_SPI2->ctrl = spi2_ctrl;
}

unsigned char xronebyte(unsigned char c)
{
    DEVICE_SPI2->data = c;
    DEVICE_SPI2->ctrl = spi2_ctrl | SPI2_START;
    while (DEVICE_SPI2->ctrl & SPI2_BUSY)
        ;
    return (unsigned char)(DEVICE_SPI2->data & 0xffu);
}

/* sdc_initialize / sdc_readp from spi_mmc.c call spi_cs/xronebyte/spi_setspeed.
   No spif_* (SPI flash) functions needed when CONFIG_SPIFLASH=0. */

extern void putstr(const char *);

/* spi_loopback_test: enable spi2 hardware loopback, send 0xA5, verify echo.
   Writes directly to ctrl (bypasses spi2_ctrl shadow) so it can self-contain
   the loopback bit without affecting CS state seen by spi_mmc.c. */
void spi_loopback_test(void)
{
    unsigned char rx;
    /* loopback=bit3=1, cs[0]=1 (desel), start=0 */
    DEVICE_SPI2->ctrl = 0x09u;        /* bits: loop(3)=1, cs0(0)=1 */
    DEVICE_SPI2->data = 0xA5u;
    DEVICE_SPI2->ctrl = 0x09u | 0x02u; /* + start(1)=1 */
    while (DEVICE_SPI2->ctrl & 0x02u)  /* poll busy */
        ;
    rx = (unsigned char)(DEVICE_SPI2->data & 0xffu);
    /* restore: no loopback, cs[0]=1 (desel), speed=reset */
    spi2_ctrl = 0x01u;
    DEVICE_SPI2->ctrl = spi2_ctrl;
    if (rx == 0xA5u)
        putstr("SPI LOOPBACK OK\n");
    else
        putstr("SPI LOOPBACK FAIL\n");
}

#else /* original spi.vhd path — unchanged below */

#define SPI_RES		0xAB
#define SPI_RDID	0x9F
#define SPI_READ	0x03
#define SPI_FAST_READ	0x0B

#define SPIDATA	(DEVICE_FLASH->data)
#define SPICTL	(DEVICE_FLASH->ctrl)

static unsigned chipselect_status;
static unsigned spi_speed;

#ifdef Digilent_Boot
	#define Sectors	32
#else
	#define Sectors 64
#endif
#define Page_size	256
#define Pages		256

/* chip: 0 none of cs is active; 1: Data FLASH cs is actived 2: FPGA configure cs is active */
void spi_cs(int chip) {
	unsigned rv;
	/* just in case for pervious transmission if not finished */
	rv = SPICTL;
	while(rv & SpiCtrl_Xmit) {
		rv = SPICTL;
	}
	if(chip) {
		if(chip == SpiData_Chip) {
			chipselect_status = spi_speed | SpiCtrl_CCS | SpiCtrl_DCS;
		} else if(chip == SpiConf_Chip) {
			chipselect_status = spi_speed | SpiCtrl_ACS | SpiCtrl_DCS;
		}
	} else
		chipselect_status = SpiCtrl_ACS | SpiCtrl_CCS | SpiCtrl_DCS;
	SPICTL = chipselect_status;
}
/* if speed == 0: 400KHz, if speed = true: 12.5 MHz */
void spi_setspeed(int speed) {
	if(!speed)
		spi_speed = SpiCtrl_setDiv(31);
	else
		spi_speed = SpiCtrl_setDiv(0);
}
unsigned char xronebyte(unsigned char c) {
	unsigned uv;
	SPIDATA = c;
	SPICTL = chipselect_status | SpiCtrl_Xmit;
	uv = SPICTL;
	while(uv & SpiCtrl_Busy) 
		uv = SPICTL;
	uv = SPIDATA;
	return uv & 0xff;
}
int spi_read(int chip, unsigned addr, unsigned char *bufptr, unsigned len) {
	unsigned char *ptr;

	ptr = bufptr;
	spi_cs(chip);
	xronebyte(SPI_READ);
	xronebyte(addr >> 16);
	xronebyte(addr >> 8);
	xronebyte(addr);
	while(len) {
		*ptr++ = xronebyte(0);
		len--;
	}
	spi_cs(0);
	
	return 1;  
}
#if 0
unsigned char get_sr(int chip) {
	unsigned char rc;
	spi_cs(chip);
	xronebyte(SPI_RDSR);
	rc = xronebyte(0);
	spi_cs(0);
	return rc;
}
#endif
/* return manufacturer ID | MemoryType | MemoryCapacity | UID format */
static unsigned get_rdid(int chip) {
	unsigned rv;
	int i;
	spi_cs(chip);
	xronebyte(SPI_RDID);
	rv = 0;
	for(i = 0; i < 4; i++) {
		rv <<= 8;
		rv |= xronebyte(0);
	}
	spi_cs(0);
	return rv;
}
static char get_res(int chip) {
	char rc;
	spi_cs(chip);
	xronebyte(SPI_RES);
	xronebyte(0);
	xronebyte(0);
	xronebyte(0);
	rc = xronebyte(0);
	spi_cs(0);
	return rc;
}
/* Start of diskio.h functions, These are default SPI Flash readonly drive */
static unsigned dataflash_size = 0;
DSTATUS spif_initialize(void) {
	unsigned uv;
	/*  use get_rdid to test the max size of FLASH */
	spi_setspeed(1);
	uv = get_res(SpiData_Chip);	/* SPI Flash wakeup */
	if(!uv)
		return STA_NOINIT;
	uv = get_rdid(SpiData_Chip);
	uv >>= 8;
	uv &= 0xff;	/* get the cap byte */
	if(uv == 0x16){	/* M25P32 */
		dataflash_size = 4194304;
	} else if(uv == 0x15) {	/* M25P16 */
		dataflash_size = 2097152;	/* Sectors * Pages * Page_size */
	} else if(uv == 0x17) { /* MX25L64 */
		dataflash_size = 8388608;
	} else {
		dataflash_size = 0;
		return STA_NOINIT;
	}
	return 0;
}
DRESULT spif_readp (BYTE* dest, DWORD sector, WORD soffset, WORD bytecount) {
	unsigned uv;
	if(!dataflash_size)
		return RES_PARERR;
	uv = sector * 512 + soffset;
	if((uv + bytecount) >= dataflash_size)
		return RES_PARERR;
	spi_read(SpiData_Chip, uv, dest, bytecount);
	return RES_OK;
}

#endif /* DEVICE_SPI2_ADDR */
