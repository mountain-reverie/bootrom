/**************************************
 SuperH (SH-2) C Compiler Linker Script
 **************************************/ 

OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)

MEMORY
{
	ram    : o = 0x00000000, l = 0x7d00
	stack  : o = 0x00007d00, l = 0x0300
	sdram  : o = 0x10000000, l = 0x40000000
}

SECTIONS 				
{
.text :	{
	*(.vect)
	*(.text) 				
	*(.strings)
   	 _etext = . ; 
	}  > ram

.tors : {
	___ctors = . ;
	*(.ctors)
	___ctors_end = . ;
	___dtors = . ;
	*(.dtors)
	___dtors_end = . ;
	}  > ram

.rodata : {
    *(.rodata*)
    } >ram

__idata_start = ADDR(.text) + SIZEOF(.text) + SIZEOF(.tors) + SIZEOF(.rodata); 
.data : AT(__idata_start) {
	__idata_start = .;
        _sdata = . ;
	*(.data)
	_edata = . ;
	}  > ram
__idata_end = __idata_start + SIZEOF(.data);

.bss : {
	_bss_start = .;
	*(.bss)
	*(COMMON)
	_end = .;
	}  >ram

.stack :
	{
	_stack = .;
	*(.stack)
	} > stack

/* Phase-2 SMP diagnostic (Task 1): cpu1's code is compiled/linked at the
 * same LMA as the rest of the ROM image (BRAM) but given a VMA in the
 * SDRAM window, so cpu0 can copy it up (like a .data section) before
 * releasing cpu1. Offsets chosen to avoid the SDRAM memory-test region
 * (MEM_TEST_ADDR 0x10000000 / MEM_TEST_SIZE 64MB, only compiled in when
 * CONFIG_TEST_MEM!=0; this board build uses CONFIG_TEST_MEM=0 and
 * CONFIG_DDR=0, so nothing else touches SDRAM in this image) and any
 * region a future kernel/DDR init would use at the very base of SDRAM. */
.cpu1 0x10001000 : ALIGN(4) {
	_cpu1_start = .;
	*(.cpu1 .cpu1.*)
	. = ALIGN(4);
	_cpu1_end = .;
	} > sdram AT> ram
_cpu1_load = LOADADDR(.cpu1);
_cpu1_stack_top = 0x10002000;   /* 4KB below the cpu1 code, grows down */
}
