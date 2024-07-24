/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "graphics.h"
#include "render_layer_common.hpp"

#ifdef DEBUG
#include "util.h"
#endif

// TODO: inline asm
// TODO: maskstore seems inferior to XOR AND XOR, possibly dump AVX2 variant altogether?
#if defined(HAS_RENDER_LAYER32X8_AVX)// && defined(__AVX__)
//#ifdef __AVX__
#include <immintrin.h>
//#else
#define ALIGN32 __attribute__((aligned(32)))
//#endif

union ymm
{
//#ifdef __AVX__
  __m256i vec;
  __m256 vecs;
//#endif
  uint32_t values[8] ALIGN32;
};

//#ifdef __AVX__
#ifdef _MSC_VER
// FIXME
#else
typedef __v8su selector_t;
#endif
//#else
//typedef uint32_t (selector_t)[8] ALIGN32;
//#endif

static const selector_t selectors_mzx[256] =
{
  { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1 },
  { 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 1, 1 },
  { 0, 0, 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 1, 0, 1 },
  { 0, 0, 0, 0, 0, 1, 1, 0 }, { 0, 0, 0, 0, 0, 1, 1, 1 },
  { 0, 0, 0, 0, 1, 0, 0, 0 }, { 0, 0, 0, 0, 1, 0, 0, 1 },
  { 0, 0, 0, 0, 1, 0, 1, 0 }, { 0, 0, 0, 0, 1, 0, 1, 1 },
  { 0, 0, 0, 0, 1, 1, 0, 0 }, { 0, 0, 0, 0, 1, 1, 0, 1 },
  { 0, 0, 0, 0, 1, 1, 1, 0 }, { 0, 0, 0, 0, 1, 1, 1, 1 },
  { 0, 0, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 1, 0, 0, 0, 1 },
  { 0, 0, 0, 1, 0, 0, 1, 0 }, { 0, 0, 0, 1, 0, 0, 1, 1 },
  { 0, 0, 0, 1, 0, 1, 0, 0 }, { 0, 0, 0, 1, 0, 1, 0, 1 },
  { 0, 0, 0, 1, 0, 1, 1, 0 }, { 0, 0, 0, 1, 0, 1, 1, 1 },
  { 0, 0, 0, 1, 1, 0, 0, 0 }, { 0, 0, 0, 1, 1, 0, 0, 1 },
  { 0, 0, 0, 1, 1, 0, 1, 0 }, { 0, 0, 0, 1, 1, 0, 1, 1 },
  { 0, 0, 0, 1, 1, 1, 0, 0 }, { 0, 0, 0, 1, 1, 1, 0, 1 },
  { 0, 0, 0, 1, 1, 1, 1, 0 }, { 0, 0, 0, 1, 1, 1, 1, 1 },
  { 0, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0, 0, 1 },
  { 0, 0, 1, 0, 0, 0, 1, 0 }, { 0, 0, 1, 0, 0, 0, 1, 1 },
  { 0, 0, 1, 0, 0, 1, 0, 0 }, { 0, 0, 1, 0, 0, 1, 0, 1 },
  { 0, 0, 1, 0, 0, 1, 1, 0 }, { 0, 0, 1, 0, 0, 1, 1, 1 },
  { 0, 0, 1, 0, 1, 0, 0, 0 }, { 0, 0, 1, 0, 1, 0, 0, 1 },
  { 0, 0, 1, 0, 1, 0, 1, 0 }, { 0, 0, 1, 0, 1, 0, 1, 1 },
  { 0, 0, 1, 0, 1, 1, 0, 0 }, { 0, 0, 1, 0, 1, 1, 0, 1 },
  { 0, 0, 1, 0, 1, 1, 1, 0 }, { 0, 0, 1, 0, 1, 1, 1, 1 },
  { 0, 0, 1, 1, 0, 0, 0, 0 }, { 0, 0, 1, 1, 0, 0, 0, 1 },
  { 0, 0, 1, 1, 0, 0, 1, 0 }, { 0, 0, 1, 1, 0, 0, 1, 1 },
  { 0, 0, 1, 1, 0, 1, 0, 0 }, { 0, 0, 1, 1, 0, 1, 0, 1 },
  { 0, 0, 1, 1, 0, 1, 1, 0 }, { 0, 0, 1, 1, 0, 1, 1, 1 },
  { 0, 0, 1, 1, 1, 0, 0, 0 }, { 0, 0, 1, 1, 1, 0, 0, 1 },
  { 0, 0, 1, 1, 1, 0, 1, 0 }, { 0, 0, 1, 1, 1, 0, 1, 1 },
  { 0, 0, 1, 1, 1, 1, 0, 0 }, { 0, 0, 1, 1, 1, 1, 0, 1 },
  { 0, 0, 1, 1, 1, 1, 1, 0 }, { 0, 0, 1, 1, 1, 1, 1, 1 },
  { 0, 1, 0, 0, 0, 0, 0, 0 }, { 0, 1, 0, 0, 0, 0, 0, 1 },
  { 0, 1, 0, 0, 0, 0, 1, 0 }, { 0, 1, 0, 0, 0, 0, 1, 1 },
  { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 1 },
  { 0, 1, 0, 0, 0, 1, 1, 0 }, { 0, 1, 0, 0, 0, 1, 1, 1 },
  { 0, 1, 0, 0, 1, 0, 0, 0 }, { 0, 1, 0, 0, 1, 0, 0, 1 },
  { 0, 1, 0, 0, 1, 0, 1, 0 }, { 0, 1, 0, 0, 1, 0, 1, 1 },
  { 0, 1, 0, 0, 1, 1, 0, 0 }, { 0, 1, 0, 0, 1, 1, 0, 1 },
  { 0, 1, 0, 0, 1, 1, 1, 0 }, { 0, 1, 0, 0, 1, 1, 1, 1 },
  { 0, 1, 0, 1, 0, 0, 0, 0 }, { 0, 1, 0, 1, 0, 0, 0, 1 },
  { 0, 1, 0, 1, 0, 0, 1, 0 }, { 0, 1, 0, 1, 0, 0, 1, 1 },
  { 0, 1, 0, 1, 0, 1, 0, 0 }, { 0, 1, 0, 1, 0, 1, 0, 1 },
  { 0, 1, 0, 1, 0, 1, 1, 0 }, { 0, 1, 0, 1, 0, 1, 1, 1 },
  { 0, 1, 0, 1, 1, 0, 0, 0 }, { 0, 1, 0, 1, 1, 0, 0, 1 },
  { 0, 1, 0, 1, 1, 0, 1, 0 }, { 0, 1, 0, 1, 1, 0, 1, 1 },
  { 0, 1, 0, 1, 1, 1, 0, 0 }, { 0, 1, 0, 1, 1, 1, 0, 1 },
  { 0, 1, 0, 1, 1, 1, 1, 0 }, { 0, 1, 0, 1, 1, 1, 1, 1 },
  { 0, 1, 1, 0, 0, 0, 0, 0 }, { 0, 1, 1, 0, 0, 0, 0, 1 },
  { 0, 1, 1, 0, 0, 0, 1, 0 }, { 0, 1, 1, 0, 0, 0, 1, 1 },
  { 0, 1, 1, 0, 0, 1, 0, 0 }, { 0, 1, 1, 0, 0, 1, 0, 1 },
  { 0, 1, 1, 0, 0, 1, 1, 0 }, { 0, 1, 1, 0, 0, 1, 1, 1 },
  { 0, 1, 1, 0, 1, 0, 0, 0 }, { 0, 1, 1, 0, 1, 0, 0, 1 },
  { 0, 1, 1, 0, 1, 0, 1, 0 }, { 0, 1, 1, 0, 1, 0, 1, 1 },
  { 0, 1, 1, 0, 1, 1, 0, 0 }, { 0, 1, 1, 0, 1, 1, 0, 1 },
  { 0, 1, 1, 0, 1, 1, 1, 0 }, { 0, 1, 1, 0, 1, 1, 1, 1 },
  { 0, 1, 1, 1, 0, 0, 0, 0 }, { 0, 1, 1, 1, 0, 0, 0, 1 },
  { 0, 1, 1, 1, 0, 0, 1, 0 }, { 0, 1, 1, 1, 0, 0, 1, 1 },
  { 0, 1, 1, 1, 0, 1, 0, 0 }, { 0, 1, 1, 1, 0, 1, 0, 1 },
  { 0, 1, 1, 1, 0, 1, 1, 0 }, { 0, 1, 1, 1, 0, 1, 1, 1 },
  { 0, 1, 1, 1, 1, 0, 0, 0 }, { 0, 1, 1, 1, 1, 0, 0, 1 },
  { 0, 1, 1, 1, 1, 0, 1, 0 }, { 0, 1, 1, 1, 1, 0, 1, 1 },
  { 0, 1, 1, 1, 1, 1, 0, 0 }, { 0, 1, 1, 1, 1, 1, 0, 1 },
  { 0, 1, 1, 1, 1, 1, 1, 0 }, { 0, 1, 1, 1, 1, 1, 1, 1 },
  { 1, 0, 0, 0, 0, 0, 0, 0 }, { 1, 0, 0, 0, 0, 0, 0, 1 },
  { 1, 0, 0, 0, 0, 0, 1, 0 }, { 1, 0, 0, 0, 0, 0, 1, 1 },
  { 1, 0, 0, 0, 0, 1, 0, 0 }, { 1, 0, 0, 0, 0, 1, 0, 1 },
  { 1, 0, 0, 0, 0, 1, 1, 0 }, { 1, 0, 0, 0, 0, 1, 1, 1 },
  { 1, 0, 0, 0, 1, 0, 0, 0 }, { 1, 0, 0, 0, 1, 0, 0, 1 },
  { 1, 0, 0, 0, 1, 0, 1, 0 }, { 1, 0, 0, 0, 1, 0, 1, 1 },
  { 1, 0, 0, 0, 1, 1, 0, 0 }, { 1, 0, 0, 0, 1, 1, 0, 1 },
  { 1, 0, 0, 0, 1, 1, 1, 0 }, { 1, 0, 0, 0, 1, 1, 1, 1 },
  { 1, 0, 0, 1, 0, 0, 0, 0 }, { 1, 0, 0, 1, 0, 0, 0, 1 },
  { 1, 0, 0, 1, 0, 0, 1, 0 }, { 1, 0, 0, 1, 0, 0, 1, 1 },
  { 1, 0, 0, 1, 0, 1, 0, 0 }, { 1, 0, 0, 1, 0, 1, 0, 1 },
  { 1, 0, 0, 1, 0, 1, 1, 0 }, { 1, 0, 0, 1, 0, 1, 1, 1 },
  { 1, 0, 0, 1, 1, 0, 0, 0 }, { 1, 0, 0, 1, 1, 0, 0, 1 },
  { 1, 0, 0, 1, 1, 0, 1, 0 }, { 1, 0, 0, 1, 1, 0, 1, 1 },
  { 1, 0, 0, 1, 1, 1, 0, 0 }, { 1, 0, 0, 1, 1, 1, 0, 1 },
  { 1, 0, 0, 1, 1, 1, 1, 0 }, { 1, 0, 0, 1, 1, 1, 1, 1 },
  { 1, 0, 1, 0, 0, 0, 0, 0 }, { 1, 0, 1, 0, 0, 0, 0, 1 },
  { 1, 0, 1, 0, 0, 0, 1, 0 }, { 1, 0, 1, 0, 0, 0, 1, 1 },
  { 1, 0, 1, 0, 0, 1, 0, 0 }, { 1, 0, 1, 0, 0, 1, 0, 1 },
  { 1, 0, 1, 0, 0, 1, 1, 0 }, { 1, 0, 1, 0, 0, 1, 1, 1 },
  { 1, 0, 1, 0, 1, 0, 0, 0 }, { 1, 0, 1, 0, 1, 0, 0, 1 },
  { 1, 0, 1, 0, 1, 0, 1, 0 }, { 1, 0, 1, 0, 1, 0, 1, 1 },
  { 1, 0, 1, 0, 1, 1, 0, 0 }, { 1, 0, 1, 0, 1, 1, 0, 1 },
  { 1, 0, 1, 0, 1, 1, 1, 0 }, { 1, 0, 1, 0, 1, 1, 1, 1 },
  { 1, 0, 1, 1, 0, 0, 0, 0 }, { 1, 0, 1, 1, 0, 0, 0, 1 },
  { 1, 0, 1, 1, 0, 0, 1, 0 }, { 1, 0, 1, 1, 0, 0, 1, 1 },
  { 1, 0, 1, 1, 0, 1, 0, 0 }, { 1, 0, 1, 1, 0, 1, 0, 1 },
  { 1, 0, 1, 1, 0, 1, 1, 0 }, { 1, 0, 1, 1, 0, 1, 1, 1 },
  { 1, 0, 1, 1, 1, 0, 0, 0 }, { 1, 0, 1, 1, 1, 0, 0, 1 },
  { 1, 0, 1, 1, 1, 0, 1, 0 }, { 1, 0, 1, 1, 1, 0, 1, 1 },
  { 1, 0, 1, 1, 1, 1, 0, 0 }, { 1, 0, 1, 1, 1, 1, 0, 1 },
  { 1, 0, 1, 1, 1, 1, 1, 0 }, { 1, 0, 1, 1, 1, 1, 1, 1 },
  { 1, 1, 0, 0, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0, 0, 1 },
  { 1, 1, 0, 0, 0, 0, 1, 0 }, { 1, 1, 0, 0, 0, 0, 1, 1 },
  { 1, 1, 0, 0, 0, 1, 0, 0 }, { 1, 1, 0, 0, 0, 1, 0, 1 },
  { 1, 1, 0, 0, 0, 1, 1, 0 }, { 1, 1, 0, 0, 0, 1, 1, 1 },
  { 1, 1, 0, 0, 1, 0, 0, 0 }, { 1, 1, 0, 0, 1, 0, 0, 1 },
  { 1, 1, 0, 0, 1, 0, 1, 0 }, { 1, 1, 0, 0, 1, 0, 1, 1 },
  { 1, 1, 0, 0, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1, 0, 1 },
  { 1, 1, 0, 0, 1, 1, 1, 0 }, { 1, 1, 0, 0, 1, 1, 1, 1 },
  { 1, 1, 0, 1, 0, 0, 0, 0 }, { 1, 1, 0, 1, 0, 0, 0, 1 },
  { 1, 1, 0, 1, 0, 0, 1, 0 }, { 1, 1, 0, 1, 0, 0, 1, 1 },
  { 1, 1, 0, 1, 0, 1, 0, 0 }, { 1, 1, 0, 1, 0, 1, 0, 1 },
  { 1, 1, 0, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 0, 1, 1, 1 },
  { 1, 1, 0, 1, 1, 0, 0, 0 }, { 1, 1, 0, 1, 1, 0, 0, 1 },
  { 1, 1, 0, 1, 1, 0, 1, 0 }, { 1, 1, 0, 1, 1, 0, 1, 1 },
  { 1, 1, 0, 1, 1, 1, 0, 0 }, { 1, 1, 0, 1, 1, 1, 0, 1 },
  { 1, 1, 0, 1, 1, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 0, 0, 0, 0, 0 }, { 1, 1, 1, 0, 0, 0, 0, 1 },
  { 1, 1, 1, 0, 0, 0, 1, 0 }, { 1, 1, 1, 0, 0, 0, 1, 1 },
  { 1, 1, 1, 0, 0, 1, 0, 0 }, { 1, 1, 1, 0, 0, 1, 0, 1 },
  { 1, 1, 1, 0, 0, 1, 1, 0 }, { 1, 1, 1, 0, 0, 1, 1, 1 },
  { 1, 1, 1, 0, 1, 0, 0, 0 }, { 1, 1, 1, 0, 1, 0, 0, 1 },
  { 1, 1, 1, 0, 1, 0, 1, 0 }, { 1, 1, 1, 0, 1, 0, 1, 1 },
  { 1, 1, 1, 0, 1, 1, 0, 0 }, { 1, 1, 1, 0, 1, 1, 0, 1 },
  { 1, 1, 1, 0, 1, 1, 1, 0 }, { 1, 1, 1, 0, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 0, 0, 0, 1 },
  { 1, 1, 1, 1, 0, 0, 1, 0 }, { 1, 1, 1, 1, 0, 0, 1, 1 },
  { 1, 1, 1, 1, 0, 1, 0, 0 }, { 1, 1, 1, 1, 0, 1, 0, 1 },
  { 1, 1, 1, 1, 0, 1, 1, 0 }, { 1, 1, 1, 1, 0, 1, 1, 1 },
  { 1, 1, 1, 1, 1, 0, 0, 0 }, { 1, 1, 1, 1, 1, 0, 0, 1 },
  { 1, 1, 1, 1, 1, 0, 1, 0 }, { 1, 1, 1, 1, 1, 0, 1, 1 },
  { 1, 1, 1, 1, 1, 1, 0, 0 }, { 1, 1, 1, 1, 1, 1, 0, 1 },
  { 1, 1, 1, 1, 1, 1, 1, 0 }, { 1, 1, 1, 1, 1, 1, 1, 1 }
};

