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

#include "../graphics.h"

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
    for(ch_x = 0; ch_x < layer->w; ch_x++, src++)
    {
      c = src->char_value;
      if(c == INVISIBLE_CHAR)
        continue;

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
      for(row = 0; row < CHAR_H; row++, char_ptr++)
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
      }
    }
  }
}
