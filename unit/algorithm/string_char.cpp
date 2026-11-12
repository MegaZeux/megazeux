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

#include <limits.h>

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
  int expected_ignore_case;
};

#define NO INT_MIN

static constexpr string_pos_test_data match_data[] =
{
  { "a",          1, 'a',     0,  0 },
  { "bbbbba",     6, 'a',     5,  5 },
  { "bbbbba",     6, 'b',     0,  0 },
  { "string",     6, 'g',     5,  5 },
  { "string",     6, 'n',     4,  4 },
  { "string",     6, 'i',     3,  3 },
  { "string",     6, 'r',     2,  2 },
  { "string",     6, 't',     1,  1 },
  { "string",     6, 's',     0,  0 },
  { "STRING",     6, 'g',     NO, 5 },
  { "STRING",     6, 'n',     NO, 4 },
  { "STRING",     6, 'i',     NO, 3 },
  { "STRING",     6, 'r',     NO, 2 },
  { "STRING",     6, 't',     NO, 1 },
  { "STRING",     6, 's',     NO, 0 },
  { "string",     6, 'G',     NO, 5 },
  { "string",     6, 'N',     NO, 4 },
  { "string",     6, 'I',     NO, 3 },
  { "string",     6, 'R',     NO, 2 },
  { "string",     6, 'T',     NO, 1 },
  { "string",     6, 'S',     NO, 0 },
  { "\0awaw",     5, 'w',     2,  2 },
  { "\0awaw",     5, 'W',     NO, 2 },
  { "awawa",      6, '\0',    5,  5 },
  { "str\x14ng",  6, '\x14',  3,  3 },
  { "\xfe\x01",   3, '\x01',  1,  1 },
  { "fread*",     6, '*',     5,  5 },
};

static constexpr string_pos_test_data nomatch_data[] =
{
  { "",           0, 'a',     NO, NO },
  { "b",          1, 'a',     NO, NO },
  { "bbbbba",     6, 'c',     NO, NO },
  { "string",     3, 'n',     NO, NO },
  { "string",     6, '\0',    NO, NO },
  { "str3ng",     6, 'i',     NO, NO },
  { "str3ng",     6, '\x14',  NO, NO },
  { "aaaaa",      5, '\x81',  NO, NO },
  { "fwrite*",    6, '*',     NO, NO },
};

static constexpr string_pos_test_data align4_data[] =
{
  { "*%%%________", 12, '*',  0,  0 },
  { "%*%%________", 12, '*',  1,  1 },
  { "%%*%________", 12, '*',  2,  2 },
  { "%%%*________", 12, '*',  3,  3 },
  { "____*%%%____", 12, '*',  4,  4 },
  { "____%*%%____", 12, '*',  5,  5 },
  { "____%%*%____", 12, '*',  6,  6 },
  { "____%%%*____", 12, '*',  7,  7 },
  { "________*%%%", 12, '*',  8,  8 },
  { "________%*%%", 12, '*',  9,  9 },
  { "________%%*%", 12, '*',  10, 10 },
  { "________%%%*", 12, '*',  11, 11 },
  { "q%%%________", 12, 'Q',  NO, 0 },
  { "%W%%________", 12, 'w',  NO, 1 },
  { "%%e%________", 12, 'E',  NO, 2 },
  { "%%%R________", 12, 'r',  NO, 3 },
  { "____t%%%____", 12, 'T',  NO, 4 },
  { "____%Y%%____", 12, 'y',  NO, 5 },
  { "____%%u%____", 12, 'U',  NO, 6 },
  { "____%%%I____", 12, 'i',  NO, 7 },
  { "________o%%%", 12, 'O',  NO, 8 },
  { "________%P%%", 12, 'p',  NO, 9 },
  { "________%%a%", 12, 'A',  NO, 10 },
  { "________%%%S", 12, 's',  NO, 11 },
};

