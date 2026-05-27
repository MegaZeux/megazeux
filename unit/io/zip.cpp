/* MegaZeux
 *
 * Copyright (C) 2020-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../../src/util.h"
#include "../../src/io/memfile.h"
#include "../../src/io/path.h"
#include "../../src/io/zip.h"
#include "../../src/io/zip_stream.h"

static const size_t BUFFER_SIZE = (1 << 17);
static const char DATA_DIR[] = "unit/io/data";

struct pair
{
  /* const would be nice on these, but it breaks in GCC 4.8 */
  const char *data;
  size_t len;
  uint16_t flags;
  std::vector<uint8_t> external;
};

struct zip_stream_test_data
{
  const char *testname; // For debug output.
  pair expected;
  pair deflate;
  pair deflate64;
  pair shrink;
  pair reduce1;
  pair reduce2;
  pair reduce3;
  pair reduce4;
  pair implode;
  boolean inputs_are_paths;
  boolean expected_is_base64;
  boolean compressed_is_base64;
};

static zip_stream_test_data data[] =
{
  // WHITE.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    "WHITE.PAL",
    { "????????????????????????????????????????????????", 48, 0, {} },
    { "sycRAAA=", 5, 0, {} },
    { "sycRAAA=", 5, 0, {} },
    { "PwIKHEiwoMGDCAUC", 12, 0, {} },
    {},
    {},
    {},
    {},
    { "DwASAyQVNic4OWp7TJ1uHwkGARM05faW93/+CAZgAw==", 31, 0, {} },
    false,
    false,
    true
  },
  // FRED_01.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    "FRED_01.PAL",
    { "AAAAAAAqACoAGz8bBz8HAjQCKhUAKioqFRUVFRU/FT8VFT8/PxUVPxU/MzMJPz8/", 48, 0, {} },
    { "YwABLSCUtpdmt2dnMmHSEmXQ0tISBQF7IBS1t7cHs4yNOYFMAA==", 37, 0, {} },
    { "YwABLSCUtpdmt2dnMmHSEmXQ0tISBQF7IBS1t7cHs4yNOYFMAA==", 37, 0, {} },
    { "AAIGVAGA4IYfGw78OCCAhgAVFQqqgFihYoUfFyv+2Kjx4owZCTb+AA==", 40, 0, {} },
    {},
    {},
    {},
    {},
    {},
    false,
    true,
    true
  },
  // FRED_02.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    "FRED_02.PAL",
    { "AAAAAAAqACoAACoqKgAAKgAqBwcHDw8PCQkJCgoKFT8VFT8/FSoVKhUAPz8VPz8/", 48, 0, {} },
    { "HYaxEQBACMLsPG2oWIIl2H+r502KpD6KJemuuwHMzO7SpE3FSm0/", 39, 0, {} },
    { "HYaxEQBACMLsPG2oWIIl2H+r502KpD6KJemuuwHMzO7SpE3FSm0/", 39, 0, {} },
    { "AAIGVAGAYEEVBgmqOMDwgcMEEBVIrPCjAsWKKipkBPCjYscf", 36, 0, {} },
    {},
    {},
    {},
    {},
    {},
    false,
    true,
    true
  },
  // CN_S.CHR from Dark Nova was originally imploded.
  {
    "CN_S.CHR",
    { "CN_S.CHR",           3584, 0, {} },
    { "CN_S.CHR.deflate",   1618, 0, {} },
    { "CN_S.CHR.deflate",   1618, 0, {} }, // Result is same as above.
    { "CN_S.CHR.shrink",    1906, 0, {} },
    { "CN_S.CHR.reduce1",   2123, 0, {} },
    { "CN_S.CHR.reduce2",   2134, 0, {} },
    { "CN_S.CHR.reduce3",   2147, 0, {} },
    { "CN_S.CHR.reduce4",   2138, 0, {} },
    { "CN_S.CHR.implode",   1793, 0x0000, {} },
    true,
    false,
    false
  },
  // FREAKSOF.MZX from Freaks Collection was originally imploded.
  {
    "FREAKSOF.MZX",
    { "FREAKSOF.MZX",       16525, 0, {} },
    { "freaksof.deflate",   6132, 0, {} },
    { "freaksof.deflate64", 6137, 0, {} },
    { "freaksof.shrink",    9425, 0, {} },
    { "freaksof.reduce1",   7600, 0, {} },
    { "freaksof.reduce2",   7417, 0, {} },
    { "freaksof.reduce3",   7386, 0, {} },
    { "freaksof.reduce4",   7367, 0, {} },
    { "freaksof.implode",   6586, 0x0000, {} },
    true,
    false,
    false
  },
  {
    "dch1.txt",
    { "dch1.txt",           7045, 0, {} },
    { "dch1.deflate",       3385, 0, {} },
    { "dch1.deflate64",     3352, 0, {} },
    { "dch1.shrink",        3980, 0, {} },
    { "dch1.reduce1",       4549, 0, {} },
    { "dch1.reduce2",       4505, 0, {} },
    { "dch1.reduce3",       4459, 0, {} },
    { "dch1.reduce4",       4411, 0, {} },
    { "dch1.implode",       3666, 0x0006, {} },
    true,
    false,
    false
  },
  {
    "CT_LEVEL.MOD",
    { "CT_LEVEL.MOD",       111885, 0, {} },
    { "ct_level.deflate",   61105, 0, {} },
    { "ct_level.deflate64", 61051, 0, {} },
    { "ct_level.shrink",    80628, 0, {} },
    { "ct_level.reduce1",   67216, 0, {} },
    { "ct_level.reduce2",   66109, 0, {} },
    { "ct_level.reduce3",   66077, 0, {} },
    { "ct_level.reduce4",   65837, 0, {} },
    { "ct_level.implode",   66879, 0x0000, {} },
    true,
    false,
    false
  },
};

