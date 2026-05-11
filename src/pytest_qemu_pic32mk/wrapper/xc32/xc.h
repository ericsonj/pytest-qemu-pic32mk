/*
 * xc.h — GCC/mipsel compatibility shim for Microchip XC32 built-ins.
 *
 * Provides CP0 access macros and PIC32MK SFR register definitions so that
 * the FreeRTOS PIC32MK port (port.c / portmacro.h / port_asm.S) compiles
 * with mipsel-linux-gnu-gcc.
 *
 * SFR base addresses come from pic32mk.h constants:
 *   EVIC     0xBF810000  (SFR_BASE + EVIC_OFFSET = 0xBF800000 + 0x010000)
 *   Timer1   0xBF820000  (SFR_BASE + T1_OFFSET   = 0xBF800000 + 0x020000)
 *   UART1    0xBF828000  (SFR_BASE + UART1_OFFSET = 0xBF800000 + 0x028000)
 */

#ifndef XC_H
#define XC_H

/* ========================================================================
 * Assembly context — define CP0 register names as GAS operand tokens
 * ======================================================================== */
#ifdef __ASSEMBLER__

/* Register name aliases (zero, k0, k1, sp, gp, ra, s0-s7, t0-t9, etc.)
 * GNU AS for MIPS requires the $ prefix; sys/regdef.h provides CPP macros
 * that expand bare names (e.g. k0) to their $-prefixed form ($26). */
#include <sys/regdef.h>

#define _CP0_STATUS     $12, 0
#define _CP0_CAUSE      $13, 0
#define _CP0_EPC        $14, 0
#define _CP0_COUNT      $9,  0
#define _CP0_COMPARE    $11, 0
#define _CP0_EBASE      $15, 1
#define _CP0_INTCTL     $12, 1
#define _CP0_SRSCTL     $12, 2

/* SFR addresses used directly in assembly (e.g. "la reg, IFS0CLR")
 * PIC32MK: CLR=+4, SET=+8 from each register base */
#define IFS0CLR         0xBF810044
#define IEC0SET         0xBF8100C8
#define IEC0CLR         0xBF8100C4

#else /* C context */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * CP0 access macros using GCC inline asm
 * ----------------------------------------------------------------------- */

#define _CP0_GET_STATUS() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$12,0" : "=r"(__r)); __r; })

#define _CP0_SET_STATUS(_v) \
    __asm__ volatile("mtc0 %0,$12,0; ehb" : : "r"((uint32_t)(_v)))

#define _CP0_GET_CAUSE() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$13,0" : "=r"(__r)); __r; })

#define _CP0_SET_CAUSE(_v) \
    __asm__ volatile("mtc0 %0,$13,0; ehb" : : "r"((uint32_t)(_v)))

#define _CP0_GET_EPC() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$14,0" : "=r"(__r)); __r; })

#define _CP0_GET_BADVADDR() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$8,0" : "=r"(__r)); __r; })

#define _CP0_GET_COUNT() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$9,0" : "=r"(__r)); __r; })

#define _CP0_SET_COUNT(_v) \
    __asm__ volatile("mtc0 %0,$9,0; ehb" : : "r"((uint32_t)(_v)))

#define _CP0_SET_COMPARE(_v) \
    __asm__ volatile("mtc0 %0,$11,0; ehb" : : "r"((uint32_t)(_v)))

/* CP0 Cause register bit masks */
#define _CP0_CAUSE_DC_MASK      0x08000000u  /* Disable Count (bit 27) */

/* CLZ — count leading zeros */
static inline unsigned int _clz(unsigned int x)
{
    return x ? __builtin_clz(x) : 32u;
}

/* Bit-set / bit-clear helpers for CP0 registers (XC32 built-ins) */
#define _CP0_BIS_STATUS(_mask) \
    _CP0_SET_STATUS(_CP0_GET_STATUS() | (uint32_t)(_mask))

#define _CP0_BIC_STATUS(_mask) \
    _CP0_SET_STATUS(_CP0_GET_STATUS() & ~(uint32_t)(_mask))

#define _CP0_BIS_CAUSE(_mask) \
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() | (uint32_t)(_mask))

#define _CP0_BIC_CAUSE(_mask) \
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() & ~(uint32_t)(_mask))

/* XC32 built-in to disable interrupts — returns old CP0_Status (IE in bit 0) */
static inline uint32_t __builtin_disable_interrupts(void)
{
    uint32_t status;
    __asm__ volatile("di %0; ehb" : "=r"(status));
    return status;
}

/* XC32 built-in to enable interrupts */
static inline void __builtin_enable_interrupts(void)
{
    __asm__ volatile("ei; ehb" : : : "memory");
}

/* XC32 built-in to write CP0 register — used by Harmony plib_eeprom.c */
static inline void __builtin_mtc0(unsigned reg, unsigned sel, uint32_t val)
{
    if (reg == 12 && sel == 0) {
        _CP0_SET_STATUS(val);
    }
}

/* -----------------------------------------------------------------------
 * EVIC registers  (base 0xBF810000)
 * IFS/IEC/IPC each have SET (+4), CLR (+8), INV (+C) sub-registers.
 * ----------------------------------------------------------------------- */

#define _EVIC_BASE      0xBF810000u

/* EVIC CLR/SET/INV convention (PIC32MK): CLR at +4, SET at +8, INV at +C */
#define IFS0            (*(volatile uint32_t *)(_EVIC_BASE + 0x0040u))
#define IFS0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0044u))
#define IFS0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0048u))
#define IFS0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x004Cu))

#define IFS1            (*(volatile uint32_t *)(_EVIC_BASE + 0x0050u))
#define IFS1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0054u))
#define IFS1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0058u))
#define IFS1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x005Cu))

#define IEC0            (*(volatile uint32_t *)(_EVIC_BASE + 0x00C0u))
#define IEC0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00C4u))
#define IEC0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00C8u))
#define IEC0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00CCu))

#define IEC1            (*(volatile uint32_t *)(_EVIC_BASE + 0x00D0u))
#define IEC1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00D4u))
#define IEC1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00D8u))
#define IEC1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00DCu))

/* UART1 interrupt bit positions in IFS1/IEC1 (from p32mk1024mcm100.h):
 *   U1E = bit 6, U1RX = bit 7, U1TX = bit 8 */
#define _IFS1_U1EIF_MASK        0x00000040u
#define _IFS1_U1RXIF_MASK       0x00000080u
#define _IFS1_U1TXIF_MASK       0x00000100u
#define _IEC1_U1EIE_MASK        0x00000040u
#define _IEC1_U1RXIE_MASK       0x00000080u
#define _IEC1_U1TXIE_MASK       0x00000100u

#define IPC0            (*(volatile uint32_t *)(_EVIC_BASE + 0x0140u))
#define IPC0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0144u))
#define IPC0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0148u))
#define IPC0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x014Cu))

/* IPC1 covers EVIC sources 4-7.  Timer1 is source 4 → byte 0 of IPC1.
 * IP field at bits [4:2], IS field at bits [1:0]. */
#define IPC1            (*(volatile uint32_t *)(_EVIC_BASE + 0x0150u))
#define IPC1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0154u))
#define IPC1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0158u))
#define IPC1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x015Cu))

/* IPC1 bit-field struct for T1IP / T1IS access */
typedef union {
    struct {
        uint32_t T1IS   : 2;   /* bits [1:0]  subpriority for Timer1 */
        uint32_t T1IP   : 3;   /* bits [4:2]  priority    for Timer1 */
        uint32_t        : 27;
    };
    uint32_t w;
} __IPC1_T1_t;

#define IPC1bits        (*(volatile __IPC1_T1_t *)(_EVIC_BASE + 0x0150u))

/*
 * IPC8 covers EVIC sources 32-35.  USB1 = source 34 → byte 2 of IPC8.
 * IPC8 offset = 0x0140 + 8*0x10 = 0x01C0.
 * USB1 priority bits = [18:16], subpriority [20:19].
 */
#define IPC8            (*(volatile uint32_t *)(_EVIC_BASE + 0x01C0u))
#define IPC8CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01C4u))
#define IPC8SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01C8u))
#define IPC8INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01CCu))
#define _IPC8_USB1IP_POSITION   18u   /* USB1 priority bits [20:18] (EVIC shift+2) */
#define _IPC8_USB1IP_MASK       (0x7u << 18u)
#define _IPC8_USB1IS_POSITION   16u   /* USB1 subpriority bits [17:16] */
#define _IPC8_USB1IS_MASK       (0x3u << 16u)

/*
 * IPC9 covers EVIC sources 36-39.  UART1 FAULT=38 (byte 2), RX=39 (byte 3).
 * IPC9 offset = 0x0140 + 9*0x10 = 0x01D0.
 */
#define IPC9            (*(volatile uint32_t *)(_EVIC_BASE + 0x01D0u))
#define IPC9CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01D4u))
#define IPC9SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01D8u))
#define IPC9INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01DCu))

/*
 * IPC10 covers EVIC sources 40-43.  UART1 TX=40 → byte 0 of IPC10.
 * IPC10 offset = 0x0140 + 10*0x10 = 0x01E0.
 */
#define IPC10           (*(volatile uint32_t *)(_EVIC_BASE + 0x01E0u))
#define IPC10CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x01E4u))
#define IPC10SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x01E8u))
#define IPC10INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x01ECu))

/* IPC11 offset = 0x0140 + 11*0x10 = 0x01F0 */
#define IPC11           (*(volatile uint32_t *)(_EVIC_BASE + 0x01F0u))
#define IPC11CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x01F4u))
#define IPC11SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x01F8u))
#define IPC11INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x01FCu))

/* IPC12 offset = 0x0200 */
#define IPC12           (*(volatile uint32_t *)(_EVIC_BASE + 0x0200u))
#define IPC12CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0204u))
#define IPC12SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0208u))
#define IPC12INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x020Cu))

/* IPC14 offset = 0x0220 */
#define IPC14           (*(volatile uint32_t *)(_EVIC_BASE + 0x0220u))
#define IPC14CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0224u))
#define IPC14SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0228u))
#define IPC14INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x022Cu))

/* IPC16 offset = 0x0240 */
#define IPC16           (*(volatile uint32_t *)(_EVIC_BASE + 0x0240u))
#define IPC16CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0244u))
#define IPC16SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0248u))
#define IPC16INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x024Cu))

/* IPC19 offset = 0x0270 */
#define IPC19           (*(volatile uint32_t *)(_EVIC_BASE + 0x0270u))
#define IPC19CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0274u))
#define IPC19SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0278u))
#define IPC19INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x027Cu))

/* IPC20 offset = 0x0280 */
#define IPC20           (*(volatile uint32_t *)(_EVIC_BASE + 0x0280u))
#define IPC20CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0284u))
#define IPC20SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0288u))
#define IPC20INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x028Cu))

/* IPC21 offset = 0x0290 */
#define IPC21           (*(volatile uint32_t *)(_EVIC_BASE + 0x0290u))
#define IPC21CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0294u))
#define IPC21SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0298u))
#define IPC21INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x029Cu))

/* IPC22 offset = 0x02A0 */
#define IPC22           (*(volatile uint32_t *)(_EVIC_BASE + 0x02A0u))
#define IPC22CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x02A4u))
#define IPC22SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x02A8u))
#define IPC22INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x02ACu))

/* IPC25 offset = 0x0140 + 25*0x10 = 0x02D0 */
#define IPC25           (*(volatile uint32_t *)(_EVIC_BASE + 0x02D0u))
#define IPC25CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x02D4u))
#define IPC25SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x02D8u))
#define IPC25INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x02DCu))

/* IPC offsets verified against p32mk1024mcm100.h: IPC_BASE + N*0x10 */
/* IPC41: 0x0140 + 41*0x10 = 0x03D0 */
#define IPC41           (*(volatile uint32_t *)(_EVIC_BASE + 0x03D0u))
#define IPC41CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x03D4u))
#define IPC41SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x03D8u))
#define IPC41INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x03DCu))

/* IPC42: 0x0140 + 42*0x10 = 0x03E0 */
#define IPC42           (*(volatile uint32_t *)(_EVIC_BASE + 0x03E0u))
#define IPC42CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x03E4u))
#define IPC42SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x03E8u))
#define IPC42INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x03ECu))

/* IPC46: 0x0140 + 46*0x10 = 0x0420 */
#define IPC46           (*(volatile uint32_t *)(_EVIC_BASE + 0x0420u))
#define IPC46CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0424u))
#define IPC46SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0428u))
#define IPC46INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x042Cu))

/* IPC47: 0x0140 + 47*0x10 = 0x0430 */
#define IPC47           (*(volatile uint32_t *)(_EVIC_BASE + 0x0430u))
#define IPC47CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0434u))
#define IPC47SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0438u))
#define IPC47INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x043Cu))

/* IPC50: 0x0140 + 50*0x10 = 0x0460 */
#define IPC50           (*(volatile uint32_t *)(_EVIC_BASE + 0x0460u))
#define IPC50CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0464u))
#define IPC50SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0468u))
#define IPC50INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x046Cu))

/* IPC51: 0x0140 + 51*0x10 = 0x0470 */
#define IPC51           (*(volatile uint32_t *)(_EVIC_BASE + 0x0470u))
#define IPC51CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0474u))
#define IPC51SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0478u))
#define IPC51INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x047Cu))

/* IPC53: 0x0140 + 53*0x10 = 0x0490 */
#define IPC53           (*(volatile uint32_t *)(_EVIC_BASE + 0x0490u))
#define IPC53CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x0494u))
#define IPC53SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x0498u))
#define IPC53INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x049Cu))

/* IPC54: 0x0140 + 54*0x10 = 0x04A0 */
#define IPC54           (*(volatile uint32_t *)(_EVIC_BASE + 0x04A0u))
#define IPC54CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x04A4u))
#define IPC54SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x04A8u))
#define IPC54INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x04ACu))

/* IPC56: 0x0140 + 56*0x10 = 0x04C0 */
#define IPC56           (*(volatile uint32_t *)(_EVIC_BASE + 0x04C0u))
#define IPC56CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x04C4u))
#define IPC56SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x04C8u))
#define IPC56INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x04CCu))

/* PRISS — Priority Shadow Select register (offset 0x10 from EVIC base) */
#define PRISS           (*(volatile uint32_t *)(_EVIC_BASE + 0x0010u))

/* INTCON register (multi-vector mode control) */
#define INTCON          (*(volatile uint32_t *)(_EVIC_BASE + 0x0000u))
#define INTCONCLR       (*(volatile uint32_t *)(_EVIC_BASE + 0x0004u))
#define INTCONSET       (*(volatile uint32_t *)(_EVIC_BASE + 0x0008u))
#define INTCONINV       (*(volatile uint32_t *)(_EVIC_BASE + 0x000Cu))
#define _INTCON_MVEC_MASK   0x00001000u   /* bit 12: multi-vector mode */

/* IPC2 offset = 0x0140 + 2*0x10 = 0x0160 */
#define IPC2            (*(volatile uint32_t *)(_EVIC_BASE + 0x0160u))
#define IPC2CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0164u))
#define IPC2SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0168u))
#define IPC2INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x016Cu))

/* IPC3 offset = 0x0170 */
#define IPC3            (*(volatile uint32_t *)(_EVIC_BASE + 0x0170u))
#define IPC3CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0174u))
#define IPC3SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0178u))
#define IPC3INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x017Cu))

/* IPC4 offset = 0x0180 */
#define IPC4            (*(volatile uint32_t *)(_EVIC_BASE + 0x0180u))
#define IPC4CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0184u))
#define IPC4SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0188u))
#define IPC4INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x018Cu))

/* IPC6 offset = 0x01A0 */
#define IPC6            (*(volatile uint32_t *)(_EVIC_BASE + 0x01A0u))
#define IPC6CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01A4u))
#define IPC6SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01A8u))
#define IPC6INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01ACu))

/* IPC7 offset = 0x01B0 */
#define IPC7            (*(volatile uint32_t *)(_EVIC_BASE + 0x01B0u))
#define IPC7CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01B4u))
#define IPC7SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01B8u))
#define IPC7INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01BCu))

/* IFS0 / IEC0 named bit masks (source number = bit position) */
#define _IFS0_T1IF_MASK         (1u << 4)   /* Timer1 = EVIC source 4  */
#define _IEC0_T1IE_MASK         (1u << 4)   /* Timer1 = EVIC source 4  */
#define _IFS0_T2IF_MASK         (1u << 9)   /* Timer2 = EVIC source 9  */
#define _IEC0_T2IE_MASK         (1u << 9)
#define _IFS0_T3IF_MASK         (1u << 14)  /* Timer3 = EVIC source 14 */
#define _IEC0_T3IE_MASK         (1u << 14)
#define _IFS0_T4IF_MASK         (1u << 19)  /* Timer4 = EVIC source 19 */
#define _IEC0_T4IE_MASK         (1u << 19)
#define _IFS0_T5IF_MASK         (1u << 24)  /* Timer5 = EVIC source 24 */
#define _IEC0_T5IE_MASK         (1u << 24)
#define _IFS0_T6IF_MASK         (1u << 28)  /* Timer6 = EVIC source 28 */
#define _IEC0_T6IE_MASK         (1u << 28)

/* IFS1 / IEC1 — EVIC sources 32-63 (IFS1 bit = source - 32) */
#define _IFS1_T7IF_MASK         (1u << 0)   /* Timer7 = EVIC source 32 */
#define _IEC1_T7IE_MASK         (1u << 0)
#define _IFS1_T8IF_MASK         (1u << 4)   /* Timer8 = EVIC source 36 */
#define _IEC1_T8IE_MASK         (1u << 4)
#define _IFS1_T9IF_MASK         (1u << 8)   /* Timer9 = EVIC source 40 */
#define _IEC1_T9IE_MASK         (1u << 8)

/* UART2 interrupt bits in IFS1/IEC1: U2E=bit24, U2RX=bit25, U2TX=bit26 */
#define _IFS1_U2EIF_MASK        0x01000000u  /* source 56 */
#define _IFS1_U2RXIF_MASK       0x02000000u  /* source 57 */
#define _IFS1_U2TXIF_MASK       0x04000000u  /* source 58 */
#define _IEC1_U2EIE_MASK        0x01000000u
#define _IEC1_U2RXIE_MASK       0x02000000u
#define _IEC1_U2TXIE_MASK       0x04000000u

#define _IFS0_CS0IF_MASK        (1u << 1)   /* CoreSW0 = EVIC source 1 */
#define _IEC0_CS0IE_MASK        (1u << 1)
#define _IEC0_CS0IE_POSITION    1

/* IC1/IC2 IFS0/IEC0 masks (IC1E=5, IC1=6, IC2E=10, IC2=11) */
#define _IFS0_IC1EIF_MASK       (1u << 5)   /* IC1 error  = EVIC source 5  */
#define _IEC0_IC1EIE_MASK       (1u << 5)
#define _IFS0_IC1IF_MASK        (1u << 6)   /* IC1 capture = EVIC source 6 */
#define _IEC0_IC1IE_MASK        (1u << 6)
#define _IFS0_IC2EIF_MASK       (1u << 10)  /* IC2 error  = EVIC source 10 */
#define _IEC0_IC2EIE_MASK       (1u << 10)
#define _IFS0_IC2IF_MASK        (1u << 11)  /* IC2 capture = EVIC source 11 */
#define _IEC0_IC2IE_MASK        (1u << 11)
#define _IPC0_CS0IP_MASK        (0x7u << 10)
#define _IPC0_CS0IP_POSITION    10

/* -----------------------------------------------------------------------
 * Timer1 registers  (base 0xBF820000, stride 0x10)
 * ----------------------------------------------------------------------- */

#define _T1_BASE        0xBF820000u

#define T1CON           (*(volatile uint32_t *)(_T1_BASE + 0x000u))
#define T1CONCLR        (*(volatile uint32_t *)(_T1_BASE + 0x004u))
#define T1CONSET        (*(volatile uint32_t *)(_T1_BASE + 0x008u))
#define TMR1            (*(volatile uint32_t *)(_T1_BASE + 0x010u))
#define PR1             (*(volatile uint32_t *)(_T1_BASE + 0x020u))

/* T1CON bit-field struct */
typedef union {
    struct {
        uint32_t            : 1;   /* bit 0  reserved */
        uint32_t TCS        : 1;   /* bit 1  clock source */
        uint32_t TSYNC      : 1;   /* bit 2  sync */
        uint32_t            : 1;   /* bit 3  reserved */
        uint32_t TCKPS      : 2;   /* bits [5:4]  prescaler: 0=1:1,1=1:8,2=1:64,3=1:256 */
        uint32_t            : 1;   /* bit 6  reserved */
        uint32_t TGATE      : 1;   /* bit 7  gated accumulation */
        uint32_t            : 5;   /* bits [12:8] reserved */
        uint32_t SIDL       : 1;   /* bit 13 stop-in-idle */
        uint32_t            : 1;   /* bit 14 reserved */
        uint32_t ON         : 1;   /* bit 15 timer on — Microchip also calls this TON */
        uint32_t            : 16;
    };
    /* Alias so port.c's T1CONbits.TON works too */
    struct {
        uint32_t            : 15;
        uint32_t TON        : 1;
        uint32_t            : 16;
    };
    uint32_t w;
} __T1CON_t;

#define T1CONbits       (*(volatile __T1CON_t *)(_T1_BASE + 0x000u))

#define _T1CON_ON_MASK  (1u << 15)  /* Timer ON bit */

/* -----------------------------------------------------------------------
 * Timer2-9 registers  (Type B: T2,T4,T6,T8 — Type C: T3,T5,T7,T9)
 * Each block is 0x200 bytes apart from the previous.
 * Register layout: TxCON+0x00, TMRx+0x10, PRx+0x20  (SET+4/CLR+8 each)
 * ----------------------------------------------------------------------- */

#define _T2_BASE   0xBF820200u
#define T2CON      (*(volatile uint32_t *)(_T2_BASE + 0x000u))
#define T2CONCLR   (*(volatile uint32_t *)(_T2_BASE + 0x004u))
#define T2CONSET   (*(volatile uint32_t *)(_T2_BASE + 0x008u))
#define TMR2       (*(volatile uint32_t *)(_T2_BASE + 0x010u))
#define PR2        (*(volatile uint32_t *)(_T2_BASE + 0x020u))
#define _T2CON_ON_MASK  (1u << 15)

#define _T3_BASE   0xBF820400u
#define T3CON      (*(volatile uint32_t *)(_T3_BASE + 0x000u))
#define T3CONCLR   (*(volatile uint32_t *)(_T3_BASE + 0x004u))
#define T3CONSET   (*(volatile uint32_t *)(_T3_BASE + 0x008u))
#define TMR3       (*(volatile uint32_t *)(_T3_BASE + 0x010u))
#define PR3        (*(volatile uint32_t *)(_T3_BASE + 0x020u))
#define _T3CON_ON_MASK  (1u << 15)

#define _T4_BASE   0xBF820600u
#define T4CON      (*(volatile uint32_t *)(_T4_BASE + 0x000u))
#define T4CONCLR   (*(volatile uint32_t *)(_T4_BASE + 0x004u))
#define T4CONSET   (*(volatile uint32_t *)(_T4_BASE + 0x008u))
#define TMR4       (*(volatile uint32_t *)(_T4_BASE + 0x010u))
#define PR4        (*(volatile uint32_t *)(_T4_BASE + 0x020u))
#define _T4CON_ON_MASK  (1u << 15)

