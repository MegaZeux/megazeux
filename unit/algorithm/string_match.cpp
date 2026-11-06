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
#include "../../src/algorithm/string_match.c"

struct wildcard_test_data
{
  const char *haystack;
  size_t haystack_len;
  const char *needle;
  size_t needle_len;
  boolean case_result;
  boolean ignore_case_result;
};

static constexpr const wildcard_test_data no_wildcards[] =
{
  { "abcde",      5,  "abcde",            5,  true,   true },
  { "aBcDe",      5,  "AbCdE",            5,  false,  true },
  { "abcdef",     6,  "abcde",            5,  false,  false },
  { "abcd",       4,  "abcde",            5,  false,  false },
  { "",           1,  "",                 1,  true,   true },
  { "",           0,  "",                 0,  true,   true },
  { "",           0,  "",                 1,  false,  false },
  { "",           1,  "",                 0,  false,  false },
  { "ab\0cd",     5,  "ab\0cd",           5,  true,   true },
  { "aB\0cD",     5,  "Ab\0Cd",           5,  false,  true },
  { "@a`b@",      5,  "`a@b`",            5,  false,  false },
  { "[a{a[",      5,  "{a[b{",            5,  false,  false },
};

static constexpr const wildcard_test_data escapes[] =
{
  { "a?%\\",      4,  "a\\?\\%\\\\",      7,  true,   true },
  { "\\%?a",      4,  "\\\\\\%\\?a",      7,  true,   true },
  { "A?%\\",      4,  "a\\?\\%\\\\",      7,  false,  true },
  { "\\%?a",      4,  "\\\\\\%\\?A",      7,  false,  true },
  { "bcde",       4,  "b\\?\\%\\\\",      7,  false,  false },
  { "edcb",       4,  "\\\\\\%\\?b",      7,  false,  false },
  { "abcde",      5,  "\\%",              2,  false,  false },
  { "%",          1,  "\\%",              2,  true,   true },
  { "edcba",      5,  "\\?\\?\\?\\?\\?",  10, false,  false },
  { "?????",      5,  "\\?\\?\\?\\?\\?",  10, true,   true },
  { "abc\\",      4,  "abc\\",            4,  true,   true },
  { "AbC\\",      4,  "aBc\\",            4,  false,  true },
  { "abc\\",      4,  "abc\\\\",          5,  true,   true },
  { "AbC\\",      4,  "aBc\\\\",          5,  false,  true },
  { "a\\b\\c",    5,  "a\\b\\c",          5,  true,   true },
  { "A\\b\\C",    5,  "a\\B\\c",          5,  false,  true },
};

static constexpr const wildcard_test_data questionmark[] =
{
  { "abcde",      5,  "a?c?e",            5,  true,   true },
  { "ABCDE",      5,  "a?c?e",            5,  false,  true },
  { "abcde",      5,  "A?C?E",            5,  false,  true },
  { "abcd",       4,  "?????",            5,  false,  false },
  { "abcdef",     6,  "?????",            5,  false,  false },
  { "abcde",      5,  "?\\??\\??",        7,  false,  false },
  { "?b?d?",      5,  "\\??\\??\\?",      8,  true,   true },
  { "abcde",      5,  "?\\%?",            4,  false,  false },
  { "ab%de",      5,  "??\\%??",          6,  true,   true },
  { "??\\???",    6,  "?????",            5,  false,  false },
  { "\0\0\0\0\0", 5,  "?????",            5,  true,   true },
  { "?????",      5,  "\0\0\0\0\0",       5,  false,  false },
};

static constexpr const wildcard_test_data percent[] =
{
  { "abcde",      5,  "%",                1,  true,   true },
  { "ABCDE",      5,  "%",                1,  true,   true },
  { "%%%%%%%",    7,  "%",                1,  true,   true },
  { "%",          1,  "%%%%%%%",          7,  true,   true },
  { "\\%",        1,  "%",                1,  true,   true },
  { "\0\0\0\0\0", 5,  "%",                1,  true,   true },
  { "",           0,  "%",                1,  true,   true },
  { "",           0,  "%%%%%",            5,  true,   true },
  { "%",          1,  "",                 0,  false,  false },
};

