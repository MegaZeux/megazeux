/* MegaZeux
 *
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

#include "Unit.hpp"
#include "UnitIO.hpp"

#include "../src/io/path.h"

extern "C" {
#include "../src/render/render.c"
}

#include "../src/platform/platform_endian.c"

#define BUILD_REFERENCE_RENDERER
#include "../src/render/render_layer.cpp"
#include "../src/render_layer.cpp"
#include "../src/render_layer_avx.cpp"
#include "../src/render_layer_neon.cpp"
#include "../src/render_layer_rvv.cpp"
#include "../src/render_layer_sse2.cpp"

typedef void (*render_layer_fp)(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer);

//#define GENERATE_FILES

#define DIR "unit" DIR_SEPARATOR "render" DIR_SEPARATOR

enum smzx_type
{
  MZX = 0,
  SMZX = 1
};

enum flat_bpp
{
  FLAT16,
  FLAT32,
  FLAT_YUY2,
  FLAT_UYVY,
  FLAT_YVYU
};

static const uint8_t palette[][3] =
{
  {  0,  0,  0 },
  {  0,  0, 42 },
  {  0, 42,  0 },
  {  0, 42, 42 },
  { 42,  0,  0 },
  { 42,  0, 42 },
  { 42, 21,  0 },
  { 42, 42, 42 },
  { 21, 21, 21 },
  { 21, 21, 63 },
  { 21, 63, 21 },
  { 21, 63, 63 },
  { 63, 21, 21 },
  { 63, 21, 63 },
  { 63, 63, 21 },
  { 63, 63, 63 }
};

static size_t calc_offset(size_t frame_w, size_t frame_h, size_t full_w, size_t full_h)
{
  return (full_h - frame_h) * full_w / 2 + (full_w - frame_w) / 2;
}

template<typename T, int MISALIGN = 0, int GENERATE = 1>
class render_frame
{
  size_t frame_w;
  size_t frame_h;
  size_t full_width;
  size_t full_height;
  size_t start_offset;
  std::vector<T> buf;

public:
  static constexpr T fill = static_cast<T>(0xbabababababababa);

  render_frame(size_t w, size_t h):
   frame_w(w), frame_h(h), full_width(w + 16), full_height(h + 16),
   buf(full_width * full_height, T(fill))
  {
    start_offset = calc_offset(frame_w, frame_h, full_width, full_height);
  }

  T *pixels()
  {
    return buf.data() + start_offset;
  }

  const T *pixels() const
  {
    return buf.data() + start_offset;
  }

  size_t pitch()
  {
    return sizeof(T) * (full_width - MISALIGN);
  }

  size_t bpp()
  {
    return 8 * sizeof(T);
  }

  void check_out_of_frame() const
  {
    size_t skip = full_width - frame_w;
    size_t i;
    size_t j;
    size_t y;

    // misalign rows if requested by reducing the skip size.
    skip -= MISALIGN;

    for(i = 0; i < start_offset; i++)
      ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);

    for(y = 0; y < frame_h; y++)
    {
      i += frame_w;
      for(j = 0; j < skip; j++, i++)
        ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);
    }
    size_t full_size = buf.size();
    for(; i < full_size; i++)
      ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);
  }

  void check_in_frame(const graphics_data &graphics, const char *file) const
  {
    size_t frame_w_bytes = frame_w * sizeof(T);
    size_t y;
    const T *buf_pos;

#ifdef GENERATE_FILES
    if(GENERATE)
    {
      std::vector<T> out(frame_w * frame_h);
      T *pix_pos = out.data();
      buf_pos = pixels();
      for(y = 0; y < frame_h; y++)
      {
        memcpy(pix_pos, buf_pos, frame_w_bytes);
        buf_pos += full_width - MISALIGN;
        pix_pos += frame_w;
      }
      unit::io::save_tga(out, frame_w, frame_h,
       graphics.flat_intensity_palette, graphics.screen_mode ? 256 : 32, file);
    }
#endif

    std::vector<T> expected = unit::io::load_tga<T>(file);
    const T *exp_pos = expected.data();
    buf_pos = buf.data() + start_offset;

    for(y = 0; y < frame_h; y++)
    {
      ASSERTMEM(buf_pos, exp_pos, frame_w_bytes,
       "frame mismatch on line %zu: %s", y, file);
      buf_pos += full_width - MISALIGN;
      exp_pos += frame_w;
    }

#ifdef GENERATE_FILES
    FAIL("generation code enabled");
#endif
  }

  void check(const graphics_data &graphics, const char *file) const
  {
    check_in_frame(graphics, file);
    check_out_of_frame();
  }
};

static inline unsigned comp6to5(unsigned c)
{
  return c >> 1;
}

static inline unsigned comp6to8(unsigned c)
{
  return ((c * 255u) + 32u) / 63u;
}

template<flat_bpp FLAT>
static inline uint32_t flat_color(unsigned r, unsigned g, unsigned b);

// TGA 16bpp
template<>
inline uint32_t flat_color<FLAT16>(unsigned r, unsigned g, unsigned b)
{
  r = comp6to5(r);
  g = comp6to5(g);
  b = comp6to5(b);

  uint32_t val = 0x8000 | (r << 10u) | (g << 5u) | b;
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  val = ((val & 0xff) << 8) | (val >> 8);
#endif
  return val;
}

// TGA 32bpp
template<>
inline uint32_t flat_color<FLAT32>(unsigned r, unsigned g, unsigned b)
{
  r = comp6to8(r);
  g = comp6to8(g);
  b = comp6to8(b);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  return (b << 24u) | (g << 16u) | (r << 8u) | 0xffu;
#else
  return 0xff000000u | (r << 16u) | (g << 8u) | b;
#endif
}

template<>
inline uint32_t flat_color<FLAT_YUY2>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_yuy2(comp6to8(r), comp6to8(g), comp6to8(b));
}

template<>
inline uint32_t flat_color<FLAT_UYVY>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_uyvy(comp6to8(r), comp6to8(g), comp6to8(b));
}

template<>
inline uint32_t flat_color<FLAT_YVYU>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_yvyu(comp6to8(r), comp6to8(g), comp6to8(b));
}

/* Preset enough of the graphics data for these tests to work.
 * Currently this requires:
 *
 *   charset (all renderers)
 *   flat_intensity_palette (16/32-bit renderers)
 *   smzx_indices (SMZX renderers)
 *   protected_pal_position (MZX renderers)
 *
 * Additionally, the render_graph* functions require layer data
 * in text_video and render_layer requires an initialized
 * video_layer struct (it does not need to be in the graphics data).
 *
 * The cursor and mouse render functions don't use the graphics data.
 */
