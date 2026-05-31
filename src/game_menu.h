/* MegaZeux
 *
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

#ifndef __GAME_MENU_H
#define __GAME_MENU_H

#include "compat.h"

__M_BEGIN_DECLS

#include "core.h"
#include "event/keysym.h"

boolean allow_exit_menu(struct world *mzx_world, boolean is_titlescreen);
boolean allow_enter_menu(struct world *mzx_world, boolean is_titlescreen);
boolean allow_help_system(struct world *mzx_world, boolean is_titlescreen);
boolean allow_settings_menu(struct world *mzx_world, boolean is_titlescreen,
 boolean is_override);
boolean allow_load_world_menu(struct world *mzx_world);
boolean allow_save_menu(struct world *mzx_world);
boolean allow_load_menu(struct world *mzx_world, boolean is_titlescreen);
boolean allow_debug_menu(struct world *mzx_world);

void game_menu(context *parent, boolean start_selected, enum keycode *retval,
 boolean *retval_alt);
void main_menu(context *parent, boolean start_selected, enum keycode *retval);

__M_END_DECLS

#endif // __GAME_MENU_H
