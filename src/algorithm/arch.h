/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MEGAZEUX_ALGORITHM_ARCH_H
#define MEGAZEUX_ALGORITHM_ARCH_H

#include <stdint.h>

/* FIXME: versions for this header? BSDs? POSIX version is 8 (2024)? */
#if defined(__GLIBC__)
#include <endian.h>
#endif

/* Architecture-specific defines.
 * See https://github.com/cpredef/predef for more information. */

/* Machine defines. */

#if /* GNU */  defined(__amd64__) || defined(__x86_64__) || \
    /* MSVC */ defined(_M_X64) || defined(_M_AMD64)
#define ARCHITECTURE_IS_X86_64 1
#endif
#if /* GNU */    defined(__i386__) || \
    /* MSVC */   defined(_M_X86) || defined(_M_IX86) || \
    /* Watcom */ defined(__X86__) || \
    /* MinGW */  defined(_X86_) || \
    defined(ARCHITECTURE_IS_X86_64)
#define ARCHITECTURE_IS_X86 1
#endif

#if /* GNU */ defined(__alpha__) || /* MSVC */ defined(_M_ALPHA)
#define ARCHITECTURE_IS_ALPHA 1
#endif

#if /* GNU */ defined(__aarch64__) || /* MSVC */ defined(_M_ARM64)
#define ARCHITECTURE_IS_ARM64 1
#endif
#if /* GNU */ defined(__arm__) || /* MSVC */ defined(_M_ARM) || \
    defined(ARCHITECTURE_IS_ARM64)
#define ARCHITECTURE_IS_ARM 1
#endif

#if /* GNU */ defined(__hppa__)
#define ARCHITECTURE_IS_HPPA 1
#if defined(_PA_RISC_2_0) || defined(__RISC2_0_) || \
    defined(__HPPA20__) || defined(__PA8000__)
#define ARCHITECTURE_IS_HPPA64 1
#endif
#endif

#if /* Official */ defined(__loongarch__)
#define ARCHITECTURE_IS_LOONGARCH 1
#if __loongarch_grlen >= 64
#define ARCHITECTURE_IS_LOONGARCH64 1
#endif
#endif

#if /* GNU */ defined(__m68k__)
#define ARCHITECTURE_IS_M68K 1
#endif

#if /* GNU */ defined(__mips__) || defined(__mips) || defined(__MIPS__)
#define ARCHITECTURE_IS_MIPS 1
#if /* GNU */ defined(_R4000) || defined(_R5900) || \
    /* ? */ (defined(_MIPS_SIM) && defined(_ABI64) && _MIPS_SIM == _ABI64) || \
    /* ? */ (defined(_MIPS_ISA) && _MIPS_ISA >= _MIPS_ISA_MIPS3)
#define ARCHITECTURE_IS_MIPS64 1
#endif
#endif

#if /* GNU */ defined(__powerpc64__) || defined(__ppc64__) || \
              defined(__PPC64__) || defined(_ARCH_PPC64)
#define ARCHITECTURE_IS_PPC64 1
#endif
#if /* GNU */ defined(__powerpc) || defined(__powerpc__) || defined(_ARCH_PPC) || \
    /* MSVC */ defined(_M_PPC) || \
    defined(ARCHITECTURE_IS_PPC64)
#define ARCHITECTURE_IS_PPC 1
#endif

#if /* Official */ defined(__riscv)
#define ARCHITECTURE_IS_RISCV 1
#if __riscv_xlen >= 64
#define ARCHITECTURE_IS_RISCV64 1
#endif
#endif

#if /* GNU */ defined(__s390x__) || /* clang */ defined(__zarch__)
#define ARCHITECTURE_IS_S390X 1
#endif
#if /* GNU */ defined(__s390__) || \
    defined(ARCHITECTURE_IS_S390X)
#define ARCHITECTURE_IS_S390 1
#endif

#if /* GNU */ defined(__sparc__) || /* Sun */ defined(__sparc)
#define ARCHITECTURE_IS_SPARC 1
#if /* GNU */ defined(__sparc_v9__) || /* Sun */ defined(__sparcv9) || \
    /* Sun */ defined(__arch64__)
#define ARCHITECTURE_IS_SPARC64 1
#endif
#endif

#if /* GNU */ defined(__sh__)
#define ARCHITECTURE_IS_SUPERH 1
#endif


/* Integer/pointer endian. */
#define ARCH_LIL_ENDIAN 0x0d0c0b0a
#define ARCH_BIG_ENDIAN 0x0a0b0c0d

/* Try compiler macros first. */
#if defined(__BYTE_ORDER__) && \
    defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN
#endif

#elif defined(__LITTLE_ENDIAN__)
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN

#elif defined(__BIG_ENDIAN__)
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN
#endif /* compiler macros */