static constexpr string_pos_test_data align8_data[] =
{
  { "*%%%%%%%________________", 24, '*',  0,  0 },
  { "%*%%%%%%________________", 24, '*',  1,  1 },
  { "%%*%%%%%________________", 24, '*',  2,  2 },
  { "%%%*%%%%________________", 24, '*',  3,  3 },
  { "%%%%*%%%________________", 24, '*',  4,  4 },
  { "%%%%%*%%________________", 24, '*',  5,  5 },
  { "%%%%%%*%________________", 24, '*',  6,  6 },
  { "%%%%%%%*________________", 24, '*',  7,  7 },
  { "________*%%%%%%%________", 24, '*',  8,  8 },
  { "________%*%%%%%%________", 24, '*',  9,  9 },
  { "________%%*%%%%%________", 24, '*',  10, 10 },
  { "________%%%*%%%%________", 24, '*',  11, 11 },
  { "________%%%%*%%%________", 24, '*',  12, 12 },
  { "________%%%%%*%%________", 24, '*',  13, 13 },
  { "________%%%%%%*%________", 24, '*',  14, 14 },
  { "________%%%%%%%*________", 24, '*',  15, 15 },
  { "________________*%%%%%%%", 24, '*',  16, 16 },
  { "________________%*%%%%%%", 24, '*',  17, 17 },
  { "________________%%*%%%%%", 24, '*',  18, 18 },
  { "________________%%%*%%%%", 24, '*',  19, 19 },
  { "________________%%%%*%%%", 24, '*',  20, 20 },
  { "________________%%%%%*%%", 24, '*',  21, 21 },
  { "________________%%%%%%*%", 24, '*',  22, 22 },
  { "________________%%%%%%%*", 24, '*',  23, 23 },
  { "d%%%%%%%________________", 24, 'D',  NO,  0 },
  { "%F%%%%%%________________", 24, 'f',  NO,  1 },
  { "%%g%%%%%________________", 24, 'G',  NO,  2 },
  { "%%%H%%%%________________", 24, 'h',  NO,  3 },
  { "%%%%j%%%________________", 24, 'J',  NO,  4 },
  { "%%%%%K%%________________", 24, 'k',  NO,  5 },
  { "%%%%%%l%________________", 24, 'L',  NO,  6 },
  { "%%%%%%%Z________________", 24, 'z',  NO,  7 },
  { "________x%%%%%%%________", 24, 'X',  NO,  8 },
  { "________%C%%%%%%________", 24, 'c',  NO,  9 },
  { "________%%v%%%%%________", 24, 'V',  NO, 10 },
  { "________%%%B%%%%________", 24, 'b',  NO, 11 },
  { "________%%%%n%%%________", 24, 'N',  NO, 12 },
  { "________%%%%%M%%________", 24, 'm',  NO, 13 },
  { "________%%%%%%Q%________", 24, 'q',  NO, 14 },
  { "________%%%%%%%w________", 24, 'W',  NO, 15 },
  { "________________E%%%%%%%", 24, 'e',  NO, 16 },
  { "________________%r%%%%%%", 24, 'R',  NO, 17 },
  { "________________%%T%%%%%", 24, 't',  NO, 18 },
  { "________________%%%y%%%%", 24, 'Y',  NO, 19 },
  { "________________%%%%U%%%", 24, 'u',  NO, 20 },
  { "________________%%%%%i%%", 24, 'I',  NO, 21 },
  { "________________%%%%%%O%", 24, 'o',  NO, 22 },
  { "________________%%%%%%%p", 24, 'P',  NO, 23 },
};

