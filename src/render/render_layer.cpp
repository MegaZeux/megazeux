/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdlib.h>
#include <assert.h>

#include "render_layer.h"

#include "../graphics.h"
#include "../platform/platform_endian.h"

// Skip unused variants to reduce compile time on these platforms.
#if defined(CONFIG_WII) || defined(ANDROID) || defined(__EMSCRIPTEN__) || \
 defined(CONFIG_PSVITA)
#define SKIP_8BPP
#define SKIP_16BPP
#endif

#if defined(CONFIG_3DS)
#define SKIP_8BPP
#if !defined(CONFIG_SDL)
#define SKIP_16BPP
#endif
#endif

#if defined(CONFIG_DJGPP) && !defined(CONFIG_DOS_SVGA)
#define SKIP_8BPP
#define SKIP_16BPP
#endif

#if defined(CONFIG_DREAMCAST)
#define SKIP_8BPP
#endif

#include "render_layer_code.hpp"

#ifdef BUILD_REFERENCE_RENDERER
// This layer renderer is very slow, but it should work properly.
// The renderers in render_layer_code.hpp should generally be used instead.
// It might be useful to build for tests or benchmarking, though.

static inline void reference_renderer(uint32_t * RESTRICT pixels,
 int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  unsigned int mode = layer->mode;

  unsigned int ch_x, ch_y;
  uint16_t c;

  const struct char_element *src = layer->data;
  int row, col;
  int tcol = layer->transparent_col;

  const uint8_t *char_ptr;
  uint8_t current_char_byte;

  uint32_t *drawPtr;
  uint32_t pix;
  int pix_pos;
  //size_t advance = (pitch / 4) - 8;

  int x, y, i;

  uint32_t char_colors[4];
  int char_idx[4];
  int idx;

  unsigned int protected_pal_position = graphics->protected_pal_position;

  for(ch_y = 0; ch_y < layer->h; ch_y++)
  {
    for(ch_x = 0; ch_x < layer->w; ch_x++)
    {
      c = src->char_value;
      if(c != INVISIBLE_CHAR)
      {
        // Char values of 256+, prior to offsetting, are from the protected set
        if(c > 0xFF)
        {
          c = (c & 0xFF) + PROTECTED_CHARSET_POSITION;
        }
        else
        {
          c += layer->offset;
          c %= PROTECTED_CHARSET_POSITION;
        }

        if(mode)
        {
          for(i = 0; i < 4; i++)
          {
            idx = ((src->bg_color & 0xF) << 4 | (src->fg_color & 0xF)) * 4 + i;
            char_idx[i] = graphics->smzx_indices[idx];
            char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
          }
        }
        else
        {
          if(src->bg_color >= 16)
            char_idx[0] = (src->bg_color - 16) % 16 + protected_pal_position;
          else
            char_idx[0] = src->bg_color;

          if(src->fg_color >= 16)
            char_idx[1] = (src->fg_color - 16) % 16 + protected_pal_position;
          else
            char_idx[1] = src->fg_color;

          for(i = 0; i < 2; i++)
            char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
        }

        char_ptr = graphics->charset + (c * CHAR_H);
        for(row = 0; row < CHAR_H; row++)
        {
          current_char_byte = *char_ptr;
          if(mode == 0)
          {
            for(col = 0; col < CHAR_W; col++)
            {
              y = layer->y + ch_y * 14 + row;
              x = layer->x + ch_x * 8 + col;

              if(x >= 0 && x < width_px && y >= 0 && y < height_px)
              {
                drawPtr = pixels + (pitch / sizeof(uint32_t)) * y + x;

                pix_pos = (current_char_byte & (0x80 >> col)) << col >> 7;

                if(char_idx[pix_pos] != tcol)
                  *drawPtr = char_colors[pix_pos];
              }
            }
          }
          else
          {
            for(col = 0; col < CHAR_W; col+=2)
            {
              y = layer->y + ch_y * 14 + row;
              x = layer->x + ch_x * 8 + col;

              if(x >= -1 && x < width_px && y >= 0 && y < height_px)
              {
                drawPtr = pixels + (pitch / sizeof(uint32_t)) * y + x;

                pix_pos = (current_char_byte & (0xC0 >> col)) << col >> 6;

                if(char_idx[pix_pos] != tcol)
                {
                  pix = char_colors[pix_pos];
                  if(x >= 0)
                    *drawPtr = pix;
                  if(x < width_px - 1)
                    *(++drawPtr) = pix;
                }
              }
            }
          }
          char_ptr++;
        }
      }
      src++;
    }
  }
}
#endif

