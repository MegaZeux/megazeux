/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

// This file recursively instantiates template functions to generate many
// versions of the render_layer_func function. The right function to call is
// selected through a dispatch mechanism.

// Errors and warnings in this file are duplicated hundreds of times over, so
// when working on this function it can be good to redirect make's stderr
// stream to a file.

#include "render_layer_common.hpp"

#include "../graphics.h"
#include "../util.h"
#include "../platform/log.h"
#include "../platform/platform_attribute.h"
#include "../platform/platform_endian.h"

#include <stdlib.h>

#ifdef IS_CXX_11
#include <type_traits>
#endif

#ifdef _MSC_VER
#include <cstdio>
#endif

#if defined(PLATFORM_UNALIGN_32) || defined(PLATFORM_UNALIGN_64)
/* May be using intentional unaligned accesses; tell UBSan to calm down. */
#define RENDER_EXTRA_ATTRIBUTES ATTRIBUTE_NO_SANITIZE("alignment")
#else
#define RENDER_EXTRA_ATTRIBUTES
#endif

template<typename PIXTYPE>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR, int CLIP>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer)
 RENDER_EXTRA_ATTRIBUTES;

/**
 * Alignment of pixel buffer (8bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint8_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint8_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint8_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 16:
      render_layer_func<uint8_t, uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 8:
      render_layer_func<uint8_t, uint8_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (8bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (16bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint16_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint16_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint16_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 16:
      render_layer_func<uint16_t, uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (16bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (32bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint32_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint32_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint32_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (32bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Renderer bits-per-pixel (8, 16, or 32).
 * Sets of renderers for lower bpps are larger since they try to support more
 * alignment options, so several platforms disable them altogether to reduce
 * executable size and/or compilation time.
 */
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int bpp, int align, int smzx, int trans, int clip)
{
  switch(bpp)
  {
#ifndef SKIP_8BPP
    case 8:
      render_layer_func<uint8_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
#ifndef SKIP_16BPP
    case 16:
      render_layer_func<uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
#ifndef SKIP_32BPP
    case 32:
      render_layer_func<uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
    default:
      fprintf(mzxerr, "INVALID RENDERER ARG bpp=%d\n"
       "(is this bpp enabled for this platform?)\n", bpp);
      exit(1);
      break;
  }
}

/**
 * Renderer is SMZX (1) or normal MZX (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  switch(smzx)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, 0>(
       pixels, width_px, height_px, pitch, graphics, layer,
       trans, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer_func<PIXTYPE, ALIGNTYPE, 1>(
       pixels, width_px, height_px, pitch, graphics, layer,
       trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG smzx=%d\n", smzx);
      exit(1);
      break;
  }
}

/**
 * Layer transparency enabled (1) or disabled (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int trans, int clip)
{
  switch(trans)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer,
       clip);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer,
       clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG trans=%d\n", trans);
      exit(1);
      break;
  }
}

/**
 * Renderer should clip the layer at the screen boundaries (1) or not (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG clip=%d\n", clip);
      exit(1);
      break;
  }
}

// Macros to perform these shifts while ignoring spurious compiler warnings
// that can't be turned off for older versions of GCC. Since BPP is constexpr
// these should all optimize to single shifts.
#define BPPx1(c) (c << (BPP - 1) << 1)
#define BPPx2(c) (c << (BPP - 1) << (BPP - 1) << 2)
#define BPPx3(c) (c << (BPP - 1) << (BPP - 1) << (BPP - 1) << 3)

/**
 * Precompute an array of color combinations for multipixel write renderers.
 *
 * Since this requires 1<<PPW values to be computed, 8 PPW renderers would
 * need 256 combinations, which would probably hurt performance more than help
 * it. Instead, 8 PPW just uses a special case of the 4 PPW code (see below).
 */
template<int BPP, int PPW, typename ALIGNTYPE>
static inline void set_colors_mzx(ALIGNTYPE (&dest)[16], ALIGNTYPE bg, ALIGNTYPE fg)
{
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
  switch(PPW)
  {
    case 1:
      break;

    case 2:
      dest[0] = BPPx1(bg) | bg;
      dest[1] = BPPx1(fg) | bg;
      dest[2] = BPPx1(bg) | fg;
      dest[3] = BPPx1(fg) | fg;
      break;

    case 4:
    case 8:
      dest[0]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[1]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[2]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[3]  = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[4]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[5]  = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[6]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[7]  = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[8]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[9]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[10] = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[11] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[12] = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[13] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[14] = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | fg;
      dest[15] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | fg;
      break;
  }
#else
  switch(PPW)
  {
    case 1:
      break;

    case 2:
      dest[0] = BPPx1(bg) | bg;
      dest[1] = BPPx1(bg) | fg;
      dest[2] = BPPx1(fg) | bg;
      dest[3] = BPPx1(fg) | fg;
      break;

    case 4:
    case 8:
      dest[0]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[1]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[2]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[3]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[4]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[5]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[6]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[7]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | fg;
      dest[8]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[9]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[10] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[11] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[12] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[13] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[14] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[15] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | fg;
      break;
  }
#endif
}

