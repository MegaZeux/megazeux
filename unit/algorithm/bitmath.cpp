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
#include "../../src/algorithm/bitmath.h"

#include <inttypes.h>

static constexpr uint32_t values32[] =
{
  /* Single bits */
  0x00000001u, 0x00000002u, 0x00000004u, 0x00000008u,
  0x00000010u, 0x00000020u, 0x00000040u, 0x00000080u,
  0x00000100u, 0x00000200u, 0x00000400u, 0x00000800u,
  0x00001000u, 0x00002000u, 0x00004000u, 0x00008000u,
  0x00010000u, 0x00020000u, 0x00040000u, 0x00080000u,
  0x00100000u, 0x00200000u, 0x00400000u, 0x00800000u,
  0x01000000u, 0x02000000u, 0x04000000u, 0x08000000u,
  0x10000000u, 0x20000000u, 0x40000000u, 0x80000000u,

  /* All left */
  0xffffffffu, 0xfffffffeu, 0xfffffffcu, 0xfffffff8u,
  0xfffffff0u, 0xffffffe0u, 0xffffffc0u, 0xffffff80u,
  0xffffff00u, 0xfffffe00u, 0xfffffc00u, 0xfffff800u,
  0xfffff000u, 0xffffe000u, 0xffffc000u, 0xffff8000u,
  0xffff0000u, 0xfffe0000u, 0xfffc0000u, 0xfff80000u,
  0xfff00000u, 0xffe00000u, 0xffc00000u, 0xff800000u,
  0xff000000u, 0xfe000000u, 0xfc000000u, 0xf8000000u,
  0xf0000000u, 0xe0000000u, 0xc0000000u, 0x80000000u,

  /* All right */
  0xffffffffu, 0x7fffffffu, 0x3fffffffu, 0x1fffffffu,
  0x0fffffffu, 0x07ffffffu, 0x03ffffffu, 0x01ffffffu,
  0x00ffffffu, 0x007fffffu, 0x003fffffu, 0x001fffffu,
  0x000fffffu, 0x0007ffffu, 0x0003ffffu, 0x0001ffffu,
  0x0000ffffu, 0x00007fffu, 0x00003fffu, 0x00001fffu,
  0x00000fffu, 0x000007ffu, 0x000003ffu, 0x000001ffu,
  0x000000ffu, 0x0000007fu, 0x0000003fu, 0x0000001fu,
  0x0000000fu, 0x00000007u, 0x00000003u, 0x00000001u,
};

