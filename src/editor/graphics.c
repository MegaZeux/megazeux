/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 *
 * YUV Renderers:
 *   Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 *
 * OpenGL #2 Renderer:
 *   Copyright (C) 2007 Joel Bouchard Lamontagne <logicow@gmail.com>
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

#include "../graphics.h"
#include "../util.h" /* mzx_res */
#include "../io/vio.h"

#include <stdint.h>
#include <string.h>

static uint8_t ascii_charset[CHAR_SIZE * CHARSET_SIZE];
static uint8_t blank_charset[CHAR_SIZE * CHARSET_SIZE];
static uint8_t smzx_charset[CHAR_SIZE * CHARSET_SIZE];
static uint8_t smzx_charset2[CHAR_SIZE * CHARSET_SIZE];

void store_backup_palette(char dest[SMZX_PAL_SIZE * 3])
{
  uint32_t i;
  uint8_t *r;
  uint8_t *g;
  uint8_t *b;

  for(i = 0; i < graphics.protected_pal_position; i++)
  {
    r = (uint8_t *)(dest++);
    g = (uint8_t *)(dest++);
    b = (uint8_t *)(dest++);
    get_rgb(i, r, g, b);
  }
}

void load_backup_palette(const char src[SMZX_PAL_SIZE * 3])
{
  load_palette_mem(src, SMZX_PAL_SIZE * 3);
}

void store_backup_indices(char dest[SMZX_PAL_SIZE * 4])
{
  memcpy(dest, graphics.smzx_indices, SMZX_PAL_SIZE * 4);
}

void load_backup_indices(const char src[SMZX_PAL_SIZE * 4])
{
  memcpy(graphics.smzx_indices, src, SMZX_PAL_SIZE * 4);
}

void save_palette(char *fname)
{
  vfile *pal_file = vfopen_unsafe(fname, "wb");

  if(pal_file)
  {
    int num_colors = PAL_SIZE;
    int i;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;

    for(i = 0; i < num_colors; i++)
    {
      vfputc(get_red_component(i), pal_file);
      vfputc(get_green_component(i), pal_file);
      vfputc(get_blue_component(i), pal_file);
    }

    vfclose(pal_file);
  }
}

void save_index_file(char *fname)
{
  vfile *idx_file = vfopen_unsafe(fname, "wb");

  if(idx_file)
  {
    int i;

    for(i = 0; i < SMZX_PAL_SIZE; i++)
    {
      vfputc(get_smzx_index(i, 0), idx_file);
      vfputc(get_smzx_index(i, 1), idx_file);
      vfputc(get_smzx_index(i, 2), idx_file);
      vfputc(get_smzx_index(i, 3), idx_file);
    }

    vfclose(idx_file);
  }
}

void ec_save_set_var(const char *name, uint16_t first_chr, unsigned int num)
{
  vfile *vf = vfopen_unsafe(name, "wb");

  if(vf)
  {
    if(num + first_chr > PROTECTED_CHARSET_POSITION)
    {
      if(first_chr > PROTECTED_CHARSET_POSITION)
        first_chr = PROTECTED_CHARSET_POSITION;

      num = PROTECTED_CHARSET_POSITION - first_chr;
    }

    vfwrite(graphics.charset + (first_chr * CHAR_SIZE), CHAR_SIZE, num, vf);
    vfclose(vf);
  }
}

void ec_change_block(uint8_t offset, uint8_t charset,
 uint8_t width, uint8_t height, const char *matrix)
{
  // Change a block of chars on the 32x8 charset
  int skip;
  int x;
  int y;

  width = MZX_CLAMP(width, 1, 32);
  height = MZX_CLAMP(height, 1, 8);

  skip = 32 - width;

  // No need to bound offset (uint8_t)
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {
      ec_change_char((charset * 256) + offset, matrix);
      matrix += CHAR_SIZE;
      offset++;
    }
    offset += skip;
  }
}

void ec_read_block(uint8_t offset, uint8_t charset,
 uint8_t width, uint8_t height, char *matrix)
{
  // Read a block of chars from the 32x8 charset
  int skip;
  int x;
  int y;

  width = MZX_CLAMP(width, 1, 32);
  height = MZX_CLAMP(height, 1, 8);

  skip = 32 - width;

  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {
      ec_read_char((charset * 256) + offset, matrix);
      matrix += CHAR_SIZE;
      offset++;
    }
    offset += skip;
  }
}

void load_editor_charsets(void)
{
  ec_load_set_secondary(mzx_res_get_by_id(MZX_ASCII_CHR), ascii_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_BLANK_CHR), blank_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_SMZX_CHR),  smzx_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_SMZX2_CHR), smzx_charset2);
}

void ec_load_smzx(void)
{
  ec_mem_load_set(smzx_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_smzx2(void)
{
  ec_mem_load_set(smzx_charset2, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_blank(void)
{
  ec_mem_load_set(blank_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_ascii(void)
{
  ec_mem_load_set(ascii_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_char_ascii(uint16_t char_number)
{
  unsigned int ascii_number = char_number & 0xFF;
  uint8_t *ascii_char = ascii_charset + ascii_number * CHAR_SIZE;

  ec_change_char(char_number, (char *)ascii_char);
}

void ec_load_char_mzx(uint16_t char_number)
{
  unsigned int default_number = char_number & 0xFF;
  uint8_t *default_char = graphics.default_charset + default_number * CHAR_SIZE;

  ec_change_char(char_number, (char *)default_char);
}

/**
 * Returns the number of pixels (from 0 to 112) that are the same between two
 * chars.
 */
unsigned int compare_char(uint16_t chr_a, uint16_t chr_b)
{
  uint8_t *a = graphics.charset + (chr_a % FULL_CHARSET_SIZE) * CHAR_SIZE;
  uint8_t *b = graphics.charset + (chr_b % FULL_CHARSET_SIZE) * CHAR_SIZE;
  unsigned int same = 0;
  int i;
  int mask;

  if(get_screen_mode())
  {
    for(i = 0; i < CHAR_SIZE; i++)
      for(mask = 0xC0; mask > 0; mask >>= 2)
        same += (a[i] & mask) == (b[i] & mask);

    same *= 2;
  }
  else
  {
    for(i = 0; i < CHAR_SIZE; i++)
      for(mask = 0x80; mask > 0; mask >>= 1)
        same += (a[i] & mask) == (b[i] & mask);
  }
  return same;
}
