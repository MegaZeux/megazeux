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
enum png_error png_read_fallback(void *handle, png_read_function readfn,
 void *priv, png_read_alloc_function allocfn, png_bool skip_signature);

enum png_error png_write_fallback(
 uint32_t width, uint32_t height, enum png_pixel_fmt fmt,
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
 uint32_t width, uint32_t height, enum png_pixel_fmt fmt,
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
    case PNG_PIXEL_RGB24:
      png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      set_filler = 1;
      break;

    case PNG_PIXEL_RGBA32:
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
  /* png_read_fallback is currently not useful enough for this, just fail. */
  return PNG_ERROR_INIT;
}

enum png_error png_write(
 uint32_t width, uint32_t height, enum png_pixel_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
  return png_write_fallback(width, height, fmt, handle, writefn, priv, writerowfn);
}

#endif /* !IMAGE_FILE_LIBPNG */


#if defined(MZX_UNIT_TESTS)

/* Extremely limited fallback PNG reader for builds without libpng.
 * This loader only supports the subset of PNGs that the fallback writer is
 * capable of loading, which is only useful for regression testing.
 *
 * Currently, there is not really any reason to prefer this over libpng.
 */
#include <inttypes.h>
#include <zlib.h>

#define MAGIC_BE(a,b,c,d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define GET32_BE(buf) \
 (((uint32_t)(buf)[0] << 24) | ((buf)[1] << 16) | ((buf)[2] << 8) | ((buf)[3]))

#define INIT_CHUNK() do { \
  uint8_t tmp[8]; \
  if(readfn(tmp, 8, handle) < 8) \
    goto error; \
  chunk_bytes = GET32_BE(tmp + 0); \
  chunk_magic = GET32_BE(tmp + 4); \
  crc = crc32(0, tmp + 4, 4); \
  in_bytes = 0; \
} while(0)

#define CRC_CHUNK() do { \
  uint8_t tmp[4]; \
  if(readfn(tmp, 4, handle) < 4) \
    goto error; \
  if(crc != GET32_BE(tmp)) \
    goto error; \
} while(0)

#define READ_CHUNK() do { if(chunk_bytes) { \
  in_bytes = IMAGE_MIN(chunk_bytes, sizeof(in_buffer)); \
  if(readfn(in_buffer, in_bytes, handle) < in_bytes) \
    goto error; \
  crc = crc32(crc, in_buffer, in_bytes); \
  chunk_bytes -= in_bytes; \
  if(chunk_bytes == 0) \
    CRC_CHUNK(); \
}} while(0)

#define FINISH_CHUNK() while(chunk_bytes) { READ_CHUNK(); }

#define INFLATE_SETUP_ROW() do { \
  z.next_out = row_buffer + (pixel_pitch - 1); \
  z.avail_out = row_bytes - (pixel_pitch - 1); \
} while(0)

#define INFLATE_SETUP_INPUT() do { \
  z.next_in = in_buffer; \
  z.avail_in = in_bytes; \
} while(0)

#define INFLATE_ROW(p) do { \
  zret = inflate(&z, (p)); \
  if(zret < 0 && zret != Z_BUF_ERROR) \
    goto error; \
} while(0)

/* The spec lies about "unsigned arithmetic modulo 256" being used in this
 * function. Only the inputs and return value are uint8_t... */
static inline uint8_t paeth_predictor(int a, int b, int c)
{
  int p = a + b - c;
  int pa = p > a ? p - a : a - p;
  int pb = p > b ? p - b : b - p;
  int pc = p > c ? p - c : c - p;
  return  (pa < pb && pa < pc) ? (uint8_t)a :
          (pb < pc)            ? (uint8_t)b : (uint8_t)c;
}

static inline png_bool filter_row(uint8_t * RESTRICT row_buffer,
 const uint8_t *row_prev, uint32_t width, unsigned pixel_pitch)
{
  /* pixel_pitch extra bytes were allocated at the start of the row to allow
   * for efficient Sub, Average, and Paeth filtering. The filter selector
   * is read to the last of these bytes, and needs to be wiped prior to
   * filtering. */
  uint8_t *pos = row_buffer + pixel_pitch;
  const uint8_t *left;
  const uint8_t *top;
  const uint8_t *top_left;
  const uint8_t *stop = pos + (size_t)width * pixel_pitch;
  unsigned selector = pos[-1];

  pos[-1] = '\0';
  switch(selector)
  {
  case 0: /* None */
    return 1;

  case 1: /* Sub: p[x, y] += p[x - 1, y] */
    left = row_buffer;
    while(pos < stop)
      *(pos++) += *(left++);
    return 1;

  case 2: /* Up: p[x, y] += p[x, y - 1] */
    top = row_prev + pixel_pitch;
    while(pos < stop)
      *(pos++) += *(top++);
    return 1;

  case 3: /* Average: p[x, y] += average(p[x - 1, y], p[x, y - 1]) */
    left = row_buffer;
    top = row_prev + pixel_pitch;
    while(pos < stop)
      *(pos++) += ((unsigned)*(top++) + *(left++)) >> 1u;
    return 1;

  case 4: /* Paeth: p[x, y] += pred(p[x - 1, y], p[x, y - 1], p[x - 1, y - 1]) */
    left = row_buffer;
    top = row_prev + pixel_pitch;
    top_left = row_prev;
    while(pos < stop)
      *(pos++) += paeth_predictor(*(left++), *(top++), *(top_left++));
    return 1;

  default:
    return 0;
  }
}

