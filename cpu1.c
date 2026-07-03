/* cpu1.c — Phase-2 SMP demo + cached-SDRAM 16-bit-read regression guard.
 *
 * cpu1 is the secondary core. It reads a seed cpu0 placed in the shared-RAM
 * mailbox (proving it ran and consumed cpu0's data), then performs a
 * self-checking probe of 16-bit (mov.w) reads from *cacheable SDRAM* and
 * publishes the result for cpu0 to verify. cpu1 never touches the UART or any
 * other cpu0-owned peripheral — the mailbox is the only communication channel.
 *
 * cpu1_main() is linked into the .cpu1 output section (VMA in the SDRAM window,
 * LMA in BRAM — see linker/sh32.x) so that cpu0's copy loop in main_sh() can
 * stage it into SDRAM before releasing cpu1 via CPU1_ENABLE. cpu1 is therefore
 * the first code to *execute and load data from cacheable SDRAM*, which is why
 * it is the natural place to guard the dcache halfword-read path.
 *
 * REGRESSION GUARD (mov.w from cacheable SDRAM):
 *   A 16-bit load that goes through the D-cache must return the correct
 *   halfword for both address parities. Store a known word 0x11112222 to an
 *   SDRAM scratch location, then read back the high (a(1)=0) and low (a(1)=1)
 *   halfwords with mov.w and recombine them. Correct big-endian SH-2 result is
 *   0x11112222; if the a(1)=1 read wrongly returns the high half the result is
 *   0x11111111 and cpu0 reports CPU1 FAIL. (BRAM/uncached reads are unaffected
 *   — the fault is specific to the cached-SDRAM 16-bit read path.)
 */

#define SHRAM_SEED   (*(volatile unsigned int *)0x00008100u)  /* in:  cpu0 seed */
#define SHRAM_RESULT (*(volatile unsigned int *)0x00008108u)  /* out: cpu1 result */
#define SHRAM_FINISH (*(volatile unsigned int *)0x0000810cu)  /* out: cpu1 done */
#define SHRAM_ECHO   (*(volatile unsigned int *)0x00008110u)  /* dbg: seed echo */

/* SDRAM scratch word, in the SDRAM window between the cpu1 code and its stack
 * (stack top 0x10002000, grows down; code ends well below 0x10001800). */
#define SDRAM_SCRATCH_W (*(volatile unsigned int   *)0x10001800u)
#define SDRAM_SCRATCH_H ( (volatile unsigned short *)0x10001800u)

void __attribute__((section(".cpu1")))
cpu1_main(void)
{
    unsigned int s = SHRAM_SEED;
    SHRAM_ECHO = s;                       /* prove cpu1 ran + read cpu0's seed */

    /* Cached-SDRAM 16-bit halfword read guard. */
    SDRAM_SCRATCH_W = 0x11112222u;        /* 32-bit store into cacheable SDRAM */
    unsigned int hi = SDRAM_SCRATCH_H[0]; /* mov.w a(1)=0 -> expect 0x1111     */
    unsigned int lo = SDRAM_SCRATCH_H[1]; /* mov.w a(1)=1 -> expect 0x2222     */
    SHRAM_RESULT = ((hi & 0xffffu) << 16) | (lo & 0xffffu);  /* expect 0x11112222 */

    SHRAM_FINISH = 0x00000001u;
    for (;;) { }
}
