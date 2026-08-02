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

#include "../Unit.hpp"

#include <vector>
#include <zlib.h>

#include "../../src/io/memfile.h"
#include "../../src/io/image_file.c"
#include "../../src/io/image_gif.c"
#include "../../src/io/image_png.c"

#define DATA_BASEDIR "unit/io/image_file/"

struct image_file_const
{
  unsigned int width;
  unsigned int height;
  const struct rgba_color *data;
};

/********** BW template. **********/

#define WHITE { 255, 255, 255, 255 }
#define BLACK {   0,   0,   0, 255 }

static const struct rgba_color raw_bw[] =
{
  // R G B A; 5x5
  BLACK, WHITE, WHITE, WHITE, BLACK,
  WHITE, WHITE, WHITE, WHITE, WHITE,
  WHITE, WHITE, BLACK, WHITE, WHITE,
  WHITE, WHITE, WHITE, WHITE, WHITE,
  BLACK, WHITE, WHITE, WHITE, BLACK,
};

static const struct image_file_const base_bw_img
{
  5, 5, raw_bw
};

/********** Greyscale template. **********/

static const struct rgba_color raw_gs[] =
{
  // R G B A; 4x7
  { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
  { 170, 170, 170, 255 }, { 170, 170, 170, 255 }, { 170, 170, 170, 255 }, { 170, 170, 170, 255 },
  {  85,  85,  85, 255 }, {  85,  85,  85, 255 }, {  85,  85,  85, 255 }, {  85,  85,  85, 255 },
  {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
  { 255, 255, 255, 170 }, { 255, 255, 255, 170 }, { 255, 255, 255, 170 }, { 255, 255, 255, 170 },
  { 255, 255, 255,  85 }, { 255, 255, 255,  85 }, { 255, 255, 255,  85 }, { 255, 255, 255,  85 },
  { 255, 255, 255,   0 }, { 255, 255, 255,   0 }, { 255, 255, 255,   0 }, { 255, 255, 255,   0 },
};

static const struct image_file_const base_gs_img =
{
  4, 7, raw_gs
};

/********** RGB(A) template. **********/

static const struct rgba_color raw_rgba[] =
{
  // R G B A; 8x6
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 },
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 },

  { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },
  { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },

  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 },
  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 },

  { 255, 255, 0, 170 }, { 255, 255, 0, 170 }, { 255, 255, 0, 170 }, { 255, 255, 0, 170 },
  { 255, 170, 0,  85 }, { 255, 170, 0,  85 }, { 255, 170, 0,  85 }, { 255, 170, 0,  85 },

  { 255, 0, 255, 170 }, { 255, 0, 255, 170 }, { 255, 0, 255, 170 }, { 255, 0, 255, 170 },
  { 170, 0, 255,  85 }, { 170, 0, 255,  85 }, { 170, 0, 255,  85 }, { 170, 0, 255,  85 },

  { 0, 255, 255, 170 }, { 0, 255, 255, 170 }, { 0, 255, 255, 170 }, { 0, 255, 255, 170 },
  { 0, 255, 170,  85 }, { 0, 255, 170,  85 }, { 0, 255, 170,  85 }, { 0, 255, 170,  85 },
};

static const struct image_file_const base_rgba_img
{
  8, 6, raw_rgba
};

/********** RGB(A) template for 2x scaling formats. **********/

static const struct rgba_color raw_rgba_2x[] =
{
  // R G B A; 4 x 4
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },
  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
  // Only present in 1x1_interlaced.gif
  { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255},
  { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255},
  { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255},
  { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255}, { 0, 0, 0, 255},
};

static const struct image_file_const base_scaling_img
{
  4, 4, raw_rgba_2x
};

static const struct image_file_const base_scaling_img_interlaced
{
  4, 8, raw_rgba_2x
};

/* Allow error in 16-bpp conversions since they can't accurately reproduce
 * the source data. +-3 seems good enough for the current inputs.
 */
