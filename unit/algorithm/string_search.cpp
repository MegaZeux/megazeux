/* MegaZeux
 *
 * Copyright (C) 2019-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <string.h>

#include "../../src/algorithm/string_search.c"

struct string_pair
{
  const char *needle;
  const char *haystack;
};

struct string_pair_idx
{
  const char *needle;
  const char *haystack;
  ssize_t where;
};

UNITTEST(Search)
{
  // left: needle, right: haystack
  static const string_pair haystack_none[] =
  {
    { "", "literally anything" },
    { "literally anything", "" },
    { "abcd", "abdacbdacbdabdcabd" },
    {
      "abce",
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
    },
    { "abcf", "s" },
    { "some needle", "some needl" },
    { "dome needle", "aome needle" },
    { "case", "CASE" },
    { "abcde", "cbdebcedbcdebcdbecdbedcbebcdebcdebcedbcbdcbedcbedbcdebcedbc" },
    { "abcde", "cbdebcedbcdebcdbecAbcdEbebcdebABCDEedbcbdcbedcbedbcdebAbcde" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAA"
    },
    { "abcdabc", "abceabceabceabceabc" },
  };
  static const string_pair_idx haystack_once[] =
  {
    { "abcde", "abcde", 0 },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaa", 15 },
    {
      "------------separator",
      "fhs-----------fhsdklfhsdlseparator"
      "aflskdfhsdjklsfhdj------------separator", 52
    },
    { "case", "CASEcaseCASE", 4 },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      141
    },
    { "abcdabc", "abceabcdabceabceabc", 4 },
    { "pfatt",
      "set \"pf\" to "
      "\"('pldir'*6+('pfatt'>0*3)+('pfwalk'=1 ? 1 : 'pfwalk'=3 ? 2 : 0))\"", 26
    },
    {
      "textdone", "set \"macroar_!textdone\" to 0", 14
    },
    {
      "!text", "set \"$macrof_!text:Info\" to \"$\"", 13
    },
    {
      "scripts.txt", "set \"scripts.txt\" to \"FWRITE_OPEN\"", 5
    },
    {
      "$local", "set \"$@\" to \"R&&p_id&&.&$local_('local12')&\"", 24
    },
  };
  static const string_pair haystack_twice[] =
  {
    { "abcde", "abcdeabcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcdaaaaaaaaa" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcd" },
    { "abcd", "abcdabcabcabcabcabcabcbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdabcd" },
    {
      "------------separator",
      "fhsfhsdklfhsdl------------separator"
      "aflskdfhsdjklsfhdj------------separator--"
    },
    { "case", "caseCASEcase" },
    { "abba", "abbabba" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },
    { "abcdabc", "abceabcdabcdabceabc" },
  };
  struct mzx_string_search_data data;
  const char *A;
  const char *B;
  const void *dest;

  SECTION(OneOff)
  {
    for(const string_pair &d : haystack_none)
    {
      A = d.haystack;
      B = d.needle;

      ASSERT(!mzx_string_search(A, strlen(A), B, strlen(B), nullptr, false), "%s", A);
    }

    for(const string_pair_idx &d : haystack_once)
    {
      A = d.haystack;
      B = d.needle;
      ssize_t where = d.where;

      dest = mzx_string_search(A, strlen(A), B, strlen(B), nullptr, false);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + strlen(A), "%s", A);
      ASSERTEQ((const char *)dest - A, where, "%s", A);
    }

    for(const string_pair &d : haystack_twice)
    {
      A = d.haystack;
      B = d.needle;

      dest = mzx_string_search(A, strlen(A), B, strlen(B), nullptr, false);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + strlen(A), "%s", A);
    }
  }

  SECTION(Repeating)
  {
    for(const string_pair_idx &d : haystack_once)
    {
      const char *dest;
      A = d.haystack;
      B = d.needle;
      ssize_t where = d.where;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      mzx_string_search_index(B, b_len, &data, false);

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, false);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      ASSERTEQ((const char *)dest - A, where, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERT(!mzx_string_search(A, a_len, B, b_len, &data, false), "%s", A);
    }

    for(const string_pair &d : haystack_twice)
    {
      const char *dest;
      A = d.haystack;
      B = d.needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      mzx_string_search_index(B, b_len, &data, false);

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, false);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, false);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERT(!mzx_string_search(A, a_len, B, b_len, &data, false), "%s", A);
    }
  }
}

UNITTEST(SearchCaseInsensitive)
{
  // left: needle, right: haystack
  static const string_pair haystack_none[] =
  {
    { "", "literally anything" },
    { "literally anything", "" },
    { "abcd", "abdacbdacbdabdcabd" },
    {
      "abce",
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
    },
    { "abcf", "s" },
    { "some needle", "some needl" },
    { "dome needle", "aome needle" },
    { "abcde", "cbdebcedbcdebcdbecdbedcbebcdebcdebcedbcbdcbedcbedbcdebcedbc" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaa",
      "baaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },
    { "abcdabc", "abceABCeabceABCeabc" },
  };
  static const string_pair haystack_once[] =
  {
    { "abcde", "abcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaa" },
    {
      "------------separator",
      "fhs-----------fhsdklfhsdlseparator"
      "aflskdfhsdjklsfhdj------------separator"
    },
    { "case", "CASE" },
    {
      "aAAAAAaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaa"
      "baaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaa"
    },
    { "abcdabc", "abceaBcdAbCeabceabc" },
  };
  static const string_pair haystack_twice[] =
  {
    { "abcde", "abcdeabcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcdaaaaaaaaa" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcd" },
    { "abcd", "abcdabcabcabcabcabcabcbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdabcd" },
    {
      "------------separator",
      "fhsfhsdklfhsdl------------separator"
      "aflskdfhsdjklsfhdj------------separator--"
    },
    { "case", "caseCASE" },
    { "abba", "abbabba" },
    { "abcde", "cbdebcedbcdebcdbecAbcdEbebcdebABCDEedbcbdcbedcbedbcdebAbcdb" },
    {
      "aaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAA"
      "baaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaa"
    },
    { "abcdabc", "abceABCDabcDabceABC" },
  };
  struct mzx_string_search_data data;
  const char *A;
  const char *B;
  const void *dest;

  SECTION(OneOff)
  {
    for(const string_pair &d : haystack_none)
    {
      A = d.haystack;
      B = d.needle;

      ASSERT(!mzx_string_search(A, strlen(A), B, strlen(B), NULL, true), "%s", A);
    }

    for(const string_pair &d : haystack_once)
    {
      A = d.haystack;
      B = d.needle;

      dest = mzx_string_search(A, strlen(A), B, strlen(B), NULL, true);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + strlen(A), "%s", A);
    }

    for(const string_pair &d : haystack_twice)
    {
      A = d.haystack;
      B = d.needle;

      dest = mzx_string_search(A, strlen(A), B, strlen(B), NULL, true);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + strlen(A), "%s", A);
    }
  }

  SECTION(Repeating)
  {
    for(const string_pair &d : haystack_once)
    {
      const char *dest;
      A = d.haystack;
      B = d.needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      mzx_string_search_index(B, b_len, &data, true);

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, true);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERT(!mzx_string_search(A, a_len, B, b_len, &data, true), "%s", A);
    }

    for(const string_pair &d : haystack_twice)
    {
      const char *dest;
      A = d.haystack;
      B = d.needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      mzx_string_search_index(B, b_len, &data, true);

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, true);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      dest = (const char *)mzx_string_search(A, a_len, B, b_len, &data, true);
      ASSERT(dest, "%s", A);
      ASSERT(dest >= A && dest < A + a_len, "%s", A);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERT(!mzx_string_search(A, a_len, B, b_len, &data, true), "%s", A);
    }
  }
}