static const selector_t selectors_smzx[256] =
{
  { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 1, 1 },
  { 0, 0, 0, 0, 0, 0, 2, 2 }, { 0, 0, 0, 0, 0, 0, 3, 3 },
  { 0, 0, 0, 0, 1, 1, 0, 0 }, { 0, 0, 0, 0, 1, 1, 1, 1 },
  { 0, 0, 0, 0, 1, 1, 2, 2 }, { 0, 0, 0, 0, 1, 1, 3, 3 },
  { 0, 0, 0, 0, 2, 2, 0, 0 }, { 0, 0, 0, 0, 2, 2, 1, 1 },
  { 0, 0, 0, 0, 2, 2, 2, 2 }, { 0, 0, 0, 0, 2, 2, 3, 3 },
  { 0, 0, 0, 0, 3, 3, 0, 0 }, { 0, 0, 0, 0, 3, 3, 1, 1 },
  { 0, 0, 0, 0, 3, 3, 2, 2 }, { 0, 0, 0, 0, 3, 3, 3, 3 },
  { 0, 0, 1, 1, 0, 0, 0, 0 }, { 0, 0, 1, 1, 0, 0, 1, 1 },
  { 0, 0, 1, 1, 0, 0, 2, 2 }, { 0, 0, 1, 1, 0, 0, 3, 3 },
  { 0, 0, 1, 1, 1, 1, 0, 0 }, { 0, 0, 1, 1, 1, 1, 1, 1 },
  { 0, 0, 1, 1, 1, 1, 2, 2 }, { 0, 0, 1, 1, 1, 1, 3, 3 },
  { 0, 0, 1, 1, 2, 2, 0, 0 }, { 0, 0, 1, 1, 2, 2, 1, 1 },
  { 0, 0, 1, 1, 2, 2, 2, 2 }, { 0, 0, 1, 1, 2, 2, 3, 3 },
  { 0, 0, 1, 1, 3, 3, 0, 0 }, { 0, 0, 1, 1, 3, 3, 1, 1 },
  { 0, 0, 1, 1, 3, 3, 2, 2 }, { 0, 0, 1, 1, 3, 3, 3, 3 },
  { 0, 0, 2, 2, 0, 0, 0, 0 }, { 0, 0, 2, 2, 0, 0, 1, 1 },
  { 0, 0, 2, 2, 0, 0, 2, 2 }, { 0, 0, 2, 2, 0, 0, 3, 3 },
  { 0, 0, 2, 2, 1, 1, 0, 0 }, { 0, 0, 2, 2, 1, 1, 1, 1 },
  { 0, 0, 2, 2, 1, 1, 2, 2 }, { 0, 0, 2, 2, 1, 1, 3, 3 },
  { 0, 0, 2, 2, 2, 2, 0, 0 }, { 0, 0, 2, 2, 2, 2, 1, 1 },
  { 0, 0, 2, 2, 2, 2, 2, 2 }, { 0, 0, 2, 2, 2, 2, 3, 3 },
  { 0, 0, 2, 2, 3, 3, 0, 0 }, { 0, 0, 2, 2, 3, 3, 1, 1 },
  { 0, 0, 2, 2, 3, 3, 2, 2 }, { 0, 0, 2, 2, 3, 3, 3, 3 },
  { 0, 0, 3, 3, 0, 0, 0, 0 }, { 0, 0, 3, 3, 0, 0, 1, 1 },
  { 0, 0, 3, 3, 0, 0, 2, 2 }, { 0, 0, 3, 3, 0, 0, 3, 3 },
  { 0, 0, 3, 3, 1, 1, 0, 0 }, { 0, 0, 3, 3, 1, 1, 1, 1 },
  { 0, 0, 3, 3, 1, 1, 2, 2 }, { 0, 0, 3, 3, 1, 1, 3, 3 },
  { 0, 0, 3, 3, 2, 2, 0, 0 }, { 0, 0, 3, 3, 2, 2, 1, 1 },
  { 0, 0, 3, 3, 2, 2, 2, 2 }, { 0, 0, 3, 3, 2, 2, 3, 3 },
  { 0, 0, 3, 3, 3, 3, 0, 0 }, { 0, 0, 3, 3, 3, 3, 1, 1 },
  { 0, 0, 3, 3, 3, 3, 2, 2 }, { 0, 0, 3, 3, 3, 3, 3, 3 },
  { 1, 1, 0, 0, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0, 1, 1 },
  { 1, 1, 0, 0, 0, 0, 2, 2 }, { 1, 1, 0, 0, 0, 0, 3, 3 },
  { 1, 1, 0, 0, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1, 1, 1 },
  { 1, 1, 0, 0, 1, 1, 2, 2 }, { 1, 1, 0, 0, 1, 1, 3, 3 },
  { 1, 1, 0, 0, 2, 2, 0, 0 }, { 1, 1, 0, 0, 2, 2, 1, 1 },
  { 1, 1, 0, 0, 2, 2, 2, 2 }, { 1, 1, 0, 0, 2, 2, 3, 3 },
  { 1, 1, 0, 0, 3, 3, 0, 0 }, { 1, 1, 0, 0, 3, 3, 1, 1 },
  { 1, 1, 0, 0, 3, 3, 2, 2 }, { 1, 1, 0, 0, 3, 3, 3, 3 },
  { 1, 1, 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 0, 0, 1, 1 },
  { 1, 1, 1, 1, 0, 0, 2, 2 }, { 1, 1, 1, 1, 0, 0, 3, 3 },
  { 1, 1, 1, 1, 1, 1, 0, 0 }, { 1, 1, 1, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1, 1, 2, 2 }, { 1, 1, 1, 1, 1, 1, 3, 3 },
  { 1, 1, 1, 1, 2, 2, 0, 0 }, { 1, 1, 1, 1, 2, 2, 1, 1 },
  { 1, 1, 1, 1, 2, 2, 2, 2 }, { 1, 1, 1, 1, 2, 2, 3, 3 },
  { 1, 1, 1, 1, 3, 3, 0, 0 }, { 1, 1, 1, 1, 3, 3, 1, 1 },
  { 1, 1, 1, 1, 3, 3, 2, 2 }, { 1, 1, 1, 1, 3, 3, 3, 3 },
  { 1, 1, 2, 2, 0, 0, 0, 0 }, { 1, 1, 2, 2, 0, 0, 1, 1 },
  { 1, 1, 2, 2, 0, 0, 2, 2 }, { 1, 1, 2, 2, 0, 0, 3, 3 },
  { 1, 1, 2, 2, 1, 1, 0, 0 }, { 1, 1, 2, 2, 1, 1, 1, 1 },
  { 1, 1, 2, 2, 1, 1, 2, 2 }, { 1, 1, 2, 2, 1, 1, 3, 3 },
  { 1, 1, 2, 2, 2, 2, 0, 0 }, { 1, 1, 2, 2, 2, 2, 1, 1 },
  { 1, 1, 2, 2, 2, 2, 2, 2 }, { 1, 1, 2, 2, 2, 2, 3, 3 },
  { 1, 1, 2, 2, 3, 3, 0, 0 }, { 1, 1, 2, 2, 3, 3, 1, 1 },
  { 1, 1, 2, 2, 3, 3, 2, 2 }, { 1, 1, 2, 2, 3, 3, 3, 3 },
  { 1, 1, 3, 3, 0, 0, 0, 0 }, { 1, 1, 3, 3, 0, 0, 1, 1 },
  { 1, 1, 3, 3, 0, 0, 2, 2 }, { 1, 1, 3, 3, 0, 0, 3, 3 },
  { 1, 1, 3, 3, 1, 1, 0, 0 }, { 1, 1, 3, 3, 1, 1, 1, 1 },
  { 1, 1, 3, 3, 1, 1, 2, 2 }, { 1, 1, 3, 3, 1, 1, 3, 3 },
  { 1, 1, 3, 3, 2, 2, 0, 0 }, { 1, 1, 3, 3, 2, 2, 1, 1 },
  { 1, 1, 3, 3, 2, 2, 2, 2 }, { 1, 1, 3, 3, 2, 2, 3, 3 },
  { 1, 1, 3, 3, 3, 3, 0, 0 }, { 1, 1, 3, 3, 3, 3, 1, 1 },
  { 1, 1, 3, 3, 3, 3, 2, 2 }, { 1, 1, 3, 3, 3, 3, 3, 3 },
  { 2, 2, 0, 0, 0, 0, 0, 0 }, { 2, 2, 0, 0, 0, 0, 1, 1 },
  { 2, 2, 0, 0, 0, 0, 2, 2 }, { 2, 2, 0, 0, 0, 0, 3, 3 },
  { 2, 2, 0, 0, 1, 1, 0, 0 }, { 2, 2, 0, 0, 1, 1, 1, 1 },
  { 2, 2, 0, 0, 1, 1, 2, 2 }, { 2, 2, 0, 0, 1, 1, 3, 3 },
  { 2, 2, 0, 0, 2, 2, 0, 0 }, { 2, 2, 0, 0, 2, 2, 1, 1 },
  { 2, 2, 0, 0, 2, 2, 2, 2 }, { 2, 2, 0, 0, 2, 2, 3, 3 },
  { 2, 2, 0, 0, 3, 3, 0, 0 }, { 2, 2, 0, 0, 3, 3, 1, 1 },
  { 2, 2, 0, 0, 3, 3, 2, 2 }, { 2, 2, 0, 0, 3, 3, 3, 3 },
  { 2, 2, 1, 1, 0, 0, 0, 0 }, { 2, 2, 1, 1, 0, 0, 1, 1 },
  { 2, 2, 1, 1, 0, 0, 2, 2 }, { 2, 2, 1, 1, 0, 0, 3, 3 },
  { 2, 2, 1, 1, 1, 1, 0, 0 }, { 2, 2, 1, 1, 1, 1, 1, 1 },
  { 2, 2, 1, 1, 1, 1, 2, 2 }, { 2, 2, 1, 1, 1, 1, 3, 3 },
  { 2, 2, 1, 1, 2, 2, 0, 0 }, { 2, 2, 1, 1, 2, 2, 1, 1 },
  { 2, 2, 1, 1, 2, 2, 2, 2 }, { 2, 2, 1, 1, 2, 2, 3, 3 },
  { 2, 2, 1, 1, 3, 3, 0, 0 }, { 2, 2, 1, 1, 3, 3, 1, 1 },
  { 2, 2, 1, 1, 3, 3, 2, 2 }, { 2, 2, 1, 1, 3, 3, 3, 3 },
  { 2, 2, 2, 2, 0, 0, 0, 0 }, { 2, 2, 2, 2, 0, 0, 1, 1 },
  { 2, 2, 2, 2, 0, 0, 2, 2 }, { 2, 2, 2, 2, 0, 0, 3, 3 },
  { 2, 2, 2, 2, 1, 1, 0, 0 }, { 2, 2, 2, 2, 1, 1, 1, 1 },
  { 2, 2, 2, 2, 1, 1, 2, 2 }, { 2, 2, 2, 2, 1, 1, 3, 3 },
  { 2, 2, 2, 2, 2, 2, 0, 0 }, { 2, 2, 2, 2, 2, 2, 1, 1 },
  { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 2, 2, 2, 3, 3 },
  { 2, 2, 2, 2, 3, 3, 0, 0 }, { 2, 2, 2, 2, 3, 3, 1, 1 },
  { 2, 2, 2, 2, 3, 3, 2, 2 }, { 2, 2, 2, 2, 3, 3, 3, 3 },
  { 2, 2, 3, 3, 0, 0, 0, 0 }, { 2, 2, 3, 3, 0, 0, 1, 1 },
  { 2, 2, 3, 3, 0, 0, 2, 2 }, { 2, 2, 3, 3, 0, 0, 3, 3 },
  { 2, 2, 3, 3, 1, 1, 0, 0 }, { 2, 2, 3, 3, 1, 1, 1, 1 },
  { 2, 2, 3, 3, 1, 1, 2, 2 }, { 2, 2, 3, 3, 1, 1, 3, 3 },
  { 2, 2, 3, 3, 2, 2, 0, 0 }, { 2, 2, 3, 3, 2, 2, 1, 1 },
  { 2, 2, 3, 3, 2, 2, 2, 2 }, { 2, 2, 3, 3, 2, 2, 3, 3 },
  { 2, 2, 3, 3, 3, 3, 0, 0 }, { 2, 2, 3, 3, 3, 3, 1, 1 },
  { 2, 2, 3, 3, 3, 3, 2, 2 }, { 2, 2, 3, 3, 3, 3, 3, 3 },
  { 3, 3, 0, 0, 0, 0, 0, 0 }, { 3, 3, 0, 0, 0, 0, 1, 1 },
  { 3, 3, 0, 0, 0, 0, 2, 2 }, { 3, 3, 0, 0, 0, 0, 3, 3 },
  { 3, 3, 0, 0, 1, 1, 0, 0 }, { 3, 3, 0, 0, 1, 1, 1, 1 },
  { 3, 3, 0, 0, 1, 1, 2, 2 }, { 3, 3, 0, 0, 1, 1, 3, 3 },
  { 3, 3, 0, 0, 2, 2, 0, 0 }, { 3, 3, 0, 0, 2, 2, 1, 1 },
  { 3, 3, 0, 0, 2, 2, 2, 2 }, { 3, 3, 0, 0, 2, 2, 3, 3 },
  { 3, 3, 0, 0, 3, 3, 0, 0 }, { 3, 3, 0, 0, 3, 3, 1, 1 },
  { 3, 3, 0, 0, 3, 3, 2, 2 }, { 3, 3, 0, 0, 3, 3, 3, 3 },
  { 3, 3, 1, 1, 0, 0, 0, 0 }, { 3, 3, 1, 1, 0, 0, 1, 1 },
  { 3, 3, 1, 1, 0, 0, 2, 2 }, { 3, 3, 1, 1, 0, 0, 3, 3 },
  { 3, 3, 1, 1, 1, 1, 0, 0 }, { 3, 3, 1, 1, 1, 1, 1, 1 },
  { 3, 3, 1, 1, 1, 1, 2, 2 }, { 3, 3, 1, 1, 1, 1, 3, 3 },
  { 3, 3, 1, 1, 2, 2, 0, 0 }, { 3, 3, 1, 1, 2, 2, 1, 1 },
  { 3, 3, 1, 1, 2, 2, 2, 2 }, { 3, 3, 1, 1, 2, 2, 3, 3 },
  { 3, 3, 1, 1, 3, 3, 0, 0 }, { 3, 3, 1, 1, 3, 3, 1, 1 },
  { 3, 3, 1, 1, 3, 3, 2, 2 }, { 3, 3, 1, 1, 3, 3, 3, 3 },
  { 3, 3, 2, 2, 0, 0, 0, 0 }, { 3, 3, 2, 2, 0, 0, 1, 1 },
  { 3, 3, 2, 2, 0, 0, 2, 2 }, { 3, 3, 2, 2, 0, 0, 3, 3 },
  { 3, 3, 2, 2, 1, 1, 0, 0 }, { 3, 3, 2, 2, 1, 1, 1, 1 },
  { 3, 3, 2, 2, 1, 1, 2, 2 }, { 3, 3, 2, 2, 1, 1, 3, 3 },
  { 3, 3, 2, 2, 2, 2, 0, 0 }, { 3, 3, 2, 2, 2, 2, 1, 1 },
  { 3, 3, 2, 2, 2, 2, 2, 2 }, { 3, 3, 2, 2, 2, 2, 3, 3 },
  { 3, 3, 2, 2, 3, 3, 0, 0 }, { 3, 3, 2, 2, 3, 3, 1, 1 },
  { 3, 3, 2, 2, 3, 3, 2, 2 }, { 3, 3, 2, 2, 3, 3, 3, 3 },
  { 3, 3, 3, 3, 0, 0, 0, 0 }, { 3, 3, 3, 3, 0, 0, 1, 1 },
  { 3, 3, 3, 3, 0, 0, 2, 2 }, { 3, 3, 3, 3, 0, 0, 3, 3 },
  { 3, 3, 3, 3, 1, 1, 0, 0 }, { 3, 3, 3, 3, 1, 1, 1, 1 },
  { 3, 3, 3, 3, 1, 1, 2, 2 }, { 3, 3, 3, 3, 1, 1, 3, 3 },
  { 3, 3, 3, 3, 2, 2, 0, 0 }, { 3, 3, 3, 3, 2, 2, 1, 1 },
  { 3, 3, 3, 3, 2, 2, 2, 2 }, { 3, 3, 3, 3, 2, 2, 3, 3 },
  { 3, 3, 3, 3, 3, 3, 0, 0 }, { 3, 3, 3, 3, 3, 3, 1, 1 },
  { 3, 3, 3, 3, 3, 3, 2, 2 }, { 3, 3, 3, 3, 3, 3, 3, 3 }
};

