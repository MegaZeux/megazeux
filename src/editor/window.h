/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_EDITOR_WINDOW_H
#define MEGAZEUX_EDITOR_WINDOW_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../window.h"
#include "../world_struct.h"

#define CHAR_PAL_REG    254
#define CHAR_PAL_SMZX   184
#define CHAR_PAL_WILD    63

#define CHAR_SMZX_C0      0
#define CHAR_SMZX_C1    182
#define CHAR_SMZX_C2    183
#define CHAR_SMZX_C3    219

EDITOR_LIBSPEC int list_menu(const char *const *choices, int choice_size,
 const char *title, int current, int num_choices, int xpos, int ypos);
int color_selection(int current, int allow_wild);
void draw_color_box(int color, int q_bit, int x, int y, int x_limit);
struct element *construct_check_box(int x, int y, const char **choices,
 int num_choices, int max_length, int *results);
struct element *construct_char_box(int x, int y, const char *question,
 int allow_char_255, int *result);
struct element *construct_color_box(int x, int y,
 const char *question, int allow_wildcard, int *result);
struct element *construct_board_list(int x, int y,
 const char *title, int board_zero_as_none, int *result);
int add_board(struct world *mzx_world, int current);
int choose_board(struct world *mzx_world, int current, const char *title,
 int board0_none);
int choose_file(struct world *mzx_world, const char *const *wildcards,
 char *ret, const char *title, enum allow_dirs allow_dirs);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_WINDOW_H */
