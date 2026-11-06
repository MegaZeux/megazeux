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

struct special_string_pair
{
  const char *a;
  const char *b;
  size_t n;
  int expected;
};

struct special_string_pair_aligned
{
  alignstr<uint64_t> a;
  alignstr<uint64_t> b;
  size_t n;
  int expected;
};

static const special_string_pair fail_for_memcasecmp_only[] =
{
  { "\0a", "\0b", 3, -1 },
  { "h\0b", "h\0a", 4, 1 },
  { "he\0\xfe", "he\0\0", 5, 0xfe },
  { "hew\0\0", "hew\0\xfe", 6, -0xfe },
  { "hewo\0abcdef~", "hewo\0abcdef`", 13, 30 },
  { "hewo >:3\0\n\0\x31\xfc", "hewo >:3\0\n\0\x31\xfch", 14, -'h' },
};

static const special_string_pair_aligned fail_for_memcasecmp_only64[] =
{
  /* Explicit constructors so old GCC versions don't get confused... */
  { alignstr<uint64_t>("\0a"),
    alignstr<uint64_t>("\0b"), 3, -1 },
  { alignstr<uint64_t>("h\0b"),
    alignstr<uint64_t>("h\0a"), 4, 1 },
  { alignstr<uint64_t>("he\0\xfe"),
    alignstr<uint64_t>("he\0\0"), 5, 0xfe },
  { alignstr<uint64_t>("hew\0\0"),
    alignstr<uint64_t>("hew\0\xfe"), 6, -0xfe },
  { alignstr<uint64_t>("hewo\0abcdef~"),
    alignstr<uint64_t>("hewo\0aBCDEf`"), 13, 30 },
  { alignstr<uint64_t>("hewo >:3\0\n\0\x31\xfc"),
    alignstr<uint64_t>("hewo >:3\0\n\0\x31\xfch"), 14, -'h' },
};

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
    /* Explicit constructors so old GCC versions don't get confused... */
    { alignstr<uint64_t>(""),
      alignstr<uint64_t>(""), },
    { alignstr<uint64_t>(" "),
      alignstr<uint64_t>(" "), },
    { alignstr<uint64_t>("abcde"),
      alignstr<uint64_t>("ABCDE"), },
    { alignstr<uint64_t>("testTESTtestTEST"),
      alignstr<uint64_t>("TESTtestTESTtest"), },
    { alignstr<uint64_t>("aaaaaaaabaaaaaaabaaaaaaabaaaaaaa"),
      alignstr<uint64_t>("aaaaaaaabaaaaaaabaaaaaaabaaaaaaa"), },
    { alignstr<uint64_t>("aaaaaaaabaaaaaaaBAAAAAAAbaaaaaaa"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa"), },
    { alignstr<uint64_t>("@!%$*^!@$&#!*([]"),
      alignstr<uint64_t>("@!%$*^!@$&#!*([]"), },
    { alignstr<uint64_t>(u8"śśśśśśśś"),
      alignstr<uint64_t>(u8"śśśśśśśś"), },
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
      ASSERT(!strcasecmp(a, b), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strcasecmp(a, b), "%s", a);
      ASSERT(!strcasecmp(a, b), "%s", a);
    }

    for(const special_string_pair &p : fail_for_memcasecmp_only)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strcasecmp(a, b), "%s", a);
      ASSERT(!strcasecmp(a, b), "%s", a);
    }

    for(const special_string_pair_aligned &p : fail_for_memcasecmp_only64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strcasecmp(a, b), "%s", a);
      ASSERT(!strcasecmp(a, b), "%s", a);
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
      ASSERT(!strncasecmp(a, b, strlen(a) + 1), "%s", a);
    }

    for(const string_pair_aligned &p : pairs64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strncasecmp(a, b, strlen(a) + 1), "%s", a);
      ASSERT(!strncasecmp(a, b, strlen(a) + 1), "%s", a);
    }

    for(const special_string_pair &p : fail_for_memcasecmp_only)
    {
      a = p.a;
      b = p.b;
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strncasecmp(a, b, p.n), "%s", a);
      ASSERT(!strncasecmp(a, b, p.n), "%s", a);
    }

    for(const special_string_pair_aligned &p : fail_for_memcasecmp_only64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      ASSERTEQ(strlen(a), strlen(b), "%s", a);
      ASSERT(!mzx_strncasecmp(a, b, p.n), "%s", a);
      ASSERT(!strncasecmp(a, b, p.n), "%s", a);
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
    /* Explicit constructors so old GCC versions don't get confused... */
    { alignstr<uint64_t>("-"),
      alignstr<uint64_t>("_"), ('-' - '_') },
    { alignstr<uint64_t>("testTESTtestTEST"),
      alignstr<uint64_t>("TASTtastTASTtast"), ('e' - 'a') },
    { alignstr<uint64_t>("aaaaaaaa*aaaaaaaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'b') },
    { alignstr<uint64_t>("aaaaaaaab*aaaaaaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaaba*aaaaaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaabaa*aaaaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaabaaa*aaaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaabaaaa*aaBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaabaaaaa*aBAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("aaaaaaaabaaaaaa*BAAAAAAA"),
      alignstr<uint64_t>("aaaaaaaaBAAAAAAAbaaaaaaa"), ('*' - 'a') },
    { alignstr<uint64_t>("@!%$*^!@$&#!*([a"),
      alignstr<uint64_t>("@!%$*^!@$&#!*([b"), ('a' - 'b') },
    { alignstr<uint64_t>(u8"śśŚśśśŚś"),
      alignstr<uint64_t>(u8"ŚśśśŚśśś"), 1 },
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

    for(const special_string_pair &p : fail_for_memcasecmp_only)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(memcasecmp(a, b, p.n), expected, "%s", a);
    }

    for(const special_string_pair_aligned &p : fail_for_memcasecmp_only64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;

      ASSERTEQ((size_t)a & (ALIGN_64_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_64_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(memcasecmp(a, b, p.n), expected, "%s", a);
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

    for(const special_string_pair &p : fail_for_memcasecmp_only)
    {
      a = p.a;
      b = p.b;
      expected = p.expected;
      ASSERTEQ(memcasecmp32(a, b, p.n), expected, "%s", a);
    }

    for(const special_string_pair_aligned &p : fail_for_memcasecmp_only64)
    {
      a = p.a.c_str();
      b = p.b.c_str();
      expected = p.expected;

      ASSERTEQ((size_t)a & (ALIGN_64_MODULO - 1), 0, "a not aligned");
      ASSERTEQ((size_t)b & (ALIGN_64_MODULO - 1), 0, "b not aligned");
      ASSERTEQ(memcasecmp32(a, b, p.n), expected, "%s", a);
    }
  }
}
