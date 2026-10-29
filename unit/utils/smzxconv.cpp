/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "../UnitIO.hpp"
#include "../../src/utils/smzxconv.c"

/* TODO: this test could probably be much more thorough. */

/* png2smzx always outputs layer mode MZM2. */
#define MZM_DATA_OFFSET 16

static const char *DATA_DIR = "unit/utils/data";

struct smzxconv_data
{
  const char *input;
  const char *expected;
  size_t width;
  size_t height;
};

struct smzxconv_input
{
  std::vector<uint8_t> &buffer;
  size_t pos;
};

static size_t image_read_fn(void *dest, size_t num, void *handle)
{
  smzxconv_input *in = reinterpret_cast<smzxconv_input *>(handle);

  if(in->pos >= in->buffer.size())
    return 0;

  num = Unit::min(num, in->buffer.size() - in->pos);
  memcpy(dest, in->buffer.data() + in->pos, num);
  in->pos += num;
  return num;
}

UNITTEST(Full)
{
  static constexpr smzxconv_data testfiles[] =
  {
    { "smzxconv_full.ppm.gz", "smzxconv_full.ppm", 640, 350 },
  };

  mzx_glyph chr[256];
  mzx_color pal[256];
  smzx_converter *c;
  char path[MAX_PATH];

  Unit::pushd cd(DATA_DIR);

  for(const smzxconv_data &d : testfiles)
  {
    std::vector<uint8_t> expected = unit::io::load_compressed(d.input);
    smzxconv_input in = { expected, 0 };
    struct image_file image;

    enum image_error err = load_image_from_stream(&in, image_read_fn, &image, nullptr);
    ASSERTEQ(err, IMAGE_OK, "%s", d.input);
    ASSERTEQ(image.width, d.width, "%s", d.input);
    ASSERTEQ(image.height, d.height, "%s", d.input);

    c = smzx_convert_init(image.width / 8, image.height / 14, 0, -1, 256, 0, 16);
    ASSERT(c, "%s", d.input);

    std::vector<mzx_tile> tile(image.width * image.height);

    int ret = smzx_convert(c, image.data, tile.data(), chr, pal);
    smzx_convert_free(c);
    image_free(&image);

    ASSERTEQ(ret, 1, "%s", d.input);

    snprintf(path, sizeof(path), "%s.mzm", d.expected);
    expected = unit::io::load(path);
    ASSERTMEM(reinterpret_cast<uint8_t *>(tile.data()),
     expected.data() + MZM_DATA_OFFSET, expected.size() - MZM_DATA_OFFSET, "%s", d.input);

    snprintf(path, sizeof(path), "%s.chr", d.expected);
    expected = unit::io::load(path);
    ASSERTMEM(reinterpret_cast<uint8_t *>(chr),
     expected.data(), expected.size(), "%s", d.input);

    snprintf(path, sizeof(path), "%s.pal", d.expected);
    expected = unit::io::load(path);
    ASSERTMEM(reinterpret_cast<uint8_t *>(pal),
     expected.data(), expected.size(), "%s", d.input);
  }
}

/* TODO: test skip_char and other settings. */
/* TODO: bounding tests should be done by smzx_convert_init, not png2smzx */
