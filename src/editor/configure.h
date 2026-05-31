/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef MEGAZEUX_EDITOR_CONFIGURE_H
#define MEGAZEUX_EDITOR_CONFIGURE_H

#include "../compat.h"
#include "../const.h"

MEGAZEUX_BEGIN_DECLS

#include "macro.h"

#define NUM_SAVED_POSITIONS 10

enum robo_ed_color_codes
{
  ROBO_ED_CC_CURRENT_LINE,
  ROBO_ED_CC_IMMEDIATES,
  ROBO_ED_CC_IMMEDIATES_2,
  ROBO_ED_CC_CHARACTERS,
  ROBO_ED_CC_COLORS,
  ROBO_ED_CC_DIRECTIONS,
  ROBO_ED_CC_THINGS,
  ROBO_ED_CC_PARAMS,
  ROBO_ED_CC_STRINGS,
  ROBO_ED_CC_EQUALITIES,
  ROBO_ED_CC_CONDITIONS,
  ROBO_ED_CC_ITEMS,
  ROBO_ED_CC_EXTRAS,
  ROBO_ED_CC_COMMANDS,
  ROBO_ED_CC_INVALID_IGNORE,
  ROBO_ED_CC_INVALID_DELETE,
  ROBO_ED_CC_INVALID_COMMENT
};

struct saved_position
{
  int board_id;
  int cursor_x;
  int cursor_y;
  int scroll_x;
  int scroll_y;
  int debug_x;
};

struct editor_config_info
{
  // Board editor options
  boolean board_editor_hide_help;
  boolean editor_space_toggles;
  boolean editor_tab_focuses_view;
  boolean editor_load_board_assets;
  boolean editor_thing_menu_places;
  boolean editor_show_thing_toggles;
  int editor_show_thing_blink_speed;
  int undo_history_size;

  // Defaults for new boards
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
  int board_width;
  int board_height;
  boolean can_shoot;
  boolean can_bomb;
  boolean fire_burns_spaces;
  boolean fire_burns_fakes;
  boolean fire_burns_trees;
  boolean fire_burns_brown;
  boolean fire_burns_forever;
  boolean forest_to_floor;
  boolean collect_bombs;
  boolean dragons_can_randomly_move;
  boolean restart_if_hurt;
  boolean reset_on_entry;
  boolean reset_on_entry_same_board;
  boolean player_locked_ns;
  boolean player_locked_ew;
  boolean player_locked_att;
  int time_limit;
  int explosions_leave;
  int saving_enabled;
  int overlay_enabled;
  char charset_path[MAX_PATH];
  char palette_path[MAX_PATH];

  // Palette editor options
  boolean palette_editor_hide_help;

  // Robot editor options
  boolean editor_enter_splits;
  char color_codes[32];
  boolean color_coding_on;
  int default_invalid_status;
  boolean disassemble_extras;
  int disassemble_base;
  boolean robot_editor_hide_help;

  // Backup options
  int backup_count;
  int backup_interval;
  char backup_name[256];
  char backup_ext[256];

  // Macro options
  char default_macros[5][64];
  int num_extended_macros;
  int num_macros_allocated;
  struct ext_macro **extended_macros;

  // Saved positions
  struct saved_position saved_positions[NUM_SAVED_POSITIONS];
  struct saved_position vlayer_positions[NUM_SAVED_POSITIONS];
};

EDITOR_LIBSPEC void default_editor_config(void);
EDITOR_LIBSPEC void store_editor_config_backup(void);
EDITOR_LIBSPEC void free_editor_config(void);

EDITOR_LIBSPEC struct editor_config_info *get_editor_config(void);
void load_editor_config_backup(void);

size_t get_local_editor_config_name(char *dest, size_t dest_len,
 const char *mzx_file_path);
void save_local_editor_config(struct editor_config_info *conf,
 const char *mzx_file_path);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_CONFIGURE_H */