#define _T5_BASE   0xBF820800u
#define T5CON      (*(volatile uint32_t *)(_T5_BASE + 0x000u))
#define T5CONCLR   (*(volatile uint32_t *)(_T5_BASE + 0x004u))
#define T5CONSET   (*(volatile uint32_t *)(_T5_BASE + 0x008u))
#define TMR5       (*(volatile uint32_t *)(_T5_BASE + 0x010u))
#define PR5        (*(volatile uint32_t *)(_T5_BASE + 0x020u))
#define _T5CON_ON_MASK  (1u << 15)

#define _T6_BASE   0xBF820A00u
#define T6CON      (*(volatile uint32_t *)(_T6_BASE + 0x000u))
#define T6CONCLR   (*(volatile uint32_t *)(_T6_BASE + 0x004u))
#define T6CONSET   (*(volatile uint32_t *)(_T6_BASE + 0x008u))
#define TMR6       (*(volatile uint32_t *)(_T6_BASE + 0x010u))
#define PR6        (*(volatile uint32_t *)(_T6_BASE + 0x020u))
#define _T6CON_ON_MASK  (1u << 15)

#define _T7_BASE   0xBF820C00u
#define T7CON      (*(volatile uint32_t *)(_T7_BASE + 0x000u))
#define T7CONCLR   (*(volatile uint32_t *)(_T7_BASE + 0x004u))
#define T7CONSET   (*(volatile uint32_t *)(_T7_BASE + 0x008u))
#define TMR7       (*(volatile uint32_t *)(_T7_BASE + 0x010u))
#define PR7        (*(volatile uint32_t *)(_T7_BASE + 0x020u))
#define _T7CON_ON_MASK  (1u << 15)

#define _T8_BASE   0xBF820E00u
#define T8CON      (*(volatile uint32_t *)(_T8_BASE + 0x000u))
#define T8CONCLR   (*(volatile uint32_t *)(_T8_BASE + 0x004u))
#define T8CONSET   (*(volatile uint32_t *)(_T8_BASE + 0x008u))
#define TMR8       (*(volatile uint32_t *)(_T8_BASE + 0x010u))
#define PR8        (*(volatile uint32_t *)(_T8_BASE + 0x020u))
#define _T8CON_ON_MASK  (1u << 15)

#define _T9_BASE   0xBF821000u
#define T9CON      (*(volatile uint32_t *)(_T9_BASE + 0x000u))
#define T9CONCLR   (*(volatile uint32_t *)(_T9_BASE + 0x004u))
#define T9CONSET   (*(volatile uint32_t *)(_T9_BASE + 0x008u))
#define TMR9       (*(volatile uint32_t *)(_T9_BASE + 0x010u))
#define PR9        (*(volatile uint32_t *)(_T9_BASE + 0x020u))
#define _T9CON_ON_MASK  (1u << 15)

/* -----------------------------------------------------------------------
 * Input Capture IC1–IC2 registers  (§18, DS60001519E)
 * IC1: 0xBF822000, IC2: 0xBF822200 (0x200 stride)
 * Register layout: ICxCON+0x00, ICxBUF+0x10 (CLR+4/SET+8/INV+C)
 * ----------------------------------------------------------------------- */

#define _IC1_BASE   0xBF822000u
#define IC1CON      (*(volatile uint32_t *)(_IC1_BASE + 0x000u))
#define IC1CONCLR   (*(volatile uint32_t *)(_IC1_BASE + 0x004u))
#define IC1CONSET   (*(volatile uint32_t *)(_IC1_BASE + 0x008u))
#define IC1CONINV   (*(volatile uint32_t *)(_IC1_BASE + 0x00Cu))
#define IC1BUF      (*(volatile uint32_t *)(_IC1_BASE + 0x010u))
#define _IC1CON_ON_MASK     (1u << 15)
#define _IC1CON_ON_POSITION 15

#define _IC2_BASE   0xBF822200u
#define IC2CON      (*(volatile uint32_t *)(_IC2_BASE + 0x000u))
#define IC2CONCLR   (*(volatile uint32_t *)(_IC2_BASE + 0x004u))
#define IC2CONSET   (*(volatile uint32_t *)(_IC2_BASE + 0x008u))
#define IC2CONINV   (*(volatile uint32_t *)(_IC2_BASE + 0x00Cu))
#define IC2BUF      (*(volatile uint32_t *)(_IC2_BASE + 0x010u))
#define _IC2CON_ON_MASK     (1u << 15)
#define _IC2CON_ON_POSITION 15

#define _IC16_BASE  0xBF843E00u
#define IC16CON     (*(volatile uint32_t *)(_IC16_BASE + 0x000u))
#define IC16CONCLR  (*(volatile uint32_t *)(_IC16_BASE + 0x004u))
#define IC16CONSET  (*(volatile uint32_t *)(_IC16_BASE + 0x008u))
#define IC16CONINV  (*(volatile uint32_t *)(_IC16_BASE + 0x00Cu))
#define IC16BUF     (*(volatile uint32_t *)(_IC16_BASE + 0x010u))
#define _IC16CON_ON_MASK     (1u << 15)
#define _IC16CON_ON_POSITION 15

/* -----------------------------------------------------------------------
 * Output Compare OC1–OC3 registers  (§19, DS60001519E)
 * OC1: 0xBF824000, OC2: 0xBF824200, OC3: 0xBF824400 (0x200 stride)
 * Register layout: OCxCON+0x00, OCxR+0x10, OCxRS+0x20 (CLR+4/SET+8/INV+C)
 * ----------------------------------------------------------------------- */

#define _OC1_BASE   0xBF824000u
#define OC1CON      (*(volatile uint32_t *)(_OC1_BASE + 0x000u))
#define OC1CONCLR   (*(volatile uint32_t *)(_OC1_BASE + 0x004u))
#define OC1CONSET   (*(volatile uint32_t *)(_OC1_BASE + 0x008u))
#define OC1CONINV   (*(volatile uint32_t *)(_OC1_BASE + 0x00Cu))
#define OC1R        (*(volatile uint32_t *)(_OC1_BASE + 0x010u))
#define OC1RS       (*(volatile uint32_t *)(_OC1_BASE + 0x020u))
#define _OC1CON_ON_MASK     (1u << 15)
#define _OC1CON_ON_POSITION 15

#define _OC2_BASE   0xBF824200u
#define OC2CON      (*(volatile uint32_t *)(_OC2_BASE + 0x000u))
#define OC2CONCLR   (*(volatile uint32_t *)(_OC2_BASE + 0x004u))
#define OC2CONSET   (*(volatile uint32_t *)(_OC2_BASE + 0x008u))
#define OC2CONINV   (*(volatile uint32_t *)(_OC2_BASE + 0x00Cu))
#define OC2R        (*(volatile uint32_t *)(_OC2_BASE + 0x010u))
#define OC2RS       (*(volatile uint32_t *)(_OC2_BASE + 0x020u))
#define _OC2CON_ON_MASK     (1u << 15)
#define _OC2CON_ON_POSITION 15

#define _OC3_BASE   0xBF824400u
#define OC3CON      (*(volatile uint32_t *)(_OC3_BASE + 0x000u))
#define OC3CONCLR   (*(volatile uint32_t *)(_OC3_BASE + 0x004u))
#define OC3CONSET   (*(volatile uint32_t *)(_OC3_BASE + 0x008u))
#define OC3CONINV   (*(volatile uint32_t *)(_OC3_BASE + 0x00Cu))
#define OC3R        (*(volatile uint32_t *)(_OC3_BASE + 0x010u))
#define OC3RS       (*(volatile uint32_t *)(_OC3_BASE + 0x020u))
#define _OC3CON_ON_MASK     (1u << 15)
#define _OC3CON_ON_POSITION 15

/* OC4–OC9: 0xBF824600–0xBF825000, 0x200 stride */
#define _OC4_BASE   0xBF824600u
#define OC4CON      (*(volatile uint32_t *)(_OC4_BASE + 0x000u))
#define OC4CONCLR   (*(volatile uint32_t *)(_OC4_BASE + 0x004u))
#define OC4CONSET   (*(volatile uint32_t *)(_OC4_BASE + 0x008u))
#define OC4CONINV   (*(volatile uint32_t *)(_OC4_BASE + 0x00Cu))
#define OC4R        (*(volatile uint32_t *)(_OC4_BASE + 0x010u))
#define OC4RS       (*(volatile uint32_t *)(_OC4_BASE + 0x020u))
#define _OC4CON_ON_MASK     (1u << 15)
#define _OC4CON_ON_POSITION 15

#define _OC5_BASE   0xBF824800u
#define OC5CON      (*(volatile uint32_t *)(_OC5_BASE + 0x000u))
#define OC5CONCLR   (*(volatile uint32_t *)(_OC5_BASE + 0x004u))
#define OC5CONSET   (*(volatile uint32_t *)(_OC5_BASE + 0x008u))
#define OC5CONINV   (*(volatile uint32_t *)(_OC5_BASE + 0x00Cu))
#define OC5R        (*(volatile uint32_t *)(_OC5_BASE + 0x010u))
#define OC5RS       (*(volatile uint32_t *)(_OC5_BASE + 0x020u))
#define _OC5CON_ON_MASK     (1u << 15)
#define _OC5CON_ON_POSITION 15

#define _OC6_BASE   0xBF824A00u
#define OC6CON      (*(volatile uint32_t *)(_OC6_BASE + 0x000u))
#define OC6CONCLR   (*(volatile uint32_t *)(_OC6_BASE + 0x004u))
#define OC6CONSET   (*(volatile uint32_t *)(_OC6_BASE + 0x008u))
#define OC6CONINV   (*(volatile uint32_t *)(_OC6_BASE + 0x00Cu))
#define OC6R        (*(volatile uint32_t *)(_OC6_BASE + 0x010u))
#define OC6RS       (*(volatile uint32_t *)(_OC6_BASE + 0x020u))
#define _OC6CON_ON_MASK     (1u << 15)
#define _OC6CON_ON_POSITION 15

#define _OC7_BASE   0xBF824C00u
#define OC7CON      (*(volatile uint32_t *)(_OC7_BASE + 0x000u))
#define OC7CONCLR   (*(volatile uint32_t *)(_OC7_BASE + 0x004u))
#define OC7CONSET   (*(volatile uint32_t *)(_OC7_BASE + 0x008u))
#define OC7CONINV   (*(volatile uint32_t *)(_OC7_BASE + 0x00Cu))
#define OC7R        (*(volatile uint32_t *)(_OC7_BASE + 0x010u))
#define OC7RS       (*(volatile uint32_t *)(_OC7_BASE + 0x020u))
#define _OC7CON_ON_MASK     (1u << 15)
#define _OC7CON_ON_POSITION 15

#define _OC8_BASE   0xBF824E00u
#define OC8CON      (*(volatile uint32_t *)(_OC8_BASE + 0x000u))
#define OC8CONCLR   (*(volatile uint32_t *)(_OC8_BASE + 0x004u))
#define OC8CONSET   (*(volatile uint32_t *)(_OC8_BASE + 0x008u))
#define OC8CONINV   (*(volatile uint32_t *)(_OC8_BASE + 0x00Cu))
#define OC8R        (*(volatile uint32_t *)(_OC8_BASE + 0x010u))
#define OC8RS       (*(volatile uint32_t *)(_OC8_BASE + 0x020u))
#define _OC8CON_ON_MASK     (1u << 15)
#define _OC8CON_ON_POSITION 15

#define _OC9_BASE   0xBF825000u
#define OC9CON      (*(volatile uint32_t *)(_OC9_BASE + 0x000u))
#define OC9CONCLR   (*(volatile uint32_t *)(_OC9_BASE + 0x004u))
#define OC9CONSET   (*(volatile uint32_t *)(_OC9_BASE + 0x008u))
#define OC9CONINV   (*(volatile uint32_t *)(_OC9_BASE + 0x00Cu))
#define OC9R        (*(volatile uint32_t *)(_OC9_BASE + 0x010u))
#define OC9RS       (*(volatile uint32_t *)(_OC9_BASE + 0x020u))
#define _OC9CON_ON_MASK     (1u << 15)
#define _OC9CON_ON_POSITION 15

/* OC10–OC16: base 0xBF845200, 0x200 stride (second SFR bank) */
#define _OC10_BASE  0xBF845200u
#define OC10CON     (*(volatile uint32_t *)(_OC10_BASE + 0x000u))
#define OC10CONCLR  (*(volatile uint32_t *)(_OC10_BASE + 0x004u))
#define OC10CONSET  (*(volatile uint32_t *)(_OC10_BASE + 0x008u))
#define OC10CONINV  (*(volatile uint32_t *)(_OC10_BASE + 0x00Cu))
#define OC10R       (*(volatile uint32_t *)(_OC10_BASE + 0x010u))
#define OC10RS      (*(volatile uint32_t *)(_OC10_BASE + 0x020u))
#define _OC10CON_ON_MASK     (1u << 15)
#define _OC10CON_ON_POSITION 15

#define _OC11_BASE  0xBF845400u
#define OC11CON     (*(volatile uint32_t *)(_OC11_BASE + 0x000u))
#define OC11CONCLR  (*(volatile uint32_t *)(_OC11_BASE + 0x004u))
#define OC11CONSET  (*(volatile uint32_t *)(_OC11_BASE + 0x008u))
#define OC11CONINV  (*(volatile uint32_t *)(_OC11_BASE + 0x00Cu))
#define OC11R       (*(volatile uint32_t *)(_OC11_BASE + 0x010u))
#define OC11RS      (*(volatile uint32_t *)(_OC11_BASE + 0x020u))
#define _OC11CON_ON_MASK     (1u << 15)
#define _OC11CON_ON_POSITION 15

#define _OC12_BASE  0xBF845600u
#define OC12CON     (*(volatile uint32_t *)(_OC12_BASE + 0x000u))
#define OC12CONCLR  (*(volatile uint32_t *)(_OC12_BASE + 0x004u))
#define OC12CONSET  (*(volatile uint32_t *)(_OC12_BASE + 0x008u))
#define OC12CONINV  (*(volatile uint32_t *)(_OC12_BASE + 0x00Cu))
#define OC12R       (*(volatile uint32_t *)(_OC12_BASE + 0x010u))
#define OC12RS      (*(volatile uint32_t *)(_OC12_BASE + 0x020u))
#define _OC12CON_ON_MASK     (1u << 15)
#define _OC12CON_ON_POSITION 15

#define _OC13_BASE  0xBF845800u
#define OC13CON     (*(volatile uint32_t *)(_OC13_BASE + 0x000u))
#define OC13CONCLR  (*(volatile uint32_t *)(_OC13_BASE + 0x004u))
#define OC13CONSET  (*(volatile uint32_t *)(_OC13_BASE + 0x008u))
#define OC13CONINV  (*(volatile uint32_t *)(_OC13_BASE + 0x00Cu))
#define OC13R       (*(volatile uint32_t *)(_OC13_BASE + 0x010u))
#define OC13RS      (*(volatile uint32_t *)(_OC13_BASE + 0x020u))
#define _OC13CON_ON_MASK     (1u << 15)
#define _OC13CON_ON_POSITION 15

#define _OC14_BASE  0xBF845A00u
#define OC14CON     (*(volatile uint32_t *)(_OC14_BASE + 0x000u))
#define OC14CONCLR  (*(volatile uint32_t *)(_OC14_BASE + 0x004u))
#define OC14CONSET  (*(volatile uint32_t *)(_OC14_BASE + 0x008u))
#define OC14CONINV  (*(volatile uint32_t *)(_OC14_BASE + 0x00Cu))
#define OC14R       (*(volatile uint32_t *)(_OC14_BASE + 0x010u))
#define OC14RS      (*(volatile uint32_t *)(_OC14_BASE + 0x020u))
#define _OC14CON_ON_MASK     (1u << 15)
#define _OC14CON_ON_POSITION 15

#define _OC15_BASE  0xBF845C00u
#define OC15CON     (*(volatile uint32_t *)(_OC15_BASE + 0x000u))
#define OC15CONCLR  (*(volatile uint32_t *)(_OC15_BASE + 0x004u))
#define OC15CONSET  (*(volatile uint32_t *)(_OC15_BASE + 0x008u))
#define OC15CONINV  (*(volatile uint32_t *)(_OC15_BASE + 0x00Cu))
#define OC15R       (*(volatile uint32_t *)(_OC15_BASE + 0x010u))
#define OC15RS      (*(volatile uint32_t *)(_OC15_BASE + 0x020u))
#define _OC15CON_ON_MASK     (1u << 15)
#define _OC15CON_ON_POSITION 15

#define _OC16_BASE  0xBF845E00u
#define OC16CON     (*(volatile uint32_t *)(_OC16_BASE + 0x000u))
#define OC16CONCLR  (*(volatile uint32_t *)(_OC16_BASE + 0x004u))
#define OC16CONSET  (*(volatile uint32_t *)(_OC16_BASE + 0x008u))
#define OC16CONINV  (*(volatile uint32_t *)(_OC16_BASE + 0x00Cu))
#define OC16R       (*(volatile uint32_t *)(_OC16_BASE + 0x010u))
#define OC16RS      (*(volatile uint32_t *)(_OC16_BASE + 0x020u))
#define _OC16CON_ON_MASK     (1u << 15)
#define _OC16CON_ON_POSITION 15

/* -----------------------------------------------------------------------
 * WDT registers  (base 0xBF800C00, DS60001519E §17, Register 17-1)
 * WDTCON layout:
 *   bits[31:16] WDTCLRKEY  — write 0x5743 (16-bit to WDTCON+2) to clear
 *   bit  15     ON         — WDT enable
 *   bits[14:13] unimplemented
 *   bits[12:8]  RUNDIV[4:0]— read-only, post-scaler ratio (run mode)
 *   bits[7:6]   unimplemented
 *   bits[5:1]   SLPDIV[4:0]— read-only, post-scaler ratio (sleep mode)
 *   bit  0      WDTWINEN   — windowed mode enable
 * ----------------------------------------------------------------------- */

#define _WDT_BASE       0xBF800C00u

#define WDTCON          (*(volatile uint32_t *)(_WDT_BASE + 0x000u))
#define WDTCONCLR       (*(volatile uint32_t *)(_WDT_BASE + 0x004u))
#define WDTCONSET       (*(volatile uint32_t *)(_WDT_BASE + 0x008u))
#define WDTCONINV       (*(volatile uint32_t *)(_WDT_BASE + 0x00Cu))

typedef union {
    struct {
        uint32_t WDTWINEN   : 1;   /* bit 0:     windowed mode enable */
        uint32_t SLPDIV     : 5;   /* bits[5:1]: sleep post-scaler (R/O) */
        uint32_t            : 2;   /* bits[7:6]: unimplemented */
        uint32_t RUNDIV     : 5;   /* bits[12:8]:run post-scaler (R/O) */
        uint32_t            : 2;   /* bits[14:13]:unimplemented */
        uint32_t ON         : 1;   /* bit 15:    WDT enable */
        uint32_t WDTCLRKEY  : 16;  /* bits[31:16]:clear key (write 0x5743) */
    };
    uint32_t w;
} __WDTCON_t;

#define WDTCONbits      (*(volatile __WDTCON_t *)(_WDT_BASE + 0x000u))

/* -----------------------------------------------------------------------
 * CFG / PMD / SYSKEY registers (§6, DS60001519E)
 * Base: 0xBF800000  (System Configuration)
 * ----------------------------------------------------------------------- */

#define _CFG_BASE       0xBF800000u

/* CFGCON — Configuration Control */
#define CFGCON          (*(volatile uint32_t *)(_CFG_BASE + 0x000u))
#define CFGCONCLR       (*(volatile uint32_t *)(_CFG_BASE + 0x004u))
#define CFGCONSET       (*(volatile uint32_t *)(_CFG_BASE + 0x008u))
#define CFGCONINV       (*(volatile uint32_t *)(_CFG_BASE + 0x00Cu))

typedef union {
    struct {
        uint32_t            : 28;  /* bits[27:0] */
        uint32_t PGLOCK     : 1;   /* bit 28: Permission-group lock */
        uint32_t PMDLOCK    : 1;   /* bit 29: PMD lock */
        uint32_t IOLOCK     : 1;   /* bit 30: I/O lock */
        uint32_t            : 1;   /* bit 31 */
    };
    uint32_t w;
} __CFGCON_t;

#define CFGCONbits      (*(volatile __CFGCON_t *)(_CFG_BASE + 0x000u))

/* SYSKEY — System Unlock Key (always reads 0) */
#define SYSKEY          (*(volatile uint32_t *)(_CFG_BASE + 0x030u))

/* PMD1–PMD7 — Peripheral Module Disable */
#define PMD1            (*(volatile uint32_t *)(_CFG_BASE + 0x040u))
#define PMD1CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x044u))
#define PMD1SET         (*(volatile uint32_t *)(_CFG_BASE + 0x048u))
#define PMD1INV         (*(volatile uint32_t *)(_CFG_BASE + 0x04Cu))

#define PMD2            (*(volatile uint32_t *)(_CFG_BASE + 0x050u))
#define PMD2CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x054u))
#define PMD2SET         (*(volatile uint32_t *)(_CFG_BASE + 0x058u))
#define PMD2INV         (*(volatile uint32_t *)(_CFG_BASE + 0x05Cu))

#define PMD3            (*(volatile uint32_t *)(_CFG_BASE + 0x060u))
#define PMD3CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x064u))
#define PMD3SET         (*(volatile uint32_t *)(_CFG_BASE + 0x068u))
#define PMD3INV         (*(volatile uint32_t *)(_CFG_BASE + 0x06Cu))

#define PMD4            (*(volatile uint32_t *)(_CFG_BASE + 0x070u))
#define PMD4CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x074u))
#define PMD4SET         (*(volatile uint32_t *)(_CFG_BASE + 0x078u))
#define PMD4INV         (*(volatile uint32_t *)(_CFG_BASE + 0x07Cu))

#define PMD5            (*(volatile uint32_t *)(_CFG_BASE + 0x080u))
#define PMD5CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x084u))
#define PMD5SET         (*(volatile uint32_t *)(_CFG_BASE + 0x088u))
#define PMD5INV         (*(volatile uint32_t *)(_CFG_BASE + 0x08Cu))

#define PMD6            (*(volatile uint32_t *)(_CFG_BASE + 0x090u))
#define PMD6CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x094u))
#define PMD6SET         (*(volatile uint32_t *)(_CFG_BASE + 0x098u))
#define PMD6INV         (*(volatile uint32_t *)(_CFG_BASE + 0x09Cu))

#define PMD7            (*(volatile uint32_t *)(_CFG_BASE + 0x0A0u))
#define PMD7CLR         (*(volatile uint32_t *)(_CFG_BASE + 0x0A4u))
#define PMD7SET         (*(volatile uint32_t *)(_CFG_BASE + 0x0A8u))
#define PMD7INV         (*(volatile uint32_t *)(_CFG_BASE + 0x0ACu))

/* -----------------------------------------------------------------------
 * CRU — Clock Reference Unit registers (§9, DS60001519E)
 * Base: 0xBF801200  (CRU offset from SFR base)
 * ----------------------------------------------------------------------- */

#define _CRU_BASE       0xBF801200u

