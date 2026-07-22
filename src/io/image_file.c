/* MegaZeux
 *
 * Copyright (C) 2021-2025 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "image_file.h"
#include "image_gif.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#endif

#if 0
#define debug(...) imagedbg("IMG: " __VA_ARGS__)
#else
#define debug imagenodbg
#endif

/* TODO: hack for MegaZeux fopen_unsafe */
#undef fopen

/* Internal compat for MegaZeux boolean type. */
typedef image_bool boolean;
#ifndef __cplusplus
#undef true
#undef false
#define true IMAGE_TRUE
#define false IMAGE_FALSE
#endif

// These constraints are determined by the maximum size of a board/vlayer/MZM,
// multiplied by the number of pixels per char in a given dimension.
#define MAXIMUM_WIDTH  (32767u * 8u)
#define MAXIMUM_HEIGHT (32767u * 14u)
#define MAXIMUM_PIXELS ((16u << 20u) * 8u * 14u)

// Maximum number of magic bytes.
// BMP, NetPBM: 2; GIF: 3; PNG: 8; TGA: none, but 18 header bytes.
#define CHECK_LENGTH 18

#define ROUND_32 (1u << 31u)

#define IMAGE_EOF -1

typedef struct imageinfo
{
  struct image_file *out;
  void *in;
  image_read_function readfn;
  int unget_chr;
}
imageinfo;

const char *image_error_string(enum image_error err)
{
  switch(err)
  {
    case IMAGE_OK:
      return "success";
    case IMAGE_ERROR_UNKNOWN:
      return "unknown error";
    case IMAGE_ERROR_IO:
      return "read error";
    case IMAGE_ERROR_MEM:
      return "out of memory";
    case IMAGE_ERROR_SIGNATURE:
      return "unrecognized image type";
    case IMAGE_ERROR_CONSTRAINT:
      return "image size exceeds maximum";

    case IMAGE_ERROR_RAW_UNSUPPORTED_BPP:
      return "unsupported RAW bits per pixel";

    case IMAGE_ERROR_PNG_INIT:
      return "failed to init PNG decoder";
    case IMAGE_ERROR_PNG_FAILED:
      return "unknown PNG decoder error";

    case IMAGE_ERROR_GIF_INVALID:
      return "invalid GIF data";
    case IMAGE_ERROR_GIF_SIGNATURE:
      return "unsupported GIF type";

    case IMAGE_ERROR_BMP_UNSUPPORTED_DIB:
      return "unsupported BMP DIB header size";
    case IMAGE_ERROR_BMP_UNSUPPORTED_PLANES:
      return "unsupported BMP plane count";
    case IMAGE_ERROR_BMP_UNSUPPORTED_COMPRESSION:
      return "unsupported BMP compression method";
    case IMAGE_ERROR_BMP_UNSUPPORTED_BPP:
      return "unsupported BMP bits per pixel";
    case IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE8:
      return "unsupported BMP bits per pixel (must be 8 for BI_RLE8)";
    case IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE4:
      return "unsupported BMP bits per pixel (must be 4 for BI_RLE4)";
    case IMAGE_ERROR_BMP_UNSUPPORTED_BPP_BITFIELDS:
      return "unsupported BMP bits per pixel (must be 16 or 32 for BI_BITFIELDS)";
    case IMAGE_ERROR_BMP_BAD_1BPP:
      return "error unpacking 1BPP BMP";
    case IMAGE_ERROR_BMP_BAD_2BPP:
      return "error unpacking 2BPP BMP";
    case IMAGE_ERROR_BMP_BAD_4BPP:
      return "error unpacking 4BPP BMP";
    case IMAGE_ERROR_BMP_BAD_8BPP:
      return "error loading 8BPP BMP";
    case IMAGE_ERROR_BMP_BAD_16BPP:
      return "error loading 16BPP BMP";
    case IMAGE_ERROR_BMP_BAD_24BPP:
      return "error loading 24BPP BMP";
    case IMAGE_ERROR_BMP_BAD_32BPP:
      return "error loading 32BPP BMP";
    case IMAGE_ERROR_BMP_BAD_RLE:
      return "error decoding RLE BMP";
    case IMAGE_ERROR_BMP_BAD_SIZE:
      return "invalid BMP image size";
    case IMAGE_ERROR_BMP_BAD_COLOR_TABLE:
      return "invalid BMP color table";
    case IMAGE_ERROR_BMP_BAD_BITFIELDS_DIB:
      return "invalid BMP DIB version for BI_BITFIELDS";
    case IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_CONTINUITY:
      return "invalid non-continuous BMP BI_BITFIELDS mask";
    case IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_OVERLAP:
      return "invalid overlapping BMP BI_BITFIELDS mask";
    case IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_RANGE:
      return "invalid out-of-range BMP BI_BITFIELDS mask for 16bpp";

    case IMAGE_ERROR_PBM_BAD_HEADER:
      return "failed to read PBM header";
    case IMAGE_ERROR_PBM_BAD_MAXVAL:
      return "bad PBM header maxval";
    case IMAGE_ERROR_PAM_BAD_WIDTH:
      return "bad PAM width";
    case IMAGE_ERROR_PAM_BAD_HEIGHT:
      return "bad PAM height";
    case IMAGE_ERROR_PAM_BAD_DEPTH:
      return "bad PAM depth";
    case IMAGE_ERROR_PAM_BAD_MAXVAL:
      return "bad PAM maxval";
    case IMAGE_ERROR_PAM_BAD_TUPLTYPE:
      return "bad PAM tupltype";
    case IMAGE_ERROR_PAM_MISSING_ENDHDR:
      return "missing PAM endhdr";
    case IMAGE_ERROR_PAM_DEPTH_TUPLTYPE_MISMATCH:
      return "PAM tupltype does not support image depth";

    case IMAGE_ERROR_TGA_NOT_A_TGA:
      return "not a TGA";
    case IMAGE_ERROR_TGA_BAD_RLE:
      return "bad TGA RLE stream";
  }
  return "unknown error";
}

static uint16_t read16be(const uint8_t in[2])
{
  return (in[0] << 8u) | in[1];
}

static uint32_t read32be(const uint8_t in[4])
{
  return (in[0] << 24u) | (in[1] << 16u) | (in[2] << 8u) | in[3];
}

static uint16_t read16le(const uint8_t in[2])
{
  return (in[1] << 8u) | in[0];
}

static uint32_t read32le(const uint8_t in[4])
{
  return (in[3] << 24u) | (in[2] << 16u) | (in[1] << 8u) | in[0];
}

/**
 * Image dimensions check for image_init and PNG/GIF loaders.
 */
static boolean image_constraint(uint32_t width, uint32_t height)
{
  debug("Image constraint: %zu by %zu\n", (size_t)width, (size_t)height);
  if(!width || !height || width > MAXIMUM_WIDTH || height > MAXIMUM_HEIGHT ||
   (uint64_t)width * (uint64_t)height > MAXIMUM_PIXELS)
    return false;

  return true;
}

/**
 * Initialize an image file's pixel array and other values.
 */
static enum image_error image_init(uint32_t width, uint32_t height,
 struct image_file *dest)
{
  struct rgba_color *data;

  if(!image_constraint(width, height))
    return IMAGE_ERROR_CONSTRAINT;

  data = (struct rgba_color *)calloc(width * height, sizeof(struct rgba_color));
  if(!data)
    return IMAGE_ERROR_MEM;

  dest->width = width;
  dest->height = height;
  dest->data = data;
  return IMAGE_OK;
}

/**
 * Free an image struct's allocated data. Does not modify any other fields.
 */
void image_free(struct image_file *dest)
{
  free(dest->data);
  dest->data = NULL;
}


#ifdef CONFIG_PNG

/**
 * PNG loader.
 */

#include <setjmp.h>
#include <png.h>

#if PNG_LIBPNG_VER < 10504
#define png_set_scale_16(p) png_set_strip_16(p)
#endif

static void png_read_fn(png_struct *png, png_byte *dest, size_t count)
{
  imageinfo *s = (imageinfo *)png_get_io_ptr(png);
  if(s->readfn(dest, count, s->in) < count)
    png_error(png, "eof");
}

static enum image_error load_png(imageinfo *s)
{
  struct image_file *dest = s->out;
  png_struct *png = NULL;
  png_info *info = NULL;
  png_byte ** volatile row_ptrs = NULL;
  png_uint_32 w;
  png_uint_32 h;
  png_uint_32 i;
  png_uint_32 j;
  int bit_depth;
  int color_type;
  enum image_error ret;

  debug("Image type: PNG\n");
  dest->data = NULL;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png)
    return IMAGE_ERROR_PNG_INIT;
  info = png_create_info_struct(png);
  if(!info)
  {
    ret = IMAGE_ERROR_PNG_INIT;
    goto error;
  }

  if(setjmp(png_jmpbuf(png)))
  {
    ret = IMAGE_ERROR_PNG_FAILED;
    goto error;
  }

  png_set_read_fn(png, s, png_read_fn);
  png_set_sig_bytes(png, 8);

  png_read_info(png, info);
  png_get_IHDR(png, info, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

  ret = image_init(w, h, dest);
  if(ret)
    goto error;

  row_ptrs = (png_byte **)malloc(sizeof(png_byte *) * h);
  if(!row_ptrs)
  {
    ret = IMAGE_ERROR_MEM;
    goto error;
  }

  for(i = 0, j = 0; i < h; i++, j += w)
    row_ptrs[i] = (png_byte *)(dest->data + j);

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
  dest->width = w;
  dest->height = h;
  return IMAGE_OK;

error:
  png_destroy_read_struct(&png, info ? &info : NULL, NULL);

  free(dest->data);
  free(row_ptrs);
  return ret;
}

