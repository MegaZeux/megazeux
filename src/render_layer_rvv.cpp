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

/**
 * rvv:
 *
 * vrgather (select elements from register, equivalent to vtbl)
 * vmerge (select one of two inputs from mask, equivalent to vbsl/xor-and-xor)
 * vmand (mask-and for merging masks--is this faster than vand?)
 *
 * is it possible to write both with the same code?
 */

#if defined(HAS_RENDER_LAYER32X8_RVV) && defined(__linux__) && \
 defined(__riscv_vector) && defined(__riscv_v_min_vlen) && \
 __riscv_v_min_vlen >= 64

#include <riscv_vector.h>

// On the extreme off chance someone needs big endian, print an error.
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
#error RVV renderer is configured for little endian only--please report this!
#endif

static const uint32_t selectors_mzx[16][8] __attribute__((aligned(32)))=
{
  { 0, 0, 0, 0 },
  { 0, 0, 0, 1 },
  { 0, 0, 1, 0 },
  { 0, 0, 1, 1 },
  { 0, 1, 0, 0 },
  { 0, 1, 0, 1 },
  { 0, 1, 1, 0 },
  { 0, 1, 1, 1 },
  { 1, 0, 0, 0 },
  { 1, 0, 0, 1 },
  { 1, 0, 1, 0 },
  { 1, 0, 1, 1 },
  { 1, 1, 0, 0 },
  { 1, 1, 0, 1 },
  { 1, 1, 1, 0 },
  { 1, 1, 1, 1 },
};

static const uint32_t selectors_smzx[16][8] __attribute__((aligned(32))) =
{
  { 0, 0, 0, 0 },
  { 0, 0, 1, 1 },
  { 0, 0, 2, 2 },
  { 0, 0, 3, 3 },
  { 1, 1, 0, 0 },
  { 1, 1, 1, 1 },
  { 1, 1, 2, 2 },
  { 1, 1, 3, 3 },
  { 2, 2, 0, 0 },
  { 2, 2, 1, 1 },
  { 2, 2, 2, 2 },
  { 2, 2, 3, 3 },
  { 3, 3, 0, 0 },
  { 3, 3, 1, 1 },
  { 3, 3, 2, 2 },
  { 3, 3, 3, 3 },
};

template<typename VECTYPE>
class vec
{
public:
  static size_t setvlmax();
  static VECTYPE zero();
  static VECTYPE load_4x2(const uint32_t *src_l, const uint32_t *src_r);
  static VECTYPE load(const uint32_t *src);
  static void store(uint32_t *dest, VECTYPE val);
  static VECTYPE permute_colors(VECTYPE colors, VECTYPE selector);
  static VECTYPE bit_select(VECTYPE a, VECTYPE b, VECTYPE m);
  static VECTYPE bit_and(VECTYPE a, VECTYPE b);

  template<int SMZX>
  static VECTYPE get_selector(uint8_t char_byte)
  {
    auto &selectors = SMZX ? selectors_smzx : selectors_mzx;
    const uint32_t *src_l = selectors[char_byte >> 4];
    const uint32_t *src_r = selectors[char_byte & 0xf];
    return load_4x2(src_l, src_r);
  }

  static VECTYPE bit_select(VECTYPE a, VECTYPE b, VECTYPE m1, VECTYPE m2)
  {
    return bit_select(a, b, bit_and(m1, m2));
  }
};

template<>
size_t vec<vuint32m4_t>::setvlmax()
{
  return __riscv_vsetvlmax_e32m4();
}
template<>
size_t vec<vuint32m2_t>::setvlmax()
{
  return __riscv_vsetvlmax_e32m2();
}
template<>
size_t vec<vuint32m1_t>::setvlmax()
{
  return __riscv_vsetvlmax_e32m1();
}

template<>
vuint32m4_t vec<vuint32m4_t>::zero()
{
  return __riscv_vmv_s_x_u32m4(0, 8);
}
template<>
vuint32m2_t vec<vuint32m2_t>::zero()
{
  return __riscv_vmv_s_x_u32m2(0, 8);
}
template<>
vuint32m1_t vec<vuint32m1_t>::zero()
{
  return __riscv_vmv_s_x_u32m1(0, 8);
}

