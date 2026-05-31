/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick - exophase@adelphia.net
 * Copyright (C) 2017-2022 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "char_ed.h"

#include "../core.h"
#include "../data.h"
#include "../graphics.h"
#include "../helpsys.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"
#include "../io/path.h"
#include "../io/vio.h"

#include "../event/event.h"

#include "configure.h"
#include "graphics.h"
#include "undo.h"
#include "window.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------
//  +----------------+ Current char-     (#000)
//  |................|    ..........0..........   Alternate:
//  |................|              ^
//  |................| %%      Move cursor        Alt+F   'Fill'
//  |................| -+      Change char        Alt+%%  Shift char
//  |................| Space   Toggle pixel       Ctrl+Z  Undo
//  |................| Enter   Select char        Ctrl+Y  Redo
//  |................| Del     Clear char         Alt+B   Block (Shift+%%)
//  |................| N       'Negative'         F2      Copy to buffer
//  |................| M       'Mirror'           F3      Copy from buffer
//  |................| F       'Flip'             F4      Revert to ASCII
//  |................| H       More help          F5      Revert to MZX
//  |................|
//  |................| C       Palette [ ]
//  |................| Tab     Draw    (off)
//  +----------------+ Ins/1-4 Select  [  ]
//----------------------------------------------

//Mouse- Menu=Command
//       Grid=Toggle
//       "Current Char-"=Enter
//       "..0.."=Change char

#define CHARS_SHOW_WIDTH 22

uint32_t mini_draw_layer = -1;
uint32_t mini_highlight_layer = -1;
static char char_copy_buffer[8 * 14 * 32];
static int char_copy_width = 8;
static int char_copy_height = 14;
static int char_copy_mode = 0;
static char current_char = 1;
static int current_width = 1;
static int current_height = 1;
static int highlight_width = 1;
static int highlight_height = 1;
static int current_charset = 0;
static int current_palette = 0x08;
static int use_default_palette = 1;
static int export_mode = 0;
static int num_files = 1;

static int char_copy_use_offset = 0;

static int selected_subdivision[19] =
{
  -1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0, -1, -1, 0, -1, -1, 0
};

static const char *chr_ext[] = { ".CHR", NULL };

static const char *help_text[3] =
{
  "\x12\x1d\t Move cursor\n"
  "-+\t Change char\n"
  "Space   Toggle pixel\n"
  "Enter   Select char\n"
  "Del\tClear char\n"
  "N\t  'Negative'\n"
  "M\t  'Mirror'\n"
  "F\t  'Flip'\n"
  "H\t  More help\n"
  "\n"
  "C\t  Palette (   )\n"
  "Tab\tDraw"
  ,
  "Alt+F   'Fill'\n"
  "Alt+\x12\x1d  Shift char\n"
  "Alt+B   Block (Shift+\x12\x1d)\n"
  "F2\t Copy to buffer\n"
  "F3\t Copy from buffer\n"
  "F4\t Revert to ASCII\n"
  "F5\t Revert to MZX\n"
  "\n"
  "H\t  More help\n"
  "\n"
  "C\t  Palette (   )\n"
  "Tab\tDraw"
  ,
  "PgUp    Prev. charset\n"
  "PgDn    Next charset\n"
  "Alt+X   Export\n"
  "Alt+I   Import\n"
  "Ctrl+Z  Undo\n"
  "Ctrl+Y  Redo\n"
  "\n"
  "\n"
  "H\t  Back\n"
  "\n"
  "C\t  Palette (   )\n"
  "Tab\tDraw"
};

static const char *help_text_smzx = "Ins/1-4 Select";

static const int prev_pixel[] = { 3, 2, 0, 1 };
static const int next_pixel[] = { 2, 3, 1, 0 };

static void char_editor_default_colors(void)
{
  // Char display
  set_protected_rgb(2, 21, 21, 21);
  set_protected_rgb(3, 35, 35, 35);
  set_protected_rgb(4, 28, 28, 28);
  set_protected_rgb(5, 42, 42, 42);
  // Selection
  set_protected_rgb(6, 14, 42, 56);
  set_protected_rgb(13, 7, 21, 49);
}

static void copy_color_to_protected(unsigned int from, unsigned int to)
{
  // Hack
  memcpy(&(graphics.protected_palette[to]), &(graphics.palette[from]),
   sizeof(struct rgb_color));
}

static void char_editor_update_colors(void)
{
  // Char display
  switch(get_screen_mode())
  {
    case 1:
    {
      // Interpolate the middle colors
      unsigned int upper = (current_palette & 0xF0) >> 4;
      unsigned int lower = (current_palette & 0x0F);
      uint8_t r0, g0, b0;
      uint8_t r3, g3, b3;

      get_rgb(upper, &r0, &g0, &b0);
      get_rgb(lower, &r3, &g3, &b3);

      copy_color_to_protected(upper, 2);
      set_protected_rgb(3, (r3*2 + r0)/3, (g3*2 + g0)/3, (b3*2 + b0)/3);
      set_protected_rgb(4, (r0*2 + r3)/3, (g0*2 + g3)/3, (b0*2 + b3)/3);
      copy_color_to_protected(lower, 5);
      break;
    }

    case 2:
    case 3:
    {
      // Hack
      uint8_t *index = graphics.smzx_indices + (current_palette * 4);

      copy_color_to_protected(*(index++), 2);
      copy_color_to_protected(*(index++), 3);
      copy_color_to_protected(*(index++), 4);
      copy_color_to_protected(*(index++), 5);
      break;
    }
  }

  // Selection
  set_protected_rgb(6, 14, 42, 56);
  set_protected_rgb(13, 7, 21, 49);
}

static void fill_region(char *buffer, int x, int y,
 int buffer_width, int buffer_height, int check, int draw)
{
  int offset = x + (y * buffer_width);
  if((x >= 0) && (x < buffer_width) &&
   (y >= 0) && (y < buffer_height) &&
   (buffer[offset] == check))
  {
    buffer[offset] = draw;

    fill_region(buffer, x - 1, y, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x + 1, y, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x, y - 1, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x, y + 1, buffer_width,
     buffer_height, check, draw);
  }
}