#endif /* CONFIG_PNG */


/**
 * GIF loader.
 */

static enum image_error convert_gif_error(enum gif_error err)
{
  switch(err)
  {
    case GIF_OK:        return IMAGE_OK;
    case GIF_IO:        return IMAGE_ERROR_IO;
    case GIF_MEM:       return IMAGE_ERROR_MEM;
    case GIF_INVALID:   return IMAGE_ERROR_GIF_INVALID;
    case GIF_SIGNATURE: return IMAGE_ERROR_GIF_SIGNATURE;
  }
  return IMAGE_ERROR_UNKNOWN;
}

static enum image_error load_gif(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct gif_info gif;
  struct gif_rgba *pixels;
  enum gif_error ret;
  unsigned width;
  unsigned height;

  debug("Image type: GIF\n");

  ret = gif_read(&gif, s->in, s->readfn, true);
  if(ret != GIF_OK)
  {
    debug("read failed: %s\n", gif_error_string(ret));
    return convert_gif_error(ret);
  }

  gif_composite_size(&width, &height, &gif);

  if(!image_constraint(width, height))
  {
    gif_close(&gif);
    return IMAGE_ERROR_CONSTRAINT;
  }

  ret = gif_composite(&pixels, &gif, malloc);
  if(ret == GIF_OK)
  {
    dest->data = (struct rgba_color *)pixels;
    dest->width = width;
    dest->height = height;
  }
  else
    debug("composite failed: %s\n", gif_error_string(ret));

  gif_close(&gif);
  return convert_gif_error(ret);
}


/**
 * BMP loader.
 */

#define BMP_HEADER_LEN  14
#define BMP_DIB_MAX_LEN 128

enum dib_type
{
  DIB_UNKNOWN,
  BITMAPCOREHEADER,     // 12
  OS22XBITMAPHEADER,    // 64 or 16
  BITMAPINFOHEADER,     // 40
  BITMAPV2INFOHEADER,   // 52
  BITMAPV3INFOHEADER,   // 56
  BITMAPV4HEADER,       // 108
  BITMAPV5HEADER,       // 124
};

enum dib_compression
{
  BI_RGB            = 0,  // None
  BI_RLE8           = 1,  // 8bpp RLE
  BI_RLE4           = 2,  // 4bpp RLE
  BI_BITFIELDS      = 3,  // RGB (v2) or RGBA (v3+) bitfield masks
  BI_OS2_HUFFMAN    = 3,  // OS22XBITMAPHEADER has this, not BI_BITFIELDS.
  BI_JPEG           = 4,  // JPEG
  BI_OS2_RLE24      = 4,  // OS22XBITMAPHEADER has this, not BI_JPEG.
  BI_PNG            = 5,  // PNG
  BI_ALPHABITFIELDS = 6,  // RGBA bitfield masks (Windows CE 5.0)
  BI_CMYK           = 11,
  BI_BMYKRLE8       = 12,
  BI_CMYKRLE4       = 13,
  BI_MAX
};

struct bmp_bitfield_mask
{
  uint32_t mask;
  unsigned shift;
  uint64_t maxscale;
};

// NOTE: not packed, don't fread directly into this.
struct bmp_header
{
  uint16_t magic;
  uint32_t filesize;
  uint16_t reserved0;
  uint16_t reserved1;
  uint32_t pixarray;

  struct rgba_color *color_table;
  uint32_t streamidx;
  uint32_t rowpadding;
  enum dib_type type;

  // Core DIB header.
  uint32_t dibsize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bpp;

  // BITMAPINFOHEADER fields.
  uint32_t compr_method;
  uint32_t image_size;
  uint32_t horiz_res;
  uint32_t vert_res;
  uint32_t palette_size;
  uint32_t important;

  // BITMAPV2INFOHEADER and BITMAPV3INFOHEADER fields.
  struct bmp_bitfield_mask r_mask;
  struct bmp_bitfield_mask g_mask;
  struct bmp_bitfield_mask b_mask;
  struct bmp_bitfield_mask a_mask;
};

static enum dib_type bmp_get_dib_type(uint32_t length)
{
  switch(length)
  {
    case 12:  return BITMAPCOREHEADER;
    case 64:  return OS22XBITMAPHEADER;
    case 16:  return OS22XBITMAPHEADER;
    case 40:  return BITMAPINFOHEADER;
    case 52:  return BITMAPV2INFOHEADER;
    case 56:  return BITMAPV3INFOHEADER;
    case 108: return BITMAPV4HEADER;
    case 124: return BITMAPV5HEADER;
    default:  return DIB_UNKNOWN;
  }
}

static enum image_error bmp_init_bitfields_mask(const struct bmp_header *bmp,
 struct bmp_bitfield_mask *m, const char *name)
{
  unsigned shift = 0;
  unsigned maxval;

  if(m->mask != 0)
  {
    uint32_t mask = m->mask;

    // Skip leading bits and count the shift.
    while(~mask & 1)
    {
      mask >>= 1;
      shift++;
    }
    // Mask should be continuous.
    while(mask & 1)
      mask >>= 1;

    if(mask != 0)
    {
      debug("non-continuous bitfield.%s: %08x\n", name, (unsigned)m->mask);
      return IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_CONTINUITY;
    }
  }

  if(bmp->bpp == 16 && m->mask & 0xffff0000u)
  {
    debug("nonsense bitfield.%s for 16bpp: %08x\n", name, (unsigned)m->mask);
    return IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_RANGE;
  }

  maxval = m->mask >> shift;

  m->shift = shift;
  m->maxscale = maxval ? ((uint64_t)255 << 32u) / maxval : 0;

  debug("bitfield.%s: (%08xh >> %2u) *%3x.%08xh\n", name,
   (unsigned)m->mask, m->shift, (unsigned)(m->maxscale >> 32), (unsigned)m->maxscale);

  return IMAGE_OK;
}

static enum image_error bmp_init_bitfields(struct bmp_header *bmp)
{
  enum image_error ret;

  ret = bmp_init_bitfields_mask(bmp, &(bmp->r_mask), "r");
  if(ret)
    return ret;

  ret = bmp_init_bitfields_mask(bmp, &(bmp->g_mask), "g");
  if(ret)
    return ret;

  ret = bmp_init_bitfields_mask(bmp, &(bmp->b_mask), "b");
  if(ret)
    return ret;

  ret = bmp_init_bitfields_mask(bmp, &(bmp->a_mask), "a");
  if(ret)
    return ret;

  if((bmp->r_mask.mask & bmp->g_mask.mask) ||
     (bmp->r_mask.mask & bmp->b_mask.mask) ||
     (bmp->r_mask.mask & bmp->a_mask.mask) ||
     (bmp->g_mask.mask & bmp->b_mask.mask) ||
     (bmp->g_mask.mask & bmp->a_mask.mask) ||
     (bmp->b_mask.mask & bmp->a_mask.mask))
  {
    debug("overlapping bitfield masks: %08x %08x %08x %08x\n",
     (unsigned)bmp->r_mask.mask, (unsigned)bmp->g_mask.mask,
     (unsigned)bmp->b_mask.mask, (unsigned)bmp->a_mask.mask);
    return IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_OVERLAP;
  }
  return IMAGE_OK;
}

static inline uint32_t bmp_decode_bitfields_component(
 const struct bmp_bitfield_mask *m, uint32_t value)
{
  return (((value & m->mask) >> m->shift) * m->maxscale + ROUND_32) >> 32u;
}

static inline void bmp_decode_bitfields(const struct bmp_header *bmp,
 struct rgba_color *out, uint32_t value, image_bool alpha)
{
  out->r = bmp_decode_bitfields_component(&(bmp->r_mask), value);
  out->g = bmp_decode_bitfields_component(&(bmp->g_mask), value);
  out->b = bmp_decode_bitfields_component(&(bmp->b_mask), value);

  if(alpha)
    out->a = bmp_decode_bitfields_component(&(bmp->a_mask), value);
  else
    out->a = 255;
}

static struct rgba_color *bmp_read_color_table(struct bmp_header *bmp, imageinfo *s)
{
  uint32_t palette_alloc = (1 << bmp->bpp);
  uint32_t palette_size = bmp->palette_size;
  uint32_t i;
  size_t col_len;
  uint8_t d[4];

  struct rgba_color *color_table = (struct rgba_color *)calloc(palette_alloc, sizeof(struct rgba_color));
  struct rgba_color *pos = color_table;
  if(!color_table)
    return NULL;

  // Ignore values after the allocated palette if they exist...
  if(palette_size > palette_alloc)
    palette_size = palette_alloc;

