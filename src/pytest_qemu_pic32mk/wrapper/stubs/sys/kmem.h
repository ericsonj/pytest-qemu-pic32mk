/*
 * sys/kmem.h — MIPS KSEG address translation macros.
 *
 * Adapted from /opt/microchip/xc32/v4.60/pic32mx/include/pic32m-libs/sys/kmem.h
 * All macros are pure C preprocessor / standard C typedefs; no XC32-specific
 * extensions are used, so this header compiles unchanged under mipsel-linux-gnu-gcc.
 */

#ifndef __KMEM_H__
#define __KMEM_H__

#ifdef __ASSEMBLER__

/* Translate KSEG0/KSEG1 kernel virtual address to physical address and back */
#define KVA_TO_PA(v)        ((v) & 0x1fffffff)
#define PA_TO_KVA0(pa)      ((pa) | 0x80000000)
#define PA_TO_KVA1(pa)      ((pa) | 0xa0000000)

/* Translate between KSEG0 and KSEG1 */
#define KVA0_TO_KVA1(v)     ((v) | 0x20000000)
#define KVA1_TO_KVA0(v)     ((v) & ~0x20000000)

#else /* C context */

typedef unsigned long _paddr_t; /* physical address */
typedef unsigned long _vaddr_t; /* virtual address  */

/* Translate KSEG0/KSEG1 kernel virtual address to physical address and back */
#define KVA_TO_PA(v)        ((_paddr_t)(v) & 0x1fffffff)
#define PA_TO_KVA0(pa)      ((void *)((pa) | 0x80000000))
#define PA_TO_KVA1(pa)      ((void *)((pa) | 0xa0000000))

/* Translate between KSEG0 and KSEG1 virtual addresses */
#define KVA0_TO_KVA1(v)     ((void *)((unsigned)(v) |  0x20000000))
#define KVA1_TO_KVA0(v)     ((void *)((unsigned)(v) & ~0x20000000))

/* Test for KSEG membership */
#define IS_KVA(v)           ((int)(v) < 0)
#define IS_KVA0(v)          (((unsigned)(v) >> 29) == 0x4)
#define IS_KVA1(v)          (((unsigned)(v) >> 29) == 0x5)
#define IS_KVA01(v)         (((unsigned)(v) >> 30) == 0x2)

#endif /* __ASSEMBLER__ */

#endif /* __KMEM_H__ */
