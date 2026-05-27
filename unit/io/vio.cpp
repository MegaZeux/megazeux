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
#include "../../src/network/Scoped.hpp"
#include "../../src/io/path.h"
#include "../../src/io/vfs.h" /* VIRTUAL_FILESYSTEM */
#include "../../src/io/vio.h"

#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif

static constexpr char TEST_READ_FILENAME[]  = "VFILE_TEST_DATA";
static constexpr char TEST_WRITE_FILENAME[] = "VFILE_TEST_WRITE";
static constexpr char TEST_DIR[]            = "VFILE_TEST_DIR";

// NOTE: using the combining encodings of öè here to appease Mac OS X,
// which converts some single codepoint encodings to combining encodings.
// Some macOS SDKs (but not MacPorts clang) also have trouble with emoji.
#define UTF8_DIR_STRING u8"\u00e6Ro\u0308e\u0300mMJ\u00b7\u2021\u00b2\u2019\u02c6\u00de\u2018$"
static constexpr char UTF8_DIR[] = UTF8_DIR_STRING;
//static constexpr char UTF8_FILE[] = u8"\u00A5\u2014\U0001F970";
static constexpr char UTF8_FILE[] = u8"f\u0302(\u03be)=\u222b(-\u221e,\u221e)f(x)e^(-i2\u03c0\u03bex)dx";
static constexpr char UTF8_FILE2[] = u8"\u222b(-\u221e,\u221e)f(x)e^(-i2\u03c0\u03bex)dx=f\u0302(\u03be)";

// Randomly generated binary data. Note: this is null terminated for vfputs.
// The first byte is also \n to help test vungetc/vfsafegets behavior.
static const uint8_t test_data[] =
{
  0x0A,0xFF,0xE9,0x06,0x94,0xEE,0xC2,0xCE,0x87,0x72,0x5C,0xCE,0x5A,0xBD,0x72,0xE8,
  0x3A,0xD3,0x47,0x5F,0x2B,0x3F,0x8E,0x31,0x04,0x04,0x87,0xA7,0xBB,0xD6,0x6C,0x2A,
  0xE3,0x8A,0x0B,0x58,0x73,0xCC,0xB4,0x57,0xC9,0x10,0x55,0x69,0xEF,0xCB,0xE4,0xC1,
  0x1C,0xE2,0xFD,0x96,0x7E,0x62,0xCD,0x75,0x36,0x73,0x2E,0xDB,0xA5,0x87,0x13,0x03,
  0xE6,0xD7,0x27,0xFD,0x9B,0x10,0xB1,0x0D,0xED,0x49,0xBB,0x01,0xCE,0x39,0x73,0x84,
  0x81,0xA7,0xD1,0xB3,0x5D,0x21,0x7A,0xAD,0xCE,0xF1,0xE5,0x20,0x9C,0x4D,0xBC,0x1A,
  0x70,0xCE,0x85,0x1C,0x94,0x24,0x81,0x71,0xFA,0x07,0xD4,0xBD,0x80,0x70,0xE7,0xD9,
  0x72,0x0A,0x0B,0xDE,0x50,0xE3,0x5F,0x80,0xD2,0x68,0xF3,0x9E,0x2A,0x90,0x2D,0x16,
  0x89,0x57,0x6C,0xDE,0xE4,0x6E,0xEC,0x51,0xF8,0x31,0xEA,0xC6,0x8C,0xD9,0x08,0x67,
  0xF6,0xF3,0xF2,0x41,0xE0,0x11,0x42,0x97,0x4C,0xBF,0xA1,0x7C,0xD6,0xB8,0x2F,0xA1,
  0x39,0x5A,0x26,0x6B,0x15,0x58,0xBA,0x48,0xF0,0xAF,0x43,0x44,0x7A,0xDB,0x9D,0xD7,
  0x15,0x4A,0xCF,0x01,0x94,0x11,0xEC,0x99,0x43,0xDE,0x37,0xE3,0x28,0x2D,0x8A,0x60,
  0x89,0xBF,0xF9,0xEA,0xAF,0x48,0xB2,0x07,0xE9,0x69,0x27,0x5E,0xD3,0xDD,0x70,0xD0,
  0xD7,0xF7,0xEA,0x49,0xF5,0x4C,0x25,0x2F,0xC0,0xAD,0xFD,0xFA,0xA9,0x58,0x06,0xFD,
  0x80,0x6E,0x2E,0x83,0x38,0xA8,0x9D,0x1E,0xEB,0x46,0xE0,0x3C,0x1D,0x49,0x47,0xFB,
  0x45,0xE2,0x8B,0x3F,0x8A,0x2A,0xB4,0x01,0xCA,0x13,0x3A,0xEA,0xE0,0x9F,0x6B,0x00,
};

static constexpr int VFSAFEGETS_BUFFER = 64;
static constexpr int MAX_LINES = 10;

static char execdir[1024];
static char writedir[1024];

struct vfsafegets_data
{
  const char *filename;
  const char *input;
  const char *output[MAX_LINES];
};

static const vfsafegets_data vfsafegets_test_data[] =
{
  // Test 1.
  {
    "VFILE_TEST_DATA_TEXT",

    "kjflkjsdlfksjdfksdj\r\n"
    "abcdef\n"
    //"i hope no one is actually using these in 2020 ;-(\r"
    "use this line to fill past the buffer!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
    "\r\n"
    "end of file also counts as an end of line",
    {
      "kjflkjsdlfksjdfksdj",
      "abcdef",
      //"i hope no one is actually using these in 2020 ;-(",
      "use this line to fill past the buffer!!!!!!!!!!!!!!!!!!!!!!!!!!",
      "!!!!!!!!!",
      "",
      "end of file also counts as an end of line",
      nullptr
    }
  },
  // Test 2.
  {
    "VFILE_TEST_DATA_TEXT2",
    "trailing EOL should result in a single line read.\n",
    {
      "trailing EOL should result in a single line read.",
      nullptr
    }
  },
  // Test 3.
  {
    "VFILE_TEST_DATA_TEXT3",
    "trailing EOL should result in a single line read.\r\n",
    {
      "trailing EOL should result in a single line read.",
      nullptr
    }
  },
};

#define back_up(bytes_from_end, vf) \
{ \
  static_assert(bytes_from_end < 0, "invalid bytes from end value :("); \
  vfseek(vf, sizeof(test_data) + bytes_from_end - 1, SEEK_SET); \
  int c = vfgetc(vf); \
  ASSERTEQ(c, test_data[sizeof(test_data) + bytes_from_end - 1], "not eof"); \
}

static void test_vfgetc(vfile *vf)
{
  for(size_t i = 0; i < sizeof(test_data); i++)
  {
    int c = vfgetc(vf);
    ASSERTEQ(c, test_data[i], "vfgetc offset=%zu", i);
  }
  int c = vfgetc(vf);
  ASSERTEQ(c, EOF, "eof");
}

static void test_vfgetw(vfile *vf)
{
  int expected;
  int c;

  for(size_t i = 0; i < sizeof(test_data); i += 2)
  {
    c = vfgetw(vf);
    expected = test_data[i] | (test_data[i + 1] << 8);
    ASSERTEQ(c, expected, "vfgetw offset=%zu", i);
  }
  // Both bytes should cause EOF.
  c = vfgetw(vf);
  ASSERTEQ(c, EOF, "eof (byte 0)");

  back_up(-1, vf);
  c = vfgetw(vf);
  ASSERTEQ(c, EOF, "eof (byte 1)");
}

static void test_vfgetd(vfile *vf)
{
  int expected;
  int c;

  for(size_t i = 0; i < sizeof(test_data); i += 4)
  {
    c = vfgetd(vf);
    expected = static_cast<int>(test_data[i] | (test_data[i + 1] << 8) |
     (test_data[i + 2] << 16) | (test_data[i + 3] << 24));
    ASSERTEQ(c, expected, "vfgetd offset=%zu", i);
  }
  // All four bytes should cause EOF.
  c = vfgetd(vf);
  ASSERTEQ(c, EOF, "eof (byte 0)");

  back_up(-1, vf);
  c = vfgetd(vf);
  ASSERTEQ(c, EOF, "eof (byte 1)");

  back_up(-2, vf);
  c = vfgetd(vf);
  ASSERTEQ(c, EOF, "eof (byte 2)");

  back_up(-3, vf);
  c = vfgetd(vf);
  ASSERTEQ(c, EOF, "eof (byte 3)");
}

static void test_vfgetq(vfile *vf)
{
  int64_t expected;
  int64_t c;

  for(size_t i = 0; i < sizeof(test_data); i += 8)
  {
    c = vfgetq(vf);
    expected = static_cast<int64_t>(test_data[i] | (test_data[i + 1] << 8) |
     ((uint64_t)test_data[i + 2] << 16) | ((uint64_t)test_data[i + 3] << 24) |
     ((uint64_t)test_data[i + 4] << 32) | ((uint64_t)test_data[i + 5] << 40) |
     ((uint64_t)test_data[i + 6] << 48) | ((uint64_t)test_data[i + 7] << 56));
    ASSERTEQ(c, expected, "vfgetq offset=%zu", i);
  }
  // All eight bytes should cause EOF.
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 0)");

  back_up(-1, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 1)");

  back_up(-2, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 2)");

  back_up(-3, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 3)");

  back_up(-4, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 4)");

  back_up(-5, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 5)");

  back_up(-6, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 6)");

  back_up(-7, vf);
  c = vfgetq(vf);
  ASSERTEQ(c, EOF, "eof (byte 7)");
}

static void test_vfread(vfile *vf)
{
  static constexpr int len = sizeof(test_data);
  char buffer[len * 2];
  int ret;

  // One read >length should be refused.
  ret = vfread(buffer, len * 2, 1, vf);
  ASSERTEQ(ret, 0, "vfread too much data");
  vrewind(vf);

  // Two reads =length should read exactly length once.
  ret = vfread(buffer, len, 2, vf);
  ASSERTEQ(ret, 1, "vfread 2x len");
  vrewind(vf);

  // More reads...
  ret = vfread(buffer, len / 2, 3, vf);
  ASSERTEQ(ret, 2, "vfread 1.5x len");
  vrewind(vf);

  ret = vfread(buffer, len / 4, 5, vf);
  ASSERTEQ(ret, 4, "vfread 1.25x len");
  vrewind(vf);

  ret = vfread(buffer, 3, len, vf);
  ASSERTEQ(ret, len / 3, "vfread len x 3");
  // NOTE: seems to be implementation-defined whether this ends up
  // at 255 or 256, so don't bother testing that.

  vrewind(vf);
  ret = vfread(buffer, 0, len, vf);
  ASSERTEQ(ret, 0, "vfread 0 x len");
  ASSERTEQ(vftell(vf), 0, "vfread 0 x len");

  ret = vfread(buffer, len, 0, vf);
  ASSERTEQ(ret, 0, "vfread len x 0");
  ASSERTEQ(vftell(vf), 0, "vfread len x 0");
}