static void expand_buffer(char *buffer, int width, int height,
 int cwidth, int cheight, int current_char, int current_charset)
{
  char char_buffer[32 * 14];
  char *buffer_ptr = buffer;
  char *cbuffer_ptr = char_buffer;
  int cx, cy, x, y, i, shift;
  int cskip = 32 - cwidth;
  char *last_ptr, *last_ptr2;

  for(cy = 0; cy < cheight; cy++, current_char += cskip)
  {
    for(cx = 0; cx < cwidth; cx++, current_char++, cbuffer_ptr += 14)
    {
      // Bind to the current charset
      current_char &= 0xFF;
      ec_read_char(current_char + (current_charset * 256), cbuffer_ptr);
    }
  }

  if(get_screen_mode())
  {
    int skip = (width - 1) * 4;
    int skip2 = width * 4 * 14;

    for(cbuffer_ptr = char_buffer, y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          for(shift = 6; shift >= 0; shift -= 2, buffer_ptr++)
          {
            *buffer_ptr = ((*cbuffer_ptr) >> shift) & 3;
          }
        }
        buffer_ptr = last_ptr2 + 4;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
  else
  {
    int skip = (width - 1) * 8;
    int skip2 = width * 8 * 14;

    for(cbuffer_ptr = char_buffer, y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          for(shift = 7; shift >= 0; shift--, buffer_ptr++)
          {
            *buffer_ptr = ((*cbuffer_ptr) >> shift) & 1;
          }
        }
        buffer_ptr = last_ptr2 + 8;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
}

static void collapse_buffer(char *buffer, int width, int height,
 int cwidth, int cheight, int current_char, int current_charset,
 struct undo_history *h)
{
  char char_buffer[32 * 14];
  char *buffer_ptr = buffer;
  char *cbuffer_ptr = char_buffer;
  int cx, cy, x, y, i, shift;
  int cskip = 32 - cwidth;
  char *last_ptr, *last_ptr2;
  int current_row;

  if(h)
    add_charset_undo_frame(h, current_charset, current_char, cwidth, cheight);

  if(get_screen_mode())
  {
    int skip = (width - 1) * 4;
    int skip2 = width * 4 * 14;

    for(y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          current_row = 0;
          for(shift = 6; shift >= 0; shift -= 2, buffer_ptr++)
          {
            current_row |= (*buffer_ptr) << shift;
          }
          *cbuffer_ptr = current_row;
        }
        buffer_ptr = last_ptr2 + 4;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
  else
  {
    int skip = (width - 1) * 8;
    int skip2 = width * 8 * 14;

    for(y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          current_row = 0;
          for(shift = 7; shift >= 0; shift--, buffer_ptr++)
          {
            current_row |= (*buffer_ptr) << shift;
          }
          *cbuffer_ptr = current_row;
        }
        buffer_ptr = last_ptr2 + 8;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }

  cbuffer_ptr = char_buffer;

  for(cy = 0; cy < cheight; cy++, current_char += cskip)
  {
    for(cx = 0; cx < cwidth; cx++, current_char++, cbuffer_ptr += 14)
    {
      // Bind to the current charset
      current_char &= 0xFF;
      ec_change_char(current_char + (current_charset * 256), cbuffer_ptr);
    }
  }

  if(h)
    update_undo_frame(h);
}

static void change_copy_buffer_mode(int new_mode)
{
  char *old_pos;
  char *new_pos;

  /* The SMZX copy buffer is stored different from the regular MZX copy buffer,
   * which causes issues when attempting to copy between the two. Convert
   * between them as-needed to fix this.
   */

  if(new_mode == 0 && char_copy_mode != 0)
  {
    old_pos = char_copy_buffer + char_copy_width * char_copy_height;
    new_pos = char_copy_buffer + char_copy_width * char_copy_height * 2;

    while(old_pos > char_copy_buffer)
    {
      old_pos--;
      new_pos -= 2;
      new_pos[0] = (*old_pos & 0x02) >> 1;
      new_pos[1] = (*old_pos & 0x01);
    }
    char_copy_width *= 2;
  }
  else

  if(new_mode != 0 && char_copy_mode == 0)
  {
    char *stop = char_copy_buffer + char_copy_width * char_copy_height;
    old_pos = char_copy_buffer;
    new_pos = char_copy_buffer;

    while(old_pos < stop)
    {
      *new_pos = ((old_pos[0] & 0x01) << 1) | (old_pos[1] & 0x01);
      old_pos += 2;
      new_pos++;
    }
    char_copy_width /= 2;
  }

  char_copy_mode = new_mode;
}

static void draw_multichar(char *buffer, int start_x, int start_y,
 int width, int height, int current_x, int current_y,
 int highlight_width, int highlight_height)
{
  char *buffer_ptr = buffer;
  int buffer_width = width * 8;
  char highlight[2] = { 0x11, 0x1B };
  char colors[2];
  int c_offset;
  int color;
  int x, y;

  if(use_default_palette)
  {
    colors[0] = 0x18;
    colors[1] = 0x17;
    color = 0x87;
    c_offset = 16;
  }
  else
  {
    color = current_palette;
    colors[0] = (color & 0xF0) >> 4;
    colors[1] = (color & 0x0F);
    c_offset = 0;
  }

  if((height == 1) && (width <= 3))
  {
    char chars[2] = { 250, 219 };
    int index;

    for(y = 0; y < 14; y++)
    {
      for(x = 0; x < (width * 8); x++, buffer_ptr++)
      {
        int draw_x = start_x + (x * 2);
        int draw_y = start_y + y;
        index = *buffer_ptr;
        draw_char_ext(chars[index], color, draw_x,     draw_y, PRO_CH, c_offset);
        draw_char_ext(chars[index], color, draw_x + 1, draw_y, PRO_CH, c_offset);
      }
    }

    for(y = 0; y < highlight_height; y++)
    {
      color_line(highlight_width * 2, start_x + (current_x * 2),
       start_y + current_y + y, 0x1B);
    }
  }
  else
  {
    char half_chars[4] = { 0x20, 0xDF, 0xDC, 0xDB };
    int current_char;
    int upper;
    int lower;

    for(y = 0; y < (height * 7); y++, buffer_ptr += buffer_width)
    {
      for(x = 0; x < buffer_width; x++, buffer_ptr++)
      {
        upper = *buffer_ptr;
        lower = *(buffer_ptr + buffer_width);
        current_char = (lower << 1) + upper;

        draw_char_ext(half_chars[current_char], color,
         start_x + x, start_y + y, PRO_CH, c_offset);
      }
    }

    // Start at the top
    y = highlight_height;
    if(current_y & 1)
    {
      buffer_ptr = buffer + ((current_y - 1) * buffer_width) + current_x;

      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        upper = *buffer_ptr;
        lower = *(buffer_ptr + buffer_width);

        draw_char_mixed_pal_ext(half_chars[2], colors[upper], highlight[lower],
         start_x + x + current_x, start_y + (current_y / 2), PRO_CH);
      }
      current_y++;
      y--;
    }

    while(y > 1)
    {
      buffer_ptr = buffer + (current_y * buffer_width) + current_x;
      color_line(highlight_width, start_x + current_x,
       start_y + (current_y / 2), 0x1B);
      current_y += 2;
      y -= 2;
    }

    if(y)
    {
      buffer_ptr = buffer + (current_y * buffer_width) + current_x;

      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        upper = *buffer_ptr;
        lower = *(buffer_ptr + buffer_width);

        draw_char_mixed_pal_ext(half_chars[1], colors[lower], highlight[upper],
         start_x + x + current_x, start_y + (current_y / 2), PRO_CH);
      }
    }
  }
}

static void draw_multichar_smzx(char *buffer, int start_x, int start_y,
 int width, int height, int current_x, int current_y,
 int highlight_width, int highlight_height)
{
  char *buffer_ptr = buffer;
  char current_color;
  char base_colors[4] = { 0x2, 0x3, 0x4, 0x5 };
  char highlight_colors[4] = { 0x1, 0x6, 0xD, 0xB };
  int bg_color = (base_colors[0] << 4) + base_colors[3];
  int buffer_width = width * 4;

  int x, y;
  int draw_x, draw_y;

  if((height == 1) && (width <= 3))
  {
    int skip = buffer_width - highlight_width;

    int current_pixel;
    for(y = 0; y < 14; y++)
    {
      for(x = 0; x < buffer_width; x++, buffer_ptr++)
      {
        current_pixel = *buffer_ptr;

        if(current_pixel)
        {
          write_string("\xDB\xDB\xDB\xDB", start_x + (x * 4),
           start_y + y, base_colors[current_pixel], WR_NONE);
        }
        else
        {
          write_string("\xFA\xFA\xFA\xFA", start_x + (x * 4),
           start_y + y, bg_color, WR_NONE);
        }
      }
    }

    buffer_ptr = buffer + current_x + ((current_y) * buffer_width);

    for(y = 0; y < highlight_height; y++, buffer_ptr += skip)
    {
      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        current_pixel = *buffer_ptr;

        if(current_pixel)
        {
          write_string("\xDB\xDB\xDB\xDB", start_x +
           ((x + current_x) * 4), start_y + current_y + y,
           highlight_colors[current_pixel], WR_NONE);
        }
        else
        {
          write_string("\xFA\xFA\xFA\xFA", start_x +
           ((x + current_x) * 4), start_y + y + current_y,
           0x1B, WR_NONE);
        }
      }
    }
  }
  else
  {
    int skip = buffer_width - highlight_width;

    for(y = 0; y < (height * 7); y++, buffer_ptr += buffer_width)
    {
      for(x = 0; x < buffer_width; x++, buffer_ptr++)
      {
        current_color = (base_colors[(int)(*buffer_ptr)]) |
         ((base_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);

        draw_char(223, current_color, start_x + (x * 2),     start_y + y);
        draw_char(223, current_color, start_x + (x * 2) + 1, start_y + y);
      }
    }

    // Start at the top
    y = highlight_height;
    if(current_y & 1)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        current_color = (base_colors[(int)(*buffer_ptr)]) |
         ((highlight_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);

        draw_x = start_x + (current_x + x) * 2;
        draw_y = start_y + (current_y / 2);
        draw_char(223, current_color, draw_x,     draw_y);
        draw_char(223, current_color, draw_x + 1, draw_y);
      }

      buffer_ptr += (skip * 2);
      y--;

      current_y++;
    }

    while(y > 1)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        current_color = (highlight_colors[(int)(*buffer_ptr)]) |
         ((highlight_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);

        draw_x = start_x + (current_x + x) * 2;
        draw_y = start_y + (current_y / 2);
        draw_char(223, current_color, draw_x,     draw_y);
        draw_char(223, current_color, draw_x + 1, draw_y);
      }
      buffer_ptr += (skip * 2);
      current_y += 2;
      y -= 2;
    }

    if(y)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        current_color = (highlight_colors[(int)(*buffer_ptr)]) |
         ((base_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);

        draw_x = start_x + (current_x + x) * 2;
        draw_y = start_y + (current_y / 2);
        draw_char(223, current_color, draw_x,     draw_y);
        draw_char(223, current_color, draw_x + 1, draw_y);
      }
    }
  }
}

