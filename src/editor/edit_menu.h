/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_EDITOR_EDIT_MENU_H
#define MEGAZEUX_EDITOR_EDIT_MENU_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"
#include "buffer_struct.h"

subcontext *create_edit_menu(context *parent);

void update_edit_menu(subcontext *ctx, enum editor_mode mode,
 enum cursor_mode cursor_mode, int cursor_x, int cursor_y, int screen_height,
 struct buffer_info *buffer, boolean use_default_color);

void edit_menu_show_board_mod(subcontext *ctx);
void edit_menu_show_robot_memory(subcontext *ctx);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_EDIT_MENU_H */
