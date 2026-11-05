/* MegaZeux
 *
 * Copyright (C) 2012-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Boyer-Moore string search implementation.
 * These functions are currently only used by the editor.
 *
 * Portions of this implementation are based on the text description of the
 * Boyer-Moore algorithm on Wikipedia:
 *
 * https://en.wikipedia.org/wiki/Boyer–Moore_string-search_algorithm
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_compare.h"
#include "string_search.h"
#include "../util.h"

#if defined(CONFIG_EDITOR) || defined(MZX_UNIT_TESTS)
#define ENABLE_STRING_SEARCH
#endif

#ifdef ENABLE_STRING_SEARCH

/**
 * Determine if the position of B starting at pos is also a prefix of B.
 */
static boolean prefix_check(const unsigned char *B, const size_t b_len,
 const size_t pos, boolean ignore_case)
{
  if(ignore_case)
    return !memcasecmp(B, B + pos, b_len - pos);
  else
    return !memcmp(B, B + pos, b_len - pos);
}

/**
 * If the position of B starting at pos is also a suffix of B, return the
 * length of this suffix. Otherwise, return 0.
 */
static size_t get_suffix_length(const unsigned char *B, const size_t b_len,
 const size_t pos, boolean ignore_case)
{
  size_t i;

  if(ignore_case)
  {
    for(i = 0; i < pos; i++)
      if(mzx_tolower(B[pos - i]) != mzx_tolower(B[b_len - i - 1]))
        break;
  }
  else
    for(i = 0; i < pos; i++)
      if(B[pos - i] != B[b_len - i - 1])
        break;

  return i;
}

#endif /* ENABLE_STRING_SEARCH */

/**
 * Initialize a string search index for a needle substring to be used in
 * string_search(). This only needs to be performed once per search string.
 * This may be called subsequently on the same search_string_data struct to
 * prepare it for a different needle string. The needle string does not need
 * to be null-terminated.
 *
 * @param B             Pointer to needle substring.
 * @param b_len         Length of needle substring.
 * @param data          Struct to store string search index info to.
 * @param ignore_case   True for case-insensitive; false for case-sensitive.
 */
void mzx_string_search_index(const void *B, const size_t b_len,
 struct mzx_string_search_data *data, boolean ignore_case)
{
#ifdef ENABLE_STRING_SEARCH
  unsigned int *bad_char = data->bad_char;
  unsigned int *good_suffix = data->good_suffix;
  const unsigned char *start = (const unsigned char *)B;
  const unsigned char *end = start + b_len - 1;
  const unsigned char *pos;
  ssize_t i;

  assert(B);
  assert(data);

  /**
   * Bad characters rule.
   *
   * If a character is encountered that doesn't exist in the pattern string,
   * the current pattern position can be advanced entirely past the bad
   * character. If the bad character encountered is in the pattern string but
   * is out of sequence, advance the current position such that the last
   * occurence of the bad character in the pattern is aligned with the position
   * where the mismatch occurred.
   *
   * If the position of the bad character in the pattern string is to the right
   * of the mismatch position the algorithm must rely on either the good prefix
   * and good suffix rules (see below) or advance the current pattern position
   * right by a single character (see string_search).
   */
  for(i = 0; i < 256; i++)
    bad_char[i] = b_len;

  if(!ignore_case)
  {
    for(pos = start; pos < end; pos++)
      bad_char[*pos] = end - pos;
  }
  else
  {
    for(pos = start; pos < end; pos++)
      bad_char[mzx_tolower(*pos)] = end - pos;

    // Duplicating the lowercase values over the uppercase values helps avoid
    // an extra tolower in the search function.
    memcpy(bad_char + 'A', bad_char + 'a', sizeof(unsigned int) * 26);
  }

  /**
   * Currently the space for the good prefix/suffix table is statically
   * allocated as part of the data struct, so only generate it if it will fit
   * in the provided buffer. The currently provided buffer size is good enough
   * for all current applications of this algorithm (as of writing this).
   */
  data->has_good_suffix_table = (b_len <= MZX_STRING_SEARCH_SUFFIX_MAX);

