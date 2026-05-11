/* gnu/stubs-o32_soft.h — stub for MIPS o32 soft-float ABI.
 *
 * The mipsel-linux-gnu sysroot ships only the hard-float variant.
 * Since this firmware is built with -nostdlib -ffreestanding, no actual
 * glibc symbols are used; the file just needs to exist so that
 * <gnu/stubs.h> (included transitively via <features.h>) can be
 * preprocessed without error.
 *
 * Content mirrors stubs-o32_hard.h: the same libc stubs are absent from
 * soft-float builds for identical reasons.
 */

#ifndef _GNU_STUBS_O32_SOFT_H
#define _GNU_STUBS_O32_SOFT_H

#ifdef _LIBC
# error Applications may not define the macro _LIBC
#endif

#define __stub_chflags
#define __stub_fchflags
#define __stub_gtty
#define __stub_revoke
#define __stub_setlogin
#define __stub_sigreturn
#define __stub_stty

#endif /* _GNU_STUBS_O32_SOFT_H */