/* OSCCON — Oscillator Control */
#define OSCCON          (*(volatile uint32_t *)(_CRU_BASE + 0x00u))
#define OSCCONCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x04u))
#define OSCCONSET       (*(volatile uint32_t *)(_CRU_BASE + 0x08u))
#define OSCCONINV       (*(volatile uint32_t *)(_CRU_BASE + 0x0Cu))

/* OSCTUN — Oscillator Tuning */
#define OSCTUN          (*(volatile uint32_t *)(_CRU_BASE + 0x10u))
#define OSCTUNCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x14u))
#define OSCTUNSET       (*(volatile uint32_t *)(_CRU_BASE + 0x18u))
#define OSCTUNINV       (*(volatile uint32_t *)(_CRU_BASE + 0x1Cu))

/* SPLLCON — System PLL Control */
#define SPLLCON         (*(volatile uint32_t *)(_CRU_BASE + 0x20u))
#define SPLLCONCLR      (*(volatile uint32_t *)(_CRU_BASE + 0x24u))
#define SPLLCONSET      (*(volatile uint32_t *)(_CRU_BASE + 0x28u))
#define SPLLCONINV      (*(volatile uint32_t *)(_CRU_BASE + 0x2Cu))

/* UPLLCON — USB PLL Control */
#define UPLLCON         (*(volatile uint32_t *)(_CRU_BASE + 0x30u))
#define UPLLCONCLR      (*(volatile uint32_t *)(_CRU_BASE + 0x34u))
#define UPLLCONSET      (*(volatile uint32_t *)(_CRU_BASE + 0x38u))
#define UPLLCONINV      (*(volatile uint32_t *)(_CRU_BASE + 0x3Cu))

/* RCON / RSWRST / RNMICON / PWRCON — Reset control */
#define RCON            (*(volatile uint32_t *)(_CRU_BASE + 0x40u))
#define RCONCLR         (*(volatile uint32_t *)(_CRU_BASE + 0x44u))
#define RCONSET         (*(volatile uint32_t *)(_CRU_BASE + 0x48u))
#define RCONINV         (*(volatile uint32_t *)(_CRU_BASE + 0x4Cu))

#define RSWRST          (*(volatile uint32_t *)(_CRU_BASE + 0x50u))
#define RSWRSTCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x54u))
#define RSWRSTSET       (*(volatile uint32_t *)(_CRU_BASE + 0x58u))
#define RSWRSTINV       (*(volatile uint32_t *)(_CRU_BASE + 0x5Cu))

#define RNMICON         (*(volatile uint32_t *)(_CRU_BASE + 0x60u))
#define RNMICONCLR      (*(volatile uint32_t *)(_CRU_BASE + 0x64u))
#define RNMICONSET      (*(volatile uint32_t *)(_CRU_BASE + 0x68u))
#define RNMICONINV      (*(volatile uint32_t *)(_CRU_BASE + 0x6Cu))

#define PWRCON          (*(volatile uint32_t *)(_CRU_BASE + 0x70u))
#define PWRCONCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x74u))
#define PWRCONSET       (*(volatile uint32_t *)(_CRU_BASE + 0x78u))
#define PWRCONINV       (*(volatile uint32_t *)(_CRU_BASE + 0x7Cu))

#define _RSWRST_SWRST_MASK      0x00000001u

/* REFO1CON / REFO1TRIM — Reference Clock Output 1 */
#define REFO1CON        (*(volatile uint32_t *)(_CRU_BASE + 0x80u))
#define REFO1CONCLR     (*(volatile uint32_t *)(_CRU_BASE + 0x84u))
#define REFO1CONSET     (*(volatile uint32_t *)(_CRU_BASE + 0x88u))
#define REFO1CONINV     (*(volatile uint32_t *)(_CRU_BASE + 0x8Cu))
#define REFO1TRIM       (*(volatile uint32_t *)(_CRU_BASE + 0x90u))
#define REFO1TRIMCLR    (*(volatile uint32_t *)(_CRU_BASE + 0x94u))
#define REFO1TRIMSET    (*(volatile uint32_t *)(_CRU_BASE + 0x98u))
#define REFO1TRIMINV    (*(volatile uint32_t *)(_CRU_BASE + 0x9Cu))

/* REFO2CON / REFO2TRIM — Reference Clock Output 2 */
#define REFO2CON        (*(volatile uint32_t *)(_CRU_BASE + 0xA0u))
#define REFO2CONCLR     (*(volatile uint32_t *)(_CRU_BASE + 0xA4u))
#define REFO2CONSET     (*(volatile uint32_t *)(_CRU_BASE + 0xA8u))
#define REFO2CONINV     (*(volatile uint32_t *)(_CRU_BASE + 0xACu))
#define REFO2TRIM       (*(volatile uint32_t *)(_CRU_BASE + 0xB0u))
#define REFO2TRIMCLR    (*(volatile uint32_t *)(_CRU_BASE + 0xB4u))
#define REFO2TRIMSET    (*(volatile uint32_t *)(_CRU_BASE + 0xB8u))
#define REFO2TRIMINV    (*(volatile uint32_t *)(_CRU_BASE + 0xBCu))

/* REFO3CON / REFO3TRIM — Reference Clock Output 3 */
#define REFO3CON        (*(volatile uint32_t *)(_CRU_BASE + 0xC0u))
#define REFO3CONCLR     (*(volatile uint32_t *)(_CRU_BASE + 0xC4u))
#define REFO3CONSET     (*(volatile uint32_t *)(_CRU_BASE + 0xC8u))
#define REFO3CONINV     (*(volatile uint32_t *)(_CRU_BASE + 0xCCu))
#define REFO3TRIM       (*(volatile uint32_t *)(_CRU_BASE + 0xD0u))
#define REFO3TRIMCLR    (*(volatile uint32_t *)(_CRU_BASE + 0xD4u))
#define REFO3TRIMSET    (*(volatile uint32_t *)(_CRU_BASE + 0xD8u))
#define REFO3TRIMINV    (*(volatile uint32_t *)(_CRU_BASE + 0xDCu))

/* REFO4CON / REFO4TRIM — Reference Clock Output 4 */
#define REFO4CON        (*(volatile uint32_t *)(_CRU_BASE + 0xE0u))
#define REFO4CONCLR     (*(volatile uint32_t *)(_CRU_BASE + 0xE4u))
#define REFO4CONSET     (*(volatile uint32_t *)(_CRU_BASE + 0xE8u))
#define REFO4CONINV     (*(volatile uint32_t *)(_CRU_BASE + 0xECu))
#define REFO4TRIM       (*(volatile uint32_t *)(_CRU_BASE + 0xF0u))
#define REFO4TRIMCLR    (*(volatile uint32_t *)(_CRU_BASE + 0xF4u))
#define REFO4TRIMSET    (*(volatile uint32_t *)(_CRU_BASE + 0xF8u))
#define REFO4TRIMINV    (*(volatile uint32_t *)(_CRU_BASE + 0xFCu))

/* PB1DIV–PB7DIV — Peripheral Bus Clock Dividers */
#define PB1DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x100u))
#define PB1DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x104u))
#define PB1DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x108u))
#define PB1DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x10Cu))

#define PB2DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x110u))
#define PB2DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x114u))
#define PB2DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x118u))
#define PB2DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x11Cu))

#define PB3DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x120u))
#define PB3DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x124u))
#define PB3DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x128u))
#define PB3DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x12Cu))

#define PB4DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x130u))
#define PB4DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x134u))
#define PB4DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x138u))
#define PB4DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x13Cu))

#define PB5DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x140u))
#define PB5DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x144u))
#define PB5DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x148u))
#define PB5DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x14Cu))

#define PB6DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x150u))
#define PB6DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x154u))
#define PB6DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x158u))
#define PB6DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x15Cu))

#define PB7DIV          (*(volatile uint32_t *)(_CRU_BASE + 0x160u))
#define PB7DIVCLR       (*(volatile uint32_t *)(_CRU_BASE + 0x164u))
#define PB7DIVSET       (*(volatile uint32_t *)(_CRU_BASE + 0x168u))
#define PB7DIVINV       (*(volatile uint32_t *)(_CRU_BASE + 0x16Cu))

/* PBxDIV bitfield struct */
typedef union {
    struct {
        uint32_t PBDIV      : 7;   /* bits[6:0]: divisor value */
        uint32_t            : 4;   /* bits[10:7] */
        uint32_t PBDIVRDY   : 1;   /* bit 11: divisor ready (R/O) */
        uint32_t            : 3;   /* bits[14:12] */
        uint32_t ON         : 1;   /* bit 15: clock enabled */
        uint32_t            : 16;
    };
    uint32_t w;
} __PBxDIV_t;

#define PB1DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x100u))
#define PB2DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x110u))
#define PB3DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x120u))
#define PB4DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x130u))
#define PB5DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x140u))
#define PB6DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x150u))
#define PB7DIVbits      (*(volatile __PBxDIV_t *)(_CRU_BASE + 0x160u))

/* CLKSTAT — Clock Status (read-only) */
#define CLKSTAT         (*(volatile uint32_t *)(_CRU_BASE + 0x190u))

/* IFS0 bit-field struct — EVIC sources 0-31 */
typedef union {
    struct {
        uint32_t CTIF   : 1;   /* bit 0:  Core Timer          */
        uint32_t CS0IF  : 1;   /* bit 1:  Core SW0            */
        uint32_t CS1IF  : 1;   /* bit 2:  Core SW1            */
        uint32_t INT0IF : 1;   /* bit 3:  External INT0       */
        uint32_t T1IF   : 1;   /* bit 4:  Timer1 (src 4)      */
        uint32_t        : 4;   /* bits 5-8                    */
        uint32_t T2IF   : 1;   /* bit 9:  Timer2 (src 9)      */
        uint32_t        : 4;   /* bits 10-13                  */
        uint32_t T3IF   : 1;   /* bit 14: Timer3 (src 14)     */
        uint32_t        : 4;   /* bits 15-18                  */
        uint32_t T4IF   : 1;   /* bit 19: Timer4 (src 19)     */
        uint32_t        : 4;   /* bits 20-23                  */
        uint32_t T5IF   : 1;   /* bit 24: Timer5 (src 24)     */
        uint32_t        : 3;   /* bits 25-27                  */
        uint32_t T6IF   : 1;   /* bit 28: Timer6 (src 28)     */
        uint32_t        : 3;   /* bits 29-31                  */
    };
    uint32_t w;
} __IFS0_t;

#define IFS0bits        (*(volatile __IFS0_t *)(_EVIC_BASE + 0x0040u))

/* IFS1 bit-field struct — EVIC sources 32-63 (bit = source - 32) */
typedef union {
    struct {
        uint32_t T7IF   : 1;   /* bit 0:  Timer7 (src 32)     */
        uint32_t        : 3;   /* bits 1-3                    */
        uint32_t T8IF   : 1;   /* bit 4:  Timer8 (src 36)     */
        uint32_t        : 3;   /* bits 5-7                    */
        uint32_t T9IF   : 1;   /* bit 8:  Timer9 (src 40)     */
        uint32_t        : 23;  /* bits 9-31                   */
    };
    uint32_t w;
} __IFS1_t;

#define IFS1bits        (*(volatile __IFS1_t *)(_EVIC_BASE + 0x0050u))
#define IFS1CLR         (*(volatile uint32_t  *)(_EVIC_BASE + 0x0054u))
#define IFS1SET         (*(volatile uint32_t  *)(_EVIC_BASE + 0x0058u))

/* IEC0 bit-field struct — EVIC sources 0-31 */
typedef union {
    struct {
        uint32_t CTIE   : 1;   /* bit 0:  Core Timer          */
        uint32_t CS0IE  : 1;   /* bit 1:  Core SW0            */
        uint32_t CS1IE  : 1;   /* bit 2:  Core SW1            */
        uint32_t INT0IE : 1;   /* bit 3:  External INT0       */
        uint32_t T1IE   : 1;   /* bit 4:  Timer1 (src 4)      */
        uint32_t        : 4;   /* bits 5-8                    */
        uint32_t T2IE   : 1;   /* bit 9:  Timer2 (src 9)      */
        uint32_t        : 4;   /* bits 10-13                  */
        uint32_t T3IE   : 1;   /* bit 14: Timer3 (src 14)     */
        uint32_t        : 4;   /* bits 15-18                  */
        uint32_t T4IE   : 1;   /* bit 19: Timer4 (src 19)     */
        uint32_t        : 4;   /* bits 20-23                  */
        uint32_t T5IE   : 1;   /* bit 24: Timer5 (src 24)     */
        uint32_t        : 3;   /* bits 25-27                  */
        uint32_t T6IE   : 1;   /* bit 28: Timer6 (src 28)     */
        uint32_t        : 3;   /* bits 29-31                  */
    };
    uint32_t w;
} __IEC0_t;

#define IEC0bits        (*(volatile __IEC0_t *)(_EVIC_BASE + 0x00C0u))

/* IEC1 bit-field struct — EVIC sources 32-63 (bit = source - 32) */
typedef union {
    struct {
        uint32_t T7IE   : 1;   /* bit 0:  Timer7 (src 32)     */
        uint32_t        : 3;   /* bits 1-3                    */
        uint32_t T8IE   : 1;   /* bit 4:  Timer8 (src 36)     */
        uint32_t        : 3;   /* bits 5-7                    */
        uint32_t T9IE   : 1;   /* bit 8:  Timer9 (src 40)     */
        uint32_t        : 23;  /* bits 9-31                   */
    };
    uint32_t w;
} __IEC1_t;

#define IEC1bits        (*(volatile __IEC1_t *)(_EVIC_BASE + 0x00D0u))
#define IEC1CLR         (*(volatile uint32_t  *)(_EVIC_BASE + 0x00D4u))
#define IEC1SET         (*(volatile uint32_t  *)(_EVIC_BASE + 0x00D8u))

/* -----------------------------------------------------------------------
 * UART1 registers  (base 0xBF828000)
 * ----------------------------------------------------------------------- */

#define _U1_BASE        0xBF828000u

#define U1MODE          (*(volatile uint32_t *)(_U1_BASE + 0x000u))
#define U1MODECLR       (*(volatile uint32_t *)(_U1_BASE + 0x004u))
#define U1MODESET       (*(volatile uint32_t *)(_U1_BASE + 0x008u))
#define U1MODEINV       (*(volatile uint32_t *)(_U1_BASE + 0x00Cu))
#define U1STA           (*(volatile uint32_t *)(_U1_BASE + 0x010u))
#define U1STACLR        (*(volatile uint32_t *)(_U1_BASE + 0x014u))
#define U1STASET        (*(volatile uint32_t *)(_U1_BASE + 0x018u))
#define U1STAINV        (*(volatile uint32_t *)(_U1_BASE + 0x01Cu))
#define U1TXREG         (*(volatile uint32_t *)(_U1_BASE + 0x020u))
#define U1RXREG         (*(volatile uint32_t *)(_U1_BASE + 0x030u))
#define U1BRG           (*(volatile uint32_t *)(_U1_BASE + 0x040u))
#define U1BRGCLR        (*(volatile uint32_t *)(_U1_BASE + 0x044u))
#define U1BRGSET        (*(volatile uint32_t *)(_U1_BASE + 0x048u))
#define U1BRGINV        (*(volatile uint32_t *)(_U1_BASE + 0x04Cu))

/* U1MODE bit masks (plib-compatible _MASK suffix) */
#define _U1MODE_STSEL_MASK      0x00000001u
#define _U1MODE_PDSEL0_MASK     0x00000002u
#define _U1MODE_PDSEL1_MASK     0x00000004u
#define _U1MODE_PDSEL_MASK      0x00000006u
#define _U1MODE_BRGH_MASK       0x00000008u
#define _U1MODE_RXINV_MASK      0x00000010u
#define _U1MODE_ABAUD_MASK      0x00000020u
#define _U1MODE_LPBACK_MASK     0x00000040u
#define _U1MODE_WAKE_MASK       0x00000080u
#define _U1MODE_SIDL_MASK       0x00002000u
#define _U1MODE_ON_MASK         0x00008000u

/* U1STA bit masks (plib-compatible _MASK suffix) */
#define _U1STA_URXDA_MASK       0x00000001u
#define _U1STA_OERR_MASK        0x00000002u
#define _U1STA_FERR_MASK        0x00000004u
#define _U1STA_PERR_MASK        0x00000008u
#define _U1STA_RIDLE_MASK       0x00000010u
#define _U1STA_ADDEN_MASK       0x00000020u
#define _U1STA_TRMT_MASK        0x00000100u
#define _U1STA_UTXBF_MASK       0x00000200u
#define _U1STA_UTXEN_MASK       0x00000400u
#define _U1STA_UTXBRK_MASK      0x00000800u
#define _U1STA_URXEN_MASK       0x00001000u
#define _U1STA_UTXISEL0_MASK    0x00004000u
#define _U1STA_UTXISEL1_MASK    0x00008000u

/* Legacy shorthand (backward compat) */
#define _U1STA_UTXBF    _U1STA_UTXBF_MASK
#define _U1STA_UTXEN    _U1STA_UTXEN_MASK
#define _U1MODE_ON      _U1MODE_ON_MASK

/* -----------------------------------------------------------------------
 * IFS2 / IEC2  (sources 64–95)
 * Timer 6 = source 76 → bit 12
 * ----------------------------------------------------------------------- */

#define IFS2            (*(volatile uint32_t *)(_EVIC_BASE + 0x0060u))
#define IFS2CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0064u))
#define IFS2SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0068u))
#define IFS2INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x006Cu))

#define IEC2            (*(volatile uint32_t *)(_EVIC_BASE + 0x00E0u))
#define IEC2CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00E4u))
#define IEC2SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00E8u))
#define IEC2INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00ECu))

/* Timer 6 interrupt masks (bit 12) */
#define _IFS2_T6IF_MASK         0x00001000u
#define _IEC2_T6IE_MASK         0x00001000u

/* Timer 7 interrupt masks (bit 16) */
#define _IFS2_T7IF_MASK         0x00010000u
#define _IEC2_T7IE_MASK         0x00010000u

/* Timer 8 interrupt masks (bit 20) */
#define _IFS2_T8IF_MASK         0x00100000u
#define _IEC2_T8IE_MASK         0x00100000u

/* Timer 9 interrupt masks (bit 24) */
#define _IFS2_T9IF_MASK         0x01000000u
#define _IEC2_T9IE_MASK         0x01000000u

/* IFS2bits for firmware compatibility (bit 12 = T6IF, bit 16 = T7IF, bit 20 = T8IF, bit 24 = T9IF) */
typedef union {
    struct {
        uint32_t :12;
        uint32_t T6IF:1;
        uint32_t :3;
        uint32_t T7IF:1;
        uint32_t :3;
        uint32_t T8IF:1;
        uint32_t :3;
        uint32_t T9IF:1;
        uint32_t :7;
    };
    uint32_t w;
} __IFS2bits_t;
#define IFS2bits (*(volatile __IFS2bits_t *)(_EVIC_BASE + 0x0060u))

/* -----------------------------------------------------------------------
 * IFS3 / IEC3  (sources 96–127)
 * UART2: U2E=115 (bit 19), U2RX=116 (bit 20), U2TX=117 (bit 21)
 * ----------------------------------------------------------------------- */

#define IFS3            (*(volatile uint32_t *)(_EVIC_BASE + 0x0070u))
#define IFS3CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0074u))
#define IFS3SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0078u))
#define IFS3INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x007Cu))

#define IEC3            (*(volatile uint32_t *)(_EVIC_BASE + 0x00F0u))
#define IEC3CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00F4u))
#define IEC3SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00F8u))
#define IEC3INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00FCu))

#define _IFS3_U2EIF_MASK        (1u << 19)   /* source 115 */
#define _IFS3_U2RXIF_MASK       (1u << 20)   /* source 116 */
#define _IFS3_U2TXIF_MASK       (1u << 21)   /* source 117 */
#define _IEC3_U2EIE_MASK        (1u << 19)
#define _IEC3_U2RXIE_MASK       (1u << 20)
#define _IEC3_U2TXIE_MASK       (1u << 21)

/* -----------------------------------------------------------------------
 * UART2 registers  (base 0xBF828200)
 * ----------------------------------------------------------------------- */

#define _U2_BASE        0xBF828200u

#define U2MODE          (*(volatile uint32_t *)(_U2_BASE + 0x000u))
#define U2MODECLR       (*(volatile uint32_t *)(_U2_BASE + 0x004u))
#define U2MODESET       (*(volatile uint32_t *)(_U2_BASE + 0x008u))
#define U2MODEINV       (*(volatile uint32_t *)(_U2_BASE + 0x00Cu))
#define U2STA           (*(volatile uint32_t *)(_U2_BASE + 0x010u))
#define U2STACLR        (*(volatile uint32_t *)(_U2_BASE + 0x014u))
#define U2STASET        (*(volatile uint32_t *)(_U2_BASE + 0x018u))
#define U2STAINV        (*(volatile uint32_t *)(_U2_BASE + 0x01Cu))
#define U2TXREG         (*(volatile uint32_t *)(_U2_BASE + 0x020u))
#define U2RXREG         (*(volatile uint32_t *)(_U2_BASE + 0x030u))
#define U2BRG           (*(volatile uint32_t *)(_U2_BASE + 0x040u))
#define U2BRGCLR        (*(volatile uint32_t *)(_U2_BASE + 0x044u))
#define U2BRGSET        (*(volatile uint32_t *)(_U2_BASE + 0x048u))
#define U2BRGINV        (*(volatile uint32_t *)(_U2_BASE + 0x04Cu))

/* U2MODE bit masks (same field layout as U1MODE) */
#define _U2MODE_STSEL_MASK      0x00000001u
#define _U2MODE_PDSEL0_MASK     0x00000002u
#define _U2MODE_PDSEL1_MASK     0x00000004u
#define _U2MODE_PDSEL_MASK      0x00000006u
#define _U2MODE_BRGH_MASK       0x00000008u
#define _U2MODE_RXINV_MASK      0x00000010u
#define _U2MODE_ABAUD_MASK      0x00000020u
#define _U2MODE_LPBACK_MASK     0x00000040u
#define _U2MODE_WAKE_MASK       0x00000080u
#define _U2MODE_SIDL_MASK       0x00002000u
#define _U2MODE_ON_MASK         0x00008000u

/* U2STA bit masks (same field layout as U1STA) */
#define _U2STA_URXDA_MASK       0x00000001u
#define _U2STA_OERR_MASK        0x00000002u
#define _U2STA_FERR_MASK        0x00000004u
#define _U2STA_PERR_MASK        0x00000008u
#define _U2STA_RIDLE_MASK       0x00000010u
#define _U2STA_ADDEN_MASK       0x00000020u
#define _U2STA_TRMT_MASK        0x00000100u
#define _U2STA_UTXBF_MASK       0x00000200u
#define _U2STA_UTXEN_MASK       0x00000400u
#define _U2STA_UTXBRK_MASK      0x00000800u
#define _U2STA_URXEN_MASK       0x00001000u
#define _U2STA_UTXISEL0_MASK    0x00004000u
#define _U2STA_UTXISEL1_MASK    0x00008000u

/* Compatibility aliases for upstream HAL debug UART wiring. */
#define U6MODECLR               U2MODECLR
#define U6MODESET               U2MODESET
#define _U6MODE_ON_MASK         _U2MODE_ON_MASK

/* -----------------------------------------------------------------------
 * UART4 registers  (base 0xBF848600)
 * ----------------------------------------------------------------------- */

#define _U4_BASE        0xBF848600u

