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

/* TODO: compare performance of these two implementations,
 * both on newer hardware and on older. Newer might be moot
 * due to AVX having better masked write instructions.
 * i5-6200u: maskmovdqu is HORRIBLY slow. */
//#define USE_MASKMOVDQU

#ifdef DEBUG
#include "util.h"
#endif

#if defined(__x86_64__) || defined(__SSE2__) || \
 defined(_M_AMD64) || defined(_M_X64) || \
 (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define USE_INTRINSICS
#include <emmintrin.h>
#include <mmintrin.h>
#endif

#if defined(HAS_RENDER_LAYER32X4_SSE2) && \
 (defined(USE_INTRINSICS) || defined(__GNUC__))

union xmm
{
#ifdef USE_INTRINSICS
  __m128i vec;
#endif
  uint32_t values[4];
};

static inline void set_colors_mzx_sse2(xmm * RESTRICT dest, xmm colors)
{
  // The top qword may be uninitialized, so set these manually.
  // Autovectorization correctly optimizes this to PUNPCKLQDQ.
  colors.values[2] = colors.values[0];
  colors.values[3] = colors.values[1];
#ifdef USE_INTRINSICS
  __m128i x1010 = colors.vec;
  __m128i x0000 = _mm_shuffle_epi32(x1010, 0x00);
  __m128i x1111 = _mm_shuffle_epi32(x1010, 0x55);
  __m128i x0101 = _mm_unpacklo_epi32(x1111, x0000);
  __m128i *d128 = reinterpret_cast<__m128i *>(dest);

  d128[0]  = x0000;
  d128[1]  = _mm_unpacklo_epi64(x0000, x1010);
  d128[2]  = _mm_unpacklo_epi64(x0000, x0101);
  d128[3]  = _mm_unpacklo_epi64(x0000, x1111);
  d128[4]  = _mm_unpacklo_epi64(x1010, x0000);
  d128[5]  = x1010;
  d128[6]  = _mm_unpacklo_epi64(x1010, x0101);
  d128[7]  = _mm_unpacklo_epi64(x1010, x1111);
  d128[8]  = _mm_unpacklo_epi64(x0101, x0000);
  d128[9]  = _mm_unpacklo_epi64(x0101, x1010);
  d128[10] = x0101;
  d128[11] = _mm_unpacklo_epi64(x0101, x1111);
  d128[12] = _mm_unpacklo_epi64(x1111, x0000);
  d128[13] = _mm_unpacklo_epi64(x1111, x1010);
  d128[14] = _mm_unpacklo_epi64(x1111, x0101);
  d128[15] = x1111;
#else
  /* pshufd is nicer but has more latency on old systems. */
  asm(
    "movdqa %1, %%xmm2"             "\n\t"  // 1010 (5)
    "pshufd $0x00, %%xmm2, %%xmm0"  "\n\t"  // 0000 (0)
    "movdqa %%xmm2, %%xmm1"         "\n\t"
    "movdqa %%xmm0, 0x00(%0)"       "\n\t"
    "movdqa %%xmm2, 0x50(%0)"       "\n\t"
    "pshufd $0x55, %%xmm2, %%xmm3"  "\n\t"  // 1111 (f)
    "movdqa %%xmm3, %%xmm1"         "\n\t"
    "movdqa %%xmm3, 0xf0(%0)"       "\n\t"
    "punpckldq %%xmm0, %%xmm1"      "\n\t"  // 0101 (a)
    "movdqa %%xmm1, 0xa0(%0)"       "\n\t"

    "punpcklqdq %%xmm2, %%xmm0"     "\n\t"  // 1000 (1)
    "movdqa %%xmm0, 0x10(%0)"       "\n\t"
    "punpcklqdq %%xmm1, %%xmm0"     "\n\t"  // 0100 (2)
    "movdqa %%xmm0, 0x20(%0)"       "\n\t"
    "punpcklqdq %%xmm3, %%xmm0"     "\n\t"  // 1100 (3)
    "movdqa %%xmm0, 0x30(%0)"       "\n\t"
    "punpcklqdq %%xmm0, %%xmm2"     "\n\t"  // 0010 (4)
    "movdqa %%xmm2, 0x40(%0)"       "\n\t"

    "punpcklqdq %%xmm1, %%xmm2"     "\n\t"  // 0110 (6)
    "movdqa %%xmm2, 0x60(%0)"       "\n\t"
    "punpcklqdq %%xmm3, %%xmm2"     "\n\t"  // 1110 (7)
    "movdqa %%xmm2, 0x70(%0)"       "\n\t"
    "punpcklqdq %%xmm0, %%xmm1"     "\n\t"  // 0001 (8)
    "movdqa %%xmm1, 0x80(%0)"       "\n\t"
    "punpcklqdq %%xmm2, %%xmm1"     "\n\t"  // 1001 (9)
    "movdqa %%xmm1, 0x90(%0)"       "\n\t"

    "punpcklqdq %%xmm3, %%xmm1"     "\n\t"  // 1101 (b)
    "movdqa %%xmm1, 0xb0(%0)"       "\n\t"
    "punpcklqdq %%xmm0, %%xmm3"     "\n\t"  // 0011 (c)
    "movdqa %%xmm3, 0xc0(%0)"       "\n\t"
    "punpcklqdq %%xmm2, %%xmm3"     "\n\t"  // 1011 (d)
    "movdqa %%xmm3, 0xd0(%0)"       "\n\t"
    "punpcklqdq %%xmm1, %%xmm3"     "\n\t"  // 0111 (e)
    "movdqa %%xmm3, 0xe0(%0)"       "\n\t"
    :
    : "r" (dest), "x" (colors)
    : "xmm0", "xmm1", "xmm2", "xmm3"
  );
#endif
}

static inline void set_colors_smzx_sse2(xmm * RESTRICT dest, xmm colors)
{
#ifdef USE_INTRINSICS
  __m128i x0123 = colors.vec;
  __m128i x0000 = _mm_shuffle_epi32(x0123, 0x00);
  __m128i x1111 = _mm_shuffle_epi32(x0123, 0x55);
  __m128i x2222 = _mm_shuffle_epi32(x0123, 0xaa);
  __m128i x3333 = _mm_shuffle_epi32(x0123, 0xff);
  __m128i *d128 = reinterpret_cast<__m128i *>(dest);

  d128[0]  = x0000;
  d128[1]  = _mm_unpacklo_epi64(x0000, x1111);
  d128[2]  = _mm_unpacklo_epi64(x0000, x2222);
  d128[3]  = _mm_unpacklo_epi64(x0000, x3333);
  d128[4]  = _mm_unpacklo_epi64(x1111, x0000);
  d128[5]  = x1111;
  d128[6]  = _mm_unpacklo_epi64(x1111, x2222);
  d128[7]  = _mm_unpacklo_epi64(x1111, x3333);
  d128[8]  = _mm_unpacklo_epi64(x2222, x0000);
  d128[9]  = _mm_unpacklo_epi64(x2222, x1111);
  d128[10] = x2222;
  d128[11] = _mm_unpacklo_epi64(x2222, x3333);
  d128[12] = _mm_unpacklo_epi64(x3333, x0000);
  d128[13] = _mm_unpacklo_epi64(x3333, x1111);
  d128[14] = _mm_unpacklo_epi64(x3333, x2222);
  d128[15] = x3333;
#else
  // TODO: punpcklqdq
  dest += 8;
  asm(
    "movdqa %1, %%xmm8"             "\n\t"
    "pshufd $0x00, %%xmm8, %%xmm0"  "\n\t"
    "pshufd $0x50, %%xmm8, %%xmm1"  "\n\t"
    "pshufd $0xa0, %%xmm8, %%xmm2"  "\n\t"
    "pshufd $0xf0, %%xmm8, %%xmm3"  "\n\t"
    "pshufd $0x05, %%xmm8, %%xmm4"  "\n\t"
    "pshufd $0x55, %%xmm8, %%xmm5"  "\n\t"
    "pshufd $0xa5, %%xmm8, %%xmm6"  "\n\t"
    "pshufd $0xf5, %%xmm8, %%xmm7"  "\n\t"
    "movdqa %%xmm0, -128(%0)"       "\n\t"
    "movdqa %%xmm1, -112(%0)"       "\n\t"
    "movdqa %%xmm2,  -96(%0)"       "\n\t"
    "movdqa %%xmm3,  -80(%0)"       "\n\t"
    "movdqa %%xmm4,  -64(%0)"       "\n\t"
    "movdqa %%xmm5,  -48(%0)"       "\n\t"
    "movdqa %%xmm6,  -32(%0)"       "\n\t"
    "movdqa %%xmm7,  -16(%0)"       "\n\t"
    "pshufd $0x0a, %%xmm8, %%xmm0"  "\n\t"
    "pshufd $0x5a, %%xmm8, %%xmm1"  "\n\t"
    "pshufd $0xaa, %%xmm8, %%xmm2"  "\n\t"
    "pshufd $0xfa, %%xmm8, %%xmm3"  "\n\t"
    "pshufd $0x0f, %%xmm8, %%xmm4"  "\n\t"
    "pshufd $0x5f, %%xmm8, %%xmm5"  "\n\t"
    "pshufd $0xaf, %%xmm8, %%xmm6"  "\n\t"
    "pshufd $0xff, %%xmm8, %%xmm7"  "\n\t"
    "movdqa %%xmm0,   0(%0)"        "\n\t"
    "movdqa %%xmm1,  16(%0)"        "\n\t"
    "movdqa %%xmm2,  32(%0)"        "\n\t"
    "movdqa %%xmm3,  48(%0)"        "\n\t"
    "movdqa %%xmm4,  64(%0)"        "\n\t"
    "movdqa %%xmm5,  80(%0)"        "\n\t"
    "movdqa %%xmm6,  96(%0)"        "\n\t"
    "movdqa %%xmm7, 112(%0)"        "\n\t"

    :
    : "r" (dest), "x" (colors)
    : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
      "xmm5", "xmm6", "xmm7", "xmm8"
  );
#endif
}

template<int SMZX, int TR, int CLIP>
static inline void render_layer32x4_sse2(
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

#ifdef USE_INTRINSICS
  xmm set_colors[16];
  xmm set_opaque[16];
  xmm left_mask[2];
  xmm right_mask[2];
#else
  uint32_t set_colors_unaligned[(16 * 4 * 2) + (2 * 4 * 2) + 16];
  size_t set_colors_offset = 16 -
   (((reinterpret_cast<size_t>(set_colors_unaligned)) >> 2) & 15);
  xmm *set_colors =
   reinterpret_cast<xmm *>(set_colors_unaligned + set_colors_offset);
  xmm *set_opaque = set_colors + 16;
  xmm *left_mask = set_opaque + 16;
  xmm *right_mask = left_mask + 2;
#endif

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
  // ssize_t to force matching registers with set_colors/set_opaque.
  ssize_t char_byte_left;
  ssize_t char_byte_right;

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
          left_mask[i >> 2].values[i & 3] = 0;
          right_mask[i >> 2].values[i & 3] = 0xffffffffu;
        }
        for(; i < CHAR_W; i++)
        {
          left_mask[i >> 2].values[i & 3] = 0xffffffffu;
          right_mask[i >> 2].values[i & 3] = 0;
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

      if(prev != ((uint16_t *)src)[1])
      {
        xmm char_colors;
        xmm char_masks;

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
              char_masks.values[i] = idx == tcol ? 0 : 0xffffffffu;
            }
          }
          set_colors_smzx_sse2(set_colors, char_colors);
          if(TR && has_tcol)
            set_colors_smzx_sse2(set_opaque, char_masks);
        }
        else
        {
          int bg = select_color_16(src->bg_color, ppal);
          int fg = select_color_16(src->fg_color, ppal);
          char_colors.values[0] = graphics->flat_intensity_palette[bg];
          char_colors.values[1] = graphics->flat_intensity_palette[fg];
          set_colors_mzx_sse2(set_colors, char_colors);

          if(TR)
          {
            has_tcol = bg == tcol || fg == tcol;
            all_tcol = bg == tcol && fg == tcol;
            byte_tcol = bg == tcol ? 0 : 0xff;

            if(has_tcol)
            {
              char_masks.values[0] = (bg == tcol) ? 0 : 0xffffffffu;
              char_masks.values[1] = (fg == tcol) ? 0 : 0xffffffffu;
              set_colors_mzx_sse2(set_opaque, char_masks);
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
        xmm *clip_mask = x == clip_xl ? left_mask : right_mask;

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

            xmm left = set_colors[char_ptr[i] >> 4];
            xmm right = set_colors[char_ptr[i] & 0x0f];
            xmm left_op{};
            xmm right_op{};
            int j;
            if(TR && has_tcol)
            {
              if(byte_tcol == char_ptr[i])
                continue;

              left_op = set_opaque[char_ptr[i] >> 4];
              right_op = set_opaque[char_ptr[i] & 0xf];
            }
            for(j = 0; j < 4; j++)
            {
              if(clip_mask[0].values[j])
                if(!TR || !has_tcol || left_op.values[j])
                  out_ptr[j] = left.values[j];
            }
            for(j = 0; j < 4; j++)
            {
              if(clip_mask[1].values[j])
                if(!TR || !has_tcol || right_op.values[j])
                  out_ptr[j + 4] = right.values[j];
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

#ifdef USE_MASKMOVDQU
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqa     (%3, %1, 1), %%xmm0"  "\n\t"
              "movdqa     (%3, %2, 1), %%xmm1"  "\n\t"
              "movdqa     (%4, %1, 1), %%xmm2"  "\n\t"
              "movdqa     (%4, %2, 1), %%xmm3"  "\n\t"
              "pand        0(%5), %%xmm2"       "\n\t"
              "pand       16(%5), %%xmm3"       "\n\t"
              "mov        %0, %%rdi"            "\n\t"
              "maskmovdqu %%xmm2, %%xmm0"       "\n\t"
              "lea        16(%0), %%rdi"        "\n\t"
              "maskmovdqu %%xmm3, %%xmm1"       "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(set_opaque),
                "r"(clip_mask)
              : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "rdi"
            );
#elif defined(USE_INTRINSICS)
            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

            __m128i_u *dest = reinterpret_cast<__m128i_u *>(out_ptr);
            __m128i col_l = _mm_load_si128(&set_colors[char_byte_left].vec);
            __m128i col_r = _mm_load_si128(&set_colors[char_byte_right].vec);
            __m128i tr_l = _mm_load_si128(&set_opaque[char_byte_left].vec);
            __m128i tr_r = _mm_load_si128(&set_opaque[char_byte_right].vec);
            __m128i bg_l = _mm_loadu_si128(dest);
            __m128i bg_r = _mm_loadu_si128(dest + 1);
            tr_l &= _mm_load_si128(&clip_mask[0].vec);
            tr_r &= _mm_load_si128(&clip_mask[1].vec);

            _mm_storeu_si128(dest,     (col_l & tr_l) | (bg_l & ~tr_l));
            _mm_storeu_si128(dest + 1, (col_r & tr_r) | (bg_r & ~tr_r));
#else
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqu   0(%0), %%xmm2"      "\n\t" // load image
              "movdqu  16(%0), %%xmm3"      "\n\t"
              "movdqa  %%xmm2, %%xmm0"      "\n\t"
              "movdqa  %%xmm3, %%xmm1"      "\n\t"
              "pxor    (%3, %1, 1), %%xmm0" "\n\t" // image ^ colors
              "pxor    (%3, %2, 1), %%xmm1" "\n\t"
              "pand     0(%5), %%xmm0"      "\n\t" // & clip_mask
              "pand    16(%5), %%xmm1"      "\n\t"
              "pand    (%4, %1, 1), %%xmm0" "\n\t" // & opaque
              "pand    (%4, %2, 1), %%xmm1" "\n\t" // off = 0, on = image ^ colors
              "pxor    %%xmm2, %%xmm0"      "\n\t" // ^ image
              "pxor    %%xmm3, %%xmm1"      "\n\t" // off = image, on = colors
              "movdqu  %%xmm0,  0(%0)"      "\n\t"
              "movdqu  %%xmm1, 16(%0)"      "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(set_opaque),
                "r"(clip_mask)
              : "xmm0", "xmm1", "xmm2", "xmm3"
            );
#endif
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

#ifdef USE_MASKMOVDQU
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqa     (%3, %1, 1), %%xmm0"  "\n\t"
              "movdqa     (%3, %2, 1), %%xmm1"  "\n\t"
              "movdqa      0(%4), %%xmm2"       "\n\t"
              "movdqa     16(%4), %%xmm3"       "\n\t"
              "mov        %0, %%rdi"            "\n\t"
              "maskmovdqu %%xmm2, %%xmm0"       "\n\t"
              "lea        16(%0), %%rdi"        "\n\t"
              "maskmovdqu %%xmm3, %%xmm1"       "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(clip_mask)
              : "xmm0", "xmm1", "xmm1", "xmm2", "rdi"
            );
#elif defined(USE_INTRINSICS)
            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

            __m128i_u *dest = reinterpret_cast<__m128i_u *>(out_ptr);
            __m128i col_l = _mm_load_si128(&set_colors[char_byte_left].vec);
            __m128i col_r = _mm_load_si128(&set_colors[char_byte_right].vec);
            __m128i tr_l = _mm_load_si128(&clip_mask[0].vec);
            __m128i tr_r = _mm_load_si128(&clip_mask[1].vec);
            __m128i bg_l = _mm_loadu_si128(dest);
            __m128i bg_r = _mm_loadu_si128(dest + 1);

            _mm_storeu_si128(dest,     (col_l & tr_l) | (bg_l & ~tr_l));
            _mm_storeu_si128(dest + 1, (col_r & tr_r) | (bg_r & ~tr_r));
#else
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqu   0(%0), %%xmm2"      "\n\t" // load image
              "movdqu  16(%0), %%xmm3"      "\n\t"
              "movdqa  %%xmm2, %%xmm0"      "\n\t"
              "movdqa  %%xmm3, %%xmm1"      "\n\t"
              "pxor    (%3, %1, 1), %%xmm0" "\n\t" // image ^ colors
              "pxor    (%3, %2, 1), %%xmm1" "\n\t"
              "pand     0(%4), %%xmm0"      "\n\t" // & clip_mask
              "pand    16(%4), %%xmm1"      "\n\t" // off = 0, on = image ^ colors
              "pxor    %%xmm2, %%xmm0"      "\n\t" // ^ image
              "pxor    %%xmm3, %%xmm1"      "\n\t" // off = image, on = colors
              "movdqu  %%xmm0,  0(%0)"      "\n\t"
              "movdqu  %%xmm1, 16(%0)"      "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(clip_mask)
              : "xmm0", "xmm1", "xmm2", "xmm3"
            );
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

