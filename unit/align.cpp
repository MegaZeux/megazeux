/* MegaZeux
 *
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Test the alignment of several name fields in structs.
 * If enough inputs into memcmp and memcasecmp are consistently 4 aligned,
 * this allows faster comparisons to be performed and also allows some code
 * to assume alignment will be the general case.
 */

#include "Unit.hpp"
#include "../src/counter_struct.h"
#include "../src/io/zip.h"
#include "../src/platform/platform_endian.h"

UNITTEST(counter_struct_name)
{
  static_assert((offsetof(struct counter, name) & (ALIGN_32_MODULO - 1)) == 0,
   "struct counter::name is not aligned to 4 bytes!");
}

UNITTEST(string_struct_name)
{
  static_assert((offsetof(struct string, name) & (ALIGN_32_MODULO - 1)) == 0,
   "struct string::name is not aligned to 4 bytes!");
}

UNITTEST(zip_file_header_file_name)
{
  static_assert((offsetof(struct zip_file_header, file_name) & (ALIGN_32_MODULO - 1)) == 0,
   "struct zip_file_header::file_name is not aligned to 4 bytes!");
}
