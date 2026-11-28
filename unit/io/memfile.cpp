/* MegaZeux
 *
 * Copyright (C) 2020-2024 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "../../src/io/memfile.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

UNITTEST(mfopen)
{
  unsigned char buffer[16];

  SECTION(mfopen)
  {
    struct memfile mf;

    mfopen(buffer, sizeof(buffer), &mf);
    ASSERTEQ(mf.start, buffer, "");
    ASSERTEQ(mf.current, buffer, "");
    ASSERTEQ(mf.end, mf.start + sizeof(buffer), "");
    ASSERTEQ(mf.alloc, false, "");
    ASSERTEQ(mf.seek_past_end, false, "");
    ASSERTEQ(mf.is_write, false, "");
  }

  SECTION(mfopen_wr)
  {
    struct memfile mf;

    mfopen_wr(buffer, sizeof(buffer), &mf);
    ASSERTEQ(mf.start, buffer, "");
    ASSERTEQ(mf.current, buffer, "");
    ASSERTEQ(mf.end, mf.start + sizeof(buffer), "");
    ASSERTEQ(mf.alloc, false, "");
    ASSERTEQ(mf.seek_past_end, false, "");
    ASSERTEQ(mf.is_write, true, "");
  }

  SECTION(mfopen_alloc)
  {
    struct memfile *mf;

    mf = mfopen_alloc(buffer, sizeof(buffer));
    ASSERT(mf, "");
    ASSERTEQ(mf->start, buffer, "");
    ASSERTEQ(mf->current, buffer, "");
    ASSERTEQ(mf->end, buffer + sizeof(buffer), "");
    ASSERTEQ(mf->alloc, true, "");
    ASSERTEQ(mf->seek_past_end, false, "");
    ASSERTEQ(mf->is_write, false, "");

    int ret = mf_alloc_free(mf);
    ASSERTEQ(ret, 0, "");
  }

  SECTION(mf_alloc_free_on_static_memfile)
  {
    struct memfile mf;
    mfopen(buffer, sizeof(buffer), &mf);

    int ret = mf_alloc_free(&mf);
    ASSERTEQ(ret, -1, "");
    ASSERTEQ(mf.start, buffer, "");
    ASSERTEQ(mf.current, buffer, "");
    ASSERTEQ(mf.end, buffer + sizeof(buffer), "");
  }
}

UNITTEST(mfsync)
{
  unsigned char buffers[4][32];
  struct memfile mf;
  int i;

  void *buf;
  size_t len;

  for(i = 0; i < arraysize(buffers); i++)
  {
    mfopen(buffers[i], sizeof(buffers[i]), &mf);
    ASSERTEQ(mf.start, buffers[i], "%d", i);
    ASSERTEQ(mf.end, buffers[i] + sizeof(buffers[i]), "%d", i);

    mfsync(&buf, &len, &mf);
    ASSERTEQ(buf, (void *)buffers[i], "%d", i);
    ASSERTEQ((int)len, sizeof(buffers[i]), "%d", i);

    mfsync(&buf, nullptr, &mf);
    ASSERTEQ(buf, (void *)buffers[i], "%d", i);

    mfsync(nullptr, &len, &mf);
    ASSERTEQ((int)len, sizeof(buffers[i]), "%d", i);

    mfsync(nullptr, nullptr, &mf);
  }
}

struct buffer_data
{
  char *buffer;
  size_t buffer_len;
  boolean has0;
  boolean has8;
  boolean has16;
  boolean has32;
  boolean has64;
  boolean has128;
};

UNITTEST(mfhasspace)
{
  char buffer8[8];
  char buffera[16];
  char bufferb[32];
  char bufferc[64];
  char bufferd[128];
  char buffere[256];
  struct buffer_data pairs[] =
  {
    { buffer8, sizeof(buffer8), true, true, false, false, false, false },
    { buffera, sizeof(buffera), true, true, true,  false, false, false },
    { bufferb, sizeof(bufferb), true, true, true,  true,  false, false },
    { bufferc, sizeof(bufferc), true, true, true,  true,  true,  false },
    { bufferd, sizeof(bufferd), true, true, true,  true,  true,  true  },
    { buffere, sizeof(buffere), true, true, true,  true,  true,  true  },
  };
  struct memfile mf;
  int i;

  for(i = 0; i < arraysize(pairs); i++)
  {
    mfopen(pairs[i].buffer, pairs[i].buffer_len, &mf);
    ASSERTEQ(pairs[i].has0,   mfhasspace(0, &mf), "%d", i);
    ASSERTEQ(pairs[i].has8,   mfhasspace(8, &mf), "%d", i);
    ASSERTEQ(pairs[i].has16,  mfhasspace(16, &mf), "%d", i);
    ASSERTEQ(pairs[i].has32,  mfhasspace(32, &mf), "%d", i);
    ASSERTEQ(pairs[i].has64,  mfhasspace(64, &mf), "%d", i);
    ASSERTEQ(pairs[i].has128, mfhasspace(128, &mf), "%d", i);

    mf.current = (mf.end - mf.start)/2 + mf.start;
    ASSERTEQ(pairs[i].has16,  mfhasspace(8, &mf), "%d", i);
    ASSERTEQ(pairs[i].has32,  mfhasspace(16, &mf), "%d", i);
    ASSERTEQ(pairs[i].has64,  mfhasspace(32, &mf), "%d", i);
    ASSERTEQ(pairs[i].has128, mfhasspace(64, &mf), "%d", i);

    mf.current = mf.end;
    ASSERTEQ(pairs[i].has0,   mfhasspace(0, &mf), "%d", i);
  }

  struct memfile tmp{};
  ASSERTEQ(mfhasspace(0, &tmp), false, "NULL memfile->current should return false");
}

UNITTEST(mfmove)
{
  const int OFFSET = 96;
  unsigned char buffera[128];
  unsigned char bufferb[256];
  unsigned char bufferc[64];
  struct memfile mf;

  static_assert(sizeof(bufferc) < OFFSET,
   "OFFSET should be larger than the size of bufferc.");

  mfopen_wr(buffera, sizeof(buffera), &mf);
  mf.current = mf.start + 96;
  ASSERTEQ(mf.current, buffera + 96, "");

  mfmove(bufferb, sizeof(bufferb), &mf);
  ASSERTEQ(mf.start, bufferb, "");
  ASSERTEQ(mf.end, bufferb + sizeof(bufferb), "");
  ASSERTEQ(mf.current, bufferb + 96, "");

  mfmove(bufferc, sizeof(bufferc), &mf);
  ASSERTEQ(mf.start, bufferc, "");
  ASSERTEQ(mf.end, bufferc + sizeof(bufferc), "");
  ASSERTEQ(mf.current, bufferc + sizeof(bufferc), "");
}

UNITTEST(mfresize)
{
  const int BUFSIZE = 128;
  const int NEWSIZE = 256;
  void *buf = malloc(BUFSIZE);
  struct memfile mf;
  size_t size;

  ASSERT(buf, "");
  mfopen(buf, BUFSIZE, &mf);
  mfresize(NEWSIZE, &mf);
  mfsync(&buf, &size, &mf);
  ASSERTEQ(size, NEWSIZE, "");

  mfresize(BUFSIZE, &mf);
  mfsync(&buf, &size, &mf);
  ASSERTEQ(size, BUFSIZE, "");

  free(buf);
}

struct seq
{
  long int position;
  int next;
  int retval;
};

UNITTEST(mfseek_mftell)
{
  unsigned char buffer[256];
  struct memfile mf;
  static const int offs_safe[] =
  {
    128,
    256,
    0,
    32,
    79,
    84,
    255,
    1,
    172,
  };
  static const int offs_unsafe[] =
  {
    -1,
    -1000,
    -128731282,
  };
  static const int offs_maybe_safe[] =
  {
    sizeof(buffer) + 1,
    1024,
    9999,
  };
  int ret;

  mfopen_wr(buffer, sizeof(buffer), &mf);

  SECTION(mftell)
  {
    for(int pos : offs_safe)
    {
      mf.current = mf.start + pos;
      ASSERTEQ(mftell(&mf), pos, "mftell safe %d", pos);
    }
  }

  SECTION(seek_set)
  {
    for(int pos : offs_safe)
    {
      ret = mfseek(&mf, pos, SEEK_SET);
      ASSERTEQ(ret, 0, "seek_set safe %d", pos);
      ASSERTEQ(mftell(&mf), pos, "seek_set safe %d", pos);
    }

    ASSERT(!mfseek(&mf, 129, SEEK_SET), "");

    for(int pos : offs_unsafe)
    {
      ret = mfseek(&mf, pos, SEEK_SET);
      ASSERTEQ(ret, -1, "seek_set unsafe %d", pos);
      ASSERTEQ(mftell(&mf), 129, "seek_set unsafe %d", pos);
    }

    for(int pos : offs_maybe_safe)
    {
      ret = mfseek(&mf, pos, SEEK_SET);
      ASSERTEQ(ret, -1, "seek_set unsafe2 %d", pos);
      ASSERTEQ(mftell(&mf), 129, "seek_set unsafe2 %d", pos);
    }
  }

  SECTION(seek_cur)
  {
    static const seq sequence[] =
    {
      {   0,   32,  0 },
      {  32,  -17,  0 },
      {  15,  200,  0 },
      { 215,  -80,  0 },
      { 135,   62,  0 },
      { 197,  100, -1 },
      { 197, -500, -1 },
      { 197,    0,  0 }
    };

    for(const seq seq : sequence)
    {
      ASSERTEQ(mftell(&mf), seq.position, "seek_cur sequence %ld->%d", seq.position, seq.next);
      ret = mfseek(&mf, seq.next, SEEK_CUR);
      ASSERTEQ(ret, seq.retval, "seek_cur sequence %ld->%d", seq.position, seq.next);
    }
  }

  SECTION(seek_end)
  {
    mfseek(&mf, 0, SEEK_END);
    ASSERTEQ(mftell(&mf), sizeof(buffer), "seek_end");
  }

  SECTION(seek_past_end)
  {
    mf.seek_past_end = true;

    for(int pos : offs_maybe_safe)
    {
      ret = mfseek(&mf, pos, SEEK_SET);
      ASSERTEQ(ret, 0, "seek_set past end %d", pos);
      ASSERTEQ(mftell(&mf), pos, "seek_set past end %d", pos);
      ASSERTEQ(mfhasspace(1, &mf), false, "seek_set past end %d", pos);
    }
  }
}

UNITTEST(read_write)
{
  static const uint8_t data8[] =
  {
    0x00, 0x01, 0xFF, 0xFE, 0x7F, 0x53, 0xA3, 0xD8,
    0xFF, 0x00, 0x12, 0x34, 0x45, 0x67, 0x89, 0xAB,
    /* shut GCC -O3, -fno-lto up */ 0xaa
  };
  const int SIZE = sizeof(data8) - 1;

  static const uint16_t data16[] =
  {
    0x0100, 0xFEFF, 0x537F, 0xD8A3,
    0x00FF, 0x3412, 0x6745, 0xAB89,
  };
  static const int32_t data32s[] =
  {
    (int)(0xFEFF0100), (int)(0xD8A3537F),
    (int)(0x341200FF), (int)(0xAB896745),
  };
  static const uint32_t data32u[] =
  {
    0xFEFF0100, 0xD8A3537F,
    0x341200FF, 0xAB896745,
  };
  static const int64_t data64s[] =
  {
    static_cast<int64_t>(0xD8A3537FFEFF0100ULL),
    static_cast<int64_t>(0xAB896745341200FFULL),
  };
  static const uint64_t data64u[] =
  {
    0xD8A3537FFEFF0100ULL,
    0xAB896745341200FFULL,
  };

  uint8_t dest[SIZE * 2];
  struct memfile mf;
  int res;
  int res2;
  int i;

  static_assert(arraysize(data16) * 2 == SIZE,
   "input array size doesn't match u16 output array size!");

  static_assert(arraysize(data32s) * 4 == SIZE,
   "input array size doesn't match s32 output array size!");
  static_assert(arraysize(data32u) * 4 == SIZE,
   "input array size doesn't match u32 output array size!");

  static_assert(arraysize(data64s) * 8 == SIZE,
   "input array size doesn't match s64 output array size!");
  static_assert(arraysize(data64u) * 8 == SIZE,
   "input array size doesn't match u64 output array size!");

  SECTION(mfgetc)
  {
    mfopen(data8, SIZE, &mf);

    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    for(int i = 0; i < SIZE; i++)
    {
      uint8_t tmp = mfgetc(&mf);
      ASSERTEQ(tmp, data8[i], "");
    }
  }

  SECTION(mfgetw)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(uint16_t value : data16)
    {
      uint16_t tmp = mfgetw(&mf);
      ASSERTEQ(tmp, value, "");
    }
  }

  SECTION(mfgetd)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(int32_t value : data32s)
    {
      int tmp = mfgetd(&mf);
      ASSERTEQ(tmp, value, "");
    }
  }

  SECTION(mfgetud)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(uint32_t value : data32u)
    {
      uint32_t tmp = mfgetud(&mf);
      ASSERTEQ(tmp, value, "");
    }
  }

  SECTION(mfgetq)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(int64_t value : data64s)
    {
      int64_t tmp = mfgetq(&mf);
      ASSERTEQ(tmp, value, "");
    }
  }

  SECTION(mfgetuq)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(uint64_t value : data64u)
    {
      uint64_t tmp = mfgetuq(&mf);
      ASSERTEQ(tmp, value, "");
    }
  }

  SECTION(mfread)
  {
    mfopen(data8, SIZE, &mf);

    res = mfread(dest, SIZE, 1, &mf);
    ASSERTEQ(res, 1, "read SIZE x 1");
    ASSERTMEM(data8, dest, SIZE, "read SIZE x 1");

    mf.current = mf.start;
    res = mfread(dest, 1, SIZE, &mf);
    ASSERTEQ(res, SIZE, "read 1 x SIZE");
    ASSERTMEM(data8, dest, SIZE, "read 1 x SIZE");

    mf.current = mf.start;
    res = mfread(dest, SIZE/2, 1, &mf);
    res2 = mfread(dest + SIZE/2, 1, SIZE/2, &mf);
    ASSERTEQ(res, 1, "read SIZE/2");
    ASSERTEQ(res2, SIZE/2, "read SIZE/2");
    ASSERTMEM(data8, dest, SIZE, "read SIZE/2");

    mf.current = mf.start;
    res = mfread(dest, SIZE, 2, &mf);
    ASSERTEQ(res, 1, "read SIZE x 2");

    mf.current = mf.start;
    res = mfread(dest, 3, SIZE*2/3, &mf);
    ASSERTEQ(res, SIZE/3, "read 3 x (SIZE * 2/3), past end");

    res = mfread(dest, 1, SIZE - SIZE/3*3 + 1, &mf);
    ASSERTEQ(res, SIZE - SIZE/3*3, "read 1 x 2, past end");

    res = mfread(dest, 1, 1, &mf);
    ASSERTEQ(res, 0, "read 1, past end");

    // This shouldn't happen, but the end bounding should catch it anyway.
    mf.current = mf.end + 1;
    res = mfread(dest, 1, 1, &mf);
    ASSERTEQ(res, 0, "read 1, current past end");

    uint8_t noread = 0xef;
    mf.current = mf.start;
    res = mfread(&noread, 0, 1, &mf);
    ASSERTEQ(res, 0, "read 0 x 1");
    ASSERTEQ(noread, 0xef, "read 0 x 1");
    ASSERTEQ(mf.start, mf.current, "read 0 x 1");

    res = mfread(&noread, 1, 0, &mf);
    ASSERTEQ(res, 0, "read 1 x 0");
    ASSERTEQ(noread, 0xef, "read 1 x 0");
    ASSERTEQ(mf.start, mf.current, "read 1 x 0");
  }

  SECTION(mfputc)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < SIZE; i++)
    {
      res = mfputc(data8[i], &mf);
      ASSERTEQ((unsigned)res, data8[i], "");
      ASSERTEQ((unsigned)res, dest[i], "");
    }
  }

  SECTION(mfputw)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < arraysize(data16); i++)
    {
      mfputw(data16[i], &mf);
      ASSERTEQ((uint16_t)dest[i * 2 + 0], data16[i] & 0xFF, "");
      ASSERTEQ((uint16_t)dest[i * 2 + 1], (data16[i] >> 8) & 0xFF, "");
    }
  }

  SECTION(mfputd)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < arraysize(data32s); i++)
    {
      mfputd(data32s[i], &mf);
      ASSERTEQ((int)dest[i * 4 + 0], data32s[i] & 0xFF, "");
      ASSERTEQ((int)dest[i * 4 + 1], (data32s[i] >> 8) & 0xFF, "");
      ASSERTEQ((int)dest[i * 4 + 2], (data32s[i] >> 16) & 0xFF, "");
      ASSERTEQ((int)dest[i * 4 + 3], (data32s[i] >> 24) & 0xFF, "");
    }
  }

  SECTION(mfputud)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < arraysize(data32u); i++)
    {
      mfputud(data32u[i], &mf);
      ASSERTEQ((uint32_t)dest[i * 4 + 0], data32u[i] & 0xFF, "");
      ASSERTEQ((uint32_t)dest[i * 4 + 1], (data32u[i] >> 8) & 0xFF, "");
      ASSERTEQ((uint32_t)dest[i * 4 + 2], (data32u[i] >> 16) & 0xFF, "");
      ASSERTEQ((uint32_t)dest[i * 4 + 3], (data32u[i] >> 24) & 0xFF, "");
    }
  }

  SECTION(mfputq)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < arraysize(data64s); i++)
    {
      mfputq(data64s[i], &mf);
      ASSERTEQ((int)dest[i * 8 + 0], data64s[i] & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 1], (data64s[i] >> 8) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 2], (data64s[i] >> 16) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 3], (data64s[i] >> 24) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 4], (data64s[i] >> 32) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 5], (data64s[i] >> 40) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 6], (data64s[i] >> 48) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 7], (data64s[i] >> 56) & 0xFF, "");
    }
  }

  SECTION(mfputuq)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen_wr(dest, sizeof(dest), &mf);

    for(i = 0; i < arraysize(data64u); i++)
    {
      mfputq(data64u[i], &mf);
      ASSERTEQ((int)dest[i * 8 + 0], data64u[i] & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 1], (data64u[i] >> 8) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 2], (data64u[i] >> 16) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 3], (data64u[i] >> 24) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 4], (data64u[i] >> 32) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 5], (data64u[i] >> 40) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 6], (data64u[i] >> 48) & 0xFF, "");
      ASSERTEQ((int)dest[i * 8 + 7], (data64u[i] >> 56) & 0xFF, "");
    }
  }

  SECTION(mfwrite)
  {
    mfopen_wr(dest, SIZE, &mf);

    res = mfwrite(data8, SIZE, 1, &mf);
    ASSERTEQ(res, 1, "write SIZE x 1");
    ASSERTMEM(data8, dest, SIZE, "write SIZE x 1");

    mf.current = mf.start;
    res = mfwrite(data8, 1, SIZE, &mf);
    ASSERTEQ(res, SIZE, "write 1 x SIZE");
    ASSERTMEM(data8, dest, SIZE, "write 1 x SIZE");

    mf.current = mf.start;
    res = mfwrite(data8, SIZE/2, 1, &mf);
    res2 = mfwrite(data8 + SIZE/2, 1, SIZE/2, &mf);
    ASSERTEQ(res, 1, "write SIZE/2");
    ASSERTEQ(res2, SIZE/2, "write SIZE/2");
    ASSERTMEM(data8, dest, SIZE, "write SIZE/2");

    mf.current = mf.start;
    res = mfwrite(data8, SIZE, 2, &mf);
    ASSERTEQ(res, 1, "");

    mf.current = mf.start;
    res = mfwrite(data8, 3, SIZE*2/3, &mf);
    ASSERTEQ(res, SIZE/3, "write 3 * (SIZE*2/3), past end");

    res = mfwrite(data8, 1, SIZE - SIZE/3*3 + 1, &mf);
    ASSERTEQ(res, SIZE - SIZE/3*3, "write 1 x 2, past end");

    res = mfwrite(data8, 1, 1, &mf);
    ASSERTEQ(res, 0, "write 1, past end");

    // This shouldn't happen, but the end bounding should catch it anyway.
    mf.current = mf.end + 1;
    res = mfwrite(dest, 1, 1, &mf);
    ASSERTEQ(res, 0, "write 1, current past end");

    uint8_t nowrite = 0xef;
    mf.current = mf.start;
    res = mfwrite(&nowrite, 0, 1, &mf);
    ASSERTEQ(res, 0, "write 0 x 1");
    ASSERTEQ(dest[0], data8[0], "write 0 x 1");
    ASSERTEQ(mf.start, mf.current, "write 0 x 1");

    res = mfwrite(&nowrite, 1, 0, &mf);
    ASSERTEQ(res, 0, "write 1 x 0");
    ASSERTEQ(dest[0], data8[0], "write 1 x 0");
    ASSERTEQ(mf.start, mf.current, "write 1 x 0");
  }
}