static constexpr const wildcard_test_data percent_end[] =
{
  { "abcde",      5,  "abc%",             4,  true,   true },
  { "AbCcE",      5,  "aBc%",             4,  false,  true },
  { "abcDE",      5,  "abc%",             4,  true,   true },
  { "sdjkflasdk", 10, "s%%",              3,  true,   true },
  { "z",          1,  "z%%%%%",           6,  true,   true },
  { "Z",          1,  "z%%%%%",           6,  false,  true },
  { "z",          1,  "Z%%%%%",           6,  false,  true },
  { "%abc",       4,  "\\?%",             3,  false,  false },
  { "%abc",       4,  "\\%%",             3,  true,   true },
  { "?\\?abc",    6,  "\\?\\\\%",         5,  true,   true },
  { "ab",         2,  "ba%",              3,  false,  false },
};

static constexpr const wildcard_test_data percent_start[] =
{
  { "abcde",      5,  "%cde",             4,  true,   true },
  { "AbCdE",      5,  "%cDe",             4,  false,  true },
  { "!?%?!",      5,  "%!",               2,  true,   true },
  { "z",          1,  "%%%%%z",           6,  true,   true },
  { "Z",          1,  "%%%%%z",           6,  false,  true },
  { "z",          1,  "%%%%%Z",           6,  false,  true },
  { "\\%\\%",     4,  "%\\\\%",           4,  true,   true },
  { "\\?%abc",    6,  "%\\%abc",          6,  true,   true },
  { "ab",         2,  "%ba",              3,  false,  false },
  { "ab",         2,  "%a",               2,  false,  false },
};

static constexpr const wildcard_test_data percent_mixed[] =
{
  { "abcde",      5,  "a%e",              3,  true,   true },
  { "aB_dE",      5,  "Ab%De",            5,  false,  true },
  { "abcde",      5,  "%abcde%",          7,  true,   true },
  { "abcde",      5,  "%a%b%c%d%e%",      11, true,   true },
  { "ab___de",    7,  "%b%d%",            5,  true,   true },
  { "abababca",   8,  "%abc%",            5,  true,   true },
  { "abCbaBca",   8,  "%abc%",            5,  false,  true },
  { "ab_bc_cd",   8,  "%abc%",            5,  false,  false },
  { "edcba",      5,  "e%b",              3,  false,  false },
  { "edcba",      5,  "%a%a",             4,  false,  false },
  { "ab%%cd",     6,  "ab%cd",            5,  true,   true },
  { "ab%%cd",     6,  "ab\\%cd",          6,  false,  false },
  { "ab\\%cd",    6,  "ab%%cd",           6,  true,   true },
  { "ab%cd",      5,  "ab\\%%cd",         7,  true,   true },
  { "ab\\%cd",    6,  "ab\\%%cd",         7,  false,  false },
  { "\x1f\0\xfe", 4,  "%\x1f%",           4,  true,   true },
  { "\x0f\0\xfe", 4,  "%\x1f%",           4,  false,  false },
  { "@abc",       4,  "`%c",              3,  false,  false },
  { "abc[",       4,  "a%{",              3,  false,  false },
  { "abc",        3,  "a%%%%%%%%%c",      11, true,   true },
};

static constexpr const wildcard_test_data combined[] =
{
  { "abc",        3,  "?%",               2,  true,   true },
  { "abc",        3,  "%?",               2,  true,   true },
  { "zyx",        3,  "?%?%?",            5,  true,   true },
  { "zyx",        3,  "%?%%%?%%%%?",      11, true,   true },
  { "abcde",      5,  "??%??",            5,  true,   true },
  { "abcde",      5,  "?????%",           6,  true,   true },
  { "abcde",      5,  "%?????",           6,  true,   true },
  { "abcde",      5,  "???%???",          7,  false,  false },
  { "abcde",      5,  "%??????",          7,  false,  false },
  { "abcde",      5,  "??????%",          7,  false,  false },
  { "abcde",      5,  "?????%?????",      11, false,  false },
};

static constexpr const wildcard_test_data combined_end[] =
{
  { "abcde",      5,  "a?%",              3,  true,   true },
  { "abcde",      5,  "a%?",              3,  true,   true },
  { "abcde",      5,  "ab?%%%%%?",        9,  true,   true },
  { "abcde",      5,  "a%????%",          7,  true,   true },
  { "abcde",      5,  "abc???%",          7,  false,  false },
  { "AbCdE",      5,  "aBcD?%",           6,  false,  true },
  { "zYxWv",      5,  "ZyX?%?%",          7,  false,  true },
  { "@fdnjdgfd",  9,  "`???%",            5,  false,  false },
  { "`fdnjdgfd",  9,  "@???%",            5,  false,  false },
  { "[mbmkg",     6,  "{%?%?",            5,  false,  false },
  { "{mbmkg",     6,  "[%?%?",            5,  false,  false },
  { "1234",       4,  "1????%",           6,  false,  false },
  { "1234",       4,  "12???%",           6,  false,  false },
  { "1234",       4,  "123??%",           6,  false,  false },
  { "1234",       4,  "1234?%",           6,  false,  false },
  { "1234",       4,  "\\??%",            4,  false,  false },
};