static void debase64(std::vector<uint8_t> &dest, const void *_src, size_t src_len)
{
  static const char lut[256] =
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62,0, 0, 0, 63,
    52,53,54,55,56,57,58,59,60,61,0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,0, 0, 0, 0, 0,
    0, 26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,0, 0, 0, 0, 0,
  };
  const uint8_t *src = (const uint8_t *)_src;
  size_t i;

  assert((src_len & 3) == 0);

  dest.reserve(src_len * 3 / 4);
  dest.resize(0);

  for(i = 0; i < src_len; i += 4)
  {
    uint32_t x = lut[src[i+3]] | (lut[src[i+2]] << 6) |
     (lut[src[i+1]] << 12) | (lut[src[i]] << 18);

    dest.emplace_back((x >> 16) & 0xFF);
    dest.emplace_back((x >> 8) & 0xFF);
    dest.emplace_back(x & 0xFF);
  }

  if(src[src_len - 1] == '=')
    dest.pop_back();
  if(src[src_len - 2] == '=')
    dest.pop_back();
}

static const uint8_t *load_file(std::vector<uint8_t> &buffer, const char *path)
{
  char path_buffer[MAX_PATH];
  if(path_join(path_buffer, sizeof(path_buffer), DATA_DIR, path) < 1)
    return nullptr;

  unit::io::load_vector(buffer, path_buffer);
  return buffer.data();
}

static const uint8_t *load_file(pair &pair)
{
  return load_file(pair.external, pair.data);
}

static void check_data(zip_stream_test_data &data)
{
  if(data.inputs_are_paths)
  {
    load_file(data.expected);
    load_file(data.deflate);
    load_file(data.deflate64);
    load_file(data.shrink);
    load_file(data.reduce1);
    load_file(data.reduce2);
    load_file(data.reduce3);
    load_file(data.reduce4);
    load_file(data.implode);
    data.expected_is_base64 = false;
    data.compressed_is_base64 = false;
  }
}

static void check_data()
{
  static boolean loaded = false;
  if(!loaded)
  {
    loaded = true;
    for(zip_stream_test_data &d : data)
      check_data(d);
  }
}

static const zip_method_handler *get_stream(zip_compression_method method)
{
  assert(method <= arraysize(zip_method_handlers));
  return zip_method_handlers[method];
}

static enum zip_error compress(const zip_method_handler *stream,
 zip_stream_data *sd, zip_compression_method method, uint16_t flags,
 const uint8_t *in, size_t in_len, std::vector<uint8_t> &out, size_t *final_out)
{
  enum zip_error result;

  ASSERT(in, "");
  ASSERT(final_out, "");
  ASSERT(stream->compress_open, "");
  ASSERT(stream->compress_block, "");
  ASSERT(stream->input, "");
  ASSERT(stream->output, "");
  ASSERT(stream->close, "");

  stream->compress_open(sd, method, flags);
  stream->input(sd, in, in_len);
  stream->output(sd, out.data(), out.size());

  result = stream->compress_block(sd, true);

  stream->close(sd, nullptr, final_out);
  return result;
}

static enum zip_error decompress(const zip_method_handler *stream,
 zip_stream_data *sd, zip_compression_method method, uint16_t flags,
 const uint8_t *in, size_t in_len, std::vector<uint8_t> &out)
{
  enum zip_error result;

  ASSERT(in, "");
  ASSERT(stream->decompress_open, "");
  ASSERT(stream->decompress_block || stream->decompress_file, "");
  ASSERT(stream->input, "");
  ASSERT(stream->output, "");
  ASSERT(stream->close, "");

  stream->decompress_open(sd, method, flags);
  stream->input(sd, in, in_len);
  stream->output(sd, out.data(), out.size());

  if(stream->decompress_block)
    result = stream->decompress_block(sd);
  else
    result = stream->decompress_file(sd);

  size_t final_len = 0;
  stream->close(sd, nullptr, &final_len);
/*
  char buf[32];
  snprintf(buf, 32, "%d.in", method);
  FILE *fp = fopen_unsafe(buf, "wb");
  fwrite(in, in_len, 1, fp);
  fclose(fp);
  snprintf(buf, 32, "%d.out", method);
  fp = fopen_unsafe(buf, "wb");
  fwrite(out, final_len, 1, fp);
  fclose(fp);
*/
  return result;
}

static void decompress_set(const uint8_t *&x, size_t &x_len, std::vector<uint8_t> &v,
 const pair &p, boolean is_base64, const zip_stream_test_data &d)
{
  if(is_base64)
  {
    debase64(v, p.data, strlen(p.data));
  }
  else

  if(d.inputs_are_paths)
  {
    ASSERT(p.external.size(), "%s: external data is blank", d.testname);
    v = p.external;
  }
  else
  {
    v.resize(p.len);
    memcpy(v.data(), p.data, p.len);
  }
  x = v.data();
  x_len = v.size();
}