struct memsafegets_data
{
  const char * const input;
  const char * const output[8];
};

UNITTEST(mfsafegets)
{
  // NOTE: classic Mac line ends not currently supported, don't test...
  static const memsafegets_data data[] =
  {
    {
      "a string\nanother string\n",
      {
        "a string",
        "another string",
        nullptr
      }
    },
    {
      "I'am wining this is so cool\r\nThere is no one versing you though\r\n",
      {
        "I'am wining this is so cool",
        "There is no one versing you though",
        nullptr
      }
    },
    {
      ":loop\nset a 123\nwait 1\ngoto loop\n",
      {
        ":loop",
        "set a 123",
        "wait 1",
        "goto loop",
        nullptr
      }
    },
    {
      ":loop\r\nset a 123\r\nwait 1\r\ngoto loop\r\n",
      {
        ":loop",
        "set a 123",
        "wait 1",
        "goto loop",
        nullptr
      }
    },
    {
      "a\nb\nc\nd\ne\nf\ng",
      { "a", "b", "c", "d", "e", "f", "g", nullptr }
    },
    {
      "a\r\nb\r\nc\r\nd\r\ne\r\nf\r\ng",
      { "a", "b", "c", "d", "e", "f", "g", nullptr }
    },
  };
  const int SHORT_BUF_LEN = 32;
  static const memsafegets_data short_data[] =
  {
    {
      "this should be truncated by the tiny buffer and split into multiple lines"
      "\nand still work after\n",
      {
        "this should be truncated by the",
        " tiny buffer and split into mul",
        "tiple lines",
        "and still work after",
        nullptr
      }
    },
    {
      "wtf are you\r\nusing this very small and pathetic\r\nbuffer for anyway"
      " it's 2020 already\r\n",
      {
        "wtf are you",
        "using this very small and pathe",
        "tic",
        "buffer for anyway it's 2020 alr",
        "eady",
        nullptr
      }
    }
  };
  struct memfile mf;
  char buffer[512];
  int j;

  SECTION(NormalData)
  {
    for(const memsafegets_data &d : data)
    {
      mfopen(d.input, strlen(d.input), &mf);

      for(j = 0; j < arraysize(d.output); j++)
      {
        char *result = mfsafegets(buffer, sizeof(buffer), &mf);
        if(result && d.output[j])
        {
          ASSERTCMP(result, d.output[j], "%s: %d", d.input, j);
        }
        else
          ASSERTEQ(result, d.output[j], "%s: %d", d.input, j);
      }
    }
  }

  SECTION(SmallData)
  {
    for(const memsafegets_data &d : short_data)
    {
      mfopen(d.input, strlen(d.input), &mf);

      for(j = 0; j < arraysize(d.output); j++)
      {
        char *result = mfsafegets(buffer, SHORT_BUF_LEN, &mf);
        if(result && d.output[j])
        {
          ASSERTCMP(result, d.output[j], "%s: %d", d.input, j);
        }
        else
          ASSERTEQ(result, d.output[j], "%s: %d", d.input, j);
      }
    }
  }
}