  // OS/2 1.x uses RGB, everything else uses XRGB.
  // (Bitfields may change this, but currently aren't supported.)
  if(bmp->type == BITMAPCOREHEADER)
  {
    col_len = 3;
  }
  else
    col_len = 4;

  for(i = 0; i < palette_size; i++)
  {
    if(s->readfn(d, col_len, s->in) < col_len)
      break;

    pos->r = d[2];
    pos->g = d[1];
    pos->b = d[0];
    pos->a = 255;
    pos++;
  }
  bmp->streamidx += col_len * palette_size;

  if(i < palette_size)
  {
    free(color_table);
    return NULL;
  }

  return color_table;
}

static enum image_error bmp_pixarray_u1(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;
  int i;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width;)
    {
      char v;
      int value;
      if(s->readfn(&v, 1, s->in) < 1)
        return IMAGE_ERROR_BMP_BAD_1BPP;

      for(value = v, i = 7; i >= 0 && x < bmp->width; i--, x++)
      {
        int idx = (value >> i) & 0x01;
        *(pos++) = color_table[idx];
      }
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_1BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u2(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;
  int i;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width;)
    {
      char v;
      int value;
      if(s->readfn(&v, 1, s->in) < 1)
        return IMAGE_ERROR_BMP_BAD_2BPP;

      for(value = v, i = 6; i >= 0 && x < bmp->width; i -= 2, x++)
      {
        int idx = (value >> i) & 0x03;
        *(pos++) = color_table[idx];
      }
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_2BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u4(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x += 2)
    {
      char value;
      if(s->readfn(&value, 1, s->in) < 1)
        return IMAGE_ERROR_BMP_BAD_4BPP;

      d[0] = (value >> 0) & 0x0f;
      d[1] = (value >> 4) & 0x0f;

      *(pos++) = color_table[d[0]];
      if(x + 1 < bmp->width)
        *(pos++) = color_table[d[1]];
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_4BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u8(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      uint8_t value;
      if(s->readfn(&value, 1, s->in) < 1)
        return IMAGE_ERROR_BMP_BAD_8BPP;

      *(pos++) = color_table[value];
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_8BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u16(const struct bmp_header *bmp,
 imageinfo * RESTRICT s, const image_bool bitfields)
{
  struct image_file *dest = s->out;
  const image_bool alpha = !!bmp->a_mask.mask;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      uint16_t value;
      if(s->readfn(d, 2, s->in) < 2)
        return IMAGE_ERROR_BMP_BAD_16BPP;

      value = read16le(d);

      if(bitfields)
      {
        bmp_decode_bitfields(bmp, pos++, value, alpha);
        continue;
      }

      pos->r = (((value >> 10) & 0x1f) * 255u + 16) / 31u;
      pos->g = (((value >>  5) & 0x1f) * 255u + 16) / 31u;
      pos->b = (((value >>  0) & 0x1f) * 255u + 16) / 31u;
      pos->a = 255;
      pos++;
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_16BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u24(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      if(s->readfn(d, 3, s->in) < 3)
        return IMAGE_ERROR_BMP_BAD_24BPP;

      pos->r = d[2];
      pos->g = d[1];
      pos->b = d[0];
      pos->a = 255;
      pos++;
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return IMAGE_ERROR_BMP_BAD_24BPP;
  }
  return IMAGE_OK;
}

static enum image_error bmp_pixarray_u32(const struct bmp_header *bmp,
 imageinfo * RESTRICT s, const image_bool bitfields)
{
  struct image_file *dest = s->out;
  const image_bool alpha = !!bmp->a_mask.mask;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      if(s->readfn(d, 4, s->in) < 4)
        return IMAGE_ERROR_BMP_BAD_32BPP;

      if(bitfields)
      {
        bmp_decode_bitfields(bmp, pos++, read32le(d), alpha);
        continue;
      }

      pos->r = d[2];
      pos->g = d[1];
      pos->b = d[0];
      pos->a = 255;
      pos++;
    }
    // 32bpp is always aligned, no padding required.
  }
  return IMAGE_OK;
}

static enum image_error bmp_read_pixarray_uncompressed(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  switch(bmp->bpp)
  {
    case 1:   return bmp_pixarray_u1(bmp, s);
    case 2:   return bmp_pixarray_u2(bmp, s);
    case 4:   return bmp_pixarray_u4(bmp, s);
    case 8:   return bmp_pixarray_u8(bmp, s);
    case 16:  return bmp_pixarray_u16(bmp, s, IMAGE_FALSE);
    case 24:  return bmp_pixarray_u24(bmp, s);
    case 32:  return bmp_pixarray_u32(bmp, s, IMAGE_FALSE);
  }
  return IMAGE_ERROR_BMP_UNSUPPORTED_BPP;
}

static enum image_error bmp_read_pixarray_bitfields(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  switch(bmp->bpp)
  {
    case 16: return bmp_pixarray_u16(bmp, s, IMAGE_TRUE);
    case 32: return bmp_pixarray_u32(bmp, s, IMAGE_TRUE);
  }
  return IMAGE_ERROR_BMP_UNSUPPORTED_BPP;
}

/**
 * Microsoft RLE8 and RLE4 compression.
 */
static enum image_error bmp_read_pixarray_rle(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  ssize_t x = 0;
  ssize_t y = bmp->height - 1;
  ssize_t i;
  uint8_t d[2];

  if(bmp->bpp != 4 && bmp->bpp != 8) // Should never happen...
    return IMAGE_ERROR_UNKNOWN;

  while(x < bmp->width && y >= 0)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width + x]);
    //debug("RLE%u now at: %zd %zd\n", bmp->bpp, x, y);

    while(true)
    {
      if(s->readfn(d, 2, s->in) < 2)
        return IMAGE_ERROR_BMP_BAD_RLE;

      if(!d[0])
      {
        if(d[1] == 0)
        {
          // End of line.
          //debug("RLE%u EOL  : %3u %3u\n", bmp->bpp, d[0], d[1]);
          x = 0;
          y--;
          break;
        }
        else

        if(d[1] == 1)
        {
          // End of file.
          //debug("RLE%u EOF  : %3u %3u\n", bmp->bpp, d[0], d[1]);
          return IMAGE_OK;
        }
        else

        if(d[1] == 2)
        {
          // Position delta.
          // Documentation refers to unsigned y moving the current position
          // "down", but since this is still OS/2 line order, it means "up".
          if(s->readfn(d, 2, s->in) < 2)
            return IMAGE_ERROR_BMP_BAD_RLE;

          //debug("RLE%u delta: %3u %3u\n", bmp->bpp, d[0], d[1]);
          x += d[0];
          y -= d[1];
          break;
        }
        else
        {
          // Absolute mode.
          uint8_t idx;
          //debug("RLE%u abs. : %3u %3u\n", bmp->bpp, d[0], d[1]);
          if(bmp->bpp == 8)
          {
            for(i = 0; i < d[1] && x < bmp->width; i++, x++)
            {
              if(s->readfn(&idx, 1, s->in) < 1)
                return IMAGE_ERROR_BMP_BAD_RLE;

              *(pos++) = color_table[idx];
            }
            // Absolute mode runs are padded to word boundaries.
            if(d[1] & 1)
              s->readfn(&idx, 1, s->in);
          }
          else // bpp == 4
          {
            for(i = 0; i < d[1] && x < bmp->width;)
            {
              if(s->readfn(&idx, 1, s->in) < 1)
                return IMAGE_ERROR_BMP_BAD_RLE;

              *(pos++) = color_table[(idx >> 4) & 0x0f];
              i++, x++;

              if(i < d[1] && x < bmp->width)
              {
                *(pos++) = color_table[(idx >> 0) & 0x0f];
                i++, x++;
              }
            }
            // Absolute mode runs are padded to word boundaries.
            if((d[1] & 0x03) == 1 || (d[1] & 0x03) == 2)
              s->readfn(&idx, 1, s->in);
          }

          if(i < d[1])
          {
            debug("RLE%u reached x=bmp->width during absolute run (should this be valid)?\n",
             bmp->bpp);
            return IMAGE_ERROR_BMP_BAD_RLE;
          }
        }
      }
      else
      {
        // Run.
        //debug("RLE%u run  : %3u %02xh\n", bmp->bpp, d[0], d[1]);
        if(bmp->bpp == 8)
        {
          for(i = 0; i < d[0] && x < bmp->width; i++, x++)
            *(pos++) = color_table[d[1]];
        }
        else // bpp == 4
        {
          uint8_t a = (d[1] >> 4) & 0x0f;
          uint8_t b = (d[1] >> 0) & 0x0f;
          for(i = 0; i < d[0] && x < bmp->width;)
          {
            *(pos++) = color_table[a];
            i++, x++;

            if(i < d[0] && x < bmp->width)
            {
              *(pos++) = color_table[b];
              i++, x++;
            }
          }
        }

        if(i < d[0])
        {
          debug("RLE%u reached x=bmp->width during encoded run (should this be valid)?\n",
           bmp->bpp);
          return IMAGE_ERROR_BMP_BAD_RLE;
        }
      }
    }
  }

  // Check for EOF following EOL.
  if(x == 0 && y == -1)
  {
    if(s->readfn(d, 2, s->in) < 2 || d[0] != 0 || d[1] != 1)
      debug("RLE%u missing EOF!\n", bmp->bpp);

    return IMAGE_OK;
  }

  debug("RLE%u out of bounds\n", bmp->bpp);
  return IMAGE_ERROR_BMP_BAD_RLE;
}

