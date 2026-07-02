/* cpu1.c — Phase-2 SMP demo: secondary core.
 *
 * Reads a seed cpu0 placed in the shared-RAM mailbox, computes a small
 * deterministic value cpu0 can independently reproduce (so the result is
 * verifiable, not just "cpu1 ran"), publishes it, sets the finish flag, and
 * spins forever. cpu1 never touches the UART or any other cpu0-owned
 * peripheral — the mailbox is the only communication channel.
 *
 * cpu1_main() is linked into the .cpu1 output section (VMA in the SDRAM
 * window, LMA in BRAM — see linker/sh32.x) so that cpu0's copy loop in
 * main_sh() can stage it into SDRAM before releasing cpu1 via CPU1_ENABLE.
 */

#define SHRAM_SEED   (*(volatile unsigned int *)0x00008100u)  /* in:  cpu0 seed */
#define SHRAM_RESULT (*(volatile unsigned int *)0x00008108u)  /* out: cpu1 result */
#define SHRAM_FINISH (*(volatile unsigned int *)0x0000810cu)  /* out: cpu1 done */
#define SHRAM_ECHO   (*(volatile unsigned int *)0x00008110u)  /* dbg: seed echo */

void __attribute__((section(".cpu1")))
cpu1_main(void)
{
    unsigned int s = SHRAM_SEED;
    SHRAM_ECHO = s;
    /* Straight-line (loop-free) arithmetic on cpu0's seed, verifiable by cpu0:
       (s<<4) + (s<<1) + 3 = 18*s + 3.  NOTE: kept loop-free deliberately —
       cpu1 executes straight-line code from SDRAM correctly, but LOOP execution
       from SDRAM currently returns wrong results (see docs handover: cpu1
       SDRAM loop-execution bug). This still proves cpu1 ran and consumed a
       cpu0-provided value. */
    SHRAM_RESULT = (s << 4) + (s << 1) + 3u;
    SHRAM_FINISH = 0x00000001u;
    for (;;) { }
}