/* Try to confuse the bit math used in the fast loop. */
static constexpr string_pos_test_data inversions_data[] =
{
  /* Simple bit inversions of needle character, not present in any word */
  { "\x80\x40\x20\x10\x08\x04\x02\x01\x80\x40\x20\x10\x08\x04\x02\x01", 16, '\0',   NO, NO },
  { "\x10\x20\x40\x80\x01\x02\x04\x08\x10\x20\x40\x80\x01\x02\x04\x08", 16, '\0',   NO, NO },
  { "\xa0\x60\x00\x30\x28\x24\x22\x21\xa0\x60\x00\x30\x28\x24\x22\x21", 16, ' ',    NO, NO },
  { "\x30\x00\x60\xa0\x21\x22\x24\x28\x30\x00\x60\xa0\x21\x22\x24\x28", 16, ' ',    NO, NO },
  { "\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e", 16, '\x7f', NO, NO },
  { "\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77", 16, '\x7f', NO, NO },
  { "\xc0\x00\x60\x50\x48\x44\x42\x41\xc0\x00\x60\x50\x48\x44\x42\x41", 16, '@',    NO, NO },
  { "\x50\x60\x00\xc0\x41\x42\x44\x48\x50\x60\x00\xc0\x41\x42\x44\x48", 16, '@',    NO, NO },
  { "\xe0\x20\x40\x70\x68\x64\x62\x61\xe0\x20\x40\x70\x68\x64\x62\x61", 16, '`',    NO, NO },
  { "\x70\x40\x20\xe0\x61\x62\x64\x68\x70\x40\x20\xe0\x61\x62\x64\x68", 16, '`',    NO, NO },
  { "\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a", 16, '[',    NO, NO },
  { "\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53", 16, '[',    NO, NO },
  { "\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a", 16, '{',    NO, NO },
  { "\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73", 16, '{',    NO, NO },
  { "\xc1\x01\xff\x51\x49\x45\x43\x40\xc1\x01\xff\x51\x49\x45\x43\x40", 16, 'A',    NO, NO },
  { "\x51\xff\x01\xc1\x40\x43\x45\x49\x51\xff\x01\xc1\x40\x43\x45\x49", 16, 'A',    NO, NO },
  { "\xe1\x21\xff\x71\x69\x65\x63\x60\xe1\x21\xff\x71\x69\x65\x63\x60", 16, 'a',    NO, NO },
  { "\x71\xff\x21\xe1\x60\x63\x65\x69\x71\xff\x21\xe1\x60\x63\x65\x69", 16, 'a',    NO, NO },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5b\xda\x1a\xff\x4a\x52\x5e\x58\x5b", 16, 'Z',    NO, NO },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x4a\xff\x1a\xda\x5b\x58\x5e\x52", 16, 'Z',    NO, NO },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x72\xfa\x3a\xff\x6a\x72\x7e\x78\x72", 16, 'z',    NO, NO },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b", 16, 'z',    NO, NO },
  /* False positives, real match outside of word */
  { "\x80\x40\x20\x10\x08\x04\x02\x01\x80\x40\x20\x10\x08\x04\x02\x01\x00", 17, '\0',   16, 16 },
  { "\x10\x20\x40\x80\x01\x02\x04\x08\x10\x20\x40\x80\x01\x02\x04\x08\x00", 17, '\0',   16, 16 },
  { "\xa0\x60\x00\x30\x28\x24\x22\x21\xa0\x60\x00\x30\x28\x24\x22\x21\x20", 17, ' ',    16, 16 },
  { "\x30\x00\x60\xa0\x21\x22\x24\x28\x30\x00\x60\xa0\x21\x22\x24\x28\x20", 17, ' ',    16, 16 },
  { "\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e\x7f", 17, '\x7f', 16, 16 },
  { "\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77\x7f", 17, '\x7f', 16, 16 },
  { "\xc0\x00\x60\x50\x48\x44\x42\x41\xc0\x00\x60\x50\x48\x44\x42\x41\x40", 17, '@',    16, 16 },
  { "\x50\x60\x00\xc0\x41\x42\x44\x48\x50\x60\x00\xc0\x41\x42\x44\x48\x40", 17, '@',    16, 16 },
  { "\xe0\x20\x40\x70\x68\x64\x62\x61\xe0\x20\x40\x70\x68\x64\x62\x61\x60", 17, '`',    16, 16 },
  { "\x70\x40\x20\xe0\x61\x62\x64\x68\x70\x40\x20\xe0\x61\x62\x64\x68\x60", 17, '`',    16, 16 },
  { "\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a\x5b", 17, '[',    16, 16 },
  { "\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53\x5b", 17, '[',    16, 16 },
  { "\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a\x7b", 17, '{',    16, 16 },
  { "\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73\x7b", 17, '{',    16, 16 },
  { "\xc1\x01\xff\x51\x49\x45\x43\x40\xc1\x01\xff\x51\x49\x45\x43\x40\x41", 17, 'A',    16, 16 },
  { "\x51\xff\x01\xc1\x40\x43\x45\x49\x51\xff\x01\xc1\x40\x43\x45\x49\x41", 17, 'A',    16, 16 },
  { "\xc1\x01\xff\x51\x49\x45\x43\x40\xc1\x01\xff\x51\x49\x45\x43\x40\x61", 17, 'A',    NO, 16 },
  { "\x51\xff\x01\xc1\x40\x43\x45\x49\x51\xff\x01\xc1\x40\x43\x45\x49\x61", 17, 'A',    NO, 16 },
  { "\xe1\x21\xff\x71\x69\x65\x63\x60\xe1\x21\xff\x71\x69\x65\x63\x60\x61", 17, 'a',    16, 16 },
  { "\x71\xff\x21\xe1\x60\x63\x65\x69\x71\xff\x21\xe1\x60\x63\x65\x69\x61", 17, 'a',    16, 16 },
  { "\xe1\x21\xff\x71\x69\x65\x63\x60\xe1\x21\xff\x71\x69\x65\x63\x60\x41", 17, 'a',    NO, 16 },
  { "\x71\xff\x21\xe1\x60\x63\x65\x69\x71\xff\x21\xe1\x60\x63\x65\x69\x41", 17, 'a',    NO, 16 },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5b\xda\x1a\xff\x4a\x52\x5e\x58\x5b\x5a", 17, 'Z',    16, 16 },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x5a", 17, 'Z',    16, 16 },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5b\xda\x1a\xff\x4a\x52\x5e\x58\x5b\x7a", 17, 'Z',    NO, 16 },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x7a", 17, 'Z',    NO, 16 },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x72\xfa\x3a\xff\x6a\x72\x7e\x78\x72\x7a", 17, 'z',    16, 16 },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x7a", 17, 'z',    16, 16 },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x72\xfa\x3a\xff\x6a\x72\x7e\x78\x72\x5a", 17, 'z',    NO, 16 },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x5a", 17, 'z',    NO, 16 },
  /* False positives, real match inside of word */
  { "\x80\x40\x20\x10\x08\x04\x02\x00\x80\x40\x20\x10\x08\x04\x02\x01", 16, '\0',   7,  7  },
  { "\x10\x20\x40\x80\x01\x02\x04\x00\x10\x20\x40\x80\x01\x02\x04\x08", 16, '\0',   7,  7  },
  { "\x80\x40\x20\x10\x08\x04\x02\x01\x80\x40\x20\x00\x08\x04\x02\x01", 16, '\0',   11, 11 },
  { "\x10\x20\x40\x80\x01\x02\x04\x08\x10\x20\x40\x00\x01\x02\x04\x08", 16, '\0',   11, 11 },
  { "\xa0\x60\x00\x30\x28\x24\x22\x20\xa0\x60\x00\x30\x28\x24\x22\x21", 16, ' ',    7,  7  },
  { "\x30\x00\x60\xa0\x21\x22\x24\x20\x30\x00\x60\xa0\x21\x22\x24\x28", 16, ' ',    7,  7  },
  { "\xa0\x60\x00\x30\x28\x24\x22\x21\xa0\x60\x00\x20\x28\x24\x22\x21", 16, ' ',    11, 11 },
  { "\x30\x00\x60\xa0\x21\x22\x24\x28\x30\x00\x60\x20\x21\x22\x24\x28", 16, ' ',    11, 11 },
  { "\xff\x3f\x5f\x6f\x77\x7b\x7d\x7f\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e", 16, '\x7f', 7,  7  },
  { "\x6f\x5f\x3f\xff\x7e\x7d\x7b\x7f\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77", 16, '\x7f', 7,  7  },
  { "\xff\x3f\x5f\x6f\x77\x7b\x7d\x7e\xff\x3f\x5f\x7f\x77\x7b\x7d\x7e", 16, '\x7f', 11, 11 },
  { "\x6f\x5f\x3f\xff\x7e\x7d\x7b\x77\x6f\x5f\x3f\x7f\x7e\x7d\x7b\x77", 16, '\x7f', 11, 11 },
  { "\xc0\x00\x60\x50\x48\x44\x42\x40\xc0\x00\x60\x50\x48\x44\x42\x41", 16, '@',    7,  7  },
  { "\x50\x60\x00\xc0\x41\x42\x44\x40\x50\x60\x00\xc0\x41\x42\x44\x48", 16, '@',    7,  7  },
  { "\xc0\x00\x60\x50\x48\x44\x42\x41\xc0\x00\x60\x40\x48\x44\x42\x41", 16, '@',    11, 11 },
  { "\x50\x60\x00\xc0\x41\x42\x44\x48\x50\x60\x00\x40\x41\x42\x44\x48", 16, '@',    11, 11 },
  { "\xe0\x20\x40\x70\x68\x64\x62\x60\xe0\x20\x40\x70\x68\x64\x62\x61", 16, '`',    7,  7  },
  { "\x70\x40\x20\xe0\x61\x62\x64\x60\x70\x40\x20\xe0\x61\x62\x64\x68", 16, '`',    7,  7  },
  { "\xe0\x20\x40\x70\x68\x64\x62\x61\xe0\x20\x40\x60\x68\x64\x62\x61", 16, '`',    11, 11 },
  { "\x70\x40\x20\xe0\x61\x62\x64\x68\x70\x40\x20\x60\x61\x62\x64\x68", 16, '`',    11, 11 },
  { "\xdb\x1b\x7b\x4b\x53\x5f\x59\x5b\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a", 16, '[',    7,  7  },
  { "\x4b\x7b\x1b\xdb\x5a\x59\x5f\x5b\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53", 16, '[',    7,  7  },
  { "\xdb\x1b\x7b\x4b\x53\x5f\x59\x5a\xdb\x1b\x7b\x5b\x53\x5f\x59\x5a", 16, '[',    11, 11 },
  { "\x4b\x7b\x1b\xdb\x5a\x59\x5f\x53\x4b\x7b\x1b\x5b\x5a\x59\x5f\x53", 16, '[',    11, 11 },
  { "\xfb\x3b\x5b\x6b\x73\x7f\x79\x7b\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a", 16, '{',    7,  7  },
  { "\x6b\x5b\x3b\xfb\x7a\x79\x7f\x7b\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73", 16, '{',    7,  7  },
  { "\xfb\x3b\x5b\x6b\x73\x7f\x79\x7a\xfb\x3b\x5b\x7b\x73\x7f\x79\x7a", 16, '{',    11, 11 },
  { "\x6b\x5b\x3b\xfb\x7a\x79\x7f\x73\x6b\x5b\x3b\x7b\x7a\x79\x7f\x73", 16, '{',    11, 11 },
  { "\xc1\x01\xff\x51\x49\x45\x43\x41\xc1\x01\xff\x51\x49\x45\x43\x40", 16, 'A',    7,  7  },
  { "\x51\xff\x01\xc1\x40\x43\x45\x41\x51\xff\x01\xc1\x40\x43\x45\x49", 16, 'A',    7,  7  },
  { "\xc1\x01\xff\x51\x49\x45\x43\x40\xc1\x01\xff\x41\x49\x45\x43\x40", 16, 'A',    11, 11 },
  { "\x51\xff\x01\xc1\x40\x43\x45\x49\x51\xff\x01\x41\x40\x43\x45\x49", 16, 'A',    11, 11 },
  { "\xc1\x01\xff\x51\x49\x45\x43\x61\xc1\x01\xff\x51\x49\x45\x43\x40", 16, 'A',    NO, 7  },
  { "\x51\xff\x01\xc1\x40\x43\x45\x61\x51\xff\x01\xc1\x40\x43\x45\x49", 16, 'A',    NO, 7  },
  { "\xc1\x01\xff\x51\x49\x45\x43\x40\xc1\x01\xff\x61\x49\x45\x43\x40", 16, 'A',    NO, 11 },
  { "\x51\xff\x01\xc1\x40\x43\x45\x49\x51\xff\x01\x61\x40\x43\x45\x49", 16, 'A',    NO, 11 },
  { "\xe1\x21\xff\x71\x69\x65\x63\x61\xe1\x21\xff\x71\x69\x65\x63\x60", 16, 'a',    7,  7  },
  { "\x71\xff\x21\xe1\x60\x63\x65\x61\x71\xff\x21\xe1\x60\x63\x65\x69", 16, 'a',    7,  7  },
  { "\xe1\x21\xff\x71\x69\x65\x63\x60\xe1\x21\xff\x61\x69\x65\x63\x60", 16, 'a',    11, 11 },
  { "\x71\xff\x21\xe1\x60\x63\x65\x69\x71\xff\x21\x61\x60\x63\x65\x69", 16, 'a',    11, 11 },
  { "\xe1\x21\xff\x71\x69\x65\x63\x41\xe1\x21\xff\x71\x69\x65\x63\x60", 16, 'a',    NO, 7  },
  { "\x71\xff\x21\xe1\x60\x63\x65\x41\x71\xff\x21\xe1\x60\x63\x65\x69", 16, 'a',    NO, 7  },
  { "\xe1\x21\xff\x71\x69\x65\x63\x60\xe1\x21\xff\x41\x69\x65\x63\x60", 16, 'a',    NO, 11 },
  { "\x71\xff\x21\xe1\x60\x63\x65\x69\x71\xff\x21\x41\x60\x63\x65\x69", 16, 'a',    NO, 11 },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5a\xda\x1a\xff\x4a\x52\x5e\x58\x5b", 16, 'Z',    7,  7  },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x5a\x4a\xff\x1a\xda\x5b\x58\x5e\x52", 16, 'Z',    7,  7  },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5b\xda\x1a\xff\x5a\x52\x5e\x58\x5b", 16, 'Z',    11, 11 },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x4a\xff\x1a\x5a\x5b\x58\x5e\x52", 16, 'Z',    11, 11 },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x7a\xda\x1a\xff\x4a\x52\x5e\x58\x5b", 16, 'Z',    NO, 7  },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x7a\x4a\xff\x1a\xda\x5b\x58\x5e\x52", 16, 'Z',    NO, 7  },
  { "\xda\x1a\xff\x4a\x52\x5e\x58\x5b\xda\x1a\xff\x7a\x52\x5e\x58\x5b", 16, 'Z',    NO, 11 },
  { "\x4a\xff\x1a\xda\x5b\x58\x5e\x52\x4a\xff\x1a\x7a\x5b\x58\x5e\x52", 16, 'Z',    NO, 11 },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x7a\xfa\x3a\xff\x6a\x72\x7e\x78\x72", 16, 'z',    7,  7  },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7a\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b", 16, 'z',    7,  7  },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x72\xfa\x3a\xff\x7a\x72\x7e\x78\x72", 16, 'z',    11, 11 },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x6a\xff\x3a\x7a\x7b\x78\x7e\x7b", 16, 'z',    11, 11 },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x5a\xfa\x3a\xff\x6a\x72\x7e\x78\x72", 16, 'z',    NO, 7  },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x5a\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b", 16, 'z',    NO, 7  },
  { "\xfa\x3a\xff\x6a\x72\x7e\x78\x72\xfa\x3a\xff\x5a\x72\x7e\x78\x72", 16, 'z',    NO, 11 },
  { "\x6a\xff\x3a\xfa\x7b\x78\x7e\x7b\x6a\xff\x3a\x5a\x7b\x78\x7e\x7b", 16, 'z',    NO, 11 },
};