/* Return the alignment of a particular value (typically pixel array address).
 * Multiple addresses can be ORed together to get the lowest-common alignment
 * for multiple inputs. This function should not take architecture unalignment
 * capabilities or vector rendering into consideration.
 */
static size_t get_align_for_offset(size_t value)
{
#ifndef SKIP_64_ALIGN
  if(value % sizeof(uint64_t) == 0)
  {
#if ARCHITECTURE_BITS < 64
    static boolean printed = false;
    if(!printed)
    {
      warn("ARCHITECTURE_BITS not configured for this arch -- report this\n");
      printed = true;
    }
    return ARCHITECTURE_BITS;
#endif
    return 64;
  }
  else
#endif

  if(value % sizeof(uint32_t) == 0)
  {
    return 32;
  }
  else

  if(value % sizeof(uint16_t) == 0)
  {
    return 16;
  }
  else
    return 8;
}

/* Select the safest renderer given the alignment of the pixel
 * array, row pitch, and size_t. This function should not take architecture
 * alignment capabilities or vector rendering into consideration.
 */
static inline void select_aligned_renderer(const void *pixels,
 size_t width_px, size_t height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int &bpp, int &align, int &smzx, int &trans, int &clip)
{
  smzx = layer->mode;
  trans = layer->transparent_col != -1;
  clip = 0;

  if(layer->x < 0 || layer->y < 0 ||
   (layer->x + layer->w * CHAR_W) > width_px ||
   (layer->y + layer->h * CHAR_H) > height_px)
    clip = 1;

  if(bpp == -1)
    bpp = graphics->bits_per_pixel;

  size_t drawStart =
   (size_t)((char *)pixels + layer->y * (ptrdiff_t)pitch + (layer->x * bpp / 8));

  /**
   * Select the highest pixel align the current platform is capable of.
   * Additionally, to simplify the renderer code, the align must also be
   * capable of addressing the first pixel of the draw (e.g. a 64-bit platform
   * will use a 32-bit align if the layer is on an odd horizontal pixel).
   * This must be true for the first pixel of every character row drawn, so
   * the pitch must be in alignment too.
   *
   * In other words, the alignment is the minimum of:
   *   the number of bits of size_t (rough approximation of arch width),
   *   the number of bits of the first pixel's alignment in memory,
   *   the number of bits of the pitch's alignment (for rows after the first).
   */
  align = get_align_for_offset(sizeof(size_t) | drawStart | pitch);
}

/* Override selected alignment for platforms that support fast unaligned.
 */
static inline void select_unaligned_renderer(int &align)
{
#if defined(PLATFORM_UNALIGN_64)
  if(align >= PLATFORM_UNALIGN_64 * 8)
    align = 64;
#elif defined(PLATFORM_UNALIGN_32)
  if(align >= PLATFORM_UNALIGN_32 * 8 && align < 32)
    align = 32;
#endif
}

void render_layer(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
#if defined(BUILD_REFERENCE_RENDERER) && !defined(MZX_UNIT_TESTS)
  reference_renderer(reinterpret_cast<uint32_t * RESTRICT>(pixels),
   width_px, height_px, pitch, graphics, layer);
  return;
#endif

  int smzx = 0;
  int trans = 0;
  int align = 0;
  int clip = 0;
  select_aligned_renderer(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);

  select_unaligned_renderer(align);

  render_layer_func(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);
}