static void test_write_check(vfile *vf)
{
  uint8_t tmp[sizeof(test_data)];
  int r = vfread(tmp, sizeof(test_data), 1, vf);
  ASSERTEQ(r, 1, "test_write_check vfread");
  ASSERTMEM(test_data, tmp, sizeof(test_data), "test_write_check memcmp");
  r = vfgetc(vf);
  ASSERTEQ(r, EOF, "test_write_check eof");
}

static void test_vfputc(vfile *vf)
{
  //char buf[64];
  long pos = vftell(vf);

  for(size_t i = 0; i < sizeof(test_data); i++)
  {
    //snprintf(buf, sizeof(buf), "vfputc offset=%d", i);
    vfputc(test_data[i], vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputw(vfile *vf)
{
  //char buf[64];
  long pos = vftell(vf);

  for(size_t i = 0; i < sizeof(test_data); i += 2)
  {
    int d = test_data[i] | (test_data[i + 1] << 8);
    //snprintf(buf, sizeof(buf), "vfputw offset=%zu", i);
    vfputw(d, vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputd(vfile *vf)
{
  //char buf[64];
  long pos = vftell(vf);

  for(size_t i = 0; i < sizeof(test_data); i += 4)
  {
    int d = test_data[i] | (test_data[i + 1] << 8) |
     (test_data[i + 2] << 16) | (test_data[i + 3] << 24);
    //snprintf(buf, sizeof(buf), "vfputw offset=%zu, i);
    vfputd(d, vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputq(vfile *vf)
{
  //char buf[64];
  long pos = vftell(vf);

  for(size_t i = 0; i < sizeof(test_data); i += 8)
  {
    int64_t d = static_cast<int64_t>(test_data[i] | (test_data[i + 1] << 8) |
     ((uint64_t)test_data[i + 2] << 16) | ((uint64_t)test_data[i + 3] << 24) |
     ((uint64_t)test_data[i + 4] << 32) | ((uint64_t)test_data[i + 5] << 40) |
     ((uint64_t)test_data[i + 6] << 48) | ((uint64_t)test_data[i + 7] << 56));
    //snprintf(buf, sizeof(buf), "vfputw offset=%zu, i);
    vfputq(d, vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfwrite(vfile *vf)
{
  long pos = vftell(vf);

  int r = vfwrite(test_data, 1, sizeof(test_data), vf);
  ASSERTEQ(r, sizeof(test_data), "vfwrite");

  r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);

  pos = vftell(vf);

  r = vfwrite(test_data, 0, 1, vf);
  ASSERTEQ(r, 0, "vfwrite 0 x 1");
  ASSERTEQ(vftell(vf), pos, "vfwrite 0 x 1");

  r = vfwrite(test_data, 1, 0, vf);
  ASSERTEQ(r, 0, "vfwrite 1 x 0");
  ASSERTEQ(vftell(vf), pos, "vfwrite 1 x 0");
}

static void test_vfputs(vfile *vf)
{
  long pos = vftell(vf);

  int r = vfputs(reinterpret_cast<const char *>(test_data), vf);
  ASSERTEQ(r, 0, "vfputs");

  r = vfputc(0, vf);
  ASSERTEQ(r, 0, "vfputc");

  r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(r, 0, "vfseek");
  test_write_check(vf);
}

// Note: vf_printf is a wrapper for vf_vprintf, so both are tested by this.
static void test_vf_printf(vfile *vf)
{
  long pos = vftell(vf);

  int ret = vf_printf(vf, "%128.128s%.128s", (const char *)test_data, (const char *)test_data + 128);
  ASSERTEQ(ret, 255, "vf_printf");

  // *printf doesn't write a null terminator.
  ret = vfputc(0, vf);
  ASSERTEQ(ret, 0, "vfputc");

  ret = vfseek(vf, pos, SEEK_SET);
  ASSERTEQ(ret, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfseek_vftell_vrewind_read(vfile *vf)
{
  static constexpr int set_valid[] = { 0, 128, 256, 7, 19, 157, 79, 9999 };
  static constexpr int cur_valid[] = { 0, 128, 64, 32, 16, -16, -32, -64, -128 }; // Cumulative.
  static constexpr int set_cur_invalid[] = { -15, -120312, -1 };
  static constexpr int end_valid[] = { 0, -1, -5, -20, -256, 10, 9999 };
  static constexpr int end_invalid[] = { -257, -2000, -127848, -2391231 };
  static constexpr int len = sizeof(test_data);

  long expected;
  long pos;
  int ret;

  // SEEK_SET (valid).
  for(int value : set_valid)
  {
    ret = vfseek(vf, value, SEEK_SET);
    pos = vftell(vf);
    ASSERTEQ(ret, 0, "SEEK_SET valid: %d", value);
    ASSERTEQ(pos, value, "SEEK_SET valid: %d", value);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQ(pos, 0, "vrewind after SEEK_SET");

  // SEEK_CUR (valid).
  expected = 0;
  for(int value : cur_valid)
  {
    expected += value;
    ret = vfseek(vf, value, SEEK_CUR);
    pos = vftell(vf);
    ASSERTEQ(ret, 0, "SEEK_CUR valid: %d", value);
    ASSERTEQ(pos, expected, "SEEK_CUR valid: %d", value);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQ(pos, 0, "vrewind after SEEK_CUR");

  // SEEK_SET and SEEK_CUR (invalid).
  for(int value : set_cur_invalid)
  {
    ret = vfseek(vf, value, SEEK_SET);
    ASSERT(ret != 0, "SEEK_SET invalid: %d", value);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQ(pos, 0, "SEEK_SET invalid: %d", value);

    ret = vfseek(vf, value, SEEK_CUR);
    ASSERT(ret != 0, "SEEK_CUR invalid: %d", value);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQ(pos, 0, "SEEK_CUR invalid: %d", value);
  }

  // SEEK_END (valid).
  for(int value : end_valid)
  {
    ret = vfseek(vf, value, SEEK_END);
    pos = vftell(vf);
    ASSERTEQ(ret, 0, "SEEK_END valid: %d", value);
    ASSERTEQ(pos, len + value, "SEEK_END valid: %d", value);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQ(pos, 0, "vrewind after SEEK_END");

  // SEEK_END (invalid).
  for(int value : end_invalid)
  {
    ret = vfseek(vf, value, SEEK_END);
    ASSERT(ret != 0, "SEEK_END invalid: %d", value);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQ(pos, 0, "SEEK_END invalid: %d", value);
  }
}

static void test_filelength(long expected_len, vfile *vf)
{
  long pos = vftell(vf);
  long len = vfilelength(vf, false);
  long newpos = vftell(vf);

  ASSERTEQ(len, expected_len, "vfilelength");
  ASSERTEQ(pos, newpos, "vfilelength with no rewind");

  if(len >> 1)
  {
    vfseek(vf, len >> 1, SEEK_SET);
    len = vfilelength(vf, true);
    newpos = vftell(vf);

    ASSERTEQ(len, expected_len, "vfilelength 2");
    ASSERTEQ(newpos, 0, "vfilelength with rewind");
  }
}

// Make sure vfilelength accurately updates to match the current file position.
// NOTE: this may not work on append files...
static void test_filelength_write(vfile *vf)
{
  long pos = vftell(vf);
  long len;
  long size;
  int ret;

  // New file or appending to file, length should be vftell().
  len = vfilelength(vf, false);
  ASSERTEQ(len, pos, "vfilelength == vftell (1)");

  // Write first half.
  size = sizeof(test_data)/2;
  ret = vfwrite(test_data, size, 1, vf);
  ASSERTEQ(ret, 1, "vfwrite (1)");

  len = vfilelength(vf, false);
  ASSERTEQ(len, size + pos, "vfilelength (2)");
  ASSERTEQ(vftell(vf), size + pos, "vftell (2)");

  // Write some more junk.
  ret = vfputc(0x12, vf);
  ASSERTEQ(ret, 0x12, "");
  ret = vfputc(0x34, vf);
  ASSERTEQ(ret, 0x34, "");
  ret = vfputw(0x7856, vf);
  ASSERTEQ(ret, 0x7856, "");

  size += 4;
  len = vfilelength(vf, false);
  ASSERTEQ(len, size + pos, "vfilelength (3)");
  ASSERTEQ(vftell(vf), size + pos, "vftell (3)");
}

static void test_vungetc(vfile *vf)
{
  long pos;
  int ret;
  int64_t value;
  int64_t first_qword;
  char next_64[64];
  char last_5[5];
  char tmp[64];
  char *retstr;

  first_qword = vfgetq(vf);
  vfread(next_64, 64, 1, vf);
  vfseek(vf, sizeof(test_data) - 5, SEEK_SET);
  vfread(last_5, 5, 1, vf);
  vrewind(vf);

  // vungetc should fail if EOF or some other junk is provided.
  // Note: MSVCRT stdio &255s non-EOF values (?).
  ret = vungetc(EOF, vf);
  ASSERTEQ(ret, EOF, "vungetc EOF");
  ret = vungetc(-128141, vf);
  ASSERTEQ(ret, EOF, "vungetc -128141");
  ret = vungetc(256, vf);
  ASSERTEQ(ret, EOF, "vungetc 256");

  // vfgetc should read the buffered char.
  ret = vungetc(0xAB, vf);
  ASSERTEQ(ret, 0xAB, "vungetc 0xAB");
  value = vfgetc(vf);
  ASSERTEQ(value, 0xAB, "vfgetc 0xAB");

  // vfgetw should read the buffered char + one char from the data.
  ret = vungetc(0xCD, vf);
  ASSERTEQ(ret, 0xCD, "vungetc 0xCD");
  value = vfgetw(vf);
  ASSERTEQ(value, 0xCD | ((first_qword & 0xFF) << 8), "vfgetw 0xCD ...");

  // vfgetd should read the buffered char + three chars from the data.
  ret = vungetc(0xEF, vf);
  ASSERTEQ(ret, 0xEF, "vungetc 0xEF");
  value = vfgetd(vf);
  ASSERTEQ(value, 0xEF | (first_qword & 0xFFFFFF00), "vfgetd 0xEF ...");

  // vfgetq should read the buffered char + seven chars from the data.
  ret = vfseek(vf, 1, SEEK_SET);
  ASSERTEQ(ret, 0, "vungetc 0x69");
  ret = vungetc(0x69, vf);
  ASSERTEQ(ret, 0x69, "vungetc 0x69");
  value = vfgetq(vf);
  ASSERTEQ(value, 0x69 | (first_qword & ~0xFF), "vfgetq 0x69 ...");

  // vfread should read the buffered char + n-1 chars from the data.
  ret = vungetc(0x12, vf);
  ASSERTEQ(ret, 0x12, "vungetc 0x12");
  value = vfread(tmp, 64, 1, vf);
  ASSERTEQ(value, 1, "vfread 0x12 ...");
  ASSERTEQ(tmp[0], 0x12, "vfread 0x12 ...");
  ASSERTMEM(tmp + 1, next_64, 63, "vfread 0x12 ...");

  // vfsafegets should read the buffered char + n-1 chars from the data.
  vfseek(vf, sizeof(test_data) - 5, SEEK_SET);
  ret = vungetc(0xFF, vf);
  ASSERTEQ(ret, 0xFF, "vungetc 0xFF");
  retstr = vfsafegets(tmp, sizeof(tmp), vf);
  ASSERT(retstr, "vfsafegets 0xFF ...");
  ASSERTEQ(tmp[0], 0xFF, "vfsafegets 0xFF ...");
  ASSERTMEM(tmp + 1, last_5, 5, "vfsafegets 0xFF ...");

  // If the buffer char is \r and the next char in the stream is \n, consume both.
  vfseek(vf, 0, SEEK_SET);
  ret = vungetc(0x0D, vf);
  ASSERTEQ(ret, 0x0D, "vungetc 0x0D");
  retstr = vfsafegets(tmp, sizeof(tmp), vf);
  ASSERT(retstr, "vfsafegets 0x0D ...");
  ASSERTEQ((int)tmp[0], 0, "vfsafegets 0x0D ...");
  pos = vftell(vf);
  ASSERTEQ(pos, 1, "vfsafegets 0x0D ...");

  // Reading a buffer char from the end of the file should not return NULL.
  vfseek(vf, 0, SEEK_END);
  ret = vungetc('a', vf);
  ASSERTEQ(ret, 'a', "vungetc 'a'");
  retstr = vfsafegets(tmp, sizeof(tmp), vf);
  ASSERT(retstr, "vsafegets 'a'");
  ASSERTCMP(retstr, "a", "vsafegets 'a'");
  pos = vftell(vf);
  ASSERTEQ(pos, sizeof(test_data), "vsafegets 'a'");

  // vseek should discard the buffered char.
  ret = vungetc(0x34, vf);
  ASSERTEQ(ret, 0x34, "vungetc 0x34");
  vfseek(vf, 128, SEEK_SET);
  value = vfgetc(vf);
  ASSERTEQ(value, test_data[128], "vfgetc NOT 0x34");

  // vrewind should discard the buffered char.
  ret = vungetc(0x56, vf);
  ASSERTEQ(ret, 0x56, "vungetc 0x56");
  vrewind(vf);
  value = vfgetq(vf);
  ASSERTEQ(value, first_qword, "vfgetd NOT 0x56 ...");

  // vfilelength with rewind should discard the buffered char.
  ret = vungetc(0x78, vf);
  ASSERTEQ(ret, 0x78, "vungetc 0x78");
  long len = vfilelength(vf, true);
  ASSERTEQ(len, sizeof(test_data), "vfgetd NOT 0x78");
  value = vfgetq(vf);
  ASSERTEQ(value, first_qword, "vfgetd NOT 0x78");

  // ftell should subtract the buffered char count from the cursor for binary
  // streams. Calling ftell when (cursor - buffered char count)<0 is undefined,
  // so read a char after seek (avoids Wine bugs not present in Windows).
  ret = vfseek(vf, 127, SEEK_SET);
  ASSERTEQ(ret, 0, "vfseek");
  ret = vfgetc(vf);
  ASSERTEQ(ret, 0x16, "vfgetc 0x16");
  ret = vungetc(0x9A, vf);
  ASSERTEQ(ret, 0x9A, "vungetc 0x9A");
  pos = vftell(vf);
  ASSERTEQ(pos, 127, "vftell after vungetc");
}

static void test_vfsafegets(vfile *vf, const char *filename,
 const char * const (&output)[MAX_LINES])
{
  char buffer[VFSAFEGETS_BUFFER];
  char *cursor;
  int line = 0;

  while((cursor = vfsafegets(buffer, VFSAFEGETS_BUFFER, vf)))
  {
    ASSERT(line < MAX_LINES - 1, "%s:%d", filename, line);
    ASSERT(output[line], "%s:%d", filename, line);
    ASSERTCMP(buffer, output[line], "%s:%d", filename, line);
    line++;
  }
  ASSERT(!output[line], "");
}

#define READ_TESTS(vf) do { \
  SECTION(read_vfgetc)      test_vfgetc(vf); \
  SECTION(read_vfgetw)      test_vfgetw(vf); \
  SECTION(read_vfgetd)      test_vfgetd(vf); \
  SECTION(read_vfgetq)      test_vfgetq(vf); \
  SECTION(read_vfread)      test_vfread(vf); \
  SECTION(read_vfseektell)  test_vfseek_vftell_vrewind_read(vf); \
  SECTION(read_filelength)  test_filelength(sizeof(test_data), vf); \
  SECTION(read_vungetc)     test_vungetc(vf); \
} while(0)

#define WRITE_TESTS(vf, is_append) do { \
  SECTION(write_vfputc)     test_vfputc(vf); \
  SECTION(write_vfputw)     test_vfputw(vf); \
  SECTION(write_vfputd)     test_vfputd(vf); \
  SECTION(write_vfputq)     test_vfputq(vf); \
  SECTION(write_vfwrite)    test_vfwrite(vf); \
  SECTION(write_vfputs)     test_vfputs(vf); \
  SECTION(write_vf_printf)  test_vf_printf(vf); \
  SECTION(write_filelength) test_filelength_write(vf); \
  /*if(!is_append) { SECTION(write_vfseektell)  test_vfseek_vftell_rewind_write(vf); } */ \
} while(0)

static void exec_dir()
{
  int ret = chdir(execdir);
  ASSERTEQ(ret, 0, "change to exec dir");
}

static void write_dir()
{
  int ret = chdir(writedir);
  ASSERTEQ(ret, 0, "change to write dir");
}

UNITTEST(VioInit)
{
  char *t = getcwd(execdir, sizeof(execdir));
  ASSERTEQ(t, execdir, "");

  int ret = chdir(UNIT_TMP_DIR);
  ASSERTEQ(ret, 0, "change to write dir");

  t = getcwd(writedir, sizeof(writedir));
  ASSERTEQ(t, writedir, "");

  FILE *fp = fopen_unsafe(TEST_READ_FILENAME, "wb");
  ASSERT(fp, "fopen_unsafe");

  int r = fwrite(test_data, sizeof(test_data), 1, fp);
  ASSERTEQ(r, 1, "fwrite");

  r = fclose(fp);
  ASSERTEQ(r, 0, "fclose");

  for(const vfsafegets_data &d : vfsafegets_test_data)
  {
    fp = fopen_unsafe(d.filename, "wb");
    ASSERT(fp, "fopen_unsafe");

    r = fwrite(d.input, strlen(d.input), 1, fp);
    ASSERTEQ(r, 1, "fwrite");

    r = fclose(fp);
    ASSERTEQ(r, 0, "fclose");
  }
}

UNITTEST(ModeFlags)
{
  struct mode_flag_pairs
  {
    const char *mode;
    int expected;
  };

  static const mode_flag_pairs data[] =
  {
    { "r", VF_READ },
    { "w", VF_WRITE | VF_TRUNCATE },
    { "a", VF_WRITE | VF_APPEND },
    { "r+", VF_READ | VF_WRITE },
    { "w+", VF_READ | VF_WRITE | VF_TRUNCATE },
    { "a+", VF_READ | VF_WRITE | VF_APPEND },
    { "rb", VF_READ | VF_BINARY },
    { "wb", VF_WRITE | VF_TRUNCATE | VF_BINARY },
    { "ab", VF_WRITE | VF_APPEND | VF_BINARY },
    // Both orderings of '+' and 'b' are valid.
    { "r+b", VF_READ | VF_WRITE | VF_BINARY },
    { "w+b", VF_READ | VF_WRITE | VF_TRUNCATE | VF_BINARY },
    { "a+b", VF_READ | VF_WRITE | VF_APPEND | VF_BINARY },
    { "rb+", VF_READ | VF_WRITE | VF_BINARY },
    { "wb+", VF_READ | VF_WRITE | VF_TRUNCATE | VF_BINARY },
    { "ab+", VF_READ | VF_WRITE | VF_APPEND | VF_BINARY },
    // 't' to explicitly state text mode is non-standard but seems to be well-supported.
    { "rt", VF_READ },
    { "wt", VF_WRITE | VF_TRUNCATE },
    { "at", VF_WRITE | VF_APPEND },
    { "r+t", VF_READ | VF_WRITE },
    { "w+t", VF_READ | VF_WRITE | VF_TRUNCATE },
    { "a+t", VF_READ | VF_WRITE | VF_APPEND },
    // 'r'/'w'/'a' must be first.
    { "br", 0 },
    { "bw", 0 },
    { "ba", 0 },
    { "+r", 0 },
    { "+w", 0 },
    { "+a", 0 },
    { "tr", 0 },
    // Other invalid junk.
    { "+", 0 },
    { "b", 0 },
    { "t", 0 },
    { "    ", 0 },
    { "fjdskfdj", 0 },
    { "", 0 },
  };

  for(const mode_flag_pairs &d : data)
  {
    int res = vfile_get_mode_flags(d.mode);
    ASSERTEQ(res, d.expected, "%s", d.mode);
  }
}

UNITTEST(FileRead)
{
  write_dir();

  ScopedFile<vfile, vfclose> vf_in =
   vfopen_unsafe_ext(TEST_READ_FILENAME, "rb", V_SMALL_BUFFER);
  ASSERT(vf_in, "");
  ASSERT(~vfile_get_flags(vf_in) & VF_VIRTUAL, "");
  READ_TESTS(vf_in);
}

UNITTEST(FileWrite)
{
  write_dir();

  ScopedFile<vfile, vfclose> vf_out = vfopen_unsafe(TEST_WRITE_FILENAME, "w+b");
  ASSERT(vf_out, "");
  ASSERT(~vfile_get_flags(vf_out) & VF_VIRTUAL, "");
  WRITE_TESTS(vf_out, false);
}

UNITTEST(FileAppend)
{
  write_dir();

  ScopedFile<vfile, vfclose> vf_out = vfopen_unsafe(TEST_WRITE_FILENAME, "a+b");
  ASSERT(vf_out, "");
  ASSERT(~vfile_get_flags(vf_out) & VF_VIRTUAL, "");
  // Align the read cursor with the write cursor.
  vfseek(vf_out, 0, SEEK_END);
  WRITE_TESTS(vf_out, true);
}

UNITTEST(MemoryRead)
{
  char buffer[sizeof(test_data)];
  memcpy(buffer, test_data, sizeof(test_data));
  ScopedFile<vfile, vfclose> vf_in = vfile_init_mem(buffer, sizeof(test_data), "rb");
  ASSERT(vf_in, "");
  ASSERT(~vfile_get_flags(vf_in) & VF_VIRTUAL, "");
  READ_TESTS(vf_in);
}

UNITTEST(MemoryWrite)
{
  static constexpr int len = sizeof(test_data);
  char buffer[len];
  int ret;

  ScopedFile<vfile, vfclose> vf_out = vfile_init_mem(buffer, len, "w+b");
  ASSERT(vf_out, "");
  ASSERT(~vfile_get_flags(vf_out) & VF_VIRTUAL, "");
  WRITE_TESTS(vf_out, false);

  SECTION(NoWritePastEnd)
  {
    /* Make sure writes that would exceed a fixed size buffer are refused. */
    vfseek(vf_out, len, SEEK_SET);
    ret = vfputc(0xAB, vf_out);
    ASSERTEQ(ret, EOF, "");

    vfseek(vf_out, len - 1, SEEK_SET);
    ret = vfputw(0xCD, vf_out);
    ASSERTEQ(ret, EOF, "");

    vfseek(vf_out, len - 3, SEEK_SET);
    ret = vfputd(0xEF, vf_out);
    ASSERTEQ(ret, EOF, "");

    static constexpr char tmp[] = "abcdefghij";
    vfseek(vf_out, len - 7, SEEK_SET);
    ret = vfputs(tmp, vf_out);
    ASSERTEQ(ret, EOF, "");

    vfseek(vf_out, 128, SEEK_SET);
    ret = vfwrite(test_data, len, 1, vf_out);
    ASSERTEQ(ret, 0, "");
  }
}

/* Make sure buffer expansion works correctly. */
UNITTEST(MemoryWriteExt)
{
  void *buffer = nullptr;
  size_t size = 0;

  ScopedFile<vfile, vfclose> vf_out = vfile_init_mem_ext(&buffer, &size, "w+b");
  ASSERT(vf_out, "");
  ASSERT(~vfile_get_flags(vf_out) & VF_VIRTUAL, "");
  WRITE_TESTS(vf_out, false);
  free(buffer);
}

/* That goes for append mode too... */
UNITTEST(MemoryAppendExt)
{
  static constexpr size_t BUF_SIZE = 256;
  void *buffer = malloc(BUF_SIZE);
  size_t size = BUF_SIZE;
  ASSERT(buffer, "");

  {
    ScopedFile<vfile, vfclose> vf_out = vfile_init_mem_ext(&buffer, &size, "a+b");
    ASSERT(vf_out, "");
    ASSERT(~vfile_get_flags(vf_out) & VF_VIRTUAL, "");
    // Align the read cursor with the write cursor.
    vfseek(vf_out, 0, SEEK_END);
    WRITE_TESTS(vf_out, true);
  }
  free(buffer);
  if(this->expected_section)
    ASSERTEQ(size, sizeof(test_data) + BUF_SIZE, "");
}

UNITTEST(vfsafegets)
{
  write_dir();

  SECTION(FileBinary)
  {
    for(const vfsafegets_data &d : vfsafegets_test_data)
    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe_ext(d.filename, "rb", V_SMALL_BUFFER);
      ASSERT(vf, "");
      test_vfsafegets(vf, d.filename, d.output);
    }
  }

  SECTION(FileText)
  {
    // In practice, nothing in MZX uses text streams (intentionally) because
    // even most MZX text files can contain binary chars. See:
    // https://www.digitalmzx.com/forums/index.php?app=tracker&showissue=592
    for(const vfsafegets_data &d : vfsafegets_test_data)
    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(d.filename, "r");
      ASSERT(vf, "");
      test_vfsafegets(vf, d.filename, d.output);
    }
  }

  SECTION(Memory)
  {
    // Memory vfiles don't bother implementing text mode so just assume binary.
    for(const vfsafegets_data &d : vfsafegets_test_data)
    {
      size_t len = strlen(d.input);
      ScopedPtr<char[]> tmp = new char[len + 1];

      memcpy(tmp.get(), d.input, len + 1);
      ScopedFile<vfile, vfclose> vf = vfile_init_mem(tmp.get(), len, "rb");
      ASSERT(vf, "");
      test_vfsafegets(vf, d.filename, d.output);
    }
  }
}

UNITTEST(vtempfile)
{
  write_dir();

  SECTION(File)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(0);
    ASSERT(vf, "");
    test_vfwrite(vf);
  }

  SECTION(Memory)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(sizeof(test_data));
    ASSERT(vf, "");
    test_vfwrite(vf);
  }

  SECTION(MemorySmallInit)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(1);
    ASSERT(vf, "");
    test_vfputc(vf);
  }
}

UNITTEST(Filesystem)
{
  static constexpr char TEST_RENAME_FILENAME[] = "VFILE_TEST_dfbdfbshd";
  static constexpr char TEST_RENAME_DIR[] = "VFILE_TEST_DIR_ndfjsdbnfjdfd";
  struct stat stat_info{}; // 0-init to silence MemorySanitizer.
  int ret;

  write_dir();

  SECTION(vchdir)
  {
    exec_dir();
    ret = vchdir("unit/io/data");
    ASSERTEQ(ret, 0, "");

    ScopedFile<vfile, vfclose> vf = vfopen_unsafe("CT_LEVEL.MOD", "rb");
    ASSERT(vf, "");
    long len = vfilelength(vf, false);
    ASSERTEQ(len, 111885, "");
  }

  SECTION(vgetcwd)
  {
    char buffer[1024];
    char buffer2[1024];
    char *t = vgetcwd(buffer, sizeof(buffer));
    ASSERTEQ(t, buffer, "");
    ret = vchdir("..");
    ASSERTEQ(ret, 0, "");
    t = vgetcwd(buffer2, sizeof(buffer2));
    ASSERTEQ(t, buffer2, "");
    size_t len = strlen(buffer2);
    ASSERT(len < sizeof(buffer), "");
    ASSERTMEM(buffer, buffer2, len, "");
  }

  SECTION(vmkdir)
  {
    ret = vmkdir(TEST_DIR, 0777);
    ASSERTEQ(ret, 0, "");
    ret = stat(TEST_DIR, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISDIR(stat_info.st_mode), "");
  }

  SECTION(vunlink)
  {
    ret = stat(TEST_WRITE_FILENAME, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISREG(stat_info.st_mode), "");
    ret = vunlink(TEST_WRITE_FILENAME);
    ASSERTEQ(ret, 0, "");
    ret = stat(TEST_WRITE_FILENAME, &stat_info);
    ASSERT(ret != 0, "");
  }

  SECTION(vrmdir)
  {
    ret = stat(TEST_DIR, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISDIR(stat_info.st_mode), "");
    ret = vrmdir(TEST_DIR);
    ASSERTEQ(ret, 0, "");
    ret = stat(TEST_DIR, &stat_info);
    ASSERT(ret != 0, "");
  }

  SECTION(vaccess)
  {
#ifndef _WIN32
    static constexpr int access_flags = R_OK|W_OK|X_OK;
#else
    static constexpr int access_flags = R_OK|W_OK;
#endif
    ret = access(".", access_flags);
    ASSERTEQ(ret, 0, "");
    ret = vaccess(".", R_OK|W_OK|X_OK);
    ASSERTEQ(ret, 0, "");

    exec_dir();
    ret = vchdir("unit/io/data");
    ASSERTEQ(ret, 0, "");

    ret = access("CT_LEVEL.MOD", R_OK|W_OK);
    ASSERTEQ(ret, 0, "");
    ret = vaccess("CT_LEVEL.MOD", R_OK|W_OK);
    ASSERTEQ(ret, 0, "");
  }

  SECTION(vstat)
  {
    struct stat stat_info2{};
    ret = stat(".", &stat_info);
    ASSERTEQ(ret, 0, "");
    ret = vstat(".", &stat_info2);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISDIR(stat_info.st_mode), "");
    ASSERT(S_ISDIR(stat_info2.st_mode), "");

    exec_dir();
    ret = vchdir("unit/io/data");
    ASSERTEQ(ret, 0, "");

    ret = stat("CT_LEVEL.MOD", &stat_info);
    ASSERTEQ(ret, 0, "");
    ret = vstat("CT_LEVEL.MOD", &stat_info2);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISREG(stat_info.st_mode), "");
    ASSERT(S_ISREG(stat_info2.st_mode), "");
  }

  SECTION(vrename)
  {
    char buffer[1024];

    // Clean up from any previous tests...
    snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_RENAME_DIR, TEST_RENAME_FILENAME);
    vunlink(buffer);
    snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIR, TEST_RENAME_FILENAME);
    vunlink(buffer);
    vrmdir(TEST_RENAME_DIR);
    vrmdir(TEST_DIR);

    ret = vmkdir(TEST_DIR, 0777);
    ASSERTEQ(ret, 0, "");

    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_WRITE_FILENAME, "wb");
      ASSERT(vf, "");
    }

    ret = vstat(TEST_WRITE_FILENAME, &stat_info);
    ASSERTEQ(ret, 0, "");

    // Rename file.
    ret = vrename(TEST_WRITE_FILENAME, buffer);
    ASSERTEQ(ret, 0, "");

    ret = vstat(buffer, &stat_info);
    ASSERTEQ(ret, 0, "");

    // Rename dir.
    ret = vrename(TEST_DIR, TEST_RENAME_DIR);
    ASSERTEQ(ret, 0, "");

    ret = vstat(TEST_RENAME_DIR, &stat_info);
    ASSERTEQ(ret, 0, "");

    // Renamed filename still in dir?
    ret = vchdir(TEST_RENAME_DIR);
    ASSERTEQ(ret, 0, "");

    ret = vstat(TEST_RENAME_FILENAME, &stat_info);
    ASSERTEQ(ret, 0, "");
  }

  SECTION(UTF8)
  {
    static constexpr int utf8_dir_len = sizeof(UTF8_DIR) - 1;

    ret = vstat(UTF8_DIR, &stat_info);
    if(!ret)
    {
      ret = vchdir(UTF8_DIR);
      ASSERTEQ(ret, 0, "");
      ret = vunlink(UTF8_FILE);
      ret = vunlink(UTF8_FILE2);
      ret = vchdir("..");
      ASSERTEQ(ret, 0, "");
      ret = vrmdir(UTF8_DIR);
      ASSERTEQ(ret, 0, "");
    }

    ret = vmkdir(UTF8_DIR, 0777);
    ASSERTEQ(ret, 0, "");

    ret = vstat(UTF8_DIR, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERT(S_ISDIR(stat_info.st_mode), "");

    ret = vchdir(UTF8_DIR);
    ASSERTEQ(ret, 0, "");
    char buffer[1024];

    char *t = vgetcwd(buffer, sizeof(buffer));
    ASSERTEQ(t, buffer, "");
    size_t len = strlen(buffer);
    ASSERTCMP(buffer + len - utf8_dir_len, UTF8_DIR, "");

    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(UTF8_FILE, "wb");
      ASSERT(vf, "");
      ret = vfwrite(test_data, sizeof(test_data), 1, vf);
      ASSERTEQ(ret, 1, "");
    }

    ret = vstat(UTF8_FILE, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERTEQ(stat_info.st_size, sizeof(test_data), "");
    ASSERT(S_ISREG(stat_info.st_mode), "");

    ret = vaccess(UTF8_FILE, R_OK|W_OK);
    ASSERTEQ(ret, 0, "");

    ret = vrename(UTF8_FILE, UTF8_FILE2);
    ASSERTEQ(ret, 0, "");

    ret = vstat(UTF8_FILE2, &stat_info);
    ASSERTEQ(ret, 0, "");
    ASSERTEQ(stat_info.st_size, sizeof(test_data), "");
    ASSERT(S_ISREG(stat_info.st_mode), "");

    ret = vunlink(UTF8_FILE2);
    ASSERTEQ(ret, 0, "");

    ret = vchdir("..");
    ASSERTEQ(ret, 0, "");

    ret = vrmdir(UTF8_DIR);
    ASSERTEQ(ret, 0, "");
  }
}

/**
 * Test dirent replacement functions separately from the other filesystem
 * functions, including UTF-8 cases.
 *
 * Note: files returned by dirent seem fairly platform-dependent, so filter
 * out anything starting with . for this test. Also, don't rely on the results
 * being ordered, because Linux doesn't sort them like Windows does.
 *
 * FIXME: should test the d_type return value too.
 */

template<long N>
void test_dir_contents(const char *dirname, const char * const (&expected)[N],
 int flags = 0)
{
  boolean has_expected[N]{};
  char buffer[1024];
  boolean ret;
  long length;
  long i;

  ScopedFile<vdir, vdir_close> dir = vdir_open_ext(dirname, flags);
  ASSERT(dir != NULL, "vdir_open(%s) failed", dirname);

  length = vdir_length(dir);
  ASSERTEQ(vdir_tell(dir), 0, "vdir_tell should be 0 after open: %s", dirname);

  while(vdir_read(dir, buffer, sizeof(buffer), NULL))
  {
    if(buffer[0] == '.')
      continue;

    for(i = 0; i < N; i++)
    {
      if(expected[i] && !strcmp(buffer, expected[i]))
      {
        has_expected[i] = true;
        break;
      }
    }
    if(i == N)
      FAIL("unexpected file '%s' found in '%s'!", buffer, dirname);
  }
  if(flags & VDIR_NO_SCAN)
    goto skip_scan_checks;

  // At EOF-vdir_length and vdir_tell should be equal.
  ASSERTEQ(vdir_tell(dir), length, "vdir_tell should be length after open: %s", dirname);

  ret = vdir_read(dir, NULL, 0, NULL);
  ASSERTEQ(ret, false, "vdir_read at directory EOF should return false: %s", dirname);

  // Seek back to start.
  ret = vdir_seek(dir, 0);
  ASSERTEQ(ret, true, "vdir_seek should return true: %s", dirname);
  ASSERTEQ(vdir_tell(dir), 0, "vdir_tell after seek back to start should be 0: %s", dirname);

  if(length >= 2)
  {
    ret = vdir_seek(dir, length / 2);
    ASSERTEQ(ret, true, "vdir_seek should return true: %s", dirname);
    ASSERTEQ(vdir_tell(dir), length / 2, "vdir_tell failed: %s", dirname);

    ret = vdir_seek(dir, length);
    ASSERTEQ(ret, true, "vdir_seek should return true: %s", dirname);
    ASSERTEQ(vdir_tell(dir), length, "vdir_tell failed: %s", dirname);

    ret = vdir_seek(dir, -19824);
    ASSERTEQ(ret, false, "vdir_seek should reject invalid negative seek value: %s", dirname);
    ASSERTEQ(vdir_tell(dir), length, "vdir_tell should be unchanged: %s", dirname);

    ret = vdir_seek(dir, length + 10);
    ASSERTEQ(ret, true, "vdir_seek past end should be successful: %s", dirname);
    ASSERTEQ(vdir_tell(dir), length, "vdir_seek past end should seek to end: %s", dirname);
  }

  // Rewind back to start.
  ret = vdir_rewind(dir);
  ASSERTEQ(ret, true, "vdir_rewind should return true: %s", dirname);
  ASSERTEQ(vdir_tell(dir), 0, "vdir_tell after rewind back to start should be 0: %s", dirname);

skip_scan_checks:
  for(i = 0; i < N; i++)
  {
    if(!expected[i])
      continue;

    if(!has_expected[i])
      FAIL("missing expected file '%s' from '%s'!", expected[i], dirname);
  }
}

UNITTEST(dirent)
{
  static const char TEST_DIRENT_DIR[]     = "VFILE_TEST_DIRENT";
  static const char TEST_DIRENT_DIR_UTF[] = u8"VFILE_TEST_DIRENT_" UTF8_DIR_STRING;

  static const char *TEST_DIRENT_EMPTY[] = { nullptr };
  static const char *TEST_DIRENT_NONEMPTY[] =
  {
    "file1",
    "file2",
    UTF8_FILE,
    UTF8_FILE2,
  };

  // Cleanup from prior tests.
  char buffer[1024];
  int ret;

  write_dir();

  for(const char *filename : TEST_DIRENT_NONEMPTY)
  {
    snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIRENT_DIR, filename);
    vunlink(buffer);
    snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIRENT_DIR_UTF, filename);
    vunlink(buffer);
  }
  vrmdir(TEST_DIRENT_DIR);
  vrmdir(TEST_DIRENT_DIR_UTF);

  ret = vmkdir(TEST_DIRENT_DIR, 0777);
  ASSERTEQ(ret, 0, "");
  ret = vmkdir(TEST_DIRENT_DIR_UTF, 0777);
  ASSERTEQ(ret, 0, "");

  SECTION(Empty)
  {
    test_dir_contents(TEST_DIRENT_DIR, TEST_DIRENT_EMPTY);
  }

  SECTION(EmptyNoScan)
  {
    test_dir_contents(TEST_DIRENT_DIR, TEST_DIRENT_EMPTY, VDIR_NO_SCAN);
  }

  SECTION(NonEmpty)
  {
    for(const char *filename : TEST_DIRENT_NONEMPTY)
    {
      snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIRENT_DIR, filename);
      vfile *tmp = vfopen_unsafe(buffer, "wb");
      ASSERT(tmp, "failed to create file '%s'", buffer);
      vfclose(tmp);
    }

    test_dir_contents(TEST_DIRENT_DIR, TEST_DIRENT_NONEMPTY);
  }

  SECTION(NonEmptyNoScan)
  {
    for(const char *filename : TEST_DIRENT_NONEMPTY)
    {
      snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIRENT_DIR, filename);
      vfile *tmp = vfopen_unsafe(buffer, "wb");
      ASSERT(tmp, "failed to create file '%s'", buffer);
      vfclose(tmp);
    }
    test_dir_contents(TEST_DIRENT_DIR, TEST_DIRENT_NONEMPTY, VDIR_NO_SCAN);
  }

  SECTION(EmptyUTF8)
  {
    test_dir_contents(TEST_DIRENT_DIR_UTF, TEST_DIRENT_EMPTY);
  }

  SECTION(NonEmptyUTF8)
  {
    for(const char *filename : TEST_DIRENT_NONEMPTY)
    {
      snprintf(buffer, sizeof(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIRENT_DIR_UTF, filename);
      vfile *tmp = vfopen_unsafe(buffer, "wb");
      ASSERT(tmp, "failed to create file '%s'", buffer);
      vfclose(tmp);
    }

    test_dir_contents(TEST_DIRENT_DIR_UTF, TEST_DIRENT_NONEMPTY);
  }
}

static constexpr const char *expected_real_roots[] =
{
#if defined(_WIN32) || defined(CONFIG_DJGPP)
  /* The only guaranteed root for these is C: */
  "C:",
#elif defined(CONFIG_AMIGA)
  "sys:",
#else
  DIR_SEPARATOR
#endif
};

UNITTEST(vvolumelist)
{
  boolean found[arraysize(expected_real_roots)]{};

  ScopedFile<vvolumelist, vvolumelist_close> volumes = vvolumelist_open();
  const char *file;

  ASSERT(volumes, "failed to open volume list");

  while((file = vvolumelist_read(volumes)))
  {
    for(int i = 0; i < arraysize(expected_real_roots); i++)
    {
      if(!strcasecmp(file, expected_real_roots[i]))
      {
        ASSERT(!found[i], "root '%s' duplicated in volume list", expected_real_roots[i]);
        found[i] = true;
      }
    }
  }
  for(int i = 0; i < arraysize(found); i++)
    ASSERT(found[i], "root '%s' missing from volume list", expected_real_roots[i]);
}


/**********************************************************************
 * Virtual filesystem tests.
 */

static constexpr size_t cache_max_size = 16384;

class vfssetup
{
public:
  vfssetup(boolean enable_cache)
  {
    // Safety--purge the VFS if it exists first.
    vio_filesystem_exit();

    if(enable_cache)
    {
      boolean ret = vio_filesystem_init(cache_max_size, cache_max_size / 2, true);
      ASSERT(ret, "");
    }
    else
    {
      boolean ret = vio_filesystem_init(0, 0, false);
      ASSERT(ret, "");
    }
  }
  ~vfssetup() { vio_filesystem_exit(); }
};

UNITTEST(VirtualRead)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr char read_file[] = "virtual_read.bin";
  size_t sz;
  int ret;

  write_dir();

  vfssetup a(false);

  ret = vio_virtual_file(read_file);
  ASSERT(ret, "");

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(read_file, "w+b");
  ASSERT(vf, "");

  int flags = vfile_get_flags(vf);
  ASSERT(flags & VF_MEMORY, "");
  ASSERT(flags & VF_VIRTUAL, "");

  // Sadly one write is required currently :(
  sz = vfwrite(test_data, 1, sizeof(test_data), vf);
  ASSERTEQ(sz, sizeof(test_data), "");
  vrewind(vf);

  READ_TESTS(vf);
}

