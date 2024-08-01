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

#ifdef __aarch64__
#define RENDER_VQTBL
#endif

#ifdef HAS_RENDER_LAYER32X4_NEON
#include <arm_neon.h>

// Both set_colors and selectors are set up for little endian only.
// On the extreme off chance someone needs big endian, print an error.
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
#error Neon renderer is configured for little endian only--please report this!
#endif

#ifdef RENDER_VQTBL
#define SELECT_0 0, 1, 2, 3
#define SELECT_1 4, 5, 6, 7
#define SELECT_2 8, 9, 10, 11
#define SELECT_3 12, 13, 14, 15

static const uint8x16_t selectors_mzx[16] =
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

static const uint8x16_t selectors_smzx[16] =
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

template<int SMZX>
static inline uint8x16_t get_selector(unsigned half_char)
{
  const uint8x16_t (&selectors)[16] = SMZX ? selectors_smzx : selectors_mzx;
  return selectors[half_char];
}

static inline uint32x4_t permute_colors(uint32x4_t colors, uint8x16_t selector)
{
  return vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(colors), selector));
}

#else /* !RENDER_VQTBL */

static inline void set_colors_mzx_neon(uint32x4_t * RESTRICT dest,
 uint32x4_t colors)
{
#ifdef __aarch64
  uint32x4_t x0000 = vdupq_laneq_u32(colors, 0);
  uint32x4_t x1111 = vdupq_laneq_u32(colors, 1);
  uint32x4_t x1010 = vreinterpretq_u32_u64(
                      vdupq_laneq_u64(vreinterpretq_u64_u32(colors), 0));
#else
  uint32x4_t x0000 = vdupq_lane_u32(vget_low_u32(colors), 0);
  uint32x4_t x1111 = vdupq_lane_u32(vget_low_u32(colors), 1);
#endif
  uint32x4_t x1010 = vreinterpretq_u32_u64(
                      vdupq_lane_u64(
                       vget_low_u64(vreinterpretq_u64_u32(colors)), 0));

  uint32x4_t x0101 = vzipq_u32(x1111, x0000).val[0];

  dest[0]  = x0000;
  dest[1]  = vzipq_u32(x0000, x1010).val[0];
  dest[2]  = vzipq_u32(x1010, x0000).val[0];
  dest[3]  = vzipq_u32(x1010, x1010).val[0];
  dest[4]  = vzipq_u32(x0000, x0101).val[0];
  dest[5]  = x1010;
  dest[6]  = vzipq_u32(x1010, x0101).val[0];
  dest[7]  = vzipq_u32(x1010, x1111).val[0];
  dest[8]  = vzipq_u32(x0101, x0000).val[0];
  dest[9]  = vzipq_u32(x0101, x1010).val[0];
  dest[10] = x0101;
  dest[11] = vzipq_u32(x1111, x1010).val[0];
  dest[12] = vzipq_u32(x0101, x0101).val[0];
  dest[13] = vzipq_u32(x0101, x1111).val[0];
  dest[14] = vzipq_u32(x1111, x0101).val[0];
  dest[15] = x1111;
}