static constexpr const wildcard_test_data combined_start[] =
{
  { "abcde",      5,  "?%e",              3,  true,   true },
  { "abcde",      5,  "%?e",              3,  true,   true },
  { "abcde",      5,  "?%%%??de",         8,  true,   true },
  { "aBcDe",      5,  "%??%CdE",          7,  false,  true },
  { "zYxWv",      5,  "?%?%wV",           6,  false,  true },
  { "kfjssd@",    7,  "???%d`",           6,  false,  false },
  { "kfjssd`",    7,  "???%d@",           6,  false,  false },
  { "ieuwo[",     6,  "%?%o{",            5,  false,  false },
  { "ieuwo{",     6,  "%?%o[",            5,  false,  false },
  { "1234",       4,  "????%4",           6,  false,  false },
  { "1234",       4,  "???%34",           6,  false,  false },
  { "1234",       4,  "??%234",           6,  false,  false },
  { "1234",       4,  "?%1234",           6,  false,  false },
  { "1234",       4,  "?%\\?",            4,  false,  false },
};

static constexpr const wildcard_test_data combined_mixed[] =
{
  { "2.91",       4,  "?.??%",            5,  true,   true },
  { "2.91abcde",  9,  "?.??%",            5,  true,   true },
  { "29.1",       4,  "?.??%",            5,  false,  false },
  { "002 te.mzx", 10, "??? %.mzx",        9,  true,   true },
  { "002 TE.mzx", 10, "??? %.mzx",        9,  true,   true },
  { "002_te.mzx", 10, "??? %.mzx",        9,  false,  false },
  { "002 te.MZX", 10, "??? %.mzx",        9,  false,  true },
  { "aw\0wa",     5,  "?%\0%?",           5,  true,   true },
  { "wawa\0",     5,  "%?\0?%",           5,  false,  false },
  { "wawa\0",     5,  "?%?\0%",           5,  true,   true },
  { "**$**$**",   8,  "*?%$%?*%$%?",      11, true,   true },
  { "**$**$**",   8,  "*?%$%?*%$%?*",     12, true,   true },
  { "**$**$**",   8,  "*??$%?*%$%?*",     12, false,  false },
  { "**$**$**",   8,  "*?%$%?*?$%?*",     12, false,  false },
  { "**$**$**",   8,  "*?%$%?*%$%??*",    13, false,  false },

  /* Following cases copied out of the wildcard testworld: */
  { "abcdef",     6, "%??c??f",           7,  true,   true },
  { "abcdef",     6, "?%c?%f",            6,  true,   true },
  { "abcdef",     6, "a?c?%?f",           7,  true,   true },
  { "abababa",    7, "a%a%a%",            6,  true,   true },
  { "0,0,3,1",    7, "%,3,?",             5,  true,   true },
  { "0,0,3,1",    7, "%,%,3,?",           7,  true,   true },
  { "000\\",      4, "%0\\",              3,  true,   true },
  { "000\\s",     5, "%0\\s",             4,  true,   true },
  { "abc*ABC",    7,  "a%a%bc",           6,  false,  true },
  { "ABC*abc",    7,  "a%A%bc",           6,  false,  true },
  { "a_u_u",      5,  "a%?u%?",           6,  true,   true },
  { "A_U_U",      5,  "a%?u%?",           6,  false,  true },
  { "au_u_",      5,  "a%u?",             4,  true,   true },
  { "AU_U_",      5,  "a%u?",             4,  false,  true },
  /* With these inputs, the old compare_wildcard read the input string
   * out-of-bounds and then wrote to the state array out-of-bounds.
   * Requires including the trailing nul to reproduce with ASan. */
  { "kz*w",       5,  "k%z%?w",           7,  true,   true },
  { "kz*w",       5,  "K%Z%?W",           7,  false,  true },
};

