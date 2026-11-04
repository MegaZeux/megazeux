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
#include "../../src/algorithm/string_sep.c"

struct strsep_test
{
  const char *base;
  const char *delim;
  const char *expected[8];
  size_t expected_nuls[8];
};

static constexpr strsep_test data[] =
{
  { "a string", " ",
    { "a", "string", nullptr },
    { 1, 8 }},

  { "line 1\nline 2\nline 3\n", "\n",
    { "line 1", "line 2", "line 3", "", nullptr },
    { 6, 13, 20, 21 }},

  { "a:b::c:d", ":",
    { "a", "b", "", "c", "d", nullptr },
    { 1, 3, 4, 6, 8 }},

  { "that's multiple delimiter's", " '",
    { "that", "s", "multiple", "delimiter", "s", nullptr },
    { 4, 6, 15, 25, 27 }},

  /* Delimiter in haystack inaccessible due to C string nul terminator */
  { "c_str 1\0[]", "[",
    { "c_str 1", nullptr },
    { 7 }},

  /* Delimiter in needle inaccessible due to C string nul terminator */
  { "c_str 2", "\0 ",
    { "c_str 2", nullptr },
    { 7 }},
};

UNITTEST(mzx_strsep)
{
  char *tmp;
  char *tmp2;
  char *ret;
  char *ret2;

  SECTION(NullString)
  {
    tmp = NULL;
    ASSERTEQ(mzx_strsep(&tmp, "\n"), nullptr, "");
    tmp = NULL;
    ASSERTEQ(strsep(&tmp, "\n"), nullptr, "");
  }

  /* These cases are unspecified for GNU/BSD strsep? */
  SECTION(BadArgs)
  {
    ASSERTEQ(mzx_strsep(NULL, "abc"), nullptr, "");

    char string[] = "string";
    tmp = string;
    ASSERTEQ(mzx_strsep(&tmp, NULL), nullptr, "");

    tmp = NULL;
    ASSERTEQ(mzx_strsep(&tmp, NULL), nullptr, "");
  }

  SECTION(Strings)
  {
    char buf[256];
    char buf2[256];
    char buf3[256];
    size_t len;
    size_t i;

    for(const strsep_test &d : data)
    {
      strcpy(buf, d.base);
      strcpy(buf2, d.base);
      strcpy(buf3, d.base);
      len = strlen(d.base) + 1;

      tmp = buf;
      tmp2 = buf2;
      for(i = 0; i < (size_t)arraysize(d.expected); i++)
      {
        char *oldtmp = tmp;
        char *oldtmp2 = tmp2;

        ret = mzx_strsep(&tmp, d.delim);
        ret2 = strsep(&tmp2, d.delim);

        /* Return value should be identical to prior cursor value */
        ASSERTEQ(ret, oldtmp, "%s:%zu", d.base, i);
        ASSERTEQ(ret2, oldtmp2, "%s:%zu", d.base, i);

        if(ret == nullptr)
        {
          ASSERTEQ(ret2, nullptr, "%s:%zu", d.base, i);
          break;
        }

        /* Token value should be the same */
        ASSERTCMP(ret, d.expected[i], "%s:%zu", d.base, i);
        ASSERTCMP(ret2, d.expected[i], "%s:%zu", d.base, i);

        /* Overwritten character should be in the delimiter needle */
        ASSERT(strchr(d.base, ret[strlen(ret)]), "%s:%zu", d.base, i);

        /* Simulate the buffer and verify the actual buffer is the same. */
        buf3[d.expected_nuls[i]] = '\0';
        ASSERTMEM(buf, buf3, len, "%s:%zu", d.base, i);
        ASSERTMEM(buf2, buf3, len, "%s:%zu", d.base, i);
      }
    }
  }
}
