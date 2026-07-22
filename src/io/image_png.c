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

#ifdef IMAGE_FILE_LIBPNG

#include <setjmp.h>
#include <png.h>

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

enum png_error png_read(void *handle, const png_read_function readfn,
 void *priv, const png_read_alloc_function allocfn, png_bool skip_signature)
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
  png_destroy_read_struct(&png, &info, NULL);

  free(row_ptrs);
  return PNG_OK;

error:
  png_destroy_read_struct(&png, info ? &info : NULL, NULL);
  free(row_ptrs);
  return ret;
}

/* FIXME: writer */

#else /* !IMAGE_FILE_LIBPNG */

enum png_error png_read(void *handle, const png_read_function readfn,
 void *priv, const png_read_alloc_function allocfn, png_bool skip_signature)
{
  return PNG_ERROR_INIT;
}

/* FIXME: fallback writer */

#endif /* !IMAGE_FILE_LIBPNG */
