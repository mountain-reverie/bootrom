/* cpu1_diag.c — Phase-2 SMP bring-up diagnostic (Task 1).
 *
 * Temporary: this file exists to prove, in GHDL simulation, that cpu0 can
 * release cpu1 and cpu1 can execute code staged into SDRAM by cpu0 (the
 * canonical jcore spin-table bring-up). It is folded into a real smp.c in
 * Task 2 and should be considered scaffolding.
 *
 * cpu1_main() is linked into the .cpu1 output section (VMA in the SDRAM
 * window, LMA in BRAM — see linker/sh32.x) so that cpu0's copy loop in
 * main_sh() can stage it into SDRAM before releasing cpu1 via CPU1_ENABLE.
 * Once running, cpu1 writes a sentinel value to the shared-RAM mailbox at
 * SHRAM_SENTINEL and spins forever.
 */

#define SHRAM_SENTINEL (*(volatile unsigned int *)0x0000810cu)

void __attribute__((section(".cpu1")))
cpu1_main(void)
{
    SHRAM_SENTINEL = 0xC0DE0001u;   /* proof cpu1 executed */
    for (;;) { }
}
