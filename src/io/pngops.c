/* MegaZeux
 *
 * Copyright (C) 2008 Alistair Strachan <alistair@devzero.co.uk>
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

#include "pngops.h"
#include "vio.h"
#include "../util.h"
#include "../platform/log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NEED_PNG_WRITE_SCREEN

#ifdef CONFIG_PNG

#include <png.h>

/* Prior to libpng 1.5.16, png_write_row takes a non-const pointer despite
 * it never modifying the row data. In some very old libpng versions, the
 * type png_const_bytep does not even exist (encountered in 1.2.49). */
#if defined(PNG_LIBPNG_VER) && PNG_LIBPNG_VER >= 10516
typedef png_const_bytep png_maybeconst_bytep;
#else
typedef png_bytep png_maybeconst_bytep;
#endif

static void png_write_vfile(png_struct *png_ptr, png_byte *data, png_size_t length)
{
  vfile *vf = (vfile *)png_get_io_ptr(png_ptr);
  if(vfwrite(data, 1, length, vf) < length)
    png_error(png_ptr, "write error");
}

static void png_flush_vfile(png_struct *png_ptr) {}

/* Trivial PNG dumper; this routine is a modification (simplification) of
 * code pinched from http://www2.autistici.org/encelo/prog_sdldemos.php.
 *
 * Palette support was added, the original support was broken.
 *
 * Copyright (C) 2006 Angelo "Encelo" Theodorou
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 */
/*
int png_write_image_8bpp(const char *name, size_t w, size_t h,
 const struct rgb_color *pal, unsigned count, void *priv,
 const uint8_t *(*row_pixels_callback)(size_t num_pixels, void *priv))
{
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_colorp volatile pal_ptr = NULL;
  int volatile ret = false;
  int type;
  size_t i;
  vfile *vf;

  vf = vfopen_unsafe(name, "wb");
  if(!vf)
    goto exit_out;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  png_set_write_fn(png_ptr, vf, png_write_vfile, png_flush_vfile);

  // we know we have an 8-bit surface; save a palettized PNG
  type = PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE;
  png_set_IHDR(png_ptr, info_ptr, w, h, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  pal_ptr = (png_colorp)cmalloc(count * sizeof(png_color));
  if(!pal_ptr)
    goto exit_free_close;

  for(i = 0; i < count; i++)
  {
    pal_ptr[i].red = pal[i].r;
    pal_ptr[i].green = pal[i].g;
    pal_ptr[i].blue = pal[i].b;
  }
  png_set_PLTE(png_ptr, info_ptr, pal_ptr, count);

  // do the write of the header
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  // and then the surface
  for(i = 0; i < h; i++)
  {
    const uint8_t *row = row_pixels_callback(w, priv);
    if(!row)
      goto exit_free_close;

    png_write_row(png_ptr, (png_maybeconst_bytep)row);
  }
  png_write_end(png_ptr, info_ptr);

  // all done
  ret = true;

exit_free_close:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  free(pal_ptr);
exit_close:
  vfclose(vf);
exit_out:
  return ret;
}
*/

int png_write_image_32bpp(const char *name, size_t w, size_t h, void *priv,
 const uint32_t *(*row_pixels_callback)(size_t num_pixels, void *priv))
{
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  int volatile ret = false;
  int type;
  size_t i;
  vfile *vf;

  vf = vfopen_unsafe(name, "wb");
  if(!vf)
    goto exit_out;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  png_set_write_fn(png_ptr, vf, png_write_vfile, png_flush_vfile);

  // 24-bit png
  type = PNG_COLOR_TYPE_RGB;
  png_set_IHDR(png_ptr, info_ptr, w, h, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  // do the write of the header
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  // our surface is 32bpp ABGR, so set up filler and order
  png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
  //png_set_bgr(png_ptr);

  // and then the surface
  for(i = 0; i < h; i++)
  {
    const uint32_t *row = row_pixels_callback(w, priv);
    if(!row)
      goto exit_free_close;

    png_write_row(png_ptr, (png_maybeconst_bytep)row);
  }
  png_write_end(png_ptr, info_ptr);

  // all done
  ret = true;

exit_free_close:
  png_destroy_write_struct(&png_ptr, &info_ptr);
exit_close:
  vfclose(vf);
exit_out:
  return ret;
}

#else /* !CONFIG_PNG */

/* Much more limited fallback PNG writer for builds without libpng.
 * This has fewer features than libpng but, since MegaZeux already relies
 * on zlib, it doesn't increase binary size by much more than a BMP writer.
 * Unlike a BMP writer, it allows large board/vlayer images to be compressed.
 */
#include <zlib.h>

#define MAGIC_BE(buf, str) do { \
  (buf)[0] = (str)[0]; \
  (buf)[1] = (str)[1]; \
  (buf)[2] = (str)[2]; \
  (buf)[3] = (str)[3]; \
} while(0)

