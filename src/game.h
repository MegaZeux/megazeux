/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
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

/* Declarations for GAME.CPP */

#ifndef MEGAZEUX_GAME_H
#define MEGAZEUX_GAME_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"
#include "world_struct.h"

CORE_LIBSPEC void title_screen(context *parent);
CORE_LIBSPEC void load_board_module(struct world *mzx_world);
CORE_LIBSPEC boolean load_game_module(struct world *mzx_world, char *filename,
 boolean fail_if_same);

void clear_intro_mesg(void);
void draw_intro_mesg(struct world *mzx_world);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void play_game(context *parent, boolean *_fade_in);
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_GAME_H */
