/* MegaZeux
 *
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

#ifndef MEGAZEUX_BOARD_STRUCT_H
#define MEGAZEUX_BOARD_STRUCT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "robot_struct.h"

struct board
{
  char board_name[32];

  int board_width;
  int board_height;

  int overlay_mode;
  char *level_id;
  char *level_param;
  char *level_color;
  char *level_under_id;
  char *level_under_param;
  char *level_under_color;
  char *overlay;
  char *overlay_color;

  char mod_playing[MAX_PATH];
  int viewport_x;
  int viewport_y;
  int viewport_width;
  int viewport_height;
  int can_shoot;
  int can_bomb;
  int fire_burn_brown;
  int fire_burn_space;
  int fire_burn_fakes;
  int fire_burn_trees;
  int explosions_leave;
  int save_mode;
  int forest_becomes;
  int collect_bombs;
  int fire_burns;
  int dragons_can_randomly_move;
  int spittingtiger_moves;
  int board_dir[4];
  int restart_if_zapped;
  int reset_on_entry;
  int reset_on_entry_same_board;
  int time_limit;
  int last_key;
  int num_input;
  size_t input_size;
  size_t input_allocated;
  char *input_string;
  int player_last_dir;
  char bottom_mesg[ROBOT_MAX_TR];
  int b_mesg_timer;
  int lazwall_start;
  int b_mesg_row;
  int b_mesg_col;
  int scroll_x;
  int scroll_y;
  int locked_x;
  int locked_y;
  int player_ns_locked;
  int player_ew_locked;
  int player_attack_locked;
  int volume;
  int volume_inc;
  int volume_target;
  char *charset_path;
  char *palette_path;
  size_t charset_path_allocated;
  size_t palette_path_allocated;
  unsigned char blind_dur_v1;
  unsigned char firewalker_dur_v1;
  unsigned char freeze_time_dur_v1;
  unsigned char slow_time_dur_v1;
  unsigned char wind_dur_v1;

  int num_robots;
  int num_robots_active;
  int num_robots_allocated;
  struct robot **robot_list;
  struct robot **robot_list_name_sorted;
  int num_scrolls;
  int num_scrolls_allocated;
  struct scroll **scroll_list;
  int num_sensors;
  int num_sensors_allocated;
  struct sensor **sensor_list;
#if defined(DEBUG) || defined(CONFIG_EXTRAM)
  boolean is_extram;
#endif
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_BOARD_STRUCT_H */