#ifdef USE_MASKMOVDQU
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqa     (%3, %1, 1), %%xmm0"  "\n\t"
              "movdqa     (%3, %2, 1), %%xmm1"  "\n\t"
              "movdqa     (%4, %1, 1), %%xmm2"  "\n\t"
              "movdqa     (%4, %2, 1), %%xmm3"  "\n\t"
              "mov        %0, %%rdi"            "\n\t"
              "maskmovdqu %%xmm2, %%xmm0"       "\n\t"
              "lea        16(%0), %%rdi"        "\n\t"
              "maskmovdqu %%xmm3, %%xmm1"       "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(set_opaque)
              : "xmm0", "xmm1", "xmm2", "xmm3", "rdi"
            );
#elif defined(USE_INTRINSICS)
            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

            __m128i_u *dest = reinterpret_cast<__m128i_u *>(out_ptr);
            __m128i col_l = _mm_load_si128(&set_colors[char_byte_left].vec);
            __m128i col_r = _mm_load_si128(&set_colors[char_byte_right].vec);
            __m128i tr_l = _mm_load_si128(&set_opaque[char_byte_left].vec);
            __m128i tr_r = _mm_load_si128(&set_opaque[char_byte_right].vec);
            __m128i bg_l = _mm_loadu_si128(dest);
            __m128i bg_r = _mm_loadu_si128(dest + 1);

            _mm_storeu_si128(dest,     (col_l & tr_l) | (bg_l & ~tr_l));
            _mm_storeu_si128(dest + 1, (col_r & tr_r) | (bg_r & ~tr_r));
