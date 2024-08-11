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
#include "platform_endian.h"
#include "render_layer_common.hpp"

#ifdef DEBUG
#include "util.h"
#endif

#if defined(HAS_RENDER_LAYER32X4_ALTIVEC) && defined(__ALTIVEC__)
#include <altivec.h>

#define ALIGN16 __attribute__((aligned(16)))

typedef __vector unsigned char vec8x16_t;
typedef __vector unsigned int vec32x4_t;

/* Preserve byte order, the flat palette already accounts for endianness. */
#define SELECT_0 0, 1, 2, 3
#define SELECT_1 4, 5, 6, 7
#define SELECT_2 8, 9, 10, 11
#define SELECT_3 12, 13, 14, 15

static const uint8_t selectors_mzx[16][16] ALIGN16 =
{
  { SELECT_0, SELECT_0, SELECT_0, SELECT_0 },
  { SELECT_0, SELECT_0, SELECT_0, SELECT_1 },
  { SELECT_0, SELECT_0, SELECT_1, SELECT_0 },
  { SELECT_0, SELECT_0, SELECT_1, SELECT_1 },
  { SELECT_0, SELECT_1, SELECT_0, SELECT_0 },
  { SELECT_0, SELECT_1, SELECT_0, SELECT_1 },
  { SELECT_0, SELECT_1, SELECT_1, SELECT_0 },
  { SELECT_0, SELECT_1, SELECT_1, SELECT_1 },
  { SELECT_1, SELECT_0, SELECT_0, SELECT_0 },
  { SELECT_1, SELECT_0, SELECT_0, SELECT_1 },
  { SELECT_1, SELECT_0, SELECT_1, SELECT_0 },
  { SELECT_1, SELECT_0, SELECT_1, SELECT_1 },
  { SELECT_1, SELECT_1, SELECT_0, SELECT_0 },
  { SELECT_1, SELECT_1, SELECT_0, SELECT_1 },
  { SELECT_1, SELECT_1, SELECT_1, SELECT_0 },
  { SELECT_1, SELECT_1, SELECT_1, SELECT_1 },
};

static const uint8_t selectors_smzx[16][16] ALIGN16 =
{
  { SELECT_0, SELECT_0, SELECT_0, SELECT_0 },
  { SELECT_0, SELECT_0, SELECT_1, SELECT_1 },
  { SELECT_0, SELECT_0, SELECT_2, SELECT_2 },
  { SELECT_0, SELECT_0, SELECT_3, SELECT_3 },
  { SELECT_1, SELECT_1, SELECT_0, SELECT_0 },
  { SELECT_1, SELECT_1, SELECT_1, SELECT_1 },
  { SELECT_1, SELECT_1, SELECT_2, SELECT_2 },
  { SELECT_1, SELECT_1, SELECT_3, SELECT_3 },
  { SELECT_2, SELECT_2, SELECT_0, SELECT_0 },
  { SELECT_2, SELECT_2, SELECT_1, SELECT_1 },
  { SELECT_2, SELECT_2, SELECT_2, SELECT_2 },
  { SELECT_2, SELECT_2, SELECT_3, SELECT_3 },
  { SELECT_3, SELECT_3, SELECT_0, SELECT_0 },
  { SELECT_3, SELECT_3, SELECT_1, SELECT_1 },
  { SELECT_3, SELECT_3, SELECT_2, SELECT_2 },
  { SELECT_3, SELECT_3, SELECT_3, SELECT_3 },
};

template<int ALIGNED>
static void altivec_store(uint32_t *out_ptr, vec32x4_t col_l, vec32x4_t col_r,
 vec32x4_t tr_l, vec32x4_t tr_r)
{
#ifdef __VSX__
  // IBM docs claim dereferencing should work for unaligned read/write
  // but in practice with GCC only the intrinsics worked.
  vec32x4_t bg_l = vec_xl(0, out_ptr);
  vec32x4_t bg_r = vec_xl(16, out_ptr);

  vec_xst(vec_sel(bg_l, col_l, tr_l),  0, out_ptr);
  vec_xst(vec_sel(bg_r, col_r, tr_r), 16, out_ptr);
#else
  if(ALIGNED)
  {
    vec32x4_t bg_l = vec_ld(0, out_ptr);
    vec32x4_t bg_r = vec_ld(16, out_ptr);

    vec_st(vec_sel(bg_l, col_l, tr_l),  0, out_ptr);
    vec_st(vec_sel(bg_r, col_r, tr_r), 16, out_ptr);
  }
  else
  {
    vec8x16_t selector = vec_lvsr(0, out_ptr);
    vec32x4_t bg_l = vec_ld(0, out_ptr); // TRUNCATED!
    vec32x4_t bg_m = vec_ld(16, out_ptr);
    vec32x4_t bg_r = vec_ld(32, out_ptr);
    vec32x4_t zero = { 0, 0, 0, 0 };

    bg_l = vec_sel(bg_l,  vec_perm(zero, col_l, selector),
                          vec_perm(zero, tr_l, selector));
    bg_m = vec_sel(bg_m,  vec_perm(col_l, col_r, selector),
                          vec_perm(tr_l, tr_r, selector));
    bg_r = vec_sel(bg_r,  vec_perm(col_r, zero, selector),
                          vec_perm(tr_r, zero, selector));

    vec_st(bg_l,  0, out_ptr);
    vec_st(bg_m, 16, out_ptr);
    vec_st(bg_r, 32, out_ptr);
  }
#endif
}

