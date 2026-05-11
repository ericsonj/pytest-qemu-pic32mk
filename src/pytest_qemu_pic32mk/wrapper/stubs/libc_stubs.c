/*
 * libc_stubs.c — Minimal libc functions for bare-metal FreeRTOS build
 *
 * FreeRTOS sources (tasks.c, queue.c, etc.) call standard libc functions
 * like memset() and memcpy(). Since we build with -nostdlib -ffreestanding,
 * we must provide them ourselves.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* Backing storage for the bare-metal stdin/stdout stream handles.
 * setbuf() is a no-op in our stdio.h stub, so no buffering is ever set up. */
FILE _stdin_stream  = { 0 };
FILE _stdout_stream = { 0 };

/* FreeRTOS_tasks.c uses memset to zero TCB and stack memory */
void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

/* queue.c uses memcpy to copy item data into/out of queue storage */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

/* overlapping-safe copy: forward if dst < src, backward otherwise */
void *memmove (void *dest, const void *src, size_t len)
{
  char *d = dest;
  const char *s = src;
  if (d < s)
    while (len--)
      *d++ = *s++;
  else
    {
      const char *lasts = s + (len-1);
      char *lastd = d + (len-1);
      while (len--)
        *lastd-- = *lasts--;
    }
  return dest;
}


size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
    {
        p++;
    }

    return (size_t)(p - s);
}

static size_t snprintf_putc(char *dst, size_t size, size_t pos, char c)
{
    if (size > 0U && pos + 1U < size)
    {
        dst[pos] = c;
    }

    return pos + 1U;
}

static size_t snprintf_puts(char *dst, size_t size, size_t pos,
                            const char *s, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        pos = snprintf_putc(dst, size, pos, s[i]);
    }

    return pos;
}

static size_t snprintf_put_unsigned(char *dst, size_t size, size_t pos,
                                    unsigned long value, unsigned int base,
                                    bool uppercase, unsigned int width,
                                    char pad)
{
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t len = 0;

    do
    {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value != 0UL);

    while (len < width)
    {
        buf[len++] = pad;
    }

    while (len > 0U)
    {
        pos = snprintf_putc(dst, size, pos, buf[--len]);
    }

    return pos;
}

static size_t snprintf_put_signed(char *dst, size_t size, size_t pos,
                                  long value, unsigned int width, char pad)
{
    unsigned long magnitude;

    if (value < 0)
    {
        magnitude = (unsigned long)(-(value + 1L)) + 1UL;
        if (pad == '0' && width > 0U)
        {
            pos = snprintf_putc(dst, size, pos, '-');
            width--;
            return snprintf_put_unsigned(dst, size, pos, magnitude, 10U,
                                         false, width, pad);
        }

        magnitude = (unsigned long)(-(value + 1L)) + 1UL;
        if (width > 1U)
        {
            width--;
        }
        pos = snprintf_put_unsigned(dst, size, pos, magnitude, 10U,
                                    false, width, pad);
        return pos;
    }

    return snprintf_put_unsigned(dst, size, pos, (unsigned long)value, 10U,
                                 false, width, pad);
}

int vsnprintf(char *dst, size_t size, const char *format, va_list args)
{
    size_t pos = 0;

    while (*format != '\0')
    {
        if (*format != '%')
        {
            pos = snprintf_putc(dst, size, pos, *format++);
            continue;
        }

        format++;

        if (*format == '\0')
        {
            break;
        }

        {
            char pad = ' ';
            unsigned int width = 0;
            bool long_modifier = false;

            if (*format == '0')
            {
                pad = '0';
                format++;
            }

            while (*format >= '0' && *format <= '9')
            {
                width = (width * 10U) + (unsigned int)(*format - '0');
                format++;
            }

            while (*format == 'l')
            {
                long_modifier = true;
                format++;
            }

            switch (*format)
            {
            case '%':
                pos = snprintf_putc(dst, size, pos, '%');
                break;
            case 'c':
                pos = snprintf_putc(dst, size, pos, (char)va_arg(args, int));
                break;
            case 's':
            {
                const char *s = va_arg(args, const char *);

                if (s == NULL)
                {
                    s = "(null)";
                }

                pos = snprintf_puts(dst, size, pos, s, strlen(s));
                break;
            }
            case 'd':
            case 'i':
                if (long_modifier)
                {
                    pos = snprintf_put_signed(dst, size, pos,
                                              va_arg(args, long), width, pad);
                }
                else
                {
                    pos = snprintf_put_signed(dst, size, pos,
                                              (long)va_arg(args, int), width, pad);
                }
                break;
            case 'u':
                if (long_modifier)
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                va_arg(args, unsigned long),
                                                10U, false, width, pad);
                }
                else
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                (unsigned long)va_arg(args, unsigned int),
                                                10U, false, width, pad);
                }
                break;
            case 'x':
            case 'X':
                if (long_modifier)
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                va_arg(args, unsigned long),
                                                16U, *format == 'X', width, pad);
                }
                else
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                (unsigned long)va_arg(args, unsigned int),
                                                16U, *format == 'X', width, pad);
                }
                break;
            default:
                pos = snprintf_putc(dst, size, pos, '%');
                pos = snprintf_putc(dst, size, pos, *format);
                break;
            }
        }

        format++;
    }

    if (size > 0U)
    {
        dst[(pos < size) ? pos : (size - 1U)] = '\0';
    }

    return (int)pos;
}