static constexpr uint64_t values64[] =
{
  /* Single bits */
  0x0000000000000001u, 0x0000000000000002u, 0x0000000000000004u, 0x0000000000000008u,
  0x0000000000000010u, 0x0000000000000020u, 0x0000000000000040u, 0x0000000000000080u,
  0x0000000000000100u, 0x0000000000000200u, 0x0000000000000400u, 0x0000000000000800u,
  0x0000000000001000u, 0x0000000000002000u, 0x0000000000004000u, 0x0000000000008000u,
  0x0000000000010000u, 0x0000000000020000u, 0x0000000000040000u, 0x0000000000080000u,
  0x0000000000100000u, 0x0000000000200000u, 0x0000000000400000u, 0x0000000000800000u,
  0x0000000001000000u, 0x0000000002000000u, 0x0000000004000000u, 0x0000000008000000u,
  0x0000000010000000u, 0x0000000020000000u, 0x0000000040000000u, 0x0000000080000000u,
  0x0000000100000000u, 0x0000000200000000u, 0x0000000400000000u, 0x0000000800000000u,
  0x0000001000000000u, 0x0000002000000000u, 0x0000004000000000u, 0x0000008000000000u,
  0x0000010000000000u, 0x0000020000000000u, 0x0000040000000000u, 0x0000080000000000u,
  0x0000100000000000u, 0x0000200000000000u, 0x0000400000000000u, 0x0000800000000000u,
  0x0001000000000000u, 0x0002000000000000u, 0x0004000000000000u, 0x0008000000000000u,
  0x0010000000000000u, 0x0020000000000000u, 0x0040000000000000u, 0x0080000000000000u,
  0x0100000000000000u, 0x0200000000000000u, 0x0400000000000000u, 0x0800000000000000u,

  /* All left */
  0xffffffffffffffffu, 0xfffffffffffffffeu, 0xfffffffffffffffcu, 0xfffffffffffffff8u,
  0xfffffffffffffff0u, 0xffffffffffffffe0u, 0xffffffffffffffc0u, 0xffffffffffffff80u,
  0xffffffffffffff00u, 0xfffffffffffffe00u, 0xfffffffffffffc00u, 0xfffffffffffff800u,
  0xfffffffffffff000u, 0xffffffffffffe000u, 0xffffffffffffc000u, 0xffffffffffff8000u,
  0xffffffffffff0000u, 0xfffffffffffe0000u, 0xfffffffffffc0000u, 0xfffffffffff80000u,
  0xfffffffffff00000u, 0xffffffffffe00000u, 0xffffffffffc00000u, 0xffffffffff800000u,
  0xffffffffff000000u, 0xfffffffffe000000u, 0xfffffffffc000000u, 0xfffffffff8000000u,
  0xfffffffff0000000u, 0xffffffffe0000000u, 0xffffffffc0000000u, 0xffffffff80000000u,
  0xffffffff00000000u, 0xfffffffe00000000u, 0xfffffffc00000000u, 0xfffffff800000000u,
  0xfffffff000000000u, 0xffffffe000000000u, 0xffffffc000000000u, 0xffffff8000000000u,
  0xffffff0000000000u, 0xfffffe0000000000u, 0xfffffc0000000000u, 0xfffff80000000000u,
  0xfffff00000000000u, 0xffffe00000000000u, 0xffffc00000000000u, 0xffff800000000000u,
  0xffff000000000000u, 0xfffe000000000000u, 0xfffc000000000000u, 0xfff8000000000000u,
  0xfff0000000000000u, 0xffe0000000000000u, 0xffc0000000000000u, 0xff80000000000000u,
  0xff00000000000000u, 0xfe00000000000000u, 0xfc00000000000000u, 0xf800000000000000u,
  0xf000000000000000u, 0xe000000000000000u, 0xc000000000000000u, 0x8000000000000000u,

  /* All right */
  0xffffffffffffffffu, 0x7fffffffffffffffu, 0x3fffffffffffffffu, 0x1fffffffffffffffu,
  0x0fffffffffffffffu, 0x07ffffffffffffffu, 0x03ffffffffffffffu, 0x01ffffffffffffffu,
  0x00ffffffffffffffu, 0x007fffffffffffffu, 0x003fffffffffffffu, 0x001fffffffffffffu,
  0x000fffffffffffffu, 0x0007ffffffffffffu, 0x0003ffffffffffffu, 0x0001ffffffffffffu,
  0x0000ffffffffffffu, 0x00007fffffffffffu, 0x00003fffffffffffu, 0x00001fffffffffffu,
  0x00000fffffffffffu, 0x000007ffffffffffu, 0x000003ffffffffffu, 0x000001ffffffffffu,
  0x000000ffffffffffu, 0x0000007fffffffffu, 0x0000003fffffffffu, 0x0000001fffffffffu,
  0x0000000fffffffffu, 0x00000007ffffffffu, 0x00000003ffffffffu, 0x00000001ffffffffu,
  0x00000000ffffffffu, 0x000000007fffffffu, 0x000000003fffffffu, 0x000000001fffffffu,
  0x000000000fffffffu, 0x0000000007ffffffu, 0x0000000003ffffffu, 0x0000000001ffffffu,
  0x0000000000ffffffu, 0x00000000007fffffu, 0x00000000003fffffu, 0x00000000001fffffu,
  0x00000000000fffffu, 0x000000000007ffffu, 0x000000000003ffffu, 0x000000000001ffffu,
  0x000000000000ffffu, 0x0000000000007fffu, 0x0000000000003fffu, 0x0000000000001fffu,
  0x0000000000000fffu, 0x00000000000007ffu, 0x00000000000003ffu, 0x00000000000001ffu,
  0x00000000000000ffu, 0x000000000000007fu, 0x000000000000003fu, 0x000000000000001fu,
  0x000000000000000fu, 0x0000000000000007u, 0x0000000000000003u, 0x0000000000000001u,
};

template<typename T, typename U>
static inline T naive_bit_cast(U x)
{
  static_assert(sizeof(T) == sizeof(U), "");
  T y;
  memcpy(&y, &x, sizeof(y));
  return y;
}

static int naive_clz(uint32_t value)
{
  int count = 0;
  for(uint32_t i = 0x80000000u; i; i >>= 1, count++)
    if(value & i)
      break;
  return count;
}

static int naive_clz(uint64_t value)
{
  int count = 0;
  for(uint64_t i = 0x8000000000000000ull; i; i >>= 1, count++)
    if(value & i)
      break;
  return count;
}

static int naive_ctz(uint32_t value)
{
  int count = 0;
  for(uint32_t i = 1; i; i <<= 1, count++)
    if(value & i)
      break;
  return count;
}

static int naive_ctz(uint64_t value)
{
  int count = 0;
  for(uint64_t i = 1; i; i <<= 1, count++)
    if(value & i)
      break;
  return count;
}

static uint32_t naive_round_to_power_of_two(uint32_t value)
{
  uint32_t n;
  for(n = 1u; n; n <<= 1)
    if(n >= value)
      break;
  return n;
}