static enum image_error load_bmp(imageinfo *s)
{
  struct image_file *dest = s->out;
  uint8_t buf[BMP_DIB_MAX_LEN] = { 'B', 'M' };
  struct bmp_header bmp;
  enum image_error ret;

  debug("Image type: BMP\n");

  // Note: already read magic bytes.
  if(s->readfn(buf + 2, BMP_HEADER_LEN - 2, s->in) < BMP_HEADER_LEN - 2)
    return IMAGE_ERROR_IO;

  memset(&bmp, 0, sizeof(bmp));

  bmp.filesize  = read32le(buf + 2);
  bmp.reserved0 = read16le(buf + 6);
  bmp.reserved0 = read16le(buf + 8);
  bmp.pixarray  = read32le(buf + 10);
  bmp.streamidx = 14;

  // DIB header size.
  if(s->readfn(buf, 4, s->in) < 4)
    return IMAGE_ERROR_IO;

  bmp.dibsize = read32le(buf + 0);
  bmp.type = bmp_get_dib_type(bmp.dibsize);

  if(bmp.type == DIB_UNKNOWN)
  {
    debug("Unknown BMP DIB header size %zu!\n", (size_t)bmp.dibsize);
    return IMAGE_ERROR_BMP_UNSUPPORTED_DIB;
  }

  if(s->readfn(buf + 4, bmp.dibsize - 4, s->in) < bmp.dibsize - 4)
    return IMAGE_ERROR_IO;

  if(bmp.type == BITMAPCOREHEADER)
  {
    debug("BITMAPCOREHEADER\n");
    bmp.width           = read16le(buf + 4);
    bmp.height          = read16le(buf + 6);
    bmp.planes          = read16le(buf + 8);
    bmp.bpp             = read16le(buf + 10);
  }
  else
  {
    debug("BITMAPINFOHEADER or compatible (size %u)\n", (unsigned)bmp.dibsize);
    bmp.width           = (int32_t)read32le(buf + 4);
    bmp.height          = (int32_t)read32le(buf + 8);
    bmp.planes          = read16le(buf + 12);
    bmp.bpp             = read16le(buf + 14);

    if(bmp.dibsize > 16)
    {
      bmp.compr_method  = read32le(buf + 16);
      bmp.image_size    = read32le(buf + 20);
      bmp.horiz_res     = read32le(buf + 24);
      bmp.vert_res      = read32le(buf + 28);
      bmp.palette_size  = read32le(buf + 32);
      bmp.important     = read32le(buf + 36);
    }

    if(bmp.dibsize >= 52 && bmp.type >= BITMAPV2INFOHEADER)
    {
      bmp.r_mask.mask   = read32le(buf + 40);
      bmp.g_mask.mask   = read32le(buf + 44);
      bmp.b_mask.mask   = read32le(buf + 48);
    }

    if(bmp.dibsize >= 56 && bmp.type >= BITMAPV3INFOHEADER)
    {
      bmp.a_mask.mask   = read32le(buf + 52);
    }
  }
  bmp.streamidx += bmp.dibsize;

  if(bmp.width < 0 || bmp.height < 0)
  {
    debug("invalid BMP dimensions %zd x %zd", (ssize_t)bmp.width, (ssize_t)bmp.height);
    return IMAGE_ERROR_BMP_BAD_SIZE;
  }

  if(bmp.planes != 1)
  {
    debug("invalid BMP planes %u\n", bmp.planes);
    return IMAGE_ERROR_BMP_UNSUPPORTED_PLANES;
  }

  /* This compression method is ambiguous with the supported BI_BITFIELDS. */
  if(bmp.type == OS22XBITMAPHEADER && bmp.compr_method == BI_OS2_HUFFMAN)
  {
    debug("unsupported BMP compression type OS/2 Huffman 1D\n");
    return IMAGE_ERROR_BMP_UNSUPPORTED_COMPRESSION;
  }

  if(bmp.compr_method != BI_RGB &&
   bmp.compr_method != BI_RLE8 && bmp.compr_method != BI_RLE4 &&
   bmp.compr_method != BI_BITFIELDS && bmp.compr_method != BI_ALPHABITFIELDS)
  {
    debug("unsupported BMP compression type %zu\n", (size_t)bmp.compr_method);
    return IMAGE_ERROR_BMP_UNSUPPORTED_COMPRESSION;
  }
  debug("Compression: %zu\n", (size_t)bmp.compr_method);

  if(bmp.bpp != 1 && bmp.bpp != 2 && bmp.bpp != 4 && bmp.bpp != 8 &&
   bmp.bpp != 16 && bmp.bpp != 24 && bmp.bpp != 32)
  {
    debug("unsupported BMP BPP %u\n", bmp.bpp);
    return IMAGE_ERROR_BMP_UNSUPPORTED_BPP;
  }
  debug("BPP: %u\n", bmp.bpp);

  if(bmp.compr_method == BI_RLE8 && bmp.bpp != 8)
  {
    debug("unsupported BPP %u for BI_RLE8\n", bmp.bpp);
    return IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE8;
  }

  if(bmp.compr_method == BI_RLE4 && bmp.bpp != 4)
  {
    debug("unsupported BPP %u for BI_RLE4\n", bmp.bpp);
    return IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE4;
  }

  /* Init BI_BITFIELDS data if applicable. The relevant DIB fields must have
   * been loaded and the BPP must be 16 or 32.
   */
  if(bmp.compr_method == BI_BITFIELDS || bmp.compr_method == BI_ALPHABITFIELDS)
  {
    if(bmp.type < BITMAPV2INFOHEADER ||
     (bmp.type < BITMAPV3INFOHEADER && bmp.compr_method == BI_ALPHABITFIELDS))
    {
      debug("unsupported DIB size %d for BI_BITFIELDS %u", bmp.type,
       (unsigned)bmp.compr_method);
      return IMAGE_ERROR_BMP_BAD_BITFIELDS_DIB;
    }

    if(bmp.bpp != 16 && bmp.bpp != 32)
    {
      debug("unsupported BPP %u for BI_BITFIELDS\n", bmp.bpp);
      return IMAGE_ERROR_BMP_UNSUPPORTED_BPP_BITFIELDS;
    }

    ret = bmp_init_bitfields(&bmp);
    if(ret)
    {
      debug("failed to init BMP bitfields masks\n");
      return ret;
    }
  }

  if(bmp.bpp <= 8)
  {
    // If palette_size == 0, use 2^bpp.
    if(!bmp.palette_size)
    {
      bmp.palette_size = (1 << bmp.bpp);
      debug("Palette size: %zu (default)\n", (size_t)bmp.palette_size);
    }
    else
      debug("Palette size: %zu\n", (size_t)bmp.palette_size);

    // Read color table.
    bmp.color_table = bmp_read_color_table(&bmp, s);
    if(!bmp.color_table)
    {
      debug("failed to read BMP color table\n");
      return IMAGE_ERROR_BMP_BAD_COLOR_TABLE;
    }
  }

  // Padding: round up row size (bpp*width) to a whole number of bytes. Padding
  // size is 0 to 3 extra bytes required to pad that value to a multiple of 4.
  bmp.rowpadding = (4 - (((bmp.bpp * (uint64_t)bmp.width + 7) / 8))) & 3;
  debug("Row padding: %zu\n", (size_t)bmp.rowpadding);

  if(bmp.streamidx < bmp.pixarray)
    debug("Bytes to skip for pixarray: %zu\n", (size_t)bmp.pixarray - bmp.streamidx);

  // Assume non-seeking stream, skip everything prior to pixarray with fread.
  while(bmp.streamidx < bmp.pixarray)
  {
    size_t r = IMAGE_MIN(bmp.pixarray - bmp.streamidx, sizeof(buf));
    bmp.streamidx += r;

    if(s->readfn(buf, 1, s->in) < 1)
    {
      ret = IMAGE_ERROR_IO;
      goto err_free;
    }
  }

  ret = image_init(bmp.width, bmp.height, dest);
  if(ret)
    goto err_free;

  switch(bmp.compr_method)
  {
    case BI_RGB:
      ret = bmp_read_pixarray_uncompressed(&bmp, s);
      break;

    // ImageMagick tends to automatically emit this for 8bpp BMPs.
    // GIMP can emit both types of RLE, though this is turned off by default.
    case BI_RLE8:
    case BI_RLE4:
      ret = bmp_read_pixarray_rle(&bmp, s);
      break;

    case BI_BITFIELDS:
    case BI_ALPHABITFIELDS:
      ret = bmp_read_pixarray_bitfields(&bmp, s);
      break;

    default:
      // Shouldn't happen--method is filtered above!
      ret = IMAGE_ERROR_UNKNOWN;
      break;
  }

  if(ret)
    goto err_free_image;

