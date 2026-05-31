/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2017-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "configure.h"
#include "error.h"
#include "graphics.h"
#include "world.h"
#include "io/pngops.h"
#include "io/vio.h"

#include "event/event.h"
#include "platform/platform.h"
#include "render/render.h"
#include "render/render_layer.h"
#include "render/renderers.h"

#ifdef CONFIG_SDL
#include "platform/sdl/SDLmzx.h"
#endif // CONFIG_SDL

#include "util.h"

#define CURSOR_BLINK_RATE 115

__editor_maybe_static struct graphics_data graphics;

static const struct rgb_color default_pal[16] =
{
  /* r, g, b, unused */
  { 0, 0, 0, 0 },
  { 0, 0, 170, 0 },
  { 0, 170, 0, 0 },
  { 0, 170, 170, 0 },
  { 170, 0, 0, 0 },
  { 170, 0, 170, 0 },
  { 170, 85, 0, 0 },
  { 170, 170, 170, 0 },
  { 85, 85, 85, 0 },
  { 85, 85, 255, 0 },
  { 85, 255, 85, 0 },
  { 85, 255, 255, 0 },
  { 255, 85, 85, 0 },
  { 255, 85, 255, 0 },
  { 255, 255, 85, 0 },
  { 255, 255, 255, 0 }
};

#if (NUM_CHARSETS < 16)
static boolean extended_charsets_check(boolean show_error, int pos, int count);
#else
static inline boolean extended_charsets_check(boolean s, int p, int c)
{
  return true;
}
#endif

static void remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  if(graphics->renderer->remap_charbyte)
    graphics->renderer->remap_charbyte(graphics, chr, byte);
}

static void remap_char(struct graphics_data *graphics, uint16_t chr)
{
  if(graphics->renderer->remap_char)
    graphics->renderer->remap_char(graphics, chr);
}

static void remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t len)
{
  if(graphics->renderer->remap_char_range)
    graphics->renderer->remap_char_range(graphics, first, len);
}

void ec_change_byte(uint16_t chr, uint8_t byte, uint8_t new_value)
{
  extended_charsets_check(true, chr, 1);

  chr = chr % PROTECTED_CHARSET_POSITION;
  graphics.charset[(chr * CHAR_SIZE) + byte] = new_value;

  remap_charbyte(&graphics, chr, byte);
}

void ec_change_char(uint16_t chr, const char matrix[CHAR_SIZE])
{
  extended_charsets_check(true, chr, 1);

  chr = chr % PROTECTED_CHARSET_POSITION;
  memcpy(graphics.charset + (chr * CHAR_SIZE), matrix, CHAR_SIZE);

  remap_char(&graphics, chr);
}

uint8_t ec_read_byte(uint16_t chr, uint8_t byte)
{
  extended_charsets_check(true, chr, 1);

  chr = chr % PROTECTED_CHARSET_POSITION;
  return graphics.charset[(chr * CHAR_SIZE) + byte];
}

void ec_read_char(uint16_t chr, char matrix[CHAR_SIZE])
{
  extended_charsets_check(true, chr, 1);

  chr = chr % PROTECTED_CHARSET_POSITION;
  memcpy(matrix, graphics.charset + (chr * CHAR_SIZE), CHAR_SIZE);
}

void ec_clear_set(void)
{
  memset(graphics.charset, 0, PROTECTED_CHARSET_POSITION * CHAR_SIZE);
  remap_char_range(&graphics, 0, FULL_CHARSET_SIZE);
}

boolean ec_load_set(const char *filename)
{
  vfile *vf = vfopen_unsafe(filename, "rb");
  if(vf)
  {
    int count = vfread(graphics.charset, CHAR_SIZE, PROTECTED_CHARSET_POSITION, vf);
    vfclose(vf);

    if(count > 0)
    {
      // some renderers may want to map charsets to textures
      remap_char_range(&graphics, 0, count);
      return true;
    }
  }
  return false;
}

__editor_maybe_static void ec_load_set_secondary(const char *filename,
 uint8_t dest[CHAR_SIZE * CHARSET_SIZE])
{
  vfile *vf = vfopen_unsafe(filename, "rb");
  if(vf)
  {
    int count = vfread(dest, CHAR_SIZE, CHARSET_SIZE, vf);
    vfclose(vf);

    // This might have been somewhere in the charset, so
    // some renderers may want to map charsets to textures
    if(count > 0)
      remap_char_range(&graphics, 0, FULL_CHARSET_SIZE);
  }
}

int ec_load_set_var(const char *filename, uint16_t first_chr, int version)
{
  vfile *vf = vfopen_unsafe(filename, "rb");
  if(vf)
  {
    int maxchars = PROTECTED_CHARSET_POSITION;
    int count;

    int size = vfilelength(vf, false) / CHAR_SIZE;

    if(version >= V290)
    {
      extended_charsets_check(true, first_chr, size);
    }
    else
      maxchars = 256;

    if(first_chr > maxchars)
    {
      vfclose(vf);
      return -1;
    }

    if(size + first_chr > maxchars)
      size = maxchars - first_chr;

    count = vfread(graphics.charset + (first_chr * CHAR_SIZE), CHAR_SIZE, size, vf);
    vfclose(vf);

    // some renderers may want to map charsets to textures
    if(count > 0)
      remap_char_range(&graphics, first_chr, count);

    return count;
  }
  return -1;
}

void ec_mem_load_set(const void *buffer, size_t len)
{
  // This is used only for legacy and ZIP world loading and the default charsets
  // Use ec_clear_set() in conjunction with this for world loads.
  size_t count;

  if(len > CHAR_SIZE * PROTECTED_CHARSET_POSITION)
    len = CHAR_SIZE * PROTECTED_CHARSET_POSITION;

  memcpy(graphics.charset, buffer, len);

  // some renderers may want to map charsets to textures
  count = len / CHAR_SIZE;
  if(count > 0)
    remap_char_range(&graphics, 0, count);
}

void ec_mem_load_set_var(const void *buffer, size_t len, uint16_t first_chr,
 int version)
{
  size_t maxchars = PROTECTED_CHARSET_POSITION;
  size_t offset = first_chr * CHAR_SIZE;
  size_t count = (len + CHAR_SIZE - 1) / CHAR_SIZE;

  if(version >= V290)
  {
    extended_charsets_check(true, first_chr, count);
  }
  else
    maxchars = 256;

  if(first_chr > maxchars)
    return;

  if(count > maxchars - first_chr)
  {
    count = maxchars - first_chr;
    len = count * CHAR_SIZE;
  }

  memcpy(graphics.charset + offset, buffer, len);

  // some renderers may want to map charsets to textures
  if(count > 0)
    remap_char_range(&graphics, first_chr, count);
}

void ec_mem_save_set_var(void *buffer, size_t len, uint16_t first_chr)
{
  size_t offset = first_chr * CHAR_SIZE;
  size_t size = MIN(PROTECTED_CHARSET_POSITION * CHAR_SIZE - offset, len);

  if(first_chr < PROTECTED_CHARSET_POSITION)
    memcpy(buffer, graphics.charset + offset, size);
}

__editor_maybe_static void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset, CHAR_SIZE * CHARSET_SIZE);
}

static void update_colors(struct rgb_color *palette, unsigned int count)
{
  graphics.renderer->update_colors(&graphics, palette, count);
}

static unsigned int make_palette(struct rgb_color *palette)
{
  unsigned int paletteSize;
  int i;

  // Is SMZX mode set?
  if(graphics.screen_mode)
  {
    // SMZX mode 1: set all colors to diagonals
    if(graphics.screen_mode == 1)
    {
      for(i = 0; i < SMZX_PAL_SIZE; i++)
      {
        palette[i].r =
         ((graphics.intensity_palette[i & 15].r << 1) +
          graphics.intensity_palette[i >> 4].r) / 3;
        palette[i].g =
         ((graphics.intensity_palette[i & 15].g << 1) +
          graphics.intensity_palette[i >> 4].g) / 3;
        palette[i].b =
         ((graphics.intensity_palette[i & 15].b << 1) +
          graphics.intensity_palette[i >> 4].b) / 3;
      }
    }
    else
    {
      memcpy(palette, graphics.intensity_palette,
       sizeof(struct rgb_color) * SMZX_PAL_SIZE);
    }
    paletteSize = SMZX_PAL_SIZE;
  }
  else
  {
    memcpy(palette, graphics.intensity_palette,
     sizeof(struct rgb_color) * PAL_SIZE);
    paletteSize = PAL_SIZE;
  }
  memcpy(palette + paletteSize, graphics.protected_palette,
   sizeof(struct rgb_color) * PROTECTED_PAL_SIZE);
  graphics.protected_pal_position = paletteSize;

  paletteSize += PROTECTED_PAL_SIZE;
  if(paletteSize > 256 && !layer_renderer_check(false))
  {
    // Renderers without layer support often have poor upport for large
    // palettes, so to be safe we restrict the palette size
    paletteSize = 256;
  }
  return paletteSize;
}

void update_palette(void)
{
  struct rgb_color new_palette[FULL_PAL_SIZE];
  update_colors(new_palette, make_palette(new_palette));
}

void default_palette(void)
{
  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);

  if(!graphics.fade_status)
  {
    memcpy(graphics.intensity_palette, default_pal,
     sizeof(struct rgb_color) * PAL_SIZE);
  }
  graphics.palette_dirty = true;
}