static void init_graphics_base(struct graphics_data &graphics)
{
  static std::vector<uint8_t> default_chr;
  static std::vector<uint8_t> edit_chr;

  if(!default_chr.size())
    default_chr = unit::io::load("assets/default.chr");
  if(!edit_chr.size())
    edit_chr = unit::io::load("assets/edit.chr");

  memcpy(graphics.charset, default_chr.data(), CHARSET_SIZE * CHAR_H);
  memcpy(graphics.charset + PRO_CH * CHAR_H, edit_chr.data(), CHARSET_SIZE * CHAR_H);
}

template<flat_bpp FLAT>
static void init_graphics_mzx(struct graphics_data &graphics)
{
  graphics.screen_mode = 0;
  graphics.protected_pal_position = 16;
  for(size_t i = 0; i < 16; i++)
  {
    uint32_t flat = flat_color<FLAT>(palette[i][0], palette[i][1], palette[i][2]);
    graphics.flat_intensity_palette[i] = flat;
    graphics.flat_intensity_palette[i + 16] = flat;
    graphics.flat_intensity_palette[i + 256] = flat;
  }
}

template<flat_bpp FLAT>
static void init_graphics_smzx(struct graphics_data &graphics)
{
  init_graphics_mzx<FLAT>(graphics);
  graphics.screen_mode = 3;
  graphics.protected_pal_position = 256;

  for(size_t i = 0; i < 256; i++)
  {
    size_t bg = i >> 4;
    size_t fg = i & 0x0f;
    size_t r = fg << 2;
    size_t b = bg << 2;
    graphics.flat_intensity_palette[i] = flat_color<FLAT>(r, b, b);

    graphics.smzx_indices[i * 4 + 0] = (bg << 4) | bg;
    graphics.smzx_indices[i * 4 + 1] = (bg << 4) | fg;
    graphics.smzx_indices[i * 4 + 2] = (fg << 4) | bg;
    graphics.smzx_indices[i * 4 + 3] = (fg << 4) | fg;
  }
}

#define out(ch,bg,fg) do { \
  pos->char_value = (ch) + offset; \
  pos->bg_color = (bg); \
  pos->fg_color = (fg); \
  pos++; \
} while(0)

static void init_layer_data(struct char_element *dest, int w, int h,
 int offset, int c_offset, int col)
{
  struct char_element *pos;
  int x, y, ch;
  int bg = (col >> 4) + c_offset;
  int fg = (col & 0x0f) + c_offset;

  // Border in given color containing all characters:
  pos = dest;
  ch = 0;
  for(y = 0; y < h; y++)
  {
    if(y < 2 || h - y <= 2)
    {
      for(x = 0; x < w; x++, ch++)
        out(ch & 255, bg, fg);
    }
    else
    {
      for(x = 0; x < 4; x++, ch++)
        out(ch & 255, bg, fg);
      pos += w - 8;
      for(x = w - 4; x < w; x++, ch++)
        out(ch & 255, bg, fg);
    }
  }

  // Centered palette display.
  if(w >= 64 && h >= 22)
  {
    pos = dest;
    pos += w * ((h - 16) / 2);
    pos += (w - 32) / 2;

    for(y = 0; y < 16; y++)
    {
      for(x = 0; x < 16; x++)
      {
        out(176, y, x);
        out(176, y, x);
      }
      pos += w - 32;
    }
  }
}

static void init_layer(struct video_layer &layer, int w, int h,
 int mode, struct char_element *data)
{
  layer.w = w;
  layer.h = h;
  layer.transparent_col = -1;
  layer.mode = mode;
  layer.data = data;
}

template<smzx_type MODE, flat_bpp FLAT>
struct render_graph_init
{
  render_graph_init(struct graphics_data &graphics)
  {
    init_graphics_base(graphics);
    if(MODE == SMZX)
      init_graphics_smzx<FLAT>(graphics);
    else
      init_graphics_mzx<FLAT>(graphics);
  }
};

template<smzx_type MODE, flat_bpp FLAT>
struct render_layer_init : public render_graph_init<MODE, FLAT>
{
  static constexpr int w = 3;
  static constexpr int h = 2;

  struct video_layer layer_screen{};
  struct video_layer layer_small{};
  struct video_layer layer_ui{};
  // NOTE: GCC 4.8.5 will ICE attempting to value-initialize these
  // arrays. Do not move the constructor value-initializers here.
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58704
  struct char_element big_data[132 * 43];
  struct char_element small_data[w * h];
  struct char_element ui_data[w * h];

  render_layer_init(struct graphics_data &graphics):
   render_graph_init<MODE, FLAT>(graphics), big_data{}, small_data{}, ui_data{}
  {
    init_layer(layer_screen, SCREEN_W, SCREEN_H,
     graphics.screen_mode, big_data);
    init_layer(layer_small, w, h, graphics.screen_mode, small_data);
    init_layer(layer_ui, w, h, 0, ui_data);

    init_layer_data(small_data, w, h, 0, 0, 0x1f);
    init_layer_data(ui_data, w, h, PRO_CH, 16, 0x2f);
  }
};


