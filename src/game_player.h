/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_GAME_PLAYER_H
#define MEGAZEUX_GAME_PLAYER_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

CORE_LIBSPEC void find_player(struct world *mzx_world);

void set_mesg(struct world *mzx_world, const char *str);
void set_mesg_direct(struct board *src_board, const char *str);

boolean player_can_save(struct world *mzx_world);
void player_switch_bomb_type(struct world *mzx_world);
void player_cheat_give_all(struct world *mzx_world);
void player_cheat_zap(struct world *mzx_world);

void hurt_player(struct world *mzx_world, enum thing damage_src);
int take_key(struct world *mzx_world, int color);
int give_key(struct world *mzx_world, int color);
void grab_item(struct world *mzx_world, int item_x, int item_y, int src_dir);
void move_player(struct world *mzx_world, int dir);
void entrance(struct world *mzx_world, int x, int y);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_GAME_PLAYER_H */