UNITTEST(VirtualWrite)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr char write_file[] = "virtual_write.bin";

  vfssetup a(false);
  int ret;

  write_dir();

  ret = vio_virtual_file(write_file);
  ASSERT(ret, "");

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(write_file, "w+b");
  ASSERT(vf, "");

  int flags = vfile_get_flags(vf);
  ASSERT(flags & VF_MEMORY, "");
  ASSERT(flags & VF_VIRTUAL, "");

  WRITE_TESTS(vf, false);
  vf.reset();

  ScopedFile<vfile, vfclose> vf_trunc = vfopen_unsafe(write_file, "wb");
  ASSERT(vf_trunc, "");
  int64_t len = vfilelength(vf_trunc, false);
  ASSERTEQ(len, 0, "mode 'wb' should truncate virtual files");
}

UNITTEST(VirtualFilesystem)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  /* Tests most filesystem aspects of virtual (not cached) files and
   * directories, plus most aspects of cached directories (which are
   * always created as they are required for virtual file support).
   */
  static constexpr char file_virt[] = "vfile_please_dont_make_a_real_file.txt";
  static constexpr char file_over[] = "overlay.txt";
  static constexpr char dir_virt[] = "vdir_please_dont_make_a_real_dir";
  static constexpr char dir_over[] = "overlay";
  static constexpr char dir_real[] = "real_dir";
  static constexpr char dir_real_file[] = "real_dir/file";

  static constexpr struct
  {
    const char *path;
    int type;
    int rm_result;
  } data[] =
  {
    { file_virt,                    S_IFREG, 0 },
    { file_over,                    S_IFREG, 0 },
    { dir_virt,                     S_IFDIR, 0 },
    { dir_over,                     S_IFDIR, 0 },
    { "../data/vdir",               S_IFDIR, ENOTEMPTY },
    { "../data/vdir/vdir2",         S_IFDIR, ENOTEMPTY },
    { "../data/vdir/vfile",         S_IFREG, 0 },
    { "../data/vdir/vdir2/vfile2",  S_IFREG, 0 },
  };
  struct stat st{}; // 0-init to silence MemorySanitizer.
  char buf[MAX_PATH];
  char *t;
  FILE *fp;
  int ret;

  // Safety--some files need to be uncached for testing.
  vio_filesystem_exit();

  write_dir();

  // Make a real directory with no overlay or cache initially.
  vunlink(dir_real_file);
  vrmdir(dir_real);
  ret = vmkdir(dir_real, 0755);
  ASSERTEQ(ret, 0, "vmkdir an uncached directory");

  vfssetup a(false);

  // Make a virtual file. The filesystem functions should fully support it.
  ret = vio_virtual_file(file_virt);
  ASSERT(ret, "");

  // Make a virtual directory. The filesystem functions should fully support it.
  ret = vio_virtual_directory(dir_virt);
  ASSERT(ret, "");

  // Make a virtual file that is overlaid over a real file.
  // The filesystem functions should ignore the real file.
  fp = fopen_unsafe(file_over, "wb");
  ASSERT(fp, "");
  fclose(fp);
  ret = vio_virtual_file(file_over);
  ASSERT(ret, "");

  // Make a virtual directory that is overlaid over a real directory.
  // The filesystem functions should ignore the real directory, especially
  // e.g. vmkdir should make a virtual child of the virtual directory, but
  // not the real one.
  vrmdir(dir_over);
  ret = vmkdir(dir_over, 0755);
  ASSERTEQ(ret, 0, "");
  ret = vio_virtual_directory(dir_over);
  ASSERT(ret, "");

  // Make virtual directories and file that use relative path tokens.
  ret = vio_virtual_directory("../data");
  ASSERT(ret, "");
  ret = vio_virtual_directory("../data/vdir");
  ASSERT(ret, "");
  ret = vio_virtual_directory("../data/./vdir/./vdir2");
  ASSERT(ret, "");
  ret = vio_virtual_file("../data/vdir/./././vfile");
  ASSERT(ret, "");
  ret = vio_virtual_file("../data/vdir/vdir2/../vdir2/vfile2");
  ASSERT(ret, "");

  SECTION(vchdir_vgetcwd)
  {
    char expected[MAX_PATH];
    char expected2[MAX_PATH];
    char expected3[MAX_PATH];
    path_join(expected, MAX_PATH, writedir, dir_virt);
    path_join(expected2, MAX_PATH, writedir, dir_over);
    memcpy(expected3, writedir, MAX_PATH);
    path_navigate_no_check(expected3, MAX_PATH, "../data/vdir/vdir2");

    ret = vchdir(dir_virt);
    ASSERTEQ(ret, 0, "vchdir into virtual directory");
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "vgetcwd in virtual directory");
    ASSERTCMP(t, expected, "vgetcwd in virtual directory");

    // Should be able to change from the current (virtual) dir
    // into the real *uncached* directory. This is important to test
    // because the current directory of the VFS MUST be cached. Changing
    // into this directory should cache it.
    path_join(buf, MAX_PATH, "..", dir_real);
    ret = vchdir(buf);
    ASSERTEQ(ret, 0, "vchdir into uncached directory");
    char expected_real[MAX_PATH];
    path_join(expected_real, MAX_PATH, writedir, dir_real);
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "vgetcwd from uncached directory");
    ASSERTCMP(t, expected_real, "vgetcwd from uncached directory");

    path_join(buf, sizeof(buf), "..", dir_over);
    ret = vchdir(buf);
    ASSERTEQ(ret, 0, "vchdir ../overlay");
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "vgetcwd in virtual directory 2");
    ASSERTCMP(t, expected2, "vgetcwd in virtual directory 2");

    ret = vchdir("..");
    ASSERTEQ(ret, 0, "vchdir back into a real dir");
    t = getcwd(buf, MAX_PATH);
    ASSERT(t, "real getcwd should be the original dir");
    ASSERTCMP(t, writedir, "real getcwd should be the original dir");

    ret = vchdir("../data/vdir/vdir2");
    ASSERTEQ(ret, 0, "vchdir into the directories made at ..");
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "vgetcwd in virtual directory 3");
    ASSERTCMP(t, expected3, "vgetcwd in virtual directory 3");

    // This specifically checks an implementation detail of vchdir:
    // vfs_chdir is used for virtual dirs, then real chdir, then vfs_chdir.
    // If the dir is cached, there's a possibility both vfs_chdirs could be
    // called, causing the real CWD and VFS CWD to desynchronize. This bug
    // required the initial CWD to also be a real/cached directory.
    // If the virtual file opens this is probably okay.
    ret = vchdir("../../..");
    ASSERTEQ(ret, 0, "vchdir into a real dir 2");
    ret = vchdir("../" UNIT_TMP_DIR);
    ASSERTEQ(ret, 0, "vchdir back into the original dir");

    t = getcwd(buf, MAX_PATH);
    ASSERT(t, "real getcwd should be the original dir");
    ASSERTCMP(t, writedir, "real getcwd should be the original dir");
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "vgetcwd should be the original dir");
    ASSERTCMP(t, writedir, "vgetcwd should be the original dir");

    // See above.
    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(file_virt, "rb");
      ASSERT(vf, "cwd and VFS cwd desyncronized!");
    }
  }

  SECTION(vmkdir)
  {
    vrmdir("a_real_dir");

    // vmkdir should make a virtual directory while in a virtual directory.
    ret = vchdir(dir_virt);
    ASSERTEQ(ret, 0, "vchdir into virtual directory");
    ret = vmkdir("another_dir", 0755);
    ASSERTEQ(ret, 0, "vmkdir inside of a virtual directory");
    ret = vchdir("another_dir");
    ASSERTEQ(ret, 0, "vchdir into new virtual directory");

    // vmkdir should be able to make real directories in real dirs while
    // the current directory is a virtual dir, too.
    char expected[MAX_PATH];
    path_join(expected, MAX_PATH, writedir, "a_real_dir");

    ret = vmkdir("../../a_real_dir", 0755);
    ASSERTEQ(ret, 0, "vmkdir a real directory from a virtual directory");
    ret = vchdir("../../a_real_dir");
    ASSERTEQ(ret, 0, "vchdir into the new real directory");
    t = getcwd(buf, MAX_PATH);
    ASSERT(t, "real getcwd should be the new directory");
    ASSERTCMP(t, expected, "real getcwd should be the new directory");

    // Nonsense dir--should set errno to ENOENT.
    ret = vmkdir("sdjfkdsjfsd/dfjsdfsd/kfglkfdlgfd", 0755);
    ASSERTEQ(ret, -1, "nonsense vmkdir should fail");
    ASSERTEQ(errno, ENOENT, "nonsense vmkdir should fail with ENOENT");
  }

  SECTION(vrename)
  {
    // Should be able to rename virtual files.
    ret = vrename(file_virt, "boob");
    ASSERTEQ(ret, 0, "vrename virt file");

    // ...but not into a nonexistent directory.
    ret = vrename("boob", "sdhjfsdfdk/boob");
    ASSERTEQ(ret, -1, "vrename virt file into nonsense directory");
    ASSERTEQ(errno, ENOENT, "vrename virt file into nonsense directory");

    // Should be able to move virtual files into, within,
    // and out of virtual directories.
    path_join(buf, MAX_PATH, dir_virt, "boob");
    ret = vrename("boob", buf);
    ASSERTEQ(ret, 0, "vrename virt file into virt dir");
    char buf2[MAX_PATH];
    path_join(buf2, MAX_PATH, dir_virt, file_virt);
    ret = vrename(buf, buf2);
    ASSERTEQ(ret, 0, "rename virt file within virt dir");
    ret = vrename(buf2, file_virt);
    ASSERTEQ(ret, 0, "rename virt file back into real dir");

    // Moving a virtual directory should move the virtual files inside of the directory.
    ret = vrename(file_virt, buf2);
    ASSERTEQ(ret, 0, "rename virt file back into virt dir");
    ret = vrename(dir_virt, "da_new_dir");
    ASSERTEQ(ret, 0, "rename virt dir with virt file inside");
    path_join(buf, MAX_PATH, "da_new_dir", file_virt);
    ret = vstat(buf, &st);
    ASSERTEQ(ret, 0, "found virt file with the new virt dir name");

    // Moving a real directory should move the virtual files inside of the directory.
    vunlink("da_new_real_dir/file");
    vrmdir("da_real_dir");
    vrmdir("da_new_real_dir");
    ret = vmkdir("da_real_dir", 0755);
    ASSERTEQ(ret, 0, "make real dir");
    path_join(buf2, MAX_PATH, "da_real_dir", file_virt);
    ret = vrename(buf, buf2);
    ASSERTEQ(ret, 0, "move virt file into real dir");
    ret = vrename("da_real_dir", "da_new_real_dir");
    ASSERTEQ(ret, 0, "rename real dir with virt file inside");
    path_join(buf, MAX_PATH, "da_new_real_dir", file_virt);
    ret = vstat(buf, &st);
    ASSERTEQ(ret, 0, "found virt file with the new real dir name");

    // Moving a directory over an uncached unempty directory shouldn't work.
    {
      ScopedFile<FILE, fclose> fp = fopen_unsafe(dir_real_file, "wb");
      ASSERT(fp, "create real file");
    }
    ret = vrename("da_new_real_dir", dir_real);
    ASSERTEQ(ret, -1, "move dir over unempty uncached dir");
    if(errno != EEXIST && errno != ENOTEMPTY)
    {
      ASSERTEQ(errno, ENOTEMPTY,
       "move dir over unempty uncached dir (should be EEXIST or ENOTEMPTY)");
    }
    ret = vstat(buf, &st);
    ASSERTEQ(ret, 0, "virt file should exist at old location");
    ret = vstat(dir_real_file, &st);
    ASSERTEQ(ret, 0, "real file should exist");
  }

  SECTION(vunlink)
  {
    for(auto &d : data)
    {
      ret = vunlink(d.path);
      if(d.type == S_IFREG)
        ASSERTEQ(ret, 0, "%s", d.path);
      else
      {
        ASSERTEQ(ret, -1, "%s", d.path);
        ASSERTEQ(errno, EPERM, "%s", d.path);
      }
    }
    ret = stat(file_over, &st);
    ASSERTEQ(ret, 0, "real file at '%s' should still exist", file_over);
  }

  SECTION(vrmdir)
  {
    for(auto &d : data)
    {
      ret = vrmdir(d.path);
      if(d.type == S_IFREG)
      {
        ASSERTEQ(ret, -1, "%s", d.path);
        ASSERTEQ(errno, ENOTDIR, "%s", d.path);
      }
      else

      if(d.rm_result)
      {
        ASSERTEQ(ret, -1, "%s", d.path);
        ASSERTEQ(errno, d.rm_result, "%s", d.path);
      }
      else
        ASSERTEQ(ret, d.rm_result, "%s", d.path);
    }
    ret = stat(dir_over, &st);
    ASSERTEQ(ret, 0, "real dir at '%s' should still exist", dir_over);

    // vrmdir should delete the underlying directory of a cached dir.
    ret = vchdir(dir_real);
    ASSERTEQ(ret, 0, "make sure dir_real is cached");
    ret = vchdir("..");
    ASSERTEQ(ret, 0, "return to initial directory");
    ret = vrmdir(dir_real);
    ASSERTEQ(ret, 0, "delete cached dir_real");
    ret = stat(dir_real, &st);
    ASSERTEQ(ret, -1, "dir_real stat should fail");
    ASSERTEQ(errno, ENOENT, "dir_real stat should fail with ENOENT");
  }

  SECTION(vaccess)
  {
    for(auto &d: data)
    {
      ret = vaccess(d.path, R_OK|W_OK|X_OK);
      ASSERTEQ(ret, 0, "%s", d.path);
    }
  }

  SECTION(vstat)
  {
    for(auto &d : data)
    {
      ret = vstat(d.path, &st);
      ASSERTEQ(ret, 0, "%s", d.path);
      ASSERTEQ(st.st_dev, VFS_MZX_DEVICE, "%s", d.path);
      ASSERTEQ((int)st.st_mode & S_IFMT, d.type, "%s", d.path);
    }
  }

  SECTION(dirent)
  {
    enum vdir_type type;
    boolean found[ARRAY_SIZE(data)]{};
    boolean found_dir_real = false;

    // Make sure dir_real IS cached.
    ret = vchdir(dir_real);
    ASSERTEQ(ret, 0, "make sure dir_real is cached");
    ret = vchdir("..");
    ASSERTEQ(ret, 0, "return to initial directory");
    t = vgetcwd(buf, MAX_PATH);
    ASSERT(t, "check initial directory");
    ASSERTCMP(t, writedir, "check initial directory");

    ScopedFile<vdir, vdir_close> dir = vdir_open(".");
    ASSERT(dir, "");

    // Virtual files should be reported in a directory exactly once.
    // Real cached files should not be counted twice.
    // TODO: virtual files that exist OVER real files do show up
    // twice for now because there's not really a good way around it.
    while(vdir_read(dir, buf, MAX_PATH, &type))
    {
      if(buf[0] != '.')
      {
        ret = vstat(buf, &st);
        ASSERTEQ(ret, 0, "failed to vstat %s", buf);
      }

      if(strcmp(buf, dir_real) == 0)
      {
        ASSERT(!found_dir_real, "duplicate of %s", dir_real);
        found_dir_real = true;
      }
      else

      for(size_t i = 0; i < ARRAY_SIZE(data); i++)
      {
        if(strcmp(buf, data[i].path) == 0)
        {
          // TODO: Hack to allow virtual files over real files to pass.
          if(data[i].path != dir_over && data[i].path != file_over)
            ASSERT(!found[i], "duplicate of %s", data[i].path);
          found[i] = true;
          break;
        }
      }
    }
    for(size_t i = 0; i < ARRAY_SIZE(data); i++)
    {
      // Ignore the files not in the current directory.
      if(data[i].path[0] == '.')
        continue;
      ASSERT(found[i], "didn't find %s", data[i].path);
    }
    ASSERT(found_dir_real, "didn't find %s", dir_real);

    // Should be able to open and step through a virtual dir.
    ScopedFile<vdir, vdir_close> dir2 = vdir_open(dir_virt);
    ASSERT(dir2, "");
    while(vdir_read(dir2, buf, MAX_PATH, &type)) {}
    ASSERTEQ(vdir_tell(dir2), vdir_length(dir2), "");
  }

  SECTION(vvolumelist)
  {
    // Straightforward--if there are virtual roots, list them after the real
    // roots. Do not list cached roots twice.
    // TODO: there is no function to add roots to the vio VFS yet.
    SKIP();
  }

  SECTION(DeleteOpenFile)
  {
    // Can't unlink open file (EBUSY).
    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(file_virt, "rb");
    ASSERT(vf, "open virtual file");
    ret = vunlink(file_virt);
    ASSERTEQ(ret, -1, "can't vunlink open virtual file");
    ASSERTEQ(errno, EBUSY, "can't vunlink open virtual file: EBUSY");
  }

  SECTION(DeleteRealParent)
  {
    // If the real parent is deleted out from under a virtual file or
    // directory, currently the virtual file will be left in a semi-
    // accessible state. It's not clear if there's a way to do this better.
    path_join(buf, MAX_PATH, dir_real, "testfile");
    ret = vio_virtual_file(buf);
    ASSERT(ret, "create virtual file in real dir");

    ret = vrmdir(dir_real);
    ASSERTEQ(ret, -1, "vrmdir should fail due to the virtual file");
    ASSERTEQ(errno, ENOTEMPTY, "vrmdir should fail due to the virtual file");

    ret = rmdir(dir_real);
    ASSERTEQ(ret, 0, "use unistd rmdir to delete the real dir");

    vio_invalidate_all();

    ret = vstat(dir_real, &st);
    ASSERTEQ(ret, 0, "real dir stat still works (cached, can't invalidate)");

    ret = vstat(buf, &st);
    ASSERTEQ(ret, 0, "virtual file stat should still work");
  }
}