union align_buf
{
  char c_str[128];
  uint64_t ignore;
};

static inline const char *make_aligned8(union align_buf &b, const char *str, size_t len)
{
  memcpy(b.c_str, str, len);
  b.c_str[len] = '\0';
  return b.c_str;
}

static inline const char *make_aligned4(union align_buf &b, const char *str, size_t len)
{
  memcpy(b.c_str + 4, str, len);
  b.c_str[len + 4] = '\0';
  return b.c_str + 4;
}

static inline const char *make_unaligned(union align_buf &b, const char *str, size_t len)
{
  memcpy(b.c_str + 1, str, len);
  b.c_str[len + 1] = '\0';
  return b.c_str + 1;
}

static void char_str(char *str, unsigned char needle)
{
  if(needle == '\0')
  {
    *(str++) = '\\';
    *(str++) = '0';
  }
  else
    *(str++) = needle;

  *str = '\0';
}

static void check_memchr_inner(const string_pos_test_data &d, const char *ext)
{
  if(!d.haystack)
    UNIMPLEMENTED();

  const void *ret_a = memchr(d.haystack, d.needle, d.len);
  const void *ret_b = mzx_memchr(d.haystack, d.needle, d.len);

  int result_a = ret_a ? reinterpret_cast<const char *>(ret_a) - d.haystack : INT_MIN;
  int result_b = ret_b ? reinterpret_cast<const char *>(ret_b) - d.haystack : INT_MIN;

  char needle_str[4];
  char_str(needle_str, d.needle);
  ASSERTEQ(result_a, d.expected,
   "memchr(\"%s\",'%s',%zu)%s", d.haystack, needle_str, d.len, ext);
  ASSERTEQ(result_b, d.expected,
   "mzx_memchr(\"%s\",'%s',%zu)%s", d.haystack, needle_str, d.len, ext);
}