  free(bmp.color_table);
  return IMAGE_OK;

err_free_image:
  debug("error reading pixarray\n");
  image_free(dest);
err_free:
  free(bmp.color_table);
  return ret;
}


/**
 * Truevision TGA (Targa) loader.
 */

enum tga_colormap_type
{
  TGA_NO_COLORMAP,
  TGA_HAS_COLORMAP,
  TGA_MAX_COLORMAP_TYPE
};

enum tga_image_type
{
  TGA_TYPE_NO_IMAGE       = 0,
  TGA_TYPE_INDEXED        = 1,
  TGA_TYPE_TRUECOLOR      = 2,
  TGA_TYPE_GREYSCALE      = 3,
  TGA_TYPE_INDEXED_RLE    = 9,
  TGA_TYPE_TRUECOLOR_RLE  = 10,
  TGA_TYPE_GREYSCALE_RLE  = 11,
  TGA_MAX_IMAGE_TYPE
};

struct tga_header
{
  uint8_t   image_id_length;
  uint8_t   map_type;
  uint8_t   image_type;
  uint16_t  map_first_index;
  uint16_t  map_length;
  uint8_t   map_depth;
  uint16_t  x_origin;
  uint16_t  y_origin;
  uint16_t  width;
  uint16_t  height;
  uint8_t   depth;
  uint8_t   descriptor;
  // Derived
  uint8_t   alpha_depth;
  uint8_t   interleaving;
  boolean   left_to_right;
  boolean   top_to_bottom;
  boolean   alpha;
  unsigned  bytes_per_pixel;
  unsigned  bytes_per_color;
  int       offset_in_image;
  int       offset_in_row;
  int       row_inc;
  int       pix_inc;

  struct rgba_color *map;
};

struct tga_cursor
{
  struct rgba_color *pixel_row;
  struct rgba_color *pixel;
  struct rgba_color *pixel_stop;
};

static void tga_cursor_init(struct tga_cursor *out, const struct tga_header *tga,
 struct rgba_color *dest)
{
  out->pixel_row = dest + tga->offset_in_image;
  out->pixel = out->pixel_row + tga->offset_in_row;
  out->pixel_stop = out->pixel + (tga->pix_inc * tga->width);
}

static void tga_cursor_inc(struct tga_cursor *out, const struct tga_header *tga)
{
  out->pixel += tga->pix_inc;
  if(out->pixel == out->pixel_stop)
  {
    out->pixel_row += tga->row_inc;
    out->pixel_stop += tga->row_inc;
    out->pixel = out->pixel_row + tga->offset_in_row;
  }
}

static void tga_cursor_rle(struct tga_cursor *out, const struct tga_header *tga,
 struct rgba_color color, size_t num)
{
  for(; num > 0; num--)
  {
    *(out->pixel) = color;
    tga_cursor_inc(out, tga);
  }
}

static int tga_bytes_per_pixel(uint8_t depth, uint8_t alpha_depth)
{
  if(depth == 15 && alpha_depth == 0)
    return 2;

  if(depth == 16 && (alpha_depth == 0 || alpha_depth == 1))
    return 2;

  if(depth == 24 && alpha_depth == 0)
    return 3;

  if(depth == 32 && (alpha_depth == 0 || alpha_depth == 8))
    return 4;

  return 0;
}

typedef struct rgba_color (*tga_decode_fn)(uint8_t *buf,
 const struct tga_header *tga);

static struct rgba_color tga_decode_index8(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color dummy = { 0, 0, 0, 255 };
  if(*buf < tga->map_length)
    return tga->map[*buf];

  return dummy;
}

static struct rgba_color tga_decode_index16(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color dummy = { 0, 0, 0, 255 };
  uint16_t index = read16le(buf);
  if(index < tga->map_length)
    return tga->map[index];

  return dummy;
}

static struct rgba_color tga_decode_greyscale(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color ret = { *buf, *buf, *buf, 255 };
  return ret;
}

static struct rgba_color tga_decode_color16(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color ret;
  uint16_t value = read16le(buf);
  ret.r = (((value >> 10) & 0x1f) * 255u + 16) / 31u;
  ret.g = (((value >>  5) & 0x1f) * 255u + 16) / 31u;
  ret.b = (((value >>  0) & 0x1f) * 255u + 16) / 31u;
  ret.a = !tga->alpha || (value >> 15) ? 255 : 0;
  return ret;
}

static struct rgba_color tga_decode_color24(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color ret = { buf[2], buf[1], buf[0], 255 };
  return ret;
}

static struct rgba_color tga_decode_color32(uint8_t *buf,
 const struct tga_header *tga)
{
  struct rgba_color ret = { buf[2], buf[1], buf[0], buf[3] };
  return ret;
}

static enum image_error tga_load_colormap(struct tga_header *tga, imageinfo *s)
{
  const int depth = tga->map_depth;
  uint8_t buf[4];
  size_t i;

  tga->map = (struct rgba_color *)calloc(tga->map_length, sizeof(struct rgba_color));
  if(!tga->map)
    return IMAGE_ERROR_MEM;

  for(i = 0; i < tga->map_length; i++)
  {
    if(s->readfn(buf, tga->bytes_per_color, s->in) < tga->bytes_per_color)
      return IMAGE_ERROR_IO;

    switch(depth)
    {
      case 15:
      case 16:
        tga->map[i] = tga_decode_color16(buf, tga);
        break;
      case 24:
        tga->map[i] = tga_decode_color24(buf, tga);
        break;
      case 32:
        tga->map[i] = tga_decode_color32(buf, tga);
        break;
    }
  }
  return IMAGE_OK;
}

static inline enum image_error tga_load_inner(const tga_decode_fn decode,
 struct tga_cursor *out, size_t num,
 const struct tga_header *tga, uint8_t * RESTRICT rowbuf, imageinfo *s)
{
  const size_t bytes_per_pixel = tga->bytes_per_pixel;
  uint8_t *pos;

  while(num > 0)
  {
    size_t sz = IMAGE_MIN(num, tga->width);
    num -= sz;
    if(s->readfn(rowbuf, sz * bytes_per_pixel, s->in) < sz * bytes_per_pixel)
      return IMAGE_ERROR_IO;

    pos = rowbuf;
    while(sz > 0)
    {
      *(out->pixel) = decode(pos, tga);
      tga_cursor_inc(out, tga);
      pos += bytes_per_pixel;
      sz--;
    }
  }
  return IMAGE_OK;
}

static inline enum image_error tga_load_image(const tga_decode_fn decode,
 const struct tga_header *tga, uint8_t * RESTRICT rowbuf, imageinfo *s)
{
  struct tga_cursor out;
  tga_cursor_init(&out, tga, s->out->data);
  return tga_load_inner(decode, &out, tga->width * tga->height, tga, rowbuf, s);
}

static inline enum image_error tga_load_image_rle(const tga_decode_fn decode,
 const struct tga_header *tga, uint8_t * RESTRICT rowbuf, imageinfo *s)
{
  struct tga_cursor out;
  struct rgba_color color;
  const size_t bytes_per_pixel = tga->bytes_per_pixel;
  enum image_error ret;
  size_t num, total;

  tga_cursor_init(&out, tga, s->out->data);
  for(total = tga->width * tga->height; total > 0;)
  {
    if(s->readfn(rowbuf, bytes_per_pixel + 1, s->in) < bytes_per_pixel + 1)
      return IMAGE_ERROR_IO;

    num = (rowbuf[0] & 0x7f) + 1;
    if(num > total)
      return IMAGE_ERROR_TGA_BAD_RLE;
    total -= num;

    color = decode(rowbuf + 1, tga);
    if(rowbuf[0] & 0x80)
    {
      tga_cursor_rle(&out, tga, color, num);
    }
    else
    {
      *(out.pixel) = color;
      tga_cursor_inc(&out, tga);

      if(num > 1)
      {
        ret = tga_load_inner(decode, &out, num - 1, tga, rowbuf, s);
        if(ret)
          return ret;
      }
    }
  }
  return IMAGE_OK;
}

