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

/* Declarations for IDPUT.ASM */

#ifndef MEGAZEUX_IDPUT_H
#define MEGAZEUX_IDPUT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"
#include "data.h"

void set_id_char_by_legacy_index(unsigned index, unsigned char value);

CORE_LIBSPEC void id_put(struct board *src_board, unsigned char x_pos,
 unsigned char y_pos, int array_x, int array_y, int ovr_x, int ovr_y);
CORE_LIBSPEC void draw_game_window(struct board *src_board,
 int array_x, int array_y);
CORE_LIBSPEC void draw_game_window_blind(struct board *src_board,
 int array_x, int array_y, int player_x, int player_y);
CORE_LIBSPEC void draw_viewport(struct board *src_board, int edge_color);

CORE_LIBSPEC unsigned char get_id_char(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_color(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_board_color(struct board *src_board, int id_offset, int ignore_under);
CORE_LIBSPEC unsigned char get_id_under_char(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_under_color(struct board *src_board, int id_offset);

#define ID_CHARS_SIZE               323
#define ID_DMG_SIZE                 128
#define ID_BULLET_COLOR_SIZE        3

// Pre-2.90 world format sizes for these arrays.
#define LEGACY_ID_CHARS_SIZE        ID_CHARS_SIZE
#define LEGACY_ID_DMG_SIZE          ID_DMG_SIZE
#define LEGACY_ID_BULLET_COLOR_SIZE ID_BULLET_COLOR_SIZE

// Constants for the CHANGE CHAR ID command. If any of the ID chars arrays are
// extended, their indexable space for the CHANGE CHAR ID command needs to be
// placed after the existing tables.
#define ID_CHARS_POS          0
#define ID_MISSILE_COLOR_POS  LEGACY_ID_CHARS_SIZE
#define ID_BULLET_COLOR_POS   (ID_MISSILE_COLOR_POS + 1)
#define ID_DMG_POS            (ID_BULLET_COLOR_POS + LEGACY_ID_BULLET_COLOR_SIZE)
#define ID_CHARS_TOTAL_SIZE   (ID_DMG_POS + LEGACY_ID_DMG_SIZE)

#define bullet_char  306
#define player_char  318
#define player_color 322

CORE_LIBSPEC extern unsigned char id_chars[ID_CHARS_SIZE];
CORE_LIBSPEC extern unsigned char id_dmg[ID_DMG_SIZE];
CORE_LIBSPEC extern unsigned char bullet_color[ID_BULLET_COLOR_SIZE];
CORE_LIBSPEC extern unsigned char missile_color;

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IDPUT_H */
