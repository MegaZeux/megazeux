/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk
 * Copyright (C) 2020, 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __ENDIAN_H
#define __ENDIAN_H

/* Specific architecture defines. */
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#define PLATFORM_IS_X86_64
#endif
#if defined(__i386__) || defined(_M_IX86) || defined(PLATFORM_IS_X86_64)
#define PLATFORM_IS_X86
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define PLATFORM_IS_ARM64
#endif
#if defined(__arm__) || defined(_M_ARM) || defined(PLATFORM_IS_ARM64)
#define PLATFORM_IS_ARM
#endif

#if defined(__powerpc64__) || defined(__PPC64__) || \
 defined(__ppc64__) || defined(_ARCH_PPC64)
#define PLATFORM_IS_PPC64
#endif
#if defined(__powerpc__) || defined(__PPC__) || \
 defined(__ppc__) || defined(_ARCH_PPC) || defined(__POWERPC__) || \
 defined(PLATFORM_IS_PPC64)
#define PLATFORM_IS_PPC
#endif

#if defined(__m68k__) || defined(mc68000) || defined(_M_M68K)
#define PLATFORM_IS_M68K
#endif

/* Use GCC/clang or a list of architectures (both checks borrowed from SDL) to
 * determine the endianness. If SDL is enabled, platform_sdl.c will check this.
 */

#define PLATFORM_LIL_ENDIAN 0x1234
#define PLATFORM_BIG_ENDIAN 0x4321

#if defined(__BIG_ENDIAN__)
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN__)
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
/* predefs from newer gcc and clang versions: */
#elif defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__) && defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#else
#error Unsupported endianness
#endif /**/
#elif defined(__hppa__) || \
    defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
    ((defined(__mips__) || defined(__mips) || defined(__MIPS__)) && defined(__MIPSEB__)) || \
    defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
    defined(__s390__) || defined(__s390x__) || defined(__zarch__) || defined(__SYSC_ZARCH__) || \
    defined(__sparc__)
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#else
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#endif

/* ModPlug and XMP both use this name to find out about endianness. It's not
 * too bad to pollute our namespace with it, so just do so here.
 */
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
#define WORDS_BIGENDIAN
#endif

/**
 * Also try to get the platform bit width.
 * Emscripten natively supports 64-bit math when compiling to Wasm.
 */
#if defined(_WIN64) || defined(__EMSCRIPTEN__) || \
  defined(PLATFORM_IS_X86_64) || \
  defined(PLATFORM_IS_PPC64) || \
  defined(PLATFORM_IS_ARM64) || \
  (defined(__sparc__) && defined(__arch64__)) || \
  ((defined(__riscv) || defined(__riscv__)) && __riscv_xlen >= 64) || \
  ((defined(__mips__) || defined(__mips) || defined(__MIPS__)) && \
    defined(_MIPS_SIM) && defined(_ABI64) && _MIPS_SIM == _ABI64) || \
  (defined(__loongarch__) && defined(__loongarch_grlen) && \
    __loongarch_grlen == 64) || \
  (defined(__GNUC__) && \
     (defined(__alpha__) || \
     defined(__s390x__) || defined(__zarch__)))
#define ARCHITECTURE_BITS 64
#else
#define ARCHITECTURE_BITS 32
#endif

/**
 * Also define some useful constants for byte alignment.
 * The ALIGN macros are for algorithms attempting to keep as aligned as useful.
 *
 * The UNALIGN macros are for algorithms where fast unaligned accesses are
 * more beneficial than keeping strictly aligned. For platforms where unaligned
 * accesses may result in a SIGBUS (ARM), or where accesses technically are
 * supported but are too slow to be worthwhile (e.g. RISC-V), do not define
 * the UNALIGN macros. (This includes ARMv8+, where alignment exceptions still
 * exist when no MMU is present or by activating a processor flag.)
 */
#define ALIGN_16_MODULO 0x02

/**
 * Some 32-bit-capable processors (such as the Motorola 68000) still align
 * their data to 16-bit boundaries.
 */
#ifdef PLATFORM_IS_M68K
#define ALIGN_32_MODULO ALIGN_16_MODULO
#else
#define ALIGN_32_MODULO 0x04
#endif

#if ARCHITECTURE_BITS >= 64
#define ALIGN_64_MODULO 0x08
#else
#define ALIGN_64_MODULO ALIGN_32_MODULO
#endif

/* x86, x86-64, and Wasm can access 32-bit and 64-bit integers unaligned.
 * This is slower than aligned accesses but is useful for rendering. */
#if defined(PLATFORM_IS_X86) || defined(__EMSCRIPTEN__)
#define PLATFORM_UNALIGN_32 0x01
#if ARCHITECTURE_BITS >= 64
#define PLATFORM_UNALIGN_64 0x01
#endif
#endif

/* ARMv7+ can access 32-bit (AArch64: and 64-bit) integers unaligned if
 * the system allows this. This is almost always (but not necessarily) enabled
 * for AArch64, and sometimes for ARMv7 and AArch32. Currently trusting
 * __ARM_FEATURE_UNALIGNED to be accurate for any relevant environment.
 * This is currently disabled for 32-bit ARM because clang 19 -O3 will emit
 * STM instructions that trap even if __ARM_FEATURE_UNALIGNED is defined. */
#if defined(PLATFORM_IS_ARM64) && defined(__ARM_FEATURE_UNALIGNED)
#define PLATFORM_UNALIGN_32 0x01
#if ARCHITECTURE_BITS >= 64
#define PLATFORM_UNALIGN_64 0x01
#endif
#endif

/* PowerPC can access 32-bit ints unaligned, but 64-bit ints must be aligned.
 * POWER8 and up have safe unaligned access for 64-bit ints. */
#if defined(PLATFORM_IS_PPC)
#define PLATFORM_UNALIGN_32 0x01
#if defined(_ARCH_PWR8)
#define PLATFORM_UNALIGN_64 0x01
#endif
#endif

/* Motorola 68000 has limited unaligned safety, see above. */
#ifdef PLATFORM_IS_M68K
#define PLATFORM_UNALIGN_32 0x02
#endif

#endif // __ENDIAN_H