static enum image_error load_tga(imageinfo *s, const uint8_t *header,
 size_t header_sz)
{
  struct tga_header tga;
  struct image_file *dest = s->out;
  uint8_t *rowbuf;
  uint8_t buf[256];
  size_t total_pixels;
  enum image_error ret;

  if(header_sz < 18)
    return IMAGE_ERROR_TGA_NOT_A_TGA;

  memset(&tga, 0, sizeof(tga));

  tga.image_id_length = header[0];
  tga.map_type        = header[1];
  tga.image_type      = header[2];
  //tga.map_first_index = read16le(header + 3);
  tga.map_length      = read16le(header + 5);
  tga.map_depth       = header[7];
  //tga.x_origin        = read16le(header + 8);
  //tga.y_origin        = read16le(header + 10);
  tga.width           = read16le(header + 12);
  tga.height          = read16le(header + 14);
  tga.depth           = header[16];
  tga.descriptor      = header[17];

  //tga.map_last_index = tga.map_first_index + tga.map_length;
  tga.alpha_depth = tga.descriptor & 0x0f;
  tga.interleaving = tga.descriptor >> 6;
  tga.left_to_right = (tga.descriptor & 0x10) == 0;
  tga.top_to_bottom = (tga.descriptor & 0x20) != 0;

  switch(tga.image_type)
  {
    case TGA_TYPE_INDEXED:
    case TGA_TYPE_INDEXED_RLE:
    {
      if(tga.map_type != TGA_HAS_COLORMAP)
        goto not_a_tga;
      if(tga.depth != 8 && tga.depth != 15 && tga.depth != 16)
        goto not_a_tga;
      if(!tga.map_length)
        goto not_a_tga;
      if(tga.depth == 8 && tga.map_length > 256)
        goto not_a_tga;
      tga.bytes_per_color = tga_bytes_per_pixel(tga.map_depth, tga.alpha_depth);
      if(!tga.bytes_per_color)
        goto not_a_tga;

      tga.bytes_per_pixel = (tga.depth > 8) ? 2 : 1;
      tga.alpha = tga.map_depth == 16 || tga.map_depth == 32;
      break;
    }

    case TGA_TYPE_TRUECOLOR:
    case TGA_TYPE_TRUECOLOR_RLE:
    {
      if(tga.map_type != TGA_NO_COLORMAP)
        goto not_a_tga;
      tga.bytes_per_pixel = tga_bytes_per_pixel(tga.depth, tga.alpha_depth);
      if(!tga.bytes_per_pixel)
        goto not_a_tga;
      tga.alpha = tga.depth == 16 || tga.depth == 32;
      break;
    }

    case TGA_TYPE_GREYSCALE:
    case TGA_TYPE_GREYSCALE_RLE:
    {
      if(tga.map_type != TGA_NO_COLORMAP)
        goto not_a_tga;
      if(tga.depth != 8)
        goto not_a_tga;
      if(tga.alpha_depth != 0)
        goto not_a_tga;

      tga.bytes_per_pixel = 1;
      break;
    }

    case TGA_TYPE_NO_IMAGE:
    default:
      goto not_a_tga;
  }

  if(tga.width == 0 || tga.height == 0 || tga.interleaving != 0)
    goto not_a_tga;

  debug("Image type: Truevision TGA\n");
  debug("  type: %u\n", tga.image_type);
  debug("  v. order: %s\n", tga.top_to_bottom ? "top-to-bottom" : "bottom-to-top");
  debug("  h. order: %s\n", tga.left_to_right ? "left-to-right" : "right-to-left");
  debug("  indexed: %s\n", tga.map_type ? "yes" : "no");
  debug("  pixel depth: %u\n", tga.depth);
  if(tga.map_type)
  {
    debug("  palette start: %u\n", tga.map_first_index);
    debug("  stored colors: %u\n", tga.map_length);
    debug("  palette depth: %u\n", tga.map_depth);
  }

  /* Skip image identification field. */
  if(s->readfn(buf, tga.image_id_length, s->in) < tga.image_id_length)
    return IMAGE_ERROR_IO;

  /* Load color map. */
  if(tga.map_type == TGA_HAS_COLORMAP)
  {
    if(tga.image_type == TGA_TYPE_INDEXED ||
     tga.image_type == TGA_TYPE_INDEXED_RLE)
    {
      ret = tga_load_colormap(&tga, s);
      if(ret)
      {
        debug("error loading colormap\n");
        return ret;
      }
    }
    else
    {
      /* Skip map if present (shouldn't happen). */
      size_t map_len = (size_t)tga.bytes_per_pixel * tga.map_length;
      while(map_len)
      {
        size_t sz = IMAGE_MIN(map_len, sizeof(buf));
        if(s->readfn(buf, sz, s->in) < sz)
        {
          debug("short read skipping colormap\n");
          return IMAGE_ERROR_MEM;
        }
        map_len -= sz;
      }
    }
  }

  /* Image data */
  rowbuf = (uint8_t *)malloc(tga.width * tga.bytes_per_pixel);
  if(!rowbuf)
  {
    debug("failed to allocate row buffer\n");
    ret = IMAGE_ERROR_MEM;
    goto err_free;
  }

  ret = image_init(tga.width, tga.height, dest);
  if(ret)
  {
    free(rowbuf);
    return ret;
  }

  total_pixels = (size_t)tga.width * tga.height;
  tga.offset_in_image = 0;
  tga.offset_in_row = 0;
  tga.row_inc = tga.width;
  tga.pix_inc = 1;

  if(!tga.top_to_bottom)
  {
    tga.offset_in_image = total_pixels - tga.width;
    tga.row_inc = -tga.width;
  }
  if(!tga.left_to_right)
  {
    tga.offset_in_row = tga.width - 1;
    tga.pix_inc = -1;
  }

  ret = IMAGE_ERROR_UNKNOWN;
  switch(tga.image_type)
  {
    case TGA_TYPE_INDEXED:
    {
      switch(tga.depth)
      {
        case 8:
          ret = tga_load_image(tga_decode_index8, &tga, rowbuf, s);
          break;

        case 15:
        case 16:
          ret = tga_load_image(tga_decode_index16, &tga, rowbuf, s);
          break;
      }
      break;
    }

    case TGA_TYPE_INDEXED_RLE:
    {
      switch(tga.depth)
      {
        case 8:
          ret = tga_load_image_rle(tga_decode_index8, &tga, rowbuf, s);
          break;

        case 15:
        case 16:
          ret = tga_load_image_rle(tga_decode_index16, &tga, rowbuf, s);
          break;
      }
      break;
    }

    case TGA_TYPE_TRUECOLOR:
    {
      switch(tga.depth)
      {
        case 15:
        case 16:
          ret = tga_load_image(tga_decode_color16, &tga, rowbuf, s);
          break;
        case 24:
          ret = tga_load_image(tga_decode_color24, &tga, rowbuf, s);
          break;
        case 32:
          ret = tga_load_image(tga_decode_color32, &tga, rowbuf, s);
          break;
      }
      break;
    }

    case TGA_TYPE_TRUECOLOR_RLE:
    {
      switch(tga.depth)
      {
        case 15:
        case 16:
          ret = tga_load_image_rle(tga_decode_color16, &tga, rowbuf, s);
          break;
        case 24:
          ret = tga_load_image_rle(tga_decode_color24, &tga, rowbuf, s);
          break;
        case 32:
          ret = tga_load_image_rle(tga_decode_color32, &tga, rowbuf, s);
          break;
      }
      break;
    }

    case TGA_TYPE_GREYSCALE:
      ret = tga_load_image(tga_decode_greyscale, &tga, rowbuf, s);
      break;

    case TGA_TYPE_GREYSCALE_RLE:
      ret = tga_load_image_rle(tga_decode_greyscale, &tga, rowbuf, s);
      break;
  }
  if(ret)
  {
    debug("error reading image data\n");
    image_free(dest);
  }

err_free:
  free(rowbuf);
  free(tga.map);
  return ret;

not_a_tga:
  return IMAGE_ERROR_TGA_NOT_A_TGA;
}


/**
 * NetPBM loaders.
 */

static int next_byte(imageinfo *s)
{
  uint8_t chr;
  if(s->unget_chr >= 0)
  {
    int tmp = s->unget_chr;
    s->unget_chr = -1;
    return tmp;
  }

  if(s->readfn(&chr, 1, s->in) < 1)
    return IMAGE_EOF;

  return chr;
}

static boolean skip_whitespace(imageinfo *s)
{
  int value = next_byte(s);
  if(value < 0)
    return false;

  if(value == '#')
  {
    // Skip line comment...
    while(value != '\n' && value >= 0)
      value = next_byte(s);

    return true;
  }

  if(isspace(value))
    return true;

  s->unget_chr = value;
  return false;
}

static int next_number(uint32_t *output, imageinfo *s)
{
  int value;
  int count;
  *output = 0;

  while(skip_whitespace(s));

  count = 0;
  for(count = 0; count <= 10; count++)
  {
    value = next_byte(s);
    if(!isdigit(value))
    {
      if(value == '#')
        skip_whitespace(s);
      else

      if(!isspace(value))
        break;

      return value;
    }

    *output = (*output * 10) + (value - '0');
  }

  // Digit count overflowed the uint32_t return value, or invalid character.
  return IMAGE_EOF;
}

static int next_value(uint32_t maxval, imageinfo *s)
{
  uint32_t v;
  if(next_number(&v, s) < 0 || v > maxval)
    return IMAGE_EOF;

  return v;
}

static int next_bit_value(imageinfo *s)
{
  int v;

  // Unlike plaintext PGM and PPM, plaintext PBM doesn't require whitespace to
  // separate components--each component is always a single digit character.
  while(skip_whitespace(s));

  v = next_byte(s);
  if(v != '0' && v != '1')
    return IMAGE_EOF;

  return (v - '0');
}

static int next_binary(uint32_t maxval, imageinfo *s)
{
  uint32_t v;
  if(maxval > 255)
    v = (next_byte(s) << 8) | next_byte(s);
  else
    v = next_byte(s);

  return (v > maxval) ? IMAGE_EOF : (int)v;
}