#define U4MODE          (*(volatile uint32_t *)(_U4_BASE + 0x000u))
#define U4MODECLR       (*(volatile uint32_t *)(_U4_BASE + 0x004u))
#define U4MODESET       (*(volatile uint32_t *)(_U4_BASE + 0x008u))
#define U4MODEINV       (*(volatile uint32_t *)(_U4_BASE + 0x00Cu))
#define U4STA           (*(volatile uint32_t *)(_U4_BASE + 0x010u))
#define U4STACLR        (*(volatile uint32_t *)(_U4_BASE + 0x014u))
#define U4STASET        (*(volatile uint32_t *)(_U4_BASE + 0x018u))
#define U4STAINV        (*(volatile uint32_t *)(_U4_BASE + 0x01Cu))
#define U4TXREG         (*(volatile uint32_t *)(_U4_BASE + 0x020u))
#define U4RXREG         (*(volatile uint32_t *)(_U4_BASE + 0x030u))
#define U4BRG           (*(volatile uint32_t *)(_U4_BASE + 0x040u))
#define U4BRGCLR        (*(volatile uint32_t *)(_U4_BASE + 0x044u))
#define U4BRGSET        (*(volatile uint32_t *)(_U4_BASE + 0x048u))
#define U4BRGINV        (*(volatile uint32_t *)(_U4_BASE + 0x04Cu))

/* U4MODE bit masks (same field layout as U1MODE) */
#define _U4MODE_STSEL_MASK      0x00000001u
#define _U4MODE_PDSEL0_MASK     0x00000002u
#define _U4MODE_PDSEL1_MASK     0x00000004u
#define _U4MODE_PDSEL_MASK      0x00000006u
#define _U4MODE_BRGH_MASK       0x00000008u
#define _U4MODE_RXINV_MASK      0x00000010u
#define _U4MODE_ABAUD_MASK      0x00000020u
#define _U4MODE_LPBACK_MASK     0x00000040u
#define _U4MODE_WAKE_MASK       0x00000080u
#define _U4MODE_SIDL_MASK       0x00002000u
#define _U4MODE_ON_MASK         0x00008000u

/* U4STA bit masks (same field layout as U1STA) */
#define _U4STA_URXDA_MASK       0x00000001u
#define _U4STA_OERR_MASK        0x00000002u
#define _U4STA_FERR_MASK        0x00000004u
#define _U4STA_PERR_MASK        0x00000008u
#define _U4STA_RIDLE_MASK       0x00000010u
#define _U4STA_ADDEN_MASK       0x00000020u
#define _U4STA_TRMT_MASK        0x00000100u
#define _U4STA_UTXBF_MASK       0x00000200u
#define _U4STA_UTXEN_MASK       0x00000400u
#define _U4STA_UTXBRK_MASK      0x00000800u
#define _U4STA_URXEN_MASK       0x00001000u
#define _U4STA_UTXISEL0_MASK    0x00004000u
#define _U4STA_UTXISEL1_MASK    0x00008000u

/* UART4 interrupt bit masks in IFS2 / IEC2 (EVIC sources 65–67)
 * U4E = bit 1, U4RX = bit 2, U4TX = bit 3 */
#define _IFS2_U4EIF_MASK        0x00000002u
#define _IFS2_U4RXIF_MASK       0x00000004u
#define _IFS2_U4TXIF_MASK       0x00000008u
#define _IEC2_U4EIE_MASK        0x00000002u
#define _IEC2_U4RXIE_MASK       0x00000004u
#define _IEC2_U4TXIE_MASK       0x00000008u

/* -----------------------------------------------------------------------
 * IFS4 / IFS5  (sources 128–159, 160–191)
 * IEC4 / IEC5
 * CAN1 combined interrupt = EVIC source 167 → IFS5/IEC5 bit 7
 * ----------------------------------------------------------------------- */

#define IFS4            (*(volatile uint32_t *)(_EVIC_BASE + 0x0080u))
#define IFS4CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0084u))
#define IFS4SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0088u))
#define IFS4INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x008Cu))

#define IFS5            (*(volatile uint32_t *)(_EVIC_BASE + 0x0090u))
#define IFS5CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0094u))
#define IFS5SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0098u))
#define IFS5INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x009Cu))

#define IEC4            (*(volatile uint32_t *)(_EVIC_BASE + 0x0100u))
#define IEC4CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0104u))
#define IEC4SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0108u))
#define IEC4INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x010Cu))

#define IEC5            (*(volatile uint32_t *)(_EVIC_BASE + 0x0110u))
#define IEC5CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0114u))
#define IEC5SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0118u))
#define IEC5INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x011Cu))

#define IFS6            (*(volatile uint32_t *)(_EVIC_BASE + 0x00A0u))
#define IFS6CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00A4u))
#define IFS6SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00A8u))
#define IFS6INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00ACu))

#define IEC6            (*(volatile uint32_t *)(_EVIC_BASE + 0x0120u))
#define IEC6CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0124u))
#define IEC6SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0128u))
#define IEC6INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x012Cu))

/* IC16 = EVIC source 216/215 = IFS6/IEC6 bits 23 (error), 24 (capture) */
#define _IFS6_IC16EIF_MASK   0x00800000u
#define _IFS6_IC16IF_MASK    0x01000000u
#define _IEC6_IC16EIE_MASK   0x00800000u
#define _IEC6_IC16IE_MASK    0x01000000u

/* CAN1 = EVIC source 167 = IFS5/IEC5 bit 7 */
#define _IFS5_CAN1IF_MASK   0x00000080u
#define _IEC5_CAN1IE_MASK   0x00000080u

/* -----------------------------------------------------------------------
 * CAN1 (CFD1*) SFR registers — base 0xBF880000
 * Layout verified against XC32 p32mk1024mcm100.h device header.
 * Each register block: +0 base, +4 SET, +8 CLR, +C INV
 * ----------------------------------------------------------------------- */

#define _CFD1_BASE      0xBF880000u

/* CiCON */
#define CFD1CON         (*(volatile uint32_t *)(_CFD1_BASE + 0x000u))
#define CFD1CONCLR      (*(volatile uint32_t *)(_CFD1_BASE + 0x004u))
#define CFD1CONSET      (*(volatile uint32_t *)(_CFD1_BASE + 0x008u))
#define CFD1CONINV      (*(volatile uint32_t *)(_CFD1_BASE + 0x00Cu))

#define _CFD1CON_CLKSEL0_POSITION   7
#define _CFD1CON_CLKSEL0_MASK       0x00000080u
#define _CFD1CON_ON_MASK            0x00008000u
#define _CFD1CON_STEF_POSITION      19
#define _CFD1CON_STEF_MASK          0x00080000u
#define _CFD1CON_TXQEN_POSITION     20
#define _CFD1CON_TXQEN_MASK         0x00100000u
#define _CFD1CON_OPMOD_POSITION     21
#define _CFD1CON_OPMOD_MASK         0x00E00000u
#define _CFD1CON_REQOP_POSITION     24
#define _CFD1CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD1NBTCFG      (*(volatile uint32_t *)(_CFD1_BASE + 0x010u))

#define _CFD1NBTCFG_SJW_POSITION    0
#define _CFD1NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD1NBTCFG_TSEG2_POSITION  8
#define _CFD1NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD1NBTCFG_TSEG1_POSITION  16
#define _CFD1NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD1NBTCFG_BRP_POSITION    24
#define _CFD1NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD1DBTCFG      (*(volatile uint32_t *)(_CFD1_BASE + 0x020u))

#define _CFD1DBTCFG_SJW_POSITION    0
#define _CFD1DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD1DBTCFG_TSEG2_POSITION  8
#define _CFD1DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD1DBTCFG_TSEG1_POSITION  16
#define _CFD1DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD1DBTCFG_BRP_POSITION    24
#define _CFD1DBTCFG_BRP_MASK        0xFF000000u

/* CiTSCON — Timestamp Control */
#define CFD1TSCON       (*(volatile uint32_t *)(_CFD1_BASE + 0x050u))
#define CFD1TSCONCLR    (*(volatile uint32_t *)(_CFD1_BASE + 0x054u))
#define CFD1TSCONSET    (*(volatile uint32_t *)(_CFD1_BASE + 0x058u))

#define _CFD1TSCON_TBCPRE_POSITION  0
#define _CFD1TSCON_TBCPRE_MASK      0x000003FFu
#define _CFD1TSCON_TBCEN_POSITION   16
#define _CFD1TSCON_TBCEN_MASK       0x00010000u
#define _CFD1TSCON_TSEOF_POSITION   17
#define _CFD1TSCON_TSEOF_MASK       0x00020000u
#define _CFD1TSCON_TSRES_POSITION   18
#define _CFD1TSCON_TSRES_MASK       0x00040000u

/* CiVEC */
#define CFD1VEC         (*(volatile uint32_t *)(_CFD1_BASE + 0x060u))
#define _CFD1VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD1INT         (*(volatile uint32_t *)(_CFD1_BASE + 0x070u))
#define CFD1INTCLR      (*(volatile uint32_t *)(_CFD1_BASE + 0x074u))
#define CFD1INTSET      (*(volatile uint32_t *)(_CFD1_BASE + 0x078u))

/* CiINT status flags (lower 16 bits) */
#define _CFD1INT_TXIF_MASK          0x00000001u   /* bit 0 */
#define _CFD1INT_RXIF_MASK          0x00000002u   /* bit 1 */
#define _CFD1INT_SERRIF_MASK        0x00001000u   /* bit 12 */
#define _CFD1INT_CERRIF_MASK        0x00002000u   /* bit 13 */
#define _CFD1INT_IVMIF_MASK         0x00008000u   /* bit 15 */
/* CiINT interrupt enables (upper 16 bits) */
#define _CFD1INT_TXIE_MASK          0x00010000u   /* bit 16 */
#define _CFD1INT_RXIE_MASK          0x00020000u   /* bit 17 */
#define _CFD1INT_SERRIE_MASK        0x10000000u   /* bit 28 */
#define _CFD1INT_CERRIE_MASK        0x20000000u   /* bit 29 */
#define _CFD1INT_IVMIE_MASK         0x80000000u   /* bit 31 */

/* CiTREC */
#define CFD1TREC        (*(volatile uint32_t *)(_CFD1_BASE + 0x0D0u))

#define _CFD1TREC_TERRCNT_POSITION  0
#define _CFD1TREC_TERRCNT_MASK      0x000000FFu
#define _CFD1TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD1TREC_EWARN_MASK        0x00010000u
#define _CFD1TREC_RXWARN_MASK       0x00020000u
#define _CFD1TREC_TXWARN_MASK       0x00040000u
#define _CFD1TREC_RXBP_MASK         0x00080000u
#define _CFD1TREC_TXBP_MASK         0x00100000u
#define _CFD1TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD1TEFCON      (*(volatile uint32_t *)(_CFD1_BASE + 0x100u))

#define _CFD1TEFCON_TEFTSEN_POSITION 5
#define _CFD1TEFCON_TEFTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD1TEFCON_UINC_MASK       0x00000100u   /* bit 8 */
#define _CFD1TEFCON_FSIZE_POSITION  24
#define _CFD1TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD1TEFSTA      (*(volatile uint32_t *)(_CFD1_BASE + 0x110u))
#define _CFD1TEFSTA_TEFNEIF_MASK    0x00000001u   /* TEF Not Empty flag, bit 0 */

/* CiTEFUA */
#define CFD1TEFUA       (*(volatile uint32_t *)(_CFD1_BASE + 0x120u))

/* CiFIFOBA — message RAM base address (Phase 3C stub in emulator) */
#define CFD1FIFOBA      (*(volatile uint32_t *)(_CFD1_BASE + 0x130u))

/* CiTXQCON */
#define CFD1TXQCON      (*(volatile uint32_t *)(_CFD1_BASE + 0x140u))
#define CFD1TXQCONCLR   (*(volatile uint32_t *)(_CFD1_BASE + 0x144u))
#define CFD1TXQCONSET   (*(volatile uint32_t *)(_CFD1_BASE + 0x148u))

#define _CFD1TXQCON_TXQEIE_MASK     0x00000010u   /* TX Queue Empty IE, bit 4 */
#define _CFD1TXQCON_UINC_MASK       0x00000100u   /* User Increment, bit 8 */
#define _CFD1TXQCON_TXREQ_MASK      0x00000200u   /* TX Request, bit 9 */
#define _CFD1TXQCON_TXPRI_POSITION  16
#define _CFD1TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD1TXQCON_FSIZE_POSITION  24
#define _CFD1TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD1TXQCON_PLSIZE_POSITION 29
#define _CFD1TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD1TXQSTA      (*(volatile uint32_t *)(_CFD1_BASE + 0x150u))
#define _CFD1TXQSTA_TXQNIF_MASK     0x00000001u   /* TX Queue Not Full, bit 0 */

/* CiTXQUA */
#define CFD1TXQUA       (*(volatile uint32_t *)(_CFD1_BASE + 0x160u))

/* CiFIFOCON1 / CiFIFOCON2 (FIFO 2 = FIFO 1 base + 0x30) */
#define CFD1FIFOCON1    (*(volatile uint32_t *)(_CFD1_BASE + 0x170u))
#define CFD1FIFOCON1CLR (*(volatile uint32_t *)(_CFD1_BASE + 0x174u))
#define CFD1FIFOCON1SET (*(volatile uint32_t *)(_CFD1_BASE + 0x178u))

#define _CFD1FIFOCON1_TFNRFNIE_MASK  0x00000001u  /* Not Empty IE, bit 0 */
#define _CFD1FIFOCON1_TFERFFIE_MASK  0x00000010u  /* TX Empty IE, bit 4 */
#define _CFD1FIFOCON1_RTREN_POSITION 6
#define _CFD1FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD1FIFOCON1_TXEN_MASK      0x00000080u  /* TX enable, bit 7 */
#define _CFD1FIFOCON1_UINC_MASK      0x00000100u  /* User Increment, bit 8 */
#define _CFD1FIFOCON1_TXREQ_MASK     0x00000200u  /* TX Request, bit 9 */
#define _CFD1FIFOCON1_TXPRI_POSITION 16
#define _CFD1FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD1FIFOCON1_FSIZE_POSITION 24
#define _CFD1FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD1FIFOCON1_PLSIZE_POSITION 29
#define _CFD1FIFOCON1_PLSIZE_MASK    0xE0000000u

#define CFD1FIFOCON2    (*(volatile uint32_t *)(_CFD1_BASE + 0x1A0u))

#define _CFD1FIFOCON2_RXTSEN_POSITION 5
#define _CFD1FIFOCON2_RXTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD1FIFOCON2_FSIZE_POSITION 24
#define _CFD1FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD1FIFOCON2_PLSIZE_POSITION 29
#define _CFD1FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 (FIFOSTA2 = FIFOSTA1 + 0x30 via pointer arithmetic in plib) */
#define CFD1FIFOSTA1    (*(volatile uint32_t *)(_CFD1_BASE + 0x180u))
#define _CFD1FIFOSTA1_TFNRFNIF_MASK  0x00000001u  /* Not Empty flag, bit 0 */
#define _CFD1FIFOSTA1_TXATIF_MASK    0x00000010u  /* TX Attempts exhausted, bit 4 */

/* CiFIFOUA1 */
#define CFD1FIFOUA1     (*(volatile uint32_t *)(_CFD1_BASE + 0x190u))

/* CiFLTCON0 */
#define CFD1FLTCON0     (*(volatile uint32_t *)(_CFD1_BASE + 0x740u))
#define CFD1FLTCON0SET  (*(volatile uint32_t *)(_CFD1_BASE + 0x748u))

#define _CFD1FLTCON0_F0BP_POSITION   0
#define _CFD1FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD1FLTCON0_FLTEN0_POSITION 7
#define _CFD1FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 (stride 0x20 per pair via pointer arithmetic in plib) */
#define CFD1FLTOBJ0     (*(volatile uint32_t *)(_CFD1_BASE + 0x7C0u))
#define _CFD1FLTOBJ0_EXIDE_MASK      0x20000000u  /* Extended ID enable, bit 29 */

/* CiMASK0 */
#define CFD1MASK0       (*(volatile uint32_t *)(_CFD1_BASE + 0x7D0u))
#define _CFD1MASK0_MIDE_MASK         0x20000000u  /* Match IDE bit, bit 29 */

/* -----------------------------------------------------------------------
 * CAN2 = EVIC source 168 = IFS5/IEC5 bit 8
 * ----------------------------------------------------------------------- */
#define _IFS5_CAN2IF_MASK   0x00000100u
#define _IEC5_CAN2IE_MASK   0x00000100u

/* -----------------------------------------------------------------------
 * CAN2 (CFD2*) SFR registers — base 0xBF881000
 * Same layout as CFD1; only the base address differs.
 * ----------------------------------------------------------------------- */

#define _CFD2_BASE      0xBF881000u

/* CiCON */
#define CFD2CON         (*(volatile uint32_t *)(_CFD2_BASE + 0x000u))
#define CFD2CONCLR      (*(volatile uint32_t *)(_CFD2_BASE + 0x004u))
#define CFD2CONSET      (*(volatile uint32_t *)(_CFD2_BASE + 0x008u))
#define CFD2CONINV      (*(volatile uint32_t *)(_CFD2_BASE + 0x00Cu))

#define _CFD2CON_CLKSEL0_POSITION   7
#define _CFD2CON_CLKSEL0_MASK       0x00000080u
#define _CFD2CON_ON_MASK            0x00008000u
#define _CFD2CON_STEF_POSITION      19
#define _CFD2CON_STEF_MASK          0x00080000u
#define _CFD2CON_TXQEN_POSITION     20
#define _CFD2CON_TXQEN_MASK         0x00100000u
#define _CFD2CON_OPMOD_POSITION     21
#define _CFD2CON_OPMOD_MASK         0x00E00000u
#define _CFD2CON_REQOP_POSITION     24
#define _CFD2CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD2NBTCFG      (*(volatile uint32_t *)(_CFD2_BASE + 0x010u))

#define _CFD2NBTCFG_SJW_POSITION    0
#define _CFD2NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD2NBTCFG_TSEG2_POSITION  8
#define _CFD2NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD2NBTCFG_TSEG1_POSITION  16
#define _CFD2NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD2NBTCFG_BRP_POSITION    24
#define _CFD2NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD2DBTCFG      (*(volatile uint32_t *)(_CFD2_BASE + 0x020u))

#define _CFD2DBTCFG_SJW_POSITION    0
#define _CFD2DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD2DBTCFG_TSEG2_POSITION  8
#define _CFD2DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD2DBTCFG_TSEG1_POSITION  16
#define _CFD2DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD2DBTCFG_BRP_POSITION    24
#define _CFD2DBTCFG_BRP_MASK        0xFF000000u

/* CiTBC */
#define CFD2TBC         (*(volatile uint32_t *)(_CFD2_BASE + 0x040u))

/* CiTSCON — Timestamp Control */
#define CFD2TSCON       (*(volatile uint32_t *)(_CFD2_BASE + 0x050u))
#define CFD2TSCONCLR    (*(volatile uint32_t *)(_CFD2_BASE + 0x054u))
#define CFD2TSCONSET    (*(volatile uint32_t *)(_CFD2_BASE + 0x058u))

#define _CFD2TSCON_TBCPRE_POSITION  0
#define _CFD2TSCON_TBCPRE_MASK      0x000003FFu
#define _CFD2TSCON_TBCEN_POSITION   16
#define _CFD2TSCON_TBCEN_MASK       0x00010000u
#define _CFD2TSCON_TSEOF_POSITION   17
#define _CFD2TSCON_TSEOF_MASK       0x00020000u
#define _CFD2TSCON_TSRES_POSITION   18
#define _CFD2TSCON_TSRES_MASK       0x00040000u

/* CiVEC */
#define CFD2VEC         (*(volatile uint32_t *)(_CFD2_BASE + 0x060u))
#define _CFD2VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD2INT         (*(volatile uint32_t *)(_CFD2_BASE + 0x070u))
#define CFD2INTCLR      (*(volatile uint32_t *)(_CFD2_BASE + 0x074u))
#define CFD2INTSET      (*(volatile uint32_t *)(_CFD2_BASE + 0x078u))

#define _CFD2INT_TXIF_MASK          0x00000001u
#define _CFD2INT_RXIF_MASK          0x00000002u
#define _CFD2INT_MODIF_MASK         0x00000008u
#define _CFD2INT_SERRIF_MASK        0x00001000u
#define _CFD2INT_CERRIF_MASK        0x00002000u
#define _CFD2INT_IVMIF_MASK         0x00008000u
#define _CFD2INT_TXIE_MASK          0x00010000u
#define _CFD2INT_RXIE_MASK          0x00020000u
#define _CFD2INT_MODIE_MASK         0x00080000u
#define _CFD2INT_SERRIE_MASK        0x10000000u
#define _CFD2INT_CERRIE_MASK        0x20000000u
#define _CFD2INT_IVMIE_MASK         0x80000000u

/* CiRXIF / CiTXIF / CiRXOVIF / CiTXATIF */
#define CFD2RXIF        (*(volatile uint32_t *)(_CFD2_BASE + 0x080u))
#define CFD2TXIF        (*(volatile uint32_t *)(_CFD2_BASE + 0x090u))
#define CFD2RXOVIF      (*(volatile uint32_t *)(_CFD2_BASE + 0x0A0u))
#define CFD2TXATIF      (*(volatile uint32_t *)(_CFD2_BASE + 0x0B0u))

/* CiTREC */
#define CFD2TREC        (*(volatile uint32_t *)(_CFD2_BASE + 0x0D0u))

#define _CFD2TREC_TERRCNT_POSITION  0
#define _CFD2TREC_TERRCNT_MASK      0x000000FFu
#define _CFD2TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD2TREC_EWARN_MASK        0x00010000u
#define _CFD2TREC_RXWARN_MASK       0x00020000u
#define _CFD2TREC_TXWARN_MASK       0x00040000u
#define _CFD2TREC_RXBP_MASK         0x00080000u
#define _CFD2TREC_TXBP_MASK         0x00100000u
#define _CFD2TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD2TEFCON      (*(volatile uint32_t *)(_CFD2_BASE + 0x100u))
#define CFD2TEFCONCLR   (*(volatile uint32_t *)(_CFD2_BASE + 0x104u))
#define CFD2TEFCONSET   (*(volatile uint32_t *)(_CFD2_BASE + 0x108u))

#define _CFD2TEFCON_TEFTSEN_POSITION 5
#define _CFD2TEFCON_TEFTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD2TEFCON_UINC_MASK       0x00000100u
#define _CFD2TEFCON_FSIZE_POSITION  24
#define _CFD2TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD2TEFSTA      (*(volatile uint32_t *)(_CFD2_BASE + 0x110u))
#define _CFD2TEFSTA_TEFNEIF_MASK    0x00000001u

/* CiTEFUA */
#define CFD2TEFUA       (*(volatile uint32_t *)(_CFD2_BASE + 0x120u))

/* CiFIFOBA */
#define CFD2FIFOBA      (*(volatile uint32_t *)(_CFD2_BASE + 0x130u))

/* CiTXQCON */
#define CFD2TXQCON      (*(volatile uint32_t *)(_CFD2_BASE + 0x140u))
#define CFD2TXQCONCLR   (*(volatile uint32_t *)(_CFD2_BASE + 0x144u))
#define CFD2TXQCONSET   (*(volatile uint32_t *)(_CFD2_BASE + 0x148u))