/* Colors should be pre-doubled here. */
template<int BPP, int PPW, typename ALIGNTYPE>
static inline void set_colors_smzx(ALIGNTYPE (&dest)[16], ALIGNTYPE (&cols)[4])
{
  ALIGNTYPE c0 = cols[0];
  ALIGNTYPE c1 = cols[1];
  ALIGNTYPE c2 = cols[2];
  ALIGNTYPE c3 = cols[3];
  switch(PPW)
  {
    case 1:
    case 2:
      break;

    case 4:
    case 8:
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
      dest[0]  = BPPx2(c0) | c0;
      dest[1]  = BPPx2(c1) | c0;
      dest[2]  = BPPx2(c2) | c0;
      dest[3]  = BPPx2(c3) | c0;
      dest[4]  = BPPx2(c0) | c1;
      dest[5]  = BPPx2(c1) | c1;
      dest[6]  = BPPx2(c2) | c1;
      dest[7]  = BPPx2(c3) | c1;
      dest[8]  = BPPx2(c0) | c2;
      dest[9]  = BPPx2(c1) | c2;
      dest[10] = BPPx2(c2) | c2;
      dest[11] = BPPx2(c3) | c2;
      dest[12] = BPPx2(c0) | c3;
      dest[13] = BPPx2(c1) | c3;
      dest[14] = BPPx2(c2) | c3;
      dest[15] = BPPx2(c3) | c3;
#else
      dest[0]  = BPPx2(c0) | c0;
      dest[1]  = BPPx2(c0) | c1;
      dest[2]  = BPPx2(c0) | c2;
      dest[3]  = BPPx2(c0) | c3;
      dest[4]  = BPPx2(c1) | c0;
      dest[5]  = BPPx2(c1) | c1;
      dest[6]  = BPPx2(c1) | c2;
      dest[7]  = BPPx2(c1) | c3;
      dest[8]  = BPPx2(c2) | c0;
      dest[9]  = BPPx2(c2) | c1;
      dest[10] = BPPx2(c2) | c2;
      dest[11] = BPPx2(c2) | c3;
      dest[12] = BPPx2(c3) | c0;
      dest[13] = BPPx2(c3) | c1;
      dest[14] = BPPx2(c3) | c2;
      dest[15] = BPPx2(c3) | c3;
#endif
      break;
  }
}

template<int PPW>
static inline unsigned get_colors_index(unsigned char_byte, int write_pos)
{
  return ((char_byte << (write_pos * PPW)) & 0xff) >> (8 - PPW);
}

/**
 * Use the set_colors array to compute multiple pixel values simultaneously.
 * This optimization is only useful for PPW >= 2 renderers.
 */
template<int PPW, typename ALIGNTYPE>
static inline ALIGNTYPE get_colors(ALIGNTYPE (&set_colors)[16], unsigned idx)
{
  switch(PPW)
  {
    // Should be unreachable, but some compilers complain...
    case 1:
      break;

    case 2:
    case 4:
      return set_colors[idx];

    case 8:
    {
      unsigned int left = (idx & 0xF0) >> 4;
      unsigned int right = (idx & 0x0F);
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
      return (set_colors[right] << sizeof(ALIGNTYPE) * 4) | set_colors[left];
#else
      return (set_colors[left] << sizeof(ALIGNTYPE) * 4) | set_colors[right];
#endif
    }
  }
  return 0;
}

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
#define PIXEL_POS(i)      (BPP * (PPW - 1 - (i)))
#define PIXEL_POS_PAIR(i) (BPP * (PPW - 2 - (i)))
#else
#define PIXEL_POS(i)      (BPP * (i))
#define PIXEL_POS_PAIR(i) (BPP * (i))
#endif