  if(data->has_good_suffix_table)
  {
    /**
     * Good prefix rule.
     *
     * Given a partial match in pattern string "AAAbbb*bbbCCC" where CCC is
     * a suffix of the matched portion of the pattern, * is the current position
     * and is a mismatch, and AAA is a prefix of the pattern string the same
     * length as CCC, shift the current position such that AAA aligns with the
     * old position of CCC. In the loop below, i == the position of *.
     * CCC can NOT be the entire matched portion of the prefix.
     *
     * Otherwise, the current position can be shifted entirely past CCC (except
     * for the case below).
     */
    size_t last_prefix_position = b_len;

    for(i = (ssize_t)b_len - 1; i >= 0; i--)
    {
      if(prefix_check(start, b_len, i + 1, ignore_case))
        last_prefix_position = i + 1;

      good_suffix[i] = MIN(b_len, b_len - 1 - i + last_prefix_position);
    }

    /**
     * Good suffix rule (takes precedence over the good prefix rule as the good
     * prefix rule could otherwise skip matches in this situation).
     *
     * Given a partial match in pattern string "bbbAAXbbb*CCC" where CCC is
     * a suffix of the pattern string, AAX is a suffix of some position within
     * the pattern string, and * is the current position and is a mismatch,
     * if AAX is equal to CCC then shift the current position so AAX aligns
     * with the old position of CCC. CCC and AAX can overlap. In the loop
     * below, i == the position of X, but the position that will use this value
     * is the position of *.
     *
     * Otherwise, do not modify the table (leaving the good prefix value
     * generated above unmodified).
     */
    for(i = 0; i < (ssize_t)b_len - 1; i++)
    {
      size_t suffix_len = get_suffix_length(start, b_len, i, ignore_case);
      if(start[i - suffix_len] != start[b_len - suffix_len - 1])
        good_suffix[b_len - suffix_len - 1] = b_len - i - 1 + suffix_len;
    }
  }
#endif /* ENABLE_STRING_SEARCH */
}

/**
 * Search for needle substring B in haystack A. This implementation uses the
 * Boyer-Moore algorithm to improve search performance, particularly for
 * repeated searches. The provided string_search_data struct must be
 * initialized by calling string_search_index() before it is used. Neither the
 * needle string nor the haystack string need to be null-terminated.
 *
 * If data is NULL, it will be allocated and initialized within this function
 * and freed prior to returning. Only provide NULL if this search will occur
 * once and you don't care about the allocation overhead.
 *
 * If a string search will only be performed once for a given needle or if the
 * needle is short, and if both strings are null-terminated, the standard
 * library function strstr should probably be used instead.
 *
 * @param A             Pointer to haystack string.
 * @param a_len         Length of haystack string.
 * @param B             Pointer to needle substring.
 * @param b_len         Length of needle substring.
 * @param data          Struct containing string search index information.
 * @param ignore_case   True for case-insensitive; false for case-sensitive.
 */
const void *mzx_string_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const struct mzx_string_search_data *data,
 boolean ignore_case)
{
#ifdef ENABLE_STRING_SEARCH
  const unsigned char *a = (const unsigned char *)A;
  const unsigned char *b = (const unsigned char *)B;
  struct mzx_string_search_data *alloc = NULL;
  const unsigned int *bad_char_index;
  const unsigned int *good_suffix_index;
  const void *result = NULL;
  ssize_t i = b_len - 1;
  ssize_t idx, idx2;
  ssize_t j;
  boolean has_good_suffix_table;

  assert(A);
  assert(B);

  if(!data)
  {
    alloc = (struct mzx_string_search_data *)cmalloc(sizeof(struct mzx_string_search_data));
    mzx_string_search_index(B, b_len, alloc, ignore_case);
    data = alloc;
  }
  bad_char_index = data->bad_char;
  good_suffix_index = data->good_suffix;
  has_good_suffix_table = data->has_good_suffix_table;

  /**
   * Note: b_len - j is the "naive" skip amount, i.e. it results in effectively
   * shifting the pattern position to the right by a single character. If the
   * good prefix/suffix table is not available it must be used in place of the
   * value that would have been read from that table.
   */
  if(!ignore_case)
  {
    while((size_t)i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && a[i] == b[j])
        j--, i--;

      if(j == -1)
      {
        result = (const void *)(a + i + 1);
        break;
      }

      idx = bad_char_index[a[i]];
      idx2 = has_good_suffix_table ? good_suffix_index[j] : b_len - j;
      i += MAX(idx, idx2);
    }
  }
  else
  {
    while((size_t)i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && mzx_tolower(a[i]) == mzx_tolower(b[j]))
        j--, i--;

      if(j == -1)
      {
        result = (const void *)(a + i + 1);
        break;
      }

      idx = bad_char_index[a[i]];
      idx2 = has_good_suffix_table ? good_suffix_index[j] : b_len - j;
      i += MAX(idx, idx2);
    }
  }

  if(alloc)
    free(alloc);

  return result;

#else /* !ENABLE_STRING_SEARCH */
  return NULL;
#endif
}