template<unsigned ERROR>
static boolean compare_16bpp(int a, int b)
{
  a -= b;
  return a >= -(int)ERROR && a <= (int)ERROR;
}

template<int ERROR>
static boolean compare_rgb16(const rgba_color &base, const rgba_color &in)
{
  return  compare_16bpp<ERROR>(base.r, in.r) && compare_16bpp<ERROR>(base.g, in.g) &&
          compare_16bpp<ERROR>(base.b, in.b) && in.a == 255;
}

template<int ERROR>
static boolean compare_rgba16(const rgba_color &base, const rgba_color &in)
{
  return  compare_16bpp<ERROR>(base.r, in.r) && compare_16bpp<ERROR>(base.g, in.g) &&
          compare_16bpp<ERROR>(base.b, in.b) && compare_16bpp<ERROR>(base.a, in.a);
}

static boolean compare_rgb(const rgba_color &base, const rgba_color &in)
{
  return (base.r == in.r) && (base.g == in.g) && (base.b == in.b) && (in.a == 255);
}

static boolean compare_rgba(const rgba_color &base, const rgba_color &in)
{
  return (base.r == in.r) && (base.g == in.g) && (base.b == in.b) && (base.a == in.a);
}

template<boolean (*COMPARE_FN)(const rgba_color &a, const rgba_color &b)>
static void compare_image(const struct image_file_const &base,
 const struct image_file &in, const char *filename)
{
  ASSERTEQ(in.width, base.width, "%s: width mismatch", filename);
  ASSERTEQ(in.height, base.height, "%s: height mismatch", filename);

  size_t num = base.width * base.height;
  for(size_t i = 0; i < num; i++)
  {
    if(!COMPARE_FN(base.data[i], in.data[i]))
    {
      FAIL("%s: pixel mismatch @ %zu - %08" PRIx32 " %08" PRIx32, filename, i,
       *(uint32_t *)(base.data + i), *(uint32_t *)(in.data + i));
    }
  }
}

template<boolean (*COMPARE_FN)(const rgba_color &a, const rgba_color &b)>
static void load_and_compare_image(const struct image_file_const &base,
 const char *filename)
{
  struct image_file img{};
  char path[512];

  snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

  enum image_error ret = load_image_from_file(path, &img, NULL);
  ASSERT(ret == IMAGE_OK, "%s: load failed: %s", filename, image_error_string(ret));
  compare_image<COMPARE_FN>(base, img, filename);
  image_free(&img);
}


UNITTEST(PNG)
{
  /* struct rgba_color and struct png_rgba must be 100% interchangeable. */
  static_assert(sizeof(struct rgba_color) == sizeof(struct png_rgba), "size");
  static_assert(offsetof(struct rgba_color, r) == offsetof(struct png_rgba, r), "r offset");
  static_assert(offsetof(struct rgba_color, g) == offsetof(struct png_rgba, g), "g offset");
  static_assert(offsetof(struct rgba_color, b) == offsetof(struct png_rgba, b), "b offset");
  static_assert(offsetof(struct rgba_color, a) == offsetof(struct png_rgba, a), "a offset");

#ifndef IMAGE_FILE_LIBPNG
  SKIP();
#else

  /* TODO: all indexed greyscale */
  static constexpr const char *inputs_bw[] =
  {
    "rgb1.png", // Indexed RGB
  };
  static constexpr const char *inputs_gs[] =
  {
    "gs8.png",  // Greyscale (GIMP only saves greyscale non-indexed...)
    "rgb2.png", // Indexed RGB
  };
  static constexpr const char *inputs_gsa[] =
  {
    "gsa8.png", // Greyscale with alpha
  };
  static constexpr const char *inputs_rgb[] =
  {
    "rgb4.png", // Indexed RGB
    "rgb8.png", // Indexed RGB
    "rgb24.png",
    "rgb24_adam7.png", // With interlacing
    "rgb48.png",
  };
  static constexpr const char *inputs_rgba[] =
  {
    "rgba8.png",  // Indexed RGB with tRNS alpha
    "rgba.png",
    "rgba64.png",
  };

  SECTION(1bpp)
  {
    for(const char *filename : inputs_bw)
      load_and_compare_image<compare_rgb>(base_bw_img, filename);
  }

  SECTION(Greyscale)
  {
    for(const char *filename : inputs_gs)
      load_and_compare_image<compare_rgb>(base_gs_img, filename);
  }

  SECTION(GreyscaleAlpha)
  {
    for(const char *filename : inputs_gsa)
      load_and_compare_image<compare_rgba>(base_gs_img, filename);
  }

  SECTION(RGB)
  {
    for(const char *filename : inputs_rgb)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);
  }

  SECTION(RGBA)
  {
    for(const char *filename : inputs_rgba)
      load_and_compare_image<compare_rgba>(base_rgba_img, filename);
  }