template<int ALIGNED>
static void altivec_store(uint32_t *out_ptr, vec32x4_t col_l, vec32x4_t col_r)
{
#ifdef __VSX__
  vec_xst(col_l,  0, out_ptr);
  vec_xst(col_r, 16, out_ptr);
#else
  if(ALIGNED)
  {
    vec_st(col_l,  0, out_ptr);
    vec_st(col_r, 16, out_ptr);
  }
  else
  {
    vec32x4_t one = { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu };
    altivec_store<0>(out_ptr, col_l, col_r, one, one);
  }
#endif
}

template<int ALIGNED, int SMZX, int TR, int CLIP>
static inline void render_layer32x4_altivec(
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

  const uint8_t (&selectors)[16][16] = SMZX ? selectors_smzx : selectors_mzx;
  vec32x4_t left_mask[2];
  vec32x4_t right_mask[2];
  vec32x4_t char_colors = { 0, 0, 0, 0 };
  vec32x4_t char_opaque = { 0, 0, 0, 0 };

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
  unsigned char_byte_left;
  unsigned char_byte_right;

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
          left_mask[i >> 2][i & 3] = 0;
          right_mask[i >> 2][i & 3] = 0xffffffffu;
        }
        for(; i < CHAR_W; i++)
        {
          left_mask[i >> 2][i & 3] = 0xffffffffu;
          right_mask[i >> 2][i & 3] = 0;
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

  for(y = start_y; y < end_y; y++, src += src_skip, dest_ptr += align_skip)
  {
    int clip_y = CLIP && (y == clip_yt || y == clip_yb);
    int pix_y = layer->y + y * CHAR_H;

    for(x = start_x; x < end_x; x++, src++, dest_ptr += CHAR_W)
    {
      ch = select_char(src, layer);
      if(ch >= INVISIBLE_CHAR)
        continue;

      if(prev != both_colors(src))
      {
        prev = both_colors(src);
        if(SMZX)
        {
          unsigned pal = ((src->bg_color << 4) | src->fg_color);
          all_tcol = true;
          has_tcol = false;
          for(i = 0; i < 4; i++)
          {
            int idx = graphics->smzx_indices[(pal << 2) + i];
            char_colors[i] = graphics->flat_intensity_palette[idx];
            if(TR)
            {
              if(!has_tcol)
              {
                static const int tcol_bytes[4] = { 0x00, 0x55, 0xaa, 0xff };
                byte_tcol = tcol_bytes[i];
              }
              all_tcol &= idx == tcol;
              has_tcol |= idx == tcol;
              char_opaque[i] = idx == tcol ? 0 : 0xffffffffu;
            }
          }
        }
        else
        {
          int bg = select_color_16(src->bg_color, ppal);
          int fg = select_color_16(src->fg_color, ppal);
          char_colors[0] = graphics->flat_intensity_palette[bg];
          char_colors[1] = graphics->flat_intensity_palette[fg];

          if(TR)
          {
            has_tcol = bg == tcol || fg == tcol;
            all_tcol = bg == tcol && fg == tcol;
            byte_tcol = bg == tcol ? 0 : 0xff;

            if(has_tcol)
            {
              char_opaque[0] = (bg == tcol) ? 0 : 0xffffffffu;
              char_opaque[1] = (fg == tcol) ? 0 : 0xffffffffu;
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
        vec32x4_t (&clip_mask)[2] = x == clip_xl ? left_mask : right_mask;

        if((x == clip_xl && pix_y <= 0) ||
         (x == clip_xr && pix_y >= height_px - CHAR_H))
        {
          // Safe (slow) algorithm for clips that intersect the redzone,
          // i.e. the top-left corner or bottom-right corner of the frame.
          // This should happen at most two chars ever per layer.
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(out_ptr + CHAR_W <= start_ptr || out_ptr >= end_ptr)
              continue;

            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;

            vec8x16_t sel_l = vec_ld(char_byte_left, *selectors);
            vec8x16_t sel_r = vec_ld(char_byte_right, *selectors);
            vec32x4_t col_l = vec_vperm(char_colors, char_colors, sel_l);
            vec32x4_t col_r = vec_vperm(char_colors, char_colors, sel_r);
            vec32x4_t tr_l = { 0, 0, 0, 0 };
            vec32x4_t tr_r = { 0, 0, 0, 0 };
            int j;
            if(TR && has_tcol)
            {
              if(byte_tcol == char_ptr[i])
                continue;

              tr_l = vec_vperm(char_opaque, char_opaque, sel_l);
              tr_r = vec_vperm(char_opaque, char_opaque, sel_r);
            }
            for(j = 0; j < 4; j++)
            {
              if(clip_mask[0][j])
                if(!TR || !has_tcol || tr_l[j])
                  out_ptr[j] = col_l[j];
            }
            for(j = 0; j < 4; j++)
            {
              if(clip_mask[1][j])
                if(!TR || !has_tcol || tr_r[j])
                  out_ptr[j + 4] = col_r[j];
            }
          }
        }
        else

        if(TR && has_tcol)
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(byte_tcol == char_ptr[i])
              continue;
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;

            vec8x16_t sel_l = vec_ld(char_byte_left, *selectors);
            vec8x16_t sel_r = vec_ld(char_byte_right, *selectors);
            vec32x4_t col_l = vec_vperm(char_colors, char_colors, sel_l);
            vec32x4_t col_r = vec_vperm(char_colors, char_colors, sel_r);
            vec32x4_t tr_l = vec_vperm(char_opaque, char_opaque, sel_l);
            vec32x4_t tr_r = vec_vperm(char_opaque, char_opaque, sel_r);

            tr_l = vec_and(tr_l, clip_mask[0]);
            tr_r = vec_and(tr_r, clip_mask[1]);

            altivec_store<ALIGNED>(out_ptr, col_l, col_r, tr_l, tr_r);
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;

            vec8x16_t sel_l = vec_ld(char_byte_left, *selectors);
            vec8x16_t sel_r = vec_ld(char_byte_right, *selectors);
            vec32x4_t col_l = vec_vperm(char_colors, char_colors, sel_l);
            vec32x4_t col_r = vec_vperm(char_colors, char_colors, sel_r);
            vec32x4_t tr_l = clip_mask[0];
            vec32x4_t tr_r = clip_mask[1];

            altivec_store<ALIGNED>(out_ptr, col_l, col_r, tr_l, tr_r);
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

            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;

            vec8x16_t sel_l = vec_ld(char_byte_left, *selectors);
            vec8x16_t sel_r = vec_ld(char_byte_right, *selectors);
            vec32x4_t col_l = vec_vperm(char_colors, char_colors, sel_l);
            vec32x4_t col_r = vec_vperm(char_colors, char_colors, sel_r);
            vec32x4_t tr_l = vec_vperm(char_opaque, char_opaque, sel_l);
            vec32x4_t tr_r = vec_vperm(char_opaque, char_opaque, sel_r);

            altivec_store<ALIGNED>(out_ptr, col_l, col_r, tr_l, tr_r);
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;

            vec8x16_t sel_l = vec_ld(char_byte_left, *selectors);
            vec8x16_t sel_r = vec_ld(char_byte_right, *selectors);
            vec32x4_t col_l = vec_vperm(char_colors, char_colors, sel_l);
            vec32x4_t col_r = vec_vperm(char_colors, char_colors, sel_r);

            altivec_store<ALIGNED>(out_ptr, col_l, col_r);
          }
        }
      }
    }
  }
}

