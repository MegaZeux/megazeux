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

#include "string_char.h"
#include "string_compare.h"
#include "string_match.h"
#include "string_search.h"

#include <stdint.h>

#define IS_WILDCARD_ESCAPE(chk, chk_end) \
 (*(chk) == '\\' && ((chk) + 1) < (chk_end) && \
  ((chk)[1] == '\\' || (chk)[1] == '?' || (chk)[1] == '%'))

/* Naive wildcard needle search.
 * Note needle_end is the end of the needle including escape chars,
 * but needle_len is the TOTAL number of chars to be matched, not including
 * escape chars. needle contents, needle_end, and needle_len are assumed to
 * be valid.
 */
static inline const uint8_t *mzx_string_wildcard_needle(
 const uint8_t *a, const uint8_t *a_end,
 const uint8_t *needle, const uint8_t *needle_end, size_t needle_len,
 uint8_t needle_endchar, boolean ignore_case)
{
  const uint8_t *start;
  const uint8_t *next;
  const uint8_t *pos;
  size_t needle_len_minus_1;

  if(needle_len == 0) /* ????? */
    return NULL;

  needle_len_minus_1 = needle_len - 1;

  a += needle_len_minus_1;
  while(a < a_end)
  {
    if(ignore_case)
      next = (const uint8_t *)memcasechr(a, needle_endchar, a_end - a);
    else
      next = (const uint8_t *)memchr(a, needle_endchar, a_end - a);
    if(next == NULL)
      break;

    start = next - needle_len_minus_1;
    pos = needle;
    for(a = start; a < next; a++, pos++)
    {
      if(*pos == '?')
      {
        continue;
      }
      else

      if(IS_WILDCARD_ESCAPE(pos, needle_end))
        pos++;

      if(ignore_case ? mzx_tolower(*a) != mzx_tolower(*pos) : *a != *pos)
        break;
    }
    if(a == next)
      return start;

    a = next + 1;
  }
  return NULL;
}

/**
 * FIXME: doc comment
 * b: % wildcard consumes 0 or more of any character.
 *    ? wildcard consumes exactly 1 of any character.
 */
boolean mzx_string_wildcard(const void *_a, size_t a_len, const void *_b,
 size_t b_len, boolean ignore_case)
{
  const uint8_t *a = (const uint8_t *)_a;
  const uint8_t *b = (const uint8_t *)_b;
  const uint8_t *a_end;
  const uint8_t *b_end;
  const uint8_t *needle;
  uint8_t needle_endchar;
  size_t needle_pending_question;
  size_t needle_len;
  ptrdiff_t skip;
  boolean many;

  if(a == NULL || b == NULL)
    return false;

  a_end = a + a_len;
  b_end = b + b_len;
  if(a_end < a || b_end < b)
    return false;

  /* Handle prefix first: */
  while(a < a_end && b < b_end && *b != '%')
  {
    if(*b == '?')
    {
      a++;
      b++;
      continue;
    }
    else

    if(IS_WILDCARD_ESCAPE(b, b_end))
      b++;

    if(ignore_case ? mzx_tolower(*a) != mzx_tolower(*b) : *a != *b)
      return false;

    a++;
    b++;
  }
  /* End of pattern--exit if there also isn't any string left. */
  if(b >= b_end)
    return a >= a_end;

  for(;;)
  {
    /* Merge wildcards, consume the number of ?s.
     * If this wildcard block is the end of the pattern and the string contains
     * at least that many characters, it's an automatic match. */
    skip = 0;
    many = false;
    while(b < b_end)
    {
      if(*b == '?')
        skip++;
      else
      if(*b == '%')
        many = true;
      else
        break;

      b++;
    }
    if(a_end - a < skip)
      return false;
    a += skip;

    if(b >= b_end)
      return many ? true : a >= a_end;

    /* Determine the length of the needle, both in the needle itself and the
     * effective length for the haystack string. Trailing ?s are not allowed;
     * they are easier to handle afterward (usually as part of a %). */
    needle = b;
    needle_len = 0;
    needle_pending_question = 0;
    while(b < b_end && *b != '%')
    {
      if(*b == '?')
      {
        needle_pending_question++;
        b++;
        continue;
      }
      else

      if(IS_WILDCARD_ESCAPE(b, b_end))
        b++;

      if(needle_pending_question > 0)
      {
        needle_len += needle_pending_question;
        needle_pending_question = 0;
      }
      needle_endchar = *(b++);
      needle_len++;
    }
    /* Unconsume trailing ?s */
    if(needle_pending_question > 0)
      b -= needle_pending_question;

    if((size_t)(a_end - a) < needle_len)
      return false;

    /* If this is the end of the pattern, this needle MUST be a suffix. */
    if(b >= b_end)
      a = a_end - needle_len;

    a = mzx_string_wildcard_needle(a, a_end, needle, b, needle_len,
     needle_endchar, ignore_case);

    if(a == NULL || a_end - a < (ptrdiff_t)needle_len)
      return false;

    a += needle_len;
  }
}
