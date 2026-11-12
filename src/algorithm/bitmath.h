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

#ifndef MEGAZEUX_ALGORITHM_BITMATH_H
#define MEGAZEUX_ALGORITHM_BITMATH_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "arch.h"

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

/* Note: these macros/functions should never be used in the case of x = 0. */
#if (defined(__GNUC__) && !defined(__clang__) && \
 __GNUC__ * 100 + __GNUC_MINOR__ >= 304)
#define mzx_clz32(x) __builtin_clz(x)
#define mzx_clz64(x) __builtin_clzll(x)
#define mzx_ctz32(x) __builtin_ctz(x)
#define mzx_ctz64(x) __builtin_ctzll(x)

#elif defined(__clang__) /* clang 5+ */
#if __has_builtin(__builtin_clz)
#define mzx_clz32(x) __builtin_clz(x)
#endif
#if __has_builtin(__builtin_clzll)
#define mzx_clz64(x) __builtin_clzll(x)
#endif
#if __has_builtin(__builtin_ctz)
#define mzx_ctz32(x) __builtin_ctz(x)
#endif
#if __has_builtin(__builtin_ctzll)
#define mzx_ctz64(x) __builtin_ctzll(x)
#endif
#endif /* clang */

static inline int mzx_soft_clz32(uint32_t x)
{
  static const uint8_t tbl[32] =
  {
    0x1f, 0x16, 0x1e, 0x15, 0x12, 0x0a, 0x1d, 0x02,
    0x14, 0x11, 0x0f, 0x0d, 0x09, 0x06, 0x1c, 0x01,
    0x17, 0x13, 0x0b, 0x03, 0x10, 0x0e, 0x07, 0x18,
    0x0c, 0x04, 0x08, 0x19, 0x05, 0x1a, 0x1b, 0x00
  };
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return tbl[(uint32_t)(x * 0x07c4acddu) >> 27];
}
#ifndef mzx_clz32
#define mzx_clz32(x) mzx_soft_clz32(x)
#endif

static inline int mzx_soft_clz64(uint64_t x)
{
  return (x > 0xffffffffu) ? mzx_soft_clz32(x >> 32) : mzx_soft_clz32(x) + 32;
}
#ifndef mzx_clz64
#define mzx_clz64(x) mzx_soft_clz64(x)
#endif

static inline int mzx_soft_ctz32(uint32_t x)
{
  static const uint8_t tbl[32] =
  {
    0x00, 0x01, 0x02, 0x06, 0x03, 0x0b, 0x07, 0x10,
    0x04, 0x0e, 0x0c, 0x15, 0x08, 0x17, 0x11, 0x1a,
    0x1f, 0x05, 0x0a, 0x0f, 0x0d, 0x14, 0x16, 0x19,
    0x1e, 0x09, 0x13, 0x18, 0x1d, 0x12, 0x1c, 0x1b
  };
  return tbl[(uint32_t)((x & -x) * 0x04653adfu) >> 27];
}
#ifndef mzx_ctz32
#define mzx_ctz32(x) mzx_soft_ctz32(x)
#endif

static inline int mzx_soft_ctz64(uint64_t x)
{
  return (x & 0xffffffffu) ? mzx_soft_ctz32(x) : mzx_soft_ctz32(x >> 32) + 32;
}
#ifndef mzx_ctz64
#define mzx_ctz64(x) mzx_soft_ctz64(x)
#endif

#if ARCHITECTURE_BITS >= 64
#define arch_uint_clz(x) mzx_clz64(x)
#define arch_uint_ctz(x) mzx_ctz64(x)
#else
#define arch_uint_clz(x) mzx_clz32(x)
#define arch_uint_ctz(x) mzx_ctz32(x)
#endif

static inline uint32_t mzx_round_to_power_of_two(uint32_t value)
{
  value -= (value > 0);
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  return value + 1;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_BITMATH_H */