#define _CFD2TXQCON_TXQEIE_MASK     0x00000010u
#define _CFD2TXQCON_UINC_MASK       0x00000100u
#define _CFD2TXQCON_TXREQ_MASK      0x00000200u
#define _CFD2TXQCON_TXPRI_POSITION  16
#define _CFD2TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD2TXQCON_FSIZE_POSITION  24
#define _CFD2TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD2TXQCON_PLSIZE_POSITION 29
#define _CFD2TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD2TXQSTA      (*(volatile uint32_t *)(_CFD2_BASE + 0x150u))
#define _CFD2TXQSTA_TXQNIF_MASK     0x00000001u

/* CiTXQUA */
#define CFD2TXQUA       (*(volatile uint32_t *)(_CFD2_BASE + 0x160u))

/* CiFIFOCON1 */
#define CFD2FIFOCON1    (*(volatile uint32_t *)(_CFD2_BASE + 0x170u))
#define CFD2FIFOCON1CLR (*(volatile uint32_t *)(_CFD2_BASE + 0x174u))
#define CFD2FIFOCON1SET (*(volatile uint32_t *)(_CFD2_BASE + 0x178u))

#define _CFD2FIFOCON1_TFNRFNIE_MASK  0x00000001u
#define _CFD2FIFOCON1_TFERFFIE_MASK  0x00000010u
#define _CFD2FIFOCON1_RTREN_POSITION 6
#define _CFD2FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD2FIFOCON1_TXEN_MASK      0x00000080u
#define _CFD2FIFOCON1_UINC_MASK      0x00000100u
#define _CFD2FIFOCON1_TXREQ_MASK     0x00000200u
#define _CFD2FIFOCON1_TXPRI_POSITION 16
#define _CFD2FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD2FIFOCON1_FSIZE_POSITION 24
#define _CFD2FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD2FIFOCON1_PLSIZE_POSITION 29
#define _CFD2FIFOCON1_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 */
#define CFD2FIFOSTA1    (*(volatile uint32_t *)(_CFD2_BASE + 0x180u))
#define _CFD2FIFOSTA1_TFNRFNIF_MASK  0x00000001u
#define _CFD2FIFOSTA1_TXATIF_MASK    0x00000010u

/* CiFIFOUA1 */
#define CFD2FIFOUA1     (*(volatile uint32_t *)(_CFD2_BASE + 0x190u))

/* CiFIFOCON2 (FIFO2 = RX FIFO in plib_canfd2) */
#define CFD2FIFOCON2    (*(volatile uint32_t *)(_CFD2_BASE + 0x1A0u))
#define CFD2FIFOCON2CLR (*(volatile uint32_t *)(_CFD2_BASE + 0x1A4u))
#define CFD2FIFOCON2SET (*(volatile uint32_t *)(_CFD2_BASE + 0x1A8u))

#define _CFD2FIFOCON2_RXTSEN_POSITION 5
#define _CFD2FIFOCON2_RXTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD2FIFOCON2_FSIZE_POSITION 24
#define _CFD2FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD2FIFOCON2_PLSIZE_POSITION 29
#define _CFD2FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA2 */
#define CFD2FIFOSTA2    (*(volatile uint32_t *)(_CFD2_BASE + 0x1B0u))
#define _CFD2FIFOSTA2_TFNRFNIF_MASK  0x00000001u

/* CiFIFOUA2 */
#define CFD2FIFOUA2     (*(volatile uint32_t *)(_CFD2_BASE + 0x1C0u))

/* CiFLTCON0 */
#define CFD2FLTCON0     (*(volatile uint32_t *)(_CFD2_BASE + 0x740u))
#define CFD2FLTCON0SET  (*(volatile uint32_t *)(_CFD2_BASE + 0x748u))

#define _CFD2FLTCON0_F0BP_POSITION   0
#define _CFD2FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD2FLTCON0_FLTEN0_POSITION 7
#define _CFD2FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 */
#define CFD2FLTOBJ0     (*(volatile uint32_t *)(_CFD2_BASE + 0x7C0u))
#define _CFD2FLTOBJ0_EXIDE_MASK      0x20000000u

/* CiMASK0 */
#define CFD2MASK0       (*(volatile uint32_t *)(_CFD2_BASE + 0x7D0u))
#define _CFD2MASK0_MIDE_MASK         0x20000000u

#define _IFS5_CAN3IF_MASK   0x08000000u
#define _IEC5_CAN3IE_MASK   0x08000000u

/* -----------------------------------------------------------------------
 * CAN3 (CFD3*) SFR registers — base 0xBF884000
 * Same layout as CFD1; only the base address differs.
 * ----------------------------------------------------------------------- */

#define _CFD3_BASE      0xBF884000u

/* CiCON */
#define CFD3CON         (*(volatile uint32_t *)(_CFD3_BASE + 0x000u))
#define CFD3CONCLR      (*(volatile uint32_t *)(_CFD3_BASE + 0x004u))
#define CFD3CONSET      (*(volatile uint32_t *)(_CFD3_BASE + 0x008u))
#define CFD3CONINV      (*(volatile uint32_t *)(_CFD3_BASE + 0x00Cu))

#define _CFD3CON_CLKSEL0_POSITION   7
#define _CFD3CON_CLKSEL0_MASK       0x00000080u
#define _CFD3CON_ON_MASK            0x00008000u
#define _CFD3CON_STEF_POSITION      19
#define _CFD3CON_STEF_MASK          0x00080000u
#define _CFD3CON_TXQEN_POSITION     20
#define _CFD3CON_TXQEN_MASK         0x00100000u
#define _CFD3CON_OPMOD_POSITION     21
#define _CFD3CON_OPMOD_MASK         0x00E00000u
#define _CFD3CON_REQOP_POSITION     24
#define _CFD3CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD3NBTCFG      (*(volatile uint32_t *)(_CFD3_BASE + 0x010u))

#define _CFD3NBTCFG_SJW_POSITION    0
#define _CFD3NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD3NBTCFG_TSEG2_POSITION  8
#define _CFD3NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD3NBTCFG_TSEG1_POSITION  16
#define _CFD3NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD3NBTCFG_BRP_POSITION    24
#define _CFD3NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD3DBTCFG      (*(volatile uint32_t *)(_CFD3_BASE + 0x020u))

#define _CFD3DBTCFG_SJW_POSITION    0
#define _CFD3DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD3DBTCFG_TSEG2_POSITION  8
#define _CFD3DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD3DBTCFG_TSEG1_POSITION  16
#define _CFD3DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD3DBTCFG_BRP_POSITION    24
#define _CFD3DBTCFG_BRP_MASK        0xFF000000u

/* CiTBC */
#define CFD3TBC         (*(volatile uint32_t *)(_CFD3_BASE + 0x040u))

/* CiTSCON — Timestamp Control */
#define CFD3TSCON       (*(volatile uint32_t *)(_CFD3_BASE + 0x050u))
#define CFD3TSCONCLR    (*(volatile uint32_t *)(_CFD3_BASE + 0x054u))
#define CFD3TSCONSET    (*(volatile uint32_t *)(_CFD3_BASE + 0x058u))

#define _CFD3TSCON_TBCPRE_POSITION  0
#define _CFD3TSCON_TBCPRE_MASK      0x000003FFu
#define _CFD3TSCON_TBCEN_POSITION   16
#define _CFD3TSCON_TBCEN_MASK       0x00010000u
#define _CFD3TSCON_TSEOF_POSITION   17
#define _CFD3TSCON_TSEOF_MASK       0x00020000u
#define _CFD3TSCON_TSRES_POSITION   18
#define _CFD3TSCON_TSRES_MASK       0x00040000u

/* CiVEC */
#define CFD3VEC         (*(volatile uint32_t *)(_CFD3_BASE + 0x060u))
#define _CFD3VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD3INT         (*(volatile uint32_t *)(_CFD3_BASE + 0x070u))
#define CFD3INTCLR      (*(volatile uint32_t *)(_CFD3_BASE + 0x074u))
#define CFD3INTSET      (*(volatile uint32_t *)(_CFD3_BASE + 0x078u))

#define _CFD3INT_TXIF_MASK          0x00000001u
#define _CFD3INT_RXIF_MASK          0x00000002u
#define _CFD3INT_MODIF_MASK         0x00000008u
#define _CFD3INT_SERRIF_MASK        0x00001000u
#define _CFD3INT_CERRIF_MASK        0x00002000u
#define _CFD3INT_IVMIF_MASK         0x00008000u
#define _CFD3INT_TXIE_MASK          0x00010000u
#define _CFD3INT_RXIE_MASK          0x00020000u
#define _CFD3INT_MODIE_MASK         0x00080000u
#define _CFD3INT_SERRIE_MASK        0x10000000u
#define _CFD3INT_CERRIE_MASK        0x20000000u
#define _CFD3INT_IVMIE_MASK         0x80000000u

/* CiRXIF / CiTXIF / CiRXOVIF / CiTXATIF */
#define CFD3RXIF        (*(volatile uint32_t *)(_CFD3_BASE + 0x080u))
#define CFD3TXIF        (*(volatile uint32_t *)(_CFD3_BASE + 0x090u))
#define CFD3RXOVIF      (*(volatile uint32_t *)(_CFD3_BASE + 0x0A0u))
#define CFD3TXATIF      (*(volatile uint32_t *)(_CFD3_BASE + 0x0B0u))

/* CiTREC */
#define CFD3TREC        (*(volatile uint32_t *)(_CFD3_BASE + 0x0D0u))

#define _CFD3TREC_TERRCNT_POSITION  0
#define _CFD3TREC_TERRCNT_MASK      0x000000FFu
#define _CFD3TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD3TREC_EWARN_MASK        0x00010000u
#define _CFD3TREC_RXWARN_MASK       0x00020000u
#define _CFD3TREC_TXWARN_MASK       0x00040000u
#define _CFD3TREC_RXBP_MASK         0x00080000u
#define _CFD3TREC_TXBP_MASK         0x00100000u
#define _CFD3TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD3TEFCON      (*(volatile uint32_t *)(_CFD3_BASE + 0x100u))
#define CFD3TEFCONCLR   (*(volatile uint32_t *)(_CFD3_BASE + 0x104u))
#define CFD3TEFCONSET   (*(volatile uint32_t *)(_CFD3_BASE + 0x108u))

#define _CFD3TEFCON_TEFTSEN_POSITION 5
#define _CFD3TEFCON_TEFTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD3TEFCON_UINC_MASK       0x00000100u
#define _CFD3TEFCON_FSIZE_POSITION  24
#define _CFD3TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD3TEFSTA      (*(volatile uint32_t *)(_CFD3_BASE + 0x110u))
#define _CFD3TEFSTA_TEFNEIF_MASK    0x00000001u

/* CiTEFUA */
#define CFD3TEFUA       (*(volatile uint32_t *)(_CFD3_BASE + 0x120u))

/* CiFIFOBA */
#define CFD3FIFOBA      (*(volatile uint32_t *)(_CFD3_BASE + 0x130u))

/* CiTXQCON */
#define CFD3TXQCON      (*(volatile uint32_t *)(_CFD3_BASE + 0x140u))
#define CFD3TXQCONCLR   (*(volatile uint32_t *)(_CFD3_BASE + 0x144u))
#define CFD3TXQCONSET   (*(volatile uint32_t *)(_CFD3_BASE + 0x148u))

#define _CFD3TXQCON_TXQEIE_MASK     0x00000010u
#define _CFD3TXQCON_UINC_MASK       0x00000100u
#define _CFD3TXQCON_TXREQ_MASK      0x00000200u
#define _CFD3TXQCON_TXPRI_POSITION  16
#define _CFD3TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD3TXQCON_FSIZE_POSITION  24
#define _CFD3TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD3TXQCON_PLSIZE_POSITION 29
#define _CFD3TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD3TXQSTA      (*(volatile uint32_t *)(_CFD3_BASE + 0x150u))
#define _CFD3TXQSTA_TXQNIF_MASK     0x00000001u

/* CiTXQUA */
#define CFD3TXQUA       (*(volatile uint32_t *)(_CFD3_BASE + 0x160u))

/* CiFIFOCON1 */
#define CFD3FIFOCON1    (*(volatile uint32_t *)(_CFD3_BASE + 0x170u))
#define CFD3FIFOCON1CLR (*(volatile uint32_t *)(_CFD3_BASE + 0x174u))
#define CFD3FIFOCON1SET (*(volatile uint32_t *)(_CFD3_BASE + 0x178u))

#define _CFD3FIFOCON1_TFNRFNIE_MASK  0x00000001u
#define _CFD3FIFOCON1_TFERFFIE_MASK  0x00000010u
#define _CFD3FIFOCON1_RTREN_POSITION 6
#define _CFD3FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD3FIFOCON1_TXEN_MASK      0x00000080u
#define _CFD3FIFOCON1_UINC_MASK      0x00000100u
#define _CFD3FIFOCON1_TXREQ_MASK     0x00000200u
#define _CFD3FIFOCON1_TXPRI_POSITION 16
#define _CFD3FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD3FIFOCON1_FSIZE_POSITION 24
#define _CFD3FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD3FIFOCON1_PLSIZE_POSITION 29
#define _CFD3FIFOCON1_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 */
#define CFD3FIFOSTA1    (*(volatile uint32_t *)(_CFD3_BASE + 0x180u))
#define _CFD3FIFOSTA1_TFNRFNIF_MASK  0x00000001u
#define _CFD3FIFOSTA1_TXATIF_MASK    0x00000010u

/* CiFIFOUA1 */
#define CFD3FIFOUA1     (*(volatile uint32_t *)(_CFD3_BASE + 0x190u))

/* CiFIFOCON2 (FIFO2 = RX FIFO) */
#define CFD3FIFOCON2    (*(volatile uint32_t *)(_CFD3_BASE + 0x1A0u))
#define CFD3FIFOCON2CLR (*(volatile uint32_t *)(_CFD3_BASE + 0x1A4u))
#define CFD3FIFOCON2SET (*(volatile uint32_t *)(_CFD3_BASE + 0x1A8u))

#define _CFD3FIFOCON2_RXTSEN_POSITION 5
#define _CFD3FIFOCON2_RXTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD3FIFOCON2_FSIZE_POSITION 24
#define _CFD3FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD3FIFOCON2_PLSIZE_POSITION 29
#define _CFD3FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA2 */
#define CFD3FIFOSTA2    (*(volatile uint32_t *)(_CFD3_BASE + 0x1B0u))
#define _CFD3FIFOSTA2_TFNRFNIF_MASK  0x00000001u

/* CiFIFOUA2 */
#define CFD3FIFOUA2     (*(volatile uint32_t *)(_CFD3_BASE + 0x1C0u))

/* CiFLTCON0 */
#define CFD3FLTCON0     (*(volatile uint32_t *)(_CFD3_BASE + 0x740u))
#define CFD3FLTCON0SET  (*(volatile uint32_t *)(_CFD3_BASE + 0x748u))

#define _CFD3FLTCON0_F0BP_POSITION   0
#define _CFD3FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD3FLTCON0_FLTEN0_POSITION 7
#define _CFD3FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 */
#define CFD3FLTOBJ0     (*(volatile uint32_t *)(_CFD3_BASE + 0x7C0u))
#define _CFD3FLTOBJ0_EXIDE_MASK      0x20000000u

/* CiMASK0 */
#define CFD3MASK0       (*(volatile uint32_t *)(_CFD3_BASE + 0x7D0u))
#define _CFD3MASK0_MIDE_MASK         0x20000000u

#define _IFS5_CAN4IF_MASK   0x10000000u
#define _IEC5_CAN4IE_MASK   0x10000000u

/* -----------------------------------------------------------------------
 * CAN4 (CFD4*) SFR registers — base 0xBF885000
 * Same layout as CFD1; only the base address differs.
 * ----------------------------------------------------------------------- */

#define _CFD4_BASE      0xBF885000u

/* CiCON */
#define CFD4CON         (*(volatile uint32_t *)(_CFD4_BASE + 0x000u))
#define CFD4CONCLR      (*(volatile uint32_t *)(_CFD4_BASE + 0x004u))
#define CFD4CONSET      (*(volatile uint32_t *)(_CFD4_BASE + 0x008u))
#define CFD4CONINV      (*(volatile uint32_t *)(_CFD4_BASE + 0x00Cu))

#define _CFD4CON_CLKSEL0_POSITION   7
#define _CFD4CON_CLKSEL0_MASK       0x00000080u
#define _CFD4CON_ON_MASK            0x00008000u
#define _CFD4CON_STEF_POSITION      19
#define _CFD4CON_STEF_MASK          0x00080000u
#define _CFD4CON_TXQEN_POSITION     20
#define _CFD4CON_TXQEN_MASK         0x00100000u
#define _CFD4CON_OPMOD_POSITION     21
#define _CFD4CON_OPMOD_MASK         0x00E00000u
#define _CFD4CON_REQOP_POSITION     24
#define _CFD4CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD4NBTCFG      (*(volatile uint32_t *)(_CFD4_BASE + 0x010u))

#define _CFD4NBTCFG_SJW_POSITION    0
#define _CFD4NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD4NBTCFG_TSEG2_POSITION  8
#define _CFD4NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD4NBTCFG_TSEG1_POSITION  16
#define _CFD4NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD4NBTCFG_BRP_POSITION    24
#define _CFD4NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD4DBTCFG      (*(volatile uint32_t *)(_CFD4_BASE + 0x020u))

#define _CFD4DBTCFG_SJW_POSITION    0
#define _CFD4DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD4DBTCFG_TSEG2_POSITION  8
#define _CFD4DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD4DBTCFG_TSEG1_POSITION  16
#define _CFD4DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD4DBTCFG_BRP_POSITION    24
#define _CFD4DBTCFG_BRP_MASK        0xFF000000u

/* CiTBC */
#define CFD4TBC         (*(volatile uint32_t *)(_CFD4_BASE + 0x040u))

/* CiTSCON — Timestamp Control */
#define CFD4TSCON       (*(volatile uint32_t *)(_CFD4_BASE + 0x050u))
#define CFD4TSCONCLR    (*(volatile uint32_t *)(_CFD4_BASE + 0x054u))
#define CFD4TSCONSET    (*(volatile uint32_t *)(_CFD4_BASE + 0x058u))

#define _CFD4TSCON_TBCPRE_POSITION  0
#define _CFD4TSCON_TBCPRE_MASK      0x000003FFu
#define _CFD4TSCON_TBCEN_POSITION   16
#define _CFD4TSCON_TBCEN_MASK       0x00010000u
#define _CFD4TSCON_TSEOF_POSITION   17
#define _CFD4TSCON_TSEOF_MASK       0x00020000u
#define _CFD4TSCON_TSRES_POSITION   18
#define _CFD4TSCON_TSRES_MASK       0x00040000u

/* CiVEC */
#define CFD4VEC         (*(volatile uint32_t *)(_CFD4_BASE + 0x060u))
#define _CFD4VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD4INT         (*(volatile uint32_t *)(_CFD4_BASE + 0x070u))
#define CFD4INTCLR      (*(volatile uint32_t *)(_CFD4_BASE + 0x074u))
#define CFD4INTSET      (*(volatile uint32_t *)(_CFD4_BASE + 0x078u))

#define _CFD4INT_TXIF_MASK          0x00000001u
#define _CFD4INT_RXIF_MASK          0x00000002u
#define _CFD4INT_MODIF_MASK         0x00000008u
#define _CFD4INT_SERRIF_MASK        0x00001000u
#define _CFD4INT_CERRIF_MASK        0x00002000u
#define _CFD4INT_IVMIF_MASK         0x00008000u
#define _CFD4INT_TXIE_MASK          0x00010000u
#define _CFD4INT_RXIE_MASK          0x00020000u
#define _CFD4INT_MODIE_MASK         0x00080000u
#define _CFD4INT_SERRIE_MASK        0x10000000u
#define _CFD4INT_CERRIE_MASK        0x20000000u
#define _CFD4INT_IVMIE_MASK         0x80000000u

/* CiRXIF / CiTXIF / CiRXOVIF / CiTXATIF */
#define CFD4RXIF        (*(volatile uint32_t *)(_CFD4_BASE + 0x080u))
#define CFD4TXIF        (*(volatile uint32_t *)(_CFD4_BASE + 0x090u))
#define CFD4RXOVIF      (*(volatile uint32_t *)(_CFD4_BASE + 0x0A0u))
#define CFD4TXATIF      (*(volatile uint32_t *)(_CFD4_BASE + 0x0B0u))

/* CiTREC */
#define CFD4TREC        (*(volatile uint32_t *)(_CFD4_BASE + 0x0D0u))

#define _CFD4TREC_TERRCNT_POSITION  0
#define _CFD4TREC_TERRCNT_MASK      0x000000FFu
#define _CFD4TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD4TREC_EWARN_MASK        0x00010000u
#define _CFD4TREC_RXWARN_MASK       0x00020000u
#define _CFD4TREC_TXWARN_MASK       0x00040000u
#define _CFD4TREC_RXBP_MASK         0x00080000u
#define _CFD4TREC_TXBP_MASK         0x00100000u
#define _CFD4TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD4TEFCON      (*(volatile uint32_t *)(_CFD4_BASE + 0x100u))
#define CFD4TEFCONCLR   (*(volatile uint32_t *)(_CFD4_BASE + 0x104u))
#define CFD4TEFCONSET   (*(volatile uint32_t *)(_CFD4_BASE + 0x108u))

#define _CFD4TEFCON_TEFTSEN_POSITION 5
#define _CFD4TEFCON_TEFTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD4TEFCON_UINC_MASK       0x00000100u
#define _CFD4TEFCON_FSIZE_POSITION  24
#define _CFD4TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD4TEFSTA      (*(volatile uint32_t *)(_CFD4_BASE + 0x110u))
#define _CFD4TEFSTA_TEFNEIF_MASK    0x00000001u

/* CiTEFUA */
#define CFD4TEFUA       (*(volatile uint32_t *)(_CFD4_BASE + 0x120u))

/* CiFIFOBA */
#define CFD4FIFOBA      (*(volatile uint32_t *)(_CFD4_BASE + 0x130u))

/* CiTXQCON */
#define CFD4TXQCON      (*(volatile uint32_t *)(_CFD4_BASE + 0x140u))
#define CFD4TXQCONCLR   (*(volatile uint32_t *)(_CFD4_BASE + 0x144u))
#define CFD4TXQCONSET   (*(volatile uint32_t *)(_CFD4_BASE + 0x148u))

#define _CFD4TXQCON_TXQEIE_MASK     0x00000010u
#define _CFD4TXQCON_UINC_MASK       0x00000100u
#define _CFD4TXQCON_TXREQ_MASK      0x00000200u
#define _CFD4TXQCON_TXPRI_POSITION  16
#define _CFD4TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD4TXQCON_FSIZE_POSITION  24
#define _CFD4TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD4TXQCON_PLSIZE_POSITION 29
#define _CFD4TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD4TXQSTA      (*(volatile uint32_t *)(_CFD4_BASE + 0x150u))
#define _CFD4TXQSTA_TXQNIF_MASK     0x00000001u

/* CiTXQUA */
#define CFD4TXQUA       (*(volatile uint32_t *)(_CFD4_BASE + 0x160u))

