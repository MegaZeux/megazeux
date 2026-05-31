/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_EDITOR_MACRO_STRUCT_H
#define MEGAZEUX_EDITOR_MACRO_STRUCT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

enum variable_type
{
  number,
  string,
  character,
  color
};

enum reference_mode
{
  decimal,
  hexidecimal
};

union variable_storage
{
  int int_storage;
  char *str_storage;
};

struct macro_variable
{
  union variable_storage storage;
  union variable_storage def;
  char *name;
};

struct macro_type
{
  int type_attributes[16];
  enum variable_type type;
  int num_variables;
  struct macro_variable *variables;
  struct macro_variable **variables_sorted;
};

struct macro_variable_reference
{
  struct macro_type *type;
  union variable_storage *storage;
  enum reference_mode ref_mode;
};

struct ext_macro
{
  char *name;
  char *label;
  int num_lines;
  char ***lines;
  struct macro_variable_reference **variable_references;
  int *line_element_count;
  int num_types;
  int total_variables;
  struct macro_type types[32];
  char *text;
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_MACRO_STRUCT_H */
