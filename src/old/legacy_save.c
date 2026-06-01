/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "legacy_world_save.h"

#include "board.h"
#include "const.h"
#include "counter.h"
#include "error.h"
#include "extmem.h"
#include "graphics.h"
#include "idput.h"
#include "legacy_rasm.h"
#include "rasm.h"
#include "sfx.h"
#include "sprite.h"
#include "window.h"
#include "world.h"
#include "world_struct.h"
#include "util.h"

/* This file is a collection of the 2.84 file format saving functions.
 * Nothing here is guaranteed to work; this file exists mainly for reference.
 */

#ifndef CONFIG_LOADSAVE_METER

static inline void meter_update_screen(int *curr, int target) {}
static inline void meter_restore_screen(void) {}
static inline void meter_initial_draw(int curr, int target, const char *title) {}

#endif //!CONFIG_LOADSAVE_METER


/*********/
/* Robot */
/*********/

size_t legacy_save_robot_calculate_size(struct world *mzx_world,
 struct robot *cur_robot, int savegame, int version)
{
  // This both prepares a robot for saving (in the case of a savegame robot in debytecode mzx)
  // and calculates the amount of space it will require. As a result, it is mandatory to
  // call this function before calling save_robot_memory.
  int program_length;
  size_t robot_size = 41; // Default size

#ifdef CONFIG_DEBYTECODE
  if(savegame)
  {
    prepare_robot_bytecode(mzx_world, cur_robot);
    program_length = cur_robot->program_bytecode_length;
  }
  else
  {
    program_length = cur_robot->program_source_length;
  }
#else /* !CONFIG_DEBYTECODE */
  program_length = cur_robot->program_bytecode_length;
#endif /* !CONFIG_DEBYTECODE */

  if(savegame)
  {
    if(version >= V284)
      robot_size += 4; // 4 bytes added for loopcount
    robot_size += 4 * 32; // 32 local counters
    robot_size += 4; // stack size
    robot_size += 4; // stack pointer
    robot_size += 4 * cur_robot->stack_size / 2; // stack
  }
  robot_size += program_length;
  return robot_size;
}