/* pix=8-bit align=32-bit */
UNITTEST(render_graph8)
{
  struct graphics_data graphics{};
  render_frame<uint8_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint8_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph8(pixels, pitch, &graphics, set_colors8_mzx);
    frame.check(graphics, DIR "8.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph8(pixels, pitch, &graphics, set_colors8_mzx);
    frame.check(graphics, DIR "8p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph8(pixels, pitch, &graphics, set_colors8_smzx);
    frame.check(graphics, DIR "8s.tga.gz");
  }
}

/* pix=16-bit align=32-bit */
UNITTEST(render_graph16)
{
  struct graphics_data graphics{};
  render_frame<uint16_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint16_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, set_colors16_mzx);
    frame.check(graphics, DIR "16.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph16(pixels, pitch, &graphics, set_colors16_mzx);
    frame.check(graphics, DIR "16p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, set_colors16_smzx);
    frame.check(graphics, DIR "16s.tga.gz");
  }
}

UNITTEST(render_graph16_yuv)
{
  struct graphics_data graphics{};
  render_frame<uint32_t> frame(SCREEN_PIX_W / 2, SCREEN_PIX_H);

  uint16_t *pixels = reinterpret_cast<uint16_t *>(frame.pixels());
  size_t pitch = frame.pitch();

  SECTION(YUY2)
  {
    render_graph_init<MZX, FLAT_YUY2> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, yuy2_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16yuy2.tga.gz");
  }

  SECTION(UYVY)
  {
    render_graph_init<MZX, FLAT_UYVY> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, uyvy_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16uyvy.tga.gz");
  }

  SECTION(YVYU)
  {
    render_graph_init<MZX, FLAT_YVYU> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, yvyu_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16yvyu.tga.gz");
  }
}

/* pix=32-bit align=32-bit */
UNITTEST(render_graph32)
{
  struct graphics_data graphics{};
  render_frame<uint32_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint32_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph32(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph32(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph32s(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32s.tga.gz");
  }
}


// Test that render_cursor overwrites the fill color completely with the
// cursor color at  the specified location and char lines.
template<typename T,
 unsigned width_ch = 4, unsigned height_ch = 3, uint32_t fill = 0xbabababau>
static void test_render_cursor(unsigned x_ch, unsigned y_ch, uint32_t flatcolor,
 uint8_t lines, uint8_t offset)
{
  static constexpr unsigned width_px = width_ch * CHAR_W;
  static constexpr unsigned height_px = height_ch * CHAR_H;
  static constexpr unsigned buf_sz = width_px * height_px * sizeof(T) / sizeof(uint32_t);
  static constexpr unsigned pitch = width_px * sizeof(T);
  static constexpr unsigned bpp = 8 * sizeof(T);

  T fill_px = static_cast<T>(fill);
  T flat_px = static_cast<T>(flatcolor);

  ASSERT(x_ch < width_ch, "bad x");
  ASSERT(y_ch < height_ch, "bad y");

  uint32_t buffer[buf_sz];
  for(size_t i = 0; i < buf_sz; i++)
    buffer[i] = fill;

  render_cursor(buffer, pitch, bpp, x_ch, y_ch, flatcolor, lines, offset);

  T *pos = reinterpret_cast<T *>(buffer);
  for(unsigned y = 0; y < height_px; y++)
  {
    for(unsigned x = 0; x < width_px; x++, pos++)
    {
      if(x >= x_ch * CHAR_W && x < (x_ch + 1) * CHAR_W &&
       y >= y_ch * CHAR_H + offset && y < y_ch * CHAR_H + offset + lines)
      {
        ASSERTEQ(*pos, flat_px, "%u,%u", x, y);
      }
      else
        ASSERTEQ(*pos, fill_px, "%u,%u", x, y);
    }
  }
}

UNITTEST(render_cursor)
{
  SECTION(8bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint8_t>(x, y, 0x0f0f0f0f, 2, 12);
        test_render_cursor<uint8_t>(x, y, 0x0f0f0f0f, 14, 0);
      }
    }
  }

  SECTION(16bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint16_t>(x, y, 0xabcfabcf, 2, 12);
        test_render_cursor<uint16_t>(x, y, 0xabcfabcf, 14, 0);
      }
    }
  }

  SECTION(32bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint32_t>(x, y, 0x12345678, 2, 12);
        test_render_cursor<uint32_t>(x, y, 0x12345678, 14, 0);
      }
    }
  }
}


// Test that render_mouse inverts the data at the given (pixel) rectangle
// according to the invert mask and sets pixels according to the alpha mask.
template<typename T,
 unsigned width_ch = 4, unsigned height_ch = 3>
static void test_render_mouse(unsigned x_px, unsigned y_px,
 uint32_t mask, uint32_t amask, uint8_t width, uint8_t height)
{
  static constexpr unsigned width_px = width_ch * CHAR_W;
  static constexpr unsigned height_px = height_ch * CHAR_H;
  static constexpr unsigned buf_sz = width_px * height_px * sizeof(T) / sizeof(uint32_t);
  static constexpr unsigned pitch = width_px * sizeof(T);
  static constexpr unsigned bpp = 8 * sizeof(T);

  T mask_px = static_cast<T>(mask);
  T amask_px = static_cast<T>(amask);

  ASSERT(x_px < width_px, "bad x");
  ASSERT(y_px < height_px, "bad y");

  uint32_t buffer[buf_sz];
  T *pos = reinterpret_cast<T *>(buffer);
  size_t i;
  for(i = 0; i < width_px * height_px; i++)
    *(pos++) = (i & 0xff) * 0x01010101;

  render_mouse(buffer, pitch, bpp, x_px, y_px, mask, amask, width, height);

  pos = reinterpret_cast<T *>(buffer);
  i = 0;
  for(unsigned y = 0; y < height_px; y++)
  {
    for(unsigned x = 0; x < width_px; x++, pos++, i++)
    {
      T expected = static_cast<T>((i & 0xff) * 0x01010101);
      if(x >= x_px && x < x_px + width && y >= y_px && y < y_px + height)
        expected = amask_px | (expected ^ mask_px);

      ASSERTEQ(*pos, expected, "%u,%u", x, y);
    }
  }
}