static char *next_line(char *dest, int dest_len, imageinfo *s)
{
  char *pos = dest;
  int num = 1;
  int v = -1;
  while(num < dest_len)
  {
    v = next_byte(s);
    if(v < 0)
    {
      if(num == 1)
        return NULL;
      break;
    }
    if(v == '\r' || v == '\n')
      break;

    *(pos++) = v;
    num++;
  }
  if(v == '\r')
  {
    v = next_byte(s);
    if(v != '\n')
      s->unget_chr = v;
  }
  *pos = '\0';
  return dest;
}

static enum image_error pbm_header(uint32_t *width, uint32_t *height, imageinfo *s)
{
  // NOTE: already read magic.
  uint32_t w = 0;
  uint32_t h = 0;

  if(next_number(&w, s) < 0 ||
   next_number(&h, s) < 0)
    return IMAGE_ERROR_PBM_BAD_HEADER;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(s);

  *width = w;
  *height = h;
  return IMAGE_OK;
}

static enum image_error ppm_header(uint32_t *width, uint32_t *height,
 uint32_t *maxval, imageinfo *s)
{
  // NOTE: already read magic. PGM and PPM use the same header format.
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t m = 0;

  if(next_number(&w, s) < 0 ||
   next_number(&h, s) < 0 ||
   next_number(&m, s) < 0)
    return IMAGE_ERROR_PBM_BAD_HEADER;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(s);

  if(!m || m > 65535)
    return IMAGE_ERROR_PBM_BAD_MAXVAL;

  *width = w;
  *height = h;
  *maxval = m;
  return IMAGE_OK;
}