static void draw_mini_buffer(int info_x, int info_y, int current_charset,
 int current_char, int current_width, int current_height,
 int highlight_width, int highlight_height)
{
  char mini_buffer[32];
  int chars_show_width;
  int chars_show_num;
  int chars_show_x;
  int chars_show_y;
  int highlight_num;
  int highlight_pos;
  uint8_t draw_char;
  int offset;
  int x;
  int y;
  int i;

  // 0- not highlight, 1- highlight
  char use_color[] = { 0x80, 0x8F };
  char use_offset[] = { 16, 16 };
  uint32_t use_layer[] = { 0, 0 };
  int is_highlight;

  mini_draw_layer = create_layer(0, 0, 80, 25, LAYER_DRAWORDER_UI - 10,
   -1, (current_charset * 256), 1);

  set_layer_mode(mini_draw_layer, 0);
  use_layer[0] = mini_draw_layer;
  use_layer[1] = mini_draw_layer;

  if(!use_default_palette)
  {
    // Apply user-defined palette to the highlight and create a layer for it
    mini_highlight_layer = create_layer(0, 0, 80, 25, LAYER_DRAWORDER_UI - 9,
     -1, (current_charset * 256), 1);

    use_layer[1] = mini_highlight_layer;
    use_color[1] = current_palette;
    use_offset[1] = 0;
  }
  else
  {
    // The protected palette and SMZX modes simply do not work well together.
    mini_highlight_layer = -1;
  }

  // Mini buffer
  for(y = 0, offset = 0; y < highlight_height; y++)
  {
    for(x = 0; x < highlight_width; x++, offset++)
    {
      mini_buffer[offset] = current_char + x + (y * 32);
    }
  }

  for(y = 0, offset = 0; y < current_height; y++)
  {
    for(x = 0; x < current_width; x++, offset++)
    {
      select_layer(UI_LAYER);
      erase_char(info_x + x, info_y + y + 1);

      select_layer(use_layer[1]);

      draw_char_ext(mini_buffer[offset], use_color[1],
       info_x + x, info_y + y + 1, 0, use_offset[1]);
    }
  }

  // Charset display
  chars_show_width = CHARS_SHOW_WIDTH - current_width;
  chars_show_x = info_x + current_width + 2;
  chars_show_y = info_y + 1;

  chars_show_num = (chars_show_width + highlight_width - 1) / highlight_width;
  chars_show_num |= 1;

  highlight_pos = (chars_show_width - highlight_width) / 2;
  highlight_num = (chars_show_num + 1) / 2 - 1;

  // Select "tiles" backward from the highlight
  for(i = 0; i < highlight_num; i++)
    current_char = char_select_next_tile(current_char, -1,
     highlight_width, highlight_height);

  for(i = 0; i < chars_show_num; i++)
  {
    // Adjust by highlight_pos; the current char should be in the center
    int adj_x = highlight_pos + (i - highlight_num)*highlight_width;

    is_highlight = (i == highlight_num);

    // Draw the current "tile"
    for(x = 0; x < highlight_width; x++)
    {
      // Ignore chars outside of the drawing area
      if((adj_x + x < 0) || (adj_x + x >= chars_show_width))
        continue;

      for(y = 0; y < highlight_height; y++)
      {
        draw_char = current_char + x + (y * 32);

        select_layer(UI_LAYER);
        erase_char(chars_show_x + adj_x + x, chars_show_y + y);

        select_layer(use_layer[is_highlight]);

        draw_char_ext(draw_char, use_color[is_highlight],
         chars_show_x + adj_x + x, chars_show_y + y,
         0, use_offset[is_highlight]);
      }
    }

    // Select the next "tile"
    current_char = char_select_next_tile(current_char, 1,
     highlight_width, highlight_height);
  }
}

