/*
 * crt0.S — PIC32MK USB CDC Bootloader startup + exception vectors
 *
 * Built for mipsel-linux-gnu-gcc, targeting QEMU PIC32MK emulator.
 *
 * Memory layout defined by btl.ld:
 *   _RESET_ADDR     = 0xBFC00000   (kseg1_boot_mem, uncached)
 *   _ebase_address  = 0x9D000000   (kseg0_program_mem, cached)
 *   _GEN_EXCPT_ADDR = 0x9D000180   (EBASE + 0x180)
 *   Interrupt vector at EBASE + 0x200
 *
 * Sections used:
 *   .reset                  → 0xBFC00000  (reset entry)
 *   .simple_tlb_refill_excpt→ 0x9D000000  (EBASE+0, TLB stub)
 *   .gen_handler            → 0x9D000180  (EBASE+0x180, general exception)
 *
 * Interrupt dispatch (.vector_0) and handler wrappers (.vector_N)
 * are in irq_dispatch.S — firmware-specific, easy to swap.
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
 * .reset section — placed at _RESET_ADDR = 0xBFC00000 by btl.ld
 * ================================================================ */
    .section .reset, "ax", @progbits
    .globl  _reset
    .ent    _reset
_reset:
    /* STATUS = BEV (bit 22) — use bootstrap exception vectors until
     * we finish CP0 setup. All interrupts disabled (IE=0). */
    lui     $8, 0x0040          /* bit 22 = BEV */
    mtc0    $8, _CP0_STATUS
    ehb

    /* EBASE = 0x9D000000 (kseg0_program_mem, per btl.ld) */
    lui     $8, 0x9D00
    mtc0    $8, _CP0_EBASE
    ehb

    /* Cause.IV = 1 → use special interrupt vector (EBASE+0x200).
     * Cause.DC = 1 → disable core timer count until firmware enables it.
     * QEMU requires IV=1 for interrupt dispatch to reach EBASE+0x200.
     * DC=1 prevents the core timer from running and setting IFS0.CTIF
     * before the firmware is ready to handle it. */
    lui     $8, 0x0880          /* bit 27 = DC, bit 23 = IV */
    mtc0    $8, _CP0_CAUSE
    ehb

    /* IntCtl.VS = 0 → all interrupts land at EBASE+0x200 (single-vector).
     * The software dispatcher in irq_dispatch.S (.vector_dispatch section)
     * is placed at EBASE+0x200 and reads EVIC INTSTAT+OFFx to dispatch
     * each interrupt to the correct IntVectorXXX_Handler.
     * (VS≠0 would require QEMU to implement OFFx-based dispatch in the
     *  CPU exception handler, which it doesn't — it uses VS-based spacing.) */
    mfc0    $8, _CP0_INTCTL
    ins     $8, $0, 5, 5        /* clear VS field [9:5] → VS=0 */
    mtc0    $8, _CP0_INTCTL
    ehb

    /* Stack pointer: top of KSEG0 data RAM.
     * kseg0_data_mem: ORIGIN = 0x80000010, LENGTH = 0x3FFF0
     * Stack top = 0x80000010 + 0x3FFF0 = 0x80040000, aligned -8 */
    lui     $29, 0x8004
    addiu   $29, $29, -8        /* sp = 0x8003FFF8 */

    /* Global pointer (defined by btl.ld) */
    la      $28, _gp

    /* ----------------------------------------------------------
     * Zero BSS: _bss_begin to _bss_end (symbols from btl.ld)
     * ---------------------------------------------------------- */
    la      $8, _bss_begin
    la      $9, _bss_end
    beq     $8, $9, .Lbss_done
    nop
.Lbss_loop:
    sw      $0, 0($8)
    addiu   $8, $8, 4
    bne     $8, $9, .Lbss_loop
    nop
.Lbss_done:

    /* ----------------------------------------------------------
     * No .data copy needed: btl.ld places .data directly in
     * kseg0_data_mem with VMA == LMA (both in RAM).  The QEMU
     * loader maps the ELF segments at their VMA addresses.
     * ---------------------------------------------------------- */

    /* ----------------------------------------------------------
     * Initialize EVIC OFFx registers from the linker-generated
     * __vector_offset_N symbols.
     *
     * On real PIC32MK hardware, the EVIC reads OFFx[source] to
     * determine the vector offset for each interrupt source.
     * XC32's CRT does this initialization; since we replaced it
     * with our own crt0, we must do it here.
     *
     * In QEMU, the CPU exception handler doesn't use OFFx — it
     * uses the standard MIPS VS-based formula. Our software
     * dispatcher in irq_dispatch.S reads OFFx explicitly.
     *
     * OFFx base: EVIC_BASE + 0x540 = 0xBF810540
     * Each OFFx is a plain 4-byte register (no SET/CLR/INV).
     * We initialize all 191 entries from the _vector_offset_table
     * defined at the end of this file.
     * ---------------------------------------------------------- */
    la      $8, _vector_offset_table    /* source table in rodata */
    lui     $9, 0xBF81
    addiu   $9, $9, 0x0540             /* $9 = &OFFx[0] = 0xBF810540 */
    addiu   $10, $8, (191 * 4)         /* $10 = end of table */
