/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __MEMCASECMP_H
#define __MEMCASECMP_H

#include "compat.h"

__M_BEGIN_DECLS

#include <assert.h>
#include <inttypes.h>
#include "platform/platform_endian.h"

/**
 * Minimum string length before aligned comparison should be invoked.
 */
#define _memcasecmp32_size (sizeof(uint32_t))
#define _memcasecmp32_threshold (_memcasecmp32_size * 2)

#if ARCHITECTURE_BITS >= 64
#define _memcasecmp64_size (sizeof(uint64_t))
#define _memcasecmp64_threshold (_memcasecmp64_size * 2)
#endif

/**
 * Macros to get byte b of unsigned int x.
 */
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
#define _memcaseget32(x,b) memtolower((x >> (b * 8)) & 0xFF)
#define _memcaseget64(x,b) _memcaseget32(x,b)
#else /* PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN */
#define _memcaseget32(x,b) memtolower((x >> (24 - (b * 8))) & 0xFF)
#define _memcaseget64(x,b) memtolower((x >> (56 - (b * 8))) & 0xFF)
#endif

static const unsigned char memtolower_table[256] =
{
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,  91,  92,  93,  94,  95,
  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

/**
 * Convert ASCII uppercase characters (65-90) to lowercase characters (97-122)
 * using a lookup table. This is faster than calling glibc's toupper().
 */
static inline int memtolower(unsigned char c)
{
  return memtolower_table[c];
}

#if ARCHITECTURE_BITS >= 64

static inline int _memcasecmp64(const uint8_t *a_value, const uint8_t *b_value,
 size_t cmp_length, size_t *position)
{
  const uint64_t *a_value_64b = (const uint64_t *)a_value;
  const uint64_t *b_value_64b = (const uint64_t *)b_value;
  size_t length_64b = cmp_length / _memcasecmp64_size;
  uint64_t val_a;
  uint64_t val_b;
  size_t i;
  int cmp;

  for(i = 0; i < length_64b; i++)
  {
    val_a = a_value_64b[i];
    val_b = b_value_64b[i];

    if(val_a != val_b)
    {
      cmp = _memcaseget64(val_a, 0) - _memcaseget64(val_b, 0);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 1) - _memcaseget64(val_b, 1);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 2) - _memcaseget64(val_b, 2);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 3) - _memcaseget64(val_b, 3);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 4) - _memcaseget64(val_b, 4);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 5) - _memcaseget64(val_b, 5);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 6) - _memcaseget64(val_b, 6);
      if(cmp)
        return cmp;

      cmp = _memcaseget64(val_a, 7) - _memcaseget64(val_b, 7);
      if(cmp)
        return cmp;
    }
  }

  *position = length_64b * _memcasecmp64_size;
  return 0;
}

#endif

static inline int _memcasecmp32(const uint8_t *a_value, const uint8_t *b_value,
 size_t cmp_length, size_t *position)
{
  const uint32_t *a_value_32b = (const uint32_t *)a_value;
  const uint32_t *b_value_32b = (const uint32_t *)b_value;
  size_t length_32b = cmp_length / _memcasecmp32_size;
  uint32_t val_a;
  uint32_t val_b;
  size_t i;
  int cmp;

  for(i = 0; i < length_32b; i++)
  {
    val_a = a_value_32b[i];
    val_b = b_value_32b[i];

    if(val_a != val_b)
    {
      cmp = _memcaseget32(val_a, 0) - _memcaseget32(val_b, 0);
      if(cmp)
        return cmp;

      cmp = _memcaseget32(val_a, 1) - _memcaseget32(val_b, 1);
      if(cmp)
        return cmp;

      cmp = _memcaseget32(val_a, 2) - _memcaseget32(val_b, 2);
      if(cmp)
        return cmp;

      cmp = _memcaseget32(val_a, 3) - _memcaseget32(val_b, 3);
      if(cmp)
        return cmp;
    }
  }

  *position = length_32b * _memcasecmp32_size;
  return 0;
}

/**
 * Compare two strings ignoring case for ASCII chars (65-90, 97-122).
 * For best results, both strings should be 4 aligned (32-bit platforms) or
 * 8 aligned (64-bit platforms), but this function is guaranteed to work
 * with input strings of any alignment.
 *
 * @param  A            A non-terminated string.
 * @param  B            A non-terminated string.
 * @param  cmp_length   The length to compare.
 * @return              0 if A==B; >0 if A>B; <0 if A<B.
 */
