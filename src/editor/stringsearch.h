/* MegaZeux
 *
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_EDITOR_STRINGSEARCH_H
#define MEGAZEUX_EDITOR_STRINGSEARCH_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stddef.h>

#define STRINGSEARCH_SUFFIX_MAX 63

struct string_search_data
{
  unsigned int bad_char[256];
  unsigned int good_suffix[STRINGSEARCH_SUFFIX_MAX];
  boolean has_good_suffix_table;
};

void string_search_index(const void *B, const size_t b_len,
 struct string_search_data *data, boolean ignore_case);
const void *string_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const struct string_search_data *data,
 boolean ignore_case);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_STRINGSEARCH_H */