UNITTEST(render_mouse)
{
  SECTION(8bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0xffffffff, 0, 8, 14);
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0xffffffff, 0, 8, 7);
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H + 7, 0xffffffff, 0, 8, 7);

        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0x3f3f3f3f, 0xc0c0c0c0, 8, 14);
      }
    }
  }

  SECTION(16bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H, 0x7fff7fff, 0x80008000, 8, 14);
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H, 0x7fff7fff, 0x80008000, 8, 7);
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H + 7, 0x7fff7fff, 0x80008000, 8, 7);
      }
    }
  }

  SECTION(32bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H, 0x00ffffff, 0xff000000, 8, 14);
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H, 0x00ffffff, 0xff000000, 8, 7);
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H + 7, 0x00ffffff, 0xff000000, 8, 7);
      }
    }
  }
}


static constexpr int SM_W = 32;
static constexpr int SM_H = 20;
static constexpr int XL_W = 132;
static constexpr int XL_H = 43;

template<typename T, smzx_type MODE, flat_bpp FLAT, int ALLOW_GENERATE = 1,
 render_layer_fp my_render_layer = render_layer>
class render_layer_tester
{
public:
/* Test rendering text_video data.
 * Output should be indistinguishable from render_graph with the same input. */
static void graphic(const char *path)
{
  struct graphics_data graphics{};
  render_frame<T, 0, 0> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  T *pixels = frame.pixels();
  size_t pitch = frame.pitch();
  size_t bpp = frame.bpp();

  render_layer_init<MODE, FLAT> d(graphics);
  init_layer_data(d.big_data, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

  my_render_layer(pixels, SCREEN_PIX_W, SCREEN_PIX_H,
   pitch, bpp, &graphics, &d.layer_screen);

  frame.check(graphics, path);
}

/* Test rendering layers at various alignments with or without clipping.
 * If TR=1, the layers will use transparent colors. */
template<int TR, int CLIP, int MISALIGN>
static void layer_common(unsigned screen_w, unsigned screen_h,
 unsigned fill_color, const char *path)
{
  constexpr int GENERATE = !MISALIGN && ALLOW_GENERATE;

  struct graphics_data graphics{};
  size_t i;
  int width_px = screen_w * CHAR_W;
  int height_px = screen_h * CHAR_H;

  render_frame<T, MISALIGN, GENERATE> frame(width_px, height_px);

  T *pixels = frame.pixels();
  size_t pitch = frame.pitch();
  size_t bpp = frame.bpp();

  render_layer_init<MODE, FLAT> d(graphics);

  // Fill black
  d.layer_screen.w = screen_w;
  d.layer_screen.h = screen_h;
  init_layer_data(d.big_data, screen_w, screen_h, 0, 0, fill_color);
  my_render_layer(pixels, width_px, height_px,
   pitch, bpp, &graphics, &d.layer_screen);

  for(i = 0; i < 8; i++)
  {
    // no-clip: gradually increase misalignment for each of the 8 renders.
    int x1 = (i * 25) + CHAR_W * 3;
    int x2 = x1;
    int x3 = x1;
    int x4 = x1;
    int y1 = (i * 1) + CHAR_H * 2;
    int y2 = y1 + CHAR_H * 4;
    int y3 = y1 + CHAR_H * 8;
    int y4 = y1 + CHAR_H * 12;

    // clip: same, except relocate each set to one of the four edges.
    if(CLIP)
    {
      // top edge
      y1 = (i * 1) - CHAR_H;
      // left edge
      x2 = (i * 1) - CHAR_W;
      y2 = (i * 29) + CHAR_H;
      // bottom edge
      y3 = (i * 1) + (screen_h - 1) * CHAR_H - 7;
      // right edge
      x4 = (i * 1) + (screen_w - 2) * CHAR_W;
      y4 = y2;
    }

    d.layer_small.x = x1;
    d.layer_small.y = y1;
    d.layer_small.transparent_col = TR ? (MODE ? 0x11 : 1) : -1;
    my_render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_small);

    d.layer_small.x = x2;
    d.layer_small.y = y2;
    d.layer_small.transparent_col = TR ? (MODE ? 0xff : 15) : -1;
    my_render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_small);

    if(!CLIP)
    {
      // Test UI rendering.
      d.layer_ui.x = x3;
      d.layer_ui.y = y3;
      d.layer_ui.transparent_col = TR ? graphics.protected_pal_position + 2 : -1;
      my_render_layer(pixels, width_px, height_px,
       pitch, bpp, &graphics, &d.layer_ui);

      d.layer_ui.x = x4;
      d.layer_ui.y = y4;
      d.layer_ui.transparent_col = TR ? graphics.protected_pal_position + 15 : -1;
      my_render_layer(pixels, width_px, height_px,
       pitch, bpp, &graphics, &d.layer_ui);
    }
    else
    {
      // Test normal clipping at bottom and right edges.
      d.layer_small.x = x3;
      d.layer_small.y = y3;
      d.layer_small.transparent_col = TR ? (MODE ? 0x11 : 1) : -1;
      my_render_layer(pixels, width_px, height_px,
       pitch, bpp, &graphics, &d.layer_small);

      d.layer_small.x = x4;
      d.layer_small.y = y4;
      d.layer_small.transparent_col = TR ? (MODE ? 0xff : 15) : -1;
      my_render_layer(pixels, width_px, height_px,
       pitch, bpp, &graphics, &d.layer_small);
    }
  }

