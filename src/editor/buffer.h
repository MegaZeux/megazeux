/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2018-2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_EDITOR_BUFFER_H
#define MEGAZEUX_EDITOR_BUFFER_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"
#include "../world_struct.h"
#include "buffer_struct.h"
#include "edit.h"
#include "undo.h"

enum thing_menu_id
{
  THING_MENU_TERRAIN,
  THING_MENU_ITEMS,
  THING_MENU_CREATURES,
  THING_MENU_PUZZLE,
  THING_MENU_TRANSPORT,
  THING_MENU_ELEMENTS,
  THING_MENU_MISC,
  THING_MENU_OBJECTS,
  NUM_THING_MENUS
};

void change_param(context *ctx, struct buffer_info *buffer, int *new_param);
void free_edit_buffer(struct buffer_info *buffer);

int place_current_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode, struct undo_history *history);
int replace_current_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode, struct undo_history *history);

void grab_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode);

void thing_menu(context *parent, enum thing_menu_id menu_number,
 struct buffer_info *buffer, boolean use_default_color, int x, int y,
 struct undo_history *history);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_BUFFER_H */
