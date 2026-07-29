/* MegaZeux
 *
 * Copyright (C) 2021-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "image_png.h"

/* Fallbacks may be enabled even if libpng is enabled for e.g. unit tests. */
enum png_error png_write_fallback(
 uint32_t width, uint32_t height, enum png_write_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn);


#ifdef IMAGE_FILE_LIBPNG

#include <png.h>
#include <setjmp.h>

#if PNG_LIBPNG_VER < 10504
#define png_set_scale_16(p) png_set_strip_16(p)
#endif

struct png_reader
{
  void *handle;
  png_read_function readfn;
};

static void png_read_fn(png_struct *png, png_byte *dest, size_t count)
{
  struct png_reader *r = (struct png_reader *)png_get_io_ptr(png);
  if(r->readfn(dest, count, r->handle) < count)
    png_error(png, "eof");
}

enum png_error png_read(void *handle, png_read_function readfn,
 void *priv, png_read_alloc_function allocfn, png_bool skip_signature)
{
  struct png_rgba *pixels;
  png_struct *png = NULL;
  png_info *info = NULL;
  png_byte ** volatile row_ptrs = NULL;
  png_uint_32 w;
  png_uint_32 h;
  png_uint_32 i;
  png_uint_32 j;
  int bit_depth;
  int color_type;
  enum png_error ret;

  struct png_reader r = { handle, readfn };

  if(!skip_signature)
  {
    uint8_t magic[8];
    if(readfn(magic, 8, handle) < 8 || png_sig_cmp(magic, 0, 8) != 0)
      return PNG_ERROR_NOT_A_PNG;
  }

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png)
    return PNG_ERROR_INIT;

  info = png_create_info_struct(png);
  if(!info)
  {
    ret = PNG_ERROR_INIT;
    goto error;
  }

  if(setjmp(png_jmpbuf(png)))
  {
    ret = PNG_ERROR_INVALID;
    goto error;
  }

  png_set_read_fn(png, &r, png_read_fn);
  png_set_sig_bytes(png, 8);

  png_read_info(png, info);
  png_get_IHDR(png, info, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

  pixels = allocfn(w, h, priv);
  if(!pixels)
  {
    ret = PNG_ERROR_MEM;
    goto error;
  }

  row_ptrs = (png_byte **)malloc(h * sizeof(png_byte *));
  if(!row_ptrs)
  {
    ret = PNG_ERROR_MEM;
    goto error;
  }

  for(i = 0, j = 0; i < h; i++, j += w)
    row_ptrs[i] = (png_byte *)(pixels + j);

  /* This SHOULD convert everything to RGBA32.
   * See the far too complicated table in libpng-manual.txt for more info. */
  if(bit_depth == 16)
    png_set_scale_16(png);
  if(color_type & PNG_COLOR_MASK_PALETTE)
    png_set_palette_to_rgb(png);
  if(!(color_type & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png);
#if PNG_LIBPNG_VER >= 10207
  if(!(color_type & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
#endif
  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  png_read_image(png, row_ptrs);
  png_read_end(png, NULL);
  ret = PNG_OK;

error:
  png_destroy_read_struct(&png, info ? &info : NULL, NULL);
  free(row_ptrs);
  return ret;
}

struct png_writer
{
  void *handle;
  png_write_function writefn;
};

static void png_write_fn(png_struct *png, png_byte *src, size_t count)
{
  struct png_writer *wr = (struct png_writer *)png_get_io_ptr(png);
  if(wr->writefn(src, count, wr->handle) < count)
    png_error(png, "eof");
}

enum png_error png_write(
 uint32_t width, uint32_t height, enum png_write_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
  png_struct *png = NULL;
  png_info *info = NULL;
  png_bool set_filler = 0;
  uint32_t i;
  enum png_error ret;

  struct png_writer wr = { handle, writefn };

  png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png)
    return PNG_ERROR_INIT;

  info = png_create_info_struct(png);
  if(!info)
  {
    ret = PNG_ERROR_INIT;
    goto error;
  }

  if(setjmp(png_jmpbuf(png)))
  {
    ret = PNG_ERROR_INVALID;
    goto error;
  }

  png_set_write_fn(png, &wr, png_write_fn, NULL);

  switch(fmt)
  {
    case PNG_WRITE_RGB24:
      png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      set_filler = 1;
      break;

    case PNG_WRITE_RGBA32:
      png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      break;

    default:
      ret = PNG_ERROR_INIT;
      goto error;
  }

  png_write_info(png, info);

  /* Needs to be called after writing the header for some reason... */
  if(set_filler)
    png_set_filler(png, 0, PNG_FILLER_AFTER);

  for(i = 0; i < height; i++)
  {
    const struct png_rgba *row = writerowfn(width, priv);
    if(!row)
    {
      ret = PNG_ERROR_INVALID;
      goto error;
    }
    png_write_row(png, (const png_byte *)row);
  }

  png_write_end(png, info);
  ret = PNG_OK;

error:
  if(info)
    png_destroy_info_struct(png, &info);

  png_destroy_write_struct(&png, NULL);
  return ret;
}

#else /* !IMAGE_FILE_LIBPNG */

enum png_error png_read(void *handle, png_read_function readfn,
 void *priv, png_read_alloc_function allocfn, png_bool skip_signature)
{
  return PNG_ERROR_INIT;
}

enum png_error png_write(
 uint32_t width, uint32_t height, enum png_write_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
  return png_write_fallback(width, height, fmt, handle, writefn, priv, writerowfn);
}

#endif /* !IMAGE_FILE_LIBPNG */


#if defined(MZX_UNIT_TESTS) || !defined(IMAGE_FILE_LIBPNG)

enum png_error png_write_fallback(
 uint32_t width, uint32_t height, enum png_write_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
  // FIXME:
  return PNG_ERROR_INIT;
}

#endif