template<>
vuint32m4_t vec<vuint32m4_t>::load_4x2(const uint32_t *src_l, const uint32_t *src_r)
{
  vuint32m2_t vec_l = __riscv_vle32_v_u32m2(src_l, 4);
  vuint32m2_t vec_r = __riscv_vle32_v_u32m2(src_r, 4);
  return __riscv_vcreate_v_u32m2_u32m4(vec_l, vec_r);
}
template<>
vuint32m2_t vec<vuint32m2_t>::load_4x2(const uint32_t *src_l, const uint32_t *src_r)
{
  vuint32m1_t vec_l = __riscv_vle32_v_u32m1(src_l, 4);
  vuint32m1_t vec_r = __riscv_vle32_v_u32m1(src_r, 4);
  return __riscv_vcreate_v_u32m1_u32m2(vec_l, vec_r);
}
template<>
vuint32m1_t vec<vuint32m1_t>::load_4x2(const uint32_t *src_l, const uint32_t *src_r)
{
  vuint32m1_t vec_l = __riscv_vle32_v_u32m1(src_l, 4);
  vuint32m1_t vec_r = __riscv_vle32_v_u32m1(src_r, 4);
  return __riscv_vslideup_vx_u32m1(vec_l, vec_r, 4, 4);
}

template<>
vuint32m4_t vec<vuint32m4_t>::load(const uint32_t *src)
{
  return __riscv_vle32_v_u32m4(src, 8);
}
template<>
vuint32m2_t vec<vuint32m2_t>::load(const uint32_t *src)
{
  return __riscv_vle32_v_u32m2(src, 8);
}
template<>
vuint32m1_t vec<vuint32m1_t>::load(const uint32_t *src)
{
  return __riscv_vle32_v_u32m1(src, 8);
}

template<>
void vec<vuint32m4_t>::store(uint32_t *src, vuint32m4_t val)
{
  __riscv_vse32_v_u32m4(src, val, 8);
}
template<>
void vec<vuint32m2_t>::store(uint32_t *src, vuint32m2_t val)
{
  __riscv_vse32_v_u32m2(src, val, 8);
}
template<>
void vec<vuint32m1_t>::store(uint32_t *src, vuint32m1_t val)
{
  __riscv_vse32_v_u32m1(src, val, 8);
}

template<>
vuint32m4_t vec<vuint32m4_t>::permute_colors(vuint32m4_t colors, vuint32m4_t selector)
{
  return __riscv_vrgather_vv_u32m4(colors, selector, 8);
}
template<>
vuint32m2_t vec<vuint32m2_t>::permute_colors(vuint32m2_t colors, vuint32m2_t selector)
{
  return __riscv_vrgather_vv_u32m2(colors, selector, 8);
}
template<>
vuint32m1_t vec<vuint32m1_t>::permute_colors(vuint32m1_t colors, vuint32m1_t selector)
{
  return __riscv_vrgather_vv_u32m1(colors, selector, 8);
}

// could possibly use the char byte directly for mzx??
template<>
vuint32m4_t vec<vuint32m4_t>::bit_select(vuint32m4_t a, vuint32m4_t b, vuint32m4_t m)
{
  return __riscv_vmerge_vvm_u32m4(a, b,
   __riscv_vmseq_vx_u32m4_b8(m, 0xffffffffu, 8), 8);
}
template<>
vuint32m2_t vec<vuint32m2_t>::bit_select(vuint32m2_t a, vuint32m2_t b, vuint32m2_t m)
{
  return __riscv_vmerge_vvm_u32m2(a, b,
   __riscv_vmseq_vx_u32m2_b16(m, 0xffffffffu, 8), 8);
}
template<>
vuint32m1_t vec<vuint32m1_t>::bit_select(vuint32m1_t a, vuint32m1_t b, vuint32m1_t m)
{
  return __riscv_vmerge_vvm_u32m1(a, b,
   __riscv_vmseq_vx_u32m1_b32(m, 0xffffffffu, 8), 8);
}

template<>
vuint32m4_t vec<vuint32m4_t>::bit_and(vuint32m4_t a, vuint32m4_t b)
{
  return __riscv_vand_vv_u32m4(a, b, 8);
}
template<>
vuint32m2_t vec<vuint32m2_t>::bit_and(vuint32m2_t a, vuint32m2_t b)
{
  return __riscv_vand_vv_u32m2(a, b, 8);
}
template<>
vuint32m1_t vec<vuint32m1_t>::bit_and(vuint32m1_t a, vuint32m1_t b)
{
  return __riscv_vand_vv_u32m1(a, b, 8);
}

