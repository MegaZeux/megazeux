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

#ifndef MEGAZEUX_LEGACY_RASM_H
#define MEGAZEUX_LEGACY_RASM_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdio.h>

#define MAX_OBJ_SIZE       2097152

#define S_IMM_U16          0
#define S_IMM_S16          0
#define S_CHARACTER        2
#define S_COLOR            3
#define S_DIR              4
#define S_THING            5
#define S_PARAM            6
#define S_STRING           7
#define S_EQUALITY         8
#define S_CONDITION        9
#define S_ITEM             10
#define S_EXTRA            11
#define S_CMD              12
#define S_UNDEFINED        13

struct search_entry
{
  const char *const name;
  int count;
  const int offsets[19];
};

struct search_entry_short
{
  const char *const name;
  int offset;
  int type;
};

// Maps bytecode positions to source code positions.
struct command_mapping
{
  int real_line;
  int bc_pos;
  int src_pos;
};

CORE_LIBSPEC int get_color(char *cmd_line);

extern const char special_first_char[256];

// This command will reallocate the program as-needed, but it won't free it.
boolean validate_legacy_bytecode(char **_bc, int *_program_length,
 int *_cur_prog_line);
boolean legacy_convert_v1_program(char **_dest, int *_dest_len,
 int *_cur_prog_line, const char *src, int src_len);

#ifdef CONFIG_DEBYTECODE

CORE_LIBSPEC int legacy_assemble_line(char *cpos, char *output_buffer, char *error_buffer,
 char *param_listing, int *arg_count_ext);

#else // !CONFIG_DEBYTECODE

char *assemble_file(char *name, int *size);
char *assemble_program(char *src, int len, int *size);

void disassemble_file(char *name, char *program, int program_length,
 int allow_ignores, int base);
CORE_LIBSPEC void disassemble_program(char *program, int program_length,
 char **_source, int *_source_length, struct command_mapping **_command_map,
 int *_command_map_length);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int legacy_assemble_line(char *cpos, char *output_buffer,
 char *error_buffer, char *param_listing, int *arg_count_ext);
CORE_LIBSPEC int disassemble_line(char *cpos, char **next, char *output_buffer,
 char *error_buffer, int *total_bytes, int print_ignores, char *arg_types,
 int *arg_count, int base);
CORE_LIBSPEC const struct search_entry_short *find_argument(char *name);
CORE_LIBSPEC void print_color(int color, char *color_buffer);
CORE_LIBSPEC int unescape_char(char *dest, char c);
#endif // CONFIG_EDITOR

#endif // !CONFIG_DEBYTECODE

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_LEGACY_RASM_H */
