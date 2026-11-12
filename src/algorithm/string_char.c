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

#include "arch.h"
#include "bitmath.h"
#include "string_char.h"
#include "string_compare.h"

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
  'x',    'y',    'z',    '[',    '\\',   ']',    '^',    '_',
  '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
  'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
  'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
  'x',    'y',    'z',    '{',    '|',    '}',    '~',    '\x7f',
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

/* Because non-letter needles get forwarded to memchr, it's possible to
 * just OR 0x20 onto every haystack byte instead of using mzx_tolower to
 * get the same effect. */
#define TOLOWER_BYTE 0x20u
#define ALL_BYTES_01 ARCH_32_OR_64(0x01010101u, 0x0101010101010101ull)
#define ALL_BYTES_20 ARCH_32_OR_64(0x20202020u, 0x2020202020202020ull)
#define ALL_BYTES_80 ARCH_32_OR_64(0x80808080u, 0x8080808080808080ull)

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
#define POSITION_OF_FIRST_ZERO(x) (arch_uint_ctz(x) >> 3)
#else
#define POSITION_OF_FIRST_ZERO(x) (arch_uint_clz(x) >> 3)
#endif

/* If any of the bytes of maybe_zero are zero, subtracting 1 from each
 * byte will cause the zero bytes to become 0xff and subtract 1 from the
 * next byte up. If any high bits are set after masking out the high
 * bits that were already set, this word contains a zero byte and needs
 * to be checked further. Otherwise, it can be skipped.
 *
 * Checking further can be performed very quickly by counting bsf/ctz on
 * little endian architectures and bsr/clz on big endian architectures.
 * Non-matching bytes have 8 zero bits and matching bytes have 7 trailing
 * zero bits; either way, dividing the result by 8 gets the byte index.
 *
 * For case-insensitive search, upper case and lower case source characters can
 * be checked at the same time by ORing bit 5 onto every byte. This requires
 * any needle that isn't A-Z or a-z to be redirected to memchr.
 */
#define CHECK_WORD(value, offset) do { \
  arch_uint maybe_zero = ((value) ^ all_lc); \
  arch_uint has_zero = (maybe_zero - ALL_BYTES_01) & ~maybe_zero & ALL_BYTES_80; \
  if(has_zero) \
    return (const uint8_t *)(offset) + POSITION_OF_FIRST_ZERO(has_zero); \
} while(0)

/**
 * Find the position of the first occurrence of a character in a string of a
 * given length. The given string does not need to contain a nul terminator,
 * and if present, this function will treat nul the same as any other value.
 *
 * Aside from the type of `needle`, this function should behave more or less
 * equivalent to the C standard library `memchr`.
 *
 * @param haystack  NON nul-terminated string to search.
 * @param needle    character to find inside of `haystack`.
 * @param len       length of `haystack`, in bytes.
 * @return          a pointer to the first occurrence of `needle` in `haystack`
 *                  or NULL if `needle` is not found.
 */
const void *mzx_memchr(const void *_h, unsigned char needle, size_t len)
{
  const uint8_t *haystack;
  const uint8_t *haystack_end;
  const arch_uint *hw;
  const arch_uint *hw_end;
  arch_uint all_lc;
  arch_uint word;

  haystack = (const uint8_t *)_h;
  haystack_end = haystack + len;

  if(len < sizeof(arch_uint))
  {
    for(; haystack < haystack_end; haystack++)
      if(*haystack == needle)
        return haystack;
    return NULL;
  }

  all_lc = ALL_BYTES_01 * needle;
  memcpy(&word, haystack, sizeof(word));
  CHECK_WORD(word, haystack);

  hw = ARCH_UINT_TRUNC_CAST(haystack + sizeof(arch_uint));
  hw_end = ARCH_UINT_TRUNC_CAST(haystack_end);
  for(; hw < hw_end; hw++)
    CHECK_WORD(*hw, hw);

  haystack = haystack_end - sizeof(arch_uint);
  memcpy(&word, haystack, sizeof(word));
  CHECK_WORD(word, haystack);

  return NULL;
}

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
 * @param needle    character to find inside of `haystack` (ignoring case).
 *                  If this character isn't a letter, this function behaves
 *                  equivalent to `memchr`.
 * @param len       length of `haystack`, in bytes.
 * @return          a pointer to the first occurrence of `needle` in `haystack`
 *                  or NULL if `needle` is not found.
 */
const void *mzx_memcasechr(const void *_h, unsigned char needle, size_t len)
{
  const uint8_t *haystack;
  const uint8_t *haystack_end;
  const arch_uint *hw;
  const arch_uint *hw_end;
  arch_uint all_lc;
  arch_uint word;

  haystack = (const uint8_t *)_h;
  haystack_end = haystack + len;

  /* If the needle isn't A-Z or a-z, this search is equivalent to memchr. */
  needle = mzx_tolower(needle);
  if(!(needle >= 'a' && needle <= 'z'))
    return memchr(haystack, needle, len);

  if(len < sizeof(arch_uint))
  {
    for(; haystack < haystack_end; haystack++)
      if((*haystack | TOLOWER_BYTE) == needle)
        return haystack;
    return NULL;
  }

  all_lc = ALL_BYTES_01 * needle;
  memcpy(&word, haystack, sizeof(word));
  CHECK_WORD(word | ALL_BYTES_20, haystack);

  hw = ARCH_UINT_TRUNC_CAST(haystack + sizeof(arch_uint));
  hw_end = ARCH_UINT_TRUNC_CAST(haystack_end);
  for(; hw < hw_end; hw++)
    CHECK_WORD(*hw | ALL_BYTES_20, hw);

  haystack = haystack_end - sizeof(arch_uint);
  memcpy(&word, haystack, sizeof(word));
  CHECK_WORD(word | ALL_BYTES_20, haystack);

  return NULL;
}