#endif /* IMAGE_FILE_LIBPNG */
}


static size_t png_memfile_read_fn(void *dest, size_t count, void *handle)
{
  struct memfile *mf = reinterpret_cast<struct memfile *>(handle);
  return mfread(dest, 1, count, mf);
}

static size_t png_memfile_write_fn(const void *src, size_t count, void *handle)
{
  struct memfile *mf = reinterpret_cast<struct memfile *>(handle);
  return mfwrite(src, 1, count, mf);
}

static png_bool unit_png_read_alloc_fn(uint32_t w, uint32_t h,
 struct png_rgba **pixels, size_t *pixels_row_pitch, void *priv)
{
  struct image_file *img = reinterpret_cast<struct image_file *>(priv);
  if(w > 32767u * 8u || h > 32767u * 14u)
    return IMAGE_FALSE;

  img->data = (struct rgba_color *)calloc(w * h, sizeof(struct png_rgba));
  img->width = w;
  img->height = h;
  *pixels = (struct png_rgba *)img->data;
  *pixels_row_pitch = w * sizeof(struct png_rgba);
  return img->data ? IMAGE_TRUE : IMAGE_FALSE;
}

struct png_write_row_priv
{
  const struct png_rgba *pos;
  const struct png_rgba *end;
};

static const struct png_rgba *png_write_row_fn(uint32_t width, void *priv)
{
  struct png_write_row_priv *wp = reinterpret_cast<struct png_write_row_priv *>(priv);
  const struct png_rgba *pos = wp->pos;

  if(pos >= wp->end)
    return NULL;

  wp->pos += width;
  return pos;
}

/* There is no unified image writing interface, but image_png.c supports
 * writing 24-bit RGB and 32-bit RGBA images separately. */
