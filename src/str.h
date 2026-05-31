/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2017-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_STR_H
#define MEGAZEUX_STR_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdio.h>
#include <string.h>

#include "world_struct.h"
#include "counter_struct.h"

// These take up more room...
#define MIN_STRING_ALLOCATE 4

// Strings cannot be longer than 4M (orig 1M)
#define MAX_STRING_LEN (1 << 22)

static inline boolean is_string(const char *buffer)
{
  size_t namelen;

  // String identifiers always begin with '$'.
  if(buffer[0] != '$')
    return false;

  /* String identifiers can contain three different delimiters:
   * '+' indicates a string offset, which has a string value.
   * '#' indicates a string length limit, which has a string value.
   * '.' indicates a string subscript or length, which has a numeric value.
   *     A subscript may be followed by a '#' to indicate integer width.
   *
   * If a '.' is the first delimiter found, the identifier is not a string.
   */
  namelen = strcspn(buffer, "#+.");
  return buffer[namelen] != '.';
}

CORE_LIBSPEC int get_string(struct world *mzx_world, char *name_buffer,
 struct string *dest, int id);
CORE_LIBSPEC const struct string *get_string_pointer(struct world *mzx_world,
 const char *name, int id);
CORE_LIBSPEC int set_string(struct world *mzx_world, char *name_buffer,
 struct string *src, int id);
CORE_LIBSPEC struct string *new_string(struct world *mzx_world,
 const char *name, size_t length, int id);
CORE_LIBSPEC void sort_string_list(struct string_list *string_list);
CORE_LIBSPEC void string_list_size(struct string_list *string_list,
 size_t *list_size, size_t *table_size, size_t *strings_size);

int string_read_as_counter(struct world *mzx_world,
 char *name_buffer, int id);
void string_write_as_counter(struct world *mzx_world,
 char *name_buffer, int value, int id);

void load_string_board(struct world *mzx_world, char *name_buffer,
 char *src_chars, int src_width, int block_width, int block_height,
 char terminator);

void inc_string(struct world *mzx_world, char *name_buffer, struct string *src,
 int id);
void dec_string_int(struct world *mzx_world, const char *name, int value,
 int id);
int compare_strings(struct string *A, struct string *B, boolean exact_case,
 boolean allow_wildcards);
int compare_strings_null_terminated(struct string *A, struct string *B);

struct string *load_new_string(struct string_list *string_list, int index,
 const char *name, int name_length, int str_length);

void clear_string_list(struct string_list *string_list);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_STR_H */
