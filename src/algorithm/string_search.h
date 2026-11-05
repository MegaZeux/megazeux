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

#ifndef MEGAZEUX_ALGORITHM_STRING_SEARCH_H
#define MEGAZEUX_ALGORITHM_STRING_SEARCH_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stddef.h>

#define MZX_STRING_SEARCH_SUFFIX_MAX 63

struct mzx_string_search_data
{
  unsigned int bad_char[256];
  unsigned int good_suffix[MZX_STRING_SEARCH_SUFFIX_MAX];
  boolean has_good_suffix_table;
};

UTILS_LIBSPEC void mzx_string_search_index(const void *B, const size_t b_len,
 struct mzx_string_search_data *data, boolean ignore_case);

UTILS_LIBSPEC const void *mzx_string_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const struct mzx_string_search_data *data,
 boolean ignore_case);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_STRING_SEARCH_H */