/* Try libc macros if that didn't work. */
#ifndef ARCH_BYTE_ORDER
/* POSIX */
#if defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#if BYTE_ORDER == LITTLE_ENDIAN
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN
#endif

/* glibc */
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN
#endif
#endif
#endif /* libc macros */

/* Try specific architecture endian macros (or guess) if that didn't work. */
#ifndef ARCH_BYTE_ORDER
/* Always LE, or bi-endian with preprocessor define for LE */
#if defined(ARCHITECTURE_IS_X86) || \
    defined(ARCHITECTURE_IS_LOONGARCH) || \
    (defined(ARCHITECTURE_IS_ARM) && \
     (defined(__ARMEL__) || defined(__AARCH64EL__))) || \
    (defined(ARCHITECTURE_IS_MIPS) && \
     (defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)))
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN

/* Always BE, or bi-endian with preprocessor define for BE */
#elif defined(ARCHITECTURE_IS_M68K) || \
      (defined(ARCHITECTURE_IS_ARM) && \
       (defined(__ARMEB__) || defined(__AARCH64EB__))) || \
      (defined(ARCHITECTURE_IS_MIPS) && \
       (defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)))
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN

/* Bi-endian; default LE if nothing else specified */
#elif defined(ARCHITECTURE_IS_ALPHA) || \
      defined(ARCHITECTURE_IS_ARM) || \
      defined(ARCHITECTURE_IS_RISCV) || \
      defined(ARCHITECTURE_IS_SUPERH)
#define ARCH_BYTE_ORDER ARCH_LIL_ENDIAN

/* Bi-endian; default BE if nothing else specified */
#elif defined(ARCHITECTURE_IS_HPPA) || \
      defined(ARCHITECTURE_IS_PPC) || \
      defined(ARCHITECTURE_IS_S390) || \
      defined(ARCHITECTURE_IS_SPARC)
#define ARCH_BYTE_ORDER ARCH_BIG_ENDIAN
#endif
#endif /* specific architecture endian macros */

#ifndef ARCH_BYTE_ORDER
#error Failed to determine endianness!
#endif

#if ARCH_BYTE_ORDER == ARCH_LIL_ENDIAN
#define ARCH_LIL_OR_BIG(lil, big) (lil)
#else
#define ARCH_LIL_OR_BIG(lil, big) (big)
#endif


/* Floating point endian. */

/* Try compiler macros first. */
#if defined(__FLOAT_WORD_ORDER__) && \
    defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ARCH_FLOAT_ORDER ARCH_LIL_ENDIAN
#elif __FLOAT_WORD_ORDER == __ORDER_BIG_ENDIAN__
#define ARCH_FLOAT_ORDER ARCH_BIG_ENDIAN
#endif
#endif /* compiler macros */

/* Try libc macros if that didn't work. */
#ifndef ARCH_FLOAT_ORDER
/* glibc */
#if defined(__FLOAT_WORD_ORDER) && defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
#define ARCH_FLOAT_ORDER ARCH_LIL_ENDIAN
#elif __FLOAT_WORD_ORDER == __BIG_ENDIAN
#define ARCH_FLOAT_ORDER ARCH_BIG_ENDIAN
#endif

/* FIXME: POSIX doesn't have FLOAT_WORD_ORDER, do any of the BSDs? */
#elif defined(FLOAT_WORD_ORDER) && defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#if FLOAT_WORD_ORDER == LITTLE_ENDIAN
#define ARCH_FLOAT_ORDER ARCH_LIL_ENDIAN
#elif FLOAT_WORD_ORDER == BIG_ENDIAN
#define ARCH_FLOAT_ORDER ARCH_BIG_ENDIAN
#endif
#endif
#endif /* libc macros */

#ifndef ARCH_FLOAT_ORDER
#define ARCH_FLOAT_ORDER ARCH_BYTE_ORDER
#endif


/* Native integer/pointer width. */
#if defined(__LP64__) || defined(_LP64) || \
    defined(ARCHITECTURE_IS_X86_64) || \
    defined(ARCHITECTURE_IS_ALPHA) || \
    defined(ARCHITECTURE_IS_ARM64) || \
    defined(ARCHITECTURE_IS_HPPA64) || \
    defined(ARCHITECTURE_IS_LOONGARCH64) || \
    defined(ARCHITECTURE_IS_MIPS64) || \
    defined(ARCHITECTURE_IS_PPC64) || \
    defined(ARCHITECTURE_IS_RISCV64) || \
    defined(ARCHITECTURE_IS_S390X) || \
    defined(ARCHITECTURE_IS_SPARC64)
#define ARCHITECTURE_BITS 64
#else
#define ARCHITECTURE_BITS 32
#endif