static boolean replace_filenum(char *dest, size_t dest_sz, const char *src, int num)
{
  char *pos = dest;
  size_t pos_left = dest_sz;

  while(*src)
  {
    size_t next = strcspn(src, "#");
    if(next > 0)
    {
      if(next >= pos_left)
        return false;

      memcpy(pos, src, next);
      pos[next] = '\0';

      src += next;
      pos += next;
      pos_left -= next;
    }

    if(*src == '#')
    {
      int out = snprintf(pos, pos_left, "%d", num);
      if((size_t)out >= pos_left)
        return false;

      pos += out;
      pos_left -= out;
      src++;
    }
  }
  *pos = '\0';

  if(!path_force_ext(dest, dest_sz, ".chr"))
    return false;

#if defined(_WIN32) || defined(CONFIG_DJGPP)
  /* Do not attempt to read/write to DOS devices. */
  if(path_safety_check(dest, PATH_SAFE_DOS_DEVICE))
    return false;
#endif

  return true;
}

static int select_export_mode(struct world *mzx_world, const char *title)
{
  struct dialog di;
  int result;

  const char *choices[] =
  {
    "Current selection (tile)",
    "Offset (linear)",
  };

  struct element *elements[] =
  {
    construct_button( 5, 5, "  OK  ", 0),
    construct_button(19, 5, "Cancel", -1),
    construct_radio_button(2, 2, choices, ARRAY_SIZE(choices), 24, &export_mode)
  };

  construct_dialog(&di, title, 24, 9, 32, 7,
   elements, ARRAY_SIZE(elements), 2);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  force_release_all_keys();

  if(!result)
  {
    return export_mode;
  }

  return result;
}

static int char_import_tile(const char *name, int char_offset, int charset,
 int highlight_width, int highlight_height)
{
  vfile *vf = vfopen_unsafe(name, "rb");
  if(vf)
  {
    size_t buffer_size = highlight_width * highlight_height * CHAR_SIZE;
    char *buffer = ccalloc(buffer_size, 1);

    size_t data_size = vfilelength(vf, true);

    if(data_size > buffer_size)
      data_size = buffer_size;

    data_size = vfread(buffer, 1, data_size, vf);
    vfclose(vf);

    ec_change_block((uint8_t)char_offset, (uint8_t)charset,
     (uint8_t)highlight_width, (uint8_t)highlight_height, buffer);

    free(buffer);
    return 0;
  }
  return -1;
}

static void char_export_tile(const char *name, int char_offset, int charset,
 int highlight_width, int highlight_height)
{
  vfile *vf = vfopen_unsafe(name, "wb");
  if(vf)
  {
    size_t buffer_size = highlight_width * highlight_height * CHAR_SIZE;
    char *buffer = ccalloc(buffer_size, 1);

    ec_read_block((uint8_t)char_offset, (uint8_t)charset,
     (uint8_t)highlight_width, (uint8_t)highlight_height, buffer);

    vfwrite(buffer, 1, buffer_size, vf);
    vfclose(vf);

    free(buffer);
  }
}

static void char_import(struct world *mzx_world, int char_offset, int charset,
 int highlight_width, int highlight_height, struct undo_history *h)
{
  static char saved_import_string[MAX_PATH];
  char import_string[256] = { 0, };
  char import_name[256];
  int current_file = 0;
  int i;

  struct element *elements[5];
  char offset_buf[20];
  char size_buf[20];

  int import_mode = 1;

  // Default to linear for 1x1 selections, otherwise prompt.
  if(highlight_width * highlight_height != 1)
    import_mode = select_export_mode(mzx_world, "Import mode");

  switch(import_mode)
  {
    case -1:
      return;

    case 0:
      sprintf(offset_buf, "~9Offset:  ~f%d", charset * 256 + char_offset);
      sprintf(size_buf,   "~9Size:    ~f%d x %d",
       highlight_width, highlight_height);

      elements[0] = construct_label(3, 20, offset_buf);
      elements[1] = construct_label(3, 21, size_buf);
      break;

    case 1:
      elements[0] = construct_number_box(3, 20, "Offset:  ", 0, 255, NUMBER_BOX,
       &char_offset);
      elements[1] = construct_label(3, 21, "~9Size:    auto");
      break;
  }

  elements[2] =
   construct_number_box(28, 20, "First: ", 0, 255, NUMBER_BOX, &current_file);
  elements[3] =
   construct_number_box(52, 20, "Count: ", 1, 256, NUMBER_BOX, &num_files);
  elements[4] =
   construct_label(28, 21, "~9Use \"file#.chr\" to import multiple charsets.");

  strcpy(import_string, saved_import_string);
  if(!file_manager(mzx_world, chr_ext, NULL, import_string,
   "Import character set(s)", ALLOW_ALL_DIRS, ALLOW_WILDCARD_FILES,
   elements, ARRAY_SIZE(elements), 2))
  {
    int num_files_present = num_files;

    strcpy(saved_import_string, import_string);
    if(strchr(import_string, '#') == NULL)
      num_files_present = 1;

    // Save the whole charset to the history to keep things simple
    add_charset_undo_frame(h, charset, 0, 32, 8);

    for(i = 0; i < num_files_present; i++, current_file++)
    {
      if(!replace_filenum(import_name, sizeof(import_name), import_string, current_file))
        continue;

      if(import_mode)
      {
        // Version needs to be sufficiently high for extended charsets
        int char_size =
         ec_load_set_var(import_name, charset * 256 + char_offset, INT_MAX);

        if(char_size != -1)
        {
          char_offset += char_size;
          char_offset &= 0xFF;
        }
      }
      else
      {
        int ret = char_import_tile(import_name, char_offset, charset,
         highlight_width, highlight_height);

        if(ret != -1)
        {
          char_offset = char_select_next_tile(char_offset, 1,
           highlight_width, highlight_height);
        }
      }
    }

    update_undo_frame(h);
  }
}