  if(CLIP)
  {
    // Corner clipping test.
    d.layer_small.transparent_col = TR ? (MODE ? 0x11 : 1) : -1;
    for(i = 0; i < 4; i++)
    {
      constexpr int show_x = CHAR_W * 2 - 1;
      constexpr int show_y = CHAR_H * 1 - 7;
      d.layer_small.x = (i & 1) ? width_px - show_x : show_x - CHAR_W * 3;
      d.layer_small.y = (i & 2) ? height_px - show_y : show_y - CHAR_H * 2;
      my_render_layer(pixels, width_px, height_px,
       pitch, bpp, &graphics, &d.layer_small);
    }
  }

  frame.check(graphics, path);
}

/* Test rendering layers at various alignments without clipping.
 * If TR=1, the layers will use transparent colors. */
static void align(const char *path)
{
  layer_common<0, 0, 0>(SM_W, SM_H, 0, path);
}
static void align_tr(const char *path)
{
  layer_common<1, 0, 0>(SM_W, SM_H, 0, path);
}
/* Same, but with clipping. */
static void clip(const char *path)
{
  layer_common<0, 1, 0>(SM_W, SM_H, 0, path);
}
static void clip_tr(const char *path)
{
  layer_common<1, 1, 0>(SM_W, SM_H, 0, path);
}

/* Test rendering layers at various alignments without clipping.
 * The buffer will be misaligned by one pixel to test that
 * the layer renderer selects an appropriately misaligned (1PPW) renderer.
 * If TR=1, the layers will use transparent colors. */
static void misalign(const char *path)
{
  layer_common<0, 0, 1>(SM_W, SM_H, 0, path);
}
static void misalign_tr(const char *path)
{
  layer_common<1, 0, 1>(SM_W, SM_H, 0, path);
}
/* Same, but with clipping. */
static void misclip(const char *path)
{
  layer_common<0, 1, 1>(SM_W, SM_H, 0, path);
}
static void misclip_tr(const char *path)
{
  layer_common<1, 1, 1>(SM_W, SM_H, 0, path);
}

/* Test rendering layers to a large destination at various alignments
 * with clipping. If TR=1, the layers will use transparent colors. */
static void large(const char *path)
{
  layer_common<0, 1, 0>(XL_W, XL_H, 0x5f, path);
}
};

/* These tests use the default render_layer dispatch path, and may select
 * any valid renderer as long as it renders correctly and doesn't crash.
 * Execution here is likely redundant with one of the tests below that
 * explicitly choose aligned/unaligned/vector rendering; this just makes
 * sure the actual dispatch that MegaZeux will execute.
 */