static png_bool convert_row(struct png_rgba * RESTRICT pixels,
 const uint8_t *row_buffer, uint32_t width, unsigned pixel_pitch,
 enum png_pixel_fmt fmt)
{
  const uint8_t *pos = row_buffer + pixel_pitch;
  uint32_t x;

  switch(fmt)
  {
  case PNG_PIXEL_RGB24:
    for(x = 0; x < width; x++)
    {
      pixels->r = *(pos++);
      pixels->g = *(pos++);
      pixels->b = *(pos++);
      pixels->a = 255;
      pixels++;
    }
    return 1;

  case PNG_PIXEL_RGBA32:
    memcpy(pixels, pos, width * sizeof(struct png_rgba));
    return 1;

  default: /* Should be unreachable */
    return 0;
  }
}

enum png_error png_read_fallback(void *handle, png_read_function readfn,
 void *priv, png_read_alloc_function allocfn, png_bool skip_signature)
{
  uint8_t in_buffer[4096];
  struct png_rgba *pixels;
  struct png_rgba *pixels_end;
  uint8_t *row_buffer = NULL;
  uint8_t *row_prev = NULL;
  uint8_t *row_tmp;
  size_t row_bytes;
  uint32_t chunk_bytes;
  uint32_t chunk_magic;
  uint32_t crc;
  uint32_t in_bytes;
  uint32_t width;
  uint32_t height;
  uint8_t component_depth;
  uint8_t color_type;
  uint8_t interlace_type;
  uint8_t compression_type;
  uint8_t filter_type;
  unsigned pixel_pitch;
  z_stream z;
  int zret;
  enum png_pixel_fmt fmt;
  enum png_error ret = PNG_ERROR_INIT;

  if(!skip_signature)
  {
    uint8_t tmp[8];
    if(readfn(tmp, 8, handle) < 8)
      return PNG_ERROR_INIT;
    if(memcmp(tmp, PNG_SIGNATURE_STRING, 8))
      return PNG_ERROR_INIT;
  }

  INIT_CHUNK();
  if(chunk_magic != MAGIC_BE('I','H','D','R') || chunk_bytes != 13)
    return PNG_ERROR_INIT;

  READ_CHUNK();
  FINISH_CHUNK();
  width             = GET32_BE(in_buffer + 0);
  height            = GET32_BE(in_buffer + 4);
  component_depth   = in_buffer[8];
  color_type        = in_buffer[9];
  interlace_type    = in_buffer[10];
  compression_type  = in_buffer[11];
  filter_type       = in_buffer[12];

  if(width < 1 || height < 1 ||
   interlace_type != 0 || compression_type != 0 || filter_type != 0)
    return PNG_ERROR_INIT;

  if(color_type == 2 && component_depth == 8)
  {
    fmt = PNG_PIXEL_RGB24;
    pixel_pitch = 3;
  }
  else

  if(color_type == 6 && component_depth == 8)
  {
    fmt = PNG_PIXEL_RGBA32;
    pixel_pitch = 4;
  }
  else
    return PNG_ERROR_INIT;

  memset(&z, 0, sizeof(z));
  if(inflateInit2(&z, MAX_WBITS) != Z_OK)
    return PNG_ERROR_INIT;

#if SIZE_MAX <= UINT32_MAX
  if(((uint64_t)width + 1) * pixel_pitch > SIZE_MAX)
    return PNG_ERROR_INIT;
  if((uint64_t)width * height > SIZE_MAX)
    return PNG_ERROR_INIT;
#endif
  row_bytes = ((size_t)width + 1) * pixel_pitch;

  row_buffer  = (uint8_t *)malloc(row_bytes);
  row_prev    = (uint8_t *)malloc(row_bytes);
  if(!row_buffer || !row_prev)
    goto error;

  /* Sub/etc filters assume pixels at x=-1 are zeroed. */
  memset(row_buffer, 0, pixel_pitch);
  /* Up/etc filters assume pixels at y=-1 are zeroed. */
  memset(row_prev, 0, row_bytes);

  pixels = allocfn(width, height, priv);
  if(!pixels)
    goto error;
  pixels_end = pixels + (size_t)width * height;

  ret = PNG_ERROR_INVALID;

  /* Validate all chunks prior to IDAT. */
  INIT_CHUNK();
  while(chunk_magic != MAGIC_BE('I','D','A','T'))
  {
    FINISH_CHUNK();
    INIT_CHUNK();
  }

  while(pixels < pixels_end)
  {
    INFLATE_SETUP_ROW();

    while(z.avail_out)
    {
      while(!z.avail_in)
      {
        if(!chunk_bytes)
        {
          INIT_CHUNK();
          if(chunk_magic != MAGIC_BE('I','D','A','T'))
            goto error;
        }
        READ_CHUNK();
        INFLATE_SETUP_INPUT();
      }
      INFLATE_ROW(Z_FINISH);
    }

    if(!filter_row(row_buffer, row_prev, width, pixel_pitch))
      goto error;
    if(!convert_row(pixels, row_buffer, width, pixel_pitch, fmt))
      goto error;

    pixels += width;

    /* Retain the previous row of pixels for filtering. */
    row_tmp = row_buffer;
    row_buffer = row_prev;
    row_prev = row_tmp;
  }
  FINISH_CHUNK();

  /* Validate all chunks following IDAT until IEND is reached. */
  INIT_CHUNK();
  while(chunk_magic != MAGIC_BE('I','E','N','D'))
  {
    FINISH_CHUNK();
    INIT_CHUNK();
  }
  FINISH_CHUNK();

  ret = PNG_OK;

error:
  inflateEnd(&z);
  free(row_buffer);
  free(row_prev);
  return ret;
}