static void char_export(struct world *mzx_world, int char_offset, int charset,
 int highlight_width, int highlight_height)
{
  static char saved_export_string[MAX_PATH];
  char export_string[256] = { 0, };
  char export_name[256];
  int current_file = 0;
  int char_size = highlight_width * highlight_height;
  int i;

  struct element *elements[5];
  char offset_buf[20];
  char size_buf[20];
  int export_mode = 1;

  if(highlight_height > 1)
    export_mode = select_export_mode(mzx_world, "Export mode");

  switch(export_mode)
  {
    case -1:
      return;

    case 0:
      snprintf(offset_buf, 20, "~9Offset:  ~f%d", charset * 256 + char_offset);
      snprintf(size_buf,   20, "~9Size:    ~f%d x %d",
       highlight_width, highlight_height);

      elements[0] = construct_label(3, 20, offset_buf);
      elements[1] = construct_label(3, 21, size_buf);
      break;

    case 1:
      elements[0] = construct_number_box(3, 20, "Offset:  ", 0, 255, NUMBER_BOX,
       &char_offset);
      elements[1] = construct_number_box(3, 21, "Size:    ", 1, 256, NUMBER_BOX,
       &char_size);
      break;
  }

  elements[2] =
   construct_number_box(28, 20, "First: ", 0, 255, NUMBER_BOX, &current_file);
  elements[3] =
   construct_number_box(52, 20, "Count: ", 1, 256, NUMBER_BOX, &num_files);
  elements[4] =
   construct_label(28, 21, "~9Use \"file#.chr\" to export multiple charsets.");

  strcpy(export_string, saved_export_string);
  if(!file_manager(mzx_world, chr_ext, ".chr", export_string,
   "Export character set(s)", ALLOW_ALL_DIRS, ALLOW_NEW_FILES,
   elements, ARRAY_SIZE(elements), 2))
  {
    int num_files_present = num_files;

    strcpy(saved_export_string, export_string);
    if(strchr(export_string, '#') == NULL)
      num_files_present = 1;

    for(i = 0; i < num_files_present; i++, current_file++)
    {
      if(!replace_filenum(export_name, sizeof(export_name), export_string, current_file))
        continue;

      if(export_mode)
      {
        ec_save_set_var(export_name, charset * 256 + char_offset, char_size);
        char_offset += char_size;
        char_offset &= 0xFF;
      }
      else
      {
        char_export_tile(export_name, char_offset, charset,
         highlight_width, highlight_height);

        char_offset = char_select_next_tile(char_offset, 1,
         highlight_width, highlight_height);
      }
    }
  }
}