UNITTEST(CacheRead)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  write_dir();

  vfssetup a(true);

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_READ_FILENAME, "rb");
  ASSERT(vf, "");

  int flags = vfile_get_flags(vf);
  ASSERT(flags & VF_FILE, "");
  ASSERT(flags & VF_MEMORY, "");
  ASSERT(flags & VF_VIRTUAL, "");

  READ_TESTS(vf);
}

UNITTEST(CacheWrite)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr char test_writeback[] = "test_writeback";
  static constexpr char data[] = "writeback worked!";
  static constexpr size_t data_len = sizeof(data) - 1;
  char buf[MAX_PATH];
  size_t sz;
  int ret;

  write_dir();

  vfssetup a(true);
  vunlink(test_writeback);

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_WRITE_FILENAME, "w+b");
  ASSERT(vf, "");

  int flags = vfile_get_flags(vf);
  ASSERT(flags & VF_FILE, "");
  ASSERT(flags & VF_MEMORY, "");
  ASSERT(flags & VF_VIRTUAL, "");

  WRITE_TESTS(vf, false);

  SECTION(WritebackSeek)
  {
    ScopedFile<vfile, vfclose> wb = vfopen_unsafe(test_writeback, "wb");
    ASSERT(wb, "");
    sz = vfwrite(data, 1, data_len, wb);
    ASSERTEQ(sz, data_len, "");
    ret = vfseek(wb, 8, SEEK_SET); // Should writeback and flush.
    ASSERTEQ(ret, 0, "");

    ScopedFile<FILE, fclose> fp = fopen_unsafe(test_writeback, "rb");
    sz = fread(buf, 1, data_len, fp);
    ASSERTEQ(sz, data_len, "");
    ASSERTMEM(data, buf, data_len, "");
    ret = fgetc(fp);
    ASSERTEQ(ret, EOF, "should be EOF");
  }

  SECTION(WritebackRewind)
  {
    ScopedFile<vfile, vfclose> wb = vfopen_unsafe(test_writeback, "wb");
    ASSERT(wb, "");
    sz = vfwrite(data, 1, data_len, wb);
    ASSERTEQ(sz, data_len, "");
    vrewind(wb); // Should writeback and flush.

    ScopedFile<FILE, fclose> fp = fopen_unsafe(test_writeback, "rb");
    sz = fread(buf, 1, data_len, fp);
    ASSERTEQ(sz, data_len, "");
    ASSERTMEM(data, buf, data_len, "");
    ret = fgetc(fp);
    ASSERTEQ(ret, EOF, "should be EOF");
  }

  SECTION(WritebackClose)
  {
    {
      ScopedFile<vfile, vfclose> wb = vfopen_unsafe(test_writeback, "wb");
      ASSERT(wb, "");
      sz = vfwrite(data, 1, data_len, wb);
      ASSERTEQ(sz, data_len, "");
    } // Destructor should invoke vfclose -> writeback and flush.

    ScopedFile<FILE, fclose> fp = fopen_unsafe(test_writeback, "rb");
    sz = fread(buf, 1, data_len, fp);
    ASSERTEQ(sz, data_len, "");
    ASSERTMEM(data, buf, data_len, "");
    ret = fgetc(fp);
    ASSERTEQ(ret, EOF, "should be EOF");
  }
}