/**
 * Finally, render the layer.
 * The optimizer will optimize out the unnecessary parts for relevant renderers.
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR, int CLIP>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
#ifdef IS_CXX_11
  constexpr int BPP = sizeof(PIXTYPE) * 8;
  constexpr int PPW = sizeof(ALIGNTYPE) / sizeof(PIXTYPE);

  // Make sure some pointless/impossible renderers are never instantiated.
  static_assert((PPW >= 1), "invalid ppw < 1");
  static_assert((PPW <= 8), "invalid ppw > 8");
  static_assert((PPW == 1) || (PPW == 2) || (PPW == 4) || (PPW == 8),
   "invalid ppw (must be power of 2)");
#else
// Use these if for whatever reason C++11 isn't available.
#define BPP (int)(sizeof(PIXTYPE) * 8)
#define PPW (int)(sizeof(ALIGNTYPE) / sizeof(PIXTYPE))
#endif

#ifdef DEBUG
  static boolean printed = false;
  if(!printed)
  {
    fprintf(mzxerr, "%s\n", __PRETTY_FUNCTION__);
    fflush(mzxerr);
    printed = true;
  }
#endif

  int x, y;
  uint16_t c;

  const struct char_element *src = layer->data;
  int row;

  // Transparency vars...
  int tcol = layer->transparent_col;
  ALIGNTYPE mask = (PIXTYPE)(~0);
  ALIGNTYPE smask = BPPx1(mask) | mask;

  const uint8_t *char_ptr;
  unsigned int current_char_byte;
  unsigned int pcol;
  unsigned idx;

  ALIGNTYPE pix;

  int i;

  // This array will actually only store PIXTYPE values, but making it
  // ALIGNTYPE instead helps avoid some warnings.
  ALIGNTYPE char_colors[4];
  int char_idx[4];
  int ppal = graphics->protected_pal_position;
  int write_pos;

  ALIGNTYPE set_colors[16];
  ALIGNTYPE set_opaque[16];
  unsigned prev = 0x10000;
  boolean has_tcol = false;
  boolean all_tcol = true;
  // In mode 0 it's possible to quickly determine if an entire byte is
  // transparent. If it is, there's no point in drawing it. SMZX modes
  // can try to do a similar trick, but it won't work if the char has
  // multiple indices set to the transparent color.
  unsigned int byte_tcol = 0xFFFF;

  size_t pix_pitch = pitch / sizeof(PIXTYPE);
  size_t pix_skip = pix_pitch * CHAR_H;
  PIXTYPE *dest_ptr = reinterpret_cast<PIXTYPE *>(pixels);
  PIXTYPE *out_ptr;
  PIXTYPE *start_ptr = dest_ptr;
  PIXTYPE *end_ptr = dest_ptr + (height_px - 1) * pix_pitch + width_px;

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
      // Precalculate clipping boundaries.
      if(layer->x + start_x * CHAR_W < 0)
        clip_xl = start_x;
      if(layer->x + end_x * CHAR_W > width_px)
        clip_xr = end_x - 1;

      if(layer->y + start_y * CHAR_H < 0)
        clip_yt = start_y;
      if(layer->y + end_y * CHAR_H > height_px)
        clip_yb = end_y - 1;
    }
    src_skip = layer->w - (end_x - start_x);
  }

  dest_ptr += dest_y * (ptrdiff_t)pix_pitch;
  dest_ptr += dest_x;
  src = layer->data + start_x + (start_y * layer->w);

  pix_skip -= (end_x - start_x) * CHAR_W;

  /* Enable 1PPW writes for >1PPW renderers at X clip boundaries if anything
   * in the render will be off of alignment. */
  const int unaligned =
   ((size_t)pixels | (size_t)dest_ptr | pitch) % sizeof(ALIGNTYPE);

  for(y = start_y; y < end_y; y++, src += src_skip, dest_ptr += pix_skip)
  {
    int clip_y = CLIP && (y == clip_yt || y == clip_yb);
    int pix_y = layer->y + y * CHAR_H;

    for(x = start_x; x < end_x; x++, src++, dest_ptr += CHAR_W)
    {
      int clip_x = CLIP && (x == clip_xl || x == clip_xr);
      int pix_x = layer->x + x * CHAR_W;

      c = select_char(src, layer);
      if(c != INVISIBLE_CHAR)
      {
        /* For 4PPW and 8PPW, only update the color arrays if the palette has
         * actually changed. This optimization is negligible or worse for lower
         * write sizes since it blocks other compiler optimizations. */
        unsigned both_cols = both_colors(src);
        if(PPW <= 2 || prev != both_cols)
        {
          prev = both_cols;
          if(SMZX)
          {
            unsigned int pal = ((src->bg_color & 0xF) << 4) | (src->fg_color & 0xF);
            ALIGNTYPE masks[4];
            all_tcol = true;
            has_tcol = false;
            byte_tcol = 0xFFFF;
            for(i = 0; i < 4; i++)
            {
              char_idx[i] = graphics->smzx_indices[pal * 4 + i];

              if(BPP > 8)
                char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
              else
                char_colors[i] = char_idx[i];

              if(TR)
              {
                if(!has_tcol)
                {
                  static const uint8_t tcol_bytes[] = { 0x00, 0x55, 0xAA, 0xFF };
                  byte_tcol = tcol_bytes[i];
                }
                has_tcol |= char_idx[i] == tcol;
                all_tcol &= char_idx[i] == tcol;
                if(PPW > 2)
                  masks[i] = char_idx[i] == tcol ? 0 : smask;
              }

              // If writing more than 2 pixels at once, preemptively double
              // them. This saves having to perform this operation later...
              if(PPW > 1)
                char_colors[i] |= BPPx1(char_colors[i]);
            }
            if(PPW > 2)
            {
              set_colors_smzx<BPP, PPW>(set_colors, char_colors);
              if(TR && has_tcol)
                set_colors_smzx<BPP, PPW>(set_opaque, masks);
            }
          }
          else
          {
            char_idx[0] = select_color_16(src->bg_color, ppal);
            char_idx[1] = select_color_16(src->fg_color, ppal);

            for(i = 0; i < 2; i++)
            {
              if(BPP > 8)
                char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
              else
                char_colors[i] = (uint8_t)char_idx[i];
            }

            if(TR)
            {
              has_tcol = (char_idx[0] == tcol || char_idx[1] == tcol);
              all_tcol = (char_idx[0] == tcol && char_idx[1] == tcol);
              byte_tcol = (char_idx[1] == tcol) ? 0xFF : 0x00;
            }

            if(PPW > 1)
            {
              set_colors_mzx<BPP, PPW>(set_colors, char_colors[0], char_colors[1]);
              if(TR && has_tcol)
              {
                ALIGNTYPE m0 = !byte_tcol ? 0 : mask;
                ALIGNTYPE m1 = byte_tcol ? 0 : mask;
                set_colors_mzx<BPP, PPW>(set_opaque, m0, m1);
              }
            }
          }
        }

        // Don't bother drawing chars that are completely transparent.
        if(TR && all_tcol)
          continue;

        out_ptr = dest_ptr;
        char_ptr = graphics->charset + (c * CHAR_H);

        // Force 1 PPW if this renderer is using unaligned writes on an edge.
        if(PPW == 1 || (CLIP && clip_x && unaligned))
        {
          for(row = 0; row < CHAR_H; row++, out_ptr += pix_pitch)
          {
            current_char_byte = char_ptr[row];

            if(TR && has_tcol && current_char_byte == byte_tcol)
              continue;
            if(CLIP && clip_y && (pix_y + row < 0 || out_ptr >= end_ptr))
              continue;

            for(write_pos = 0; write_pos < CHAR_W; write_pos++)
            {
              if(!SMZX)
              {
                if(CLIP && clip_x && (pix_x + write_pos < 0 || pix_x + write_pos >= width_px))
                  continue;

                pcol = !!(current_char_byte & (0x80 >> write_pos));
                if(!TR || !has_tcol || tcol != char_idx[pcol])
                  out_ptr[write_pos] = char_colors[pcol];
              }
              else
              {
                pcol = (current_char_byte & (0xC0 >> write_pos)) << write_pos >> 6;
                if(TR && has_tcol && tcol == char_idx[pcol])
                {
                  write_pos++;
                  continue;
                }

                pix = char_colors[pcol];
                if(!CLIP || !clip_x || (pix_x + write_pos >= 0 && pix_x + write_pos < width_px))
                  out_ptr[write_pos] = pix;

                write_pos++;

                if(!CLIP || !clip_x || (pix_x + write_pos >= 0 && pix_x + write_pos < width_px))
                  out_ptr[write_pos] = pix;
              }
            }
          }
        }
        else
        {
          for(row = 0; row < CHAR_H; row++, out_ptr += pix_pitch)
          {
            current_char_byte = char_ptr[row];

            if(TR && has_tcol && current_char_byte == byte_tcol)
              continue;
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            ALIGNTYPE *write_ptr = reinterpret_cast<ALIGNTYPE *>(out_ptr);

            for(write_pos = 0; write_pos < CHAR_W / PPW; write_pos++)
            {
              if(!CLIP || !clip_x ||
               ((pix_x + write_pos * PPW >= 0) &&
                (pix_x + write_pos * PPW + PPW - 1 < width_px)))
              {
                if(SMZX && PPW == 2)
                {
                  ALIGNTYPE shift = write_pos * PPW;
                  pcol = (current_char_byte & (0xC0 >> shift)) << shift >> 6;

                  if(!TR || !has_tcol || tcol != char_idx[pcol])
                    write_ptr[write_pos] = char_colors[pcol];
                }
                else
                {
                  idx = get_colors_index<PPW>(current_char_byte, write_pos);
                  pix = get_colors<PPW>(set_colors, idx);

                  if(TR && has_tcol)
                  {
                    ALIGNTYPE opaque = get_colors<PPW>(set_opaque, idx);
                    pix = (pix & opaque) | (write_ptr[write_pos] & ~opaque);
                  }
                  write_ptr[write_pos] = pix;
                }
              }
            }
          }
        }
      }
    }
  }
}
