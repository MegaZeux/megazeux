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

#ifndef MEGAZEUX_RENDER_LAYER_COMMON_HPP
#define MEGAZEUX_RENDER_LAYER_COMMON_HPP

#include "../graphics.h"
#include "../platform/platform_endian.h"

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
