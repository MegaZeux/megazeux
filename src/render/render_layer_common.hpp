/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_RENDER_LAYER_COMMON_HPP
#define MEGAZEUX_RENDER_LAYER_COMMON_HPP

#include "../graphics.h"
#include "../algorithm/arch.h"

/* Renderer dispatch functions. These are internal to render_layer.cpp but also
 * required by the units. Use render_layer instead of using these directly. */

void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int bpp, int align, int smzx, int trans, int clip);

/* Return the alignment of a particular value (typically pixel array address).
 * Multiple addresses can be ORed together to get the lowest-common alignment
 * for multiple inputs. This function should not take architecture unalignment
 * capabilities or vector rendering into consideration.
 *
 * TODO: ARCH_INT*_MODULO counters untested here, use type sizes instead.
 */
static inline size_t get_align_for_offset(size_t value)
{
#if ARCHITECTURE_BITS >= 64
  if(ARCH_PTR_MODULO(value, sizeof(uint64_t)) == 0)
  {
    return 64;
  }
  else
#endif

  if(ARCH_PTR_MODULO(value, sizeof(uint32_t)) == 0)
  {
    return 32;
  }
  else

  if(ARCH_PTR_MODULO(value, sizeof(uint16_t)) == 0)
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
#if defined(ARCH_INT64_UNALIGN)
  if(align >= ARCH_INT64_UNALIGN * 8)
    align = 64;
#elif defined(ARCH_INT32_UNALIGN)
  if(align >= ARCH_INT32_UNALIGN * 8 && align < 32)
    align = 32;
#endif
}


/* Renderer common functions. */

/**
 * Select the real display character for a char element.
 * TODO: this should be done at draw time.
 */
static inline int select_char(const struct char_element *src,
 const struct video_layer *layer)
{
  int ch = src->char_value;
  if(ch >= INVISIBLE_CHAR)
    return INVISIBLE_CHAR;

  // Char values >= 256, prior to offsetting, are from the protected charset.
  if(ch > 0xff)
  {
    return (ch & 0xff) + PROTECTED_CHARSET_POSITION;
  }
  else
    return ((ch & 0xff) + layer->offset) % PROTECTED_CHARSET_POSITION;
}

/**
 * Mode 0 and UI layer color selection function.
 * This needs to be done for both colors.
 */
static inline int select_color_16(uint8_t color, int ppal)
{
  // Palette values >= 16, prior to offsetting, are from the protected palette.
  if(color >= 16)
  {
    return (color - 16) % 16 + ppal;
  }
  else
    return color;
}

/**
 * Precompute clipping ranges for a layer.
 * Because the precision of the input is in chars (not pixels),
 * the destination x/y may be negative, and the effective final
 * destination x/y may extend beyond the screen. The caller is
 * responsible for correctly handling these clip areas.
 *
 * @param start_x   starting x within the layer, in chars.
 * @param start_y   starting y within the layer, in chars.
 * @param end_x     last x + 1 within the layer, in chars.
 * @param end_y     last y + 1 within the layer, in chars.
 * @param dest_x    first x to draw, in pixels. May be negative.
 * @param dest_y    first y to draw, in pixels. May be negative.
 */
static inline boolean precompute_clip(int &start_x, int &start_y,
 int &end_x, int &end_y, int &dest_x, int &dest_y,
 int width_px, int height_px, const struct video_layer *layer)
{
  int dest_last_x = layer->x + layer->w * CHAR_W;
  int dest_last_y = layer->y + layer->h * CHAR_H;
  boolean ret = false;

  if(layer->x < 0)
  {
    start_x = -(layer->x / CHAR_W);
    dest_x = layer->x % CHAR_W; // intentional truncated modulo
    ret = true;
  }
  else
  {
    start_x = 0;
    dest_x = layer->x;
  }

  if(layer->y < 0)
  {
    start_y = -(layer->y / CHAR_H);
    dest_y = layer->y % CHAR_H; // intentional truncated modulo
    ret = true;
  }
  else
  {
    start_y = 0;
    dest_y = layer->y;
  }

  end_x = layer->w;
  end_y = layer->h;

  if(dest_last_x > width_px)
  {
    end_x -= (dest_last_x - width_px) / CHAR_W;
    ret = true;
  }

  if(dest_last_y > height_px)
  {
    end_y -= (dest_last_y - height_px) / CHAR_H;
    ret = true;
  }
  return ret;
}

/**
 * Get the combined colors value from a char_element.
 */
static inline unsigned both_colors(const char_element *src)
{
#ifdef IS_CXX_11
  static_assert(offsetof(char_element, bg_color) == 2, "");
  static_assert(offsetof(char_element, fg_color) == 3, "");
#endif
  return reinterpret_cast<const uint16_t *>(src)[1];
}

#endif /* MEGAZEUX_RENDER_LAYER_COMMON_HPP */
