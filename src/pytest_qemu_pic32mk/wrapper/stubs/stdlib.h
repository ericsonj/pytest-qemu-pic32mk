/*
 * stdlib.h — Minimal stdlib stub for bare-metal build
 *
 * Provides declarations for functions implemented in libc_stubs.c.
 */

#ifndef STDLIB_H_STUB
#define STDLIB_H_STUB

#ifndef __ASSEMBLER__

#include <stddef.h>

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

int abs(int x);
int atoi(const char *str);
void *malloc(size_t size);
void free(void *ptr);
void *memmove(void *dst, const void *src, size_t n);

#endif /* __ASSEMBLER__ */

#endif /* STDLIB_H_STUB */
