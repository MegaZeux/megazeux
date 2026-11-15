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
  /* TODO: incomplete
   * - more ARM
   * - loong32/loongarch32?
   * - riscv32?
   */
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

struct aligntest
{
  uint8_t pad1[2];
  uint32_t a;
  uint64_t b;
  uint8_t pad2[3];
  uint32_t c;
  uint64_t d;
};

UNITTEST(Alignment)
{
  aligntest t{};

  ASSERTEQ(((arch_uint)&t.a) % ARCH_INT32_MODULO, 0, "");
  ASSERTEQ(((arch_uint)&t.b) % ARCH_INT64_MODULO, 0, "");
  ASSERTEQ(((arch_uint)&t.c) % ARCH_INT32_MODULO, 0, "");
  ASSERTEQ(((arch_uint)&t.d) % ARCH_INT64_MODULO, 0, "");

  /* FIXME: misalignment */
  UNIMPLEMENTED();
}

UNITTEST(IntEndian)
{
  uint8_t expected[4] = { 0x0a, 0x0b, 0x0c, 0x0d };
  uint8_t result[4];
  uint32_t value = ARCH_BYTE_ORDER;

  memcpy(result, &value, sizeof(value));
  ASSERTMEM(result, expected, 4, "");
}

/* TODO: x87? */
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

    /* ARCH_UINT_UNSAFE_CAST should only be used on architectures that can
     * actually represent misaligned pointers and support accessing them,
     * as this is technically undefined behavior. */
    /* FIXME: misalignment and ARCH_UINT_UNSAFE_CAST */
  }
}