static void force_cached(const char *filename, const char *message)
{
  struct stat st{};
  int ret = stat(filename, &st);
  ASSERTEQ(ret, 0, "%s - %s", filename, message);
  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(filename, "rb");
  ASSERT(vf, "%s - %s", filename, message);
  int flags = vfile_get_flags(vf);

  // Only write mode cached files are guaranteed to have FILE handles.
  //ASSERT(flags & VF_FILE, "%s - %s", filename, message);

  ASSERT(flags & VF_MEMORY, "%s - %s", filename, message);
  ASSERT(flags & VF_VIRTUAL, "%s - %s", filename, message);
}

UNITTEST(CacheFilesystem)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr char cached_file[] = "cached_file";
  static constexpr char cached_dir[] = "cached_dir";
  static constexpr char cached_dir_renamed[] = "new_dir_name";
  char buf[MAX_PATH];
  char buf2[MAX_PATH];
  struct stat st{};
  size_t sz;
  int ret;

  write_dir();

  path_join(buf, sizeof(buf), cached_dir, cached_file);
  path_join(buf2, sizeof(buf), cached_dir_renamed, cached_file);
  vunlink(buf);
  vunlink(buf2);
  vunlink(cached_file);
  vrmdir(cached_dir);
  vrmdir(cached_dir_renamed);

  vfssetup a(true);

  // The unistd filesystem functions should behave normally if there is a
  // cached file or directory that they are operating on. This usually works
  // by recursively purging the internal VFS cache files when detected.

  // vchdir, vgetcwd, vrmdir, dirent are already tested by VirtualFilesystem
  // as directory caching is required for virtual directories and files.
  // vmkdir is irrelevant.

  // This should create the test file, close it, and then cache it.
  {
    ScopedFile<FILE, fclose> fp = fopen_unsafe(cached_file, "wb");
    ASSERT(fp, "");
    sz = fwrite(test_data, 1, sizeof(test_data), fp);
    ASSERTEQ(sz, sizeof(test_data), "");
  }
  force_cached(cached_file, "force initial cached file");

  ret = vmkdir(cached_dir, 0755);
  ASSERTEQ(ret, 0, "");

  SECTION(vrename)
  {
    static constexpr char new_data[] = "fdsjdfsdfdssdffdfgdfkg";

    path_join(buf, MAX_PATH, cached_dir, cached_file);
    path_join(buf2, MAX_PATH, cached_dir_renamed, cached_file);

    // vrename a cached file--it should relocate correctly.
    ret = vrename(cached_file, buf);
    ASSERTEQ(ret, 0, "successfully renamed cached file");

    // Make sure it is cached again.
    force_cached(buf, "force renamed file cached");

    // vrename a directory with a cached file--it should relocate correctly
    ret = vrename(cached_dir, cached_dir_renamed);
    ASSERTEQ(ret, 0, "successfully renamed cached dir with cached file");
    ret = vstat(buf2, &st);
    ASSERTEQ(ret, 0, "successful stat of cached file");

    // Make sure it is cached again (it should already be, though).
    force_cached(buf2, "force renamed file cached (2)");

    // and opening it after modifying and closing it should reflect the changes.
    // This relies on working writeback.
    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(buf2, "wb");
      ASSERT(vf, "");
      sz = vfwrite(new_data, 1, sizeof(new_data), vf);
      ASSERTEQ(sz, sizeof(new_data), "");

      ret = vrename(buf2, cached_file);
    }

    // If the filesystem didn't allow the file to be moved, it should be in
    // the same place.
    const char *check_path = buf2;
    // If the filesystem allowed this file to be moved, make sure the file
    // also moved
    if(ret == 0)
      check_path = cached_file;

    ScopedFile<FILE, fclose> fp = fopen_unsafe(check_path, "rb");
    ASSERT(fp, "checking at %s - vrename returned %d", check_path, ret);
    sz = fread(buf, 1, sizeof(new_data), fp);
    ASSERTEQ(sz, sizeof(new_data),
     "checking at %s - vrename returned %d", check_path, ret);
    ASSERTMEM(buf, new_data, sizeof(new_data),
     "checking at %s - vrename returned %d", check_path, ret);
  }

  SECTION(vunlink)
  {
    // This should remove the file from the real filesystem and from the cache.
    ret = vunlink(cached_file);
    ASSERTEQ(ret, 0, "remove cached file");
    ret = stat(cached_file, &st);
    ASSERTEQ(ret, -1, "cached file stat should fail");
    ASSERTEQ(errno, ENOENT, "cached file stat errno should be ENOENT");
  }

  SECTION(vaccess)
  {
    // This should still work on cached files.
    ret = vaccess(cached_file, R_OK|W_OK);
    ASSERTEQ(ret, 0, "vaccess should work on a cached file");
  }

  SECTION(vstat)
  {
    // This should work on cached files, and currently returns a limited amount
    // of cached information instead of real info.
    ret = vstat(cached_file, &st);
    ASSERTEQ(ret, 0, "vstat should work on a cached file");
    ASSERTEQ(st.st_dev, VFS_MZX_DEVICE, "cached vstat should have virtual device");
    ASSERTEQ(st.st_size, sizeof(test_data), "cached vstat should have correct size");
  }

  SECTION(VirtualCreateOverCached)
  {
    // Making a virtual file over a cached file should drop the cached
    // file in favor of a virtual file, same as an uncached file.
    ret = vio_virtual_file(cached_file);
    ASSERT(ret, "make virtual file over real (cached) file");

    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(cached_file, "rb");
    ASSERT(vf, "open virtual file for read");
    int64_t len = vfilelength(vf, false);
    ASSERTEQ(len, 0, "length of virtual file should be zero");
  }

  SECTION(VirtualRenameOverCached)
  {
    // Moving a virtual file over a cached file should drop the cached
    // file in favor of a virtual file, same as an uncached file.
    static constexpr char old_file[] = "dsdjfkdsl";
    ret = vio_virtual_file(old_file);
    ASSERT(ret, "make virtual file");
    ret = vrename(old_file, cached_file);
    ASSERTEQ(ret, 0, "rename virtual file over real (cached) file");

    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(cached_file, "rb");
    ASSERT(vf, "open virtual file for read");
    int64_t len = vfilelength(vf, false);
    ASSERTEQ(len, 0, "length of virtual file should be zero");
  }
}