#define PUT32_BE(buf, val) do { \
  (buf)[0] = (val) >> 24; \
  (buf)[1] = (val) >> 16; \
  (buf)[2] = (val) >> 8; \
  (buf)[3] = (val); \
} while(0)

#define DEFLATE_OUT(p) do { \
  z.next_out = tmp2; \
  z.avail_out = sizeof(tmp2); \
  ret = deflate(&z, p); \
  if(ret < 0) \
    goto err2; \
  sz = sizeof(tmp2) - z.avail_out; \
  len += sz; \
  crc = crc32(crc, tmp2, sz); \
  if(vfwrite(tmp2, 1, sz, vf) < sz) \
    goto err2; \
} while(0)

int png_write_image_32bpp(const char *name, size_t w, size_t h, void *priv,
 const uint32_t *(*row_pixels_callback)(size_t num_pixels, void *priv))
{
  uint8_t tmp[1029];
  uint8_t tmp2[1024];
  uint32_t crc;
  uint32_t len = 0;
  size_t x, y, i, sz;
  int ret;
  z_stream z;

  vfile *vf = vfopen_unsafe(name, "w+b");
  if(!vf)
    return false;

  memset(&z, 0, sizeof(z));
  if(deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_FILTERED) != Z_OK)
    if(deflateInit2(&z, Z_BEST_SPEED, Z_DEFLATED, 9, 1, Z_RLE) != Z_OK)
      goto err;

  vfputs("\x89PNG\r\n\x1a\n", vf);
  PUT32_BE(tmp, 13);
  MAGIC_BE(tmp + 4, "IHDR");
  PUT32_BE(tmp + 8, w);
  PUT32_BE(tmp + 12, h);
  tmp[16] = 8; // 8 bits per component
  tmp[17] = 6; // RGB
  tmp[18] = 0; // deflate
  tmp[19] = 0; // filter 0
  tmp[20] = 0; // no interlace
  crc = crc32(0, tmp + 4, 17);
  PUT32_BE(tmp + 21, crc);
  vfwrite(tmp, 25, 1, vf);

  vfputd(0, vf);
  MAGIC_BE(tmp, "IDAT");
  crc = crc32(0, tmp, 4);
  vfwrite(tmp, 4, 1, vf);
  for(y = 0; y < h; y++)
  {
    const uint8_t *row = (const uint8_t *)row_pixels_callback(w, priv);
    uint8_t *pos = tmp + 5;
    if(!row)
      goto err2;

    tmp[0] = 1; // left filter
    for(i = 0; i < 4; i++)
      tmp[i + 1] = row[i];

    for(x = 1; x < w;)
    {
      size_t num = MIN(w - x, 256);
      x += num;
      // Prefilter: since it's always left filter RGBA, delta with the
      // same component of the previous pixel (the value 4 bytes ago).
      while(num)
      {
        for(i = 0; i < 4; i++)
          pos[i] = row[i + 4] - row[i];
        pos += 4;
        row += 4;
        num--;
      }

      z.next_in = tmp;
      z.avail_in = pos - tmp;
      do
      {
        DEFLATE_OUT(Z_NO_FLUSH);
      } while(z.avail_out == 0);
      pos = tmp;
    }
  }
  do
  {
    DEFLATE_OUT(Z_FINISH);
  } while(ret != Z_STREAM_END);
  deflateEnd(&z);

  PUT32_BE(tmp, crc);
  vfwrite(tmp, 4, 1, vf);

  vfputd(0, vf);
  MAGIC_BE(tmp, "IEND");
  crc = crc32(0, tmp, 4);
  PUT32_BE(tmp + 4, crc);
  vfwrite(tmp, 8, 1, vf);

  vfseek(vf, 33, SEEK_SET);
  PUT32_BE(tmp, len);
  vfwrite(tmp, 4, 1, vf);
  vfclose(vf);
  return true;