/* CiFIFOCON1 */
#define CFD4FIFOCON1    (*(volatile uint32_t *)(_CFD4_BASE + 0x170u))
#define CFD4FIFOCON1CLR (*(volatile uint32_t *)(_CFD4_BASE + 0x174u))
#define CFD4FIFOCON1SET (*(volatile uint32_t *)(_CFD4_BASE + 0x178u))

#define _CFD4FIFOCON1_TFNRFNIE_MASK  0x00000001u
#define _CFD4FIFOCON1_TFERFFIE_MASK  0x00000010u
#define _CFD4FIFOCON1_RTREN_POSITION 6
#define _CFD4FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD4FIFOCON1_TXEN_MASK      0x00000080u
#define _CFD4FIFOCON1_UINC_MASK      0x00000100u
#define _CFD4FIFOCON1_TXREQ_MASK     0x00000200u
#define _CFD4FIFOCON1_TXPRI_POSITION 16
#define _CFD4FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD4FIFOCON1_FSIZE_POSITION 24
#define _CFD4FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD4FIFOCON1_PLSIZE_POSITION 29
#define _CFD4FIFOCON1_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 */
#define CFD4FIFOSTA1    (*(volatile uint32_t *)(_CFD4_BASE + 0x180u))
#define _CFD4FIFOSTA1_TFNRFNIF_MASK  0x00000001u
#define _CFD4FIFOSTA1_TXATIF_MASK    0x00000010u

/* CiFIFOUA1 */
#define CFD4FIFOUA1     (*(volatile uint32_t *)(_CFD4_BASE + 0x190u))

/* CiFIFOCON2 (FIFO2 = RX FIFO) */
#define CFD4FIFOCON2    (*(volatile uint32_t *)(_CFD4_BASE + 0x1A0u))
#define CFD4FIFOCON2CLR (*(volatile uint32_t *)(_CFD4_BASE + 0x1A4u))
#define CFD4FIFOCON2SET (*(volatile uint32_t *)(_CFD4_BASE + 0x1A8u))

#define _CFD4FIFOCON2_RXTSEN_POSITION 5
#define _CFD4FIFOCON2_RXTSEN_MASK    0x00000020u   /* bit 5 */
#define _CFD4FIFOCON2_FSIZE_POSITION 24
#define _CFD4FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD4FIFOCON2_PLSIZE_POSITION 29
#define _CFD4FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA2 */
#define CFD4FIFOSTA2    (*(volatile uint32_t *)(_CFD4_BASE + 0x1B0u))
#define _CFD4FIFOSTA2_TFNRFNIF_MASK  0x00000001u

/* CiFIFOUA2 */
#define CFD4FIFOUA2     (*(volatile uint32_t *)(_CFD4_BASE + 0x1C0u))

/* CiFLTCON0 */
#define CFD4FLTCON0     (*(volatile uint32_t *)(_CFD4_BASE + 0x740u))
#define CFD4FLTCON0SET  (*(volatile uint32_t *)(_CFD4_BASE + 0x748u))

#define _CFD4FLTCON0_F0BP_POSITION   0
#define _CFD4FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD4FLTCON0_FLTEN0_POSITION 7
#define _CFD4FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 */
#define CFD4FLTOBJ0     (*(volatile uint32_t *)(_CFD4_BASE + 0x7C0u))
#define _CFD4FLTOBJ0_EXIDE_MASK      0x20000000u

/* CiMASK0 */
#define CFD4MASK0       (*(volatile uint32_t *)(_CFD4_BASE + 0x7D0u))
#define _CFD4MASK0_MIDE_MASK         0x20000000u

/* -----------------------------------------------------------------------
 * IFS7 / IEC7  (sources 224–255)
 * USB2 = EVIC source 244 → IFS7/IEC7 bit 20
 * IFS7 at EVIC+0x00B0, IEC7 at EVIC+0x0130
 * ----------------------------------------------------------------------- */

#define IFS7            (*(volatile uint32_t *)(_EVIC_BASE + 0x00B0u))
#define IFS7CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00B4u))
#define IFS7SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00B8u))
#define IFS7INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00BCu))

#define IEC7            (*(volatile uint32_t *)(_EVIC_BASE + 0x0130u))
#define IEC7CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0134u))
#define IEC7SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0138u))
#define IEC7INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x013Cu))

#define _IFS7_USB2IF_MASK   (1u << 20)   /* source 244 = IFS7 bit 20 */
#define _IEC7_USB2IE_MASK   (1u << 20)

/* USB1 interrupt: source 34 = IFS1 bit 2 */
#define _IFS1_USB1IF_MASK   (1u << 2)
#define _IEC1_USB1IE_MASK   (1u << 2)

/* -----------------------------------------------------------------------
 * USB1 registers  (base 0xBF889000, first user register at +0x040)
 * USB2 registers  (base 0xBF88A000, first user register at +0x040)
 *
 * _USB_BASE_ADDRESS / _USB2_BASE_ADDRESS match XC32 p32mk1024mcm100.h:
 *   USB1 = 0xBF889040,  USB2 = 0xBF88A040
 * USB_MODULE_ID is a typedef uint32_t (peripheral identified by base addr).
 * ----------------------------------------------------------------------- */

#define _USB_BASE_ADDRESS   0xBF889040u
#define _USB2_BASE_ADDRESS  0xBF88A040u
#define USB_NUMBER_OF_MODULES 2U

/* -----------------------------------------------------------------------
 * SPI5 registers (base 0xBF847800, verified against p32mk1024mcm100.h)
 * ----------------------------------------------------------------------- */

#define SPI5CON         (*(volatile uint32_t *)(0xBF847800u))
#define SPI5CONCLR      (*(volatile uint32_t *)(0xBF847804u))
#define SPI5CONSET      (*(volatile uint32_t *)(0xBF847808u))
#define SPI5CONINV      (*(volatile uint32_t *)(0xBF84780Cu))

#define SPI5STAT        (*(volatile uint32_t *)(0xBF847810u))
#define SPI5STATCLR     (*(volatile uint32_t *)(0xBF847814u))
#define SPI5STATSET     (*(volatile uint32_t *)(0xBF847818u))
#define SPI5STATINV     (*(volatile uint32_t *)(0xBF84781Cu))

#define SPI5BUF         (*(volatile uint32_t *)(0xBF847820u))
#define SPI5BRG         (*(volatile uint32_t *)(0xBF847830u))

#define SPI5CON2        (*(volatile uint32_t *)(0xBF847840u))
#define SPI5CON2CLR     (*(volatile uint32_t *)(0xBF847844u))
#define SPI5CON2SET     (*(volatile uint32_t *)(0xBF847848u))
#define SPI5CON2INV     (*(volatile uint32_t *)(0xBF84784Cu))

/* SPI5CON bit positions and masks (subset needed by Harmony plib) */
#define _SPI5CON_SRXISEL_POSITION  0u
#define _SPI5CON_STXISEL_POSITION  2u
#define _SPI5CON_MSTEN_POSITION    5u
#define _SPI5CON_CKP_POSITION      6u
#define _SPI5CON_SSEN_POSITION     7u
#define _SPI5CON_CKE_POSITION      8u
#define _SPI5CON_SMP_POSITION      9u
#define _SPI5CON_MODE16_POSITION   10u
#define _SPI5CON_MODE32_POSITION   11u
#define _SPI5CON_ENHBUF_POSITION   16u
#define _SPI5CON_MCLKSEL_POSITION  23u
#define _SPI5CON_MSSEN_POSITION    28u

#define _SPI5CON_SRXISEL_MASK      (0x3u << _SPI5CON_SRXISEL_POSITION)
#define _SPI5CON_STXISEL_MASK      (0x3u << _SPI5CON_STXISEL_POSITION)
#define _SPI5CON_CKP_MASK          (1u << _SPI5CON_CKP_POSITION)
#define _SPI5CON_CKE_MASK          (1u << _SPI5CON_CKE_POSITION)
#define _SPI5CON_MODE16_MASK       (1u << _SPI5CON_MODE16_POSITION)
#define _SPI5CON_MODE32_MASK       (1u << _SPI5CON_MODE32_POSITION)
#define _SPI5CON_ON_MASK           (1u << 15u)

/* SPI5STAT masks (subset needed by Harmony plib) */
#define _SPI5STAT_SPITBF_MASK      (1u << 1u)
#define _SPI5STAT_SPITBE_MASK      (1u << 3u)
#define _SPI5STAT_SPIRBE_MASK      (1u << 5u)
#define _SPI5STAT_SPIROV_MASK      (1u << 6u)
#define _SPI5STAT_SRMT_MASK        (1u << 11u)

/* SPI5CON2 masks */
#define _SPI5CON2_SPIROVEN_MASK    (1u << 0u)

/* SPI5 interrupt masks (IEC5/IFS5 bits 9..11) */
#define _IFS5_SPI5EIF_MASK         (1u << 9u)
#define _IFS5_SPI5RXIF_MASK        (1u << 10u)
#define _IFS5_SPI5TXIF_MASK        (1u << 11u)
#define _IEC5_SPI5EIE_MASK         (1u << 9u)
#define _IEC5_SPI5RXIE_MASK        (1u << 10u)
#define _IEC5_SPI5TXIE_MASK        (1u << 11u)

/* -----------------------------------------------------------------------
 * Vector numbers (used in configTICK_INTERRUPT_VECTOR)
 * ----------------------------------------------------------------------- */

#define _TIMER_1_VECTOR         4
#define _CORE_TIMER_VECTOR      0
#define _USB_1_VECTOR           34U
#define _USB_2_VECTOR           244U

/* SPI4-6 vectors */
#define _SPI4_FAULT_VECTOR      166U
#define _SPI4_RX_VECTOR         167U
#define _SPI4_TX_VECTOR         168U
#define _SPI5_FAULT_VECTOR      169U
#define _SPI5_RX_VECTOR         170U
#define _SPI5_TX_VECTOR         171U
#define _SPI6_FAULT_VECTOR      172U
#define _SPI6_RX_VECTOR         173U
#define _SPI6_TX_VECTOR         174U

/* -----------------------------------------------------------------------
 * USB register bit-field typedef structs
 * Extracted from XC32 p32mk1024mcm100.h (extern declarations omitted —
 * we use our own volatile-pointer macros for register access).
 * Required by driver/usb/usbfs/src/templates/usbfs_registers.h which
 * does #include <xc.h> and uses these types directly.
 * ----------------------------------------------------------------------- */

typedef struct {
  uint32_t VBUSVDIF:1;
  uint32_t :1;
  uint32_t SESENDIF:1;
  uint32_t SESVDIF:1;
  uint32_t ACTVIF:1;
  uint32_t LSTATEIF:1;
  uint32_t T1MSECIF:1;
  uint32_t IDIF:1;
} __U1OTGIRbits_t;

typedef struct {
  uint32_t VBUSVDIE:1;
  uint32_t :1;
  uint32_t SESENDIE:1;
  uint32_t SESVDIE:1;
  uint32_t ACTVIE:1;
  uint32_t LSTATEIE:1;
  uint32_t T1MSECIE:1;
  uint32_t IDIE:1;
} __U1OTGIEbits_t;

typedef struct {
  uint32_t VBUSVD:1;
  uint32_t :1;
  uint32_t SESEND:1;
  uint32_t SESVD:1;
  uint32_t :1;
  uint32_t LSTATE:1;
  uint32_t :1;
  uint32_t ID:1;
} __U1OTGSTATbits_t;

typedef struct {
  uint32_t VBUSDIS:1;
  uint32_t VBUSCHG:1;
  uint32_t OTGEN:1;
  uint32_t VBUSON:1;
  uint32_t DMPULDWN:1;
  uint32_t DPPULDWN:1;
  uint32_t DMPULUP:1;
  uint32_t DPPULUP:1;
} __U1OTGCONbits_t;

typedef struct {
  uint32_t USBPWR:1;
  uint32_t USUSPEND:1;
  uint32_t :1;
  uint32_t USBBUSY:1;
  uint32_t USLPGRD:1;
  uint32_t :2;
  uint32_t UACTPND:1;
} __U1PWRCbits_t;

typedef union {
  struct {
    uint32_t URSTIF_DETACHIF:1;
    uint32_t UERRIF:1;
    uint32_t SOFIF:1;
    uint32_t TRNIF:1;
    uint32_t IDLEIF:1;
    uint32_t RESUMEIF:1;
    uint32_t ATTACHIF:1;
    uint32_t STALLIF:1;
  };
  struct {
    uint32_t DETACHIF:1;
  };
  struct {
    uint32_t URSTIF:1;
  };
} __U1IRbits_t;

typedef union {
  struct {
    uint32_t URSTIE_DETACHIE:1;
    uint32_t UERRIE:1;
    uint32_t SOFIE:1;
    uint32_t TRNIE:1;
    uint32_t IDLEIE:1;
    uint32_t RESUMEIE:1;
    uint32_t ATTACHIE:1;
    uint32_t STALLIE:1;
  };
  struct {
    uint32_t DETACHIE:1;
  };
  struct {
    uint32_t URSTIE:1;
  };
} __U1IEbits_t;

typedef union {
  struct {
    uint32_t PIDEF:1;
    uint32_t CRC5EF_EOFEF:1;
    uint32_t CRC16EF:1;
    uint32_t DFN8EF:1;
    uint32_t BTOEF:1;
    uint32_t DMAEF:1;
    uint32_t BMXEF:1;
    uint32_t BTSEF:1;
  };
  struct {
    uint32_t :1;
    uint32_t CRC5EF:1;
  };
  struct {
    uint32_t :1;
    uint32_t EOFEF:1;
  };
} __U1EIRbits_t;

typedef union {
  struct {
    uint32_t PIDEE:1;
    uint32_t CRC5EE_EOFEE:1;
    uint32_t CRC16EE:1;
    uint32_t DFN8EE:1;
    uint32_t BTOEE:1;
    uint32_t DMAEE:1;
    uint32_t BMXEE:1;
    uint32_t BTSEE:1;
  };
  struct {
    uint32_t :1;
    uint32_t CRC5EE:1;
  };
  struct {
    uint32_t :1;
    uint32_t EOFEE:1;
  };
} __U1EIEbits_t;

typedef union {
  struct {
    uint32_t :2;
    uint32_t PPBI:1;
    uint32_t DIR:1;
    uint32_t ENDPT:4;
  };
  struct {
    uint32_t :4;
    uint32_t ENDPT0:1;
    uint32_t ENDPT1:1;
    uint32_t ENDPT2:1;
    uint32_t ENDPT3:1;
  };
} __U1STATbits_t;

typedef union {
  struct {
    uint32_t USBEN_SOFEN:1;
    uint32_t PPBRST:1;
    uint32_t RESUME:1;
    uint32_t HOSTEN:1;
    uint32_t USBRST:1;
    uint32_t PKTDIS_TOKBUSY:1;
    uint32_t SE0:1;
    uint32_t JSTATE:1;
  };
  struct {
    uint32_t USBEN:1;
  };
  struct {
    uint32_t SOFEN:1;
    uint32_t :4;
    uint32_t PKTDIS:1;
  };
  struct {
    uint32_t :5;
    uint32_t TOKBUSY:1;
  };
} __U1CONbits_t;

typedef union {
  struct {
    uint32_t DEVADDR:7;
    uint32_t LSPDEN:1;
  };
  struct {
    uint32_t DEVADDR0:1;
    uint32_t DEVADDR1:1;
    uint32_t DEVADDR2:1;
    uint32_t DEVADDR3:1;
    uint32_t DEVADDR4:1;
    uint32_t DEVADDR5:1;
    uint32_t DEVADDR6:1;
  };
} __U1ADDRbits_t;

typedef struct {
  uint32_t :1;
  uint32_t BDTPTRL:7;
} __U1BDTP1bits_t;

typedef union {
  struct {
    uint32_t FRML:8;
  };
  struct {
    uint32_t FRM0:1;
    uint32_t FRM1:1;
    uint32_t FRM2:1;
    uint32_t FRM3:1;
    uint32_t FRM4:1;
    uint32_t FRM5:1;
    uint32_t FRM6:1;
    uint32_t FRM7:1;
  };
} __U1FRMLbits_t;

typedef union {
  struct {
    uint32_t FRMH:3;
  };
  struct {
    uint32_t FRM8:1;
    uint32_t FRM9:1;
    uint32_t FRM10:1;
  };
} __U1FRMHbits_t;

typedef union {
  struct {
    uint32_t EP:4;
    uint32_t PID:4;
  };
  struct {
    uint32_t EP0:1;
  };
  struct {
    uint32_t :1;
    uint32_t EP1:1;
    uint32_t EP2:1;
    uint32_t EP3:1;
    uint32_t PID0:1;
  };
  struct {
    uint32_t :5;
    uint32_t PID1:1;
    uint32_t PID2:1;
    uint32_t PID3:1;
  };
} __U1TOKbits_t;

typedef struct {
  uint32_t CNT:8;
} __U1SOFbits_t;

typedef struct {
  uint32_t BDTPTRH:8;
} __U1BDTP2bits_t;

typedef struct {
  uint32_t BDTPTRU:8;
} __U1BDTP3bits_t;

typedef struct {
  uint32_t UASUSPND:1;
  uint32_t :2;
  uint32_t LSDEV:1;
  uint32_t USBSIDL:1;
  uint32_t :1;
  uint32_t UOEMON:1;
  uint32_t UTEYE:1;
} __U1CNFG1bits_t;

typedef struct {
  uint32_t EPHSHK:1;
  uint32_t EPSTALL:1;
  uint32_t EPTXEN:1;
  uint32_t EPRXEN:1;
  uint32_t EPCONDIS:1;
  uint32_t :1;
  uint32_t RETRYDIS:1;
  uint32_t LSPD:1;
} __U1EP0bits_t;

/* EP1–EP15 share the same layout (no RETRYDIS/LSPD fields) */
typedef struct {
  uint32_t EPHSHK:1;
  uint32_t EPSTALL:1;
  uint32_t EPTXEN:1;
  uint32_t EPRXEN:1;
  uint32_t EPCONDIS:1;
} __U1EP1bits_t, __U1EP2bits_t, __U1EP3bits_t, __U1EP4bits_t,
  __U1EP5bits_t, __U1EP6bits_t, __U1EP7bits_t, __U1EP8bits_t,
  __U1EP9bits_t, __U1EP10bits_t, __U1EP11bits_t, __U1EP12bits_t,
  __U1EP13bits_t, __U1EP14bits_t, __U1EP15bits_t;

/* -----------------------------------------------------------------------
 * USB register bit-mask and position constants
 * Extracted from XC32 p32mk1024mcm100.h for usbfs_registers.h compatibility.
 * ----------------------------------------------------------------------- */
#define _U1ADDR_DEVADDR_MASK                     0x0000007Fu
#define _U1ADDR_LSPDEN_MASK                      0x00000080u
#define _U1CNFG1_UASUSPND_MASK                   0x00000001u
#define _U1CNFG1_USBSIDL_MASK                    0x00000010u
#define _U1CNFG1_UTEYE_MASK                      0x00000080u
#define _U1CNFG1_UTEYE_POSITION                  0x00000007u
#define _U1CON_HOSTEN_MASK                       0x00000008u
#define _U1CON_PKTDIS_TOKBUSY_MASK               0x00000020u
#define _U1CON_PPBRST_MASK                       0x00000002u
#define _U1CON_RESUME_MASK                       0x00000004u
#define _U1CON_USBEN_SOFEN_MASK                  0x00000001u
#define _U1CON_USBEN_SOFEN_POSITION              0x00000000u
#define _U1CON_USBRST_MASK                       0x00000010u
#define _U1EP0_EPCONDIS_MASK                     0x00000010u
#define _U1EP0_EPCONDIS_POSITION                 0x00000004u
#define _U1EP0_EPHSHK_MASK                       0x00000001u
#define _U1EP0_EPHSHK_POSITION                   0x00000000u
#define _U1EP0_EPRXEN_MASK                       0x00000008u
#define _U1EP0_EPSTALL_MASK                      0x00000002u
#define _U1EP0_EPSTALL_POSITION                  0x00000001u
#define _U1EP0_EPTXEN_MASK                       0x00000004u
#define _U1EP0_LSPD_MASK                         0x00000080u
#define _U1EP0_RETRYDIS_MASK                     0x00000040u
#define _U1EP1_EPCONDIS_POSITION                 0x00000004u
#define _U1EP1_EPHSHK_POSITION                   0x00000000u
#define _U1EP1_EPSTALL_POSITION                  0x00000001u
#define _U1OTGCON_DMPULDWN_MASK                  0x00000010u
#define _U1OTGCON_DMPULUP_MASK                   0x00000040u
#define _U1OTGCON_DPPULDWN_MASK                  0x00000020u
#define _U1OTGCON_DPPULUP_MASK                   0x00000080u
#define _U1OTGCON_OTGEN_MASK                     0x00000004u
#define _U1OTGCON_VBUSCHG_MASK                   0x00000002u
#define _U1OTGCON_VBUSDIS_MASK                   0x00000001u
#define _U1OTGCON_VBUSON_MASK                    0x00000008u
#define _U1PWRC_USBPWR_MASK                      0x00000001u
#define _U1PWRC_USLPGRD_MASK                     0x00000010u
#define _U1PWRC_USUSPEND_MASK                    0x00000002u
#define _U1TOK_EP_MASK                           0x0000000Fu
#define _U1TOK_PID_POSITION                      0x00000004u

/* -----------------------------------------------------------------------
 * GPIO PORTA–PORTG SFR definitions
 * Base: 0xBF860000; each port occupies 0x100 bytes.
 * Offsets per port block (§12, DS60001519E):
 *   ANSEL +0x00/04/08/0C  TRIS  +0x10/14/18/1C  PORT  +0x20
 *   LAT   +0x30/34/38/3C  CNCON +0x70/74/78/7C
 *   CNEN0 +0x80/84/88/8C  (plib: CNENA/CLR/SET)
 *   CNSTAT+0x90           CNEN1 +0xA0/A4/A8/AC (plib: CNNEA/CLR/SET)
 * ----------------------------------------------------------------------- */
#define _GPIO_BASE_A    0xBF860000u
#define _GPIO_BASE_B    0xBF860100u
#define _GPIO_BASE_C    0xBF860200u
#define _GPIO_BASE_D    0xBF860300u
#define _GPIO_BASE_E    0xBF860400u
#define _GPIO_BASE_F    0xBF860500u
#define _GPIO_BASE_G    0xBF860600u

/* Expands all register names for one port given its letter and base address */
#define _GPIO_PORT_REGS(L, BASE) \
    static volatile uint32_t * const _gpio_dummy_##L = (volatile uint32_t *)(BASE); \
    (void)_gpio_dummy_##L; /* suppress unused warning if any */

/* Instead of the above approach, define each register directly: */