static void generate_cached_file(const char *path, const void *data, size_t len,
 int flags = 0)
{
  // Generate the file using regular filesystem commands and then cache it.
  // This shouldn't be done with vio since the file will be created at 0
  // length, and the invalidation check occurs at file creation.
  {
    ScopedFile<FILE, fclose> fp = fopen_unsafe(path, "wb");
    ASSERT(fp, "create: %s", path);
    size_t count = fwrite(data, 1, len, fp);
    ASSERTEQ(count, len, "create: %s", path);
  }

  // Now *try* to cache the file.
  struct stat st{};
  int ret = stat(path, &st);
  ASSERTEQ(ret, 0, "%s", path);
  ScopedFile<vfile, vfclose> vf = vfopen_unsafe_ext(path, "rb", flags);
  ASSERT(vf, "%s", path);

  flags = vfile_get_flags(vf);

  // Not guaranteed to have a FILE unless it is in write mode.
  if(flags & VF_WRITE)
    ASSERT(flags & VF_FILE, "%s", path);

  // In this case, the file should NEVER be cached.
  // Takes precedence over V_FORCE_CACHE.
  if(flags & V_DONT_CACHE)
  {
    ASSERT(~flags & VF_MEMORY, "%s", path);
    ASSERT(~flags & VF_VIRTUAL, "%s", path);
  }
  else

  // In this case, the file should ALWAYS be cached.
  if(flags & V_FORCE_CACHE)
  {
    ASSERT(flags & VF_MEMORY, "%s", path);
    ASSERT(flags & VF_VIRTUAL, "%s", path);
  }
}