UNITTEST(PNGWriter)
{
  struct memfile mf;
  struct png_write_row_priv wp;

  struct image_file img{};
#ifdef IMAGE_FILE_LIBPNG
  enum image_error iret;
#endif
  enum png_error ret;

  std::vector<uint8_t> writebuf(8192);
  mfopen_wr(writebuf.data(), writebuf.size(), &mf);

  wp.pos = reinterpret_cast<const struct png_rgba *>(base_rgba_img.data);
  wp.end = wp.pos + (base_rgba_img.width * base_rgba_img.height);

  SECTION(WriteRGB24)
  {
    ret = png_write(base_rgba_img.width, base_rgba_img.height, PNG_PIXEL_RGB24,
     &mf, png_memfile_write_fn, &wp, png_write_row_fn);
    ASSERTEQ(ret, PNG_OK, "");

    mfseek(&mf, 0, SEEK_SET);

    ret = png_read_fallback(&mf, png_memfile_read_fn,
     &img, unit_png_read_alloc_fn, false);
    ASSERTEQ(ret, PNG_OK, "");

    compare_image<compare_rgb>(base_rgba_img, img, "<mem>");
    image_free(&img);

#ifdef IMAGE_FILE_LIBPNG
    mfseek(&mf, 0, SEEK_SET);

    iret = load_image_from_stream(&mf, png_memfile_read_fn, &img, nullptr);
    ASSERTEQ(iret, IMAGE_OK, "");

    compare_image<compare_rgb>(base_rgba_img, img, "<mem>");
    image_free(&img);
#endif
  }

  SECTION(WriteRGBA32)
  {
    ret = png_write(base_rgba_img.width, base_rgba_img.height, PNG_PIXEL_RGBA32,
     &mf, png_memfile_write_fn, &wp, png_write_row_fn);
    ASSERTEQ(ret, PNG_OK, "");

    mfseek(&mf, 0, SEEK_SET);

    ret = png_read_fallback(&mf, png_memfile_read_fn,
     &img, unit_png_read_alloc_fn, false);
    ASSERTEQ(ret, PNG_OK, "");

    compare_image<compare_rgba>(base_rgba_img, img, "<mem>");
    image_free(&img);

#ifdef IMAGE_FILE_LIBPNG
    mfseek(&mf, 0, SEEK_SET);

    iret = load_image_from_stream(&mf, png_memfile_read_fn, &img, nullptr);
    ASSERTEQ(iret, IMAGE_OK, "");

    compare_image<compare_rgba>(base_rgba_img, img, "<mem>");
    image_free(&img);
#endif
  }

  SECTION(FallbackWriteRGB24)
  {
    ret = png_write_fallback(
     base_rgba_img.width, base_rgba_img.height, PNG_PIXEL_RGB24,
     &mf, png_memfile_write_fn, &wp, png_write_row_fn);
    ASSERTEQ(ret, PNG_OK, "");

    mfseek(&mf, 0, SEEK_SET);

    ret = png_read_fallback(&mf, png_memfile_read_fn,
     &img, unit_png_read_alloc_fn, false);
    ASSERTEQ(ret, PNG_OK, "");

    compare_image<compare_rgb>(base_rgba_img, img, "<mem>");
    image_free(&img);

#ifdef IMAGE_FILE_LIBPNG
    mfseek(&mf, 0, SEEK_SET);

    iret = load_image_from_stream(&mf, png_memfile_read_fn, &img, nullptr);
    ASSERTEQ(iret, IMAGE_OK, "");

    compare_image<compare_rgb>(base_rgba_img, img, "<mem>");
    image_free(&img);
#endif
  }

  SECTION(FallbackWriteRGBA32)
  {
    ret = png_write_fallback(
     base_rgba_img.width, base_rgba_img.height, PNG_PIXEL_RGBA32,
     &mf, png_memfile_write_fn, &wp, png_write_row_fn);
    ASSERTEQ(ret, PNG_OK, "");

    mfseek(&mf, 0, SEEK_SET);

    ret = png_read_fallback(&mf, png_memfile_read_fn,
     &img, unit_png_read_alloc_fn, false);
    ASSERTEQ(ret, PNG_OK, "");

    compare_image<compare_rgba>(base_rgba_img, img, "<mem>");
    image_free(&img);

#ifdef IMAGE_FILE_LIBPNG
    mfseek(&mf, 0, SEEK_SET);

    iret = load_image_from_stream(&mf, png_memfile_read_fn, &img, nullptr);
    ASSERTEQ(iret, IMAGE_OK, "");

    compare_image<compare_rgba>(base_rgba_img, img, "<mem>");
    image_free(&img);
#endif
  }
}