/* PORTA */
#define ANSELA          (*(volatile uint32_t *)(_GPIO_BASE_A + 0x00u))
#define ANSELACLR       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x04u))
#define ANSELASET       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x08u))
#define ANSELAINV       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x0Cu))
#define TRISA           (*(volatile uint32_t *)(_GPIO_BASE_A + 0x10u))
#define TRISACLR        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x14u))
#define TRISASET        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x18u))
#define TRISAINV        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x1Cu))
#define PORTA           (*(volatile uint32_t *)(_GPIO_BASE_A + 0x20u))
#define LATA            (*(volatile uint32_t *)(_GPIO_BASE_A + 0x30u))
#define LATACLR         (*(volatile uint32_t *)(_GPIO_BASE_A + 0x34u))
#define LATASET         (*(volatile uint32_t *)(_GPIO_BASE_A + 0x38u))
#define LATAINV         (*(volatile uint32_t *)(_GPIO_BASE_A + 0x3Cu))
#define CNCONA          (*(volatile uint32_t *)(_GPIO_BASE_A + 0x70u))
#define CNCONACLR       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x74u))
#define CNCONASET       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x78u))
#define CNCONAINV       (*(volatile uint32_t *)(_GPIO_BASE_A + 0x7Cu))
#define CNENA           (*(volatile uint32_t *)(_GPIO_BASE_A + 0x80u))  /* CNEN0A */
#define CNENACLR        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x84u))
#define CNENASET        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x88u))
#define CNENAINV        (*(volatile uint32_t *)(_GPIO_BASE_A + 0x8Cu))
#define CNSTATA         (*(volatile uint32_t *)(_GPIO_BASE_A + 0x90u))
#define CNSTATACLR      (*(volatile uint32_t *)(_GPIO_BASE_A + 0x94u))
#define CNNEA           (*(volatile uint32_t *)(_GPIO_BASE_A + 0xA0u))  /* CNEN1A */
#define CNNEACLR        (*(volatile uint32_t *)(_GPIO_BASE_A + 0xA4u))
#define CNNEASET        (*(volatile uint32_t *)(_GPIO_BASE_A + 0xA8u))

/* PORTB */
#define ANSELB          (*(volatile uint32_t *)(_GPIO_BASE_B + 0x00u))
#define ANSELBCLR       (*(volatile uint32_t *)(_GPIO_BASE_B + 0x04u))
#define ANSELBSET       (*(volatile uint32_t *)(_GPIO_BASE_B + 0x08u))
#define ANSELB_INV      (*(volatile uint32_t *)(_GPIO_BASE_B + 0x0Cu))
#define TRISB           (*(volatile uint32_t *)(_GPIO_BASE_B + 0x10u))
#define TRISBCLR        (*(volatile uint32_t *)(_GPIO_BASE_B + 0x14u))
#define TRISBSET        (*(volatile uint32_t *)(_GPIO_BASE_B + 0x18u))
#define PORTB           (*(volatile uint32_t *)(_GPIO_BASE_B + 0x20u))
#define LATB            (*(volatile uint32_t *)(_GPIO_BASE_B + 0x30u))
#define LATBCLR         (*(volatile uint32_t *)(_GPIO_BASE_B + 0x34u))
#define LATBSET         (*(volatile uint32_t *)(_GPIO_BASE_B + 0x38u))
#define LATBINV         (*(volatile uint32_t *)(_GPIO_BASE_B + 0x3Cu))
#define CNCONB          (*(volatile uint32_t *)(_GPIO_BASE_B + 0x70u))
#define CNCONBCLR       (*(volatile uint32_t *)(_GPIO_BASE_B + 0x74u))
#define CNCONBSET       (*(volatile uint32_t *)(_GPIO_BASE_B + 0x78u))
#define CNENB           (*(volatile uint32_t *)(_GPIO_BASE_B + 0x80u))
#define CNENBCLR        (*(volatile uint32_t *)(_GPIO_BASE_B + 0x84u))
#define CNENBSET        (*(volatile uint32_t *)(_GPIO_BASE_B + 0x88u))
#define CNSTATB         (*(volatile uint32_t *)(_GPIO_BASE_B + 0x90u))
#define CNSTATBCLR      (*(volatile uint32_t *)(_GPIO_BASE_B + 0x94u))
#define CNNEB           (*(volatile uint32_t *)(_GPIO_BASE_B + 0xA0u))
#define CNNEBCLR        (*(volatile uint32_t *)(_GPIO_BASE_B + 0xA4u))
#define CNNEBSET        (*(volatile uint32_t *)(_GPIO_BASE_B + 0xA8u))

/* PORTC */
#define ANSELC          (*(volatile uint32_t *)(_GPIO_BASE_C + 0x00u))
#define ANSELCCLR       (*(volatile uint32_t *)(_GPIO_BASE_C + 0x04u))
#define ANSELCSET       (*(volatile uint32_t *)(_GPIO_BASE_C + 0x08u))
#define TRISC           (*(volatile uint32_t *)(_GPIO_BASE_C + 0x10u))
#define TRISCCLR        (*(volatile uint32_t *)(_GPIO_BASE_C + 0x14u))
#define TRISCSET        (*(volatile uint32_t *)(_GPIO_BASE_C + 0x18u))
#define PORTC           (*(volatile uint32_t *)(_GPIO_BASE_C + 0x20u))
#define LATC            (*(volatile uint32_t *)(_GPIO_BASE_C + 0x30u))
#define LATCCLR         (*(volatile uint32_t *)(_GPIO_BASE_C + 0x34u))
#define LATCSET         (*(volatile uint32_t *)(_GPIO_BASE_C + 0x38u))
#define LATCINV         (*(volatile uint32_t *)(_GPIO_BASE_C + 0x3Cu))
#define CNCONC          (*(volatile uint32_t *)(_GPIO_BASE_C + 0x70u))
#define CNCONCCLR       (*(volatile uint32_t *)(_GPIO_BASE_C + 0x74u))
#define CNCONCSET       (*(volatile uint32_t *)(_GPIO_BASE_C + 0x78u))
#define CNENC           (*(volatile uint32_t *)(_GPIO_BASE_C + 0x80u))
#define CNENCCLR        (*(volatile uint32_t *)(_GPIO_BASE_C + 0x84u))
#define CNENCSET        (*(volatile uint32_t *)(_GPIO_BASE_C + 0x88u))
#define CNSTATC         (*(volatile uint32_t *)(_GPIO_BASE_C + 0x90u))
#define CNSTATCCLR      (*(volatile uint32_t *)(_GPIO_BASE_C + 0x94u))
#define CNNEC           (*(volatile uint32_t *)(_GPIO_BASE_C + 0xA0u))
#define CNNECCLR        (*(volatile uint32_t *)(_GPIO_BASE_C + 0xA4u))
#define CNNECSET        (*(volatile uint32_t *)(_GPIO_BASE_C + 0xA8u))

/* PORTD */
#define ANSELD          (*(volatile uint32_t *)(_GPIO_BASE_D + 0x00u))
#define ANSELDCLR       (*(volatile uint32_t *)(_GPIO_BASE_D + 0x04u))
#define ANSELDSET       (*(volatile uint32_t *)(_GPIO_BASE_D + 0x08u))
#define TRISD           (*(volatile uint32_t *)(_GPIO_BASE_D + 0x10u))
#define TRISDCLR        (*(volatile uint32_t *)(_GPIO_BASE_D + 0x14u))
#define TRISDSET        (*(volatile uint32_t *)(_GPIO_BASE_D + 0x18u))
#define PORTD           (*(volatile uint32_t *)(_GPIO_BASE_D + 0x20u))
#define LATD            (*(volatile uint32_t *)(_GPIO_BASE_D + 0x30u))
#define LATDCLR         (*(volatile uint32_t *)(_GPIO_BASE_D + 0x34u))
#define LATDSET         (*(volatile uint32_t *)(_GPIO_BASE_D + 0x38u))
#define LATDINV         (*(volatile uint32_t *)(_GPIO_BASE_D + 0x3Cu))
#define CNCOND          (*(volatile uint32_t *)(_GPIO_BASE_D + 0x70u))
#define CNCONDCLR       (*(volatile uint32_t *)(_GPIO_BASE_D + 0x74u))
#define CNCONDSET       (*(volatile uint32_t *)(_GPIO_BASE_D + 0x78u))
#define CNEND           (*(volatile uint32_t *)(_GPIO_BASE_D + 0x80u))
#define CNENDCLR        (*(volatile uint32_t *)(_GPIO_BASE_D + 0x84u))
#define CNENDSET        (*(volatile uint32_t *)(_GPIO_BASE_D + 0x88u))
#define CNSTATD         (*(volatile uint32_t *)(_GPIO_BASE_D + 0x90u))
#define CNSTATDCLR      (*(volatile uint32_t *)(_GPIO_BASE_D + 0x94u))
#define CNNED           (*(volatile uint32_t *)(_GPIO_BASE_D + 0xA0u))
#define CNNEDCLR        (*(volatile uint32_t *)(_GPIO_BASE_D + 0xA4u))
#define CNNEDSET        (*(volatile uint32_t *)(_GPIO_BASE_D + 0xA8u))

/* PORTE */
#define ANSELE          (*(volatile uint32_t *)(_GPIO_BASE_E + 0x00u))
#define ANSELECLR       (*(volatile uint32_t *)(_GPIO_BASE_E + 0x04u))
#define ANSELESET       (*(volatile uint32_t *)(_GPIO_BASE_E + 0x08u))
#define TRISE           (*(volatile uint32_t *)(_GPIO_BASE_E + 0x10u))
#define TRISECLR        (*(volatile uint32_t *)(_GPIO_BASE_E + 0x14u))
#define TRISESET        (*(volatile uint32_t *)(_GPIO_BASE_E + 0x18u))
#define PORTE           (*(volatile uint32_t *)(_GPIO_BASE_E + 0x20u))
#define LATE            (*(volatile uint32_t *)(_GPIO_BASE_E + 0x30u))
#define LATECLR         (*(volatile uint32_t *)(_GPIO_BASE_E + 0x34u))
#define LATESET         (*(volatile uint32_t *)(_GPIO_BASE_E + 0x38u))
#define LATEINV         (*(volatile uint32_t *)(_GPIO_BASE_E + 0x3Cu))
#define CNCONE          (*(volatile uint32_t *)(_GPIO_BASE_E + 0x70u))
#define CNCONECLR       (*(volatile uint32_t *)(_GPIO_BASE_E + 0x74u))
#define CNCONESET       (*(volatile uint32_t *)(_GPIO_BASE_E + 0x78u))
#define CNENE           (*(volatile uint32_t *)(_GPIO_BASE_E + 0x80u))
#define CNENECLR        (*(volatile uint32_t *)(_GPIO_BASE_E + 0x84u))
#define CNENESET        (*(volatile uint32_t *)(_GPIO_BASE_E + 0x88u))
#define CNSTATE         (*(volatile uint32_t *)(_GPIO_BASE_E + 0x90u))
#define CNSTATECLR      (*(volatile uint32_t *)(_GPIO_BASE_E + 0x94u))
#define CNNEE           (*(volatile uint32_t *)(_GPIO_BASE_E + 0xA0u))
#define CNNEECLR        (*(volatile uint32_t *)(_GPIO_BASE_E + 0xA4u))
#define CNNEESET        (*(volatile uint32_t *)(_GPIO_BASE_E + 0xA8u))

/* PORTF */
#define ANSELF          (*(volatile uint32_t *)(_GPIO_BASE_F + 0x00u))
#define ANSELFCLR       (*(volatile uint32_t *)(_GPIO_BASE_F + 0x04u))
#define ANSELFSET       (*(volatile uint32_t *)(_GPIO_BASE_F + 0x08u))
#define TRISF           (*(volatile uint32_t *)(_GPIO_BASE_F + 0x10u))
#define TRISFCLR        (*(volatile uint32_t *)(_GPIO_BASE_F + 0x14u))
#define TRISFSET        (*(volatile uint32_t *)(_GPIO_BASE_F + 0x18u))
#define PORTF           (*(volatile uint32_t *)(_GPIO_BASE_F + 0x20u))
#define LATF            (*(volatile uint32_t *)(_GPIO_BASE_F + 0x30u))
#define LATFCLR         (*(volatile uint32_t *)(_GPIO_BASE_F + 0x34u))
#define LATFSET         (*(volatile uint32_t *)(_GPIO_BASE_F + 0x38u))
#define LATFINV         (*(volatile uint32_t *)(_GPIO_BASE_F + 0x3Cu))
#define CNCONF          (*(volatile uint32_t *)(_GPIO_BASE_F + 0x70u))
#define CNCONFCLR       (*(volatile uint32_t *)(_GPIO_BASE_F + 0x74u))
#define CNCONFSET       (*(volatile uint32_t *)(_GPIO_BASE_F + 0x78u))
#define CNENF           (*(volatile uint32_t *)(_GPIO_BASE_F + 0x80u))
#define CNENFCLR        (*(volatile uint32_t *)(_GPIO_BASE_F + 0x84u))
#define CNENFSET        (*(volatile uint32_t *)(_GPIO_BASE_F + 0x88u))
#define CNSTATF         (*(volatile uint32_t *)(_GPIO_BASE_F + 0x90u))
#define CNSTATFCLR      (*(volatile uint32_t *)(_GPIO_BASE_F + 0x94u))
#define CNNEF           (*(volatile uint32_t *)(_GPIO_BASE_F + 0xA0u))
#define CNNEFCLR        (*(volatile uint32_t *)(_GPIO_BASE_F + 0xA4u))
#define CNNEFSET        (*(volatile uint32_t *)(_GPIO_BASE_F + 0xA8u))

/* PORTG */
#define ANSELG          (*(volatile uint32_t *)(_GPIO_BASE_G + 0x00u))
#define ANSELGCLR       (*(volatile uint32_t *)(_GPIO_BASE_G + 0x04u))
#define ANSELGSET       (*(volatile uint32_t *)(_GPIO_BASE_G + 0x08u))
#define ANSELGINV       (*(volatile uint32_t *)(_GPIO_BASE_G + 0x0Cu))
#define TRISG           (*(volatile uint32_t *)(_GPIO_BASE_G + 0x10u))
#define TRISGCLR        (*(volatile uint32_t *)(_GPIO_BASE_G + 0x14u))
#define TRISGSET        (*(volatile uint32_t *)(_GPIO_BASE_G + 0x18u))
#define TRISGCLR        (*(volatile uint32_t *)(_GPIO_BASE_G + 0x14u))
#define PORTG           (*(volatile uint32_t *)(_GPIO_BASE_G + 0x20u))
#define LATG            (*(volatile uint32_t *)(_GPIO_BASE_G + 0x30u))
#define LATGCLR         (*(volatile uint32_t *)(_GPIO_BASE_G + 0x34u))
#define LATGSET         (*(volatile uint32_t *)(_GPIO_BASE_G + 0x38u))
#define LATGINV         (*(volatile uint32_t *)(_GPIO_BASE_G + 0x3Cu))
#define CNCONG          (*(volatile uint32_t *)(_GPIO_BASE_G + 0x70u))
#define CNCONGCLR       (*(volatile uint32_t *)(_GPIO_BASE_G + 0x74u))
#define CNCONGSET       (*(volatile uint32_t *)(_GPIO_BASE_G + 0x78u))
#define CNENG           (*(volatile uint32_t *)(_GPIO_BASE_G + 0x80u))
#define CNENGCLR        (*(volatile uint32_t *)(_GPIO_BASE_G + 0x84u))
#define CNENGSET        (*(volatile uint32_t *)(_GPIO_BASE_G + 0x88u))
#define CNSTATG         (*(volatile uint32_t *)(_GPIO_BASE_G + 0x90u))
#define CNSTATGCLR      (*(volatile uint32_t *)(_GPIO_BASE_G + 0x94u))
#define CNNEG           (*(volatile uint32_t *)(_GPIO_BASE_G + 0xA0u))
#define CNNEGCLR        (*(volatile uint32_t *)(_GPIO_BASE_G + 0xA4u))
#define CNNEGSET        (*(volatile uint32_t *)(_GPIO_BASE_G + 0xA8u))

/* CNCON.ON bit (bit 15) — same for all ports */
#define _CNCONA_ON_MASK     0x00008000u
#define _CNCONB_ON_MASK     0x00008000u
#define _CNCONC_ON_MASK     0x00008000u
#define _CNCOND_ON_MASK     0x00008000u
#define _CNCONE_ON_MASK     0x00008000u
#define _CNCONF_ON_MASK     0x00008000u
#define _CNCONG_ON_MASK     0x00008000u

/* IFS1 / IEC1 — Change-Notice interrupt flag/enable bits (§8, DS60001519E)
 * IFS1 bit position = vector_number - 32:  CNA=44→bit12, CNB=45→bit13 … CNG=50→bit18 */
#define _IFS1_CNAIF_MASK    0x00001000u
#define _IFS1_CNBIF_MASK    0x00002000u
#define _IFS1_CNCIF_MASK    0x00004000u
#define _IFS1_CNDIF_MASK    0x00008000u
#define _IFS1_CNEIF_MASK    0x00010000u
#define _IFS1_CNFIF_MASK    0x00020000u
#define _IFS1_CNGIF_MASK    0x00040000u

#define _IEC1_CNAIE_MASK    0x00001000u
#define _IEC1_CNBIE_MASK    0x00002000u
#define _IEC1_CNCIE_MASK    0x00004000u
#define _IEC1_CNDIE_MASK    0x00008000u
#define _IEC1_CNEIE_MASK    0x00010000u
#define _IEC1_CNFIE_MASK    0x00020000u
#define _IEC1_CNGIE_MASK    0x00040000u

/* -----------------------------------------------------------------------
 * PPS (Peripheral Pin Select) registers used by plib_gpio.c GPIO_Initialize()
 * Base addresses from DS60001519E Table 14-2 / p32mk1024mcm100.h
 * ----------------------------------------------------------------------- */
/* Input remapping */
#define IC2R        (*(volatile uint32_t *)0xBF80143Cu)
#define U1RXR       (*(volatile uint32_t *)0xBF801464u)
#define U4RXR       (*(volatile uint32_t *)0xBF80147Cu)
#define C1RXR       (*(volatile uint32_t *)0xBF8014C4u)
#define C2RXR       (*(volatile uint32_t *)0xBF8014C8u)
#define IC11R       (*(volatile uint32_t *)0xBF8014FCu)
#define IC12R       (*(volatile uint32_t *)0xBF801500u)
#define IC16R       (*(volatile uint32_t *)0xBF801510u)
#define SCK5R       (*(volatile uint32_t *)0xBF801514u)
#define SDI5R       (*(volatile uint32_t *)0xBF801518u)
#define SS5R        (*(volatile uint32_t *)0xBF80151Cu)
#define C3RXR       (*(volatile uint32_t *)0xBF80152Cu)
#define C4RXR       (*(volatile uint32_t *)0xBF801530u)
#define U2RXR       (*(volatile uint32_t *)0xBF801540u)
/* Output remapping */
#define RPA0R       (*(volatile uint32_t *)0xBF801600u)
#define RPA1R       (*(volatile uint32_t *)0xBF801604u)
#define RPA7R       (*(volatile uint32_t *)0xBF80161Cu)
#define RPA12R      (*(volatile uint32_t *)0xBF801630u)
#define RPB0R       (*(volatile uint32_t *)0xBF801640u)
#define RPC6R       (*(volatile uint32_t *)0xBF801698u)
#define RPC7R       (*(volatile uint32_t *)0xBF80169Cu)
#define RPC9R       (*(volatile uint32_t *)0xBF8016A4u)
#define RPF0R       (*(volatile uint32_t *)0xBF801740u)
#define RPF1R       (*(volatile uint32_t *)0xBF801744u)
#define RPG0R       (*(volatile uint32_t *)0xBF801780u)
#define RPG6R       (*(volatile uint32_t *)0xBF801798u)
#define RPG9R       (*(volatile uint32_t *)0xBF8017A4u)

/* -----------------------------------------------------------------------
 * ADCHS — 12-bit High-Speed SAR ADC (§22, DS60001519E)
 * Base: 0xBF887000
 * ----------------------------------------------------------------------- */

#define _ADC_BASE       0xBF887000u

/* ADCCON1 — ADC Control Register 1 (offset 0x000) */
#define ADCCON1         (*(volatile uint32_t *)(_ADC_BASE + 0x000u))
#define ADCCON1CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x004u))
#define ADCCON1SET      (*(volatile uint32_t *)(_ADC_BASE + 0x008u))
#define ADCCON1INV      (*(volatile uint32_t *)(_ADC_BASE + 0x00Cu))

typedef union {
    struct {
        uint32_t DMABL      : 3;
        uint32_t STRGLVL    : 1;
        uint32_t IRQVS      : 3;
        uint32_t            : 2;
        uint32_t FSPBCLKEN  : 1;
        uint32_t FSSCLKEN   : 1;
        uint32_t CVDEN      : 1;
        uint32_t AICPMPEN   : 1;
        uint32_t SIDL       : 1;
        uint32_t            : 1;
        uint32_t ON         : 1;
        uint32_t STRGSRC    : 5;
        uint32_t SELRES     : 2;
        uint32_t FRACT      : 1;
        uint32_t TRBSLV     : 3;
        uint32_t TRBMST     : 3;
        uint32_t TRBERR     : 1;
        uint32_t TRBEN      : 1;
    };
    uint32_t w;
} __ADCCON1_t;

#define ADCCON1bits     (*(volatile __ADCCON1_t *)(_ADC_BASE + 0x000u))

/* ADCCON2 — ADC Control Register 2 (offset 0x010) */
#define ADCCON2         (*(volatile uint32_t *)(_ADC_BASE + 0x010u))
#define ADCCON2CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x014u))
#define ADCCON2SET      (*(volatile uint32_t *)(_ADC_BASE + 0x018u))
#define ADCCON2INV      (*(volatile uint32_t *)(_ADC_BASE + 0x01Cu))

typedef union {
    struct {
        uint32_t ADCDIV     : 7;
        uint32_t            : 1;
        uint32_t ADCEIS     : 3;
        uint32_t            : 1;
        uint32_t ADCEIOVR   : 1;
        uint32_t EOSIEN     : 1;
        uint32_t REFFLTIEN  : 1;
        uint32_t BGVRIEN    : 1;
        uint32_t SAMC       : 10;
        uint32_t CVDCPL     : 3;
        uint32_t EOSRDY     : 1;
        uint32_t REFFLT     : 1;
        uint32_t BGVRRDY    : 1;
    };
    uint32_t w;
} __ADCCON2_t;

#define ADCCON2bits     (*(volatile __ADCCON2_t *)(_ADC_BASE + 0x010u))

/* ADCCON3 — ADC Control Register 3 (offset 0x020) */
#define ADCCON3         (*(volatile uint32_t *)(_ADC_BASE + 0x020u))
#define ADCCON3CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x024u))
#define ADCCON3SET      (*(volatile uint32_t *)(_ADC_BASE + 0x028u))
#define ADCCON3INV      (*(volatile uint32_t *)(_ADC_BASE + 0x02Cu))

typedef union {
    struct {
        uint32_t ADINSEL    : 6;
        uint32_t GSWTRG     : 1;
        uint32_t GLSWTRG    : 1;
        uint32_t RQCNVRT    : 1;
        uint32_t SAMP       : 1;
        uint32_t UPDRDY     : 1;
        uint32_t UPDIEN     : 1;
        uint32_t TRGSUSP    : 1;
        uint32_t VREFSEL    : 3;
        uint32_t DIGEN0     : 1;
        uint32_t DIGEN1     : 1;
        uint32_t DIGEN2     : 1;
        uint32_t DIGEN3     : 1;
        uint32_t DIGEN4     : 1;
        uint32_t DIGEN5     : 1;
        uint32_t            : 1;
        uint32_t DIGEN7     : 1;
        uint32_t CONCLKDIV  : 6;
        uint32_t ADCSEL     : 2;
    };
    uint32_t w;
} __ADCCON3_t;

#define ADCCON3bits     (*(volatile __ADCCON3_t *)(_ADC_BASE + 0x020u))

