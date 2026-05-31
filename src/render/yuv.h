/* MegaZeux
 *
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

// General macros and functions for YUV manipulation. Supports YUY2, UYVY, and
// YVYU. SDL doesn't provide functions for packing/unpacking these.

#ifndef MEGAZEUX_YUV_H
#define MEGAZEUX_YUV_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../platform/platform_endian.h"
#include <stdint.h>

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN

#define YUY2_Y1_SHIFT 24
#define YUY2_Y2_SHIFT  8
#define YUY2_U_SHIFT  16
#define YUY2_V_SHIFT   0

#define UYVY_Y1_SHIFT 16
#define UYVY_Y2_SHIFT  0
#define UYVY_U_SHIFT  24
#define UYVY_V_SHIFT   8

#define YVYU_Y1_SHIFT 24
#define YVYU_Y2_SHIFT  8
#define YVYU_U_SHIFT   0
#define YVYU_V_SHIFT  16

#else

#define YUY2_Y1_SHIFT  0
#define YUY2_Y2_SHIFT 16
#define YUY2_U_SHIFT   8
#define YUY2_V_SHIFT  24

#define UYVY_Y1_SHIFT  8
#define UYVY_Y2_SHIFT 24
#define UYVY_U_SHIFT   0
#define UYVY_V_SHIFT  16

#define YVYU_Y1_SHIFT  0
#define YVYU_Y2_SHIFT 16
#define YVYU_U_SHIFT  24
#define YVYU_V_SHIFT   8

#endif

#define YUY2_Y1_MASK ((uint32_t)(0xFF) << YUY2_Y1_SHIFT)
#define YUY2_Y2_MASK ((uint32_t)(0xFF) << YUY2_Y2_SHIFT)
#define YUY2_U_MASK  ((uint32_t)(0xFF) << YUY2_U_SHIFT)
#define YUY2_V_MASK  ((uint32_t)(0xFF) << YUY2_V_SHIFT)
#define YUY2_UV_MASK (YUY2_U_MASK | YUY2_V_MASK)

#define UYVY_Y1_MASK ((uint32_t)(0xFF) << UYVY_Y1_SHIFT)
#define UYVY_Y2_MASK ((uint32_t)(0xFF) << UYVY_Y2_SHIFT)
#define UYVY_U_MASK  ((uint32_t)(0xFF) << UYVY_U_SHIFT)
#define UYVY_V_MASK  ((uint32_t)(0xFF) << UYVY_V_SHIFT)
#define UYVY_UV_MASK (UYVY_U_MASK | UYVY_V_MASK)

#define YVYU_Y1_MASK ((uint32_t)(0xFF) << YVYU_Y1_SHIFT)
#define YVYU_Y2_MASK ((uint32_t)(0xFF) << YVYU_Y2_SHIFT)
#define YVYU_U_MASK  ((uint32_t)(0xFF) << YVYU_U_SHIFT)
#define YVYU_V_MASK  ((uint32_t)(0xFF) << YVYU_V_SHIFT)
#define YVYU_UV_MASK (YVYU_U_MASK | YVYU_V_MASK)

static inline uint32_t yuy2_pack(uint8_t y1, uint8_t u, uint8_t y2, uint8_t v)
{
  return
   (y1 << YUY2_Y1_SHIFT) | (u << YUY2_U_SHIFT) |
   (y2 << YUY2_Y2_SHIFT) | (v << YUY2_V_SHIFT);
}

static inline uint32_t uyvy_pack(uint8_t y1, uint8_t u, uint8_t y2, uint8_t v)
{
  return
   (y1 << UYVY_Y1_SHIFT) | (u << UYVY_U_SHIFT) |
   (y2 << UYVY_Y2_SHIFT) | (v << UYVY_V_SHIFT);
}

static inline uint32_t yvyu_pack(uint8_t y1, uint8_t u, uint8_t y2, uint8_t v)
{
  return
   (y1 << YVYU_Y1_SHIFT) | (u << YVYU_U_SHIFT) |
   (y2 << YVYU_Y2_SHIFT) | (v << YVYU_V_SHIFT);
}

// First byte is u or v -> LE must add first, BE must shift first
static inline uint32_t yuv_subsample_uv0(uint32_t a, uint32_t b,
 uint32_t y1_mask, uint32_t y2_mask, uint32_t uv_mask)
{
  uint32_t y1 = a & y1_mask;
  uint32_t y2 = b & y2_mask;
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  uint32_t uv = (((a & uv_mask) >> 1) + ((b & uv_mask) >> 1)) & uv_mask;
#else
  uint32_t uv = (((a & uv_mask) + (b & uv_mask)) >> 1) & uv_mask;
#endif
  return y1 | y2 | uv;
}

// First byte is y1 or y2 -> LE must shift first, BE must add first
static inline uint32_t yuv_subsample_y0(uint32_t a, uint32_t b,
 uint32_t y1_mask, uint32_t y2_mask, uint32_t uv_mask)
{
  uint32_t y1 = a & y1_mask;
  uint32_t y2 = b & y2_mask;
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  uint32_t uv = (((a & uv_mask) + (b & uv_mask)) >> 1) & uv_mask;
#else
  uint32_t uv = (((a & uv_mask) >> 1) + ((b & uv_mask) >> 1)) & uv_mask;
#endif
  return y1 | y2 | uv;
}

static inline uint32_t yuy2_subsample(uint32_t a, uint32_t b)
{
  return yuv_subsample_y0(a, b, YUY2_Y1_MASK, YUY2_Y2_MASK, YUY2_UV_MASK);
}

static inline uint32_t uyvy_subsample(uint32_t a, uint32_t b)
{
  return yuv_subsample_uv0(a, b, UYVY_Y1_MASK, UYVY_Y2_MASK, UYVY_UV_MASK);
}

static inline uint32_t yvyu_subsample(uint32_t a, uint32_t b)
{
  return yuv_subsample_y0(a, b, YVYU_Y1_MASK, YVYU_Y2_MASK, YVYU_UV_MASK);
}

// Full-swing RGB to YUV conversion.
static inline void rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b,
                              uint8_t *y, uint8_t *u, uint8_t *v)
{
  *y = (9797 * r + 19237 * g + 3734 * b) >> 15;
  *u = ((18492 * (b - *y)) >> 15) + 128;
  *v = ((23372 * (r - *y)) >> 15) + 128;
}

/**
 * Studio-swing RGB to YUV/YCbCr converison.
 * This is required for Apple's OpenGL YUV texture extension.
 *
 * Note: an approach like above using B/R - Y' differences requires
 * computing a full-swing Y' separate from the output Y', which results
 * in only 1 fewer multiplication than using the matrix (so don't bother).
 *
 * Kv = [ 0.299, 0.587, 0.114 ]
 *
 * Yv  = Kv * 219 * (1 << 16) / 255
 * Cbv = (([ 0 0 1 ] - Kv) / (2 * (1 - Kb)) * 224 * (1 << 16) / 255
 * Crv = (([ 1 0 0 ] - Kv) / (2 * (1 - Kr)) * 224 * (1 << 16) / 255
 */
static inline void rgb_to_ycbcr(uint8_t r, uint8_t g, uint8_t b,
                                uint8_t *y, uint8_t *cb, uint8_t *cr)
{
  *y  = ((16829 * r +  33039 * g +  6416 * b + 32768) >> 16) + 16;
  *cb = ((-9714 * r + -19071 * g + 28784 * b + 32768) >> 16) + 128;
  *cr = ((28784 * r + -24103 * g + -4681 * b + 32768) >> 16) + 128;
}

static inline uint32_t rgb_to_yuy2(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return yuy2_pack(y, u, y, v);
}

static inline uint32_t rgb_to_uyvy(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return uyvy_pack(y, u, y, v);
}

static inline uint32_t rgb_to_yvyu(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return yvyu_pack(y, u, y, v);
}

static inline uint32_t rgb_to_apple_ycbcr_422(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t y, cb, cr;
  rgb_to_ycbcr(r, g, b, &y, &cb, &cr);
  return uyvy_pack(y, cb, y, cr);
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_YUV_H */
