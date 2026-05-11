/*
 * freertos_overrides.c — Wrapper-layer overrides for FreeRTOS hook functions
 *
 * TARGET/firmware/src/config/default/freertos_hooks.c is read-only (firmware
 * mirror), so we cannot modify it directly.  Instead, the linker's --wrap
 * mechanism is used: --wrap=vApplicationIdleHook redirects every call to
 * vApplicationIdleHook() here (__wrap_vApplicationIdleHook), leaving the
 * original empty definition in freertos_hooks.c as an unreferenced symbol.
 */

/*
 * vApplicationIdleHook — MIPS 'wait' idle hook
 *
 * The MIPS32 'wait' instruction halts the CPU pipeline until the next
 * interrupt arrives (MIPS32r2 Architecture vol.II, §4.8.1).  On QEMU this
 * lets the host thread yield instead of spinning, improving emulation
 * throughput and host CPU utilisation.
 *
 * The "memory" clobber prevents the compiler from reordering memory
 * accesses across the instruction.
 *
 * Linked via -Wl,--wrap=vApplicationIdleHook (see Makefile.py).
 */
void __wrap_vApplicationIdleHook(void)
{
    __asm__ __volatile__("wait" ::: "memory");
}