.Loffx_loop:
    lw      $11, 0($8)                 /* load __vector_offset_N */
    sw      $11, 0($9)                 /* write to OFFx[N] */
    addiu   $8, $8, 4
    addiu   $9, $9, 4
    bne     $8, $10, .Loffx_loop
    nop

    /* ----------------------------------------------------------
     * Enable interrupts: clear BEV, set IE.
     * STATUS: BEV=0, MX=1 (DSP ASE), IPL=0 (bits[15:10]=0), IE=0.
     *
     * Leave interrupts DISABLED (IE=0). FreeRTOS's vPortStartFirstTask
     * will enable them when the scheduler is ready. Enabling interrupts
     * before FreeRTOS initializes uxInterruptNesting/pxCurrentTCB causes
     * the portSAVE_CONTEXT/portRESTORE_CONTEXT macros to access garbage.
     *
     * microAptiv uses VEIC mode (Config3.VEIC=1).  In VEIC mode
     * the interrupt check is:  (Cause & 0xFF00) > (Status & 0xFF00)
     * Setting Status bits[15:8] would raise the current IPL and
     * BLOCK lower-priority interrupts.  IPL=0 allows everything.
     * ---------------------------------------------------------- */
    lui     $8, 0x0100          /* bit 24 = MX (DSP extension enable) */
    /* IE=0 — interrupts disabled until FreeRTOS scheduler starts */
    mtc0    $8, _CP0_STATUS
    ehb

    /* Jump to main() — SYS_Initialize + SYS_Tasks loop.
     * Use la+jalr (long jump) because _reset is at 0xBFC00000
     * and main is in kseg0_program_mem (0x9D000000) — different
     * 256MB regions, so jal (R_MIPS_26) cannot reach it. */
    la      $25, main
    jalr    $25
    nop

    /* main() should never return; spin if it does */
.Lhang:
    b       .Lhang
    nop
    .end    _reset

/* ================================================================
 * .simple_tlb_refill_excpt — placed at _ebase_address + 0 = 0x9D000000
 * TLB refill stub (PIC32MK has no TLB; trap here if triggered).
 * This section MUST exist so objcopy starts app.bin at 0x9D000000,
 * matching QEMU's program-flash load address.
 * ================================================================ */
    .section .simple_tlb_refill_excpt, "ax", @progbits
    .globl  _simple_tlb_refill_exception
    .ent    _simple_tlb_refill_exception
_simple_tlb_refill_exception:
1:  wait
    b       1b
    nop
    .end    _simple_tlb_refill_exception

/* ================================================================
 * .gen_handler section — placed at _GEN_EXCPT_ADDR = 0x9D000180
 * General exception handler (address errors, bus faults, syscall…)
 * ================================================================ */
    .section .gen_handler, "ax", @progbits
    .globl  _gen_exception
    .ent    _gen_exception
_gen_exception:
1:  wait
    b       1b
    nop
    .end    _gen_exception

/* ================================================================
 * Interrupt dispatch and handler wrappers are in irq_dispatch.S
 * (firmware-specific, easy to swap for different firmware targets).
 * ================================================================ */

/* ================================================================
 * Vector offset table — 191 entries for EVIC OFFx initialization.
 *
 * Each entry is the linker-computed __vector_offset_N value (offset
 * from EBASE to the handler for source N). crt0.S copies these
 * into the EVIC OFFx MMIO registers at boot.
 *
 * Placed in .rodata so it lives in program flash alongside code.
 * ================================================================ */
    .section .rodata.vector_offsets, "a", @progbits
    .align  2
    .globl  _vector_offset_table