#define SET_A(p,is_b64) \
  ASSERT((p).data, "%s: data is null", d.testname); \
  decompress_set(a, a_len, buffer_a, (p), (is_b64), d)

#define SET_B(p,is_b64) \
  if(!(p).data) \
    continue; \
  flags = (p).flags; \
  decompress_set(b, b_len, buffer_b, (p), (is_b64), d)

/**
 * Tests:
 * 1) a valid input should decompress to the exact expected output.
 * 2) a truncated input should generally fail (file) or return ZIP_INPUT_EMPTY (block).
 * 3) corrupted inputs (simulated by copying parts of the expected output over
 *    parts of the input) should just plain fail or produce a different output.
 */
template<enum zip_compression_method method>
static void decompress_boilerplate(pair zip_stream_test_data::*field)
{
  const uint8_t *a;
  const uint8_t *b;
  size_t a_len;
  size_t b_len;
  uint16_t flags;
  enum zip_error result;
  int cmp;

  const zip_method_handler *stream = get_stream(method);
  if(!stream)
  {
    if(method == ZIP_M_DEFLATE)
      FAIL("failed to get deflate stream!");
    else
      SKIP();
  }

  ASSERT(stream->create, "");
  ASSERT(stream->destroy, "");

  struct zip_stream_data *sd = stream->create();

  std::vector<uint8_t> buffer(BUFFER_SIZE);
  std::vector<uint8_t> buffer_a(BUFFER_SIZE);
  std::vector<uint8_t> buffer_b(BUFFER_SIZE);

  for(const zip_stream_test_data &d : data)
  {
    const pair &input = d.*field;

    SET_A(d.expected, d.expected_is_base64);
    SET_B(input, d.compressed_is_base64);
    result = decompress(stream, sd, method, flags, b, b_len, buffer);
    cmp = memcmp(a, buffer.data(), a_len);
    ASSERTEQ(cmp, 0, "%s: valid", d.testname);

    for(int j = 7; j >= 0; j--)
    {
      std::fill(buffer.begin(), buffer.end(), 0);

      result = decompress(stream, sd, method, flags, b, b_len * j / 8, buffer);
      ASSERT(result != ZIP_OUTPUT_FULL && result != ZIP_EOF,
       "%s: truncated %d/8", d.testname, j);

      if(result == ZIP_STREAM_FINISHED)
      {
        cmp = !memcmp(a, buffer.data(), a_len);
        ASSERTEQ(cmp, 0, "%s: truncated %d/8", d.testname, j);
      }
    }

    for(int j = 0; j < 8; j++)
    {
      std::fill(buffer.begin(), buffer.end(), 0);

      SET_B(input, d.compressed_is_base64);
      size_t size = MAX(b_len / 8, 1);
      if(size > a_len)
        continue;

      size_t a_pos = MIN(a_len - size, j * size);
      size_t b_pos = MIN(b_len - size, j * size);
      if(!memcmp(b + b_pos, a + a_pos, size))
        continue;

      memcpy(buffer_b.data() + b_pos, a + a_pos, size);
      result = decompress(stream, sd, method, flags, b, b_len, buffer);
      ASSERT(result != ZIP_EOF, "%s: corrupt %d", d.testname, j);

      if(result == ZIP_STREAM_FINISHED)
      {
        cmp = !memcmp(a, buffer.data(), a_len);
        ASSERTEQ(cmp, 0, "%s: corrupt %d", d.testname, j);
      }
    }
  }
  stream->destroy(sd);
}

UNITTEST(Decompress)
{
  check_data();

  SECTION(Inflate)
    decompress_boilerplate<ZIP_M_DEFLATE>(&zip_stream_test_data::deflate);

  SECTION(Inflate64)
    decompress_boilerplate<ZIP_M_DEFLATE64>(&zip_stream_test_data::deflate64);

  SECTION(Unshrink)
    decompress_boilerplate<ZIP_M_SHRUNK>(&zip_stream_test_data::shrink);

  SECTION(Expand1)
    decompress_boilerplate<ZIP_M_REDUCED_1>(&zip_stream_test_data::reduce1);

  SECTION(Expand2)
    decompress_boilerplate<ZIP_M_REDUCED_2>(&zip_stream_test_data::reduce2);

  SECTION(Expand3)
    decompress_boilerplate<ZIP_M_REDUCED_3>(&zip_stream_test_data::reduce3);

  SECTION(Expand4)
    decompress_boilerplate<ZIP_M_REDUCED_4>(&zip_stream_test_data::reduce4);

  SECTION(Explode)
    decompress_boilerplate<ZIP_M_IMPLODED>(&zip_stream_test_data::implode);
}

/**
 * Compress each of the raw test inputs, then decompress it and check to
 * make sure it's the same.
 */
