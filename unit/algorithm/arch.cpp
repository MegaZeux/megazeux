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

#include "../Unit.hpp"
#include "../../src/algorithm/arch.h"
#include "../../src/platform/log.h"

#include <string.h>

#if !defined(_WIN32)
#define HAS_UTSNAME
#include <sys/utsname.h>

#ifndef ARCHITECTURE_IS_X86
#define ARCHITECTURE_IS_X86 0
#endif
#ifndef ARCHITECTURE_IS_X86_64
#define ARCHITECTURE_IS_X86_64 0
#endif
#ifndef ARCHITECTURE_IS_ALPHA
#define ARCHITECTURE_IS_ALPHA 0
#endif
#ifndef ARCHITECTURE_IS_ARM
#define ARCHITECTURE_IS_ARM 0
#endif
#ifndef ARCHITECTURE_IS_ARM64
#define ARCHITECTURE_IS_ARM64 0
#endif
#ifndef ARCHITECTURE_IS_HPPA
#define ARCHITECTURE_IS_HPPA 0
#endif
#ifndef ARCHITECTURE_IS_HPPA64
#define ARCHITECTURE_IS_HPPA64 0
#endif
#ifndef ARCHITECTURE_IS_LOONGARCH
#define ARCHITECTURE_IS_LOONGARCH 0
#endif
#ifndef ARCHITECTURE_IS_LOONGARCH64
#define ARCHITECTURE_IS_LOONGARCH64 0
#endif
#ifndef ARCHITECTURE_IS_M68K
#define ARCHITECTURE_IS_M68K 0
#endif
#ifndef ARCHITECTURE_IS_MIPS
#define ARCHITECTURE_IS_MIPS 0
#endif
#ifndef ARCHITECTURE_IS_MIPS64
#define ARCHITECTURE_IS_MIPS64 0
#endif
#ifndef ARCHITECTURE_IS_PPC
#define ARCHITECTURE_IS_PPC 0
#endif
#ifndef ARCHITECTURE_IS_PPC64
#define ARCHITECTURE_IS_PPC64 0
#endif
#ifndef ARCHITECTURE_IS_RISCV
#define ARCHITECTURE_IS_RISCV 0
#endif
#ifndef ARCHITECTURE_IS_RISCV64
#define ARCHITECTURE_IS_RISCV64 0
#endif
#ifndef ARCHITECTURE_IS_S390
#define ARCHITECTURE_IS_S390 0
#endif
#ifndef ARCHITECTURE_IS_S390X
#define ARCHITECTURE_IS_S390X 0
#endif
#ifndef ARCHITECTURE_IS_SPARC
#define ARCHITECTURE_IS_SPARC 0
#endif
#ifndef ARCHITECTURE_IS_SPARC64
#define ARCHITECTURE_IS_SPARC64 0
#endif
#ifndef ARCHITECTURE_IS_SUPERH
#define ARCHITECTURE_IS_SUPERH 0
#endif
#endif /* utsname */

struct machtest
{
  const char *machine;
  int match;
};