static constexpr const wildcard_test_data combined_long[] =
{
  { "aaaaaaabaaaaaaabaaaaaaabAAAAAAABaaaaaaabaaaaaaabaaaaaaacaaab", 60,
    "%aaaaaaaa%", 10, false, false },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAAAAbaaaaaaabaaa", 60,
    "%aaaaaaab%", 10, true,  true },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAAAAbaaaaaaacaaa", 60,
    "%aaaaaaab%", 10, false, false },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAAAAbaAaAaAaB", 57,
    "%aaaaaaab%", 10, false, true },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAAAAbAaAaAaAb", 57,
    "%aaaaaaab%", 10, false, true },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAbaAaAaAaBaaa", 57,
    "%aaaaaaab%", 10, false, true },
  { "aaaaaabaaaaaabaaaaaabAAAAAAbaaaaaabaaaaaabAAAbAaAaAaAbaaa", 57,
    "%aaaaaaab%", 10, false, true },

  /* Reduced original reproducer of the end-? duplicate final needle bug. */
  { "zt.r.DDO." "tACn..." "x.w" "N..." "erz.B" "r:-i" ".m.FiT.SFR.q" "..g." "G7", 50,
    "ZT?R.Ddo." "%?%???%?%?%%?" "x.W" "%%%%?" "erz.B" "%?%%??" ".M.FiT.sfr.q" "?%?" "g?", 58,
    false, true },
};

static void check_mzx_string_wildcard(const wildcard_test_data &d, bool ignore_case)
{
  if(!d.haystack)
    UNIMPLEMENTED();

  boolean ret = mzx_string_wildcard(d.haystack, d.haystack_len,
   d.needle, d.needle_len, ignore_case);

  ASSERTEQ(ret, ignore_case ? d.ignore_case_result : d.case_result,
   "%s[%zu] ?=%s %s[%zu]",
   d.haystack, d.haystack_len, ignore_case ? "" : "=", d.needle, d.needle_len);
}

UNITTEST(mzx_string_wildcard)
{
  SECTION(BadArgument)
  {
    ASSERTEQ(mzx_string_wildcard(NULL, 0, NULL, 0, false), false, "");
    ASSERTEQ(mzx_string_wildcard(NULL, 123, "abc", 3, false), false, "");
    ASSERTEQ(mzx_string_wildcard("abc", 3, NULL, 456, false), false, "");
  }

  SECTION(NoWildcard)
    test_data_set(check_mzx_string_wildcard, no_wildcards, false);

  SECTION(NoWildcardIgnoreCase)
    test_data_set(check_mzx_string_wildcard, no_wildcards, true);

  SECTION(Escapes)
    test_data_set(check_mzx_string_wildcard, escapes, false);

  SECTION(EscapesIgnoreCase)
    test_data_set(check_mzx_string_wildcard, escapes, true);

  SECTION(QuestionMark)
    test_data_set(check_mzx_string_wildcard, questionmark, false);

  SECTION(QuestionMarkIgnoreCase)
    test_data_set(check_mzx_string_wildcard, questionmark, true);

  SECTION(Percent)
    test_data_set(check_mzx_string_wildcard, percent, false);

  SECTION(PercentIgnoreCase)
    test_data_set(check_mzx_string_wildcard, percent, true);

  SECTION(PercentEnd)
    test_data_set(check_mzx_string_wildcard, percent_end, false);

  SECTION(PercentEndIgnoreCase)
    test_data_set(check_mzx_string_wildcard, percent_end, true);

  SECTION(PercentStart)
    test_data_set(check_mzx_string_wildcard, percent_start, false);

  SECTION(PercentStartIgnoreCase)
    test_data_set(check_mzx_string_wildcard, percent_start, true);

  SECTION(PercentMixed)
    test_data_set(check_mzx_string_wildcard, percent_mixed, false);

  SECTION(PercentMixedIgnoreCase)
    test_data_set(check_mzx_string_wildcard, percent_mixed, true);

  SECTION(Combined)
    test_data_set(check_mzx_string_wildcard, combined, false);

  SECTION(CombinedIgnoreCase)
    test_data_set(check_mzx_string_wildcard, combined, true);

  SECTION(CombinedEnd)
    test_data_set(check_mzx_string_wildcard, combined_end, false);

  SECTION(CombinedEndIgnoreCase)
    test_data_set(check_mzx_string_wildcard, combined_end, true);

  SECTION(CombinedStart)
    test_data_set(check_mzx_string_wildcard, combined_start, false);

  SECTION(CombinedStartIgnoreCase)
    test_data_set(check_mzx_string_wildcard, combined_start, true);

  SECTION(CombinedMixed)
    test_data_set(check_mzx_string_wildcard, combined_mixed, false);

  SECTION(CombinedMixedIgnoreCase)
    test_data_set(check_mzx_string_wildcard, combined_mixed, true);

  SECTION(CombinedLong)
    test_data_set(check_mzx_string_wildcard, combined_long, false);

  SECTION(CombinedLongIgnoreCase)
    test_data_set(check_mzx_string_wildcard, combined_long, true);
}