UNITTEST(Compress)
{
  std::vector<uint8_t> buffer_a(BUFFER_SIZE);
  std::vector<uint8_t> buffer_cmp(BUFFER_SIZE);
  std::vector<uint8_t> buffer_dcmp(BUFFER_SIZE);
  const uint8_t *a;
  size_t a_len;
  size_t cmp_len;
  enum zip_error result;

  const zip_method_handler *stream = get_stream(ZIP_M_DEFLATE);
  if(!stream)
    FAIL("Failed to get deflate stream!");

  ASSERT(stream->create, "");
  ASSERT(stream->destroy, "");

  check_data();

  struct zip_stream_data *sd = stream->create();
  ASSERT(sd, "");

  for(const zip_stream_test_data &d : data)
  {
    SET_A(d.expected, d.expected_is_base64);

    result = compress(stream, sd, ZIP_M_DEFLATE, 0, a, a_len,
     buffer_cmp, &cmp_len);
    ASSERTEQ(result, ZIP_STREAM_FINISHED, "%s", d.testname);

    result = decompress(stream, sd, ZIP_M_DEFLATE, 0,
     buffer_cmp.data(), cmp_len, buffer_dcmp);
    ASSERTEQ(result, ZIP_STREAM_FINISHED, "%s", d.testname);

    ASSERTMEM(a, buffer_dcmp.data(), a_len, "%s", d.testname);
  }
  stream->destroy(sd);
}

static const char long_content[] =
{
  "1234567890 1234567890 1234567890\r\n"
  "1234567890 1234567890 1234567890\r\n"
  "1234567890 1234567890 1234567890"
};
static constexpr uint32_t long_len = sizeof(long_content) - 1;
static constexpr uint32_t long_crc = 0xE3046765;

enum contents_type
{
  CONTENTS_RAW,
  CONTENTS_BASE64,
  CONTENTS_FILE,
};

struct zip_test_file_data
{
  const char *filename;
  const char *contents;
  uint16_t flags;
  uint16_t method;
  uint32_t crc32;
  uint32_t uncompressed_size;
  uint32_t compressed_size;
  uint32_t offset;
  enum contents_type contents_type;
};

struct zip_test_data
{
  const char *testname; // For debug output.
  const char *data;
  size_t data_length;
  enum zip_error open_result;
  uint32_t num_files;
  zip_test_file_data files[16];

  static constexpr zip_test_data long_content_from_file(const char *filename,
   const char *internal_filename, uint16_t flags)
  {
    return zip_test_data{ filename, filename,
        0, ZIP_SUCCESS, 1,
        {{ internal_filename, long_content,
          flags, ZIP_M_NONE, long_crc, long_len, long_len, 0, CONTENTS_RAW }}};
  }
};

static const zip_test_data raw_zip_data[] =
{
  {
    "EOCD only",
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_SUCCESS, 0, {}
  },
  {
    "EOCD with prefix",
    "prefixed data here!"
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    41, ZIP_SUCCESS, 0, {}
  },
  {
    "abcde.zip",
    "abcde.zip", 0, ZIP_SUCCESS, 3,
    {
      { "abcde/", "",
        0x0000, ZIP_M_NONE, 0x00000000, 0, 0, 0, CONTENTS_RAW },
      { "abcde/a.txt", "abcde",
        0x0000, ZIP_M_NONE, 0x8587D865, 5, 5, 36, CONTENTS_RAW },
      { "abcde/b.txt", long_content,
        0x0000, ZIP_M_DEFLATE, long_crc, 100, 32, 82, CONTENTS_RAW },
    }
  },
  {
    "dch1.zip",
    "dch1.zip", 0, ZIP_SUCCESS, 1,
    {{
      "dch1.txt", "dch1.txt",
      0x0000, ZIP_M_DEFLATE, 0xA3898FBE, 7045, 3385, 0, CONTENTS_FILE }},
  },
  {
    "ct_level.zip",
    "ct_level.zip", 0, ZIP_SUCCESS, 1,
    {{
      "CT_LEVEL.MOD", "CT_LEVEL.MOD",
      0x0000, ZIP_M_DEFLATE, 0x2AF73EBE, 111885, 61105, 0, CONTENTS_FILE }},
  },
  /* Zip64 */
  {
    "EOCD (Zip64)",
    "PK\x06\x06\x2c\0\0\0\0\0\0\0\x2d\0\x2d\0\0\0\0\0\0\0\0\0"
      "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "PK\x06\x07\0\0\0\0\0\0\0\0\0\0\0\0\x01\0\0\0"
    "PK\x05\x06\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\0\0",
    98, ZIP_SUCCESS, 0, {}
  },
  {
    "dch1.zip64",
    "dch1.zip64", 0, ZIP_SUCCESS, 1,
    {{ "-", "dch1.txt",
      0x0002, ZIP_M_DEFLATE, 0xA3898FBE, 7045, 3435, 0, CONTENTS_FILE }},
  },
  /* Zip64 format variants which all contain the uncompressed long text. */
  zip_test_data::long_content_from_file("zip64/local64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/localdd64.zip", "test", ZIP_F_DATA_DESCRIPTOR),
  zip_test_data::long_content_from_file("zip64/localcd64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/localcddd64.zip", "test", ZIP_F_DATA_DESCRIPTOR),
  zip_test_data::long_content_from_file("zip64/cd64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/cduncomp64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/cdcomp64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/cdoffset64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/eocd64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/eocd64rc.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/eocd64sz.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/eocd64of.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/all64.zip", "test", 0x0000),
  zip_test_data::long_content_from_file("zip64/alldd64.zip", "test", ZIP_F_DATA_DESCRIPTOR),
};