UNITTEST(CacheMemoryLimit)
{
  // vio will enforce a maximum cache size limit in most cases.
  static constexpr size_t file_size = cache_max_size * 2 / 5;
  static constexpr size_t file_oversize = cache_max_size * 3 / 5;
  static constexpr size_t file_overmax = cache_max_size * 4;
  static constexpr char filename1[] = "cache_me1";
  static constexpr char filename2[] = "cache_me2";
  static constexpr char filename3[] = "cache_me3";
  static constexpr char data[file_overmax]{};
  size_t single_usage;
  size_t total;

  write_dir();

  vfssetup a(true);

  total = vio_filesystem_total_cached_usage();
  ASSERTEQ(total, 0, "should be empty initially");

  vunlink(filename1);
  vunlink(filename2);
  vunlink(filename3);

  SECTION(Multiple)
  {
    // Caching files over the maximum size limit should eject old files.
    generate_cached_file(filename1, data, file_size);
    single_usage = vio_filesystem_total_cached_usage();

    // Should use twice as much memory as the first file.
    // Note that the allocated memory might be bigger than the file's size,
    // but it SHOULD be consistent for each file.
    generate_cached_file(filename2, data, file_size);
    total = vio_filesystem_total_cached_usage();
    ASSERTEQ(total, single_usage * 2, "");

    // Should invalidate one of the previous files, so the size is
    // the exact same.
    generate_cached_file(filename3, data, file_size);
    total = vio_filesystem_total_cached_usage();
    ASSERTEQ(total, single_usage * 2, "");
  }

  SECTION(Oversize)
  {
    // Oversize files should be rejected from the cache.
    generate_cached_file(filename1, data, file_size);
    single_usage = vio_filesystem_total_cached_usage();

    generate_cached_file(filename1, data, file_oversize);
    total = vio_filesystem_total_cached_usage();
    ASSERTEQ(total, single_usage, "oversize file should be rejected");
  }

  SECTION(DontCache)
  {
    // The vio flag V_DONT_CACHE can be used to prevent files from being
    // added to the cache by the current operation.
    generate_cached_file(filename1, data, file_size);
    single_usage = vio_filesystem_total_cached_usage();

    generate_cached_file(filename2, data, file_size, V_DONT_CACHE);
    total = vio_filesystem_total_cached_usage();
    ASSERTEQ(single_usage, total, "V_DONT_CACHE");

    generate_cached_file(filename3, data, file_size, V_DONT_CACHE|V_FORCE_CACHE);
    total = vio_filesystem_total_cached_usage();
    ASSERTEQ(single_usage, total, "V_DONT_CACHE precedence over V_FORCE_CACHE");
  }

  SECTION(ForceCache)
  {
    // The vio flag V_FORCE_CACHE can be used to cache oversize files
    // and cache files in cases where it is impossible to free enough space.
    generate_cached_file(filename1, data, file_size);
    single_usage = vio_filesystem_total_cached_usage();

    generate_cached_file(filename2, data, file_oversize, V_FORCE_CACHE);
    size_t second = vio_filesystem_total_cached_usage();
    ASSERT(second > single_usage, "V_FORCE_CACHE overrides file size limit");

    generate_cached_file(filename3, data, file_overmax, V_FORCE_CACHE);
    size_t third = vio_filesystem_total_cached_usage();
    ASSERT(third > cache_max_size,
     "V_FORCE_CACHE overrides total size limit (%zu > %zu)",
     third, cache_max_size);
  }
}

