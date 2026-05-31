/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_EDITOR_EDIT_H
#define MEGAZEUX_EDITOR_EDIT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"

#define EDIT_SCREEN_MINIMAL     24
#define EDIT_SCREEN_NORMAL      19

enum editor_mode
{
  EDIT_BOARD = 0,
  EDIT_OVERLAY = 1,
  EDIT_VLAYER = 2,
};

enum cursor_mode
{
  CURSOR_PLACE,
  CURSOR_DRAW,
  CURSOR_TEXT,
  CURSOR_BLOCK_SELECT,
  CURSOR_BLOCK_PLACE,
  CURSOR_MZM_PLACE,
  CURSOR_ANSI_PLACE,
  MAX_CURSOR_MODE
};

EDITOR_LIBSPEC void editor_init(void);
EDITOR_LIBSPEC boolean is_editor(void);

#define EC_MAIN_BOX           25
#define EC_MAIN_BOX_DARK      16
#define EC_MAIN_BOX_CORNER    24
#define EC_MENU_NAME          27
#define EC_CURR_MENU_NAME     159
#define EC_MODE_STR           30
#define EC_MODE_HELP          31
#define EC_CURR_THING         31
#define EC_CURR_PARAM         23
#define EC_OPTION             26
#define EC_HIGHLIGHTED_OPTION 31
#define EC_DEFAULT_COLOR      28
#define EC_NA_FILL            1

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_EDIT_H */