// The library should refuse to open these...
static const zip_test_data raw_zip_invalid_data[] =
{
  {
    "empty file",
    "", 0, ZIP_NO_EOCD, 0, {}
  },
  {
    "EOCD truncated to 4 with prefix of 18",
    "                  PK\x05\x06", 22, ZIP_NO_EOCD, 0, {}
  },
  {
    "EOCD truncated to 21",
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    21, ZIP_NO_EOCD, 0, {}
  },
  {
    "Missing central directory",
    "PK\x05\x06\x00\x00\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_NO_CENTRAL_DIRECTORY, 0, {}
  },
  {
    "Multiple disks",
    "PK\x05\x06\x01\x00\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_UNSUPPORTED_MULTIPLE_DISKS, 0, {}
  },
};

static zip_archive *zip_test_open(const char *filename)
{
  char buffer[MAX_PATH];
  if(path_join(buffer, sizeof(buffer), DATA_DIR, filename) < 0)
    return nullptr;

  return zip_open_file_read(buffer);
}

static zip_archive *zip_test_open(const zip_test_data &d)
{
  if(!d.data_length)
  {
    char buffer[MAX_PATH];
    if(path_join(buffer, sizeof(buffer), DATA_DIR, d.data) < 0)
      return nullptr;

    return zip_open_file_read(buffer);
  }
  else
    return zip_open_mem_read((const void *)d.data, d.data_length);
}

static zip_archive *zip_test_open(const zip_test_data &d,
 std::vector<uint8_t> &buffer)
{
  if(!d.data_length)
  {
    if(!load_file(buffer, d.data))
      return nullptr;

    return zip_open_mem_read(buffer.data(), buffer.size());
  }
  else
    return zip_open_mem_read((const void *)d.data, d.data_length);
}

static const uint8_t *zip_get_contents(const zip_test_file_data &df,
 std::vector<uint8_t> &buffer)
{
  size_t len = df.contents ? strlen(df.contents) : 0;
  if(len)
  {
    switch(df.contents_type)
    {
      case CONTENTS_RAW:
        return reinterpret_cast<const uint8_t *>(df.contents);

      case CONTENTS_BASE64:
        debase64(buffer, df.contents, len);
        return buffer.data();

      case CONTENTS_FILE:
        if(load_file(buffer, df.contents))
        {
          assert(df.uncompressed_size == buffer.size());
          return buffer.data();
        }
        break;
    }
  }
  static const uint8_t nil[1] = { 0 };
  return nil;
}

// Check that the zip exists and that its central directory headers match the
// expected data.
static void zip_check(const zip_test_data &d, struct zip_archive *zp)
{
  ASSERT(zp, "%s", d.testname);
  ASSERTEQ(zp->num_files, d.num_files, "%s", d.testname);
  if(d.num_files)
    ASSERT(zp->files, "%s", d.testname);

  for(size_t j = 0; j < d.num_files; j++)
  {
    const zip_test_file_data &df = d.files[j];
    zip_file_header *fh = zp->files[j];

    ASSERT(fh, "%s file %zu", d.testname, j);
    ASSERTEQ(fh->flags, df.flags, "%s file %zu", d.testname, j);
    ASSERTEQ(fh->method, df.method, "%s file %zu", d.testname, j);
    ASSERTEQ(fh->crc32, df.crc32, "%s file %zu", d.testname, j);
    ASSERTEQ(fh->compressed_size, df.compressed_size, "%s file %zu", d.testname, j);
    ASSERTEQ(fh->uncompressed_size, df.uncompressed_size, "%s file %zu", d.testname, j);
    ASSERTCMP(fh->file_name, df.filename, "%s file %zu", d.testname, j);
    ASSERT(df.uncompressed_size <= BUFFER_SIZE, "%s file %zu", d.testname, j);
  }
}

#define ZIP_GET_CONTENTS(df) zip_get_contents(df, db64_buffer)