static enum image_error load_portable_bitmap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;

  debug("Image type: PBM (P1)\n");

  ret = pbm_header(&width, &height, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;

  for(i = 0; i < size; i++)
  {
    int val = next_bit_value(s);
    if(val < 0)
    {
      debug("short read in PBM P1 (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    val = val ? 0 : 255;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return IMAGE_OK;
}

static enum image_error load_portable_bitmap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;

  debug("Image type: PBM (P4)\n");

  ret = pbm_header(&width, &height, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;

  // Each row is padded to a whole number of bytes...
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width;)
    {
      int val = next_byte(s);
      int mask;
      if(val < 0)
      {
        debug("short read in PBM P4\n");
        return IMAGE_OK;
      }

      for(mask = 0x80; mask && x < width; mask >>= 1, x++)
      {
        int pixval = (val & mask) ? 0 : 255;
        pos->r = pixval;
        pos->g = pixval;
        pos->b = pixval;
        pos->a = 255;
        pos++;
      }
    }
  }
  return IMAGE_OK;
}

static enum image_error load_portable_greymap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P2)\n");

  ret = ppm_header(&width, &height, &maxval, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int val = next_value(maxval, s);
    if(val < 0)
    {
      debug("short read in PGM P2 (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    val = (val * maxscale + ROUND_32) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return IMAGE_OK;
}

static enum image_error load_portable_greymap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P5)\n");

  ret = ppm_header(&width, &height, &maxval, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int val = next_binary(maxval, s);
    if(val < 0)
    {
      debug("short read in PGM P5 (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    val = (val * maxscale + ROUND_32) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return IMAGE_OK;
}

static enum image_error load_portable_pixmap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P3)\n");

  ret = ppm_header(&width, &height, &maxval, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int r = next_value(maxval, s);
    int g = next_value(maxval, s);
    int b = next_value(maxval, s);
    if(r < 0 || g < 0 || b < 0)
    {
      debug("short read in PPM P3 (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    pos->r = (r * maxscale + ROUND_32) >> 32u;
    pos->g = (g * maxscale + ROUND_32) >> 32u;
    pos->b = (b * maxscale + ROUND_32) >> 32u;
    pos->a = 255;
    pos++;
  }
  return IMAGE_OK;
}

static enum image_error load_portable_pixmap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  enum image_error ret;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P6)\n");

  ret = ppm_header(&width, &height, &maxval, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int r = next_binary(maxval, s);
    int g = next_binary(maxval, s);
    int b = next_binary(maxval, s);
    if(r < 0 || g < 0 || b < 0)
    {
      debug("short read in PPM P6 (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    pos->r = (r * maxscale + ROUND_32) >> 32u;
    pos->g = (g * maxscale + ROUND_32) >> 32u;
    pos->b = (b * maxscale + ROUND_32) >> 32u;
    pos->a = 255;
    pos++;
  }
  return IMAGE_OK;
}

struct pam_tupl
{
  const char *name;
  uint8_t mindepth;
  uint8_t red_pos;
  uint8_t green_pos;
  uint8_t blue_pos;
  int8_t alpha_pos;
};

static enum image_error pam_set_tupltype(struct pam_tupl *dest, const char *tuplstr)
{
  static const struct pam_tupl tupltypes[] =
  {
    { "BLACKANDWHITE",        1, 0, 0, 0, -1 },
    { "GRAYSCALE",            1, 0, 0, 0, -1 },
    { "RGB",                  3, 0, 1, 2, -1 },
    { "BLACKANDWHITE_ALPHA",  2, 0, 0, 0,  1 },
    { "GRAYSCALE_ALPHA",      2, 0, 0, 0,  1 },
    { "RGB_ALPHA",            4, 0, 1, 2,  3 },
  };

  size_t i;
  for(i = 0; i < ARRAY_SIZE(tupltypes); i++)
  {
    if(!strcasecmp(tupltypes[i].name, tuplstr))
    {
      *dest = tupltypes[i];
      return IMAGE_OK;
    }
  }
  debug("unsupported TUPLTYPE '%s'!\n", tuplstr);
  return IMAGE_ERROR_PAM_BAD_TUPLTYPE;
}

static enum image_error pam_header(uint32_t *width, uint32_t *height,
 uint32_t *maxval, uint32_t *depth, struct pam_tupl *tupltype, imageinfo *s)
{
  char linebuf[256];
  char tuplstr[256];
  char *key;
  char *value;
  boolean has_w = false;
  boolean has_h = false;
  boolean has_m = false;
  boolean has_d = false;
  boolean has_e = false;
  boolean has_tu = false;
  enum image_error ret;

  while(next_line(linebuf, 256, s))
  {
    value = linebuf;
    while(isspace((uint8_t )*value))
      value++;

    if(!*value || *value == '#')
      continue;

    key = value;
    while(*value && !isspace((uint8_t)*value))
      value++;

    if(*value)
      *(value++) = '\0';

    if(!strcasecmp(key, "ENDHDR"))
    {
      has_e = true;
      break;
    }
    else

    if(!strcasecmp(key, "WIDTH"))
    {
      if(!*value || has_w)
        return IMAGE_ERROR_PAM_BAD_WIDTH;

      *width = strtoul(value, NULL, 10);
      has_w = true;
    }
    else

    if(!strcasecmp(key, "HEIGHT"))
    {
      if(!*value || has_h)
        return IMAGE_ERROR_PAM_BAD_HEIGHT;

      *height = strtoul(value, NULL, 10);
      has_h = true;
    }
    else

    if(!strcasecmp(key, "DEPTH"))
    {
      if(!*value || has_d)
        return IMAGE_ERROR_PAM_BAD_DEPTH;

      *depth = strtoul(value, NULL, 10);
      has_d = true;
    }
    else

    if(!strcasecmp(key, "MAXVAL"))
    {
      if(!*value || has_m)
        return IMAGE_ERROR_PAM_BAD_MAXVAL;

      *maxval = strtoul(value, NULL, 10);
      has_m = true;
    }
    else

    if(!strcasecmp(key, "TUPLTYPE"))
    {
      // NOTE: the spec supports combining multiple of these, but doesn't use it.
      if(!*value || has_tu)
        return IMAGE_ERROR_PAM_BAD_TUPLTYPE;

      snprintf(tuplstr, sizeof(tuplstr), "%s", value);
      has_tu = true;
    }
  }

  if(!has_w)
    return IMAGE_ERROR_PAM_BAD_WIDTH;
  if(!has_h)
    return IMAGE_ERROR_PAM_BAD_HEIGHT;
  if(!has_d)
    return IMAGE_ERROR_PAM_BAD_DEPTH;
  if(!has_m)
    return IMAGE_ERROR_PAM_BAD_MAXVAL;
  if(!has_e)
    return IMAGE_ERROR_PAM_MISSING_ENDHDR;

  if(*depth < 1 || *depth > 4)
  {
    debug("invalid depth '%u'!\n", (unsigned int)*depth);
    return IMAGE_ERROR_PAM_BAD_DEPTH;
  }

  if(*maxval < 1 || *maxval > 65535)
  {
    debug("invalid maxval '%u'!\n", (unsigned int)*maxval);
    return IMAGE_ERROR_PAM_BAD_MAXVAL;
  }

  if(has_tu)
    has_tu = pam_set_tupltype(tupltype, tuplstr);

  if(!has_tu)
  {
    // Guess TUPLTYPE from depth.
    const char *guess = "";
    switch(*depth)
    {
      case 1: guess = "GRAYSCALE"; break;
      case 2: guess = "GRAYSCALE_ALPHA"; break;
      case 3: guess = "RGB"; break;
      case 4: guess = "RGB_ALPHA"; break;
    }
    ret = pam_set_tupltype(tupltype, guess);
    if(ret)
      return ret;
  }

  if(tupltype->mindepth > *depth)
    return IMAGE_ERROR_PAM_DEPTH_TUPLTYPE_MISMATCH;

  return IMAGE_OK;
}

static enum image_error load_portable_arbitrary_map(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  struct pam_tupl tupltype;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  uint32_t maxval = 0;
  uint64_t maxscale = 0;
  uint32_t size;
  uint32_t i;
  enum image_error ret;
  int v[4] = { 0 };

  debug("Image type: PAM (P7)\n");

  // Shut up GCC 14 false positive
  memset(&tupltype, 0, sizeof(tupltype));

  ret = pam_header(&width, &height, &maxval, &depth, &tupltype, s);
  if(ret)
    return ret;

  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    uint32_t j;

    for(j = 0; j < depth; j++)
    {
      v[j] = next_binary(maxval, s);
      if(v[j] < 0)
      {
        debug("short read in PAM P7 (read %zu of %zu)\n", (size_t)i, (size_t)size);
        return IMAGE_OK;
      }

      v[j] = (v[j] * maxscale + ROUND_32) >> 32u;
    }

    pos->r = v[tupltype.red_pos];
    pos->g = v[tupltype.green_pos];
    pos->b = v[tupltype.blue_pos];
    pos->a = tupltype.alpha_pos >= 0 ? v[tupltype.alpha_pos] : 255;
    pos++;
  }
  return IMAGE_OK;
}


/**
 * Farbfeld loader.
 */

static uint8_t convert_16b_to_8b(uint16_t component)
{
  return (uint32_t)(component * 255u + 32768u) / 65535u;
}

static enum image_error load_farbfeld(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint8_t buf[8];
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;
  enum image_error ret;

  debug("Image type: farbfeld\n");

  if(s->readfn(buf, 8, s->in) < 8)
    return IMAGE_ERROR_IO;

  width  = read32be(buf + 0);
  height = read32be(buf + 4);
  ret = image_init(width, height, dest);
  if(ret)
    return ret;

  pos = dest->data;
  size = width * height;

  for(i = 0; i < size; i++)
  {
    if(s->readfn(buf, 8, s->in) < 8)
    {
      debug("short read in ff (read %zu of %zu)\n", (size_t)i, (size_t)size);
      break;
    }

    pos->r = convert_16b_to_8b(read16be(buf + 0));
    pos->g = convert_16b_to_8b(read16be(buf + 2));
    pos->b = convert_16b_to_8b(read16be(buf + 4));
    pos->a = convert_16b_to_8b(read16be(buf + 6));
    pos++;
  }
  return IMAGE_OK;
}


/**
 * Raw format loader.
 * `not_magic` is the first few bytes of the file read for the purposes of
 * identifying a file format. For raw files, this is actually pixel data.
 */
static enum image_error load_raw(imageinfo *s,
 const struct image_raw_format *format, const uint8_t *not_magic,
 size_t not_magic_length)
{
  struct image_file *dest = s->out;
  struct rgba_color *dest_pos;
  uint8_t *src_pos;
  uint8_t *data;
  uint64_t size = (uint64_t)format->width * format->height;
  uint64_t raw_size = size * format->bytes_per_pixel;
  uint64_t i;
  enum image_error ret = IMAGE_OK;
  char tmp;

  if(format->bytes_per_pixel < 1 || format->bytes_per_pixel > 4)
  {
    debug("unsupported raw bytes per pixel %zu\n", (size_t)format->bytes_per_pixel);
    return IMAGE_ERROR_RAW_UNSUPPORTED_BPP;
  }

  debug("checking if this is a raw file with properties: %zu %zu %zu\n",
   (size_t)format->width, (size_t)format->height, (size_t)format->bytes_per_pixel);

  data = (uint8_t *)malloc(raw_size);
  if(!data)
  {
    debug("can't allocate memory for raw image with given size\n");
    return IMAGE_ERROR_MEM;
  }

  if(not_magic_length)
    memcpy(data, not_magic, IMAGE_MIN(raw_size, not_magic_length));

  if(raw_size > not_magic_length)
  {
    raw_size -= not_magic_length;
    if(s->readfn(data + not_magic_length, raw_size, s->in) < raw_size)
    {
      debug("failed to read required data for raw image\n");
      ret = IMAGE_ERROR_IO;
      goto err_free;
    }
  }

  debug("Image type: raw\n");

  if(s->readfn(&tmp, 1, s->in) != 0)
    debug("data exists after expected EOF in raw file\n");

  ret = image_init(format->width, format->height, dest);
  if(ret)
    goto err_free;

  dest_pos = dest->data;
  src_pos = data;
  for(i = 0; i < size; i++)
  {
    uint8_t r, g, b, a;
    if(format->bytes_per_pixel < 3)
    {
      r = src_pos[0];
      g = src_pos[0];
      b = src_pos[0];
      a = (format->bytes_per_pixel == 2) ? src_pos[1] : 255;
    }
    else
    {
      r = src_pos[0];
      g = src_pos[1];
      b = src_pos[2];
      a = (format->bytes_per_pixel == 4) ? src_pos[3] : 255;
    }

    dest_pos->r = r;
    dest_pos->g = g;
    dest_pos->b = b;
    dest_pos->a = a;
    dest_pos++;
    src_pos += format->bytes_per_pixel;
  }

err_free:
  free(data);
  return ret;
}


/**
 * Common file handling.
 */

#define MAGIC(a,b) (((a) << 8) | (b))

/**
 * Load an image from a stream.
 * Assume this stream can NOT be seeked.
 */
enum image_error load_image_from_stream(void *fp, image_read_function readfn,
 struct image_file *dest, const struct image_raw_format *raw_format)
{
  imageinfo s = { dest, fp, readfn, -1 };
  uint8_t magic[CHECK_LENGTH];
  size_t sz = 0;
  enum image_error ret;

  memset(dest, 0, sizeof(struct image_file));
  memset(magic, 0, sizeof(magic));

  if(raw_format && raw_format->force_raw)
    goto force_raw;

  if(readfn(magic, 2, fp) < 2)
    return IMAGE_ERROR_SIGNATURE;

  switch(MAGIC(magic[0], magic[1]))
  {
    /* BMP */
    case MAGIC('B','M'): return load_bmp(&s);

    /* NetPBM */
    case MAGIC('P','1'): return load_portable_bitmap_plain(&s);
    case MAGIC('P','2'): return load_portable_greymap_plain(&s);
    case MAGIC('P','3'): return load_portable_pixmap_plain(&s);
    case MAGIC('P','4'): return load_portable_bitmap_binary(&s);
    case MAGIC('P','5'): return load_portable_greymap_binary(&s);
    case MAGIC('P','6'): return load_portable_pixmap_binary(&s);
    case MAGIC('P','7'): return load_portable_arbitrary_map(&s);
  }

  if(readfn(magic + 2, 1, fp) < 1)
    return IMAGE_ERROR_SIGNATURE;

  /* GIF */
  if(!memcmp(magic, "GIF", 3))
    return load_gif(&s);

  if(readfn(magic + 3, 5, fp) < 5)
    return IMAGE_ERROR_SIGNATURE;

  /* farbfeld */
  if(!memcmp(magic, "farbfeld", 8))
    return load_farbfeld(&s);

#ifdef CONFIG_PNG

  /* PNG (via libpng). */
  if(png_sig_cmp(magic, 0, 8) == 0)
    return load_png(&s);

#else

  if(!memcmp(magic, "\x89PNG\r\n\x1A\n", 8))
    debug("MegaZeux utils were compiled without PNG support--is this a PNG?\n");

#endif

  sz = 8 + readfn(magic + 8, 10, fp);
  if(sz == 18)
  {
    /* TGA has no signature. If the header looks wrong no bytes will be read. */
    ret = load_tga(&s, magic, 18);
    if(ret != IMAGE_ERROR_TGA_NOT_A_TGA)
      return ret;
  }

force_raw:
  if(raw_format)
  {
    enum image_error res = load_raw(&s, raw_format, magic, sz);
    if(res != IMAGE_ERROR_IO && res != IMAGE_ERROR_MEM)
      return res;
  }

  debug("unknown format\n");
  return IMAGE_ERROR_SIGNATURE;
}

static size_t stdio_read_fn(void *dest, size_t num, void *handle)
{
  return fread(dest, 1, num, (FILE *)handle);
}

/**
 * Load an image from a file.
 */
enum image_error load_image_from_file(const char *filename,
 struct image_file *dest, const struct image_raw_format *raw_format)
{
  FILE *fp;
  enum image_error ret;

  if(!strcmp(filename, "-"))
  {
#ifdef _WIN32
    /* Windows forces stdin to be text mode by default, fix it. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    return load_image_from_stream(stdin, stdio_read_fn, dest, raw_format);
  }

  fp = fopen(filename, "rb");
  if(!fp)
    return IMAGE_ERROR_IO;

  setvbuf(fp, NULL, _IOFBF, 32768);

  ret = load_image_from_stream(fp, stdio_read_fn, dest, raw_format);
  fclose(fp);

  return ret;
}
