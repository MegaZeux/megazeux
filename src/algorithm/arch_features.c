/* MegaZeux
 *
 * Copyright (C) 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "arch.h"
#include "arch_features.h"

/* TODO: MSVC and Watcom ASM */
#ifdef ARCHITECTURE_IS_X86
#if defined(__GNUC__) || defined(__clang__)
#define HAS_GNU_INLINE_ASM
#endif

static inline boolean has_cpuid(void)
{
#ifdef ARCHITECTURE_IS_X86_64
  return true;

#elif defined(HAS_GNU_INLINE_ASM)
  unsigned tmp;
  __asm__(
    "pushf"                 "\n\t" // save eflags
    "pop %%eax"             "\n\t" // load eflags
    "xor $0x200000, %%eax"  "\n\t" // invert ID bit in eflags
    "mov %%eax, %%ebx"      "\n\t"
    "push %%eax"            "\n\t" // save modified eflags
    "popf"                  "\n\t"
    "pushf"                 "\n\t"
    "pop %%eax"             "\n\t" // reload modified(?) eflags
    "xor %%ebx, %%eax"      "\n\t" // should be 0
    : "=a"(tmp)
    :
    : "ebx"
  );
  if(tmp)
    return false;
  return true;

#else
  return false;
#endif
}
#endif

boolean mzx_architecture_has_sse2(void)
{
#ifdef ARCHITECTURE_IS_X86_64
  return true;

#elif defined(ARCHITECTURE_IS_X86)
  static boolean checked = false;
  static boolean has_sse2 = false;
  if(!checked)
  {
    unsigned eax = 1;
    unsigned edx;

    if(has_cpuid())
    {
#ifdef HAS_GNU_INLINE_ASM
      __asm__(
        "cpuid"
        : "=d"(edx)
        : "a"(eax)
        : "ebx", "ecx"
      );
      if((edx & (3 << 25)) == 3 << 25) /* SSE + SSE2 */
        has_sse2 = true;
#endif
    }
    checked = true;
  }
  return has_sse2;

#else
  return false;
#endif
}

boolean mzx_architecture_has_avx(void)
{
#ifdef ARCHITECTURE_IS_X86
  static boolean checked = false;
  static boolean has_avx = false;
  if(!checked)
  {
    if(has_cpuid())
    {
#ifdef HAS_GNU_INLINE_ASM
#define CPUID_1_AVX_XGETBV ((1 << 28) | (1 << 27))
#define XGETBV_XMM_YMM_ENABLED ((1 << 2) | (1 << 1))
      int eax;
      int ecx;
      __asm__(
        "cpuid"
        : "=c"(ecx)
        : "a"(1)
        : "ebx", "edx"
      );
      if((ecx & CPUID_1_AVX_XGETBV) == CPUID_1_AVX_XGETBV)
      {
        __asm__(
          "xgetbv"
          : "=a"(eax)
          : "c" (0)
          : "edx"
        );
        if((eax & XGETBV_XMM_YMM_ENABLED) == XGETBV_XMM_YMM_ENABLED)
          has_avx = true;
      }
#endif
    }
    checked = true;
  }
  return has_avx;

#else
  return false;
#endif
}

boolean mzx_architecture_has_avx2(void)
{
#ifdef ARCHITECTURE_IS_X86
  static boolean checked = false;
  static boolean has_avx2 = false;

  if(!checked)
  {
    if(mzx_architecture_has_avx())
    {
#ifdef HAS_GNU_INLINE_ASM
      int ebx;
      __asm__(
        "cpuid"
        : "=b"(ebx)
        : "a"(7), "c"(0)
        : "edx"
      );
      if(ebx & (1 << 5)) /* AVX2 */
        has_avx2 = true;
#endif
    }
    checked = true;
  }
  return has_avx2;

#else
  return false;
#endif
}


#if defined(ARCHITECTURE_IS_ARM) && !defined(ARCHITECTURE_IS_ARM64)
/* iOS processor feature detection header (32-bit). */
#if defined(__APPLE__)
#include <sys/sysctl.h>
static boolean has_neon_check(void)
{
  size_t val = 0;
  size_t sz = sizeof(val);
  int ret = sysctlbyname("hw.optional.neon", &val, &sz, NULL, 0);
  return (ret == 0 && val);
}

/* Android API <18 feature detection via /proc/cpuinfo (32-bit). */
#elif defined(__ANDROID__) && (!defined(__ANDROID_API__) || __ANDROID_API__ < 18)
#include <stdio.h>
#warning hey this really sucks, please replace for 2.94

static boolean has_neon_check(void)
{
  char buffer[1024];
  FILE *fp = fopen_unsafe("/proc/cpuinfo", "r");
  if(!fp)
    return false;

  while(fgets(buffer, sizeof(buffer), fp))
  {
    if(strstr(buffer, "neon") == NULL && strstr(buffer, "asimd") == NULL)
      continue;

    fclose(fp);
    return true;
  }
  fclose(fp);
  return false;
}