UNITTEST(ZipRead)
{
  std::vector<uint8_t> buffer(BUFFER_SIZE);
  std::vector<uint8_t> db64_buffer;
  std::vector<uint8_t> file_buffer;
  struct zip_archive *zp;
  enum zip_error result;
  char small_buffer[32];
  boolean has_files = false;
  int cmp;

  SECTION(OpenClose)
  {
    for(const zip_test_data &d : raw_zip_data)
    {
      zp = zip_test_open(d);
      zip_check(d, zp);

      result = zip_close(zp, nullptr);
      ASSERTEQ(result, ZIP_SUCCESS, "%s", d.testname);
    }
  }

  SECTION(OpenInvalid)
  {
    for(const zip_test_data &d : raw_zip_invalid_data)
    {
      zp = zip_test_open(d);
      ASSERTEQ(zp, nullptr, "%s", d.testname);

      // TODO: can't test the actual error returned from zip_read_directory anymore :(
    }
  }

  SECTION(ReadFile)
  {
    for(const zip_test_data &d : raw_zip_data)
    {
      if(d.num_files)
      {
        has_files = true;
        zp = zip_test_open(d);
        zip_check(d, zp);

        for(size_t j = 0; j < d.num_files; j++)
        {
          const zip_test_file_data &df = d.files[j];
          size_t real_length = 0;

          result = zip_read_file(zp, buffer.data(), BUFFER_SIZE, &real_length);
          ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);
          ASSERTEQ(real_length, df.uncompressed_size, "%s file %zu", d.testname, j);

          if(real_length)
          {
            const uint8_t *contents = ZIP_GET_CONTENTS(df);
            cmp = memcmp(buffer.data(), contents, real_length);
            ASSERTEQ(cmp, 0, "%s file %zu", d.testname, j);
          }
        }
        result = zip_close(zp, nullptr);
        ASSERTEQ(result, ZIP_SUCCESS, "%s", d.testname);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }

  SECTION(ReadStream)
  {
    for(const zip_test_data &d : raw_zip_data)
    {
      if(d.num_files)
      {
        has_files = true;
        zp = zip_test_open(d);
        zip_check(d, zp);

        for(size_t j = 0; j < d.num_files; j++)
        {
          const zip_test_file_data &df = d.files[j];
          uint64_t real_length = 0;

          result = zip_read_open_file_stream(zp, &real_length);
          ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);

          const uint8_t *contents  = ZIP_GET_CONTENTS(df);
          for(size_t k = 0; k < real_length; k += sizeof(small_buffer))
          {
            size_t n = MIN(sizeof(small_buffer), real_length - k);
            result = zread(small_buffer, n, zp);
            ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);
            cmp = memcmp(small_buffer, contents + k, n);
            ASSERTEQ(cmp, 0, "%s file %zu", d.testname, j);
          }
          result = zip_read_close_stream(zp);
          ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);
        }
        result = zip_close(zp, nullptr);
        ASSERTEQ(result, ZIP_SUCCESS, "%s", d.testname);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }

  SECTION(ReadMemStream)
  {
    for(const zip_test_data &d : raw_zip_data)
    {
      if(d.num_files)
      {
        has_files = true;
        zp = zip_test_open(d, file_buffer);
        zip_check(d, zp);
        ASSERTEQ(zp->is_memory, true, "%s", d.testname);

        for(size_t j = 0; j < d.num_files; j++)
        {
          const zip_test_file_data &df = d.files[j];
          struct memfile mf;
          size_t real_length = 0;
          unsigned int method;

          result = zip_get_next_method(zp, &method);
          ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);

          result = zip_get_next_uncompressed_size(zp, &real_length);
          ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);

          result = zip_read_open_mem_stream(zp, &mf);
          if(method == ZIP_M_NONE)
          {
            const uint8_t *contents  = ZIP_GET_CONTENTS(df);
            ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);

            for(size_t k = 0; k < real_length; k += sizeof(small_buffer))
            {
              size_t n = MIN(sizeof(small_buffer), real_length - k);
              cmp = mfread(small_buffer, n, 1, &mf);
              ASSERTEQ(cmp, 1, "%s file %zu", d.testname, j);
              cmp = memcmp(small_buffer, contents + k, n);
              ASSERTEQ(cmp, 0, "%s file %zu", d.testname, j);
            }
            result = zip_read_close_stream(zp);
            ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);
          }
          else
          {
            ASSERTEQ(result, ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM, "%s file %zu", d.testname, j);
            result = zip_skip_file(zp);
            ASSERTEQ(result, ZIP_SUCCESS, "%s file %zu", d.testname, j);
          }
        }
        result = zip_close(zp, nullptr);
        ASSERTEQ(result, ZIP_SUCCESS, "%s", d.testname);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }
}

static void verify_boilerplate(const zip_test_data &d, struct zip_archive *zp,
 const char *label, std::vector<uint8_t> &verify_buffer,
 std::vector<uint8_t> &db64_buffer)
{
  enum zip_error result;

  verify_buffer.resize(BUFFER_SIZE);

  ASSERTEQ(d.num_files, zp->num_files, "%s %s", label, d.testname);
  for(size_t j = 0; j < d.num_files; j++)
  {
    const zip_test_file_data &df = d.files[j];
    const uint8_t *contents = ZIP_GET_CONTENTS(df);

    size_t real_length = 0;
    result = zip_read_file(zp, verify_buffer.data(), BUFFER_SIZE, &real_length);
    ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
    ASSERTEQ(real_length, df.uncompressed_size, "%s %s %zu", label, d.testname, j);

    int cmp = memcmp(contents, verify_buffer.data(), real_length);
    ASSERTEQ(cmp, 0, "%s %s %zu", label, d.testname, j);
  }

  result = zip_close(zp, nullptr);
  ASSERTEQ(result, ZIP_SUCCESS, "%s %s", label, d.testname);
}

