/*
 * sys/attribs.h — GCC/mipsel-linux-gnu stub for Microchip XC32 attribs.
 *
 * Maps XC32-specific function/variable attribute macros to standard GCC
 * equivalents so that Harmony-generated code compiles under mipsel-linux-gnu-gcc.
 *
 * Reference: /opt/microchip/xc32/v4.60/pic32c/include/sys/attribs.h
 */

#ifndef __ATTRIBS_H
#define __ATTRIBS_H

/* Place object into a named section — identical in GCC */
#define __section__(n)      __attribute__((section(n)))

/* Unique section: GCC has no direct equivalent; the -ffunction-sections /
 * -fdata-sections flags handle this at build level. No-op is safe here. */
#define __unique_section__  /* empty */

/* __ramfunc__ — XC32 'ramfunc' and 'unique_section' attrs are not supported
 * by GCC. Map to section + noinline, which is enough for emulation. */
#define __ramfunc__         __attribute__((section(".ramfunc"), noinline))

/* __longramfunc__ — same as __ramfunc__ but callable from any memory region.
 * 'long_call' IS supported by GCC MIPS. */
#define __longramfunc__     __attribute__((section(".ramfunc"), long_call, noinline))

/* __longcall__ — force indirect call via register. Supported by GCC MIPS. */
#define __longcall__        __attribute__((long_call))

/* __ISR — Harmony interrupt handler decorator.
 * Under XC32 this wires up the vector table; under GCC/QEMU we only need
 * the function to be visible and non-inlined. */
#ifndef __ISR
#define __ISR(vector, ipl) __attribute__((used, noinline))
#endif

#endif /* __ATTRIBS_H */
