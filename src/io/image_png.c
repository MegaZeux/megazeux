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

#include "image_common.h"
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

/* Much more limited fallback PNG writer for builds without libpng.
 * This has fewer features than libpng but, since MegaZeux already relies
 * on zlib, it doesn't increase binary size by much more than a BMP writer.
 * Unlike a BMP writer, it allows large board/vlayer images to be compressed.
 *
 * Note that this function can not seek back into the stream, so the output
 * data must be split into potentially multiple IDAT chunks.
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

#define DEFLATE_RESET() do { \
  z.next_out = tmp2 + 8; \
  z.avail_out = sizeof(tmp2) - 12; \
} while(0)

#define DEFLATE_FLUSH() do { \
  size_t sz = sizeof(tmp2) - 12 - z.avail_out; \
  PUT32_BE(tmp2, sz); \
  MAGIC_BE(tmp2 + 4, "IDAT"); \
  crc = crc32(0, tmp2 + 4, sz + 4); \
  PUT32_BE(tmp2 + 8 + sz, crc); \
  if(writefn(tmp2, sz + 12, handle) < sz + 12) \
    goto err; \
  DEFLATE_RESET(); \
} while(0)

#define DEFLATE_OUT(p) do { \
  ret = deflate(&z, p); \
  if(z.avail_out == 0 || ret == Z_STREAM_END) \
    DEFLATE_FLUSH(); \
  if(ret < 0 && ret != Z_BUF_ERROR) \
    goto err; \
} while(0)

enum png_error png_write_fallback(
 uint32_t width, uint32_t height, enum png_write_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
#define BUFFER_PIXEL_BYTES 768u * 4u
  uint8_t tmp[BUFFER_PIXEL_BYTES + 1];
  uint8_t tmp2[BUFFER_PIXEL_BYTES + 12];
  uint32_t crc;
  uint32_t x, y, i;
  uint32_t pixel_pitch;
  size_t buffer_max;
  int png_fmt;
  int ret;
  z_stream z;

  switch(fmt)
  {
    case PNG_WRITE_RGB24:
      pixel_pitch = 3;
      png_fmt = 2; /* RGB */
      break;
    case PNG_WRITE_RGBA32:
      pixel_pitch = 4;
      png_fmt = 6; /* RGB + Alpha */
      break;
    default:
      return PNG_ERROR_INIT;
  }
  buffer_max = BUFFER_PIXEL_BYTES / pixel_pitch;

  memset(&z, 0, sizeof(z));
  if(deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_FILTERED) != Z_OK)
    if(deflateInit2(&z, Z_BEST_SPEED, Z_DEFLATED, 9, 1, Z_RLE) != Z_OK)
      return PNG_ERROR_INIT;

  writefn("\x89PNG\r\n\x1a\n", 8, handle);
  PUT32_BE(tmp, 13);
  MAGIC_BE(tmp + 4, "IHDR");
  PUT32_BE(tmp + 8, width);
  PUT32_BE(tmp + 12, height);
  tmp[16] = 8;        // 8 bits per component
  tmp[17] = png_fmt;  // color type
  tmp[18] = 0;        // deflate
  tmp[19] = 0;        // filter 0
  tmp[20] = 0;        // no interlace
  crc = crc32(0, tmp + 4, 17);
  PUT32_BE(tmp + 21, crc);
  writefn(tmp, 25, handle);

  DEFLATE_RESET();

  for(y = 0; y < height; y++)
  {
    const uint8_t *row = (const uint8_t *)writerowfn(width, priv);
    uint8_t *pos = tmp + pixel_pitch + 1;
    if(!row)
      goto err;

    tmp[0] = 1; // left filter
    for(i = 0; i < pixel_pitch; i++)
      tmp[i + 1] = row[i];

    for(x = 1; x < width;)
    {
      size_t num = IMAGE_MIN(width - x, buffer_max);
      x += num;
      // Prefilter: since it's always left filter RGB(A), delta with the
      // same component of the previous pixel (the value 4 bytes ago).
      while(num)
      {
        for(i = 0; i < pixel_pitch; i++)
          pos[i] = row[i + 4] - row[i];
        pos += pixel_pitch;
        row += 4;
        num--;
      }

      z.next_in = tmp;
      z.avail_in = pos - tmp;
      do
      {
        DEFLATE_OUT(Z_NO_FLUSH);
      } while(z.avail_in != 0);
      pos = tmp;
    }
  }
  do
  {
    DEFLATE_OUT(Z_FINISH);
  } while(ret != Z_STREAM_END);
  deflateEnd(&z);

  memset(tmp, 0, 4);
  writefn(tmp, 4, handle);
  MAGIC_BE(tmp, "IEND");
  crc = crc32(0, tmp, 4);
  PUT32_BE(tmp + 4, crc);
  writefn(tmp, 8, handle);

  return PNG_OK;

err:
  deflateEnd(&z);
  return PNG_ERROR_INVALID;
}

#endif