UNITTEST(ZipWrite)
{
  std::vector<uint8_t> verify_buffer;
  std::vector<uint8_t> db64_buffer;
  // This buffer needs to be resized by C code...
  char *ext_buffer = (char *)cmalloc(32);
  size_t ext_buffer_size = 32;

  static const char OUTPUT_FILE[] = UNIT_TMP_DIR "/_output_file.zip";
  static const char LABEL[4][20] = { "File", "File(Zip64)", "MemoryExt", "MemoryExt(Zip64)" };
  struct zip_archive *zp;
  enum zip_error result;
  uint64_t final_size;

  SECTION(WriteFile)
  {
    for(int type = 0; type < 4; type++)
    {
      const char *label = LABEL[type];
      for(const zip_test_data &d : raw_zip_data)
      {
        if(type < 2)
          zp = zip_open_file_write(OUTPUT_FILE);
        else
          zp = zip_open_mem_write_ext((void **)&ext_buffer, &ext_buffer_size, 0);

        ASSERT(zp, "%s %s", label, d.testname);

        zip_set_zip64_enabled(zp, type & 1);

        for(size_t j = 0; j < d.num_files; j++)
        {
          const zip_test_file_data &df = d.files[j];
          const uint8_t *contents = ZIP_GET_CONTENTS(df);
          result = zip_write_file(zp, df.filename, (const void *)contents,
           df.uncompressed_size, df.method);

          ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
        }
        result = zip_close(zp, &final_size);
        ASSERTEQ(result, ZIP_SUCCESS, "%s %s", label, d.testname);

        if(type < 2)
          zp = zip_open_file_read(OUTPUT_FILE);
        else
          zp = zip_open_mem_read(ext_buffer, final_size);

        verify_boilerplate(d, zp, label, verify_buffer, db64_buffer);
      }
    }
  }

  SECTION(WriteStream)
  {
    for(int type = 0; type < 4; type++)
    {
      const char *label = LABEL[type];
      for(const zip_test_data &d : raw_zip_data)
      {
        if(type < 2)
          zp = zip_open_file_write(OUTPUT_FILE);
        else
          zp = zip_open_mem_write_ext((void **)&ext_buffer, &ext_buffer_size, 0);

        ASSERT(zp, "%s %s", label, d.testname);

        zip_set_zip64_enabled(zp, type & 1);

        for(size_t j = 0; j < d.num_files; j++)
        {
          const zip_test_file_data &df = d.files[j];
          const uint8_t *contents = ZIP_GET_CONTENTS(df);

          result = zip_write_open_file_stream(zp, df.filename, df.method);
          ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
          for(size_t k = 0; k < df.uncompressed_size; k += 32)
          {
            size_t size = MIN(df.uncompressed_size - k, 32);
            result = zwrite(contents + k, size, zp);
            ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
          }
          result = zip_write_close_stream(zp);
          ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
        }
        result = zip_close(zp, &final_size);
        ASSERTEQ(result, ZIP_SUCCESS, "%s %s", label, d.testname);

        if(type < 2)
          zp = zip_open_file_read(OUTPUT_FILE);
        else
          zp = zip_open_mem_read(ext_buffer, final_size);

        verify_boilerplate(d, zp, label, verify_buffer, db64_buffer);
      }
    }
  }

  // This is a special version of streaming that allows direct write access to
  // the buffer. This only works with the STORE method and likely doesn't work
  // very well with expandable buffers right now.
  SECTION(Memory_WriteMemStream)
  {
    const char *label = "MemoryFixed";
    for(const zip_test_data &d : raw_zip_data)
    {
      // Precompute likely required size for the archive.
      size_t max_name_size = 8;
      ext_buffer_size = 0;
      for(size_t j = 0; j < d.num_files; j++)
      {
        ext_buffer_size += d.files[j].uncompressed_size;
        max_name_size = MAX(max_name_size, strlen(d.files[j].filename));
      }
      ext_buffer_size += zip_bound_total_header_usage(d.num_files, max_name_size);
      ext_buffer = (char *)crealloc(ext_buffer, ext_buffer_size);

      zp = zip_open_mem_write(ext_buffer, ext_buffer_size, 0);
      ASSERT(zp, "%s %s", label, d.testname);

      for(size_t j = 0; j < d.num_files; j++)
      {
        const zip_test_file_data &df = d.files[j];
        const uint8_t *contents = ZIP_GET_CONTENTS(df);
        struct memfile mf;

        result = zip_write_open_mem_stream(zp, &mf, df.filename, df.uncompressed_size);
        ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
        int res = mfwrite(contents, df.uncompressed_size, 1, &mf);
        int expected = df.uncompressed_size != 0;
        ASSERTEQ(res, expected, "%s %s %zu", label, d.testname, j);
        result = zip_write_close_mem_stream(zp, &mf);
        ASSERTEQ(result, ZIP_SUCCESS, "%s %s %zu", label, d.testname, j);
      }
      result = zip_close(zp, &final_size);
      ASSERTEQ(result, ZIP_SUCCESS, "%s %s", label, d.testname);

      zp = zip_open_mem_read(ext_buffer, final_size);
      verify_boilerplate(d, zp, label, verify_buffer, db64_buffer);
    }
  }

  // Make sure fixed buffer memory write operations don't write past the buffer...
  SECTION(Memory_EOF)
  {
    char buffer[128];

    // EOF during EOCD write.
    zp = zip_open_mem_write(buffer, 20, 0);
    ASSERT(zp, "");
    result = zip_close(zp, nullptr);
    ASSERTEQ(result, ZIP_EOF, "Should fail EOCD write.");

    // EOF during local header write.
    zp = zip_open_mem_write(buffer, 32, 0);
    ASSERT(zp, "");
    result = zip_write_file(zp, "filename.ext", "abcde", 5, ZIP_M_NONE);
    ASSERTEQ(result, ZIP_EOF, "Should fail local header write.");
    zip_close(zp, nullptr);

    // EOF during file contents write.
    zp = zip_open_mem_write(buffer, 48, 0);
    ASSERT(zp, "");
    zip_set_zip64_enabled(zp, false);
    result = zip_write_open_file_stream(zp, "filename.ext", ZIP_M_NONE);
    ASSERTEQ(result, ZIP_SUCCESS, "Failed to open write stream.");
    result = zwrite("abcdefghij", 10, zp);
    ASSERTEQ(result, ZIP_EOF, "Should fail file write.");
    zip_write_close_stream(zp);
    zip_close(zp, nullptr);

    // EOF during central directory write.
    zp = zip_open_mem_write(buffer, 72, 0);
    ASSERT(zp, "");
    result = zip_write_file(zp, "filename.ext", "abcdefghij", 10, ZIP_M_NONE);
    ASSERTEQ(result, ZIP_SUCCESS, "Failed to write file.");
    result = zip_close(zp, nullptr);
    ASSERTEQ(result, ZIP_EOF, "Should fail to write central directory.");

    // EOF during EOCD write after successful central directory write.
    zp = zip_open_mem_write(buffer, 128, 0);
    ASSERT(zp, "");
    result = zip_write_file(zp, "filename.ext", "abcdefghij", 10, ZIP_M_NONE);
    ASSERTEQ(result, ZIP_SUCCESS, "Failed to write file");
    result = zip_close(zp, nullptr);
    ASSERTEQ(result, ZIP_EOF, "Should fail to write EOCD (2).");
  }

  // Make sure attempting to provide the external size buffer as the final
  // length doesn't crash (this really shouldn't be done though)...
  SECTION(Memory_Ext_DuplicateFinalLength)
  {
    char tmp[32]{};

    zp = zip_open_mem_write_ext(reinterpret_cast<void **>(&ext_buffer), &ext_buffer_size, 0);
    ASSERT(zp, "");

    result = zip_write_file(zp, "", tmp, sizeof(tmp), ZIP_M_NONE);
    ASSERTEQ(result, ZIP_SUCCESS, "Failed to write dummy file.");

    // This is the bad thing you shouldn't do!
    result = zip_close(zp, reinterpret_cast<uint64_t *>(&ext_buffer_size));
    ASSERTEQ(result, ZIP_SUCCESS, "Failed write central directory.");

    // Final size = local header (30) + data (32) + central header (46) + eocd (22).
    // If data descriptors are enabled, it will be 142 instead (+12).
    ASSERT(ext_buffer_size == 130 || ext_buffer_size == 142,
     "Incorrect final size: %zu", ext_buffer_size);
  }

  free(ext_buffer);
}

