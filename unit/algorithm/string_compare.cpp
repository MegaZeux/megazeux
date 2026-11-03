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
#include "../../src/algorithm/string_compare.c"

#include <string.h>

struct string_pair
{
  const char *a;
  const char *b;
};

struct string_pair_aligned
{
  alignstr<uint64_t> a;
  alignstr<uint64_t> b;
};

struct bad_string_pair
{
  const char *a;
  const char *b;
  int expected;
};

struct bad_string_pair_aligned
{
  alignstr<uint64_t> a;
  alignstr<uint64_t> b;
  int expected;
};

UNITTEST(mzx_tolower)
{
  int i;
  for(i = 0; i < 256; i++)
  {
    if(i >= 'A' && i <= 'Z')
      ASSERTEQ(mzx_tolower(i), (i - 'A' + 'a'), "");
    else
      ASSERTEQ(mzx_tolower(i), i, "");
  }
}

UNITTEST(Matching)
{
  static const string_pair pairs[] =
  {
    { "", "" },
    { " ", " " },
    { "abcde", "ABCDE" },
    { "testTESTtest", "TESTtestTEST" },
    { "@!%$*^!@$&#!*([]", "@!%$*^!@$&#!*([]" },
    { u8"śśśśśśśś", u8"śśśśśśśś" },
  };

  static const string_pair_aligned pairs64[] =
  {
    { "", "" },
    { " ", " " },
    { "abcde", "ABCDE" },
    { "testTESTtestTEST", "TESTtestTESTtest" },
    { "aaaaaaaabaaaaaaabaaaaaaabaaaaaaa", "aaaaaaaabaaaaaaabaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaaaaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "@!%$*^!@$&#!*([]", "@!%$*^!@$&#!*([]" },
    { u8"śśśśśśśś", u8"śśśśśśśś" },
  };
  const char *a;
  const char *b;

  ASSERTEQ((size_t)memcasecmp, (size_t)mzx_memcasecmp, "");
  ASSERTEQ((size_t)memcasecmp32, (size_t)mzx_memcasecmp32, "");

  SECTION(strcasecmp)
  {
    for(const string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strcasecmp(a, b), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strcasecmp(a, b), "%s", a);
    }
  }

  SECTION(strncasecmp)
  {
    for(const string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strncasecmp(a, b, strlen(a) + 1), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strncasecmp(a, b, strlen(a) + 1), "%s", a);
    }
  }

  SECTION(memcasecmp)
  {
    for(const string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!memcasecmp(a, b, strlen(a)), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();

      ASSERTEQ((size_t)a & (ALIGN_64_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_64_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!memcasecmp(a, b, strlen(a)), "%s", a);
    }
  }

  SECTION(memcasecmp32)
  {
    for(const string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!memcasecmp32(a, b, strlen(a)), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();

      ASSERTEQ((size_t)a & (ALIGN_32_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_32_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!memcasecmp32(a, b, strlen(a)), "%s", a);
    }
  }
}

UNITTEST(NoMatch)
{
  static const bad_string_pair pairs[] =
  {
    { "-", "_", ('-' - '_') },
    { "abcde", "ABCDF", ('e' - 'f') },
    { "testTESTtest", "TASTtastTAST", ('e' - 'a') },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b", ('a' - 'b') },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś", 1 },
  };

  static const bad_string_pair_aligned pairs64[] =
  {
    { "-", "_", ('-' - '_') },
    { "testTESTtestTEST", "TASTtastTASTtast", ('e' - 'a') },
    { "aaaaaaaa*aaaaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'b') },
    { "aaaaaaaab*aaaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaaba*aaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaa*aaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaa*aaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaa*aaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaaa*aBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaaaa*BAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b", ('a' - 'b') },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś", 1 },
  };
  const char *a;
  const char *b;
  int expected;

  SECTION(strcasecmp)
  {
    for(const bad_string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(mzx_strcasecmp(a, b), expected, "%s", a);
    }

    for(const bad_string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(mzx_strcasecmp(a, b), expected, "%s", a);
    }
  }

  SECTION(strncasecmp)
  {
    for(const bad_string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(mzx_strncasecmp(a, b, strlen(a)), expected, "%s", a);
    }

    for(const bad_string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(mzx_strncasecmp(a, b, strlen(a)), expected, "%s", a);
    }
  }

  SECTION(memcasecmp)
  {
    for(const bad_string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(memcasecmp(a, b, strlen(a)), expected, "%s", a);
    }

    for(const bad_string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;

      ASSERTEQ((size_t)a & (ALIGN_64_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_64_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(memcasecmp(a, b, strlen(a)), expected, "%s", a);
    }
  }

  SECTION(memcasecmp32)
  {
    for(const bad_string_pair &p : pairs)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(memcasecmp32(a, b, strlen(a)), expected, "%s", a);
    }

    for(const bad_string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;

      ASSERTEQ((size_t)a & (ALIGN_32_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_32_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERTEQ(memcasecmp32(a, b, strlen(a)), expected, "%s", a);
    }
  }
}