static void check_memcasechr_inner(const string_pos_test_data &d, const char *ext)
{
  if(!d.haystack)
    UNIMPLEMENTED();

  const void *ret_a = memcasechr(d.haystack, d.needle, d.len);
  const void *ret_b = mzx_memcasechr(d.haystack, d.needle, d.len);

  int result_a = ret_a ? reinterpret_cast<const char *>(ret_a) - d.haystack : INT_MIN;
  int result_b = ret_b ? reinterpret_cast<const char *>(ret_b) - d.haystack : INT_MIN;

  char needle_str[4];
  char_str(needle_str, d.needle);
  ASSERTEQ(result_a, d.expected_ignore_case,
   "memcasechr(\"%s\",'%s',%zu)%s", d.haystack, needle_str, d.len, ext);
  ASSERTEQ(result_b, d.expected_ignore_case,
   "mzx_memcasechr(\"%s\",'%s',%zu)%s", d.haystack, needle_str, d.len, ext);
}

static void check_memchr(const string_pos_test_data &d)
{
  check_memchr_inner(d, "");

  string_pos_test_data d2 =
   { d.haystack, d.len, d.needle, d.expected, d.expected_ignore_case };

  align_buf b;
  d2.haystack = make_aligned8(b, d.haystack, d.len);
  check_memchr_inner(d2, " align8");
  d2.haystack = make_aligned4(b, d.haystack, d.len);
  check_memchr_inner(d2, " align4");
  d2.haystack = make_unaligned(b, d.haystack, d.len);
  check_memchr_inner(d2, " unalign");
}