int char_editor(struct world *mzx_world)
{
  struct undo_history *h;
  int changed = 1;

  int x = 0;
  int y = 0;
  int draw = 0;
  int draw_new;
  int chars_width, chars_height;
  int chars_x, chars_y;
  int info_height;
  int info_x, info_y;
  int buffer_width = current_width * 8;
  int buffer_height = current_height * 14;
  int dialog_width, dialog_height;
  int dialog_x, dialog_y;
  char *buffer = cmalloc(8 * 14 * 32);
  int last_x = -1, last_y = 0;
  int block_x = 0;
  int block_y = 0;
  int block_start_x = 0;
  int block_start_y = 0;
  int block_width = buffer_width;
  int block_height = buffer_height;
  int block_mode = 0;
  int shifted = 0;
  int i, i2, key;
  int joystick_key;
  int pad_height;
  int small_chars;
  int screen_mode = get_screen_mode();
  int current_pixel = 0;
  char smzx_colors[4] = { 0x2, 0x3, 0x4, 0x5 };
  int help_page = 0;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // Prepare the history
  h = construct_charset_undo_history(get_editor_config()->undo_history_size);

  // Make sure the copy buffer is in a usable format
  change_copy_buffer_mode(screen_mode);

  if(screen_mode)
  {
    // Set up colors for SMZX char editing
    if(use_default_palette)
      char_editor_default_colors();

    else
      char_editor_update_colors();
  }

  set_context(CTX_CHAR_EDIT);

  m_show();
  // Get char
  expand_buffer(buffer, current_width, current_height,
   highlight_width, highlight_height, current_char,
   current_charset);

  save_screen();

  do
  {
    buffer_width = current_width * 8;
    buffer_height = current_height * 14;

    if((current_height == 1) &&
     (current_width <= 3))
    {
      small_chars = 0;
      mouse_size(8, 14);
      chars_width = buffer_width * 2;
      chars_height = buffer_height;
    }
    else
    {
      small_chars = 1;
      mouse_size(8, 7);
      chars_width = buffer_width;
      chars_height = buffer_height / 2;
    }

    if(screen_mode)
      buffer_width /= 2;

    if(x >= buffer_width)
      x = buffer_width - 1;

    if(y >= buffer_height)
      y = buffer_height - 1;

    dialog_width = chars_width + 4 + 27;
    dialog_height = chars_height + 4;
    pad_height = current_height;
    if(highlight_height > pad_height)
      pad_height = highlight_height;

    info_height = 15 + pad_height;

    if(dialog_height < (info_height + 2))
      dialog_height = info_height + 2;

    if(dialog_width == 79)
      dialog_width++;

    dialog_x = 40 - (dialog_width / 2);
    dialog_y = 12 - (dialog_height / 2);

    chars_x = dialog_x + 3;
    chars_y = dialog_y + (dialog_height / 2) - (chars_height / 2);
    info_x = chars_x + chars_width + 2;
    info_y = dialog_y + (dialog_height / 2) - (info_height / 2);

    restore_screen();
    save_screen();

    draw_window_box(dialog_x, dialog_y,
     dialog_x + dialog_width - 1,
     dialog_y + dialog_height - 1, 143, 128, 135, 1, 1);
    draw_window_box(chars_x - 1, chars_y - 1,
     chars_x + chars_width, chars_y + chars_height,
     128, 143, 135, 0, 0);

    // Current character
    write_string("Current char-\t(#000)", info_x, info_y, 0x8F, WR_TAB);
    write_number(current_char + (current_charset * 256), 0x8F,
     info_x + 22, info_y, 3, 3, 10);

    // Help
    write_string(help_text[help_page],
     info_x, info_y + pad_height + 2, 0x8F, WR_TAB | WR_NEWLINE);

    if(screen_mode)
    {
      // Correct "Toggle pixel" to "Place pixel"
      if(help_page == 0)
        write_string("Place pixel ",
         info_x + 8, info_y + pad_height + 4, 0x8F, WR_TAB);

      // Correct F5 to "SMZX"
      if(help_page == 1)
        write_string("SMZX",
         info_x + 18, info_y + pad_height + 8, 0x8F, WR_TAB);

      // 1-4 Select
      write_string(help_text_smzx,
       info_x, info_y + pad_height + 14, 0x8F, WR_TAB);

      if(current_pixel)
      {
        write_string("\xDB\xDB\xDB\xDB", info_x + 16,
         info_y + pad_height + 14, smzx_colors[current_pixel], WR_NONE);
      }
      else
      {
        write_string("\xFA\xFA\xFA\xFA", info_x + 16,
         info_y + pad_height + 14, (smzx_colors[0] << 4) + smzx_colors[3], WR_NONE);
      }
    }

    draw_color_box(current_palette, 0, info_x + 17,
     info_y + pad_height + 12, info_x + 20);

    switch(draw)
    {
      case 0:
        write_string("(off)   ", info_x + 16,
         info_y + pad_height + 13, 0x8F, WR_NONE);
        break;

      case 1:
        write_string("(set)   ", info_x + 16,
         info_y + pad_height + 13, 0x8F, WR_NONE);
        break;

      case 2:
        write_string("(clear) ", info_x + 16,
         info_y + pad_height + 13, 0x8F, WR_NONE);
        break;
    }

    // Update char
    if(block_mode)
    {
      if(screen_mode)
      {
        draw_multichar_smzx(buffer, chars_x, chars_y,
         current_width, current_height, block_start_x,
         block_start_y, block_width, block_height);
      }
      else
      {
        draw_multichar(buffer, chars_x, chars_y,
         current_width, current_height, block_start_x,
         block_start_y, block_width, block_height);
      }
    }
    else
    {
      if(screen_mode)
      {
        draw_multichar_smzx(buffer, chars_x, chars_y,
         current_width, current_height, x, y, 1, 1);
      }
      else
      {
        draw_multichar(buffer, chars_x, chars_y,
         current_width, current_height, x, y, 1, 1);
      }
    }

    draw_mini_buffer(info_x, info_y, current_charset, current_char,
     current_width, current_height, highlight_width, highlight_height);

    select_layer(UI_LAYER);

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    draw_new = 0;

    if(get_shift_status(keycode_internal) || (block_mode == 2))
    {
      if(!shifted)
      {
        block_x = x;
        block_y = y;
        block_start_x = x;
        block_start_y = y;
        block_mode = 1;
        block_width = 1;
        block_height = 1;
        shifted = 1;
      }
      else
      {
        block_start_x = block_x;
        block_start_y = block_y;

        if(block_start_x > x)
        {
          block_start_x = x;
          block_width = block_x - x + 1;
        }
        else
        {
          block_width = x - block_x + 1;
        }

        if(block_start_y > y)
        {
          block_start_y = y;
          block_height = block_y - y + 1;
        }
        else
        {
          block_height = y - block_y + 1;
        }
      }
    }
    else
    {
      shifted = 0;
      block_mode = 0;

      block_width = buffer_width;
      block_height = buffer_height;
      block_start_x = 0;
      block_start_y = 0;
    }

    if(get_mouse_press_ext())
    {
      int mouse_press = get_mouse_press_ext();

      if(mouse_press == MOUSE_BUTTON_WHEELUP)
      {
        // Previous pixel
        current_pixel = prev_pixel[current_pixel];
      }
      else

      if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
      {
        // Next pixel
        current_pixel = next_pixel[current_pixel];
      }
      else

      if(mouse_press == MOUSE_BUTTON_MIDDLE)
      {
        // Char selector
        key = IKEY_RETURN;
      }
    }

    if(get_mouse_status())
    {
      int mouse_status = get_mouse_status();
      int mouse_x;
      int mouse_y;
      int draw_pixel;

      get_mouse_position(&mouse_x, &mouse_y);

      if(
       (mouse_x >= chars_x) &&
       (mouse_y >= chars_y) &&
       (mouse_x < chars_x + chars_width) &&
       (mouse_y < chars_y + chars_height))
      {
        // Grid.
        if(small_chars)
        {
          get_mouse_pixel_position(&mouse_x, &mouse_y);

          x = (mouse_x / 8) - chars_x;
          y = (mouse_y / 7) - (chars_y * 2);
        }
        else
        {
          x = (mouse_x - chars_x) / 2;
          y = mouse_y - chars_y;
        }

        if(screen_mode)
        {
          x /= 2;
          draw_pixel = current_pixel;
        }
        else
        {
          draw_pixel = 1;
        }

        if(!block_mode)
        {
          int mouse_draw = 0;

          if(mouse_status & MOUSE_BUTTON(MOUSE_BUTTON_LEFT))
          {
            // Draw current pixel
            mouse_draw = 1;
          }
          else

          if(mouse_status & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
          {
            if(screen_mode)
            {
              // Grab
              current_pixel = buffer[x + (buffer_width * y)];
            }
            else
            {
              // Clear
              draw_pixel = 0;
              mouse_draw = 1;
            }
          }

          if(mouse_draw && (last_x != x || last_y != y))
          {
            if(last_x != -1)
            {
              int dx = x - last_x;
              int dy = y - last_y;
              int draw_x;
              int draw_y;

              int mx = MAX(abs(dx), abs(dy));

              for(i = 0; i <= mx; i++)
              {
                draw_x = (dx * i / mx) + last_x;
                draw_y = (dy * i / mx) + last_y;
                buffer[draw_x + (buffer_width * draw_y)] = draw_pixel;
              }
            }
            else
            {
              buffer[x + (buffer_width * y)] = draw_pixel;
            }

            if(changed)
              add_charset_undo_frame(h, current_charset, current_char,
               highlight_width, highlight_height);

            collapse_buffer(buffer, current_width, current_height,
             highlight_width, highlight_height, current_char,
             current_charset, NULL);

            update_undo_frame(h);

            changed = 0;
            last_x = x;
            last_y = y;
          }
        }
      }
      else
      {
        // Mouse not in frame
        last_x = -1;
      }
    }
    else
    {
      // Mouse not pressed
      changed = 1;
      last_x = -1;
    }

    switch(key)
    {
      case IKEY_ESCAPE:
      {
        if(block_mode == 2)
        {
          block_mode = 0;
          key = 0;
        }
        break;
      }

      case IKEY_LEFT:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          int wrap;

          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width)
          {
            wrap = buffer_ptr[0];
            memmove(buffer_ptr, buffer_ptr + 1,
             block_width - 1);
            buffer_ptr[block_width - 1] = wrap;
          }

          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);
        }
        else
        {
          x--;
          if(x < 0)
            x = buffer_width - 1;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_RIGHT:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          int wrap;

          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width)
          {
            wrap = buffer_ptr[block_width - 1];
            memmove(buffer_ptr + 1, buffer_ptr,
             block_width - 1);
            buffer_ptr[0] = wrap;
          }

          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);
        }
        else
        {
          x++;
          if(x >= buffer_width)
            x = 0;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_UP:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          char *wrap_row = cmalloc(block_width);

          memcpy(wrap_row, buffer_ptr, block_width);

          for(i = 0; i < block_height - 1; i++,
           buffer_ptr += buffer_width)
          {
            memcpy(buffer_ptr, buffer_ptr + buffer_width,
             block_width);
          }

          memcpy(buffer_ptr, wrap_row, block_width);
          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);

          free(wrap_row);
        }
        else
        {
          y--;
          if(y < 0)
            y = buffer_height - 1;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_DOWN:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + ((block_start_y + (block_height - 1)) *
           buffer_width);
          char *wrap_row = cmalloc(block_width);

          memcpy(wrap_row, buffer_ptr, block_width);

          for(i = 0; i < block_height - 1; i++,
           buffer_ptr -= buffer_width)
          {
            memcpy(buffer_ptr, buffer_ptr - buffer_width,
             block_width);
          }

          memcpy(buffer_ptr, wrap_row, block_width);
          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);

          free(wrap_row);
        }
        else
        {
          y++;
          if(y >= buffer_height)
            y = 0;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_SPACE:
      {
        if(screen_mode)
          buffer[x + (y * buffer_width)] = current_pixel;
        else
          buffer[x + (y * buffer_width)] ^= 1;

        collapse_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset, h);

        break;
      }

      case IKEY_RETURN:
      {
        int show_palette = use_default_palette ? -1 : current_palette;

        int new_char =
         char_selection_ext(current_char, 1, &highlight_width,
         &highlight_height, &current_charset, show_palette);

        int highlight_size = highlight_width * highlight_height;
        int search_order[16];
        int factors[6];
        int num_factors = 0;
        int low, high;
        int subdivision = selected_subdivision[highlight_size];

        // The first choice should be about the square root,
        // biased high.
        search_order[0] = (int)(sqrt(highlight_size) + 0.75);

        // Subsequent choices should expand outwards
        low = search_order[0];
        high = search_order[0];
        i = 1;
        while((low > 1) || (high < 6))
        {
          if(high < 6)
          {
            high++;
            search_order[i] = high;
            i++;
          }

          if(low > 1)
          {
            low--;
            search_order[i] = low;
            i++;
          }
        }

        for(i = 0; i < 6; i++)
        {
          if(!(highlight_size % search_order[i]) &&
           (highlight_size / search_order[i] <= 3))
          {
            factors[num_factors] = search_order[i];
            num_factors++;
          }
        }

        if(num_factors > 1)
        {
          struct element **elements =
           cmalloc(sizeof(struct element *) * num_factors);
          char **radio_button_strings =
           cmalloc(sizeof(char *) * num_factors);
          char **radio_button_substrings =
           cmalloc(sizeof(char *) * num_factors);
          struct dialog di;

          for(i = 0; i < num_factors; i++)
          {
            radio_button_substrings[i] = cmalloc(32);
            radio_button_strings[i] = radio_button_substrings[i];
            sprintf(radio_button_strings[i], "%d x %d",
             factors[i], highlight_size / factors[i]);
          }

          elements[0] =
           construct_radio_button(8, 2,
           (const char **)radio_button_strings,
           num_factors, 5, &subdivision);
          elements[1] = construct_button(10, num_factors + 3,
           "OK", 0);

          construct_dialog(&di, "Choose subdivision", 28,
           8, 24, 6 + num_factors, elements, 2, 0);

          run_dialog(mzx_world, &di);
          destruct_dialog(&di);

          // Prevent UI keys from carrying through.
          force_release_all_keys();

          for(i = 0; i < num_factors; i++)
            free(radio_button_substrings[i]);
          free(radio_button_substrings);
          free(radio_button_strings);
          free(elements);
        }

        selected_subdivision[highlight_size] = subdivision;

        current_width = factors[subdivision];
        current_height = highlight_size / factors[subdivision];

        if(new_char >= 0)
          current_char = new_char;

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);

        changed = 1;
        break;
      }

      case IKEY_HOME:
      {
        x = 0;
        y = 0;
        break;
      }

      case IKEY_END:
      {
        x = buffer_width - 1;
        y = buffer_height - 1;
        break;
      }

      case IKEY_INSERT:
      {
        current_pixel = buffer[x + (y * buffer_width)];
        break;
      }

      case IKEY_DELETE:
      {
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);

        for(i = 0; i < block_height; i++,
         buffer_ptr += buffer_width)
        {
          memset(buffer_ptr, 0, block_width);
        }
        collapse_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset, h);

        break;
      }

      case IKEY_TAB:
      {
        if(draw)
        {
          draw = 0;
        }
        else
        {
          if(get_shift_status(keycode_internal))
            draw = 2;
          else
            draw = 1;

          draw_new = 1;
        }

        break;
      }