UNITTEST(GIF)
{
  static constexpr const char *simple_inputs[] =
  {
    "1x1.gif",
    "1x1_layer.gif",
    "1x1_topleft.gif",
    "1x2.gif",
    "1x2_layer.gif",
    "2x1.gif",
    "2x1_layer.gif"
  };

  static constexpr struct {
    const char *filename;
    uint32_t crc;
  } complex_inputs[] =
  {
    { "example.gif", 0x733D9484 },
    { "tesseract.gif", 0xC56E4F50 },
    { "truecolor.gif", 0xE3064823 },
  };

  /* struct rgba_color and struct gif_rgba must be 100% interchangeable. */
  static_assert(sizeof(struct rgba_color) == sizeof(struct gif_rgba), "size mismatch");
  static_assert(offsetof(struct rgba_color, r) == offsetof(struct gif_rgba, r), "r offset mismatch");
  static_assert(offsetof(struct rgba_color, g) == offsetof(struct gif_rgba, g), "g offset mismatch");
  static_assert(offsetof(struct rgba_color, b) == offsetof(struct gif_rgba, b), "b offset mismatch");
  static_assert(offsetof(struct rgba_color, a) == offsetof(struct gif_rgba, a), "a offset mismatch");

  /* Simple inputs to test basic loading, as well as the scaling and
   * layer functionality of the compositor. */
  SECTION(Simple)
  {
    for(const char *filename : simple_inputs)
      load_and_compare_image<compare_rgb>(base_scaling_img, filename);
  }

  /* GIMP refused to interlace a 4x4 GIF... */
  SECTION(Interlaced)
  {
    load_and_compare_image<compare_rgb>(base_scaling_img_interlaced, "1x1_interlaced.gif");
  }

  /* More complex testing of the LZW depacking algorithm. */
  SECTION(Complex)
  {
    struct image_file img{};
    char path[512];
    enum image_error ret;

    for(const auto &in : complex_inputs)
    {
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", in.filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERT(ret == IMAGE_OK, "%s: load failed: %s", in.filename, image_error_string(ret));
      uint32_t chk = crc32(0ul, reinterpret_cast<uint8_t *>(img.data), img.width * img.height * 4);
      ASSERTEQ(chk, in.crc, "crc32 mismatch");
      image_free(&img);
    }
  }
}


UNITTEST(BMP)
{
  static const char *bw_inputs[] =
  {
    "idx_1bpp.bmp",
  };

  static const char *gs_inputs[] =
  {
    "idx_2bpp.bmp",
  };

  static const char *rgb_indexed_inputs[] =
  {
    "idx_4bpp.bmp",
    "idx_8bpp.bmp",
  };

  static const char *rgb_rle_inputs[] =
  {
    "rle_4bpp.bmp",
    "rle_8bpp.bmp",
  };

  static const char *rgb_truecolor16_inputs[] =
  {
    "true_16bpp.bmp",
  };

  static const char *rgb_truecolor_inputs[] =
  {
    "true_24bpp.bmp",
    "true_32bpp.bmp",
  };

  /* These bitfields BMPs are 100% valid by the specification, but they
   * confuse software aside from some web browsers:
   *
   * - Vivaldi (and possibly other Chromium browsers) displays all of them.
   * - ImageMagick seems to be completely unaware of BITMAPV2INFOHEADER and
   *   BITMAPV3INFOHEADER despite having output the original BITMAPV5HEADER
   *   BMPs that these are based on.
   * - Firefox can't display any of them either.
   * - GIMP and misc. Windows applications can read the RGBA8888 and RGBA4444,
   *   but not the RGB565 or RGB101010.
   */
  static const char *rgb_bitfields16_inputs[] =
  {
    "bitfields565_16bpp.bmp"
  };

  static const char *rgba_bitfields16_inputs[] =
  {
    "bitfields4444_16bpp.bmp"
  };

  static const char *rgb_bitfields32_inputs[] =
  {
    "bitfields101010_32bpp.bmp"
  };

  static const char *rgba_bitfields32_inputs[] =
  {
    "bitfields8888_32bpp.bmp"
  };

  SECTION(TrueColor)
  {
    for(const char *filename : rgb_truecolor_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);

    // 16bpp is lossy, needs a special compare...
    for(const char *filename : rgb_truecolor16_inputs)
      load_and_compare_image<compare_rgb16<3>>(base_rgba_img, filename);
  }

  SECTION(Indexed)
  {
    for(const char *filename : rgb_indexed_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);

    for(const char *filename : bw_inputs)
      load_and_compare_image<compare_rgb>(base_bw_img, filename);

    for(const char *filename : gs_inputs)
      load_and_compare_image<compare_rgb>(base_gs_img, filename);
  }

  SECTION(RLE)
  {
    for(const char *filename : rgb_rle_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);
  }

  SECTION(Bitfields)
  {
    for(const char *filename : rgb_bitfields32_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);
    for(const char *filename : rgba_bitfields32_inputs)
      load_and_compare_image<compare_rgba>(base_rgba_img, filename);

    // 16bpp is lossy, needs a special compare...
    for(const char *filename : rgb_bitfields16_inputs)
      load_and_compare_image<compare_rgb16<3>>(base_rgba_img, filename);
    for(const char *filename : rgba_bitfields16_inputs)
      load_and_compare_image<compare_rgba16<3>>(base_rgba_img, filename);
  }
}