void legacy_save_robot_to_memory(struct robot *cur_robot, struct memfile *mf,
 int savegame, int version)
{
  int program_length;
  int i;

#ifdef CONFIG_DEBYTECODE
  // Write the program's source code if it's a world file, or the
  // bytecode if it's a save file. For save files we currently must write
  // bytecode because it stores the zap status inside the programs
  // themselves (even though it's in the label cache as well).

  if(savegame)
  {
    program_length = cur_robot->program_bytecode_length;
  }
  else
  {
    program_length = cur_robot->program_source_length;
  }

  // As of 2.83 we're writing out 4 byte sizes.
  mfputd(program_length, mf);
#else /* !CONFIG_DEBYTECODE */
  program_length = cur_robot->program_bytecode_length;
  mfputw(program_length, mf);
  // Junk
  mfputw(0, mf);
#endif /* !CONFIG_DEBYTECODE */

  mfwrite(cur_robot->robot_name, LEGACY_ROBOT_NAME_SIZE, 1, mf);

  mfputc(cur_robot->robot_char, mf);
  if(savegame)
  {
    mfputw(cur_robot->cur_prog_line, mf);
    mfputc(cur_robot->pos_within_line, mf);
    mfputc(cur_robot->robot_cycle, mf);
    mfputc(cur_robot->cycle_count, mf);
    mfputc(cur_robot->bullet_type, mf);
    mfputc(cur_robot->is_locked, mf);
    mfputc(cur_robot->can_lavawalk, mf);
    mfputc(cur_robot->walk_dir, mf);
    mfputc(cur_robot->last_touch_dir, mf);
    mfputc(cur_robot->last_shot_dir, mf);
    mfputw(cur_robot->xpos, mf);
    mfputw(cur_robot->ypos, mf);
    mfputc(cur_robot->status, mf);
  }
  else
  {
    // Put some "default" values here instead
    mfputw(1, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputc(1, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputc(0, mf);
    mfputw(cur_robot->xpos, mf);
    mfputw(cur_robot->ypos, mf);
    mfputc(0, mf);
  }

  // Junk local
  mfputw(0, mf);
  mfputc(cur_robot->used, mf);
  // loop_count
  if(version <= V283)
    mfputw(cur_robot->loop_count, mf);
  else // junk
    mfputw(0, mf);

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size = cur_robot->stack_size;

    // Write the local counters
    if(version >= V284)
      mfputd(cur_robot->loop_count, mf);

    for(i = 0; i < 32; i++)
    {
      mfputd(cur_robot->local[i], mf);
    }

    // Put the stack size
    // Divide by two; we don't want to save pos_within_line values.
    mfputd(stack_size/2, mf);

    // Put the stack pointer
    mfputd(cur_robot->stack_pointer/2, mf);

    // Put the stack
    // Only put even indices; odd indices are pos_within_line values.
    for(i = 0; i < stack_size; i += 2)
    {
      mfputd(cur_robot->stack[i], mf);
    }
  }

#ifdef CONFIG_DEBYTECODE
  // NOTE: This will one day be a good thing to move out of the save game;
  // this will require that the programs become immutable and thus have
  // the zap status stored somewhere else.

  if(!savegame)
    mfwrite(cur_robot->program_source, program_length, 1, mf);
  else
#endif
    mfwrite(cur_robot->program_bytecode, program_length, 1, mf);
}

void legacy_save_robot(struct world *mzx_world, struct robot *cur_robot,
 FILE *fp, int savegame, int version)
{
  size_t robot_size = legacy_save_robot_calculate_size(mzx_world, cur_robot,
   savegame, version);
  void *buffer = cmalloc(robot_size);
  struct memfile mf;

  mfopen_wr(buffer, robot_size, &mf);
  legacy_save_robot_to_memory(cur_robot, &mf, savegame, version);

  fwrite(buffer, robot_size, 1, fp);
  free(buffer);
}

void legacy_save_scroll(struct scroll *cur_scroll, FILE *fp, int savegame)
{
  int scroll_size = (int)cur_scroll->mesg_size;

  fputw(cur_scroll->num_lines, fp);
  fputw(0, fp);
  fputw(scroll_size, fp);
  fputc(cur_scroll->used, fp);

  fwrite(cur_scroll->mesg, scroll_size, 1, fp);
}

void legacy_save_sensor(struct sensor *cur_sensor, FILE *fp, int savegame)
{
  fwrite(cur_sensor->sensor_name, 15, 1, fp);
  fputc(cur_sensor->sensor_char, fp);
  fwrite(cur_sensor->robot_to_mesg, 15, 1, fp);
  fputc(cur_sensor->used, fp);
}


/*********/
/* Board */
/*********/

static void save_RLE2_plane(char *plane, FILE *fp, int size)
{
  int i, runsize;
  char current_char;

  for(i = 0; i < size; i++)
  {
    current_char = plane[i];
    runsize = 1;

    while((i < (size - 1)) && (plane[i + 1] == current_char) &&
     (runsize < 127))
    {
      i++;
      runsize++;
    }

    // Put the runsize if necessary
    if((runsize > 1) || current_char & 0x80)
    {
      fputc(runsize | 0x80, fp);
      // Put the run character
      fputc(current_char, fp);
    }
    else
    {
      fputc(current_char, fp);
    }
  }
}

int legacy_save_board(struct world *mzx_world, struct board *cur_board,
 FILE *fp, int savegame, int file_version)
{
  int num_robots, num_scrolls, num_sensors;
  int start_location = ftell(fp);
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  int board_size = board_width * board_height;
  int i;

  // Board mode is now ignored, put 0
  fputc(0, fp);
  // Put overlay mode

  if(cur_board->overlay_mode)
  {
    fputc(0, fp);
    fputc(cur_board->overlay_mode, fp);
    fputw(cur_board->board_width, fp);
    fputw(cur_board->board_height, fp);
    save_RLE2_plane(cur_board->overlay, fp, board_size);
    fputw(cur_board->board_width, fp);
    fputw(cur_board->board_height, fp);
    save_RLE2_plane(cur_board->overlay_color, fp, board_size);
  }

  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_id, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_color, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_param, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_id, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_color, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_param, fp, board_size);

  // Save board parameters

  {
    size_t len = strlen(cur_board->mod_playing);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->mod_playing, len, 1, fp);
  }

  fputc(cur_board->viewport_x, fp);
  fputc(cur_board->viewport_y, fp);
  fputc(cur_board->viewport_width, fp);
  fputc(cur_board->viewport_height, fp);
  fputc(cur_board->can_shoot, fp);
  fputc(cur_board->can_bomb, fp);
  fputc(cur_board->fire_burn_brown, fp);
  fputc(cur_board->fire_burn_space, fp);
  fputc(cur_board->fire_burn_fakes, fp);
  fputc(cur_board->fire_burn_trees, fp);
  fputc(cur_board->explosions_leave, fp);
  fputc(cur_board->save_mode, fp);
  fputc(cur_board->forest_becomes, fp);
  fputc(cur_board->collect_bombs, fp);
  fputc(cur_board->fire_burns, fp);

  for(i = 0; i < 4; i++)
  {
    fputc(cur_board->board_dir[i], fp);
  }

  fputc(cur_board->restart_if_zapped, fp);
  fputw(cur_board->time_limit, fp);

  if(savegame)
  {
    size_t len;

    fputc(cur_board->last_key, fp);
    fputw(cur_board->num_input, fp);
    fputw((int)cur_board->input_size, fp);

    len = strlen(cur_board->input_string);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->input_string, len, 1, fp);

    fputc(cur_board->player_last_dir, fp);

    len = strlen(cur_board->bottom_mesg);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->bottom_mesg, len, 1, fp);

    fputc(cur_board->b_mesg_timer, fp);
    fputc(cur_board->lazwall_start, fp);
    fputc(cur_board->b_mesg_row, fp);
    fputc(cur_board->b_mesg_col, fp);
    fputw(cur_board->scroll_x, fp);
    fputw(cur_board->scroll_y, fp);
    fputw(cur_board->locked_x, fp);
    fputw(cur_board->locked_y, fp);
  }

  fputc(cur_board->player_ns_locked, fp);
  fputc(cur_board->player_ew_locked, fp);
  fputc(cur_board->player_attack_locked, fp);

  if(savegame)
  {
    fputc(cur_board->volume, fp);
    fputc(cur_board->volume_inc, fp);
    fputc(cur_board->volume_target, fp);
  }

  // Save robots
  num_robots = cur_board->num_robots;
  fputc(num_robots, fp);

  if(num_robots)
  {
    struct robot *cur_robot;

    for(i = 1; i <= num_robots; i++)
    {
      cur_robot = cur_board->robot_list[i];
      legacy_save_robot(mzx_world, cur_robot, fp, savegame, file_version);
    }
  }

  // Save scrolls
  num_scrolls = cur_board->num_scrolls;
  putc(num_scrolls, fp);

  if(num_scrolls)
  {
    struct scroll *cur_scroll;

    for(i = 1; i <= num_scrolls; i++)
    {
      cur_scroll = cur_board->scroll_list[i];
      legacy_save_scroll(cur_scroll, fp, savegame);
    }
  }

  // Save sensors
  num_sensors = cur_board->num_sensors;
  fputc(num_sensors, fp);

  if(num_sensors)
  {
    struct sensor *cur_sensor;

    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = cur_board->sensor_list[i];
      legacy_save_sensor(cur_sensor, fp, savegame);
    }
  }

  return (ftell(fp) - start_location);
}