UNITTEST(render_layer_mzx8)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, MZX, FLAT32>;
  SECTION(graphic)      test::graphic(DIR "8.tga.gz");
  SECTION(align)        test::align(DIR "8a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8ta.tga.gz");
  SECTION(clip)         test::clip(DIR "8c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8tc.tga.gz");
  SECTION(large)        test::large(DIR "8xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx8)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, SMZX, FLAT32>;
  SECTION(graphic)      test::graphic(DIR "8s.tga.gz");
  SECTION(align)        test::align(DIR "8sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8sta.tga.gz");
  SECTION(clip)         test::clip(DIR "8sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8stc.tga.gz");
  SECTION(large)        test::large(DIR "8sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx16)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, MZX, FLAT16>;
  SECTION(graphic)      test::graphic(DIR "16.tga.gz");
  SECTION(align)        test::align(DIR "16a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16ta.tga.gz");
  SECTION(clip)         test::clip(DIR "16c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16tc.tga.gz");
  SECTION(large)        test::large(DIR "16xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx16)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, SMZX, FLAT16>;
  SECTION(graphic)      test::graphic(DIR "16s.tga.gz");
  SECTION(align)        test::align(DIR "16sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16sta.tga.gz");
  SECTION(clip)         test::clip(DIR "16sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16stc.tga.gz");
  SECTION(large)        test::large(DIR "16sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx32)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(render_layer_smzx32)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}

/* Manual layer renderer dispatch to ensure every valid alignment combination
 * is instantiated. Calls to render_layer may instead dispatch to unaligned
 * or vector renderers--making the lower-alignment renderers impossible to
 * reach--so the regular tests aren't sufficient for coverage.
 */
static void render_layer_aligned(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  int smzx = 0;
  int trans = 0;
  int align = 0;
  int clip = 0;
  select_aligned_renderer(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);

  render_layer_func(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);
}

UNITTEST(render_layer_mzx8_aligned)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, MZX, FLAT32, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "8.tga.gz");
  SECTION(align)        test::align(DIR "8a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8ta.tga.gz");
  SECTION(clip)         test::clip(DIR "8c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8tc.tga.gz");
  SECTION(large)        test::large(DIR "8xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx8_aligned)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, SMZX, FLAT32, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "8s.tga.gz");
  SECTION(align)        test::align(DIR "8sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8sta.tga.gz");
  SECTION(clip)         test::clip(DIR "8sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8stc.tga.gz");
  SECTION(large)        test::large(DIR "8sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx16_aligned)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, MZX, FLAT16, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "16.tga.gz");
  SECTION(align)        test::align(DIR "16a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16ta.tga.gz");
  SECTION(clip)         test::clip(DIR "16c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16tc.tga.gz");
  SECTION(large)        test::large(DIR "16xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx16_aligned)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, SMZX, FLAT16, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "16s.tga.gz");
  SECTION(align)        test::align(DIR "16sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16sta.tga.gz");
  SECTION(clip)         test::clip(DIR "16sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16stc.tga.gz");
  SECTION(large)        test::large(DIR "16sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx32_aligned)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(render_layer_smzx32_aligned)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32, 0, render_layer_aligned>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}

#if defined(PLATFORM_UNALIGN_32) || defined(PLATFORM_UNALIGN_64)
/* Manual layer renderer dispatch to ensure every unaligned renderer is
 * used for every alignment combination. Calls to render_layer may instead
 * dispatch to vector renderers, so the regular tests aren't sufficient for
 * coverage.
 */
static void render_layer_unaligned(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  int smzx = 0;
  int trans = 0;
  int align = 0;
  int clip = 0;
  select_aligned_renderer(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);

  select_unaligned_renderer(align);

  render_layer_func(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);
}

UNITTEST(render_layer_mzx8_unaligned)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, MZX, FLAT32, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "8.tga.gz");
  SECTION(align)        test::align(DIR "8a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8ta.tga.gz");
  SECTION(clip)         test::clip(DIR "8c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8tc.tga.gz");
  SECTION(large)        test::large(DIR "8xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx8_unaligned)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  using test = render_layer_tester<uint8_t, SMZX, FLAT32, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "8s.tga.gz");
  SECTION(align)        test::align(DIR "8sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "8sta.tga.gz");
  SECTION(clip)         test::clip(DIR "8sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "8stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "8sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "8sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "8sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "8stc.tga.gz");
  SECTION(large)        test::large(DIR "8sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx16_unaligned)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, MZX, FLAT16, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "16.tga.gz");
  SECTION(align)        test::align(DIR "16a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16ta.tga.gz");
  SECTION(clip)         test::clip(DIR "16c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16tc.tga.gz");
  SECTION(large)        test::large(DIR "16xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx16_unaligned)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  using test = render_layer_tester<uint16_t, SMZX, FLAT16, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "16s.tga.gz");
  SECTION(align)        test::align(DIR "16sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "16sta.tga.gz");
  SECTION(clip)         test::clip(DIR "16sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "16stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "16sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "16sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "16sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "16stc.tga.gz");
  SECTION(large)        test::large(DIR "16sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx32_unaligned)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(render_layer_smzx32_unaligned)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32, 0, render_layer_unaligned>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}
#endif /* PLATFORM_UNALIGN_32 || PLATFORM_UNALIGN_64 */

static void reference_renderer_wrap(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  ASSERTEQ(bpp, 32, "reference renderer is 32-bit only");
  reference_renderer(reinterpret_cast<uint32_t * RESTRICT>(pixels),
   width_px, height_px, pitch, graphics, layer);
}

UNITTEST(reference_renderer_mzx32)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32, 0, reference_renderer_wrap>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(reference_renderer_smzx32)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32, 0, reference_renderer_wrap>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}

static void render_layer32x4_wrap(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  ASSERTEQ(bpp, 32, "vector 32x4 renderer is 32-bit only");
#ifdef HAS_RENDER_LAYER32X4
  int smzx = layer->mode;
  int trans = layer->transparent_col != -1;
  int clip =
   (layer->x < 0 || layer->y < 0 ||
    (layer->x + layer->w * CHAR_W) > width_px ||
    (layer->y + layer->h * CHAR_H) > height_px);

  if(!render_layer32x4(pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip))
#endif
  SKIP();
}

UNITTEST(render_layer32x4_mzx)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32, 0, render_layer32x4_wrap>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(render_layer32x4_smzx)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32, 0, render_layer32x4_wrap>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}

static void render_layer32x8_wrap(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
  ASSERTEQ(bpp, 32, "vector 32x8 renderer is 32-bit only");
#ifdef HAS_RENDER_LAYER32X8
  int smzx = layer->mode;
  int trans = layer->transparent_col != -1;
  int clip =
   (layer->x < 0 || layer->y < 0 ||
    (layer->x + layer->w * CHAR_W) > width_px ||
    (layer->y + layer->h * CHAR_H) > height_px);

  if(!render_layer32x8(pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip))
#endif
  SKIP();
}

UNITTEST(render_layer32x8_mzx)
{
  using test = render_layer_tester<uint32_t, MZX, FLAT32, 0, render_layer32x8_wrap>;
  SECTION(graphic)      test::graphic(DIR "32.tga.gz");
  SECTION(align)        test::align(DIR "32a.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32ta.tga.gz");
  SECTION(clip)         test::clip(DIR "32c.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32tc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32a.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32ta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32c.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32tc.tga.gz");
  SECTION(large)        test::large(DIR "32xl.tga.gz");
}

UNITTEST(render_layer32x8_smzx)
{
  using test = render_layer_tester<uint32_t, SMZX, FLAT32, 0, render_layer32x8_wrap>;
  SECTION(graphic)      test::graphic(DIR "32s.tga.gz");
  SECTION(align)        test::align(DIR "32sa.tga.gz");
  SECTION(align_tr)     test::align_tr(DIR "32sta.tga.gz");
  SECTION(clip)         test::clip(DIR "32sc.tga.gz");
  SECTION(clip_tr)      test::clip_tr(DIR "32stc.tga.gz");
  SECTION(misalign)     test::misalign(DIR "32sa.tga.gz");
  SECTION(misalign_tr)  test::misalign_tr(DIR "32sta.tga.gz");
  SECTION(misclip)      test::misclip(DIR "32sc.tga.gz");
  SECTION(misclip_tr)   test::misclip_tr(DIR "32stc.tga.gz");
  SECTION(large)        test::large(DIR "32sxl.tga.gz");
}

struct viewport_test
{
  struct
  {
    int w;
    int h;
#define STRETCH 0, 0
#define MODERN_64_35 64, 35
#define CLASSIC_4_3 4, 3
// GX stretched widescreen ratios (4:3 pixel aspect ratio).
#define MODERN_64_35_GX (64 * 3), (35 * 4)
#define CLASSIC_4_3_GX (4 * 3), (3 * 4)
    int n;
    int d;
  } in;

  struct
  {
    int x;
    int y;
    int w;
    int h;
    boolean is_integer_scaled;
  } out;
};

template<void (*set_viewport_fn)(struct graphics_data *graphics,
 struct video_window *window)>
static void test_set_window_viewport(struct graphics_data *graphics,
 struct video_window *window, const viewport_test &test)
{
  window->width_px = test.in.w;
  window->height_px = test.in.h;
  window->ratio_numerator = test.in.n;
  window->ratio_denominator = test.in.d;

  set_viewport_fn(graphics, window);

#undef OUT_STR
#define OUT_STR "%d, %d (%d:%d)", test.in.w, test.in.h, test.in.n, test.in.d

  ASSERTEQ(window->viewport_x, test.out.x, OUT_STR);
  ASSERTEQ(window->viewport_y, test.out.y, OUT_STR);
  ASSERTEQ(window->viewport_width, test.out.w, OUT_STR);
  ASSERTEQ(window->viewport_height, test.out.h, OUT_STR);
  ASSERTEQ(window->is_integer_scaled, test.out.is_integer_scaled, OUT_STR);
}

/* Screen-sized viewport, centered regardless of window size. */
UNITTEST(set_window_viewport_centered)
{
  static constexpr viewport_test data[] =
  {
    { { 640, 350, MODERN_64_35 },     { 0, 0, 640, 350, true } },
    { { 640, 480, CLASSIC_4_3 },      { 0, 65, 640, 350, true } },
    { { 640, 350, CLASSIC_4_3 },      { 0, 0, 640, 350, true } },
    { { 640, 350, MODERN_64_35_GX },  { 0, 0, 640, 350, true } },
    { { 640, 350, CLASSIC_4_3_GX },   { 0, 0, 640, 350, true } },
    { { 640, 350, STRETCH },          { 0, 0, 640, 350, true } },
    { { 640, 350, 1, 0 },             { 0, 0, 640, 350, true } },
    { { 640, 350, 0, 1 },             { 0, 0, 640, 350, true } },
    { { 640, 350, -16, -32 },         { 0, 0, 640, 350, true } },
    { { 1280, 350, MODERN_64_35 },    { 320, 0, 640, 350, true }},
    { { 1280, 700, MODERN_64_35 },    { 320, 175, 640, 350, true } },
    { { 320, 240, MODERN_64_35 },     { -160, -55, 640, 350, true } },
    { { 0, 0, MODERN_64_35 },         { -320, -175, 640, 350, true } },
  };
  struct graphics_data graphics{};
  struct video_window window{};

  for(auto &d : data)
    test_set_window_viewport<set_window_viewport_centered>(&graphics, &window, d);
}

/* Viewport scaled to fit the window according to the requested ratio. */
UNITTEST(set_window_viewport_scaled)
{
  static constexpr viewport_test data[] =
  {
    { { 640, 350, MODERN_64_35 },     { 0, 0, 640, 350, true } },
    { { 640, 480, CLASSIC_4_3 },      { 0, 0, 640, 480, false } },
    { { 640, 560, STRETCH },          { 0, 0, 640, 560, false } },
    { { 640, 480, MODERN_64_35_GX },  { 0, 7, 640, 466, false } },
    { { 640, 640, CLASSIC_4_3_GX },   { 0, 0, 640, 640, false } },
    { { 640, 480, 1, 0 },             { 0, 0, 640, 480, false } },
    { { 640, 480, 0, 1 },             { 0, 0, 640, 480, false } },
    { { 640, 480, -20, -10 },         { 0, 0, 640, 480, false } },
    { { 1280, 350, MODERN_64_35 },    { 320, 0, 640, 350, true } },
    { { 1280, 350, STRETCH },         { 0, 0, 1280, 350, true } },
    { { 1280, 700, MODERN_64_35 },    { 0, 0, 1280, 700, true } },
    { { 1280, 1024, MODERN_64_35 },   { 0, 162, 1280, 700, true } },
    { { 1366, 768, MODERN_64_35 },    { 0, 10, 1366, 747, false } },
    { { 1366, 768, CLASSIC_4_3 },     { 171, 0, 1024, 768, false } },
    { { 1366, 768, STRETCH },         { 0, 0, 1366, 768, false } },
    { { 1366, 768, CLASSIC_4_3_GX },  { 299, 0, 768, 768, false } },
    { { 320, 240, MODERN_64_35 },     { 0, 32, 320, 175, false } },
    { { 320, 240, CLASSIC_4_3 },      { 0, 0, 320, 240, false } },
    { { 320, 240, CLASSIC_4_3_GX },   { 40, 0, 240, 240, false } },
    { { 1920, 1080, MODERN_64_35 },   { 0, 15, 1920, 1050, true } },
    { { 1920, 1080, CLASSIC_4_3 },    { 240, 0, 1440, 1080, false } },
    { { 1920, 1080, STRETCH },        { 0, 0, 1920, 1080, false } },
    { { 0, 0, MODERN_64_35 },         { -1, -1, 1, 1, false } },
    { { 25600, 16800, CLASSIC_4_3 },  { 1600, 0, 22400, 16800, true } },
  };
  struct graphics_data graphics{};
  struct video_window window{};

  for(auto &d : data)
    test_set_window_viewport<set_window_viewport_scaled>(&graphics, &window, d);
}

struct viewport_coords_test
{
  struct
  {
    int x;
    int y;
    int w;
    int h;
  } viewport;

  struct
  {
    int x;
    int y;
  } in;

  struct
  {
    int x;
    int y;
  } out;
};

/* Window space -> screen space conversion for updating the logical mouse position.
 * Also returns the minimum and maximum viewport coordinates. */
UNITTEST(get_screen_coords_viewport)
{
  static constexpr viewport_coords_test data[] =
  {
    { { 0, 0, 640, 350 }, { 456, 123 }, { 456, 123 } },
    /* Clamp coordinates within the logical screen. */
    { { 0, 0, 640, 350 }, { -1, -1 }, { 0, 0 } },
    { { 0, 0, 640, 350 }, { 1000, 1000 }, { 639, 349 } },
    /* Downscale coordinates of 2x window */
    { { 0, 0, 1280, 700 }, { 1234, 456 }, { 617, 228 } },
    { { 0, 0, 1280, 700 }, { 1279, 699 }, { 639, 349 } },
    /* Upscale coordinates of GP2X half-size 4:3 window. */
    { { 0, 0, 320, 240 }, { 160, 120 }, { 320, 175 } },
    { { 0, 0, 320, 240 }, { 319, 239 }, { 638, 348 } },
    /* 1920x1080 with 64:35 (15px letterbox top and bottom). */
    { { 0, 15, 1920, 1050 }, { 0, 0 }, { 0, 0 } },
    { { 0, 15, 1920, 1050 }, { 1600, 1079 }, { 533, 349 } },
    { { 0, 15, 1920, 1050 }, { 1919, 17 }, { 639, 0 } },
    { { 0, 15, 1920, 1050 }, { 1919, 18 }, { 639, 1 } },
    { { 0, 15, 1920, 1050 }, { 1919, 20 }, { 639, 1 } },
    { { 0, 15, 1920, 1050 }, { 1919, 21 }, { 639, 2 } },
  };
  struct graphics_data graphics{};
  struct video_window window{};

  for(auto &d : data)
  {
    window.viewport_x = d.viewport.x;
    window.viewport_y = d.viewport.y;
    window.viewport_width = d.viewport.w;
    window.viewport_height = d.viewport.h;

#define NO_VALUE -65536
    int x = NO_VALUE;
    int y = NO_VALUE;
    int min_x = NO_VALUE;
    int min_y = NO_VALUE;
    int max_x = NO_VALUE;
    int max_y = NO_VALUE;
    get_screen_coords_viewport(&graphics, &window, d.in.x, d.in.y,
     &x, &y, &min_x, &min_y, &max_x, &max_y);

#undef OUT_STR
#define OUT_STR "%d,%d { %d,%d %dx%d }", d.in.x, d.in.y, \
 d.viewport.x, d.viewport.y, d.viewport.w, d.viewport.h

    ASSERTEQ(x, d.out.x, OUT_STR);
    ASSERTEQ(y, d.out.y, OUT_STR);
    ASSERTEQ(min_x, d.viewport.x, OUT_STR);
    ASSERTEQ(min_y, d.viewport.y, OUT_STR);
    ASSERTEQ(max_x, d.viewport.x + d.viewport.w - 1, OUT_STR);
    ASSERTEQ(max_y, d.viewport.y + d.viewport.h - 1, OUT_STR);
  }
}

/* Screen space -> window space conversion for warping the system mouse. */
UNITTEST(set_screen_coords_viewport)
{
  static constexpr viewport_coords_test data[] =
  {
    { { 0, 0, 640, 350 }, { 456, 123 }, { 456, 123 } },
    /* No clamping within the window currently */
    { { 0, 0, 640, 350 }, { -1, -1 }, { -1, -1 } },
    { { 0, 0, 640, 350 }, { 1000, 1000 }, { 1000, 1000 } },
    /* Upscale coordinates of 2x window. */
    { { 0, 10, 1280, 700 }, { 320, 175 }, { 640, 360 } },
    { { 0, 100, 1280, 700 }, { 320, 175 }, { 640, 450 } },
    { { 0, 0, 1280, 700 }, { 639, 349 }, { 1278, 698 } },
    /* Downscale coordinates of GP2X half-size 4:3 window. */
    { { 0, 0, 320, 240 }, { 320, 175 }, { 160, 120 } },
    { { 0, 0, 320, 240 }, { 639, 349 }, { 319, 239 } },
    { { 160, 120, 320, 240 }, { -2, -2 }, { 159, 119 } },
    { { 160, 120, 320, 240 }, { -1, -1 }, { 160, 120 } },
    { { 160, 120, 320, 240 }, { 0, 0 }, { 160, 120 } },
    { { 160, 120, 320, 240 }, { 1, 1 }, { 160, 120 } },
    { { 160, 120, 320, 240 }, { 2, 2 }, { 161, 121 } },
    /* Upscale coordinates of 1920x1080 (3x) window. */
    { { 0, 15, 1920, 1050 }, { 0, 0 }, { 0, 15 } },
    { { 0, 15, 1920, 1050 }, { 320, 175 }, { 960, 540 } },
    { { 0, 15, 1920, 1050 }, { 639, 349 }, { 1917, 1062 } },
  };
  struct graphics_data graphics{};
  struct video_window window{};

  for(auto &d : data)
  {
    window.viewport_x = d.viewport.x;
    window.viewport_y = d.viewport.y;
    window.viewport_width = d.viewport.w;
    window.viewport_height = d.viewport.h;

    int x = NO_VALUE;
    int y = NO_VALUE;
    set_screen_coords_viewport(&graphics, &window, d.in.x, d.in.y, &x, &y);

    ASSERTEQ(x, d.out.x, OUT_STR);
    ASSERTEQ(y, d.out.y, OUT_STR);
  }
}
