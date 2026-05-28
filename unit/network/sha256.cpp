/* MegaZeux
 *
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "../../src/platform/platform.h"

#include "../../src/network/sha256.c"

#define ASSETS_DIR "assets"
#define IO_DATA_DIR "unit/io/data"

struct SHA256_data
{
  const char *input;
  uint32_t result[8];
};

UNITTEST(SHA256String)
{
  static const SHA256_data data[] =
  {
    {
      "abc",
      {
        0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
        0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
      }
    },
    {
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      {
        0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039,
        0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1
      }
    }
  };
  struct SHA256_ctx ctx;

  for(const SHA256_data &d : data)
  {
    SHA256_init(&ctx);
    SHA256_update(&ctx, d.input, strlen(d.input));
    SHA256_final(&ctx);
    ASSERTMEM(ctx.H, d.result, sizeof(ctx.H), "%s", d.input);
  }
}

UNITTEST(SHA256File)
{
  static const SHA256_data data[] =
  {
    {
      ASSETS_DIR "/ascii.chr",
      {
        0x657ca658, 0x8b6bf729, 0xf0ed71a3, 0xd3c781a4,
        0x65bd30cd, 0xfb11eb5f, 0xc7e27594, 0xc0717ea1
      }
    },
    {
      ASSETS_DIR "/smzx.pal",
      {
        0x60db72c5, 0xe0bcac9b, 0xd2093019, 0x4d45f363,
        0xb49db55d, 0x93f4b404, 0xa488fbb8, 0x00d70627
      }
    },
    {
      IO_DATA_DIR "/dch1.txt",
      {
        0xd872892d, 0xdeb2a55f, 0x0207731f, 0x04290400,
        0xa619cc08, 0x07abd24e, 0x2f2b715d, 0xf27ae020
      }
    },
    {
      IO_DATA_DIR "/CN_S.CHR",
      {
        0x2668c893, 0x09ec7ebf, 0xd7b387a3, 0x11b21395,
        0xd5e6b34f, 0xd67f3e81, 0x0b407b84, 0x035667ef
      }
    },
    {
      IO_DATA_DIR "/FREAKSOF.MZX",
      {
        0x0a993ca6, 0x82909e78, 0x020f9905, 0x2083704e,
        0xfcfa7929, 0x3d3fb8cd, 0xa6ccba4c, 0xedd0fa15
      }
    },
    {
      IO_DATA_DIR "/CT_LEVEL.MOD",
      {
        0x3cdda2de, 0xb45d0eac, 0xaa0c4f47, 0xcd8acd77,
        0xe377ea2d, 0x3db5e0b4, 0x5046902f, 0x6722c2b1
      }
    },
  };
  struct SHA256_ctx ctx;
  char buffer[8192];

  for(const SHA256_data &d : data)
  {
    SHA256_init(&ctx);

    FILE *fp = fopen_unsafe(d.input, "rb");
    ASSERT(fp, "%s", d.input);

    fseek(fp, 0, SEEK_END);
    ssize_t file_len = ftell(fp);
    rewind(fp);

    ssize_t j = 0;
    while(j < file_len)
    {
      ssize_t len = Unit::min(file_len - j, (ssize_t)arraysize(buffer));
      size_t ret = fread(buffer, len, 1, fp);
      ASSERTEQ(ret, 1, "%s", d.input);

      SHA256_update(&ctx, buffer, len);
      j += len;
    }
    fclose(fp);
    SHA256_final(&ctx);
    ASSERTMEM(ctx.H, d.result, sizeof(ctx.H), "%s", d.input);
  }
}