#endif


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

/* Stack allocated row pixel buffer size, 2k pixels (RGB) or 1.5k (RGBA) */
#define BUFFER_PIXEL_BYTES 512u * 3u * 4u
/* Heap allocated zlib output maximum size plus PNG chunk overhead. */
#define BUFFER_WRITE_SIZE 32768u + 12u

#define PUT_MAGIC(buf, a, b, c, d) do { \
  (buf)[0] = (a); \
  (buf)[1] = (b); \
  (buf)[2] = (c); \
  (buf)[3] = (d); \
} while(0)

#define PUT32_BE(buf, val) do { \
  (buf)[0] = (val) >> 24; \
  (buf)[1] = (val) >> 16; \
  (buf)[2] = (val) >> 8; \
  (buf)[3] = (val); \
} while(0)

#define DEFLATE_RESET() do { \
  z.next_out = write_buf + 8u; \
  z.avail_out = BUFFER_WRITE_SIZE - 12u; \
} while(0)

#define DEFLATE_FLUSH() do { \
  size_t sz = BUFFER_WRITE_SIZE - 12u - z.avail_out; \
  PUT32_BE(write_buf, sz); \
  PUT_MAGIC(write_buf + 4u, 'I','D','A','T'); \
  crc = crc32(0, write_buf + 4u, sz + 4u); \
  PUT32_BE(write_buf + 8u + sz, crc); \
  if(writefn(write_buf, sz + 12u, handle) < sz + 12u) \
    goto error; \
  DEFLATE_RESET(); \
} while(0)

#define DEFLATE_OUT(p) do { \
  ret = deflate(&z, (p)); \
  if(z.avail_out == 0 || ret == Z_STREAM_END) \
    DEFLATE_FLUSH(); \
  if(ret < 0 && ret != Z_BUF_ERROR) \
    goto error; \
} while(0)

enum png_error png_write_fallback(
 uint32_t width, uint32_t height, enum png_pixel_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn)
{
  uint8_t tmp[BUFFER_PIXEL_BYTES + 1u];
  uint8_t *write_buf;
  uint32_t crc;
  uint32_t x, y, i;
  uint32_t pixel_pitch;
  size_t buffer_max;
  int png_fmt;
  int ret;
  z_stream z;

  switch(fmt)
  {
    case PNG_PIXEL_RGB24:
      pixel_pitch = 3;
      png_fmt = 2; /* RGB */
      break;
    case PNG_PIXEL_RGBA32:
      pixel_pitch = 4;
      png_fmt = 6; /* RGB + Alpha */
      break;
    default:
      return PNG_ERROR_INIT;
  }
  buffer_max = BUFFER_PIXEL_BYTES / pixel_pitch;

  write_buf = (uint8_t *)malloc(BUFFER_WRITE_SIZE);
  if(!write_buf)
    return PNG_ERROR_INIT;

  memset(&z, 0, sizeof(z));
  if(deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8, Z_FILTERED) != Z_OK)
  {
    if(deflateInit2(&z, Z_BEST_SPEED, Z_DEFLATED, 9, 1, Z_RLE) != Z_OK)
    {
      free(write_buf);
      return PNG_ERROR_INIT;
    }
  }

  writefn(PNG_SIGNATURE_STRING, 8, handle);
  PUT32_BE(tmp, 13);
  PUT_MAGIC(tmp + 4, 'I','H','D','R');
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
      goto error;

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
  free(write_buf);

  memset(tmp, 0, 4);
  writefn(tmp, 4, handle);
  PUT_MAGIC(tmp, 'I','E','N','D');
  crc = crc32(0, tmp, 4);
  PUT32_BE(tmp + 4, crc);
  writefn(tmp, 8, handle);

  return PNG_OK;

error:
  deflateEnd(&z);
  free(write_buf);
  return PNG_ERROR_INVALID;
}

#endif
