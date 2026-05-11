/*
 * stdio.h — Minimal stdio stub for bare-metal build
 *
 * Provides declarations for functions implemented in libc_stubs.c.
 * Output is routed to UART1 TX by polling U1TXREG directly.
 */

#ifndef STDIO_H_STUB
#define STDIO_H_STUB

#ifndef __ASSEMBLER__

#include <stdarg.h>
#include <stddef.h>

/* Minimal FILE type for bare-metal — no real buffering exists */
typedef struct { int _dummy; } FILE;

/* Bare-metal stdin/stdout: point to a single dummy stream object.
 * setbuf() is a no-op; we have no buffering to configure. */
extern FILE _stdin_stream;
extern FILE _stdout_stream;
#define stdin   (&_stdin_stream)
#define stdout  (&_stdout_stream)

static inline void setbuf(FILE *stream, char *buf) { (void)stream; (void)buf; }

int printf(const char *format, ...);
int puts(const char *s);
int putchar(int c);

/* snprintf, vsnprintf, sprintf and vsprintf are already in libc_stubs.c */
int sprintf(char *dst, const char *format, ...);
int vsprintf(char *dst, const char *format, va_list args);
int snprintf(char *dst, size_t size, const char *format, ...);
int vsnprintf(char *dst, size_t size, const char *format, va_list args);

#endif /* __ASSEMBLER__ */

#endif /* STDIO_H_STUB */
