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
#include "../../src/algorithm/arch.h"
#include "../../src/algorithm/rng.h"
#include "../../src/algorithm/string_xor.h"

static constexpr uint8_t data[] =
{
  196, 230, 7, 106, 173, 228, 24, 94, 30, 4, 81, 245, 199, 188, 205, 223, 213,
  31, 41, 149, 200, 113, 22, 166, 64, 96, 226, 8, 25, 23, 195, 45, 105, 79, 17,
  231, 234, 110, 70, 140, 255, 204, 172, 35, 60, 42, 192, 212, 232, 252, 111,
  21, 177, 221, 241, 253, 72, 47, 215, 240, 32, 88, 86, 175, 202, 100, 193,
  208, 65, 83, 119, 134, 85, 98, 183, 46, 233, 164, 129, 167, 184, 227, 210,
  141, 181, 19, 37, 174, 143, 84, 71, 92, 178, 59, 99, 220, 206, 144, 95, 90,
  13, 112, 146, 156, 114, 51, 68, 2, 125, 118, 138, 179, 36, 165, 120, 187,
  237, 246, 247, 222, 155, 74, 108, 242, 10, 80, 159, 170, 20, 34, 137, 135,
  18, 127, 61, 136, 160, 3, 224, 132, 217, 50, 157, 39, 104, 191, 236, 126,
  151, 142, 40, 33, 89, 153, 29, 219, 171, 239, 87, 49, 251, 243, 69, 182, 254,
  66, 48, 107, 128, 158, 53, 148, 124, 57, 9, 28, 249, 216, 15, 43, 163, 201,
  75, 103, 117, 238, 16, 130, 63, 102, 77, 162, 5, 185, 147, 186, 91, 62, 93,
  123, 198, 161, 168, 82, 12, 250, 176, 11, 52, 27, 131, 248, 139, 154, 73, 58,
  76, 109, 225, 1, 54, 133, 229, 194, 116, 207, 101, 14, 211, 67, 97, 0, 121,
  145, 38, 56, 169, 209, 152, 26, 244, 203, 214, 197, 180, 55, 189, 122, 44, 6,
  150, 218, 78, 190, 235, 115
};

UNITTEST(mzx_memxorchr)
{
  uint8_t buf[arraysize(data)];
  size_t x;
  int i;

  SECTION(Full)
  {
    for(x = 0; x < 256; x++)
    {
      memcpy(buf, data, arraysize(data));
      mzx_memxorchr(buf, x, arraysize(data));

      for(i = 0; i < arraysize(data); i++)
        ASSERTEQ(buf[i], data[i] ^ x, "%02zu [%d]", x, i);
    }
  }

  SECTION(Partial)
  {
    struct mzx_rng_state rng{};
    mzx_rng_init(&rng);

    for(x = 0; x < 256; x++)
    {
      int start = mzx_rng_get_random(&rng, 128);
      int end = mzx_rng_get_random(&rng, 128) + 129;

      memcpy(buf, data, arraysize(data));
      mzx_memxorchr(buf + start, x, end - start);

      for(i = 0; i < start; i++)
        ASSERTEQ(buf[i], data[i], "%02zu [%d]", x, i);

      for(; i < end; i++)
        ASSERTEQ(buf[i], data[i] ^ x, "%02zu [%d]", x, i);

      for(; i < arraysize(data); i++)
        ASSERTEQ(buf[i], data[i], "%02zu [%d]", x, i);
    }
  }
}