#else
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqu   0(%0), %%xmm2"      "\n\t" // load image
              "movdqu  16(%0), %%xmm3"      "\n\t"
              "movdqa  %%xmm2, %%xmm0"      "\n\t"
              "movdqa  %%xmm3, %%xmm1"      "\n\t"
              "pxor    (%3, %1, 1), %%xmm0" "\n\t" // image ^ colors
              "pxor    (%3, %2, 1), %%xmm1" "\n\t"
              "pand    (%4, %1, 1), %%xmm0" "\n\t" // & opaque
              "pand    (%4, %2, 1), %%xmm1" "\n\t" // off = 0, on = image ^ colors
              "pxor    %%xmm2, %%xmm0"      "\n\t" // ^ image
              "pxor    %%xmm3, %%xmm1"      "\n\t" // off = image, on = colors
              "movdqu  %%xmm0,  0(%0)"      "\n\t"
              "movdqu  %%xmm1, 16(%0)"      "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors),
                "r"(set_opaque)
              : "xmm0", "xmm1", "xmm2", "xmm3"
            );
#endif
          }
        }
        else
        {
          for(i = 0; i < CHAR_H; i++, out_ptr += align_pitch)
          {
            if(CLIP && clip_y && (out_ptr < start_ptr || out_ptr >= end_ptr))
              continue;

#ifdef USE_INTRINSICS
            char_byte_left = char_ptr[i] >> 4;
            char_byte_right = char_ptr[i] & 0x0f;

            __m128i_u *dest = reinterpret_cast<__m128i_u *>(out_ptr);
            __m128i col_l = _mm_load_si128(&set_colors[char_byte_left].vec);
            __m128i col_r = _mm_load_si128(&set_colors[char_byte_right].vec);
            _mm_storeu_si128(dest, col_l);
            _mm_storeu_si128(dest + 1, col_r);
#else
            char_byte_left = char_ptr[i] & 0xf0;
            char_byte_right = (char_ptr[i] & 0x0f) << 4;
            asm(
              "movdqa (%3, %1, 1), %%xmm0"  "\n\t"
              "movdqa (%3, %2, 1), %%xmm1"  "\n\t"
              "movdqu %%xmm0,  0(%0)"       "\n\t"
              "movdqu %%xmm1, 16(%0)"       "\n\t"
              :
              : "r"(out_ptr),
                "r"(char_byte_left),
                "r"(char_byte_right),
                "r"(set_colors)
              : "xmm0", "xmm1"
            );
#endif
          }
        }
      }
    }
  }
}

template<int SMZX, int TR>
static inline void render_layer32x4_sse2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer32x4_sse2<SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer32x4_sse2<SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;
  }
}

template<int SMZX>
static inline void render_layer32x4_sse2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int tr, int clip)
{
  switch(tr)
  {
    case 0:
      render_layer32x4_sse2<SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;

    case 1:
      render_layer32x4_sse2<SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer, clip);
      break;
  }
}

boolean render_layer32x4_sse2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  if(width_px & 7)
    return false;

  switch(smzx)
  {
    case 0:
      render_layer32x4_sse2<0>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer32x4_sse2<1>(
       pixels, width_px, height_px, pitch, graphics, layer, tr, clip);
      break;
  }
  return true;
}

#else /* !HAS_RENDER_LAYER32X4_SSE2 || .. */

boolean render_layer32x4_sse2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip)
{
  return false;
}

#endif