UNITTEST(Machine)
{
#ifdef HAS_UTSNAME
  static constexpr machtest machines[] =
  {
    { "i386",         ARCHITECTURE_IS_X86 },
    { "i486",         ARCHITECTURE_IS_X86 },
    { "i586",         ARCHITECTURE_IS_X86 },
    { "i686",         ARCHITECTURE_IS_X86 },
    { "i786",         ARCHITECTURE_IS_X86 },
    { "x86",          ARCHITECTURE_IS_X86 },
    { "amd64",        ARCHITECTURE_IS_X86 & ARCHITECTURE_IS_X86_64 },
    { "x86_64",       ARCHITECTURE_IS_X86 & ARCHITECTURE_IS_X86_64 },
    { "armel",        ARCHITECTURE_IS_ARM },
    { "armhf",        ARCHITECTURE_IS_ARM },
    { "armv6",        ARCHITECTURE_IS_ARM },
    { "armv6eb",      ARCHITECTURE_IS_ARM },
    { "armv7",        ARCHITECTURE_IS_ARM },
    { "arm64",        ARCHITECTURE_IS_ARM & ARCHITECTURE_IS_ARM64 },
    { "aarch64",      ARCHITECTURE_IS_ARM & ARCHITECTURE_IS_ARM64 },
    { "alpha",        ARCHITECTURE_IS_ALPHA },
    { "hppa",         ARCHITECTURE_IS_HPPA },
    { "parisc",       ARCHITECTURE_IS_HPPA },
    { "hppa64",       ARCHITECTURE_IS_HPPA & ARCHITECTURE_IS_HPPA64 },
    { "loong64",      ARCHITECTURE_IS_LOONGARCH & ARCHITECTURE_IS_LOONGARCH64 },
    { "loongarch64",  ARCHITECTURE_IS_LOONGARCH & ARCHITECTURE_IS_LOONGARCH64 },
    { "m68k",         ARCHITECTURE_IS_M68K },
    { "mips",         ARCHITECTURE_IS_MIPS },
    { "mips64",       ARCHITECTURE_IS_MIPS & ARCHITECTURE_IS_MIPS64 },
    { "ppc",          ARCHITECTURE_IS_PPC },
    { "ppc64",        ARCHITECTURE_IS_PPC & ARCHITECTURE_IS_PPC64 },
    { "riscv",        ARCHITECTURE_IS_RISCV }, /* NetBSD: 64-bit; T2: 32-bit */
    { "riscv64",      ARCHITECTURE_IS_RISCV & ARCHITECTURE_IS_RISCV64 },
    { "s390",         ARCHITECTURE_IS_S390 },
    { "s390x",        ARCHITECTURE_IS_S390 & ARCHITECTURE_IS_S390X },
    { "sparc",        ARCHITECTURE_IS_SPARC },
    { "sparc64",      ARCHITECTURE_IS_SPARC & ARCHITECTURE_IS_SPARC64 },
    { "sh4",          ARCHITECTURE_IS_SUPERH },
  };
  struct utsname u;
  bool has_match = false;

  int ret = uname(&u);
  ASSERT(ret >= 0, "");

  for(const machtest &m : machines)
  {
    if(strcasecmp(m.machine, u.machine) == 0)
    {
      ASSERT(m.match, "missing one or more define for '%s'", m.machine);
      has_match = true;
    }
  }
  if(!has_match)
    warn("add machine for arch '%s'\n", u.machine);
#else
  /* TODO: way to query this at runtime for Windows etc? */
  SKIP();
#endif
}

UNITTEST(IntEndian)
{
  SECTION(ByteOrder)
  {
    uint8_t expected[4] = { 0x0a, 0x0b, 0x0c, 0x0d };
    uint8_t result[4];
    uint32_t value = ARCH_BYTE_ORDER;

    memcpy(result, &value, sizeof(value));
    ASSERTMEM(result, expected, 4, "");
  }

  SECTION(LilOrBig)
  {
    uint8_t expected[4] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t result[4];
    uint32_t value = ARCH_LIL_OR_BIG(0x04030201, 0x01020304);

    memcpy(result, &value, sizeof(value));
    ASSERTMEM(result, expected, 4, "");
  }
}

UNITTEST(FloatEndian)
{
#define APPROX 3.255394079200307
  uint64_t u64 = 0x400a0b0c0d0e0f0full;
  double df;

#if ARCH_FLOAT_ORDER != ARCH_BYTE_ORDER
  /* If the float word order doesn't match the byte order,
   * swapping words should be sufficient to correct this test. */
  u64 = (u64 >> 32) | (u64 << 32);
#endif
  memcpy(&df, &u64, sizeof(uint64_t));

  if(df - APPROX < 0.00001 || df - APPROX > 0.00001)
    ASSERTEQ(df, APPROX, "");
}

UNITTEST(Bitwidth)
{
  ASSERTEQ(ARCHITECTURE_BITS >> 3, sizeof(void *), "");
  ASSERTEQ(ARCHITECTURE_BITS, ARCH_32_OR_64(32, 64), "");
}

struct aligntest
{
  uint8_t pad1[2];
  uint32_t a;
  uint64_t b;
  uint8_t pad2[3];
  uint32_t c;
  uint64_t d;
  uint8_t pad3[1];
  uint16_t e;
};