UNITTEST(TGA)
{
  static const char *bpp16_inputs[] =
  {
    "tga_15bpp.tga",
    "tga_16bpp.tga",
    "tga_16bpp_rle.tga",
    "tga_16bpp_ttb.tga",
    "tga_16bpp_ttb_rle.tga",
    "tga_idx8_15bpp.tga",
    "tga_idx8_16bpp.tga",
    "tga_idx8_16bpp_rle.tga",
    "tga_idx8_16bpp_ttb.tga",
    "tga_idx8_16bpp_ttb_rle.tga",
  };

  static const char *bpp24_inputs[] =
  {
    "tga_24bpp.tga",
    "tga_24bpp_rle.tga",
    "tga_24bpp_ttb.tga",
    "tga_24bpp_ttb_rle.tga",
    "tga_idx8_24bpp.tga",
    "tga_idx8_24bpp_rle.tga",
    "tga_idx8_24bpp_ttb.tga",
    "tga_idx8_24bpp_ttb_rle.tga",
  };

  static const char *bpp32_inputs[] =
  {
    "tga_32bpp.tga",
    "tga_32bpp_rle.tga",
    "tga_32bpp_ttb.tga",
    "tga_32bpp_ttb_rle.tga",
    "tga_32bpp_rtl.tga",      // Unlikely that anyone uses right-to-left TGAs,
    "tga_32bpp_rtl_ttb.tga",  // but support them anyway...
    "tga_idx8_32bpp.tga",
    "tga_idx8_32bpp_rle.tga",
    "tga_idx8_32bpp_ttb.tga",
    "tga_idx8_32bpp_ttb_rle.tga",
    // These are hexedited and follow the spec but nothing supports them :(
    "tga_idx16_32bpp.tga",
    "tga_idx16_32bpp_rle.tga",
    "tga_idx16_32bpp_ttb.tga",
    "tga_idx16_32bpp_ttb_rle.tga",
  };

  static const char *greyscale_inputs[] =
  {
    "tga_g.tga",
    "tga_g_rle.tga",
    "tga_g_ttb.tga",
    "tga_g_ttb_rle.tga",
  };

  SECTION(16bpp)
  {
    // 16bpp is lossy, needs a special compare...
    for(const char *filename : bpp16_inputs)
      load_and_compare_image<compare_rgb16<3>>(base_rgba_img, filename);
  }

  SECTION(24bpp)
  {
    for(const char *filename : bpp24_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);
  }

  SECTION(32bpp)
  {
    for(const char *filename : bpp32_inputs)
      load_and_compare_image<compare_rgba>(base_rgba_img, filename);
  }

  SECTION(Greyscale)
  {
    for(const char *filename : greyscale_inputs)
      load_and_compare_image<compare_rgb>(base_gs_img, filename);
  }
}