#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        // FIXME context
        help_system(NULL, mzx_world);
        break;
      }
#endif

      case IKEY_F2:
      {
        // Copy buffer
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);
        char *dest_ptr = char_copy_buffer;

        if(block_mode)
          char_copy_use_offset = 1;
        else
          char_copy_use_offset = 0;

        char_copy_width = block_width;
        char_copy_height = block_height;

        if(get_ctrl_status(keycode_internal))
        {
          // Cut to buffer
          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width, dest_ptr += block_width)
          {
            memcpy(dest_ptr, buffer_ptr, block_width);
            memset(buffer_ptr, 0, block_width);
          }

          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);
        }
        else
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width, dest_ptr += block_width)
          {
            memcpy(dest_ptr, buffer_ptr, block_width);
          }
        }
        break;
      }

      case IKEY_F3:
      {
        // Paste buffer
        int copy_width = char_copy_width;
        int copy_height = char_copy_height;
        int dest_x = x;
        int dest_y = y;
        char *src_ptr = char_copy_buffer;
        char *buffer_ptr;

        if(!char_copy_use_offset)
        {
          dest_x = 0;
          dest_y = 0;
        }

        buffer_ptr = buffer +
         dest_x + (dest_y * buffer_width);

        if((dest_x + copy_width) > buffer_width)
          copy_width = buffer_width - dest_x;

        if((dest_y + copy_height) > buffer_height)
          copy_height = buffer_height - dest_y;

        for(i = 0; i < copy_height; i++,
         buffer_ptr += buffer_width, src_ptr += char_copy_width)
        {
          memcpy(buffer_ptr, src_ptr, copy_width);
        }

        collapse_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset, h);

        break;
      }

      case IKEY_F4:
      {
        // ASCII Default
        int charset_offset = current_charset * 256;
        int char_offset = current_char;
        int skip = 32 - highlight_width;

        // ALT+F4 - do nothing.
        if(get_alt_status(keycode_internal))
          break;

        add_charset_undo_frame(h, current_charset, current_char,
         highlight_width, highlight_height);

        for(i = 0; i < highlight_height; i++,
         char_offset += skip)
        {
          for(i2 = 0; i2 < highlight_width; i2++,
           char_offset++)
          {
            // Wrap to current charset
            char_offset = char_offset % 256;
            ec_load_char_ascii(char_offset + charset_offset);
          }
        }

        update_undo_frame(h);

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);

        break;
      }

      case IKEY_F5:
      {
        // MZX Default
        int charset_offset = current_charset * 256;
        int char_offset = current_char;
        int skip = 32 - highlight_width;

        add_charset_undo_frame(h, current_charset, current_char,
         highlight_width, highlight_height);

        for(i = 0; i < highlight_height; i++,
         char_offset += skip)
        {
          for(i2 = 0; i2 < highlight_width; i2++,
           char_offset++)
          {
            // Wrap to current charset
            char_offset = char_offset % 256;
            ec_load_char_mzx(char_offset + charset_offset);
          }
        }

        update_undo_frame(h);

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);

        break;
      }

      case IKEY_KP1:
      case IKEY_1:
      {
        current_pixel = 0;
        break;
      }

      case IKEY_KP2:
      case IKEY_2:
      {
        current_pixel = 2;
        break;
      }

      case IKEY_KP3:
      case IKEY_3:
      {
        current_pixel = 1;
        break;
      }

      case IKEY_KP4:
      case IKEY_4:
      {
        current_pixel = 3;
        break;
      }

      case IKEY_KP_MINUS:
      case IKEY_MINUS:
      {
        current_char = char_select_next_tile(current_char, -1,
         highlight_width, highlight_height);

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);
        changed = 1;
        break;
      }

      case IKEY_KP_PLUS:
      case IKEY_EQUALS:
      {
        current_char = char_select_next_tile(current_char, 1,
         highlight_width, highlight_height);

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);
        changed = 1;
        break;
      }

      case IKEY_PAGEUP:
      {
        current_charset--;

        if(current_charset < 0)
          current_charset = NUM_CHARSETS - 2;

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);
        changed = 1;
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current_charset++;

        if(current_charset >= NUM_CHARSETS - 1)
          current_charset = 0;

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset);
        changed = 1;
        break;
      }

      case IKEY_b:
      {
        if(get_alt_status(keycode_internal))
        {
          block_mode = 2;
          block_x = x;
          block_y = y;
          block_start_x = x;
          block_start_y = y;
          block_width = 1;
          block_height = 1;
          shifted = 1;
        }

        break;
      }

      case IKEY_c:
      {
        if(get_alt_status(keycode_internal))
        {
          if(screen_mode)
            char_editor_default_colors();

          use_default_palette = 1;
        }
        else
        {
          int new_color = color_selection(current_palette, 0);
          if(new_color >= 0)
          {
            current_palette = new_color;
            use_default_palette = 0;

            if(screen_mode)
              char_editor_update_colors();
          }
        }
        break;
      }

      case IKEY_f:
      {
        if(get_alt_status(keycode_internal))
        {
          // Flood fill
          int check = buffer[x + (y * buffer_width)];
          int fill = check ^ 1;

          if(screen_mode)
            fill = current_pixel;

          if(check != fill)
          {
            fill_region(buffer, x, y, buffer_width, buffer_height,
             check, fill);
            collapse_buffer(buffer, current_width, current_height,
             highlight_width, highlight_height, current_char,
             current_charset, h);
          }
        }
        else
        {
          // Flip
          char *temp_buffer = cmalloc(sizeof(char) * block_width);
          int start_offset = block_start_x +
           (block_start_y * buffer_width);
          int end_offset = start_offset +
           ((block_height - 1) * buffer_width);

          for(i = 0; i < (block_height / 2); i++,
           start_offset += buffer_width, end_offset -= buffer_width)
          {
            memcpy(temp_buffer, buffer + start_offset,
             block_width);
            memcpy(buffer + start_offset, buffer + end_offset,
             block_width);
            memcpy(buffer + end_offset, temp_buffer,
             block_width);
          }

          collapse_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset, h);

          free(temp_buffer);
        }
        break;
      }

      case IKEY_h:
      {
        // Switch help page
        help_page = (help_page + 1) % 3;
        break;
      }

      case IKEY_i:
      {
        // Import
        if(get_alt_status(keycode_internal))
        {
          char_import(mzx_world, current_char, current_charset,
           highlight_width, highlight_height, h);

          expand_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset);
        }
        break;
      }

      case IKEY_m:
      {
        // Mirror
        int temp, i2;
        int start_offset = block_start_x +
         (block_start_y * buffer_width);
        int previous_offset;
        int end_offset;

        for(i = 0; i < block_height; i++)
        {
          previous_offset = start_offset;
          end_offset = start_offset + block_width - 1;
          for(i2 = 0; i2 < (block_width / 2);
           i2++, start_offset++, end_offset--)
          {
            temp = buffer[start_offset];
            buffer[start_offset] = buffer[end_offset];
            buffer[end_offset] = temp;
          }
          start_offset = previous_offset + buffer_width;
        }

        collapse_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset, h);

        break;
      }

      case IKEY_n:
      {
        // Negative
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);

        if(screen_mode)
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += (buffer_width - block_width))
          {
            for(i2 = 0; i2 < block_width; i2++, buffer_ptr++)
            {
              *buffer_ptr = 3 - *buffer_ptr;
            }
          }
        }
        else
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += (buffer_width - block_width))
          {
            for(i2 = 0; i2 < block_width; i2++, buffer_ptr++)
            {
              *buffer_ptr ^= 1;
            }
          }
        }

        collapse_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char,
         current_charset, h);

        break;
      }

      case IKEY_q:
      {
        key = IKEY_ESCAPE;
        break;
      }

      case IKEY_x:
      {
        // Export
        if(get_alt_status(keycode_internal))
        {
          char_export(mzx_world, current_char, current_charset,
           highlight_width, highlight_height);
        }
        break;
      }

      case IKEY_y:
      {
        // Redo
        if(get_ctrl_status(keycode_internal))
        {
          apply_redo(h);

          expand_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset);
          changed = 1;
        }
        break;
      }

      case IKEY_z:
      {
        // Undo
        if(get_ctrl_status(keycode_internal))
        {
          apply_undo(h);

          expand_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           current_charset);
          changed = 1;
        }
        break;
      }
    }

    if(draw_new)
    {
      switch(draw)
      {
        case 1:
        {
          if(screen_mode)
            buffer[x + (y * buffer_width)] = current_pixel;
          else
            buffer[x + (y * buffer_width)] = 1;
          break;
        }

        case 2:
        {
          buffer[x + (y * buffer_width)] = 0;
          break;
        }
      }
      collapse_buffer(buffer, current_width, current_height,
       highlight_width, highlight_height, current_char,
       current_charset, h);
    }

    // Destroy the mini buffer layers
    destruct_extra_layers(mini_draw_layer);
    destruct_extra_layers(mini_highlight_layer);

  } while(key != IKEY_ESCAPE);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  mouse_size(8, 14);

  restore_screen();
  update_screen();
  pop_context();

  if(screen_mode)
  {
    // Reset the protected palette
    default_protected_palette();
  }

  destruct_undo_history(h);

  free(buffer);

  return current_char + (current_charset * CHARSET_SIZE);
}