/*********/
/* World */
/*********/

static inline void legacy_save_counter(FILE *fp, struct counter *src_counter)
{
  size_t name_length = strlen(src_counter->name);

  fputd(src_counter->value, fp);
  fputd((int)name_length, fp);
  fwrite(src_counter->name, name_length, 1, fp);
}

static inline void legacy_save_string(FILE *fp, struct string *src_string)
{
  size_t name_length = strlen(src_string->name);
  size_t str_length = src_string->length;

  fputd((int)name_length, fp);
  fputd((int)str_length, fp);
  fwrite(src_string->name, name_length, 1, fp);
  fwrite(src_string->value, str_length, 1, fp);
}


int legacy_save_world(struct world *mzx_world, const char *file, int savegame)
{
  int file_version = MZX_LEGACY_FORMAT_VERSION;

  int i, num_boards;
  int gl_rob_position, gl_rob_save_position;
  int board_offsets_position, board_begin_position;
  int board_size;
  unsigned int *size_offset_list;
  unsigned char *charset_mem;
  unsigned char r, g, b;
  struct board *cur_board;
  FILE *fp;

  int meter_target = 2 + mzx_world->num_boards, meter_curr = 0;

  fp = fopen_unsafe(file, "wb");
  if(!fp)
  {
    error_message(E_WORLD_IO_SAVING, 0, NULL);
    return -1;
  }

  meter_initial_draw(meter_curr, meter_target, "Saving...");

  if(savegame)
  {
    // Write this MZX's version string
    fputs("MZS", fp);
    fputc((file_version >> 8) & 0xff, fp);
    fputc(file_version & 0xff, fp);

    // Write the version of the loaded world for this SAV
    fputw(mzx_world->version, fp);

    fputc(mzx_world->current_board_id, fp);
  }
  else
  {
    fwrite(mzx_world->name, BOARD_NAME_SIZE, 1, fp);

    // No protection
    fputc(0, fp);

    // Write this MZX's version string
    fputc('M', fp);
    fputc((file_version >> 8) & 0xff, fp);
    fputc(file_version & 0xff, fp);
  }

  // Save charset
  charset_mem = cmalloc(3584);
  ec_mem_save_set_var(charset_mem, 3584, 0);
  fwrite(charset_mem, 3584, 1, fp);
  free(charset_mem);

  // Save idchars array.
  fwrite(id_chars, LEGACY_ID_CHARS_SIZE, 1, fp);
  fputc(missile_color, fp);
  fwrite(bullet_color, LEGACY_ID_BULLET_COLOR_SIZE, 1, fp);
  fwrite(id_dmg, LEGACY_ID_DMG_SIZE, 1, fp);

  // Save status counters.
  fwrite((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE,
   NUM_STATUS_COUNTERS, fp);

  /* Older MZX sources refer to SAVE_INDIVIDUAL, but it has always been
   * defined. Exo eventually removed the conditional code in 2.80.
   * We don't need to think about it.
   */

  if(savegame)
  {
    fwrite(mzx_world->keys, NUM_KEYS, 1, fp);
    fputc(mzx_world->blind_dur, fp);
    fputc(mzx_world->firewalker_dur, fp);
    fputc(mzx_world->freeze_time_dur, fp);
    fputc(mzx_world->slow_time_dur, fp);
    fputc(mzx_world->wind_dur, fp);

    for(i = 0; i < 8; i++)
    {
      fputw(mzx_world->pl_saved_x[i], fp);
    }

    for(i = 0; i < 8; i++)
    {
      fputw(mzx_world->pl_saved_y[i], fp);
    }

    fwrite(mzx_world->pl_saved_board, 8, 1, fp);
    fputc(mzx_world->saved_pl_color, fp);
    fputc(mzx_world->under_player_id, fp);
    fputc(mzx_world->under_player_color, fp);
    fputc(mzx_world->under_player_param, fp);
    fputc(mzx_world->mesg_edges, fp);
    fputc(mzx_world->scroll_base_color, fp);
    fputc(mzx_world->scroll_corner_color, fp);
    fputc(mzx_world->scroll_pointer_color, fp);
    fputc(mzx_world->scroll_title_color, fp);
    fputc(mzx_world->scroll_arrow_color, fp);

    {
      size_t len = strlen(mzx_world->real_mod_playing);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->real_mod_playing, len, 1, fp);
    }
  }

  fputc(mzx_world->edge_color, fp);
  fputc(mzx_world->first_board, fp);
  fputc(mzx_world->endgame_board, fp);
  fputc(mzx_world->death_board, fp);
  fputw(mzx_world->endgame_x, fp);
  fputw(mzx_world->endgame_y, fp);
  fputc(mzx_world->game_over_sfx, fp);
  fputw(mzx_world->death_x, fp);
  fputw(mzx_world->death_y, fp);
  fputw(mzx_world->starting_lives, fp);
  fputw(mzx_world->lives_limit, fp);
  fputw(mzx_world->starting_health, fp);
  fputw(mzx_world->health_limit, fp);
  fputc(mzx_world->enemy_hurt_enemy, fp);
  fputc(mzx_world->clear_on_exit, fp);
  fputc(mzx_world->only_from_swap, fp);

  // Palette...
  for(i = 0; i < 16; i++)
  {
    get_rgb(i, &r, &g, &b);
    fputc(r, fp);
    fputc(g, fp);
    fputc(b, fp);
  }

  if(savegame)
  {
    struct counter *mzx_speed, *lock_speed;
    int vlayer_size;

    for(i = 0; i < 16; i++)
    {
      fputc(get_color_intensity(i), fp);
    }
    fputc(get_fade_status(), fp);

    fputw(mzx_world->player_restart_x, fp);
    fputw(mzx_world->player_restart_y, fp);
    fputc(mzx_world->under_player_id, fp);
    fputc(mzx_world->under_player_color, fp);
    fputc(mzx_world->under_player_param, fp);

    // Write regular counters + mzx_speed
    fputd(mzx_world->num_counters + 2, fp);
    for(i = 0; i < mzx_world->num_counters; i++)
    {
      legacy_save_counter(fp, mzx_world->counter_list[i]);
    }

    // Stupid hack
    mzx_speed = malloc(sizeof(struct counter) + sizeof("mzx_speed") - 1);
    mzx_speed->value = mzx_world->mzx_speed;
    strcpy(mzx_speed->name, "mzx_speed");
    legacy_save_counter(fp, mzx_speed);
    free(mzx_speed);

    // another stupid hack
    lock_speed = malloc(sizeof(struct counter) + sizeof("_____lock_speed") - 1);
    lock_speed->value = mzx_world->lock_speed;
    strcpy(lock_speed->name, "_____lock_speed");
    legacy_save_counter(fp, lock_speed);
    free(lock_speed);

    // Write strings
    fputd(mzx_world->num_strings, fp);

    for(i = 0; i < mzx_world->num_strings; i++)
    {
      legacy_save_string(fp, mzx_world->string_list[i]);
    }

    // Sprite data
    for(i = 0; i < MAX_SPRITES; i++)
    {
      fputw((mzx_world->sprite_list[i])->x, fp);
      fputw((mzx_world->sprite_list[i])->y, fp);
      fputw((mzx_world->sprite_list[i])->ref_x, fp);
      fputw((mzx_world->sprite_list[i])->ref_y, fp);
      fputc((mzx_world->sprite_list[i])->color, fp);
      fputc((mzx_world->sprite_list[i])->flags, fp);
      fputc((mzx_world->sprite_list[i])->width, fp);
      fputc((mzx_world->sprite_list[i])->height, fp);
      fputc((mzx_world->sprite_list[i])->col_x, fp);
      fputc((mzx_world->sprite_list[i])->col_y, fp);
      fputc((mzx_world->sprite_list[i])->col_width, fp);
      fputc((mzx_world->sprite_list[i])->col_height, fp);
    }
    // total sprites
    fputc(mzx_world->active_sprites, fp);
    // y order flag
    fputc(mzx_world->sprite_y_order, fp);
    // collision info
    fputw(mzx_world->collision_count, fp);

    for(i = 0; i < MAX_SPRITES; i++)
    {
      fputw(mzx_world->collision_list[i], fp);
    }

    // Multiplier
    fputw(mzx_world->multiplier, fp);
    // Divider
    fputw(mzx_world->divider, fp);
    // Circle divisions
    fputw(mzx_world->c_divisions, fp);
    // String FREAD and FWRITE Delimiters
    fputw(mzx_world->fread_delimiter, fp);
    fputw(mzx_world->fwrite_delimiter, fp);
    // Builtin shooting/message status
    fputc(mzx_world->bi_shoot_status, fp);
    fputc(mzx_world->bi_mesg_status, fp);

    // Write input file name and if open, position
    {
      size_t len = strlen(mzx_world->input_file_name);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->input_file_name, len, 1, fp);
    }
    fputd(mzx_world->temp_input_pos, fp);

    // Write output file name and if open, position
    {
      size_t len = strlen(mzx_world->output_file_name);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->output_file_name, len, 1, fp);
    }
    fputd(mzx_world->temp_output_pos, fp);

    fputw(get_screen_mode(), fp);

    if(get_screen_mode() > 1)
    {
      // Put SMZX mode 2 palette
      for(i = 0; i < 256; i++)
      {
        get_rgb(i, &r, &g, &b);
        fputc(r, fp);
        fputc(g, fp);
        fputc(b, fp);
      }
    }

    fputd(mzx_world->commands, fp);

    vlayer_size = mzx_world->vlayer_size;
    fputd(vlayer_size, fp);
    fputw(mzx_world->vlayer_width, fp);
    fputw(mzx_world->vlayer_height, fp);

    fwrite(mzx_world->vlayer_chars, 1, vlayer_size, fp);
    fwrite(mzx_world->vlayer_colors, 1, vlayer_size, fp);
  }

  // Put position of global robot later
  gl_rob_save_position = ftell(fp);
  // Put some 0's
  fputd(0, fp);

  // Put custom fx?
  if(mzx_world->custom_sfx_on == 1)
  {
    int offset = 0;
    size_t sfx_len;
    int length_slot_pos, next_pos, total_len;
    fputc(0, fp);
    length_slot_pos = ftell(fp);
    fputw(0, fp);
    for(i = 0; i < NUM_SFX; i++, offset += LEGACY_SFX_SIZE)
    {
      sfx_len = strlen(mzx_world->custom_sfx + offset);
      fputc((int)sfx_len, fp);
      fwrite(mzx_world->custom_sfx + offset, sfx_len, 1, fp);
    }
    // Get size of the block
    next_pos = ftell(fp);
    total_len = (next_pos - length_slot_pos) - 2;
    fseek(fp, length_slot_pos, SEEK_SET);
    fputw(total_len, fp);
    fseek(fp, next_pos, SEEK_SET);
  }

  meter_update_screen(&meter_curr, meter_target);

  num_boards = mzx_world->num_boards;
  fputc(num_boards, fp);

  // Put the names
  for(i = 0; i < num_boards; i++)
  {
    fwrite((mzx_world->board_list[i])->board_name, 25, 1, fp);
  }

  /* Due to some bugs in the NDS's libfat library, seeking backwards
   * from the end results in data corruption. To prevent this, waste
   * a little bit of memory caching the offsets of the board data so
   * we can rewrite the size/offset list with less seeking later.
   */
  size_offset_list = cmalloc(8 * num_boards);
  board_offsets_position = ftell(fp);
  fseek(fp, 8 * num_boards, SEEK_CUR);

  for(i = 0; i < num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];

    // Before messing with the board, make sure the board is
    // rid of any gaps in the object lists...
    optimize_null_objects(cur_board);

    // First save the offset of where the board will be placed
    board_begin_position = ftell(fp);
    // Now save the board and get the size
    board_size = legacy_save_board(mzx_world, cur_board, fp, savegame,
     file_version);
    // board_end_position, unused
    ftell(fp);
    // Record size/offset information.
    size_offset_list[2 * i] = board_size;
    size_offset_list[2 * i + 1] = board_begin_position;

    meter_update_screen(&meter_curr, meter_target);
  }

  // Save for global robot position
  gl_rob_position = ftell(fp);
  legacy_save_robot(mzx_world, &mzx_world->global_robot, fp, savegame, file_version);

  meter_update_screen(&meter_curr, meter_target);

  // Go back to where the global robot position should be saved
  fseek(fp, gl_rob_save_position, SEEK_SET);
  fputd(gl_rob_position, fp);

  // Go back to offsets/size list
  fseek(fp, board_offsets_position, SEEK_SET);
  for(i = 0; i < num_boards; i++)
  {
    fputd(size_offset_list[2 * i  ], fp);
    fputd(size_offset_list[2 * i + 1], fp);
  }
  free(size_offset_list);

  meter_restore_screen();

  fclose(fp);
  return 0;
}