UNITTEST(Netpbm)
{
  static const char *bw_inputs[] =
  {
    /* PBM */
    "p1.pbm",
    "p4.pbm",
  };

  static const char *grey_inputs[] =
  {
    /* PGM */
    "p2.pgm",
    "p2_18.pgm",
    "p2_64.pgm",
    "p5.pgm",
    "p5_18.pgm",
    "p5_64.pgm",
    /* PAM GRAYSCALE */
    "p7_gs.pam",
    "p7_gs18.pam",
    "p7_gs64.pam",
  };

  static const char *greya_inputs[] =
  {
    /* PAM GRAYSCALE_ALPHA */
    "p7_gsa.pam",
    "p7_gsa18.pam",
    "p7_gsa64.pam",
  };

  static const char *rgb_inputs[] =
  {
    /* PPM */
    "p3.ppm",
    "p3_18.ppm",
    "p3_64.ppm",
    "p6.ppm",
    "p6_18.ppm",
    "p6_64.ppm",
    /* PAM RGB */
    "p7_rgb.pam",
    "p7_rgb18.pam",
    "p7_rgb64.pam",
  };

  static const char *rgba_inputs[] =
  {
    /* PAM RGB_ALPHA */
    "p7_rgba.pam",
    "p7_rgba18.pam",
    "p7_rgba64.pam",
  };

  SECTION(Bitmap)
  {
    for(const char *filename : bw_inputs)
      load_and_compare_image<compare_rgb>(base_bw_img, filename);
  }

  SECTION(Greymap)
  {
    for(const char *filename : grey_inputs)
      load_and_compare_image<compare_rgb>(base_gs_img, filename);
  }

  SECTION(Pixmap)
  {
    for(const char *filename : rgb_inputs)
      load_and_compare_image<compare_rgb>(base_rgba_img, filename);
  }

  SECTION(Alpha)
  {
    for(const char *filename : greya_inputs)
      load_and_compare_image<compare_rgba>(base_gs_img, filename);

    for(const char *filename : rgba_inputs)
      load_and_compare_image<compare_rgba>(base_rgba_img, filename);
  }
}


UNITTEST(farbfeld)
{
  load_and_compare_image<compare_rgba>(base_rgba_img, "farbfeld.ff");
}


UNITTEST(raw)
{
  struct image_file img{};
  enum image_error ret;

  static const struct image_raw_format gs_format = { base_gs_img.width, base_gs_img.height, 1, true};
  static const struct image_raw_format gsa_format = { base_gs_img.width, base_gs_img.height, 2, true };
  static const struct image_raw_format rgb_format = { base_rgba_img.width, base_rgba_img.height, 3, true };
  static const struct image_raw_format rgba_format = { base_rgba_img.width, base_rgba_img.height, 4, true };

  SECTION(Greyscale)
  {
    ret = load_image_from_file(DATA_BASEDIR "raw_gs.raw", &img, &gs_format);
    ASSERT(ret == IMAGE_OK, "raw_gs.raw: load failed: %s", image_error_string(ret));
    compare_image<compare_rgb>(base_gs_img, img, "raw_gs.raw");
    image_free(&img);
  }

  SECTION(GreyscaleAlpha)
  {
    ret = load_image_from_file(DATA_BASEDIR "raw_gsa.raw", &img, &gsa_format);
    ASSERT(ret == IMAGE_OK, "raw_gsa.raw: load failed: %s", image_error_string(ret));
    compare_image<compare_rgba>(base_gs_img, img, "raw_gsa.raw");
    image_free(&img);
  }

  SECTION(RGB)
  {
    ret = load_image_from_file(DATA_BASEDIR "raw_rgb.raw", &img, &rgb_format);
    ASSERT(ret == IMAGE_OK, "raw_rgb.raw: load failed: %s", image_error_string(ret));
    compare_image<compare_rgb>(base_rgba_img, img, "raw_rgb.raw");
    image_free(&img);
  }

  SECTION(RGBA)
  {
    ret = load_image_from_file(DATA_BASEDIR "raw_rgba.raw", &img, &rgba_format);
    ASSERT(ret == IMAGE_OK, "raw_rgba.raw: load failed: %s", image_error_string(ret));
    compare_image<compare_rgba>(base_rgba_img, img, "raw_rgba.raw");
    image_free(&img);
  }
}