/* Integer alignment.
 *
 * Some platforms allow misaligned integers with little or no penalty.
 * Most prefer to keep them aligned, though. Unaligned pointers should ONLY
 * be used on platforms that define ARCH_INT32/64_UNALIGN and ONLY on
 * conventional memory (not MMIO like the PCI bus), and even then, sparingly.
 * Both _MODULO and _UNALIGN defines should be treated as alignment modulos,
 * i.e., a value of 2 means that pointer needs to be aligned to even bytes.
 * _MODULO represents the "normal" alignment and _UNALIGN represents the
 * smallest "safe" misalignment.
 *
 * Most architectures not listed here will SIGBUS on unaligned accesses.
 * RISC-V allows them via a trap sent by the processor for software emulation,
 * which is extremely slow and thus disregarded here.
 */
#define ARCH_INT16_MODULO 2

/* Motorola 68000 family supports reading 32-bit integers at 16-bit offsets.
 * This is not considered unaligned access. */
#if defined(ARCHITECTURE_IS_M68K)
#define ARCH_INT32_MODULO ARCH_INT16_MODULO
#endif

#ifndef ARCH_INT32_MODULO
#define ARCH_INT32_MODULO 4
#endif

/* 32-bit platforms almost always allow 64-bit integers at 32-bit offsets.
 * Note that this is NOT necessarily true for 64-bit floats. */
#if ARCHITECTURE_BITS < 64
#define ARCH_INT64_MODULO ARCH_INT32_MODULO
#endif

#ifndef ARCH_INT64_MODULO
#define ARCH_INT64_MODULO 8
#endif

/* x86, AMD64, and Wasm allow unaligned integer pointers in conventional
 * memory. This is slower than aligned access but useful for some cases. */
#if defined(ARCHITECTURE_IS_X86) || defined(__EMSCRIPTEN__)
#define ARCH_INT32_UNALIGN 1
#if ARCHITECTURE_BITS >= 64
#define ARCH_INT64_UNALIGN 1
#endif
#endif

/* ARM may or may not allow unaligned accesses based on the revision used and
 * whether or not it is allowed at runtime. __ARM_FEATURE_UNALIGNED is
 * provided to indicate when this is possible, but it is not trustworthy for
 * 32-bit ARM. This is almost always (but not necessarily) allowed on ARM64. */
#if defined(ARCHITECTURE_IS_ARM64) && defined(__ARM_FEATURE_UNALIGNED)
#define ARCH_INT32_UNALIGN 1
#if ARCHITECTURE_BITS >= 64
#define ARCH_INT64_UNALIGN 1
#endif
#endif

/* Enable unaligned access codepaths for Motorola 68000 (see above). */
#if defined(ARCHITECTURE_IS_M68K)
#define ARCH_INT32_UNALIGN ARCH_INT32_MODULO
#endif

/* PowerPC can access 32-bit integers unaligned but not 64-bit integers.
 * POWER8+ can access both unaligned. */
#if defined(ARCHITECTURE_IS_PPC)
#define ARCH_INT32_UNALIGN 1
#if defined(_ARCH_PWR8)
#define ARCH_INT64_UNALIGN 1
#endif
#endif


/* Fast integer type defines. */
#if ARCHITECTURE_BITS >= 64
typedef uint64_t arch_uint;
#define ARCH_UINT_MODULO        ARCH_INT64_MODULO
#define ARCH_32_OR_64(x32,x64)  (x64)
#ifdef ARCH_INT64_UNALIGN
#define ARCH_UINT_UNALIGN       ARCH_INT64_UNALIGN
#endif
#else
typedef uint32_t arch_uint;
#define ARCH_UINT_MODULO        ARCH_INT32_MODULO
#define ARCH_32_OR_64(x32,x64)  (x32)
#ifdef ARCH_INT32_UNALIGN
#define ARCH_UINT_UNALIGN       ARCH_INT32_UNALIGN
#endif
#endif

#define ARCH_UINT_TRUNC_CAST(p) ((arch_uint *)((arch_uint)(p) & ~(sizeof(arch_uint) - 1)))

#ifdef ARCH_INT32_UNALIGN
#define ARCH_UINT32_UNALIGN_CAST(p) ((uint32_t *)((arch_uint)(p) & ~(ARCH_INT32_UNALIGN - 1)))
#endif
#ifdef ARCH_INT64_UNALIGN
#define ARCH_UINT64_UNALIGN_CAST(p) ((uint64_t *)((arch_uint)(p) & ~(ARCH_INT64_UNALIGN - 1)))
#endif
#ifdef ARCH_UINT_UNALIGN
#define ARCH_UINT_UNALIGN_CAST(p) ((arch_uint *)((arch_uint)(p) & ~(ARCH_UINT_UNALIGN - 1)))
#endif

#endif /* MEGAZEUX_ALGORITHM_ARCH_H */