static void check_memcasechr(const string_pos_test_data &d)
{
  check_memcasechr_inner(d, "");

  string_pos_test_data d2 =
   { d.haystack, d.len, d.needle, d.expected, d.expected_ignore_case };

  align_buf b;
  d2.haystack = make_aligned8(b, d.haystack, d.len);
  check_memcasechr_inner(d2, " align8");
  d2.haystack = make_aligned4(b, d.haystack, d.len);
  check_memcasechr_inner(d2, " align4");
  d2.haystack = make_unaligned(b, d.haystack, d.len);
  check_memcasechr_inner(d2, " unalign");
}

UNITTEST(mzx_memchr)
{
  SECTION(Match)
    test_data_set(check_memchr, match_data);

  SECTION(NoMatch)
    test_data_set(check_memchr, nomatch_data);

  SECTION(Align4)
    test_data_set(check_memchr, align4_data);

  SECTION(Align8)
    test_data_set(check_memchr, align8_data);

  SECTION(Inversions)
    test_data_set(check_memchr, inversions_data);
}

UNITTEST(mzx_memcasechr)
{
  SECTION(Match)
    test_data_set(check_memcasechr, match_data);

  SECTION(NoMatch)
    test_data_set(check_memcasechr, nomatch_data);

  SECTION(Align4)
    test_data_set(check_memcasechr, align4_data);

  SECTION(Align8)
    test_data_set(check_memcasechr, align8_data);

  SECTION(Inversions)
    test_data_set(check_memcasechr, inversions_data);
}