static inline int memcasecmp(const void *A, const void *B, size_t cmp_length)
{
  const uint8_t *a_value = (const uint8_t *)A;
  const uint8_t *b_value = (const uint8_t *)B;
  size_t i = 0;
  size_t a_align;
  size_t b_align;
  int cmp;

#if ARCHITECTURE_BITS >= 64
  /**
   * Attempt an 8-aligned compare first. If both strings aren't of the same
   * alignment or the string is too short for this to be worth it, attempt a
   * 4-aligned compare instead.
   *
   * TODO: glibc memcpy uses a technique where it aligns the first input and
   * then uses a slower alternate function if the second input isn't also
   * aligned, which might be worth looking into.
   */
  if(cmp_length >= _memcasecmp64_threshold)
  {
    a_align = (size_t)(a_value) % ALIGN_64_MODULO;
    b_align = (size_t)(b_value) % ALIGN_64_MODULO;

    if(a_align == b_align)
    {
      while((size_t)a_value % ALIGN_64_MODULO)
      {
        cmp = memtolower(*(a_value++)) - memtolower(*(b_value++));
        if(cmp)
          return cmp;
        cmp_length--;
      }

      cmp = _memcasecmp64(a_value, b_value, cmp_length, &i);
      if(cmp)
        return cmp;
    }
    else
      goto try_32;
  }
  else
#endif

  /**
   * Attempt a 4-aligned compare. If both strings aren't of the same alignment
   * or the string is too short for this to be worth it, fall back to the slow
   * compare loop instead.
   */
  if(cmp_length >= _memcasecmp32_threshold)
  {
#if ARCHITECTURE_BITS >= 64
try_32:
#endif
    a_align = (size_t)(a_value) % ALIGN_32_MODULO;
    b_align = (size_t)(b_value) % ALIGN_32_MODULO;

    if(a_align == b_align)
    {
      while((size_t)a_value % ALIGN_32_MODULO)
      {
        cmp = memtolower(*(a_value++)) - memtolower(*(b_value++));
        if(cmp)
          return cmp;
        cmp_length--;
      }

      cmp = _memcasecmp32(a_value, b_value, cmp_length, &i);
      if(cmp)
        return cmp;
    }
  }

  /**
   * Either the aligned compares could not be used or there are a few bytes
   * left to compare. Finish comparing the string byte-by-byte...
   */
  for(; i < cmp_length; i++)
  {
    cmp = memtolower(a_value[i]) - memtolower(b_value[i]);
    if(cmp)
      return cmp;
  }

  return 0;
}

/**
 * Compare two strings known to be aligned to 4 bytes, ignoring case for
 * ASCII chars (65-90, 97-122). This is useful primarily for counter and
 * string name comparisons, which are always aligned to 4 bytes and are
 * typically too short to benefit from the 64-bit compare conditionally
 * used in the general case function.
 *
 * @param  A            A non-terminated string aligned to 4 bytes.
 * @param  B            A non-terminated string aligned to 4 bytes.
 * @param  cmp_length   The length to compare.
 * @return              0 if A==B; >0 if A>B; <0 if A<B.
 */
static inline int memcasecmp32(const void *A, const void *B, size_t cmp_length)
{
  const uint8_t *a_value = (const uint8_t *)A;
  const uint8_t *b_value = (const uint8_t *)B;
  size_t i = 0;
  int cmp;

  // The 4 alignment apparently can't be guaranteed 100% of the time, so just
  // fall back to the regular compare if there's an issue.
  if((size_t)A % ALIGN_32_MODULO || (size_t)B % ALIGN_32_MODULO)
    return memcasecmp(A, B, cmp_length);

  cmp = _memcasecmp32(a_value, b_value, cmp_length, &i);
  if(cmp)
    return cmp;

  for(; i < cmp_length; i++)
  {
    cmp = memtolower(a_value[i]) - memtolower(b_value[i]);
    if(cmp)
      return cmp;
  }
  return 0;
}

__M_END_DECLS

#endif /* __MEMCASECMP_H */
