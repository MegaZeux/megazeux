/* MegaZeux
 *
 * Copyright (C) 2018-2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_GAME_UPDATE_H
#define MEGAZEUX_GAME_UPDATE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"

void update_world(context *ctx, boolean is_title);
void update_board(context *ctx);

void update_scroll_color(void);

boolean draw_world(context *ctx, boolean is_title);

boolean update_resolve_target(struct world *mzx_world,
 boolean *fade_in_next_cycle);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_GAME_UPDATE_H */
