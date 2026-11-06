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

#include "string_char.h"
#include "string_compare.h"
#include "../platform/platform_endian.h"

#include <assert.h>
#include <stdint.h>

#define CMP(x, y) do { \
  int cmp = (int)mzx_tolower_table[(uint8_t)(x)] - \
            (int)mzx_tolower_table[(uint8_t)(y)]; \
  if(cmp) \
    return cmp; \
} while(0)

int mzx_strcasecmp(const char *a, const char *b)
{
  for(;; a++, b++)
  {
    CMP(*a, *b);
    if(*a == '\0')
      break;
  }
  return 0;
}

int mzx_strncasecmp(const char *a, const char *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++, a++, b++)
  {
    CMP(*a, *b);
    if(*a == '\0')
      break;
  }
  return 0;
}

#if ARCHITECTURE_BITS >= 64
static inline int cmp64(const uint64_t *a64, const uint64_t *b64)
{
  if(*a64 != *b64)
  {
    const uint8_t *a8 = (const uint8_t *)a64;
    const uint8_t *b8 = (const uint8_t *)b64;
    CMP(a8[0], b8[0]);
    CMP(a8[1], b8[1]);
    CMP(a8[2], b8[2]);
    CMP(a8[3], b8[3]);
    CMP(a8[4], b8[4]);
    CMP(a8[5], b8[5]);
    CMP(a8[6], b8[6]);
    CMP(a8[7], b8[7]);
  }
  return 0;
}
#endif

static inline int cmp32(const uint32_t *a32, const uint32_t *b32)
{
  if(*a32 != *b32)
  {
    const uint8_t *a8 = (const uint8_t *)a32;
    const uint8_t *b8 = (const uint8_t *)b32;
    CMP(a8[0], b8[0]);
    CMP(a8[1], b8[1]);
    CMP(a8[2], b8[2]);
    CMP(a8[3], b8[3]);
  }
  return 0;
}

static inline int cmp8_loop(const uint8_t *a8, const uint8_t *b8, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++, a8++, b8++)
    CMP(*a8, *b8);

  return 0;
}

/**
 * Compare two strings ignoring case for ASCII chars (65-90, 97-122).
 * For best results, both strings should be 4 aligned (32-bit platforms) or
 * 8 aligned (64-bit platforms), but this function is guaranteed to work
 * with input strings of any alignment.
 *
 * @param  a            A non-terminated string.
 * @param  b            A non-terminated string.
 * @param  n            The length to compare.
 * @return              0 if a==b; >0 if a>b; <0 if a<b.
 */
int mzx_memcasecmp(const void *a, const void *b, size_t n)
{
  const uint8_t *a8 = (const uint8_t *)a;
  const uint8_t *b8 = (const uint8_t *)b;

  /* Align a */
  size_t align_m = (size_t)a8 % (ARCHITECTURE_BITS >> 3);
  size_t align = (ARCHITECTURE_BITS >> 3) - align_m;

  if(align > n)
    return cmp8_loop(a8, b8, n);

  if(align_m)
  {
    int cmp = cmp8_loop(a8, b8, align);
    if(cmp)
      return cmp;

    a8 += align;
    b8 += align;
    n -= align;
  }

#if ARCHITECTURE_BITS >= 64
  if(n >= 8 && (size_t)b8 % ALIGN_64_MODULO == 0)
  {
    /* Align 64 compare */
    const uint64_t *a64 = (const uint64_t *)a8;
    const uint64_t *b64 = (const uint64_t *)b8;

    while(n >= 8)
    {
      int cmp = cmp64(a64, b64);
      if(cmp)
        return cmp;

      a64++;
      b64++;
      n -= 8;
    }
    a8 = (const uint8_t *)a64;
    b8 = (const uint8_t *)b64;
  }
  else
#endif

  if(n >= 4 && (size_t)b8 % ALIGN_32_MODULO == 0)
  {
    /* Align 32 compare */
    const uint32_t *a32 = (const uint32_t *)a8;
    const uint32_t *b32 = (const uint32_t *)b8;

    while(n >= 4)
    {
      int cmp = cmp32(a32, b32);
      if(cmp)
        return cmp;

      a32++;
      b32++;
      n -= 4;
    }
    a8 = (const uint8_t *)a32;
    b8 = (const uint8_t *)b32;
  }

  /* Remainder */
  return cmp8_loop(a8, b8, n);
}

/**
 * Compare two strings known to be aligned to 4 bytes, ignoring case for
 * ASCII chars (65-90, 97-122). This is useful primarily for counter and
 * string name comparisons, which are always aligned to 4 bytes and are
 * typically too short to benefit from the 64-bit compare conditionally
 * used in the general case function.
 *
 * @param  a            A non-terminated string aligned to 4 bytes.
 * @param  a            A non-terminated string aligned to 4 bytes.
 * @param  n            The length to compare.
 * @return              0 if a==a; >0 if a>b; <0 if a<b.
 */
int mzx_memcasecmp32(const void *a, const void *b, size_t n)
{
  const uint32_t *a32;
  const uint32_t *b32;

  /* Can't always guarantee correct alignment, so fall back in this edge case. */
  if((size_t)a % 4 || (size_t)b % 4)
    return mzx_memcasecmp(a, b, n);

  a32 = (const uint32_t *)a;
  b32 = (const uint32_t *)b;

  while(n >= 4)
  {
    int cmp = cmp32(a32, b32);
    if(cmp)
      return cmp;

    a32++;
    b32++;
    n -= 4;
  }
  return cmp8_loop((const uint8_t *)a32, (const uint8_t *)b32, n);
}
