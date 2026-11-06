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
#include "../../src/algorithm/string_char.c"

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


struct string_pos_test_data
{
  const char *haystack;
  size_t len;
  unsigned char needle;
  int expected;
};

static constexpr struct string_pos_test_data match_data[] =
{
  { "a",          1, 'a',     0 },
  { "bbbbba",     6, 'a',     5 },
  { "bbbbba",     6, 'b',     0 },
  { "string",     6, 'g',     5 },
  { "string",     6, 'n',     4 },
  { "string",     6, 'i',     3 },
  { "string",     6, 'r',     2 },
  { "string",     6, 't',     1 },
  { "string",     6, 's',     0 },
  { "STRING",     6, 'g',     5 },
  { "STRING",     6, 'n',     4 },
  { "STRING",     6, 'i',     3 },
  { "STRING",     6, 'r',     2 },
  { "STRING",     6, 't',     1 },
  { "STRING",     6, 's',     0 },
  { "string",     6, 'G',     5 },
  { "string",     6, 'N',     4 },
  { "string",     6, 'I',     3 },
  { "string",     6, 'R',     2 },
  { "string",     6, 'T',     1 },
  { "string",     6, 'S',     0 },
  { "\0awaw",     5, 'W',     2 },
  { "awawa",      6, '\0',    5 },
  { "str\x14ng",  6, '\x14',  3 },
  { "\xfe\x01",   3, '\x01',  1 },
  { "fread*",     6, '*',     5 },
};

static constexpr struct string_pos_test_data nomatch_data[] =
{
  { "",           0, 'a',     -1 },
  { "b",          1, 'a',     -1 },
  { "bbbbba",     6, 'c',     -1 },
  { "string",     3, 'n',     -1 },
  { "string",     6, '\0',    -1 },
  { "str3ng",     6, 'i',     -1 },
  { "str3ng",     6, '\x14',  -1 },
  { "aaaaa",      5, '\x81',  -1 },
  { "fwrite*",    6, '*',     -1 },
};

static constexpr struct string_pos_test_data align4_data[] =
{
  { "*%%%________", 12, '*', 0 },
  { "%*%%________", 12, '*', 1 },
  { "%%*%________", 12, '*', 2 },
  { "%%%*________", 12, '*', 3 },
  { "____*%%%____", 12, '*', 4 },
  { "____%*%%____", 12, '*', 5 },
  { "____%%*%____", 12, '*', 6 },
  { "____%%%*____", 12, '*', 7 },
  { "________*%%%", 12, '*', 8 },
  { "________%*%%", 12, '*', 9 },
  { "________%%*%", 12, '*', 10 },
  { "________%%%*", 12, '*', 11 },
};

static constexpr struct string_pos_test_data align8_data[] =
{
  { "*%%%%%%%________________", 24, '*', 0 },
  { "%*%%%%%%________________", 24, '*', 1 },
  { "%%*%%%%%________________", 24, '*', 2 },
  { "%%%*%%%%________________", 24, '*', 3 },
  { "%%%%*%%%________________", 24, '*', 4 },
  { "%%%%%*%%________________", 24, '*', 5 },
  { "%%%%%%*%________________", 24, '*', 6 },
  { "%%%%%%%*________________", 24, '*', 7 },
  { "________*%%%%%%%________", 24, '*', 8 },
  { "________%*%%%%%%________", 24, '*', 9 },
  { "________%%*%%%%%________", 24, '*', 10 },
  { "________%%%*%%%%________", 24, '*', 11 },
  { "________%%%%*%%%________", 24, '*', 12 },
  { "________%%%%%*%%________", 24, '*', 13 },
  { "________%%%%%%*%________", 24, '*', 14 },
  { "________%%%%%%%*________", 24, '*', 15 },
  { "________________*%%%%%%%", 24, '*', 16 },
  { "________________%*%%%%%%", 24, '*', 17 },
  { "________________%%*%%%%%", 24, '*', 18 },
  { "________________%%%*%%%%", 24, '*', 19 },
  { "________________%%%%*%%%", 24, '*', 20 },
  { "________________%%%%%*%%", 24, '*', 21 },
  { "________________%%%%%%*%", 24, '*', 22 },
  { "________________%%%%%%%*", 24, '*', 23 },
};

template<int N, typename... args>
static void run_tests(void (*fn)(const string_pos_test_data &, args...),
 const string_pos_test_data (&data)[N], args... fargs)
{
  for(const string_pos_test_data &d : data)
    fn(d, fargs...);
}

static void check_memcasechr(const string_pos_test_data &d)
{
  if(!d.haystack)
    UNIMPLEMENTED();

  const void *ret_a = memcasechr(d.haystack, d.needle, d.len);
  const void *ret_b = mzx_memcasechr(d.haystack, d.needle, d.len);

  int result_a = ret_a ? reinterpret_cast<const char *>(ret_a) - d.haystack : -1;
  int result_b = ret_b ? reinterpret_cast<const char *>(ret_b) - d.haystack : -1;

  ASSERTEQ(result_a, d.expected, "memcasechr(\"%s\",'%c',%zu)", d.haystack, d.needle, d.len);
  ASSERTEQ(result_b, d.expected, "mzx_memcasechr(\"%s\",'%c',%zu)", d.haystack, d.needle, d.len);
}

UNITTEST(mzx_memcasechr)
{
  SECTION(BadArguments)
  {
    ASSERTEQ(memcasechr(nullptr, 'a', 0), nullptr, "");
    ASSERTEQ(memcasechr(nullptr, 'a', 10000), nullptr, "");

    ASSERTEQ(mzx_memcasechr(nullptr, 'a', 0), nullptr, "");
    ASSERTEQ(mzx_memcasechr(nullptr, 'a', 10000), nullptr, "");
  }

  SECTION(Match)
    run_tests(check_memcasechr, match_data);

  SECTION(NoMatch)
    run_tests(check_memcasechr, nomatch_data);

  SECTION(Align4)
    run_tests(check_memcasechr, align4_data);

  SECTION(Align8)
    run_tests(check_memcasechr, align8_data);
}