UNITTEST(Alignment)
{
#if defined(ARCH_INT32_UNALIGN) || defined(ARCH_INT64_UNALIGN)
  uint8_t buf[24];
  uint8_t *pos = buf;
  size_t i;
#endif

  SECTION(Aligned)
  {
    aligntest t{};

    ASSERTEQ(((arch_uint)&t.a) % ARCH_INT32_MODULO, 0, "");
    ASSERTEQ(((arch_uint)&t.b) % ARCH_INT64_MODULO, 0, "");
    ASSERTEQ(((arch_uint)&t.c) % ARCH_INT32_MODULO, 0, "");
    ASSERTEQ(((arch_uint)&t.d) % ARCH_INT64_MODULO, 0, "");
    ASSERTEQ(((arch_uint)&t.e) % ARCH_INT16_MODULO, 0, "");
  }

  SECTION(Unaligned32)
  {
#ifndef ARCH_INT32_UNALIGN
    SKIP();
#else
    static_assert(ARCH_INT32_UNALIGN > 0, "");

    static constexpr uint32_t expected[] =
    {
      ARCH_LIL_OR_BIG(0x04030201, 0x01020304),
      ARCH_LIL_OR_BIG(0x05040302, 0x02030405),
      ARCH_LIL_OR_BIG(0x06050403, 0x03040506),
      ARCH_LIL_OR_BIG(0x07060504, 0x04050607),
      ARCH_LIL_OR_BIG(0x08070605, 0x05060708),
    };
    const uint32_t *exp_pos = expected;

    while((arch_uint)pos % ARCH_INT32_MODULO)
      pos++;

    for(i = 0; i < 8; i++)
      pos[i] = i + 1;

    for(i = 0; i <= 4; i += ARCH_INT32_UNALIGN, exp_pos++)
    {
      const uint32_t *check = ARCH_UINT32_UNALIGN_CAST(pos + i);
      ASSERTEQ(*check, *exp_pos, "%zu", i);
    }
#endif
  }

  SECTION(Unaligned64)
  {
#ifndef ARCH_INT64_UNALIGN
    SKIP();
#else
    static_assert(ARCH_INT64_UNALIGN > 0, "");

    static constexpr uint64_t expected[] =
    {
      ARCH_LIL_OR_BIG(0x0807060504030201ull, 0x0102030405060708ull),
      ARCH_LIL_OR_BIG(0x0908070605040302ull, 0x0203040506070809ull),
      ARCH_LIL_OR_BIG(0x0a09080706050403ull, 0x030405060708090aull),
      ARCH_LIL_OR_BIG(0x0b0a090807060504ull, 0x0405060708090a0bull),
      ARCH_LIL_OR_BIG(0x0c0b0a0908070605ull, 0x05060708090a0b0cull),
      ARCH_LIL_OR_BIG(0x0d0c0b0a09080706ull, 0x060708090a0b0c0dull),
      ARCH_LIL_OR_BIG(0x0e0d0c0b0a090807ull, 0x0708090a0b0c0d0eull),
      ARCH_LIL_OR_BIG(0x0f0e0d0c0b0a0908ull, 0x08090a0b0c0d0e0full),
      ARCH_LIL_OR_BIG(0x100f0e0d0c0b0a09ull, 0x090a0b0c0d0e0f10ull),
    };
    const uint64_t *exp_pos = expected;

    while((arch_uint)pos % ARCH_INT64_MODULO)
      pos++;

    for(i = 0; i < 16; i++)
      pos[i] = i + 1;

    for(i = 0; i <= 8; i += ARCH_INT64_UNALIGN, exp_pos++)
    {
      const uint64_t *check = ARCH_UINT64_UNALIGN_CAST(pos + i);
      ASSERTEQ(*check, *exp_pos, "%zu", i);
    }
#endif
  }
}

UNITTEST(ArchUint)
{
  uint8_t buffer[256]{};
  uint8_t *p8;
  arch_uint *pa;

  ASSERTEQ(ARCHITECTURE_BITS >> 3, sizeof(arch_uint), "");
  ASSERTEQ(ARCH_UINT_MODULO, ARCH_32_OR_64(ARCH_INT32_MODULO, ARCH_INT64_MODULO), "");

  for(int i = 0; i < arraysize(buffer); i++)
  {
    p8 = buffer + i;
    pa = ARCH_UINT_TRUNC_CAST(p8);
    ASSERTEQ((arch_uint)pa, (arch_uint)p8 & ARCH_32_OR_64(~3, ~7), "%d", i);

#ifdef ARCH_UINT_UNALIGN
    static_assert(ARCH_UINT_UNALIGN > 0, "");
    pa = ARCH_UINT_UNALIGN_CAST(p8);
    ASSERTEQ((arch_uint)pa, (arch_uint)p8 & ~(ARCH_UINT_UNALIGN - 1), "%d", i);
#endif
  }
}
