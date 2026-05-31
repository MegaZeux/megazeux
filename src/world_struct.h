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

#ifndef MEGAZEUX_WORLD_STRUCT_H
#define MEGAZEUX_WORLD_STRUCT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "board_struct.h"
#include "robot_struct.h"
#include "counter_struct.h"
#include "sprite_struct.h"

#include "io/vfile.h"
#include "audio/sfx.h"

#define COMMAND_CACHE_CURRENT_TIME (1 << 0)

enum change_game_state_value
{
  CHANGE_STATE_NONE,
  CHANGE_STATE_INTERRUPT_CYCLE, // Cycle interrupted without state change.
  CHANGE_STATE_SWAP_WORLD,
  CHANGE_STATE_LOAD_GAME_ROBOTIC,
  CHANGE_STATE_EXIT_GAME_ROBOTIC,
  CHANGE_STATE_PLAY_GAME_ROBOTIC,
  CHANGE_STATE_REQUEST_EXIT
};

enum fwrite_mode
{
  FWRITE_MODE_UNKNOWN,
  FWRITE_MODE_TRUNCATE,
  FWRITE_MODE_MODIFY,
  FWRITE_MODE_APPEND,
  FWRITE_MODE_MAX
};

struct world
{
  // 0 if a world has been loaded, 1 if it hasn't
  int active;
  char name[BOARD_NAME_SIZE];

  int version;
  // Move here eventually
  //char id_chars[ID_CHARS_SIZE];
  //char id_dmg[ID_DMG_SIZE];
  //char bullet_color[ID_BULLET_COLOR_SIZE];
  //char missile_color;
  char status_counters_shown[NUM_STATUS_COUNTERS][COUNTER_NAME_SIZE];

  // Save game material, part 1
  char keys[NUM_KEYS];
  int blind_dur;
  int firewalker_dur;
  int freeze_time_dur;
  int slow_time_dur;
  int wind_dur;
  int pl_saved_x[8];
  int pl_saved_y[8];
  int pl_saved_board[8];
  int saved_pl_color;
  int under_player_id;
  int under_player_color;
  int under_player_param;
  int mesg_edges;
  int scroll_base_color;
  int scroll_corner_color;
  int scroll_pointer_color;
  int scroll_title_color;
  int scroll_arrow_color;
  char real_mod_playing[MAX_PATH];
  int max_samples;
  int smzx_message;

  int edge_color;
  int first_board;
  int endgame_board;
  int death_board;
  int endgame_x;
  int endgame_y;
  int game_over_sfx;
  int death_x;
  int death_y;
  int starting_lives;
  int lives_limit;
  int starting_health;
  int health_limit;
  int enemy_hurt_enemy;
  int clear_on_exit;
  int only_from_swap;

  // Save game material, part 2
  struct counter_list counter_list;
  struct string_list string_list;
  int player_restart_x;
  int player_restart_y;
  int num_sprites;
  int num_sprites_allocated;
  int sprite_num;
  struct sprite **sprite_list;
  int active_sprites;
  int sprite_y_order;
  int collision_count;
  int *collision_list;
  int multiplier;
  int divider;
  int c_divisions;
  int fread_delimiter;
  int fwrite_delimiter;
  int bi_shoot_status;
  int bi_mesg_status;
  char output_file_name[MAX_PATH];
  char input_file_name[MAX_PATH];
  vfile *output_file;
  vfile *input_file;
  vdir *input_directory;
  boolean input_is_dir;
  enum fwrite_mode output_mode;
  int temp_input_pos;
  int temp_output_pos;
  int commands;
  int commands_stop;
  int vlayer_size;
  int vlayer_width;
  int vlayer_height;
  char *vlayer_chars;
  char *vlayer_colors;

  int num_boards;
  int num_boards_allocated;
  struct board **board_list;
  struct board *current_board;
  int current_board_id;
  int temporary_board;

  struct robot global_robot;

  struct sfx_list custom_sfx;

  // Not part of world/save files, but runtime globals
  int player_x;
  int player_y;
  int player_shoot_cooldown;

  // For moving the player between boards
  enum board_target target_where;
  int target_board;
  int target_x;
  int target_y;
  enum thing target_id;
  int target_color;
  enum thing target_d_id;
  int target_d_color;

  // Current bomb type
  int bomb_type;

  // Indiciates if the player is dead
  boolean dead;

  // Toggle used to skip the board update when slow time or
  // freeze time are active.
  boolean current_cycle_frozen;

  // Toggle used by certain built-in mechanics that update every other cycle.
  boolean current_cycle_odd;

  // Did the player just move?
  boolean player_moved;

  // Was the player on an entrance before the board scan?
  boolean player_was_on_entrance;

  // Was the player damaged while 'restart if hurt' is active?
  boolean was_zapped;

  // For use in repeat delays for player movement
  int key_up_delay;
  int key_down_delay;
  int key_right_delay;
  int key_left_delay;

  // 1-3 normal 5-7 is 1-3 but from a REL FIRST cmd
  int first_prefix;
  // Just 1-3
  int mid_prefix;
  // 1-3 normal 5-7 is 1-3 but from a REL LAST cmd
  int last_prefix;
  // Flags for data that persists during a command (keep in the prefix cache line).
  int command_cache;

  // Lets the get counter routines indiciate to the caller
  // that the result is not a typical counter but something
  // special. It's there for those horribly hacked file open
  // counters. It should normally be set to FOPEN_NONE.
  enum special_counter_return special_counter_return;

  // These are used to handle SAVE_GAME
  enum { SAVE_NONE, SAVE_GAME } robotic_save_type;
  char robotic_save_path[MAX_PATH];

  // Indicates a robotic world swap, savegame load, or game exit
  // FIXME this might be better suited to the game context.
  enum change_game_state_value change_game_state;

  // Current speed of MZX world
  int mzx_speed;
  // If we can change the speed from the F2 menu.
  int lock_speed;

  // Joystick state data.
  boolean joystick_simulate_keys;

  // Editor specific state flags.
  boolean editing;
  boolean debug_mode;

  // World validation: we don't want to alloc this file twice.
  char *raw_world_info;
  int raw_world_info_size;

  // Keep this open, just once
  vfile *help_file;

  // An array for game2.cpp
  char *update_done;
  int update_done_size;

  // Cached time for the current robot command.
  struct tm current_time;
  int64_t current_time_epoch;
  int32_t current_time_nano;
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_WORLD_STRUCT_H */
