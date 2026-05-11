/*
 * irq_dispatch.S — Software interrupt dispatcher for QEMU emulation
 *
 * Problem: QEMU's MIPS CPU exception handler computes the interrupt
 * vector offset as: offset = 0x200 + vector * (VS << 5).
 * With IntCtl.VS=0, ALL interrupts land at EBASE+0x200 regardless of
 * source. The PIC32MK hardware uses OFFx registers for per-source
 * dispatch, but QEMU doesn't implement that.
 *
 * Solution: Place this software dispatcher at EBASE+0x200 (before
 * .vector_0). It reads INTSTAT to get the source number, looks up
 * the handler offset from the EVIC OFFx register, and jumps to
 * EBASE + OFFx[source]. The OFFx registers are initialized by
 * crt0.S at boot with the linker-computed __vector_offset_N values.
 *
 * Register usage: only k0/k1 (exception-reserved scratch registers).
 * The individual handlers (in interrupts_a.S) do portSAVE_CONTEXT
 * which re-reads CP0 Cause/Status, so clobbering k0/k1 is safe.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <xc.h>

    .set    nomips16
    .set    nomicromips
    .set    noreorder
    .set    noat

/* ================================================================
 * .vector_dispatch — placed at EBASE+0x200 by linker script
 * (inserted BEFORE .vector_0 in the .vectors section).
 *
 * With IntCtl.VS=0, ALL interrupts arrive here.
 * Reads EVIC INTSTAT[7:0] to get source number, then reads OFFx
 * to get the handler offset, and jumps to EBASE + OFFx[source].
 * ================================================================ */
    .section .vector_dispatch, "ax", @progbits
    .globl  _software_vector_dispatch
    .ent    _software_vector_dispatch

_software_vector_dispatch:
    /*
     * Step 1: Read INTSTAT register (EVIC_BASE + 0x20 = 0xBF810020)
     * Bits [7:0] = interrupt source number of highest-priority pending IRQ.
     */
    lui     k0, 0xBF81             /* k0 = 0xBF810000 (EVIC base) */
    lw      k1, 0x0020(k0)        /* k1 = INTSTAT */
    andi    k1, k1, 0xFF          /* k1 = source number (bits 7:0) */

    /*
     * Step 2: Read OFFx[source] from EVIC (EVIC_BASE + 0x540 + source*4)
     * OFFx contains the offset from EBASE to the handler for this source.
     * These were initialized by crt0.S from linker __vector_offset_N symbols.
     */
    sll     k1, k1, 2             /* k1 = source * 4 (byte offset) */
    addiu   k0, k0, 0x0540        /* k0 = &OFFx[0] = 0xBF810540 */
    addu    k0, k0, k1            /* k0 = &OFFx[source] */
    lw      k0, 0(k0)             /* k0 = OFFx[source] = offset from EBASE */

    /*
     * Step 3: Jump to EBASE + OFFx[source].
     * EBASE = 0x9D000000 (set by crt0.S, also a linker symbol).
     */
    lui     k1, 0x9D00            /* k1 = _ebase_address = 0x9D000000 */
    addu    k0, k1, k0            /* k0 = EBASE + offset = handler address */
    jr      k0                    /* jump to the correct IntVectorXXX_Handler */
    nop

    .end    _software_vector_dispatch