template<typename T, typename U, typename V>
static void bit_cast_test(const V &d, T bit_cast_fn(U))
{
  U src = naive_bit_cast<U>(d);
  T dest = bit_cast_fn(src);
  ASSERTMEM(&dest, &d, sizeof(T), "");
  /* round trip */
  V chk = naive_bit_cast<V>(dest);
  ASSERTEQ(chk, d, "");
}

UNITTEST(mzx_bit_cast)
{
  SECTION(i32_to_u32)
    test_data_set(bit_cast_test<uint32_t, int32_t>, values32, mzx_bit_cast_i32_to_u32);

  SECTION(u32_to_i32)
    test_data_set(bit_cast_test<int32_t, uint32_t>, values32, mzx_bit_cast_u32_to_i32);

  SECTION(i64_to_u64)
    test_data_set(bit_cast_test<uint64_t, int64_t>, values64, mzx_bit_cast_i64_to_u64);

  SECTION(u64_to_i64)
    test_data_set(bit_cast_test<int64_t, uint64_t>, values64, mzx_bit_cast_u64_to_i64);

  SECTION(f64_to_i64)
    test_data_set(bit_cast_test<int64_t, double>, values64, mzx_bit_cast_f64_to_i64);

  SECTION(f64_to_u64)
    test_data_set(bit_cast_test<uint64_t, double>, values64, mzx_bit_cast_f64_to_u64);

  SECTION(i64_to_f64)
    test_data_set(bit_cast_test<double, int64_t>, values64, mzx_bit_cast_i64_to_f64);

  SECTION(u64_to_f64)
    test_data_set(bit_cast_test<double, uint64_t>, values64, mzx_bit_cast_u64_to_f64);
}

UNITTEST(mzx_clz32)
{
  SECTION(mzx_clz32)
  {
    for(uint32_t v : values32)
      ASSERTEQ(mzx_clz32(v), naive_clz(v), "%08" PRIx32, v);
  }

  SECTION(mzx_soft_clz32)
  {
    for(uint32_t v : values32)
      ASSERTEQ(mzx_soft_clz32(v), naive_clz(v), "%08" PRIx32, v);
  }

#if ARCHITECTURE_BITS < 64
  SECTION(arch_uint_clz)
  {
    for(uint32_t v : values32)
      ASSERTEQ(arch_uint_clz(v), naive_clz(v), "%08" PRIx32, v);
  }
#endif
}

UNITTEST(mzx_clz64)
{
  SECTION(mzx_clz64)
  {
    for(uint64_t v : values64)
      ASSERTEQ(mzx_clz64(v), naive_clz(v), "%016" PRIx64, v);
  }

  SECTION(mzx_soft_clz64)
  {
    for(uint64_t v : values64)
      ASSERTEQ(mzx_soft_clz64(v), naive_clz(v), "%016" PRIx64, v);
  }

#if ARCHITECTURE_BITS >= 64
  SECTION(arch_uint_clz)
  {
    for(uint64_t v : values64)
      ASSERTEQ(arch_uint_clz(v), naive_clz(v), "%016" PRIx64, v);
  }
#endif
}

UNITTEST(mzx_ctz32)
{
  SECTION(mzx_ctz32)
  {
    for(uint32_t v : values32)
      ASSERTEQ(mzx_ctz32(v), naive_ctz(v), "%08" PRIx32, v);
  }

  SECTION(mzx_soft_ctz32)
  {
    for(uint32_t v : values32)
      ASSERTEQ(mzx_soft_ctz32(v), naive_ctz(v), "%08" PRIx32, v);
  }

#if ARCHITECTURE_BITS < 64
  SECTION(arch_uint_ctz)
  {
    for(uint32_t v : values32)
      ASSERTEQ(arch_uint_ctz(v), naive_ctz(v), "%08" PRIx32, v);
  }
#endif
}

UNITTEST(mzx_ctz64)
{
  SECTION(mzx_ctz64)
  {
    for(uint64_t v : values64)
      ASSERTEQ(mzx_ctz64(v), naive_ctz(v), "%016" PRIx64, v);
  }

  SECTION(mzx_soft_ctz64)
  {
    for(uint64_t v : values64)
      ASSERTEQ(mzx_soft_ctz64(v), naive_ctz(v), "%016" PRIx64, v);
  }

#if ARCHITECTURE_BITS >= 64
  SECTION(arch_uint_ctz)
  {
    for(uint64_t v : values64)
      ASSERTEQ(arch_uint_ctz(v), naive_ctz(v), "%016" PRIx64, v);
  }
#endif
}

UNITTEST(mzx_round_to_power_of_two)
{
  ASSERTEQ(mzx_round_to_power_of_two(0), 1, "");
  for(uint32_t v : values32)
    ASSERTEQ(mzx_round_to_power_of_two(v), naive_round_to_power_of_two(v), "%08" PRIx32, v);
}