UNITTEST(Zip64)
{
  // Special case test files for Zip64.
  struct zip_archive *zp;
  enum zip_error result;
  size_t sz;

  SECTION(Zip64FileCount)
  {
    // This archive contains an MZX 2.92f format world with over 65535 files,
    // which requires an EOCD64 record and Zip64 support. The internal archive
    // is about 8.5 MB in size.
    zp = zip_test_open("zip64_file_count.zip");
    ASSERT(zp, "");

    result = zip_get_next_uncompressed_size(zp, &sz);
    ASSERTEQ(result, ZIP_SUCCESS, "");

    std::vector<uint8_t> buffer(sz);
    result = zip_read_file(zp, buffer.data(), sz, &sz);
    ASSERTEQ(result, ZIP_SUCCESS, "");

    zip_close(zp, NULL);

    // Open and verify inner archive.
    zp = zip_open_mem_read(buffer.data(), sz);
    ASSERT(zp, "");
    for(size_t i = 0; i < zp->num_files; i++)
    {
      result = zip_read_open_file_stream(zp, NULL);
      ASSERTEQ(result, ZIP_SUCCESS, "");
      // Close file to perform a CRC check.
      result = zip_read_close_stream(zp);
      ASSERTEQ(result, ZIP_SUCCESS, "");
    }
    zip_close(zp, NULL);
  }

  SECTION(Zip64Content)
  {
    // This double zipped archive contains an MZX counters file with a string
    // file over 4GB. Extract the outer archive to RAM, then stream the inner
    // archive to verify the uncompressed size works. For practical reasons
    // the counters file archive can't really be tested against zip.c since
    // vio.c can't transparently stream large files out of another archive.
    // The inner archive is about 5 MB in size.
    zp = zip_test_open("zip64_file_size.zip");
    ASSERT(zp, "");

    result = zip_get_next_uncompressed_size(zp, &sz);
    ASSERTEQ(result, ZIP_SUCCESS, "");

    std::vector<uint8_t> buffer(sz);
    result = zip_read_file(zp, buffer.data(), sz, &sz);
    ASSERTEQ(result, ZIP_SUCCESS, "");

    zip_close(zp, NULL);

    zp = zip_open_mem_read(buffer.data(), sz);
    ASSERT(zp, "");

    // Verifying the CRC of the internal file is too slow, just check the header.
    ASSERTEQ(zp->num_files, 1, "");
    ASSERTEQ(zp->files[0]->uncompressed_size, 4299174197ull, "");
    ASSERTEQ(zp->files[0]->compressed_size, 5062173, "");
    ASSERTEQ(zp->files[0]->crc32, 0x3cf11c4f, "");

    zip_close(zp, NULL);
  }
}