void default_protected_palette(void)
{
  memcpy(graphics.protected_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  graphics.palette_dirty = true;
}

static void init_palette(void)
{
  int i;

  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.protected_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memset(graphics.current_intensity, 0,
   sizeof(uint32_t) * PAL_SIZE);

  for(i = 0; i < SMZX_PAL_SIZE; i++)
    graphics.saved_intensity[i] = 100;

  graphics.fade_status = true;
  graphics.palette_dirty = true;
}

static int intensity(unsigned int component, unsigned int percent)
{
  component = (component * percent) / 100;

  if(component > 255)
    component = 255;

  return component;
}

void set_color_intensity(uint8_t color, unsigned int percent)
{
  if(graphics.fade_status)
  {
    graphics.saved_intensity[color] = percent;
  }
  else
  {
    int r = intensity(graphics.palette[color].r, percent);
    int g = intensity(graphics.palette[color].g, percent);
    int b = intensity(graphics.palette[color].b, percent);

    graphics.intensity_palette[color].r = r;
    graphics.intensity_palette[color].g = g;
    graphics.intensity_palette[color].b = b;

    graphics.current_intensity[color] = percent;
    graphics.palette_dirty = true;
  }
}

/** Set a color intensity in the MZX palette, even if SMZX is enabled. */
void set_color_intensity_mzx(uint8_t color, unsigned int percent)
{
  if(graphics.screen_mode >= 2)
  {
    graphics.backup_intensity[color] = percent;
  }
  else
    set_color_intensity(color, percent);
}

void set_palette_intensity(unsigned int percent)
{
  int i, num_colors;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  for(i = 0; i < num_colors; i++)
  {
    set_color_intensity(i, percent);
  }
  graphics.palette_dirty = true;
}

void set_rgb(uint8_t color, unsigned int r, unsigned int g, unsigned int b)
{
  uint32_t percent = graphics.current_intensity[color];
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;

  graphics.palette[color].r = r;
  graphics.intensity_palette[color].r = intensity(r, percent);

  graphics.palette[color].g = g;
  graphics.intensity_palette[color].g = intensity(g, percent);

  graphics.palette[color].b = b;
  graphics.intensity_palette[color].b = intensity(b, percent);
  graphics.palette_dirty = true;
}

/** Set a color in the MZX palette, even if SMZX is enabled. */
void set_rgb_mzx(uint8_t color, unsigned int r, unsigned int g, unsigned int b)
{
  if(graphics.screen_mode >= 2)
  {
    r = r * 255 / 63;
    g = g * 255 / 63;
    b = b * 255 / 63;
    graphics.backup_palette[color].r = r;
    graphics.backup_palette[color].g = g;
    graphics.backup_palette[color].b = b;
  }
  else
    set_rgb(color, r, g, b);
}

void set_protected_rgb(uint8_t color, unsigned int r, unsigned int g,
 unsigned int b)
{
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;
  graphics.protected_palette[color].r = r;
  graphics.protected_palette[color].g = g;
  graphics.protected_palette[color].b = b;
  graphics.palette_dirty = true;
}

void set_red_component(uint8_t color, unsigned int r)
{
  uint32_t percent = graphics.current_intensity[color];
  r = r * 255 / 63;

  graphics.palette[color].r = r;
  graphics.intensity_palette[color].r = intensity(r, percent);
  graphics.palette_dirty = true;
}

void set_green_component(uint8_t color, unsigned int g)
{
  uint32_t percent = graphics.current_intensity[color];
  g = g * 255 / 63;

  graphics.palette[color].g = g;
  graphics.intensity_palette[color].g = intensity(g, percent);
  graphics.palette_dirty = true;
}

void set_blue_component(uint8_t color, unsigned int b)
{
  uint32_t percent = graphics.current_intensity[color];
  b = b * 255 / 63;

  graphics.palette[color].b = b;
  graphics.intensity_palette[color].b = intensity(b, percent);
  graphics.palette_dirty = true;
}

unsigned int get_color_intensity(uint8_t color)
{
  if(graphics.fade_status)
    return graphics.saved_intensity[color];
  return graphics.current_intensity[color];
}

/** Get a color intensity from the MZX palette, even if SMZX is enabled. */
unsigned int get_color_intensity_mzx(uint8_t color)
{
  if(graphics.screen_mode >= 2)
  {
    return graphics.backup_intensity[color];
  }
  else
    return get_color_intensity(color);
}

void get_rgb(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
  *r = ((graphics.palette[color].r * 126) + 255) / 510;
  *g = ((graphics.palette[color].g * 126) + 255) / 510;
  *b = ((graphics.palette[color].b * 126) + 255) / 510;
}

/** Get a color from the MZX palette, even if SMZX is enabled. */
void get_rgb_mzx(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
  if(graphics.screen_mode >= 2)
  {
    *r = ((graphics.backup_palette[color].r * 126) + 255) / 510;
    *g = ((graphics.backup_palette[color].g * 126) + 255) / 510;
    *b = ((graphics.backup_palette[color].b * 126) + 255) / 510;
  }
  else
    get_rgb(color, r, g, b);
}

unsigned int get_red_component(uint8_t color)
{
  return ((graphics.palette[color].r * 126) + 255) / 510;
}

unsigned int get_green_component(uint8_t color)
{
  return ((graphics.palette[color].g * 126) + 255) / 510;
}

unsigned int get_blue_component(uint8_t color)
{
  return ((graphics.palette[color].b * 126) + 255) / 510;
}

static unsigned int get_smzx_index_offset(uint8_t palette, unsigned int index)
{
  index %= 4;

  if(index == 1)
    index = 2;
  else

  if(index == 2)
    index = 1;

  return (unsigned int)palette * 4 + index;
}

uint8_t get_smzx_index(uint8_t palette, unsigned int offset)
{
  offset = get_smzx_index_offset(palette, offset);

  return graphics.smzx_indices[offset];
}

void set_smzx_index(uint8_t palette, unsigned int offset, uint8_t color)
{
  // Setting the SMZX index is only supported for mode 3
  if(graphics.screen_mode != 3)
    return;

  offset = get_smzx_index_offset(palette, offset);

  graphics.smzx_indices[offset] = color % SMZX_PAL_SIZE;
  graphics.palette_dirty = true;
}

/**
 * Returns the effective luma in the range of [0,255] of a given color in the
 * game palette (0-255) or protected palette (256-271).
 */
__editor_maybe_static
int get_color_luma(unsigned int color)
{
  struct rgb_color rgb;
  unsigned int sum;

  if(color < 256)
    rgb = graphics.palette[color];
  else
    rgb = graphics.protected_palette[color % 16];

  sum = (unsigned int)rgb.r * 306u + /* 1024 * .299 */
        (unsigned int)rgb.g * 601u + /* 1024 * .587 */
        (unsigned int)rgb.b * 117u;  /* 1024 * .114 */

  return (int)((sum + 512u) / 1024u);
}

/**
 * Returns the average effective luma in the range of [0,255] of a given char
 * using a given game palette.
 */
__editor_maybe_static
int get_char_average_luma(uint16_t chr, uint8_t palette, int mode, int mask_chr)
{
  const uint8_t *char_data = graphics.charset + chr * CHAR_SIZE;
  const uint8_t *mask_values = NULL;
  boolean use_mask = false;
  uint8_t mask;
  int count = 0;
  int sum = 0;
  int x;
  int y;

  if(chr >= FULL_CHARSET_SIZE)
    return 0;

  if(mask_chr >= 0 && mask_chr < FULL_CHARSET_SIZE)
  {
    mask_values = graphics.charset + mask_chr * CHAR_SIZE;
    use_mask = true;
  }

  if(mode < 0)
    mode = graphics.screen_mode;

  if(mode)
  {
    int lumas[4];

    if(mode == 1)
    {
      // Due to quirks in mode 1, the interpolated colors 1 and 2 are not
      // actually stored in the palette. Conveniently, the stronger-weighted
      // user color can be derived from the lower bit (1->3, 2->0).
      lumas[0] = get_color_luma((palette & 0xF0) >> 4);
      lumas[3] = get_color_luma(palette & 0xF);
      lumas[1] = lumas[3];
      lumas[2] = lumas[0];
    }
    else
    {
      lumas[0] = get_color_luma(graphics.smzx_indices[palette * 4 + 0]);
      lumas[1] = get_color_luma(graphics.smzx_indices[palette * 4 + 1]);
      lumas[2] = get_color_luma(graphics.smzx_indices[palette * 4 + 2]);
      lumas[3] = get_color_luma(graphics.smzx_indices[palette * 4 + 3]);
    }

    for(y = 0; y < CHAR_H; y++)
    {
      for(x = 0, mask = 0xC0; x < CHAR_W; x += 2, mask >>= 2)
      {
        if(!use_mask || (mask_values[y] & mask))
        {
          sum += lumas[(char_data[y] & mask) >> (6 - x)];
          count++;
        }
      }
    }
  }
  else
  {
    int lumas[2] =
    {
      get_color_luma((palette & 0xF0) >> 4),
      get_color_luma(palette & 0xF),
    };

    for(y = 0; y < CHAR_H; y++)
    {
      for(x = 0, mask = 0x80; x < CHAR_W; x++, mask >>= 1)
      {
        if(!use_mask || (mask_values[y] & mask))
        {
          sum += lumas[(char_data[y] & mask) >> (7 - x)];
          count++;
        }
      }
    }
  }
  return (sum + count / 2) / count;
}

void load_palette(const char *filename)
{
  int file_size, i, r, g, b;
  vfile *pal_file;

  pal_file = vfopen_unsafe(filename, "rb");
  if(!pal_file)
    return;

  file_size = vfilelength(pal_file, false);

  switch(graphics.screen_mode)
  {
    // Regular text mode, 16 colors
    case 0:
      file_size = MIN(file_size, 16 * 3);
      break;

    // SMZX modes, up to 256 colors
    default:
      file_size = MIN(file_size, 256 * 3);
      break;
  }

  for(i = 0; i < file_size / 3; i++)
  {
    r = vfgetc(pal_file);
    g = vfgetc(pal_file);
    b = vfgetc(pal_file);
    set_rgb(i, r, g, b);
  }

  vfclose(pal_file);
}

void load_palette_mem(const void *buffer, size_t len)
{
  const uint8_t *pal = buffer;
  int size, r, g, b, i, j;

  switch(graphics.screen_mode)
  {
    // Regular text mode
    case 0:
      size = MIN(len, 16*3);
      break;

    // SMZX
    default:
      size = MIN(len, 256*3);
      break;
  }

  for(i = 0, j = 0; j+2 < size; i++)
  {
    r = pal[j++];
    g = pal[j++];
    b = pal[j++];
    set_rgb(i, r, g, b);
  }
}

void load_index_file(const char *filename)
{
  vfile *idx_file;
  int i;

  if(get_screen_mode() != 3)
    return;

  idx_file = vfopen_unsafe(filename, "rb");
  if(idx_file)
  {
    for(i = 0; i < SMZX_PAL_SIZE; i++)
    {
      set_smzx_index(i, 0, vfgetc(idx_file));
      set_smzx_index(i, 1, vfgetc(idx_file));
      set_smzx_index(i, 2, vfgetc(idx_file));
      set_smzx_index(i, 3, vfgetc(idx_file));
    }

    vfclose(idx_file);
  }
}

void save_indices(void *buffer)
{
  uint8_t *copy_buffer = buffer;
  int i;

  if(get_screen_mode() != 3)
    return;

  for(i = 0; i < SMZX_PAL_SIZE; i++)
  {
    *(copy_buffer++) = get_smzx_index(i, 0);
    *(copy_buffer++) = get_smzx_index(i, 1);
    *(copy_buffer++) = get_smzx_index(i, 2);
    *(copy_buffer++) = get_smzx_index(i, 3);
  }
}

void load_indices(const void *buffer, size_t size)
{
  const uint8_t *copy_buffer = buffer;
  unsigned int i;

  if(get_screen_mode() != 3)
    return;

  if(size > SMZX_PAL_SIZE * 4)
    size = SMZX_PAL_SIZE * 4;

  // Truncate incomplete colors
  size /= 4;

  for(i = 0; i < size; i++)
  {
    set_smzx_index(i, 0, *(copy_buffer++));
    set_smzx_index(i, 1, *(copy_buffer++));
    set_smzx_index(i, 2, *(copy_buffer++));
    set_smzx_index(i, 3, *(copy_buffer++));
  }
}

void load_indices_direct(const void *buffer, size_t size)
{
  if(size > SMZX_PAL_SIZE * 4)
    size = SMZX_PAL_SIZE * 4;

  memcpy(graphics.smzx_indices, buffer, size);
  graphics.palette_dirty = true;
}

void smzx_palette_loaded(boolean is_loaded)
{
  graphics.default_smzx_loaded = is_loaded;
}

static void update_intensity_palette(void)
{
  int i;
  for(i = 0; i < SMZX_PAL_SIZE; i++)
  {
    set_color_intensity(i, graphics.current_intensity[i]);
  }
}

static void swap_palettes(void)
{
  struct rgb_color temp_colors[SMZX_PAL_SIZE];
  uint32_t temp_intensities[SMZX_PAL_SIZE];
  memcpy(temp_colors, graphics.backup_palette,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(graphics.backup_palette, graphics.palette,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(graphics.palette, temp_colors,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(temp_intensities, graphics.backup_intensity,
   sizeof(uint32_t) * SMZX_PAL_SIZE);
  if(graphics.fade_status)
  {
    memcpy(graphics.backup_intensity,
     graphics.saved_intensity, sizeof(uint32_t) * SMZX_PAL_SIZE);
    memcpy(graphics.saved_intensity, temp_intensities,
     sizeof(uint32_t) * SMZX_PAL_SIZE);
  }
  else
  {
    memcpy(graphics.backup_intensity,
     graphics.current_intensity, sizeof(uint32_t) * SMZX_PAL_SIZE);
    memcpy(graphics.current_intensity, temp_intensities,
     sizeof(uint32_t) * SMZX_PAL_SIZE);
    update_intensity_palette();
  }
}

static void fix_layer_screen_mode(void)
{
  // Fix the screen mode for all active layers except the UI_LAYER.
  unsigned int i;
  for(i = 0; i < graphics.layer_count; i++)
    graphics.video_layers[i].mode = graphics.screen_mode;

  graphics.video_layers[UI_LAYER].mode = 0;
}

void set_screen_mode(unsigned int mode)
{
  int i;
  uint8_t *pal_idx;
  char bg, fg;
  mode %= 4;

  if((mode >= 2) && (graphics.screen_mode < 2))
  {
    swap_palettes();
    graphics.screen_mode = mode;
    if(!graphics.default_smzx_loaded)
    {
      if(graphics.fade_status)
      {
        for(i = 0; i < SMZX_PAL_SIZE; i++)
        {
          graphics.current_intensity[i] = 0;
        }
      }
      set_palette_intensity(100);
      load_palette(mzx_res_get_by_id(SMZX_PAL));
      graphics.default_smzx_loaded = true;
    }
  }
  else

  if((graphics.screen_mode >= 2) && (mode < 2))
  {
    swap_palettes();

    graphics.screen_mode = mode;
  }
  else
  {
    graphics.screen_mode = mode;
  }

  pal_idx = graphics.smzx_indices;
  if(mode == 1 || mode == 2)
  {
    for(i = 0; i < 256; i++)
    {
      bg = (i & 0xF0) >> 4;
      fg = i & 0x0F;
      pal_idx[0] = (bg << 4) | bg;
      pal_idx[1] = (bg << 4) | fg;
      pal_idx[2] = (fg << 4) | bg;
      pal_idx[3] = (fg << 4) | fg;
      pal_idx += 4;
    }
  }
  else

  if(mode == 3)
  {
    for(i = 0; i < 256; i++)
    {
      pal_idx[0] = (i + 0) & 0xFF;
      pal_idx[1] = (i + 2) & 0xFF;
      pal_idx[2] = (i + 1) & 0xFF;
      pal_idx[3] = (i + 3) & 0xFF;
      pal_idx += 4;
    }
  }

  fix_layer_screen_mode();
  graphics.palette_dirty = true;
  graphics.smzx_dirty = true;
}

unsigned int get_screen_mode(void)
{
  return graphics.screen_mode;
}

/**
 * The cursor needs to be reasonably visible whenever possible. A good deal of
 * the time this is possible with the classic cursor behavior of using the
 * foreground color of whatever is beneath it. For completely solid chars, the
 * background color is instead used.
 *
 * This behavior doesn't work very well when the foreground and background
 * colors are very close (modes 0 and 1), so instead we switch the cursor to
 * protected white or black. Foreground and background have no meaning in modes
 * 2 and 3, so always use protected white or black in these modes.
 */

static unsigned int get_cursor_color(void)
{
  struct char_element *cursor_element =
   graphics.text_video + graphics.cursor_x + (graphics.cursor_y * SCREEN_W);
  unsigned int cursor_char = cursor_element->char_value;
  unsigned int fg_color = cursor_element->fg_color;
  unsigned int bg_color = cursor_element->bg_color;
  unsigned int cursor_color;
  int i;

  if(bg_color >= 0x10)
  {
    // UI- use protected palette white or black.
    if(bg_color <= 0x19)
      cursor_color = graphics.protected_pal_position + 0x0F;

    else
      cursor_color = graphics.protected_pal_position;
  }
  else

  if(graphics.screen_mode <= 1)
  {
    // Modes 0 and 1- use the (modified) classic cursor color logic.
    // NOTE: there was a trick using uint32_t * here before that caused
    // misalignment crashes on some platforms.
    uint8_t *offset = graphics.charset + cursor_char * CHAR_SIZE;
    int cursor_solid = 0xFF;

    // Choose FG by default.
    cursor_color = fg_color;

    // If the char under the cursor is completely solid, use the BG instead.
    for(i = 0; i < CHAR_SIZE; i++)
    {
      cursor_solid &= *offset;
      offset++;
    }

    if(cursor_solid == 0xFF)
      cursor_color = bg_color;

    if(fg_color < 0x10 && bg_color < 0x10)
    {
      // If the fg and bg colors are game colors and close in brightness/the
      // same, neither color fits well, so choose a protected palette color.
      int fg_luma = get_color_luma(fg_color);
      int bg_luma = get_color_luma(bg_color);

      if(abs(fg_luma - bg_luma) < 32)
      {
        cursor_color = graphics.protected_pal_position;

        if(fg_luma + bg_luma < 256)
          cursor_color |= 0x0F;
      }
    }

    // Offset adjust protected colors if necessary.
    if(cursor_color >= 0x10)
    {
      cursor_color = graphics.protected_pal_position + (cursor_color & 0x0F);
    }
    else

    // Offset adjust mode 1 colors if necessary.
    if(graphics.screen_mode == 1)
      cursor_color = (cursor_color << 4) | (cursor_color & 0x0F);
  }

  else
  {
    // Modes 2 and 3- pick protected white or black based on the average luma.
    unsigned int pal;
    int avg;

    bg_color &= 0x0F;
    fg_color &= 0x0F;
    pal = (bg_color << 4) | fg_color;

    avg = get_char_average_luma(cursor_char, pal, graphics.screen_mode, -1);

    cursor_color = graphics.protected_pal_position;

    if(avg < 128)
      cursor_color |= 0x0F;
  }

  return cursor_color;
}

#ifndef CONFIG_NO_LAYER_RENDERING
static int compare_layers(const void *a, const void *b)
{
  return (*(struct video_layer * const *)a)->draw_order -
    (*(struct video_layer * const *)b)->draw_order;
}
#endif

void update_screen(void)
{
  uint32_t ticks = get_ticks();

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if(graphics.smzx_dirty)
  {
    /* Request an SMZX mode change from the renderer, if applicable (EGA).
     * Hardware SMZX may reset various text mode settings, so do it first.
     */
    graphics.smzx_dirty = false;
    if(graphics.renderer->set_screen_mode)
      graphics.renderer->set_screen_mode(&graphics, graphics.screen_mode);
  }

  if(graphics.palette_dirty)
  {
    update_palette();
    graphics.palette_dirty = false;
  }

#ifndef CONFIG_NO_LAYER_RENDERING
  if(graphics.requires_extended && graphics.renderer->render_layer)
  {
    uint32_t layer;
    for(layer = 0; layer < graphics.layer_count; layer++)
    {
      graphics.sorted_video_layers[layer] = &graphics.video_layers[layer];
    }

    qsort(graphics.sorted_video_layers, graphics.layer_count,
     sizeof(struct video_layer *), compare_layers);

    for(layer = 0; layer < graphics.layer_count; layer++)
    {
      if(graphics.sorted_video_layers[layer]->data &&
       !graphics.sorted_video_layers[layer]->empty)
        graphics.renderer->render_layer(&graphics,
         graphics.sorted_video_layers[layer]);
    }
  }
  else
#endif
  if(graphics.renderer->render_graph)
  {
    // Fallback if the layer renderer is unavailable or unnecessary
    graphics.renderer->render_graph(&graphics);
  }
#ifndef CONFIG_NO_LAYER_RENDERING
  else

  if(graphics.renderer->render_layer)
  {
    // Fallback using the layer renderer
    graphics.text_video_layer.mode = graphics.screen_mode;
    graphics.text_video_layer.data = graphics.text_video;

    graphics.renderer->render_layer(&graphics, &(graphics.text_video_layer));
  }
#endif

  if(graphics.renderer->render_cursor || graphics.renderer->hardware_cursor)
  {
    unsigned int cursor_color = get_cursor_color();
    unsigned int offset = 0;
    unsigned int lines = 0;
    boolean enabled = true;

    switch(graphics.cursor_mode)
    {
      case CURSOR_MODE_UNDERLINE:
        lines = 2;
        offset = 12;
        break;
      case CURSOR_MODE_SOLID:
        lines = 14;
        offset = 0;
        break;
      case CURSOR_MODE_HINT:
        break;
      case CURSOR_MODE_INVISIBLE:
      default:
        enabled = false;
        break;
    }

    // Try to render the standard software cursor first.
    if(graphics.renderer->render_cursor && enabled && graphics.cursor_flipflop)
    {
      graphics.renderer->render_cursor(&graphics,
       graphics.cursor_x, graphics.cursor_y, cursor_color, lines, offset);
    }
    else

    // Other platforms may use a hardware cursor instead that may need to be
    // updated any frame regardless of the cursor state and blinking.
    if(graphics.renderer->hardware_cursor)
    {
      graphics.renderer->hardware_cursor(&graphics,
       graphics.cursor_x, graphics.cursor_y, cursor_color, lines, offset, enabled);
    }
  }

  if(graphics.mouse_status &&
   graphics.system_mouse != SYSTEM_MOUSE_HIDE_SOFTWARE_MOUSE)
  {
    int mouse_x, mouse_y;
    get_mouse_pixel_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width) * graphics.mouse_width;
    mouse_y = (mouse_y / graphics.mouse_height) * graphics.mouse_height;

    graphics.renderer->render_mouse(&graphics, mouse_x, mouse_y,
     graphics.mouse_width, graphics.mouse_height);
  }

  graphics.renderer->sync_screen(&graphics, &(graphics.window));
}

boolean get_fade_status(void)
{
  return graphics.fade_status;
}

void dialog_fadein(void)
{
  graphics.dialog_fade_status = get_fade_status();
  if(graphics.dialog_fade_status)
  {
    clear_screen();
    insta_fadein();
  }
}

void dialog_fadeout(void)
{
  if(graphics.dialog_fade_status)
  {
    insta_fadeout();
  }
}

// Very quick fade out. Saves intensity table for fade in. Be sure
// to use in conjuction with the next function.
void vquick_fadeout(void)
{
  if(!has_video_initialized())
  {
    // If we're running without video there's no point waiting 11 frames.
    insta_fadeout();
    return;
  }

  if(!graphics.fade_status)
  {
    int i, i2, num_colors;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

    memcpy(graphics.saved_intensity, graphics.current_intensity,
     sizeof(uint32_t) * num_colors);

    for(i = 10; i >= 0; i--)
    {
      uint32_t ticks = get_ticks();

      for(i2 = 0; i2 < num_colors; i2++)
        set_color_intensity(i2, (graphics.saved_intensity[i2] * i / 10));

      graphics.palette_dirty = true;
      update_screen();

      ticks = get_ticks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
    graphics.fade_status = true;
  }
}

// Very quick fade in. Uses intensity table saved from fade out. For
// use in conjuction with the previous function.
void vquick_fadein(void)
{
  if(!has_video_initialized())
  {
    // If we're running without video there's no point waiting 11 frames.
    insta_fadein();
    return;
  }

  if(graphics.fade_status)
  {
    unsigned int i, i2, num_colors;

    graphics.fade_status = false;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

    for(i = 0; i <= 10; i++)
    {
      uint32_t ticks = get_ticks();

      for(i2 = 0; i2 < num_colors; i2++)
        set_color_intensity(i2, (graphics.saved_intensity[i2] * i / 10));

      graphics.palette_dirty = true;
      update_screen();

      ticks = get_ticks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
  }
}

// Instant fade out
void insta_fadeout(void)
{
  unsigned int i, num_colors;

  if(graphics.fade_status)
    return;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  memcpy(graphics.saved_intensity, graphics.current_intensity,
   sizeof(uint32_t) * num_colors);

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, 0);

  graphics.palette_dirty = true;
  update_screen(); // NOTE: this was called conditionally in 2.81e

  graphics.fade_status = true;
}

// Instant fade in
void insta_fadein(void)
{
  unsigned int i, num_colors;

  if(!graphics.fade_status)
    return;

  graphics.fade_status = false;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, graphics.saved_intensity[i]);

  graphics.palette_dirty = true;
  update_screen(); // NOTE: this was called conditionally in 2.81e
}

const char *video_get_default_caption(void)
{
  return graphics.default_caption;
}

static void new_empty_layer(struct video_layer *layer, int x, int y,
 unsigned int w, unsigned int h, int draw_order)
{
  // Layers are persistent and static
  if(!layer->data || layer->w != w || layer->h != h)
    layer->data = crealloc(layer->data, sizeof(struct char_element) * w * h);

  layer->w = w;
  layer->h = h;
  layer->x = x;
  layer->y = y;
  layer->mode = graphics.screen_mode;
  layer->draw_order = draw_order;
  layer->transparent_col = -1;
  layer->offset = 0;
}

uint32_t create_layer(int x, int y, unsigned int w, unsigned int h,
 int draw_order, int t_col, int offset, boolean unbound)
{
  uint32_t layer_idx = graphics.layer_count;
  struct video_layer *layer = &graphics.video_layers[layer_idx];

  new_empty_layer(layer, x, y, w, h, draw_order);
  memset(layer->data, 0xFF, sizeof(struct char_element) * w * h);
  layer->empty = true;
  layer->transparent_col = t_col;
  layer->offset = offset;
  graphics.layer_count++;

  // This shouldn't ever happen, but just in case...
  if(graphics.current_layer == layer_idx)
    select_layer(layer_idx);

  if(!graphics.requires_extended && unbound)
    graphics.requires_extended = true;

  return layer_idx;
}

void set_layer_offset(uint32_t layer, int offset)
{
  graphics.video_layers[layer].offset = offset;
}

void set_layer_mode(uint32_t layer, int mode)
{
  // In general, we want the layer to use the screen mode, but some
  // UI elements need to be able to change this.
  graphics.video_layers[layer].mode = mode;
}

void move_layer(uint32_t layer, int x, int y)
{
  graphics.video_layers[layer].x = x;
  graphics.video_layers[layer].y = y;
  if(!graphics.requires_extended && (x % CHAR_W != 0 || y % CHAR_H != 0))
    graphics.requires_extended = true;
}

static void init_layers(void)
{
  new_empty_layer(&graphics.video_layers[BOARD_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_BOARD);
  new_empty_layer(&graphics.video_layers[OVERLAY_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_OVERLAY);
  new_empty_layer(&graphics.video_layers[GAME_UI_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_GAME_UI);
  new_empty_layer(&graphics.video_layers[UI_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_UI);

  select_layer(UI_LAYER);

  graphics.layer_count = NUM_DEFAULT_LAYERS;
  graphics.layer_count_prev = graphics.layer_count;
  blank_layers();
}

void select_layer(uint32_t layer_id)
{
  struct video_layer *layer = &graphics.video_layers[layer_id];
  graphics.current_layer = layer_id;
  graphics.current_video = layer->data;
  graphics.current_video_end = layer->data + (layer->w * layer->h);
}

void blank_layers(void)
{
  // This clears the default layers and deletes all other layers

  memset(graphics.video_layers[BOARD_LAYER].data, 0x00,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[OVERLAY_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[GAME_UI_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[UI_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);

  graphics.video_layers[BOARD_LAYER].empty = false;
  graphics.video_layers[OVERLAY_LAYER].empty = true;
  graphics.video_layers[GAME_UI_LAYER].empty = true;
  graphics.video_layers[UI_LAYER].empty = true;

  // Delete the rest of the layers
  destruct_extra_layers(0);

  // Since the layers were cleared, their screen mode values need to be reset.
  fix_layer_screen_mode();
}

void destruct_extra_layers(uint32_t first)
{
  // Delete layers that have not persisted since the previous frame and
  // make all extra layers available for use.
  uint32_t i;

  if(first < NUM_DEFAULT_LAYERS)
    first = NUM_DEFAULT_LAYERS;

  if(graphics.layer_count_prev > graphics.layer_count)
  {
    for(i = graphics.layer_count; i < graphics.layer_count_prev; i++)
    {
      free(graphics.video_layers[i].data);
      graphics.video_layers[i].data = NULL;
    }
  }
  graphics.layer_count_prev = graphics.layer_count;

  if(graphics.layer_count > first)
    graphics.layer_count = first;

  // Extended graphics may be no longer needed after destroying layers
  if(graphics.layer_count == NUM_DEFAULT_LAYERS)
    graphics.requires_extended = false;
}

void destruct_layers(void)
{
  uint32_t i;
  for(i = 0; i < TEXTVIDEO_LAYERS; i++)
  {
    if(graphics.video_layers[i].data)
    {
      free(graphics.video_layers[i].data);
      graphics.video_layers[i].data = NULL;
    }
  }
  graphics.layer_count_prev = 0;
  graphics.layer_count = 0;
}

static boolean try_init_video(struct config_info *conf)
{
  if(graphics.renderer->init_video(&graphics, conf))
  {
    if(video_create_window())
      return true;

    if(graphics.renderer->free_video)
      graphics.renderer->free_video(&graphics);
  }
  return false;
}

boolean init_video(struct config_info *conf, const char *caption)
{
  graphics.screen_mode = 0;
  graphics.fullscreen = conf->fullscreen;
  graphics.fullscreen_windowed = conf->fullscreen_windowed;
  graphics.resolution_width = conf->resolution_width;
  graphics.resolution_height = conf->resolution_height;
  graphics.window_width = conf->window_width;
  graphics.window_height = conf->window_height;
  graphics.mouse_status = false;
  graphics.cursor_hint_mode = conf->cursor_hint_mode;
  graphics.cursor_timestamp = get_ticks();
  graphics.cursor_flipflop = 1;
  graphics.system_mouse = conf->system_mouse;
  graphics.grab_mouse = conf->grab_mouse;
  graphics.disable_screensaver = conf->disable_screensaver;

  memset(&(graphics.text_video_layer), 0, sizeof(struct video_layer));
  graphics.text_video_layer.w = SCREEN_W;
  graphics.text_video_layer.h = SCREEN_H;
  graphics.text_video_layer.transparent_col = -1;

  init_layers();

  if(!set_current_renderer(&graphics, conf->video_output))
    return false;

  if(conf->resolution_width <= 0 || conf->resolution_height <= 0)
  {
    // The driver should attempt to pick the best resolution automatically.
    // Not all create_window implementations support this.
    graphics.resolution_width = 0;
    graphics.resolution_height = 0;
  }

  if(conf->window_width <= 0 || conf->window_height <= 0)
  {
    graphics.window_width = 640;
    graphics.window_height = 350;
  }

  snprintf(graphics.default_caption, 32, "%s", caption);

  if(!try_init_video(conf))
  {
    // Try falling back to the first registered renderer
    debug("Failed to initialize '%s', attempting fallback.\n", conf->video_output);
    strcpy(conf->video_output, "");
    if(!set_current_renderer(&graphics, conf->video_output))
      return false;

    if(!try_init_video(conf))
    {
      // One last attempt with the "safest" settings.
      // NOTE: this was originally done in set_video_mode.
      debug("Failed to initialize fallback, trying 640x350.\n");
      graphics.window_width = 640;
      graphics.window_height = 350;
      graphics.fullscreen = false;
      graphics.allow_resize = false;
      conf->force_bpp = BPP_AUTO;

      if(!try_init_video(conf))
      {
        warn("Failed to initialize video.\n");
        return false;
      }
    }
  }

  ec_load_set_secondary(mzx_res_get_by_id(MZX_DEFAULT_CHR),
   graphics.default_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_EDIT_CHR),
   graphics.charset + (PROTECTED_CHARSET_POSITION * CHAR_SIZE));

  ec_clear_set();
  ec_load_mzx();
  init_palette();
  graphics.is_initialized = true;
  return true;
}

void quit_video(void)
{
  if(graphics.renderer->free_video)
    graphics.renderer->free_video(&graphics);

  destruct_layers();
  graphics.window.is_init = false;
}

boolean has_video_initialized(void)
{
#ifdef CONFIG_SDL
  // Dummy SDL driver should act as headless.
  const char *sdl_driver = SDL_GetCurrentVideoDriver();
  if(sdl_driver && !strcmp(sdl_driver, "dummy")) return false;
#endif /* CONFIG_SDL */

  // Renderers can also report as headless.
  if(graphics.window.is_init && graphics.window.is_headless)
    return false;

  return graphics.is_initialized;
}

static void set_window_ratio(struct video_window *window, enum ratio_type ratio)
{
  switch(ratio)
  {
    case RATIO_MODERN_64_35:
      window->ratio_numerator = 64;
      window->ratio_denominator = 35;
      break;
    case RATIO_CLASSIC_4_3:
      window->ratio_numerator = 4;
      window->ratio_denominator = 3;
      break;
    default:
    case RATIO_STRETCH:
      window->ratio_numerator = 0;
      window->ratio_denominator = 0;
      break;
  }
}

unsigned video_create_window(void)
{
  struct video_window *window = &(graphics.window);

  if(window->is_init)
    return false;

  window->bits_per_pixel = graphics.bits_per_pixel;
  window->is_fullscreen = graphics.fullscreen;
  window->is_fullscreen_windowed = graphics.fullscreen_windowed;
  window->is_headless = false;
  window->allow_resize = graphics.allow_resize;
  set_window_ratio(window, graphics.ratio);

  if(graphics.fullscreen)
  {
    window->width_px = graphics.resolution_width;
    window->height_px = graphics.resolution_height;
  }
  else
  {
    window->width_px = graphics.window_width;
    window->height_px = graphics.window_height;
  }
  video_window_update_viewport(window);

  if(graphics.renderer->create_window(&graphics, window))
  {
    window->is_init = true;
    video_set_window_caption(window, graphics.default_caption);
    video_set_window_icon(window, mzx_res_get_by_id(MZX_ICON_PNG));

    // Make sure a BPP was selected by the renderer (if applicable).
    if(window->bits_per_pixel == BPP_AUTO)
      warn("renderer->create_window must auto-select BPP! Report this!\n");

    return 1;
  }
  return 0;
}

static boolean valid_window_id(unsigned window_id)
{
  return window_id == 1;
}

const struct video_window *video_get_window(unsigned window_id)
{
  if(valid_window_id(window_id))
    return &graphics.window;

  return NULL;
}

unsigned video_window_by_platform_id(unsigned platform_id)
{
  if(graphics.window.platform_id == platform_id)
    return 1;

  return 0;
}

/**
 * Update the scaled viewport coordinates within the window according to
 * the window's scaling ratio, for use in SDL update rectangles or glViewport.
 * The viewport is also used to translate mouse coordinates between screen
 * and window space.
 *
 * This should be called by the renderer any time create_window forces the
 * window size (SDL window creation functions already handle this).
 * This is called automatically between resize_window and resize_callback.
 *
 * For best results, call glViewport (or equivalent) in either
 * resize_callback or sync_screen.
 */
void video_window_update_viewport(struct video_window *window)
{
  if(graphics.renderer->set_viewport)
  {
    graphics.renderer->set_viewport(&graphics, window);
  }
  else
  {
    debug("Implement set_viewport!");
    window->viewport_x = 0;
    window->viewport_y = 0;
    window->viewport_width = window->width_px;
    window->viewport_height = window->height_px;
    window->is_integer_scaled = false;
  }
}

/* Sync the current window size after it has ALREADY been changed by the
 * platform or by a call to a graphics.c window resize function.
 * SDL2/3 reports a window resize event after it is resized;
 * this is the only situation in which this should be called anywhere else. */
void video_sync_window_size(unsigned window_id,
 unsigned new_width_px, unsigned new_height_px)
{
  if(valid_window_id(window_id))
  {
    graphics.window.width_px = new_width_px;
    graphics.window.height_px = new_height_px;

    if(graphics.window.is_fullscreen)
    {
      graphics.resolution_width = new_width_px;
      graphics.resolution_height = new_height_px;
    }
    else
    {
      graphics.window_width = new_width_px;
      graphics.window_height = new_height_px;
    }
    video_window_update_viewport(&(graphics.window));

    if(graphics.renderer->resize_callback)
      graphics.renderer->resize_callback(&graphics, &graphics.window);

    graphics.palette_dirty = true;
    update_screen();
  }
}

static void resize_window(unsigned window_id, boolean fullscreen)
{
  if(graphics.renderer->resize_window)
  {
    if(fullscreen)
    {
      graphics.window.width_px = graphics.resolution_width;
      graphics.window.height_px = graphics.resolution_height;
    }
    else
    {
      graphics.window.width_px = graphics.window_width;
      graphics.window.height_px = graphics.window_height;
    }
    graphics.window.is_fullscreen = fullscreen;
    graphics.fullscreen = fullscreen;
    set_window_ratio(&graphics.window, graphics.ratio);

    graphics.renderer->resize_window(&graphics, &graphics.window);

    // Renderer resize_window may have modified the requested dimensions.
    video_sync_window_size(window_id,
     graphics.window.width_px, graphics.window.height_px);
  }
}

/* Change the current window size. This should NOT be called in response to
 * SDL2/3 window resize events, but when something in MegaZeux explicitly
 * wants the window size to change. If currently in fullscreen, this switches
 * to windowed mode. */
void video_resize_window(unsigned window_id,
 unsigned new_width_px, unsigned new_height_px)
{
  if(graphics.renderer->resize_window && valid_window_id(window_id))
  {
    graphics.window_width = new_width_px;
    graphics.window_height = new_height_px;
    resize_window(window_id, false);
  }
}

/* Set the current fullscreen size and switch to fullscreen mode. */
void video_resize_fullscreen(unsigned window_id,
 unsigned new_width_px, unsigned new_height_px)
{
  if(graphics.renderer->resize_window && valid_window_id(window_id))
  {
    graphics.resolution_width = new_width_px;
    graphics.resolution_height = new_height_px;
    resize_window(window_id, true);
  }
}

void video_toggle_fullscreen(void)
{
  unsigned current_fullscreen = video_get_fullscreen_window();
  if(current_fullscreen)
    resize_window(current_fullscreen, false);
  else
    resize_window(1, true);
}

unsigned video_get_fullscreen_window(void)
{
  if(graphics.window.is_fullscreen)
    return 1;
  return 0;
}

boolean video_is_fullscreen(void)
{
  return video_get_fullscreen_window() > 0;
}

boolean video_set_window_caption(struct video_window *window, const char *caption)
{
  if(!window)
    window = &(graphics.window);

  if(graphics.renderer->set_window_caption)
  {
    if(!graphics.renderer->set_window_caption(&graphics, window, caption))
    {
      debug("--VIDEO-- video_set_window_caption failed: %s\n",
       caption ? caption : "NULL");
      return false;
    }
    trace("--VIDEO-- video_set_window_caption: %s\n", caption ? caption : "NULL");
  }
  return true;
}

boolean video_set_window_icon(struct video_window *window, const char *icon_path)
{
  if(!window)
    window = &(graphics.window);

  if(graphics.renderer->set_window_icon)
  {
    if(!graphics.renderer->set_window_icon(&graphics, window, icon_path))
    {
      debug("--VIDEO-- video_set_window_icon failed: %s\n",
       icon_path ? icon_path : "NULL");
      return false;
    }
    trace("--VIDEO-- video_set_window_icon: %s\n", icon_path ? icon_path : "NULL");
  }
  return true;
}

boolean change_video_output(struct config_info *conf, const char *output)
{
  char old_video_output[sizeof(conf->video_output)];
  size_t len = sizeof(conf->video_output);
  boolean fallback = false;
  boolean retval = true;

  memcpy(old_video_output, conf->video_output, len);
  old_video_output[len - 1] = 0;

  snprintf(conf->video_output, len, "%s", output);
  conf->video_output[len - 1] = 0;

  if(graphics.renderer->free_video)
    graphics.renderer->free_video(&graphics);

  set_current_renderer(&graphics, conf->video_output);

  if(!try_init_video(conf))
  {
    retval = false;

    memcpy(conf->video_output, old_video_output, len);
    if(!set_current_renderer(&graphics, conf->video_output))
    {
      warn("Failed to roll back renderer!\n");
      fallback = true;
    }
    else

    if(!try_init_video(conf))
    {
      warn("Failed to roll back video mode!\n");
      fallback = true;
    }
  }

  if(fallback)
  {
    // Attempt the first renderer in the list (unless that just failed).
    if(!strcmp(conf->video_output, renderers_available[0]->name))
    {
      warn("Aborting!\n");
      exit(1);
    }

    snprintf(conf->video_output, len, "%s", renderers_available[0]->name);
    if(!set_current_renderer(&graphics, conf->video_output))
    {
      warn("Failed to load fallback renderer, aborting!\n");
      exit(1);
    }

    if(!try_init_video(conf))
    {
      warn("Failed to set fallback video mode, aborting!\n");
      exit(1);
    }
  }

  update_palette();
  return retval;
}

int get_available_video_output_list(const char **buffer, int buffer_len)
{
  int i;

  for(i = 0; i < buffer_len; i++)
  {
    if(!renderers_available[i])
      break;

    buffer[i] = renderers_available[i]->name;
  }
  return i;
}

int get_current_video_output(void)
{
  return graphics.renderer_num;
}

static void dirty_ui(void)
{
  if(graphics.requires_extended) return;
  if(graphics.current_layer != UI_LAYER) return;
  if(graphics.screen_mode == 0) return;
  graphics.requires_extended = true;
}

static void dirty_current(void)
{
  graphics.video_layers[graphics.current_layer].empty = false;
}

static int offset_adjust(int offset, unsigned int x, unsigned int y)
{
  // Transform the given offset from screen space to layer space
  struct video_layer *layer = &graphics.video_layers[graphics.current_layer];

  if(layer->w != SCREEN_W || layer->x != 0 || layer->y != 0)
  {
    // NOTE: using the UI drawing functions on non-aligned layers is undefined
    // behavior. Use draw_char_to_layer instead.
    assert((int)x >= (layer->x / CHAR_W));
    assert((int)y >= (layer->y / CHAR_H));
    assert(x < (layer->x / CHAR_W) + layer->w);
    assert(y < (layer->y / CHAR_H) + layer->h);

    x -= layer->x / CHAR_W;
    y -= layer->y / CHAR_H;
    return y * layer->w + x;
  }
  return offset;
}

static int hexdigit(uint8_t hex)
{
  if(hex >= '0' && hex <= '9')
    return hex - '0';
  if(hex >= 'A' && hex <= 'F')
    return hex - 'A' + 10;
  if(hex >= 'a' && hex <= 'f')
    return hex - 'a' + 10;
  return -1;
}

/**
 * Helper function for handling `color_string` strings.
 * Return the total display length of the nul terminated color string `string`.
 * Will stop after `max_size` bytes are read from `string`, if applicable.
 */
size_t color_string_length(const char *string, size_t max_size)
{
  size_t pos = 0;
  size_t i;
  for(i = 0; i < max_size; i++)
  {
    if(!string[i])
      break;

    if(string[i] == '~' || string[i] == '@')
    {
      // All ~/@ values either escape the next char or display a color code.
      i++;
      if(i >= max_size && !string[i])
        break;

      if(isxdigit((uint8_t)string[i]))
      {
        // Color code--no display chars.
        continue;
      }
    }
    // Display character.
    pos++;
  }
  return pos;
}

/**
 * Helper function for handling `color_string` strings.
 * Find the byte index of the character at display position `offset` in a color
 * string. If the string displays as fewer than `offset` characters, returns
 * the offset of the terminator or nul instead (whichever is found first).
 * Will stop after `max_size` bytes are read from `string`, if applicable.
 * This is useful for clipping a color string for display.
 */
size_t color_string_index_of(const char *string, size_t max_size, size_t offset,
 int terminator)
{
  size_t pos = 0;
  size_t i;
  for(i = 0; i < max_size && pos < offset; i++)
  {
    if(!string[i] || string[i] == terminator)
      break;

    if(string[i] == '~' || string[i] == '@')
    {
      // All ~/@ values either escape the next char or display a color code.
      i++;
      if(i >= max_size && (!string[i] || string[i] == terminator))
        break;

      if(isxdigit((uint8_t)string[i]))
      {
        // Color code--no display chars.
        continue;
      }
    }
    // Display character.
    pos++;
  }
  return i;
}

/**
 * Helper function for handling `color_string` strings.
 * Returns the final color code of a color string, same as the return value
 * of `color_string_ext_special`, without actually displaying the string.
 */
uint8_t color_string_get_final_color(const char *string, size_t max_size,
 uint8_t initial_color)
{
  int fg_color = initial_color & 0x0f;
  int bg_color = initial_color >> 4;
  size_t i;
  for(i = 0; i < max_size; i++)
  {
    if(!string[i])
      break;

    if(string[i] == '~' || string[i] == '@')
    {
      // All ~/@ values either escape the next char or display a color code.
      i++;
      if(i >= max_size && !string[i])
        break;

      if(isxdigit((uint8_t)string[i]))
      {
        // Color code--no display chars.
        int num = hexdigit(string[i]);
        if(string[i - 1] == '~')
          fg_color = num;
        else
          bg_color = num;

        continue;
      }
    }
    // Display character.
  }
  return (bg_color << 4) | fg_color;
}

// Note: 2.93 erroneously had sane color escaping behavior here.
// ~ and @ should always escape the next char regardless of whether it's ~/@.
static int write_string_intl(const char *str, unsigned int x, unsigned int y,
 uint8_t color, boolean allow_tabs, boolean allow_newline, boolean end_newline,
 boolean allow_colors, boolean mask_midchars, int chr_offset, int color_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  struct char_element *dest_copy = graphics.text_video + scr_off;

  int bg_color = (color >> 4) + color_offset;
  int fg_color = (color & 0x0F) + color_offset;
  int code;

  dirty_ui();
  dirty_current();

  while(*str && dest < graphics.current_video_end)
  {
    int cur_char = *str++;

    if(cur_char == '\n')
    {
      if(end_newline)
        break;

      if(allow_newline)
      {
        y++;
        dest = graphics.current_video + offset_adjust((y * SCREEN_W) + x, x, y);
        dest_copy = graphics.text_video + (y * SCREEN_W) + x;
        continue;
      }
    }
    else

    if(cur_char == '\t')
    {
      if(allow_tabs)
      {
        // Note: the write_line functions used 10 here, but they were used
        // only for scrolls, which don't allow control codes. 1.x allowed
        // char 9 but displayed them as char 9.
        dest += 5;
        dest_copy += 5;
        continue;
      }
    }
    else

    if(allow_colors)
    {
      if(cur_char == '@')
      {
        if(!*str)
          break;

        code = hexdigit(*str);
        if(code >= 0)
        {
          bg_color = code + color_offset;
          str++;
          continue;
        }
        else
          cur_char = *str++;
      }
      else

      if(cur_char == '~')
      {
        if(!*str)
          break;

        code = hexdigit(*str);
        if(code >= 0)
        {
          fg_color = code + color_offset;
          str++;
          continue;
        }
        else
          cur_char = *str++;
      }
    }

    if(mask_midchars)
    {
      if(cur_char >= 32 && cur_char <= 126)
        chr_offset = PRO_CH;
      else
        chr_offset = 0;
    }

    /* Draw char */
    dest->char_value = cur_char + chr_offset;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    *(dest_copy++) = *dest;
    dest++;
  }

  return ((bg_color & 0xf) << 4) | (fg_color & 0xf);
}

void write_string_ext(const char *str, unsigned int x, unsigned int y,
 uint8_t color, int flags, unsigned int chr_offset, unsigned int color_offset)
{
  boolean allow_tabs      = (flags & WR_TAB) != 0;
  boolean allow_newlines  = (flags & WR_NEWLINE) != 0;
  boolean end_newline     = (flags & WR_LINE) != 0;
  boolean allow_colors    = (flags & WR_COLOR) != 0;
  boolean mask_midchars   = (flags & WR_MASK) != 0;

  write_string_intl(str, x, y, color, allow_tabs, allow_newlines,
   end_newline, allow_colors, mask_midchars, chr_offset, color_offset);
}

void color_string_ext_special(const char *str, unsigned int x, unsigned int y,
 uint8_t *color, boolean allow_newline, unsigned int chr_offset, unsigned int color_offset)
{
  *color = write_string_intl(str, x, y, *color,
   false, allow_newline, false, true, false, chr_offset, color_offset);
}

void color_string_ext(const char *str, unsigned int x, unsigned int y,
 uint8_t color, boolean allow_newline, unsigned int chr_offset, unsigned int color_offset)
{
  color_string_ext_special(str, x, y, &color, allow_newline, chr_offset, color_offset);
}

// Set rightalign to print the rightmost char at xy and proceed to the left
// minlen is the minimum length to print. Pad with 0.

void write_number(int number, uint8_t color, unsigned int x, unsigned int y,
 int minlen, boolean rightalign, int base)
{
  char temp[12];
  minlen = CLAMP(minlen, 0, 11);

  if(base == 10)
    snprintf(temp, 12, "%0*d", minlen, number);
  else
    snprintf(temp, 12, "%0*x", minlen, number);

  temp[11] = 0;

  if(rightalign)
  {
    unsigned int shift = strlen(temp) - 1;
    if(shift < x)
      x -= shift;
    else
      x = 0;
  }

  write_string_intl(temp, x, y, color,
   false, false, false, false, false, PRO_CH, 16);
}

static void color_line_ext(unsigned int length, unsigned int x, unsigned int y,
 uint8_t color, unsigned int color_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  uint8_t bg_color = (color >> 4) + color_offset;
  uint8_t fg_color = (color & 0x0F) + color_offset;
  unsigned int i;

  dirty_ui();
  dirty_current();

  for(i = 0; i < length && dest < graphics.current_video_end; i++)
  {
    dest->char_value = dest_copy->char_value;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    *(dest_copy++) = *dest;
    dest++;
  }
}

void fill_line_ext(unsigned int length, unsigned int x, unsigned int y,
 uint8_t chr, uint8_t color, unsigned int chr_offset, unsigned int color_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  uint8_t bg_color = (color >> 4) + color_offset;
  uint8_t fg_color = (color & 0x0F) + color_offset;
  unsigned int i;

  dirty_ui();
  dirty_current();

  for(i = 0; i < length && dest < graphics.current_video_end; i++)
  {
    dest->char_value = chr + chr_offset;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    *(dest_copy++) = *dest;
    dest++;
  }
}

#ifdef CONFIG_EDITOR
void draw_char_mixed_pal_ext(uint8_t chr, uint8_t bg_color,
 uint8_t fg_color, unsigned int x, unsigned int y, unsigned int chr_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  dest->char_value = chr + chr_offset;

  dest->bg_color = bg_color & 31;
  dest->fg_color = fg_color & 31;

  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}
#endif /* CONFIG_EDITOR */

void draw_char_ext(uint8_t chr, uint8_t color, unsigned int x, unsigned int y,
 unsigned int chr_offset, unsigned int color_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  dest->char_value = chr + chr_offset;
  dest->bg_color = (color >> 4) + color_offset;
  dest->fg_color = (color & 0x0F) + color_offset;
  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}

/**
 * draw_char_ext, except if the color being drawn has a background color of 0,
 * the background color from text_video will bleed through to the new character.
 * This effect is used by legacy sprites to simulate transparency.
 */
void draw_char_bleedthru_ext(uint8_t chr, uint8_t color,
 unsigned int x, unsigned int y, unsigned int chr_offset, unsigned int color_offset)
{
  int offset = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(offset, x, y);
  struct char_element *dest_copy = graphics.text_video + offset;

  if(!(color & 0xF0))
  {
    // Bleed-through background color from text_video.
    color |= (dest_copy->bg_color & 0x0F) << 4;
  }

  dest->char_value = chr + chr_offset;
  dest->bg_color = (color >> 4) + color_offset;
  dest->fg_color = (color & 0x0F) + color_offset;
  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}

void draw_char_to_layer(uint8_t chr, uint8_t color,
 unsigned int x, unsigned int y, unsigned int chr_offset, unsigned int color_offset)
{
  int w = graphics.video_layers[graphics.current_layer].w;
  struct char_element *dest = graphics.current_video + (y * w) + x;

  assert(dest < graphics.current_video_end);

  dest->char_value = chr + chr_offset;
  dest->bg_color = (color >> 4) + color_offset;
  dest->fg_color = (color & 0x0F) + color_offset;
  dirty_current();
}

void color_string(const char *string, unsigned int x, unsigned int y, uint8_t color)
{
  color_string_ext(string, x, y, color, false, PRO_CH, 16);
}

void write_string(const char *string, unsigned int x, unsigned int y,
 uint8_t color, int flags)
{
  write_string_ext(string, x, y, color, flags, PRO_CH, 16);
}

void color_line(unsigned int length, unsigned int x, unsigned int y, uint8_t color)
{
  color_line_ext(length, x, y, color, 16);
}

void fill_line(unsigned int length, unsigned int x, unsigned int y,
 uint8_t chr, uint8_t color)
{
  fill_line_ext(length, x, y, chr, color, PRO_CH, 16);
}

void draw_char(uint8_t chr, uint8_t color, unsigned int x, unsigned int y)
{
  draw_char_ext(chr, color, x, y, PRO_CH, 16);
}

void erase_char(unsigned int x, unsigned int y)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off, x, y);
  dest->char_value = INVISIBLE_CHAR;
}

void erase_area(unsigned int x, unsigned int y, unsigned int x2, unsigned int y2)
{
  unsigned int i, j;

  for(i = y; i <= y2; i++)
    for(j = x; j <= x2; j++)
      erase_char(j, i);
}

void clear_screen(void)
{
  // Hide the game screen by drawing blank chars over the UI.
  struct char_element *dest;
  struct char_element *dest_copy = graphics.text_video;
  uint32_t current_layer = graphics.current_layer;
  int i;

  select_layer(UI_LAYER);
  dirty_current();

  dest = graphics.current_video;
  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    dest->char_value = 0;
    dest->fg_color = 16; // Protected black
    dest->bg_color = 16; // Protected black
    *(dest_copy++) = *dest;
    dest++;
  }

  select_layer(current_layer);
  update_screen();
}

void set_screen(struct char_element *src)
{
  int offset = SCREEN_W * SCREEN_H;
  int size = offset * sizeof(struct char_element);

#ifndef CONFIG_NO_LAYER_RENDERING
  memcpy(graphics.video_layers[UI_LAYER].data, src, size);
  src += offset;

  memcpy(graphics.video_layers[GAME_UI_LAYER].data, src, size);
  src += offset;
#endif

  memcpy(graphics.text_video, src, size);
}

void get_screen(struct char_element *dest)
{
  int offset = SCREEN_W * SCREEN_H;
  int size = offset * sizeof(struct char_element);

#ifndef CONFIG_NO_LAYER_RENDERING
  memcpy(dest, graphics.video_layers[UI_LAYER].data, size);
  dest += offset;

  memcpy(dest, graphics.video_layers[GAME_UI_LAYER].data, size);
  dest += offset;
#endif

  memcpy(dest, graphics.text_video, size);
}

void cursor_underline(unsigned int x, unsigned int y)
{
  graphics.cursor_mode = CURSOR_MODE_UNDERLINE;
  graphics.cursor_x = x;
  graphics.cursor_y = y;
}

void cursor_solid(unsigned int x, unsigned int y)
{
  graphics.cursor_mode = CURSOR_MODE_SOLID;
  graphics.cursor_x = x;
  graphics.cursor_y = y;
}

void cursor_hint(unsigned int x, unsigned int y)
{
  graphics.cursor_mode = graphics.cursor_hint_mode;
  graphics.cursor_x = x;
  graphics.cursor_y = y;
}

void cursor_off(void)
{
  graphics.cursor_mode = CURSOR_MODE_INVISIBLE;
}

void m_hide(void)
{
  graphics.mouse_status = false;
}

void m_show(void)
{
  graphics.mouse_status = true;
}

void mouse_size(unsigned int width, unsigned int height)
{
  graphics.mouse_width = width;
  graphics.mouse_height = height;
}

#if defined(CONFIG_ENABLE_SCREENSHOTS) || defined(CONFIG_EDITOR)

static void screenshot_init_palette(struct graphics_data *graphics,
 struct rgb_color palette[FULL_PAL_SIZE], uint32_t backup_palette[FULL_PAL_SIZE])
{
  int palette_size;
  int i;

  if(backup_palette)
  {
    memcpy(backup_palette, graphics->flat_intensity_palette,
     FULL_PAL_SIZE * sizeof(uint32_t));
  }

  palette_size = make_palette(palette);
  for(i = 0; i < palette_size; i++)
  {
    // PNG byte order: R, G, B, A
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    graphics->flat_intensity_palette[i] = 0xff |
     (palette[i].b << 8) | (palette[i].g << 16) | (palette[i].r << 24);
#else
    graphics->flat_intensity_palette[i] = 0xff000000u |
     (palette[i].b << 16) | (palette[i].g << 8) | (palette[i].r << 0);
#endif /* PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN */
  }
}

#endif /* CONFIG_ENABLE_SCREENSHOTS || CONFIG_EDITOR */

#ifdef CONFIG_ENABLE_SCREENSHOTS
#if 1 // CONFIG_PNG

#define DUMP_FMT_EXT "png"

struct dump_screen_priv
{
  const uint8_t *pix8;
  const uint32_t *pix32;
};

/*
static const uint8_t *dump_screen_callback(size_t num_pixels, void *priv)
{
  struct dump_screen_priv *data = (struct dump_screen_priv *)priv;
  const uint8_t *ret = data->pix8;
  data->pix8 += num_pixels;
  return ret;
}

static void dump_screen_real(const uint8_t *pix,
 const struct rgb_color *pal, int count, const char *name)
{
  struct dump_screen_priv priv = { pix, NULL };
  png_write_image_8bpp(name, 640, 350, pal, count, &priv, dump_screen_callback);
}
*/

static const uint32_t *dump_screen_callback_32bpp(size_t num_pixels, void *priv)
{
  struct dump_screen_priv *data = (struct dump_screen_priv *)priv;
  const uint32_t *ret = data->pix32;
  data->pix32 += num_pixels;
  return ret;
}

static void dump_screen_real_32bpp(const uint32_t *pix, const char *name)
{
  struct dump_screen_priv priv = { NULL, pix };
  png_write_image_32bpp(name, 640, 350, &priv, dump_screen_callback_32bpp);
}

#else

#define DUMP_FMT_EXT "bmp"

/*
static void dump_screen_real(const uint8_t *pix, const struct rgb_color *pal,
 int count, const char *name)
{
  vfile *file;
  int i;

  file = vfopen_unsafe(name, "wb");
  if(!file)
    return;

  // BMP header
  vfputw(0x4D42, file); // BM
  vfputd(14 + 40 + count * 4 + 640 * 350, file); // BMP + DIB + palette + image
  vfputd(0, file); // Reserved
  vfputd(14, file); // DIB header offset

  // DIB header
  vfputd(40, file); // DIB header size (Windows 3/BITMAPINFOHEADER)
  vfputd(640, file); // Width in pixels
  vfputd(350, file); // Height in pixels
  vfputw(1, file); // Number of color planes
  vfputw(8, file); // Bits per pixel
  vfputd(0, file); // Compression method (none)
  vfputd(640 * 350, file); // Image data size
  vfputd(3780, file); // Horizontal dots per meter
  vfputd(3780, file); // Vertical dots per meter
  vfputd(count, file); // Number of colors in palette
  vfputd(0, file); // Number of important colors

  // Color palette
  for(i = 0; i < count; i++)
  {
    vfputc(pal[i].b, file);
    vfputc(pal[i].g, file);
    vfputc(pal[i].r, file);
    vfputc(0, file);
  }

  // Image data
  for(i = 349; i >= 0; i--)
  {
    vfwrite(pix + i * 640, sizeof(uint8_t), 640, file);
  }

  vfclose(file);
}
*/

static void dump_screen_real_32bpp(const uint32_t *pix, const char *name)
{
  vfile *file;
  int i, x;
  uint8_t rowbuffer[SCREEN_PIX_W * 3]; // 24bpp
  uint8_t *rowbuffer_ptr;
  const uint8_t *pix_ptr;

  file = vfopen_unsafe(name, "wb");
  if(!file)
    return;

  // BMP header
  vfputw(0x4D42, file); // BM
  // BMP + DIB + image
  vfputd(14 + 40 + SCREEN_PIX_W * SCREEN_PIX_H * 3, file);
  vfputd(0, file); // Reserved
  vfputd(14, file); // DIB header offset

  // DIB header
  vfputd(40, file); // DIB header size (Windows 3/BITMAPINFOHEADER)
  vfputd(SCREEN_PIX_W, file); // Width in pixels
  vfputd(SCREEN_PIX_H, file); // Height in pixels
  vfputw(1, file); // Number of color planes
  vfputw(24, file); // Bits per pixel
  vfputd(0, file); // Compression method (none)
  vfputd(SCREEN_PIX_W * SCREEN_PIX_H * 3, file); // Image data size
  vfputd(3780, file); // Horizontal dots per meter
  vfputd(3780, file); // Vertical dots per meter
  vfputd(0, file); // Number of colors in palette
  vfputd(0, file); // Number of important colors

  // Image data
  for(i = SCREEN_PIX_H - 1; i >= 0; i--)
  {
    pix_ptr = (const uint8_t *)(pix + i * SCREEN_PIX_W);
    rowbuffer_ptr = rowbuffer;
    for(x = 0; x < SCREEN_PIX_W; x++)
    {
      rowbuffer_ptr[0] = pix_ptr[2];
      rowbuffer_ptr[1] = pix_ptr[1];
      rowbuffer_ptr[2] = pix_ptr[0];
      rowbuffer_ptr += 3;
      pix_ptr += 4;
    }
    vfwrite(rowbuffer, SCREEN_PIX_W * 3, 1, file);
  }

  vfclose(file);
}

#endif // CONFIG_PNG

#define MAX_NAME_SIZE 20

static void screenshot_cleanup_palette(const uint32_t backup_palette[FULL_PAL_SIZE])
{
  memcpy(graphics.flat_intensity_palette, backup_palette,
   FULL_PAL_SIZE * sizeof(uint32_t));
}

void dump_screen(void)
{
  struct rgb_color palette[FULL_PAL_SIZE];
  uint32_t backup_palette[FULL_PAL_SIZE];
  char name[MAX_NAME_SIZE];
  struct stat file_info;
  uint32_t *ss;
  int i;
  uint32_t layer;

  for(i = 0; i < 99999; i++)
  {
    snprintf(name, MAX_NAME_SIZE - 1, "screen%d.%s", i, DUMP_FMT_EXT);
    name[MAX_NAME_SIZE - 1] = '\0';
    if(vstat(name, &file_info))
      break;
  }

  ss = (uint32_t *)cmalloc(sizeof(uint32_t) * 640 * 350);
  if(!ss)
    return;

  screenshot_init_palette(&graphics, palette, backup_palette);

  for(layer = 0; layer < graphics.layer_count; layer++)
    graphics.sorted_video_layers[layer] = &graphics.video_layers[layer];

  qsort(graphics.sorted_video_layers, graphics.layer_count,
   sizeof(struct video_layer *), compare_layers);

  for(layer = 0; layer < graphics.layer_count; layer++)
  {
    render_layer(ss, SCREEN_PIX_W, SCREEN_PIX_H,
     SCREEN_PIX_W * sizeof(uint32_t), 32, &graphics,
     graphics.sorted_video_layers[layer]);
  }

  screenshot_cleanup_palette(backup_palette);

  //dump_screen_real(ss, palette, make_palette(palette), name);
  dump_screen_real_32bpp(ss, name);
  free(ss);
}
#endif /* CONFIG_ENABLE_SCREENSHOTS */

#ifdef CONFIG_EDITOR
struct dump_layer_priv
{
  boolean (*status_callback)(void *, size_t, size_t);
  void *status_priv;
  struct graphics_data *graphics_copy;
  uint32_t *flat_array;
  size_t pitch;
  size_t width_px;
  size_t height_px;
  size_t width_ch;
  size_t height_ch;
  size_t pos_in_layer;
  size_t pos_in_array;
  struct video_layer layer;
};

/* Render the layer one row at a time as it may be LARGE. */
static const uint32_t *dump_layer_callback(size_t num_pixels, void *priv)
{
  struct dump_layer_priv *data = (struct dump_layer_priv *)priv;
  uint32_t *pos;

  if(data->pos_in_array >= CHAR_H)
  {
    if(data->status_callback)
    {
      if(!data->status_callback(data->status_priv,
       data->pos_in_layer, data->height_ch))
        return NULL; // cancel requested
    }

    if(data->pos_in_layer >= data->height_ch)
      return NULL;

    render_layer(data->flat_array, data->width_px, CHAR_H,
     data->pitch, 32, data->graphics_copy, &data->layer);
    data->layer.data += data->width_ch;
    data->pos_in_layer++;
    data->pos_in_array = 0;
  }

  pos = data->flat_array + (data->pitch >> 2) * data->pos_in_array;
  data->pos_in_array++;
  return pos;
}

/* Render an arbitrary layer to an image.
 * TODO: The caller is responsible for constructing the layer char_element
 * array since the graphics API functions are very much intended for operating
 * on real graphical layers rather than arbitrary layers. */
boolean dump_layer_to_image(const char *filename,
 size_t width_ch, size_t height_ch, const struct char_element *layer,
 boolean (*status_callback)(void *priv, size_t p, size_t m), void *priv)
{
  struct graphics_data *graphics_copy;
  struct rgb_color palette[FULL_PAL_SIZE];
  struct dump_layer_priv data;
  uint32_t *flat_array;
  boolean ret;

  if(width_ch >= 32768 || height_ch >= 32768)
    return false;

  /* This may be run as an async task, which results in palette conflicts.
   * The easiest thing to do is duplicate graphics entirely. */
  graphics_copy = (struct graphics_data *)cmalloc(sizeof(struct graphics_data));
  if(!graphics_copy)
    return false;

  flat_array = (uint32_t *)cmalloc(width_ch * CHAR_W * CHAR_H * sizeof(uint32_t));
  if(!flat_array)
  {
    free(graphics_copy);
    return false;
  }

  memcpy(graphics_copy, &graphics, sizeof(struct graphics_data));
  memset(&data, 0, sizeof(data));
  data.status_callback = status_callback;
  data.status_priv = priv;
  data.graphics_copy = graphics_copy;
  data.flat_array = flat_array;
  data.pitch = width_ch * CHAR_W * sizeof(uint32_t);
  data.width_px = width_ch * CHAR_W;
  data.height_px = height_ch * CHAR_H;
  data.width_ch = width_ch;
  data.height_ch = height_ch;
  data.pos_in_array = CHAR_H; // redraw
  data.pos_in_layer = 0;

  data.layer.data = (struct char_element *)layer;
  data.layer.x = 0;
  data.layer.y = 0;
  data.layer.w = width_ch;
  data.layer.h = 1;
  data.layer.transparent_col = -1;
  data.layer.offset = 0;
  data.layer.mode = get_screen_mode();
  data.layer.empty = false;

  screenshot_init_palette(graphics_copy, palette, NULL);

  ret = png_write_image_32bpp(filename,
   width_ch * CHAR_W, height_ch * CHAR_H, &data, dump_layer_callback);

  free(graphics_copy);
  free(flat_array);
  return ret;
}
#endif /* CONFIG_EDITOR */

/**
 * Generate a bitmask of visible pixels for a character/palette pair using the
 * current screen mode and a given transparent color index. The provided buffer
 * must be CHAR_SIZE bytes in length. Returns `false` if there are no visible
 * pixels, otherwise `true`.
 */
boolean get_char_visible_bitmask(uint16_t char_idx, uint8_t palette,
 int transparent_color, uint8_t * RESTRICT buffer)
{
  const uint8_t *chrdata = graphics.charset + char_idx * CHAR_SIZE;
  const uint8_t HI = 0xAA;
  const uint8_t LO = 0x55;
  int is_transparent[4];
  int ret = 0x00;
  int y;

  if(graphics.screen_mode == 0)
  {
    int bg = (palette & 0xF0) >> 4;
    int fg = (palette & 0x0F);
    is_transparent[0] = (bg == transparent_color);
    is_transparent[1] = (fg == transparent_color);

    for(y = 0; y < CHAR_SIZE; y++)
    {
      uint8_t base = chrdata[y];
      uint8_t mask = 0xFF;

      if(is_transparent[0])
        mask &= base;

      if(is_transparent[1])
        mask &= ~base;

      buffer[y] = mask;
      ret |= mask;
    }
  }
  else
  {
    is_transparent[0] = graphics.smzx_indices[palette * 4 + 0] == transparent_color;
    is_transparent[1] = graphics.smzx_indices[palette * 4 + 1] == transparent_color;
    is_transparent[2] = graphics.smzx_indices[palette * 4 + 2] == transparent_color;
    is_transparent[3] = graphics.smzx_indices[palette * 4 + 3] == transparent_color;

    for(y = 0; y < CHAR_SIZE; y++)
    {
      uint8_t base = chrdata[y];
      uint8_t mask = 0xFF;

      if(is_transparent[0])
      {
        uint8_t colmask = (~base & HI) & ((~base & LO) << 1);
        mask &= ~(colmask | (colmask >> 1));
      }

      if(is_transparent[1])
      {
        uint8_t colmask = (~base & HI) & ((base & LO) << 1);
        mask &= ~(colmask | (colmask >> 1));
      }

      if(is_transparent[2])
      {
        uint8_t colmask = (base & HI) & ((~base & LO) << 1);
        mask &= ~(colmask | (colmask >> 1));
      }

      if(is_transparent[3])
      {
        uint8_t colmask = (base & HI) & ((base & LO) << 1);
        mask &= ~(colmask | (colmask >> 1));
      }

      buffer[y] = mask;
      ret |= mask;
    }
  }
  return (ret != 0x00);
}

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y)
{
#if 0
  if(graphics.renderer->get_screen_coords)
  {
    graphics.renderer->get_screen_coords(&graphics, &(graphics.window),
     screen_x, screen_y, x, y, min_x, min_y, max_x, max_y);
  }
  else
#endif
  {
    get_screen_coords_viewport(&graphics, &(graphics.window),
     screen_x, screen_y, x, y, min_x, min_y, max_x, max_y);
  }
}

void set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
#if 0
  if(graphics.renderer->set_screen_coords)
  {
    graphics.renderer->set_screen_coords(&graphics, &(graphics.window),
     x, y, screen_x, screen_y);
  }
  else
#endif
  {
    set_screen_coords_viewport(&graphics, &(graphics.window),
     x, y, screen_x, screen_y);
  }
}

void focus_screen(int x, int y)
{
  int pixel_x = x * graphics.resolution_width  / SCREEN_W +
                    graphics.resolution_width  / SCREEN_W / 2;
  int pixel_y = y * graphics.resolution_height / SCREEN_H +
                    graphics.resolution_height / SCREEN_H / 2;
  focus_pixel(pixel_x, pixel_y);
}

void focus_pixel(int x, int y)
{
  if(graphics.renderer->focus_pixel)
    graphics.renderer->focus_pixel(&graphics, x, y);
}

boolean switch_shader(const char *name)
{
  if(graphics.renderer->switch_shader)
    return graphics.renderer->switch_shader(&graphics, name);

  return false;
}

boolean layer_renderer_check(boolean show_error)
{
#ifdef CONFIG_NO_LAYER_RENDERING
  if(1)
#else
  if(!graphics.renderer->render_layer)
#endif
  {
    if(show_error)
    {
      error_message(E_NO_LAYER_RENDERER, 0, NULL);
      set_error_suppression(E_NO_LAYER_RENDERER, 1);
    }
    return false;
  }
  return true;
}

#if (NUM_CHARSETS < 16)

/**
 * Determine if an extended charsets write would be valid on platforms that
 * don't have a full-sized charset buffer. While a renderer capable of layer
 * renderering is required to display from extended charsets, the extended
 * charset buffer space is available to all 2.90+ games and it may be used to
 * copy char data into the main charset. Thus, attempts to access extended
 * charsets do not necessarily have a direct relation to the renderer.
 *
 * This should only ever be the Nintendo DS. Do not add any other platforms
 * that rely on this horrible hack to save (only 50k) RAM unless you also plan
 * to add support for the extended charsets being allocated to the heap so
 * games using them as a buffer don't break.
 */
static boolean extended_charsets_check(boolean show_error, int pos, int count)
{
  // Simulate the bounding that'd be used with the normal charset count...
  pos %= (15 * CHARSET_SIZE);

  if((pos >= PROTECTED_CHARSET_POSITION) ||
   (pos + count > PROTECTED_CHARSET_POSITION))
  {
    if(show_error)
    {
      error_message(E_NO_EXTENDED_CHARSETS, 0, NULL);
      set_error_suppression(E_NO_EXTENDED_CHARSETS, true);
    }
    return false;
  }
  return true;
}

#endif