/* Linux processor feature detection headers (32-bit). */
#elif defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
static boolean has_neon_check(void)
{
  unsigned long tmp = getauxval(AT_HWCAP);
  return !!(tmp & HWCAP_NEON);
}

/* Anything with intrinsics visible here has Neon enabled globally (32-bit)? */
#elif (defined(__ARM_ARCH) && __ARM_ARCH >= 8) || defined(__ARM_NEON)
#define has_neon_check() true

/* No intrinsics and no runtime check available; disable. */
#else
#define has_neon_check() false
#endif
#endif /* ARCHITECTURE_IS_ARM && !ARCHITECTURE_IS_ARM64 */

boolean mzx_architecture_has_neon(void)
{
#ifdef ARCHITECTURE_IS_ARM64
  return true;

#elif defined(ARCHITECTURE_IS_ARM)
  static boolean checked = false;
  static boolean has_neon = false;

  if(!checked)
  {
    has_neon = has_neon_check();
    checked = true;
  }
  return has_neon;

#else
  return false;
#endif
}

boolean mzx_architecture_has_sve(void)
{
  return false;
}


#if defined(ARCHITECTURE_IS_RISCV) && defined(__linux__)
#include <sys/auxv.h>
// The position of this is standard but none of the headers seem to agree
// on the define name or on which extensions actually get defines.
#define MZX_HWCAP_ISA_V (1ul << ('V'-'A'))

boolean mzx_architecture_has_rvv(void)
{
  int tmp = getauxval(AT_HWCAP);
  return !!(tmp & MZX_HWCAP_ISA_V);
}
#else
boolean mzx_architecture_has_rvv(void)
{
  return false;
}
#endif


#ifdef ARCHITECTURE_IS_PPC
#if defined(__linux__)
#include <sys/auxv.h>
#include <asm/cputable.h>
static int has_altivec_check(void)
{
  unsigned long tmp = getauxval(AT_HWCAP);
  return !!(tmp & PPC_FEATURE_HAS_ALTIVEC) + !!(tmp & PPC_FEATURE_HAS_VSX);
}

#elif defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
/* https://developer.apple.com/library/archive/documentation/Performance/Conceptual/CodeSpeed/Articles/HardwareTuning.html */
static int has_altivec_check(void)
{
#if defined(__APPLE__)
  int mib[2] = { CTL_HW, HW_VECTORUNIT };
#else
  int mib[2] = { CTL_MACHDEP, CPU_ALTIVEC };
#endif
  int has_ve = 0;

  if(sysctl(mib, MZX_ARRAY_SIZE(mib), &has_ve, sizeof(has_ve), NULL, 0) < 0)
    return 0;

  return has_ve != 0;
}

#else
#define has_altivec_check() false
#endif
#endif

boolean mzx_architecture_has_altivec(void)
{
#ifdef ARCHITECTURE_IS_PPC
  static boolean checked = false;
  static boolean has_altivec = false;

  if(!checked)
  {
    has_altivec = has_altivec_check() >= 1;
    checked = true;
  }
  return has_altivec;
#else
  return false;
#endif
}

boolean mzx_architecture_has_altivec_vsx(void)
{
#ifdef ARCHITECTURE_IS_PPC
  static boolean checked = false;
  static boolean has_vsx = false;

  if(!checked)
  {
    has_vsx = has_altivec_check() >= 2;
    checked = true;
  }
  return has_vsx;
#else
  return false;
#endif
}


#if defined(ARCHITECTURE_IS_LOONGARCH)
#if defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
static boolean has_hwcap(unsigned long hwcap)
{
  unsigned long ret = getauxval(AT_HWCAP);
  return !!(ret & hwcap);
}
#define has_lsx_check() has_hwcap(HWCAP_LOONGARCH_LSX)
#define has_lasx_check() has_hwcap(HWCAP_LOONGARCH_LASX)

#else
#define has_lsx_check() false
#define has_lasx_check() false
#endif
#endif /* ARCHITECTURE_IS_LOONGARCH */

boolean mzx_architecture_has_lsx(void)
{
#ifdef ARCHITECTURE_IS_LOONGARCH
  static boolean checked = false;
  static boolean has_lsx = false;

  if(!checked)
  {
    has_lsx = has_lsx_check();
    checked = true;
  }
  return has_lsx;
#else
  return false;
#endif
}

boolean mzx_architecture_has_lasx(void)
{
#ifdef ARCHITECTURE_IS_LOONGARCH
  static boolean checked = false;
  static boolean has_lasx = false;

  if(!checked)
  {
    has_lasx = has_lasx_check();
    checked = true;
  }
  return has_lasx;
#else
  return false;
#endif
}