static inline void set_colors_smzx_neon(uint32x4_t * RESTRICT dest,
 uint32x4_t colors)
{
  uint64x1_t x00 = vreinterpret_u64_u32(vdup_lane_u32(vget_low_u32(colors), 0));
  uint64x1_t x11 = vreinterpret_u64_u32(vdup_lane_u32(vget_low_u32(colors), 1));
  uint64x1_t x22 = vreinterpret_u64_u32(vdup_lane_u32(vget_high_u32(colors), 0));
  uint64x1_t x33 = vreinterpret_u64_u32(vdup_lane_u32(vget_high_u32(colors), 1));

  dest[0]  = vreinterpretq_u32_u64(vcombine_u64(x00, x00));
  dest[1]  = vreinterpretq_u32_u64(vcombine_u64(x00, x11));
  dest[2]  = vreinterpretq_u32_u64(vcombine_u64(x00, x22));
  dest[3]  = vreinterpretq_u32_u64(vcombine_u64(x00, x33));
  dest[4]  = vreinterpretq_u32_u64(vcombine_u64(x11, x00));
  dest[5]  = vreinterpretq_u32_u64(vcombine_u64(x11, x11));
  dest[6]  = vreinterpretq_u32_u64(vcombine_u64(x11, x22));
  dest[7]  = vreinterpretq_u32_u64(vcombine_u64(x11, x33));
  dest[8]  = vreinterpretq_u32_u64(vcombine_u64(x22, x00));
  dest[9]  = vreinterpretq_u32_u64(vcombine_u64(x22, x11));
  dest[10] = vreinterpretq_u32_u64(vcombine_u64(x22, x22));
  dest[11] = vreinterpretq_u32_u64(vcombine_u64(x22, x33));
  dest[12] = vreinterpretq_u32_u64(vcombine_u64(x33, x00));
  dest[13] = vreinterpretq_u32_u64(vcombine_u64(x33, x11));
  dest[14] = vreinterpretq_u32_u64(vcombine_u64(x33, x22));
  dest[15] = vreinterpretq_u32_u64(vcombine_u64(x33, x33));
}
#endif /* !RENDER_VQTBL */