template<typename VECTYPE, int SMZX, int TR, int CLIP>
static inline void render_layer32x8_rvv(
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

  using v = vec<VECTYPE>;

  uint32_t left_mask[8];
  uint32_t right_mask[8];
  uint32_t char_colors[8] = { 0 };
  uint32_t char_opaque[8] = { 0 };
  VECTYPE vec_colors = v::zero();
  VECTYPE vec_opaque = v::zero();
  VECTYPE vec_clip = v::zero();

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
          left_mask[i] = 0;
          right_mask[i] = 0xffffffffu;
        }
        for(; i < CHAR_W; i++)
        {
          left_mask[i] = 0xffffffffu;
          right_mask[i] = 0;
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
        vec_colors = v::load(char_colors);
        if(TR)
          vec_opaque = v::load(char_opaque);
      }

      if(TR && all_tcol)
        continue;

      out_ptr = dest_ptr;
      char_ptr = graphics->charset + ch * CHAR_H;

      if(CLIP && (x == clip_xl || x == clip_xr))
      {
        uint32_t (&clip_mask)[8] = x == clip_xl ? left_mask : right_mask;
        vec_clip = v::load(clip_mask);

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

            VECTYPE vec_sel = v::template get_selector<SMZX>(char_ptr[i]);
            uint32_t sel[8];
            v::store(sel, vec_sel);

            int j;
            if(TR && has_tcol && byte_tcol == char_ptr[i])
              continue;

            for(j = 0; j < 8; j++)
            {
              if(clip_mask[j])
                if(!TR || !has_tcol || char_opaque[sel[j]])
                  out_ptr[j] = char_colors[sel[j]];
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

            VECTYPE sel = v::template get_selector<SMZX>(char_ptr[i]);
            VECTYPE col = v::permute_colors(vec_colors, sel);
            VECTYPE tr = v::permute_colors(vec_opaque, sel);
            VECTYPE bg = v::load(out_ptr);

            col = v::bit_select(bg, col, tr, vec_clip);
            v::store(out_ptr, col);
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            VECTYPE sel = v::template get_selector<SMZX>(char_ptr[i]);
            VECTYPE col = v::permute_colors(vec_colors, sel);
            VECTYPE bg = v::load(out_ptr);

            col = v::bit_select(bg, col, vec_clip);
            v::store(out_ptr, col);
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

            VECTYPE sel = v::template get_selector<SMZX>(char_ptr[i]);
            VECTYPE col = v::permute_colors(vec_colors, sel);
            VECTYPE tr = v::permute_colors(vec_opaque, sel);
            VECTYPE bg = v::load(out_ptr);

            col = v::bit_select(bg, col, tr);
            v::store(out_ptr, col);
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

            VECTYPE sel = v::template get_selector<SMZX>(char_ptr[i]);
            VECTYPE col = v::permute_colors(vec_colors, sel);
            v::store(out_ptr, col);
          }
        }
      }
    }
  }
}

template<typename VECTYPE, int SMZX, int TR>
static inline void render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer32x8_rvv<VECTYPE, SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer32x8_rvv<VECTYPE, SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
  }
}

template<typename VECTYPE, int SMZX>
static inline void render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int tr, int clip)
{
  switch(tr)
  {
    case 0:
      render_layer32x8_rvv<VECTYPE, SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;

    case 1:
      render_layer32x8_rvv<VECTYPE, SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
  }
}

template<typename VECTYPE>
static inline boolean render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  switch(smzx)
  {
    case 0:
      render_layer32x8_rvv<VECTYPE, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer32x8_rvv<VECTYPE, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;
  }
  return true;
}

boolean render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  if(width_px & 7)
    return false;

  if(vec<vuint32m1_t>::setvlmax() >= 8)
  {
    render_layer32x8_rvv<vuint32m1_t>(
     pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
  }
  else

  if(vec<vuint32m2_t>::setvlmax() >= 8)
  {
    render_layer32x8_rvv<vuint32m2_t>(
     pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
  }
  else

  if(vec<vuint32m4_t>::setvlmax() >= 8)
  {
    render_layer32x8_rvv<vuint32m4_t>(
     pixels, width_px, height_px, pitch, graphics, layer, smzx, tr, clip);
  }
  else
    return false;

  return true;
}

#else /* !HAS_RENDER_LAYER32X8_RVV || !__riscv_vector */

boolean render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  return false;
}

#endif