_vector_offset_table:
    .word   __vector_offset_0
    .word   __vector_offset_1
    .word   __vector_offset_2
    .word   __vector_offset_3
    .word   __vector_offset_4
    .word   __vector_offset_5
    .word   __vector_offset_6
    .word   __vector_offset_7
    .word   __vector_offset_8
    .word   __vector_offset_9
    .word   __vector_offset_10
    .word   __vector_offset_11
    .word   __vector_offset_12
    .word   __vector_offset_13
    .word   __vector_offset_14
    .word   __vector_offset_15
    .word   __vector_offset_16
    .word   __vector_offset_17
    .word   __vector_offset_18
    .word   __vector_offset_19
    .word   __vector_offset_20
    .word   __vector_offset_21
    .word   __vector_offset_22
    .word   __vector_offset_23
    .word   __vector_offset_24
    .word   __vector_offset_25
    .word   __vector_offset_26
    .word   __vector_offset_27
    .word   __vector_offset_28
    .word   __vector_offset_29
    .word   __vector_offset_30
    .word   __vector_offset_31
    .word   __vector_offset_32
    .word   __vector_offset_33
    .word   __vector_offset_34
    .word   __vector_offset_35
    .word   __vector_offset_36
    .word   __vector_offset_37
    .word   __vector_offset_38
    .word   __vector_offset_39
    .word   __vector_offset_40
    .word   __vector_offset_41
    .word   __vector_offset_42
    .word   __vector_offset_43
    .word   __vector_offset_44
    .word   __vector_offset_45
    .word   __vector_offset_46
    .word   __vector_offset_47
    .word   __vector_offset_48
    .word   __vector_offset_49
    .word   __vector_offset_50
    .word   __vector_offset_51
    .word   __vector_offset_52
    .word   __vector_offset_53
    .word   __vector_offset_54
    .word   __vector_offset_55
    .word   __vector_offset_56
    .word   __vector_offset_57
    .word   __vector_offset_58
    .word   __vector_offset_59
    .word   __vector_offset_60
    .word   __vector_offset_61
    .word   __vector_offset_62
    .word   __vector_offset_63
    .word   __vector_offset_64
    .word   __vector_offset_65
    .word   __vector_offset_66
    .word   __vector_offset_67
    .word   __vector_offset_68
    .word   __vector_offset_69
    .word   __vector_offset_70
    .word   __vector_offset_71
    .word   __vector_offset_72
    .word   __vector_offset_73
    .word   __vector_offset_74
    .word   __vector_offset_75
    .word   __vector_offset_76
    .word   __vector_offset_77
    .word   __vector_offset_78
    .word   __vector_offset_79
    .word   __vector_offset_80
    .word   __vector_offset_81
    .word   __vector_offset_82
    .word   __vector_offset_83
    .word   __vector_offset_84
    .word   __vector_offset_85
    .word   __vector_offset_86
    .word   __vector_offset_87
    .word   __vector_offset_88
    .word   __vector_offset_89
    .word   __vector_offset_90
    .word   __vector_offset_91
    .word   __vector_offset_92
    .word   __vector_offset_93
    .word   __vector_offset_94
    .word   __vector_offset_95
    .word   __vector_offset_96
    .word   __vector_offset_97
    .word   __vector_offset_98
    .word   __vector_offset_99
    .word   __vector_offset_100
    .word   __vector_offset_101
    .word   __vector_offset_102
    .word   __vector_offset_103
    .word   __vector_offset_104
    .word   __vector_offset_105
    .word   __vector_offset_106
    .word   __vector_offset_107
    .word   __vector_offset_108
    .word   __vector_offset_109
    .word   __vector_offset_110
    .word   __vector_offset_111
    .word   __vector_offset_112
    .word   __vector_offset_113
    .word   __vector_offset_114
    .word   __vector_offset_115
    .word   __vector_offset_116
    .word   __vector_offset_117
    .word   __vector_offset_118
    .word   __vector_offset_119
    .word   __vector_offset_120
    .word   __vector_offset_121
    .word   __vector_offset_122
    .word   __vector_offset_123
    .word   __vector_offset_124
    .word   __vector_offset_125
    .word   __vector_offset_126
    .word   __vector_offset_127
    .word   __vector_offset_128
    .word   __vector_offset_129
    .word   __vector_offset_130
    .word   __vector_offset_131
    .word   __vector_offset_132
    .word   __vector_offset_133
    .word   __vector_offset_134
    .word   __vector_offset_135
    .word   __vector_offset_136
    .word   __vector_offset_137
    .word   __vector_offset_138
    .word   __vector_offset_139
    .word   __vector_offset_140
    .word   __vector_offset_141
    .word   __vector_offset_142
    .word   __vector_offset_143
    .word   __vector_offset_144
    .word   __vector_offset_145
    .word   __vector_offset_146
    .word   __vector_offset_147
    .word   __vector_offset_148
    .word   __vector_offset_149
    .word   __vector_offset_150
    .word   __vector_offset_151
    .word   __vector_offset_152
    .word   __vector_offset_153
    .word   __vector_offset_154
    .word   __vector_offset_155
    .word   __vector_offset_156
    .word   __vector_offset_157
    .word   __vector_offset_158
    .word   __vector_offset_159
    .word   __vector_offset_160
    .word   __vector_offset_161
    .word   __vector_offset_162
    .word   __vector_offset_163
    .word   __vector_offset_164
    .word   __vector_offset_165
    .word   __vector_offset_166
    .word   __vector_offset_167
    .word   __vector_offset_168
    .word   __vector_offset_169
    .word   __vector_offset_170
    .word   __vector_offset_171
    .word   __vector_offset_172
    .word   __vector_offset_173
    .word   __vector_offset_174
    .word   __vector_offset_175
    .word   __vector_offset_176
    .word   __vector_offset_177
    .word   __vector_offset_178
    .word   __vector_offset_179
    .word   __vector_offset_180
    .word   __vector_offset_181
    .word   __vector_offset_182
    .word   __vector_offset_183
    .word   __vector_offset_184
    .word   __vector_offset_185
    .word   __vector_offset_186
    .word   __vector_offset_187
    .word   __vector_offset_188
    .word   __vector_offset_189
    .word   __vector_offset_190
