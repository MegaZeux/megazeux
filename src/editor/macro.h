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

#ifndef MEGAZEUX_EDITOR_MACRO_H
#define MEGAZEUX_EDITOR_MACRO_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

struct ext_macro;
struct macro_type;
struct editor_config_info;

void free_macro(struct ext_macro *macro_src);
void add_ext_macro(struct editor_config_info *conf, const char *name,
 const char *line_data, const char *label);
const struct ext_macro *find_macro(const struct editor_config_info *conf,
 const char *name, int *next);

#ifndef CONFIG_DEBYTECODE
union variable_storage *find_macro_variable(const char *name,
 const struct macro_type *m);
char *skip_whitespace(char *src);
char *skip_to_next(char *src, char t, char a, char b);
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_MACRO_H */