/* ADCTRGMODE — ADC Trigger Mode (offset 0x030) */
#define ADCTRGMODE      (*(volatile uint32_t *)(_ADC_BASE + 0x030u))
#define ADCTRGMODECLR   (*(volatile uint32_t *)(_ADC_BASE + 0x034u))
#define ADCTRGMODESET   (*(volatile uint32_t *)(_ADC_BASE + 0x038u))
#define ADCTRGMODEINV   (*(volatile uint32_t *)(_ADC_BASE + 0x03Cu))

/* ADCIMCON1–4 — ADC Input Mode Control (offsets 0x040, 0x050, 0x060, 0x070) */
#define ADCIMCON1       (*(volatile uint32_t *)(_ADC_BASE + 0x040u))
#define ADCIMCON1CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x044u))
#define ADCIMCON1SET    (*(volatile uint32_t *)(_ADC_BASE + 0x048u))
#define ADCIMCON1INV    (*(volatile uint32_t *)(_ADC_BASE + 0x04Cu))

#define ADCIMCON2       (*(volatile uint32_t *)(_ADC_BASE + 0x050u))
#define ADCIMCON2CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x054u))
#define ADCIMCON2SET    (*(volatile uint32_t *)(_ADC_BASE + 0x058u))
#define ADCIMCON2INV    (*(volatile uint32_t *)(_ADC_BASE + 0x05Cu))

#define ADCIMCON3       (*(volatile uint32_t *)(_ADC_BASE + 0x060u))
#define ADCIMCON3CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x064u))
#define ADCIMCON3SET    (*(volatile uint32_t *)(_ADC_BASE + 0x068u))
#define ADCIMCON3INV    (*(volatile uint32_t *)(_ADC_BASE + 0x06Cu))

#define ADCIMCON4       (*(volatile uint32_t *)(_ADC_BASE + 0x070u))
#define ADCIMCON4CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x074u))
#define ADCIMCON4SET    (*(volatile uint32_t *)(_ADC_BASE + 0x078u))
#define ADCIMCON4INV    (*(volatile uint32_t *)(_ADC_BASE + 0x07Cu))

/* ADCGIRQEN1–2 — ADC Global Interrupt Request Enable (offsets 0x080, 0x090) */
#define ADCGIRQEN1      (*(volatile uint32_t *)(_ADC_BASE + 0x080u))
#define ADCGIRQEN1CLR   (*(volatile uint32_t *)(_ADC_BASE + 0x084u))
#define ADCGIRQEN1SET   (*(volatile uint32_t *)(_ADC_BASE + 0x088u))
#define ADCGIRQEN1INV   (*(volatile uint32_t *)(_ADC_BASE + 0x08Cu))

#define ADCGIRQEN2      (*(volatile uint32_t *)(_ADC_BASE + 0x090u))
#define ADCGIRQEN2CLR   (*(volatile uint32_t *)(_ADC_BASE + 0x094u))
#define ADCGIRQEN2SET   (*(volatile uint32_t *)(_ADC_BASE + 0x098u))
#define ADCGIRQEN2INV   (*(volatile uint32_t *)(_ADC_BASE + 0x09Cu))

/* ADCCSS1–2 — ADC Common Scan Select (offsets 0x0A0, 0x0B0) */
#define ADCCSS1         (*(volatile uint32_t *)(_ADC_BASE + 0x0A0u))
#define ADCCSS1CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x0A4u))
#define ADCCSS1SET      (*(volatile uint32_t *)(_ADC_BASE + 0x0A8u))
#define ADCCSS1INV      (*(volatile uint32_t *)(_ADC_BASE + 0x0ACu))

#define ADCCSS2         (*(volatile uint32_t *)(_ADC_BASE + 0x0B0u))
#define ADCCSS2CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x0B4u))
#define ADCCSS2SET      (*(volatile uint32_t *)(_ADC_BASE + 0x0B8u))
#define ADCCSS2INV      (*(volatile uint32_t *)(_ADC_BASE + 0x0BCu))

/* ADCDSTAT1–2 — ADC Data Ready Status (offsets 0x0C0, 0x0D0) */
#define ADCDSTAT1       (*(volatile uint32_t *)(_ADC_BASE + 0x0C0u))
#define ADCDSTAT1CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x0C4u))
#define ADCDSTAT1SET    (*(volatile uint32_t *)(_ADC_BASE + 0x0C8u))
#define ADCDSTAT1INV    (*(volatile uint32_t *)(_ADC_BASE + 0x0CCu))

#define ADCDSTAT2       (*(volatile uint32_t *)(_ADC_BASE + 0x0D0u))
#define ADCDSTAT2CLR    (*(volatile uint32_t *)(_ADC_BASE + 0x0D4u))
#define ADCDSTAT2SET    (*(volatile uint32_t *)(_ADC_BASE + 0x0D8u))
#define ADCDSTAT2INV    (*(volatile uint32_t *)(_ADC_BASE + 0x0DCu))

/* ADCTRG1–7 — ADC Trigger Source (offsets 0x200–0x260) */
#define ADCTRG1         (*(volatile uint32_t *)(_ADC_BASE + 0x200u))
#define ADCTRG1CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x204u))
#define ADCTRG1SET      (*(volatile uint32_t *)(_ADC_BASE + 0x208u))
#define ADCTRG1INV      (*(volatile uint32_t *)(_ADC_BASE + 0x20Cu))

#define ADCTRG2         (*(volatile uint32_t *)(_ADC_BASE + 0x210u))
#define ADCTRG2CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x214u))
#define ADCTRG2SET      (*(volatile uint32_t *)(_ADC_BASE + 0x218u))
#define ADCTRG2INV      (*(volatile uint32_t *)(_ADC_BASE + 0x21Cu))

#define ADCTRG3         (*(volatile uint32_t *)(_ADC_BASE + 0x220u))
#define ADCTRG3CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x224u))
#define ADCTRG3SET      (*(volatile uint32_t *)(_ADC_BASE + 0x228u))
#define ADCTRG3INV      (*(volatile uint32_t *)(_ADC_BASE + 0x22Cu))

#define ADCTRG4         (*(volatile uint32_t *)(_ADC_BASE + 0x230u))
#define ADCTRG4CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x234u))
#define ADCTRG4SET      (*(volatile uint32_t *)(_ADC_BASE + 0x238u))
#define ADCTRG4INV      (*(volatile uint32_t *)(_ADC_BASE + 0x23Cu))

#define ADCTRG5         (*(volatile uint32_t *)(_ADC_BASE + 0x240u))
#define ADCTRG5CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x244u))
#define ADCTRG5SET      (*(volatile uint32_t *)(_ADC_BASE + 0x248u))
#define ADCTRG5INV      (*(volatile uint32_t *)(_ADC_BASE + 0x24Cu))

#define ADCTRG6         (*(volatile uint32_t *)(_ADC_BASE + 0x250u))
#define ADCTRG6CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x254u))
#define ADCTRG6SET      (*(volatile uint32_t *)(_ADC_BASE + 0x258u))
#define ADCTRG6INV      (*(volatile uint32_t *)(_ADC_BASE + 0x25Cu))

#define ADCTRG7         (*(volatile uint32_t *)(_ADC_BASE + 0x260u))
#define ADCTRG7CLR      (*(volatile uint32_t *)(_ADC_BASE + 0x264u))
#define ADCTRG7SET      (*(volatile uint32_t *)(_ADC_BASE + 0x268u))
#define ADCTRG7INV      (*(volatile uint32_t *)(_ADC_BASE + 0x26Cu))

/* ADCTRGSNS — ADC Trigger Sense (offset 0x340) */
#define ADCTRGSNS       (*(volatile uint32_t *)(_ADC_BASE + 0x340u))
#define ADCTRGSNSCLR    (*(volatile uint32_t *)(_ADC_BASE + 0x344u))
#define ADCTRGSNSSET    (*(volatile uint32_t *)(_ADC_BASE + 0x348u))
#define ADCTRGSNSINV    (*(volatile uint32_t *)(_ADC_BASE + 0x34Cu))

/* ADCEIEN1–2 — ADC Early Interrupt Enable (offsets 0x3C0, 0x3D0) */
#define ADCEIEN1        (*(volatile uint32_t *)(_ADC_BASE + 0x3C0u))
#define ADCEIEN1CLR     (*(volatile uint32_t *)(_ADC_BASE + 0x3C4u))
#define ADCEIEN1SET     (*(volatile uint32_t *)(_ADC_BASE + 0x3C8u))
#define ADCEIEN1INV     (*(volatile uint32_t *)(_ADC_BASE + 0x3CCu))

#define ADCEIEN2        (*(volatile uint32_t *)(_ADC_BASE + 0x3D0u))
#define ADCEIEN2CLR     (*(volatile uint32_t *)(_ADC_BASE + 0x3D4u))
#define ADCEIEN2SET     (*(volatile uint32_t *)(_ADC_BASE + 0x3D8u))
#define ADCEIEN2INV     (*(volatile uint32_t *)(_ADC_BASE + 0x3DCu))

/* ADCANCON — ADC Analog and Bias Control (offset 0x400) */
#define ADCANCON        (*(volatile uint32_t *)(_ADC_BASE + 0x400u))
#define ADCANCONCLR     (*(volatile uint32_t *)(_ADC_BASE + 0x404u))
#define ADCANCONSET     (*(volatile uint32_t *)(_ADC_BASE + 0x408u))
#define ADCANCONINV     (*(volatile uint32_t *)(_ADC_BASE + 0x40Cu))

typedef union {
    struct {
        uint32_t ANEN0      : 1;
        uint32_t ANEN1      : 1;
        uint32_t ANEN2      : 1;
        uint32_t ANEN3      : 1;
        uint32_t ANEN4      : 1;
        uint32_t ANEN5      : 1;
        uint32_t            : 1;
        uint32_t ANEN7      : 1;
        uint32_t WKRDY0     : 1;
        uint32_t WKRDY1     : 1;
        uint32_t WKRDY2     : 1;
        uint32_t WKRDY3     : 1;
        uint32_t WKRDY4     : 1;
        uint32_t WKRDY5     : 1;
        uint32_t            : 1;
        uint32_t WKRDY7     : 1;
        uint32_t WKIEN0     : 1;
        uint32_t WKIEN1     : 1;
        uint32_t WKIEN2     : 1;
        uint32_t WKIEN3     : 1;
        uint32_t WKIEN4     : 1;
        uint32_t WKIEN5     : 1;
        uint32_t            : 1;
        uint32_t WKIEN7     : 1;
        uint32_t WKUPCLKCNT : 4;
        uint32_t            : 4;
    };
    uint32_t w;
} __ADCANCON_t;

#define ADCANCONbits    (*(volatile __ADCANCON_t *)(_ADC_BASE + 0x400u))

/* ADCDATA0 — ADC Data Register base (offset 0x600, stride 0x10) */
#define ADCDATA0        (*(volatile uint32_t *)(_ADC_BASE + 0x600u))

/* ADC0CFG–ADC7CFG — ADC Module Configuration (offsets 0xD00–0xD70) */
#define ADC0CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD00u))
#define ADC1CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD10u))
#define ADC2CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD20u))
#define ADC3CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD30u))
#define ADC4CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD40u))
#define ADC5CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD50u))
#define ADC6CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD60u))
#define ADC7CFG         (*(volatile uint32_t *)(_ADC_BASE + 0xD70u))

/* DEVADC0–DEVADC7 — Device ADC Calibration in Boot Flash (0xBFC45000) */
#define DEVADC0         (*(volatile uint32_t *)0xBFC45000u)
#define DEVADC1         (*(volatile uint32_t *)0xBFC45004u)
#define DEVADC2         (*(volatile uint32_t *)0xBFC45008u)
#define DEVADC3         (*(volatile uint32_t *)0xBFC4500Cu)
#define DEVADC4         (*(volatile uint32_t *)0xBFC45010u)
#define DEVADC5         (*(volatile uint32_t *)0xBFC45014u)
#define DEVADC6         (*(volatile uint32_t *)0xBFC45018u)
#define DEVADC7         (*(volatile uint32_t *)0xBFC4501Cu)

/* ADC End-of-Scan interrupt masks in IEC3/IFS3 */
#define _IFS3_AD1EOSIF_MASK     0x00000020u
#define _IEC3_AD1EOSIE_MASK     0x00000020u

/* -----------------------------------------------------------------------
 * CFGCON2 — Configuration Control Register 2 (offset 0x110 from _CFG_BASE)
 * Needed by plib_eeprom.c for EEWS (EEPROM Wait States) field.
 * ----------------------------------------------------------------------- */
#define CFGCON2         (*(volatile uint32_t *)(_CFG_BASE + 0x110u))
#define CFGCON2CLR      (*(volatile uint32_t *)(_CFG_BASE + 0x114u))
#define CFGCON2SET      (*(volatile uint32_t *)(_CFG_BASE + 0x118u))
#define CFGCON2INV      (*(volatile uint32_t *)(_CFG_BASE + 0x11Cu))

typedef union {
    struct {
        uint32_t            : 8;   /* bits[7:0] */
        uint32_t EEWS       : 4;   /* bits[11:8]: EEPROM Wait States */
        uint32_t            : 20;  /* bits[31:12] */
    };
    uint32_t w;
} __CFGCON2_t;

#define CFGCON2bits     (*(volatile __CFGCON2_t *)(_CFG_BASE + 0x110u))

/* -----------------------------------------------------------------------
 * Data EEPROM registers  (base 0xBF829000)
 * ----------------------------------------------------------------------- */

#define _DATAEE_BASE    0xBF829000u

#define EECON           (*(volatile uint32_t *)(_DATAEE_BASE + 0x00u))
#define EECONCLR        (*(volatile uint32_t *)(_DATAEE_BASE + 0x04u))
#define EECONSET        (*(volatile uint32_t *)(_DATAEE_BASE + 0x08u))
#define EECONINV        (*(volatile uint32_t *)(_DATAEE_BASE + 0x0Cu))

typedef union {
    struct {
        uint32_t CMD    : 3;   /* bits[2:0]: Command */
        uint32_t ILW    : 1;   /* bit 3: Internal Long Write */
        uint32_t ERR    : 2;   /* bits[5:4]: Error status */
        uint32_t WREN   : 1;   /* bit 6: Write Enable */
        uint32_t RW     : 1;   /* bit 7: Read/Write start */
        uint32_t        : 4;   /* bits[11:8] */
        uint32_t ABORT  : 1;   /* bit 12: Abort operation */
        uint32_t SIDL   : 1;   /* bit 13: Stop in idle */
        uint32_t RDY    : 1;   /* bit 14: Module ready */
        uint32_t ON     : 1;   /* bit 15: Module enable */
        uint32_t        : 16;  /* bits[31:16] */
    };
    uint32_t w;
} __EECON_t;

#define EECONbits       (*(volatile __EECON_t *)(_DATAEE_BASE + 0x00u))

/* EECON bit masks */
#define _EECON_CMD_MASK         0x00000007u
#define _EECON_ERR_MASK         0x00000030u
#define _EECON_WREN_MASK        0x00000040u
#define _EECON_RW_MASK          0x00000080u
#define _EECON_ON_MASK          0x00008000u
#define _EECON_RDY_MASK         0x00004000u

#define EEKEY           (*(volatile uint32_t *)(_DATAEE_BASE + 0x10u))

#define EEADDR          (*(volatile uint32_t *)(_DATAEE_BASE + 0x20u))
#define EEADDRCLR       (*(volatile uint32_t *)(_DATAEE_BASE + 0x24u))
#define EEADDRSET       (*(volatile uint32_t *)(_DATAEE_BASE + 0x28u))

#define EEDATA          (*(volatile uint32_t *)(_DATAEE_BASE + 0x30u))
#define EEDATACLR       (*(volatile uint32_t *)(_DATAEE_BASE + 0x34u))
#define EEDATASET       (*(volatile uint32_t *)(_DATAEE_BASE + 0x38u))

/* DEVEE0–DEVEE3 — Device EEPROM Configuration Words in Boot Flash */
#define DEVEE0          (*(volatile uint32_t *)0xBFC45030u)
#define DEVEE1          (*(volatile uint32_t *)0xBFC45034u)
#define DEVEE2          (*(volatile uint32_t *)0xBFC45038u)
#define DEVEE3          (*(volatile uint32_t *)0xBFC4503Cu)

/* -----------------------------------------------------------------------
 * NVM / Flash Controller — 0xBF800A00
 * ----------------------------------------------------------------------- */
#define _NVM_BASE           0xBF800A00u

#define NVMCON              (*(volatile uint32_t *)(_NVM_BASE + 0x00u))
#define NVMCONCLR           (*(volatile uint32_t *)(_NVM_BASE + 0x04u))
#define NVMCONSET           (*(volatile uint32_t *)(_NVM_BASE + 0x08u))
#define NVMCONINV           (*(volatile uint32_t *)(_NVM_BASE + 0x0Cu))

typedef struct {
    uint32_t NVMOP   :4;
    uint32_t         :2;
    uint32_t BFSWAP  :1;
    uint32_t PFSWAP  :1;
    uint32_t         :4;
    uint32_t LVDERR  :1;
    uint32_t WRERR   :1;
    uint32_t WREN    :1;
    uint32_t WR      :1;
    uint32_t         :16;
} __NVMCON_t;

#define NVMCONbits          (*(volatile __NVMCON_t *)(_NVM_BASE + 0x00u))

/* NVMCON bit masks and positions */
#define _NVMCON_NVMOP_MASK      0x000Fu
#define _NVMCON_NVMOP_POSITION  0
#define _NVMCON_BFSWAP_MASK     (1u << 6)
#define _NVMCON_PFSWAP_MASK     (1u << 7)
#define _NVMCON_LVDERR_MASK     (1u << 12)
#define _NVMCON_WRERR_MASK      (1u << 13)
#define _NVMCON_WREN_MASK       (1u << 14)
#define _NVMCON_WR_MASK         (1u << 15)

#define NVMKEY              (*(volatile uint32_t *)(_NVM_BASE + 0x10u))

#define NVMADDR             (*(volatile uint32_t *)(_NVM_BASE + 0x20u))
#define NVMADDRCLR          (*(volatile uint32_t *)(_NVM_BASE + 0x24u))
#define NVMADDRSET          (*(volatile uint32_t *)(_NVM_BASE + 0x28u))
#define NVMADDRINV          (*(volatile uint32_t *)(_NVM_BASE + 0x2Cu))

#define NVMDATA0            (*(volatile uint32_t *)(_NVM_BASE + 0x30u))
#define NVMDATA0CLR         (*(volatile uint32_t *)(_NVM_BASE + 0x34u))
#define NVMDATA0SET         (*(volatile uint32_t *)(_NVM_BASE + 0x38u))
#define NVMDATA0INV         (*(volatile uint32_t *)(_NVM_BASE + 0x3Cu))

#define NVMDATA1            (*(volatile uint32_t *)(_NVM_BASE + 0x40u))
#define NVMDATA1CLR         (*(volatile uint32_t *)(_NVM_BASE + 0x44u))
#define NVMDATA1SET         (*(volatile uint32_t *)(_NVM_BASE + 0x48u))
#define NVMDATA1INV         (*(volatile uint32_t *)(_NVM_BASE + 0x4Cu))

#define NVMDATA2            (*(volatile uint32_t *)(_NVM_BASE + 0x50u))
#define NVMDATA2CLR         (*(volatile uint32_t *)(_NVM_BASE + 0x54u))
#define NVMDATA2SET         (*(volatile uint32_t *)(_NVM_BASE + 0x58u))
#define NVMDATA2INV         (*(volatile uint32_t *)(_NVM_BASE + 0x5Cu))

#define NVMDATA3            (*(volatile uint32_t *)(_NVM_BASE + 0x60u))
#define NVMDATA3CLR         (*(volatile uint32_t *)(_NVM_BASE + 0x64u))
#define NVMDATA3SET         (*(volatile uint32_t *)(_NVM_BASE + 0x68u))
#define NVMDATA3INV         (*(volatile uint32_t *)(_NVM_BASE + 0x6Cu))

#define NVMSRCADDR          (*(volatile uint32_t *)(_NVM_BASE + 0x70u))
#define NVMSRCADDRCLR       (*(volatile uint32_t *)(_NVM_BASE + 0x74u))
#define NVMSRCADDRSET       (*(volatile uint32_t *)(_NVM_BASE + 0x78u))
#define NVMSRCADDRINV       (*(volatile uint32_t *)(_NVM_BASE + 0x7Cu))

#define NVMPWP              (*(volatile uint32_t *)(_NVM_BASE + 0x80u))
#define NVMPWPCLR           (*(volatile uint32_t *)(_NVM_BASE + 0x84u))
#define NVMPWPSET           (*(volatile uint32_t *)(_NVM_BASE + 0x88u))
#define NVMPWPINV           (*(volatile uint32_t *)(_NVM_BASE + 0x8Cu))

/* NVMPWP bit masks */
#define _NVMPWP_PWP_MASK        0x00FFFFFFu
#define _NVMPWP_PWPULOCK_MASK   (1u << 31)

#define NVMBWP              (*(volatile uint32_t *)(_NVM_BASE + 0x90u))
#define NVMBWPCLR           (*(volatile uint32_t *)(_NVM_BASE + 0x94u))
#define NVMBWPSET           (*(volatile uint32_t *)(_NVM_BASE + 0x98u))
#define NVMBWPINV           (*(volatile uint32_t *)(_NVM_BASE + 0x9Cu))

#define NVMCON2             (*(volatile uint32_t *)(_NVM_BASE + 0xA0u))
#define NVMCON2CLR          (*(volatile uint32_t *)(_NVM_BASE + 0xA4u))
#define NVMCON2SET          (*(volatile uint32_t *)(_NVM_BASE + 0xA8u))
#define NVMCON2INV          (*(volatile uint32_t *)(_NVM_BASE + 0xACu))

/* -----------------------------------------------------------------------
 * __builtin_mfc0 — Read a CP0 register (XC32 built-in, not in GCC).
 * Under emulation the Config register value is don't-care (it is only
 * read to or-in 0x3 then written back via __builtin_mtc0 which is a
 * no-op). Return 0 so the or-expression is harmless.
 * ----------------------------------------------------------------------- */
static inline uint32_t __builtin_mfc0(unsigned reg, unsigned sel)
{
    (void)reg; (void)sel;
    return 0u;
}

/* -----------------------------------------------------------------------
 * CHECON — Prefetch Cache Controller (PIC32MK base 0xBF800800)
 * Only PFMWS[2:0] and PREFEN[1:0] are used by initialization.c.
 * ----------------------------------------------------------------------- */
#define _PREFEN_BASE    0xBF800800u

typedef union {
    struct {
        uint32_t PFMWS   : 3;   /* bits [2:0]  Flash Wait States       */
        uint32_t         : 1;
        uint32_t PREFEN  : 2;   /* bits [5:4]  Prefetch Enable          */
        uint32_t         : 26;
    };
    uint32_t w;
} __CHECONbits_t;

#define CHECON      (*(volatile uint32_t       *)(_PREFEN_BASE))
#define CHECONbits  (*(volatile __CHECONbits_t *)(_PREFEN_BASE))
#define CHECONCLR   (*(volatile uint32_t       *)(_PREFEN_BASE + 0x4u))
#define CHECONSET   (*(volatile uint32_t       *)(_PREFEN_BASE + 0x8u))
#define CHECONINV   (*(volatile uint32_t       *)(_PREFEN_BASE + 0xCu))

#endif /* __ASSEMBLER__ */
#endif /* XC_H */