template<int SMZX, int TR, int CLIP>
static inline void render_layer32x4_neon(
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

#ifndef RENDER_VQTBL
  uint32x4_t set_colors[16];
  uint32x4_t set_opaque[16];
#endif
  uint32x4_t left_mask[2];
  uint32x4_t right_mask[2];
  uint32x4_t char_colors = { 0, 0, 0, 0 };
  uint32x4_t char_opaque = { 0, 0, 0, 0 };

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
#ifndef RENDER_VQTBL
          set_colors_smzx_neon(set_colors, char_colors);
          if(TR && has_tcol)
            set_colors_smzx_neon(set_opaque, char_opaque);
#endif
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
#ifndef RENDER_VQTBL
          set_colors_mzx_neon(set_colors, char_colors);
          if(TR && has_tcol)
            set_colors_mzx_neon(set_opaque, char_opaque);
#endif
        }
      }

      if(TR && all_tcol)
        continue;

      out_ptr = dest_ptr;
      char_ptr = graphics->charset + ch * CHAR_H;

      if(CLIP && (x == clip_xl || x == clip_xr))
      {
        uint32x4_t (&clip_mask)[2] = x == clip_xl ? left_mask : right_mask;

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

            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

#ifdef RENDER_VQTBL
            uint8x16_t sel_l = get_selector<SMZX>(char_byte_left);
            uint8x16_t sel_r = get_selector<SMZX>(char_byte_right);
            uint32x4_t col_l = permute_colors(char_colors, sel_l);
            uint32x4_t col_r = permute_colors(char_colors, sel_r);
#else
            uint32x4_t col_l = set_colors[char_byte_left];
            uint32x4_t col_r = set_colors[char_byte_right];
#endif
            uint32x4_t tr_l = { 0, 0, 0, 0 };
            uint32x4_t tr_r = { 0, 0, 0, 0 };
            int j;
            if(TR && has_tcol)
            {
              if(byte_tcol == char_ptr[i])
                continue;

#ifdef RENDER_VQTBL
              tr_l = permute_colors(char_opaque, sel_l);
              tr_r = permute_colors(char_opaque, sel_r);
#else
              tr_l = set_opaque[char_byte_left];
              tr_r = set_opaque[char_byte_right];
#endif
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

            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

#ifdef RENDER_VQTBL
            uint8x16_t sel_l = get_selector<SMZX>(char_byte_left);
            uint8x16_t sel_r = get_selector<SMZX>(char_byte_right);
            uint32x4_t col_l = permute_colors(char_colors, sel_l);
            uint32x4_t col_r = permute_colors(char_colors, sel_r);
            uint32x4_t tr_l = permute_colors(char_opaque, sel_l);
            uint32x4_t tr_r = permute_colors(char_opaque, sel_r);
#else
            uint32x4_t col_l = set_colors[char_byte_left];
            uint32x4_t col_r = set_colors[char_byte_right];
            uint32x4_t tr_l = set_opaque[char_byte_left];
            uint32x4_t tr_r = set_opaque[char_byte_right];
#endif
            uint32x4_t bg_l = vld1q_u32(out_ptr);
            uint32x4_t bg_r = vld1q_u32(out_ptr + 4);

            tr_l = vandq_u32(tr_l, clip_mask[0]);
            tr_r = vandq_u32(tr_r, clip_mask[1]);

            vst1q_u32(out_ptr,      vbslq_u32(tr_l, col_l, bg_l));
            vst1q_u32(out_ptr + 4,  vbslq_u32(tr_r, col_r, bg_r));
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

#ifdef RENDER_VQTBL
            uint8x16_t sel_l = get_selector<SMZX>(char_byte_left);
            uint8x16_t sel_r = get_selector<SMZX>(char_byte_right);
            uint32x4_t col_l = permute_colors(char_colors, sel_l);
            uint32x4_t col_r = permute_colors(char_colors, sel_r);
#else
            uint32x4_t col_l = set_colors[char_byte_left];
            uint32x4_t col_r = set_colors[char_byte_right];
#endif
            uint32x4_t tr_l = clip_mask[0];
            uint32x4_t tr_r = clip_mask[1];
            uint32x4_t bg_l = vld1q_u32(out_ptr);
            uint32x4_t bg_r = vld1q_u32(out_ptr + 4);

            vst1q_u32(out_ptr,      vbslq_u32(tr_l, col_l, bg_l));
            vst1q_u32(out_ptr + 4,  vbslq_u32(tr_r, col_r, bg_r));
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

            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

#ifdef RENDER_VQTBL
            uint8x16_t sel_l = get_selector<SMZX>(char_byte_left);
            uint8x16_t sel_r = get_selector<SMZX>(char_byte_right);
            uint32x4_t col_l = permute_colors(char_colors, sel_l);
            uint32x4_t col_r = permute_colors(char_colors, sel_r);
            uint32x4_t tr_l = permute_colors(char_opaque, sel_l);
            uint32x4_t tr_r = permute_colors(char_opaque, sel_r);
#else
            uint32x4_t col_l = set_colors[char_byte_left];
            uint32x4_t col_r = set_colors[char_byte_right];
            uint32x4_t tr_l = set_opaque[char_byte_left];
            uint32x4_t tr_r = set_opaque[char_byte_right];
#endif
            uint32x4_t bg_l = vld1q_u32(out_ptr);
            uint32x4_t bg_r = vld1q_u32(out_ptr + 4);

            vst1q_u32(out_ptr,      vbslq_u32(tr_l, col_l, bg_l));
            vst1q_u32(out_ptr + 4,  vbslq_u32(tr_r, col_r, bg_r));
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

#ifdef RENDER_VQTBL
            uint8x16_t sel_l = get_selector<SMZX>(char_byte_left);
            uint8x16_t sel_r = get_selector<SMZX>(char_byte_right);
            uint32x4_t col_l = permute_colors(char_colors, sel_l);
            uint32x4_t col_r = permute_colors(char_colors, sel_r);
#else
            uint32x4_t col_l = set_colors[char_byte_left];
            uint32x4_t col_r = set_colors[char_byte_right];
#endif
            vst1q_u32(out_ptr,      col_l);
            vst1q_u32(out_ptr + 4,  col_r);
          }
        }
      }
    }
  }
}

template<int SMZX, int TR>
static inline void render_layer32x4_neon(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer32x4_neon<SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer32x4_neon<SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
  }
}

template<int SMZX>
static inline void render_layer32x4_neon(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int tr, int clip)
{
  switch(tr)
  {
    case 0:
      render_layer32x4_neon<SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;

    case 1:
      render_layer32x4_neon<SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
  }
}

boolean render_layer32x4_neon(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  if(width_px & 7)
    return false;

  switch(smzx)
  {
    case 0:
      render_layer32x4_neon<0>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer32x4_neon<1>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;
  }
  return true;
}

#else /* !HAS_RENDER_LAYER32X4_NEON */

boolean render_layer32x4_neon(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  return false;
}

#endif