err2:
  deflateEnd(&z);
err:
  vfclose(vf);
  return false;
}
#endif /* !CONFIG_PNG */

#endif /* NEED_PNG_WRITE_SCREEN */

#ifdef NEED_PNG_READ_FILE
#include <png.h>

static boolean png_check_stream(vfile *vf)
{
  png_byte header[8];

  if(vfread(header, 1, 8, vf) < 8)
    return false;

  if(png_sig_cmp(header, 0, 8))
    return false;

  return true;
}

static void png_read_vfile(png_structp png_ptr, png_bytep data, size_t length)
{
  vfile *vf = (vfile *)png_get_io_ptr(png_ptr);
  if(vfread(data, 1, length, vf) < length)
    png_error(png_ptr, "read error");
}

/**
 * If `checked` is true, the first 8 bytes of the stream have already been
 * read and verified to be the PNG signature.
 */
void *png_read_stream(vfile *vf, png_uint_32 *_w, png_uint_32 *_h, boolean checked,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator)
{
  png_uint_32 i, w, h, stride;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep * volatile row_ptrs = NULL;
  void * volatile s = NULL;
  void *pixels;
  int type;
  int bpp;

  if(!checked && !png_check_stream(vf))
    goto exit_out;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_out;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  png_set_read_fn(png_ptr, vf, png_read_vfile);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bpp, &type, NULL, NULL, NULL);

  if(!constraint(w, h))
  {
    warn("Requested image failed dimension checks.\n");
    goto exit_free_close;
  }

#if PNG_LIBPNG_VER < 10504
#define png_set_scale_16(p) png_set_strip_16(p)
#endif
  /* This SHOULD convert everything to RGBA32.
   * See the far too complicated table in libpng-manual.txt for more info. */
  if(bpp == 16)
    png_set_scale_16(png_ptr);
  if(type & PNG_COLOR_MASK_PALETTE)
    png_set_palette_to_rgb(png_ptr);
  if(!(type & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png_ptr);

  if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
  {
    png_set_tRNS_to_alpha(png_ptr);
  }
#if PNG_LIBPNG_VER >= 10207
  else

  if(!(type & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
#endif

  row_ptrs = (png_bytep *)cmalloc(sizeof(png_bytep) * h);
  if(!row_ptrs)
    goto exit_free_close;

  s = allocator(w, h, &stride, &pixels);
  if(!s)
    goto exit_free_close;

  for(i = 0; i < h; i++)
    row_ptrs[i] = (png_bytep)(unsigned char *)pixels + i * stride;

  png_read_image(png_ptr, row_ptrs);

  if(_w)
    *_w = w;
  if(_h)
    *_h = h;

exit_free_close:
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  free(row_ptrs);
exit_out:
  return s;
}

void *png_read_file(const char *name, png_uint_32 *_w, png_uint_32 *_h,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator)
{
  void *s;

  vfile *vf = vfopen_unsafe(name, "rb");
  if(!vf)
    return NULL;

  s = png_read_stream(vf, _w, _h, false, constraint, allocator);
  vfclose(vf);

  if(!s)
    warn("Failed to load '%s'\n", name);

  return s;
}

#endif // NEED_PNG_READ_FILE
