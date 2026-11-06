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

#include "../platform/platform_endian.h"

#include <stdint.h>
#include <string.h>

/**
 * Convert ASCII uppercase characters (65-90) to lowercase characters (97-122)
 * using a lookup table. This is faster than calling glibc's toupper().
 */
const unsigned char mzx_tolower_table[256] =
{
  '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
  '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
  '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
  '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
  ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '\'',
  '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
  '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
  '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
  '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
  'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
  'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
  'x',    'y',     'z',   '[',    '\\',   ']',    '^',    '_',
  '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
  'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
  'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
  'x',    'y',     'z',   '{',    '|',    '}',    '~',    '\x7f',
  '\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
  '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
  '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
  '\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
  '\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
  '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
  '\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
  '\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
  '\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
  '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
  '\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
  '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
  '\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
  '\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
  '\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
  '\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
};

/**
 * Find the position of the first occurrence of a character in a string of a
 * given length, ignoring the case of ASCII characters in both the string and
 * the character. The given string does not need to contain a nul terminator,
 * and if present, this function will treat nul the same as any other value.
 *
 * Note: the len and needle arguments are logically backwards in this function
 * prototype to match the C standard library memchr(), memset(), etc.
 *
 * @param haystack  NON nul-terminated string to search.
 * @param needle    character to find inside of `haystack`.
 * @param len       length of `haystack`, in bytes.
 * @return          a pointer to the first occurrence of `needle` in `haystack`
 *                  or NULL if `needle` is not found.
 */
const void *mzx_memcasechr(const void *_h, unsigned char needle, size_t len)
{
  const uint8_t *haystack;
  const uint8_t *haystack_end;
  size_t align_m;

  if(!_h)
    return NULL;

  haystack = (const uint8_t *)_h;
  haystack_end = haystack + len;

  /* If the needle isn't A-Z or a-z, this search is equivalent to memchr. */
  needle = mzx_tolower(needle);
  if(!(needle >= 'a' && needle <= 'z'))
    return memchr(haystack, needle, len);

  align_m = (size_t)haystack % (ARCHITECTURE_BITS >> 3);
  if(align_m)
  {
    ptrdiff_t align = (ARCHITECTURE_BITS >> 3) - align_m;

    if(align > haystack_end - haystack)
      align = haystack_end - haystack;

    for(; align > 0; align--, haystack++)
      if(mzx_tolower(*haystack) == needle)
        return haystack;
  }

  if(haystack_end - haystack >= (ARCHITECTURE_BITS >> 3))
  {
#if ARCHITECTURE_BITS >= 64
    const uint64_t all_lc = 0x0101010101010101ull * needle;
    const uint64_t tlower = 0x2020202020202020ull;
    const uint64_t sub    = 0x0101010101010101ull;
    const uint64_t mask   = 0x8080808080808080ull;
    uint64_t maybe_zero;

    const uint64_t *hw = (const uint64_t *)haystack;
    const uint64_t *hw_end = (const uint64_t *)((size_t)haystack_end & ~0x7);
#else
    const uint32_t all_lc = 0x01010101u * needle;
    const uint32_t tlower = 0x20202020u;
    const uint32_t sub    = 0x01010101u;
    const uint32_t mask   = 0x80808080u;
    uint32_t maybe_zero;

    const uint32_t *hw = (const uint32_t *)haystack;
    const uint32_t *hw_end = (const uint32_t *)((size_t)haystack_end & ~0x3);
#endif

    for(; hw < hw_end; hw++)
    {
      /* If any of the bytes of maybe_zero are zero, subtracting 1 from each
       * byte will cause the zero bytes to become 0xff and subtract 1 from the
       * next byte up. If any high bits are set after masking out the high
       * bits that were already set, this word contains a zero byte and needs
       * to be checked further. Otherwise, it can be skipped.
       *
       * Upper case and lower case source characters can be checked at the same
       * time by ORing bit 5 onto every byte (for non-matching characters, this
       * translates them to other non-matching characters, so nothing breaks).
       */
      maybe_zero = ((*hw | tlower) ^ all_lc);
      if((maybe_zero - sub) & ~maybe_zero & mask)
        break;
    }
    haystack = (const uint8_t *)hw;
  }

  for(; haystack < haystack_end; haystack++)
    if(mzx_tolower(*haystack) == needle)
      return haystack;

  return NULL;
}