template<int SMZX, int TR, int CLIP>
static inline void render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
#ifdef DEBUG
  static boolean printed = false;
  if(!printed)
  {
    fprintf(mzxerr, "%s\n", __PRETTY_FUNCTION__);
    fflush(mzxerr);
    printed = true;
  }
#endif

  const __m256i *selectors = (__m256i *)(SMZX ? selectors_smzx : selectors_mzx);
  ymm char_colors{};
  ymm char_opaque{};
  ymm left_mask;
  ymm right_mask;

  int x, y, i;
  unsigned ch;
  unsigned prev = 0x10000;
  unsigned ppal = graphics->protected_pal_position;
  int tcol = layer->transparent_col;
  unsigned byte_tcol = 0x10000;
  boolean all_tcol = false;
  boolean has_tcol = false;

  struct char_element *src;
  const uint8_t *char_ptr;
  // ssize_t to force matching registers with selectors pointer.
  ssize_t char_byte;

  size_t align_pitch = pitch >> 2;
  size_t align_skip = align_pitch * CHAR_H;
  uint32_t *dest_ptr = reinterpret_cast<uint32_t *>(pixels);
  uint32_t *out_ptr;
  uint32_t *start_ptr = dest_ptr;
  uint32_t *end_ptr = dest_ptr + (height_px - 1) * align_pitch + width_px;

  // CLIP variables
  int start_x = 0;
  int start_y = 0;
  int end_x = layer->w;
  int end_y = layer->h;
  int dest_x = layer->x;
  int dest_y = layer->y;
  size_t src_skip = 0;
  int clip_xl = -1;
  int clip_xr = -1;
  int clip_yt = -1;
  int clip_yb = -1;

  if(CLIP)
  {
    if(precompute_clip(start_x, start_y, end_x, end_y, dest_x, dest_y,
     width_px, height_px, layer))
    {
      // Precalculate clipping masks.
      // This only needs to be done when the layer isn't char-aligned.
      int dest_last_x = layer->x + (int)layer->w * CHAR_W;
      if((layer->x < 0 || dest_last_x > width_px) && (dest_x & 7))
      {
        int bound_x = CHAR_W - ((unsigned)dest_x % CHAR_W);
        for(i = 0; i < bound_x; i++)
        {
          left_mask.values[i] = 0;
          right_mask.values[i] = 0xffffffffu;
        }
        for(; i < CHAR_W; i++)
        {
          left_mask.values[i] = 0xffffffffu;
          right_mask.values[i] = 0;
        }
        if(layer->x < 0)
          clip_xl = start_x;
        if(dest_last_x > width_px)
          clip_xr = end_x - 1;
      }

      if(layer->y < 0)
        clip_yt = start_y;
      if(layer->y + (int)layer->h * CHAR_H > height_px)
        clip_yb = end_y - 1;
    }
    src_skip = layer->w - (end_x - start_x);
  }

  dest_ptr += dest_y * (ptrdiff_t)align_pitch;
  dest_ptr += dest_x;
  src = layer->data + start_x + (start_y * layer->w);

  align_skip -= (end_x - start_x) * CHAR_W;

  memset(&char_colors, 0, sizeof(char_colors));
  memset(&char_opaque, 0, sizeof(char_opaque));

  for(y = start_y; y < end_y; y++, src += src_skip, dest_ptr += align_skip)
  {
    int clip_y = CLIP && (y == clip_yt || y == clip_yb);
#ifndef __AVX2__
    int pix_y = layer->y + y * CHAR_H;
#endif

    for(x = start_x; x < end_x; x++, src++, dest_ptr += CHAR_W)
    {
      ch = src->char_value;
      if(ch == INVISIBLE_CHAR)
        continue;

      if(ch > 0xFF)
      {
        ch = (ch & 0xFF) + PROTECTED_CHARSET_POSITION;
      }
      else
      {
        ch += layer->offset;
        ch %= PROTECTED_CHARSET_POSITION;
      }

      if(prev != ((uint16_t *)src)[1])
      {
        prev = ((uint16_t *)src)[1];
        if(SMZX)
        {
          unsigned pal = ((src->bg_color << 4) | src->fg_color);
          all_tcol = true;
          has_tcol = false;
          for(i = 0; i < 4; i++)
          {
            int idx = graphics->smzx_indices[(pal << 2) + i];
            char_colors.values[i] = graphics->flat_intensity_palette[idx];
            if(TR)
            {
              if(!has_tcol)
              {
                static const int tcol_bytes[4] = { 0x00, 0x55, 0xaa, 0xff };
                byte_tcol = tcol_bytes[i];
              }
              all_tcol &= idx == tcol;
              has_tcol |= idx == tcol;
              char_opaque.values[i] = idx == tcol ? 0 : 0xffffffffu;
            }
          }
          // Duplicate colors/opaque into the second lane.
          for(i = 0; i < 4; i++)
          {
            char_colors.values[i + 4] = char_colors.values[i];
            if(TR && has_tcol)
              char_opaque.values[i + 4] = char_opaque.values[i];
          }
        }
        else
        {
          int bg = select_color_16(src->bg_color, ppal);
          int fg = select_color_16(src->fg_color, ppal);
          char_colors.values[0] = graphics->flat_intensity_palette[bg];
          char_colors.values[1] = graphics->flat_intensity_palette[fg];
          // Duplicate colors into the second lane.
          for(i = 0; i < 2; i++)
            char_colors.values[i + 4] = char_colors.values[i];

          if(TR)
          {
            has_tcol = bg == tcol || fg == tcol;
            all_tcol = bg == tcol && fg == tcol;
            byte_tcol = bg == tcol ? 0 : 0xff;
            if(has_tcol)
            {
              char_opaque.values[0] = (bg == tcol) ? 0 : 0xffffffffu;
              char_opaque.values[1] = (fg == tcol) ? 0 : 0xffffffffu;
              for(i = 0; i < 2; i++)
                char_opaque.values[i + 4] = char_opaque.values[i];
            }
          }
        }
      }

      if(TR && all_tcol)
        continue;

      out_ptr = dest_ptr;
      char_ptr = graphics->charset + ch * CHAR_H;

      if(CLIP && (x == clip_xl || x == clip_xr))
      {
        ymm clip_mask = x == clip_xl ? left_mask : right_mask;

#ifndef __AVX2__
        if((x == clip_xl && pix_y <= 0) ||
         (x == clip_xr && pix_y >= height_px - CHAR_H))
        {
          // Safe (slow) algorithm for clips that intersect the redzone,
          // i.e. the top-left corner or bottom-right corner of the frame.
          // This should happen at most two chars ever per layer.
          // This is not required for AVX2, which uses no-fault mask moves.
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(out_ptr + CHAR_W <= start_ptr || out_ptr >= end_ptr)
              continue;

            ymm colors{};
            ymm opaque{};
            char_byte = char_ptr[i];
            __m256i selector = selectors[char_byte];
            colors.vecs = _mm256_permutevar_ps(char_colors.vecs, selector);
            int j;
            if(TR && has_tcol)
            {
              if(byte_tcol == char_ptr[i])
                continue;

              opaque.vecs = _mm256_permutevar_ps(char_opaque.vecs, selector);
            }
            for(j = 0; j < 8; j++)
            {
              if(clip_mask.values[j])
                if(!TR || !has_tcol || opaque.values[j])
                  out_ptr[j] = colors.values[j];
            }
          }
        }
        else
#endif /* !__AVX2__ */

        if(TR && has_tcol)
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(byte_tcol == char_ptr[i])
              continue;
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte = char_ptr[i];
            __m256i selector = selectors[char_byte];
            __m256i colors = (__m256i)_mm256_permutevar_ps(char_colors.vecs, selector);
            __m256i opaque = (__m256i)_mm256_permutevar_ps(char_opaque.vecs, selector);
            opaque &= clip_mask.vec;
#ifdef __AVX2__
            _mm256_maskstore_epi32((int *)out_ptr, opaque, colors);
#else
            __m256i bg = _mm256_loadu_si256((__m256i_u *)out_ptr);
            _mm256_storeu_si256((__m256i_u *)out_ptr, ((colors ^ bg) & opaque) ^ bg);
#endif
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte = char_ptr[i];
            __m256i selector = selectors[char_byte];
            __m256i colors = (__m256i)_mm256_permutevar_ps(char_colors.vecs, selector);
#ifdef __AVX2__
            _mm256_maskstore_epi32((int *)out_ptr, clip_mask.vec, colors);
#else
            __m256i bg = _mm256_loadu_si256((__m256i_u *)out_ptr);
            _mm256_storeu_si256((__m256i_u *)out_ptr, ((colors ^ bg) & clip_mask.vec) ^ bg);
#endif
          }
        }
      }
      else
      {
        if(TR && has_tcol)
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(byte_tcol == char_ptr[i])
              continue;
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte = char_ptr[i];
            __m256i selector = selectors[char_byte];
            __m256i colors = (__m256i)_mm256_permutevar_ps(char_colors.vecs, selector);
            __m256i opaque = (__m256i)_mm256_permutevar_ps(char_opaque.vecs, selector);
#ifdef __AVX2__
            _mm256_maskstore_epi32((int *)out_ptr, opaque, colors);
#else
            __m256i bg = _mm256_loadu_si256((__m256i_u *)out_ptr);
            _mm256_storeu_si256((__m256i_u *)out_ptr, ((colors ^ bg) & opaque) ^ bg);
#endif
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte = char_ptr[i];
            __m256i selector = selectors[char_byte];
            __m256i colors = (__m256i)_mm256_permutevar_ps(char_colors.vecs, selector);
            _mm256_storeu_si256((__m256i_u *)out_ptr, colors);
          }
        }
      }
    }
  }
}

template<int SMZX, int TR>
static inline void render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer32x8_avx<SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
    case 1:
      render_layer32x8_avx<SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
  }
}

template<int SMZX>
static inline void render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int trans, int clip)
{
  switch(trans)
  {
    case 0:
      render_layer32x8_avx<SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
    case 1:
      render_layer32x8_avx<SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
  }
}

boolean render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  if(width_px & 7)
    return false;

  switch(smzx)
  {
    case 0:
      render_layer32x8_avx<0>(
       pixels, width_px, height_px, pitch, graphics, layer, trans, clip);
      break;
    case 1:
    case 2:
    case 3:
      render_layer32x8_avx<1>(
       pixels, width_px, height_px, pitch, graphics, layer, trans, clip);
      break;
  }
  return true;
}

#else /* HAS_RENDER_LAYER32X8_AVX */

boolean render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  return false;
}

#endif /* HAS_RENDER_LAYER32X8_AVX */