int vsprintf(char *dst, const char *format, va_list args)
{
    /* Unbounded — use a large internal limit */
    return vsnprintf(dst, (size_t)0x7fffffff, format, args);
}

int sprintf(char *dst, const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = vsprintf(dst, format, args);
    va_end(args);

    return ret;
}

int snprintf(char *dst, size_t size, const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = vsnprintf(dst, size, format, args);
    va_end(args);

    return ret;
}

int __vsnprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                    const char *format, va_list args)
{
    (void)flag;
    (void)dstlen;

    return vsnprintf(dst, maxlen, format, args);
}

int __snprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                   const char *format, ...)
{
    int ret;
    va_list args;

    (void)flag;
    (void)dstlen;

    va_start(args, format);
    ret = vsnprintf(dst, maxlen, format, args);
    va_end(args);

    return ret;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcat(char *dst, const char *src)
{
    char *p = dst;
    while (*p) p++;
    while ((*p++ = *src++));
    return dst;
}

char *strtok(char *str, const char *delim)
{
    static char *saved;
    const char *d;

    if (str != NULL)
        saved = str;
    if (saved == NULL || *saved == '\0')
        return NULL;

    /* skip leading delimiters */
    while (*saved) {
        for (d = delim; *d; d++) {
            if (*saved == *d) goto skip;
        }
        break;
skip:
        saved++;
    }

    if (*saved == '\0')
        return NULL;

    char *token = saved;

    while (*saved) {
        for (d = delim; *d; d++) {
            if (*saved == *d) {
                *saved++ = '\0';
                return token;
            }
        }
        saved++;
    }

    return token;
}

/* _FORTIFY_SOURCE checked variants — delegate to plain implementations */
void *__memcpy_chk(void *dst, const void *src, size_t n, size_t dstlen)
{
    (void)dstlen;
    return memcpy(dst, src, n);
}

void *__memset_chk(void *dst, int c, size_t n, size_t dstlen)
{
    (void)dstlen;
    return memset(dst, c, n);
}

char *__strcat_chk(char *dst, const char *src, size_t dstlen)
{
    (void)dstlen;
    return strcat(dst, src);
}

int abs(int x)
{
    return (x < 0) ? -x : x;
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    while (*str == ' ' || (*str >= '\t' && *str <= '\r'))
    {
        str++;
    }

    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

/* -----------------------------------------------------------------------
 * putchar / puts / printf — formatted output
 *
 * Output is routed through __printf_putchar(), weak by default so firmware
 * can override it for any target (UART, USB CDC, etc.):
 *
 *   void __printf_putchar(char c) { while (U1STA & _U1STA_UTXBF_MASK); U1TXREG = c; }
 *
 * Default implementation uses MIPS UHI semihosting (UHI_write = 5).
 * QEMU UHI ABI (mips-semi.c):
 *   $25 (t9) = operation (5 = UHI_write)
 *   $4  (a0) = fd        (1 = stdout)
 *   $5  (a1) = buffer pointer
 *   $6  (a2) = byte count
 * QEMU intercepts SDBBP 1 before the guest executes it and writes
 * to host stdout.  Requires -semihosting in QEMU args.
 * Without it, SDBBP raises an Illegal Instruction fault.
 * ----------------------------------------------------------------------- */
__attribute__((weak)) void __printf_putchar(char c)
{
    register unsigned int t9 __asm__("$25") = 5; /* UHI_write */
    register unsigned int a0 __asm__("$4")  = 1; /* fd = stdout */
    register char        *a1 __asm__("$5")  = &c;
    register unsigned int a2 __asm__("$6")  = 1; /* count */
    __asm__ __volatile__("sdbbp 1" : "+r"(t9) : "r"(a0), "r"(a1), "r"(a2) : "memory");
}

int putchar(int c)
{
    __printf_putchar((char)c);
    return c;
}

int puts(const char *s)
{
    while (*s)
        __printf_putchar(*s++);
    __printf_putchar('\n');
    return 0;
}

int printf(const char *format, ...)
{
    char buf[256];
    va_list args;
    int len;
    int i;

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    for (i = 0; i < len && i < (int)sizeof(buf); i++)
        __printf_putchar(buf[i]);

    return len;
}

/* -----------------------------------------------------------------------
 * Heap allocator — general-purpose for bare-metal emulator projects.
 *
 * Uses a singly-linked block list with first-fit search + forward
 * coalescing. Each block carries a small header; free blocks are merged
 * with their immediate successor on every free() call.
 *
 * Heap storage: a static array in BSS so the linker catches overflow at
 * build time (unlike a _end-based runtime heap that silently corrupts the
 * stack). Override WRAPPER_HEAP_SIZE at compile time per project:
 *
 *   CFLAGS += -DWRAPPER_HEAP_SIZE=(16*1024)
 *
 * Supports: malloc / free / calloc / realloc.
 * Thread safety: none. Add a critical-section lock around heap_lock() /
 * heap_unlock() if your RTOS is present.
 * ----------------------------------------------------------------------- */

#ifndef WRAPPER_HEAP_SIZE
#  define WRAPPER_HEAP_SIZE  (4u * 1024u)   /* 4 KB default */
#endif

/* Minimum payload after a split (prevents tiny unusable fragments). */
#define HEAP_MIN_SPLIT  8u

/* Round up to 8-byte alignment. */
#define HEAP_ALIGN(n)   (((n) + 7u) & ~7u)

/* Block header — sits immediately before each payload.
 * 'size' is the total block size including the header itself.
 * 'next' walks ALL blocks in address order (both free and allocated). */
typedef struct heap_block {
    size_t             size;   /* sizeof(header) + payload */
    unsigned int       free;   /* 1 = available, 0 = in use */
    struct heap_block *next;   /* next block or NULL */
} heap_block_t;

#define HEAP_HDR  sizeof(heap_block_t)

static char         _heap_pool[WRAPPER_HEAP_SIZE] __attribute__((aligned(8)));
static heap_block_t *_heap_root;

static void heap_init(void)
{
    _heap_root        = (heap_block_t *)_heap_pool;
    _heap_root->size  = sizeof(_heap_pool);
    _heap_root->free  = 1;
    _heap_root->next  = (void *)0;
}

/* Merge each free block with its free successor (one forward pass). */
static void heap_coalesce(void)
{
    heap_block_t *b = _heap_root;
    while (b && b->next) {
        if (b->free && b->next->free) {
            b->size += b->next->size;
            b->next  = b->next->next;
        } else {
            b = b->next;
        }
    }
}

void *malloc(size_t size)
{
    if (size == 0u)
        return (void *)0;

    if (!_heap_root)
        heap_init();

    size = HEAP_ALIGN(size);

    heap_block_t *b = _heap_root;
    while (b) {
        if (b->free && b->size >= HEAP_HDR + size) {
            /* Split the block if the leftover is big enough to reuse. */
            if (b->size >= HEAP_HDR + size + HEAP_HDR + HEAP_MIN_SPLIT) {
                heap_block_t *tail = (heap_block_t *)((char *)b + HEAP_HDR + size);
                tail->size = b->size - HEAP_HDR - size;
                tail->free = 1;
                tail->next = b->next;
                b->size    = HEAP_HDR + size;
                b->next    = tail;
            }
            b->free = 0;
            return (void *)(b + 1);
        }
        b = b->next;
    }

    return (void *)0;   /* out of heap */
}

void free(void *ptr)
{
    if (!ptr)
        return;

    heap_block_t *b = (heap_block_t *)ptr - 1;
    b->free = 1;
    heap_coalesce();
}

void *calloc(size_t nmemb, size_t size)
{
    /* Overflow-safe multiply */
    if (nmemb && size > (size_t)-1 / nmemb)
        return (void *)0;

    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr)
        return malloc(size);

    if (size == 0u) {
        free(ptr);
        return (void *)0;
    }

    heap_block_t *b = (heap_block_t *)ptr - 1;
    size_t payload  = b->size - HEAP_HDR;

    /* Already fits — return as-is (no shrink-split to keep it simple). */
    if (HEAP_ALIGN(size) <= payload)
        return ptr;

    void *newp = malloc(size);
    if (!newp)
        return (void *)0;

    memcpy(newp, ptr, payload);
    free(ptr);
    return newp;
}