UNITTEST(vfile_force_to_memory)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  // Safety--make sure this really is disabled.
  vio_filesystem_exit();

  write_dir();

  const auto &common_test = [](const char *mode, boolean expected_ret)
  {
    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_READ_FILENAME, mode);
    ASSERT(vf, "");

    int ret = vfile_force_to_memory(vf);
    ASSERTEQ(ret, expected_ret, "");

    if(ret)
    {
      int flags = vfile_get_flags(vf);
      ASSERT(flags & VF_MEMORY, "");
      ASSERT(~flags & VF_FILE, "");

      uint8_t buf[sizeof(test_data)];
      size_t sz = vfread(buf, 1, sizeof(test_data), vf);
      ASSERTEQ(sz, sizeof(test_data), "");
      ASSERTMEM(buf, test_data, sizeof(test_data), "");

      // This should always work because it's already in memory!
      ret = vfile_force_to_memory(vf);
      ASSERT(ret, "");
    }
  };

  SECTION(NoVFS)
  {
    // File won't be in memory -> is converted to a memory tempfile.
    common_test("rb", true);
  }

  SECTION(VFS)
  {
    // File will already be in memory via the cache.
    vfssetup a(true);
    common_test("rb", true);
  }

  SECTION(Write)
  {
    // This function should reject writes either way.
    common_test("r+b", false);

    vfssetup a(true);
    common_test("r+b", false);
  }
}