template<int ALIGNED, int SMZX, int TR>
static inline void render_layer32x4_altivec(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer32x4_altivec<ALIGNED, SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer32x4_altivec<ALIGNED, SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
  }
}

template<int ALIGNED, int SMZX>
static inline void render_layer32x4_altivec(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int tr, int clip)
{
  switch(tr)
  {
    case 0:
      render_layer32x4_altivec<ALIGNED, SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;

    case 1:
      render_layer32x4_altivec<ALIGNED, SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
  }
}

template<int ALIGNED>
boolean render_layer32x4_altivec(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  switch(smzx)
  {
    case 0:
      render_layer32x4_altivec<ALIGNED, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer32x4_altivec<ALIGNED, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;
  }
  return true;
}

boolean render_layer32x4_altivec(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  if(width_px & 7)
    return false;

#ifdef __VSX__
  if(!platform_has_altivec_vsx())
    return false;

  render_layer32x4_altivec<0>(
   pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
#else
  if(((size_t)((uint32_t *)pixels + layer->x) & 15) == 0 && (pitch & 15) == 0)
  {
    // Aligned non-VSX
    render_layer32x4_altivec<1>(
     pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
  }
  else
  {
    // Unaligned (TODO: is this even worth enabling?)
    render_layer32x4_altivec<0>(
     pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
  }
#endif
  return true;
}
#else /* !HAS_RENDER_LAYER32X4_ALTIVEC && __ALTIVEC__ */

boolean render_layer32x4_altivec(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  return false;
}

#endif
