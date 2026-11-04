/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "board.h"
#include "const.h"
#include "error.h"
#include "legacy_board.h"
#include "robot.h"
#include "world.h"
#include "world_format.h"
#include "util.h"
#include "world_struct.h"
#include "algorithm/string_compare.h"
#include "io/memfile.h"
#include "io/zip.h"


static int save_board_info(struct board *cur_board, struct zip_archive *zp,
 int savegame, int file_version, int world_version, const char *name)
{
  const char *tmp;
  char *buffer;
  struct memfile mf;
  int result;

  size_t size = BOARD_PROPS_SIZE;

  if(savegame)
    size += BOARD_SAVE_PROPS_SIZE;

  buffer = cmalloc(size);

  mfopen_wr(buffer, size, &mf);

  save_prop_s_293(BPROP_BOARD_NAME, cur_board->board_name, BOARD_NAME_SIZE, &mf);
  save_prop_w(BPROP_BOARD_WIDTH, cur_board->board_width, &mf);
  save_prop_w(BPROP_BOARD_HEIGHT, cur_board->board_height, &mf);
  save_prop_c(BPROP_OVERLAY_MODE, cur_board->overlay_mode, &mf);
  save_prop_c(BPROP_NUM_ROBOTS, cur_board->num_robots, &mf);
  save_prop_c(BPROP_NUM_SCROLLS, cur_board->num_scrolls, &mf);
  save_prop_c(BPROP_NUM_SENSORS, cur_board->num_sensors, &mf);
  save_prop_w(BPROP_FILE_VERSION, file_version, &mf);

  save_prop_s(BPROP_MOD_PLAYING, cur_board->mod_playing, &mf);
  save_prop_c(BPROP_VIEWPORT_X, cur_board->viewport_x, &mf);
  save_prop_c(BPROP_VIEWPORT_Y, cur_board->viewport_y, &mf);
  save_prop_c(BPROP_VIEWPORT_WIDTH, cur_board->viewport_width, &mf);
  save_prop_c(BPROP_VIEWPORT_HEIGHT, cur_board->viewport_height, &mf);
  save_prop_c(BPROP_CAN_SHOOT, cur_board->can_shoot, &mf);
  save_prop_c(BPROP_CAN_BOMB, cur_board->can_bomb, &mf);
  save_prop_c(BPROP_FIRE_BURN_BROWN, cur_board->fire_burn_brown, &mf);
  save_prop_c(BPROP_FIRE_BURN_SPACE, cur_board->fire_burn_space, &mf);
  save_prop_c(BPROP_FIRE_BURN_FAKES, cur_board->fire_burn_fakes, &mf);
  save_prop_c(BPROP_FIRE_BURN_TREES, cur_board->fire_burn_trees, &mf);
  save_prop_c(BPROP_EXPLOSIONS_LEAVE, cur_board->explosions_leave, &mf);
  save_prop_c(BPROP_SAVE_MODE, cur_board->save_mode, &mf);
  save_prop_c(BPROP_FOREST_BECOMES, cur_board->forest_becomes, &mf);
  save_prop_c(BPROP_COLLECT_BOMBS, cur_board->collect_bombs, &mf);
  save_prop_c(BPROP_FIRE_BURNS, cur_board->fire_burns, &mf);
  save_prop_c(BPROP_BOARD_N, cur_board->board_dir[0], &mf);
  save_prop_c(BPROP_BOARD_S, cur_board->board_dir[1], &mf);
  save_prop_c(BPROP_BOARD_E, cur_board->board_dir[2], &mf);
  save_prop_c(BPROP_BOARD_W, cur_board->board_dir[3], &mf);
  save_prop_c(BPROP_RESTART_IF_ZAPPED, cur_board->restart_if_zapped, &mf);
  save_prop_w(BPROP_TIME_LIMIT, cur_board->time_limit, &mf);
  save_prop_c(BPROP_PLAYER_NS_LOCKED, cur_board->player_ns_locked, &mf);
  save_prop_c(BPROP_PLAYER_EW_LOCKED, cur_board->player_ew_locked, &mf);
  save_prop_c(BPROP_PLAYER_ATTACK_LOCKED, cur_board->player_attack_locked, &mf);
  save_prop_c(BPROP_RESET_ON_ENTRY, cur_board->reset_on_entry, &mf);

  tmp = cur_board->charset_path ? cur_board->charset_path : "";
  save_prop_s(BPROP_CHARSET_PATH, tmp, &mf);

  tmp = cur_board->palette_path ? cur_board->palette_path : "";
  save_prop_s(BPROP_PALETTE_PATH, tmp, &mf);

  if(world_version >= V293)
  {
    save_prop_c(BPROP_RESET_ON_ENTRY_SAME_BOARD,
     cur_board->reset_on_entry_same_board, &mf);
    save_prop_c(BPROP_DRAGONS_CAN_RANDOMLY_MOVE,
     cur_board->dragons_can_randomly_move, &mf);
  }

  if(savegame)
  {
    save_prop_w(BPROP_SCROLL_X, cur_board->scroll_x, &mf);
    save_prop_w(BPROP_SCROLL_Y, cur_board->scroll_y, &mf);
    save_prop_w(BPROP_LOCKED_X, cur_board->locked_x, &mf);
    save_prop_w(BPROP_LOCKED_Y, cur_board->locked_y, &mf);
    save_prop_c(BPROP_PLAYER_LAST_DIR, cur_board->player_last_dir, &mf);
    save_prop_c(BPROP_LAZWALL_START, cur_board->lazwall_start, &mf);
    save_prop_c(BPROP_LAST_KEY, cur_board->last_key, &mf);
    save_prop_d(BPROP_NUM_INPUT, cur_board->num_input, &mf);
    save_prop_d(BPROP_INPUT_SIZE, cur_board->input_size, &mf);

    tmp = cur_board->input_string ? cur_board->input_string : "";
    save_prop_s(BPROP_INPUT_STRING, tmp, &mf);

    save_prop_s(BRPOP_BOTTOM_MESG, cur_board->bottom_mesg, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_TIMER, cur_board->b_mesg_timer, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_ROW, cur_board->b_mesg_row, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_COL, cur_board->b_mesg_col, &mf);
    save_prop_c(BPROP_VOLUME, cur_board->volume, &mf);
    save_prop_c(BPROP_VOLUME_INC, cur_board->volume_inc, &mf);
    save_prop_c(BPROP_VOLUME_TARGET, cur_board->volume_target, &mf);

    // Don't bother saving these compatibility fields unless they're needed.
    if(file_version >= V293 && world_version < V200)
    {
      save_prop_c(BPROP_BLIND_DUR, cur_board->blind_dur_v1, &mf);
      save_prop_c(BPROP_FIREWALKER_DUR, cur_board->firewalker_dur_v1, &mf);
      save_prop_c(BPROP_FREEZE_TIME_DUR, cur_board->freeze_time_dur_v1, &mf);
      save_prop_c(BPROP_SLOW_TIME_DUR, cur_board->slow_time_dur_v1, &mf);
      save_prop_c(BPROP_WIND_DUR, cur_board->wind_dur_v1, &mf);
    }
  }

  save_prop_eof(&mf);

  size = mftell(&mf);

  result = zip_write_file(zp, name, buffer, size, ZIP_M_NONE);

  free(buffer);
  return result;
}

int save_board(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, int board_id)
{
  int board_size = cur_board->board_width * cur_board->board_height;
  char name[10];
  int count;
  int i;

  sprintf(name, "b%2.2X", (unsigned char)board_id);

  if(save_board_info(cur_board, zp, savegame, file_version, mzx_world->version, name))
    goto err;

  sprintf(name+3, "bid");
  if(zip_write_file(zp, name, cur_board->level_id,
   board_size, ZIP_M_DEFLATE))
    goto err;

  sprintf(name+3, "bpr");
  if(zip_write_file(zp, name, cur_board->level_param,
   board_size, ZIP_M_DEFLATE))
    goto err;

  sprintf(name+3, "bco");
  if(zip_write_file(zp, name, cur_board->level_color,
   board_size, ZIP_M_DEFLATE))
    goto err;

  sprintf(name+3, "uid");
  if(zip_write_file(zp, name, cur_board->level_under_id,
   board_size, ZIP_M_DEFLATE))
    goto err;

  sprintf(name+3, "upr");
  if(zip_write_file(zp, name, cur_board->level_under_param,
   board_size, ZIP_M_DEFLATE))
    goto err;

  sprintf(name+3, "uco");
  if(zip_write_file(zp, name, cur_board->level_under_color,
   board_size, ZIP_M_DEFLATE))
    goto err;

  if(cur_board->overlay_mode)
  {
    sprintf(name+3, "och");
    if(zip_write_file(zp, name, cur_board->overlay,
     board_size, ZIP_M_DEFLATE))
      goto err;

    sprintf(name+3, "oco");
    if(zip_write_file(zp, name, cur_board->overlay_color,
     board_size, ZIP_M_DEFLATE))
      goto err;
  }

  name[3] = 'r';
  count = cur_board->num_robots;
  if(count)
  {
    struct robot *cur_robot;

    for(i = 1; i <= count; i++)
    {
      cur_robot = cur_board->robot_list[i];
      if(cur_robot)
      {
        sprintf(name+4, "%2.2X", (unsigned char) i);
        save_robot(mzx_world, cur_robot, zp, savegame, file_version, name);
      }
    }
  }

  sprintf(name+3, "sc");
  count = cur_board->num_scrolls;
  if(count)
  {
    struct scroll *cur_scroll;

    for(i = 1; i <= count; i++)
    {
      cur_scroll = cur_board->scroll_list[i];
      if(cur_scroll)
      {
        sprintf(name+5, "%2.2X", (unsigned char) i);
        save_scroll(cur_scroll, zp, file_version, name);
      }
    }
  }

  sprintf(name+3, "se");
  count = cur_board->num_sensors;
  if(count)
  {
    struct sensor *cur_sensor;

    for(i = 1; i <= count; i++)
    {
      cur_sensor = cur_board->sensor_list[i];
      if(cur_sensor)
      {
        sprintf(name+5, "%2.2X", (unsigned char) i);
        save_sensor(cur_sensor, zp, file_version, name);
      }
    }
  }

  return 0;

err:
  return -1;
}

void default_board_settings(struct world *mzx_world, struct board *cur_board)
{
  cur_board->mod_playing[0] = 0;
  cur_board->viewport_x = 0;
  cur_board->viewport_y = 0;
  cur_board->viewport_width = 80;
  cur_board->viewport_height = 25;
  cur_board->can_shoot = 0;
  cur_board->can_bomb = 0;
  cur_board->fire_burn_brown = 0;
  cur_board->fire_burn_space = 0;
  cur_board->fire_burn_fakes = 0;
  cur_board->fire_burn_trees = 0;
  cur_board->explosions_leave = 0;
  cur_board->save_mode = 1;
  cur_board->forest_becomes = 0;
  cur_board->collect_bombs = 0;
  cur_board->fire_burns = 0;
  cur_board->dragons_can_randomly_move =
   (mzx_world->version < VERSION_PORT || mzx_world->version >= V293) ? true : false;
  cur_board->spittingtiger_moves =
   (mzx_world->version >= V294)         ? SPITTINGTIGER_MOVES_SLOWLY :
   (mzx_world->version >= VERSION_PORT) ? SPITTINGTIGER_MOVES_SLOWLY_SELF_DESTRUCTS
                                        : SPITTINGTIGER_MOVES_NORMALLY;
  cur_board->board_dir[0] = NO_BOARD;
  cur_board->board_dir[1] = NO_BOARD;
  cur_board->board_dir[2] = NO_BOARD;
  cur_board->board_dir[3] = NO_BOARD;
  cur_board->restart_if_zapped = 0;
  cur_board->reset_on_entry = 0;
  cur_board->reset_on_entry_same_board = (mzx_world->version >= V293);
  cur_board->time_limit = 0;
  cur_board->charset_path = NULL;
  cur_board->palette_path = NULL;
  cur_board->charset_path_allocated = 0;
  cur_board->palette_path_allocated = 0;

  cur_board->scroll_x = 0;
  cur_board->scroll_y = 0;
  cur_board->locked_x = -1;
  cur_board->locked_y = -1;

  cur_board->player_last_dir = 0x10;
  cur_board->player_ns_locked = 0;
  cur_board->player_ew_locked = 0;
  cur_board->player_attack_locked = 0;

  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;

  cur_board->lazwall_start = 7;
  cur_board->last_key = '?';
  cur_board->num_input = 0;
  cur_board->input_size = 0;
  cur_board->input_allocated = 0;
  cur_board->input_string = NULL;
  cur_board->bottom_mesg[0] = 0;
  cur_board->b_mesg_timer = 0;
  cur_board->b_mesg_row = 24;
  cur_board->b_mesg_col = -1;
  cur_board->blind_dur_v1 = 0;
  cur_board->firewalker_dur_v1 = 0;
  cur_board->freeze_time_dur_v1 = 0;
  cur_board->slow_time_dur_v1 = 0;
  cur_board->wind_dur_v1 = 0;

#if defined(DEBUG) || defined(CONFIG_EXTRAM)
  cur_board->is_extram = false;
#endif
}

void dummy_board(struct world *mzx_world, struct board *cur_board)
{
  // Allocate placeholder data for broken boards so they will run
  int size = 2000;
  cur_board->overlay_mode = OVERLAY_OFF;
  cur_board->board_width = 80;
  cur_board->board_height = 25;

  default_board_settings(mzx_world, cur_board);

  cur_board->level_id = ccalloc(1, size);
  cur_board->level_color = ccalloc(1, size);
  cur_board->level_param = ccalloc(1, size);
  cur_board->level_under_id = ccalloc(1, size);
  cur_board->level_under_color = ccalloc(1, size);
  cur_board->level_under_param = ccalloc(1, size);

  cur_board->level_id[0] = 127;

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = ccalloc(1, sizeof(struct robot *));
  cur_board->robot_list_name_sorted = ccalloc(1, sizeof(struct robot *));
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = ccalloc(1, sizeof(struct scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = ccalloc(1, sizeof(struct sensor *));
}

void board_set_input_string(struct board *cur_board, const char *input, size_t len)
{
  if(!len || !input || !input[0])
  {
    if(cur_board->input_string)
      cur_board->input_string[0] = '\0';
    return;
  }

  if(len + 1 > cur_board->input_allocated)
  {
    size_t size = MAX(len + 1, 81);
    cur_board->input_string = crealloc(cur_board->input_string, size);
    cur_board->input_allocated = size;
  }
  // The provided input may be a part of the input string!
  memmove(cur_board->input_string, input, len);
  cur_board->input_string[len] = '\0';
}

__editor_maybe_static void board_set_charset_path(struct board *cur_board,
 const char *path, size_t path_len)
{
  if(!path_len || !path || !path[0])
  {
    if(cur_board->charset_path)
      cur_board->charset_path[0] = '\0';
    return;
  }

  if(path_len + 1 > cur_board->charset_path_allocated)
  {
    size_t size = MAX(path_len + 1, 32);
    cur_board->charset_path = crealloc(cur_board->charset_path, size);
    cur_board->charset_path_allocated = size;
  }
  memcpy(cur_board->charset_path, path, path_len);
  cur_board->charset_path[path_len] = '\0';
}

__editor_maybe_static void board_set_palette_path(struct board *cur_board,
 const char *path, size_t path_len)
{
  if(!path_len || !path || !path[0])
  {
    if(cur_board->palette_path)
      cur_board->palette_path[0] = '\0';
    return;
  }

  if(path_len + 1 > cur_board->palette_path_allocated)
  {
    size_t size = MAX(path_len + 1, 32);
    cur_board->palette_path = crealloc(cur_board->palette_path, size);
    cur_board->palette_path_allocated = size;
  }
  memcpy(cur_board->palette_path, path, path_len);
  cur_board->palette_path[path_len] = '\0';
}

#define err_if_missing(expected) if(last_ident < expected) { goto err_free; }

static int load_board_info(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int *file_version)
{
  char *buffer;
  size_t actual_size;
  struct memfile mf;
  struct memfile prop;
  int info_file_version;
  int last_ident = -1;
  int ident;
  int size;
  int v;

  if(zip_get_next_uncompressed_size(zp, &actual_size) != ZIP_SUCCESS)
    return -1;

  buffer = cmalloc(actual_size);

  zip_read_file(zp, buffer, actual_size, &actual_size);

  mfopen(buffer, actual_size, &mf);

  while(next_prop(&prop, &ident, &size, &mf))
  {
    switch(ident)
    {
      case BPROP_EOF:
        mfseek(&mf, 0, SEEK_END);
        break;

      // Essential
      case BPROP_BOARD_NAME:
        size = MIN(size, BOARD_NAME_SIZE - 1);
        mfread(cur_board->board_name, size, 1, &prop);
        cur_board->board_name[size] = 0;
        break;

      case BPROP_BOARD_WIDTH:
        err_if_missing(BPROP_BOARD_NAME);
        cur_board->board_width = load_prop_int(&prop);
        if(cur_board->board_width < 1 || cur_board->board_width > 32767)
          goto err_free;
        break;

      case BPROP_BOARD_HEIGHT:
        err_if_missing(BPROP_BOARD_WIDTH);
        cur_board->board_height = load_prop_int(&prop);
        if(cur_board->board_height < 1 || cur_board->board_height > 32767)
          goto err_free;
        break;

      case BPROP_OVERLAY_MODE:
        err_if_missing(BPROP_BOARD_HEIGHT);
        cur_board->overlay_mode =
         load_prop_int_u(&prop, OVERLAY_OFF, OVERLAY_TRANSPARENT);
        break;

      case BPROP_NUM_ROBOTS:
        err_if_missing(BPROP_OVERLAY_MODE);
        v = load_prop_int_u(&prop, 0, 255);
        cur_board->num_robots = v;
        cur_board->num_robots_allocated = v;
        break;

      case BPROP_NUM_SCROLLS:
        err_if_missing(BPROP_NUM_ROBOTS);
        v = load_prop_int_u(&prop, 0, 255);
        cur_board->num_scrolls = v;
        cur_board->num_scrolls_allocated = v;
        break;

      case BPROP_NUM_SENSORS:
        err_if_missing(BPROP_NUM_SCROLLS);
        v = load_prop_int_u(&prop, 0, 255);
        cur_board->num_sensors = v;
        cur_board->num_sensors_allocated = v;
        break;

      case BPROP_FILE_VERSION:
        err_if_missing(BPROP_NUM_SENSORS);
        info_file_version = load_prop_int(&prop);
        // Rearchived worlds may contain boards from older verisons, so just
        // let version mismatches pass through. This will override the file
        // version for this board and its components only.
        if(info_file_version < V290 || info_file_version > MZX_VERSION)
          goto err_free;

        *file_version = info_file_version;
        break;


      // Non-essential
      case BPROP_MOD_PLAYING:
        size = MIN(size, MAX_PATH-1);
        mfread(cur_board->mod_playing, size, 1, &prop);
        cur_board->mod_playing[size] = 0;
        break;

      case BPROP_VIEWPORT_X:
        cur_board->viewport_x = load_prop_int_u(&prop, 0, 79);
        break;

      case BPROP_VIEWPORT_Y:
        cur_board->viewport_y = load_prop_int_u(&prop, 0, 24);
        break;

      case BPROP_VIEWPORT_WIDTH:
        cur_board->viewport_width = load_prop_int_u(&prop, 1, 80);
        break;

      case BPROP_VIEWPORT_HEIGHT:
        cur_board->viewport_height = load_prop_int_u(&prop, 1, 25);
        break;

      case BPROP_CAN_SHOOT:
        cur_board->can_shoot = load_prop_boolean(&prop);
        break;

      case BPROP_CAN_BOMB:
        cur_board->can_bomb = load_prop_boolean(&prop);
        break;

      case BPROP_FIRE_BURN_BROWN:
        cur_board->fire_burn_brown = load_prop_boolean(&prop);
        break;

      case BPROP_FIRE_BURN_SPACE:
        cur_board->fire_burn_space = load_prop_boolean(&prop);
        break;

      case BPROP_FIRE_BURN_FAKES:
        cur_board->fire_burn_fakes = load_prop_boolean(&prop);
        break;

      case BPROP_FIRE_BURN_TREES:
        cur_board->fire_burn_trees = load_prop_boolean(&prop);
        break;

      case BPROP_EXPLOSIONS_LEAVE:
        cur_board->explosions_leave =
         load_prop_int_u(&prop, EXPL_LEAVE_SPACE, EXPL_LEAVE_FIRE);
        break;

      case BPROP_SAVE_MODE:
        cur_board->save_mode =
         load_prop_int_u(&prop, CAN_SAVE, CAN_SAVE_ON_SENSOR);
        break;

      case BPROP_FOREST_BECOMES:
        cur_board->forest_becomes =
         load_prop_int_u(&prop, FOREST_TO_EMPTY, FOREST_TO_FLOOR);
        break;

      case BPROP_COLLECT_BOMBS:
        cur_board->collect_bombs = load_prop_boolean(&prop);
        break;

      case BPROP_FIRE_BURNS:
        cur_board->fire_burns =
         load_prop_int_u(&prop, FIRE_BURNS_LIMITED, FIRE_BURNS_FOREVER);
        break;

      case BPROP_BOARD_N:
        cur_board->board_dir[0] = load_prop_int_u(&prop, 0, NO_BOARD);
        break;

      case BPROP_BOARD_S:
        cur_board->board_dir[1] = load_prop_int_u(&prop, 0, NO_BOARD);
        break;

      case BPROP_BOARD_E:
        cur_board->board_dir[2] = load_prop_int_u(&prop, 0, NO_BOARD);
        break;

      case BPROP_BOARD_W:
        cur_board->board_dir[3] = load_prop_int_u(&prop, 0, NO_BOARD);
        break;

      case BPROP_RESTART_IF_ZAPPED:
        cur_board->restart_if_zapped = load_prop_boolean(&prop);
        break;

      case BPROP_TIME_LIMIT:
        cur_board->time_limit = load_prop_int(&prop);
        break;

      case BPROP_PLAYER_NS_LOCKED:
        cur_board->player_ns_locked = load_prop_boolean(&prop);
        break;

      case BPROP_PLAYER_EW_LOCKED:
        cur_board->player_ew_locked = load_prop_boolean(&prop);
        break;

      case BPROP_PLAYER_ATTACK_LOCKED:
        cur_board->player_attack_locked = load_prop_boolean(&prop);
        break;

      case BPROP_RESET_ON_ENTRY:
        cur_board->reset_on_entry = load_prop_boolean(&prop);
        break;

      case BPROP_CHARSET_PATH:
        size = MIN(size, MAX_PATH - 1);
        board_set_charset_path(cur_board, (const char *)prop.start, size);
        break;

      case BPROP_PALETTE_PATH:
        size = MIN(size, MAX_PATH - 1);
        board_set_palette_path(cur_board, (const char *)prop.start, size);
        break;

      case BPROP_RESET_ON_ENTRY_SAME_BOARD:
        if(*file_version >= V293 && mzx_world->version >= V293)
          cur_board->reset_on_entry_same_board = load_prop_boolean(&prop);
        break;

      case BPROP_DRAGONS_CAN_RANDOMLY_MOVE:
        if(*file_version >= V293 && mzx_world->version >= V293)
          cur_board->dragons_can_randomly_move = load_prop_boolean(&prop);
        break;


      // Savegame only
      case BPROP_SCROLL_X:
        cur_board->scroll_x = load_prop_int_s(&prop, -32768, 32767);
        break;

      case BPROP_SCROLL_Y:
        cur_board->scroll_y = load_prop_int_s(&prop, -32768, 32767);
        break;

      case BPROP_LOCKED_X:
        cur_board->locked_x = load_prop_int_s(&prop, -1, 32767);
        break;

      case BPROP_LOCKED_Y:
        cur_board->locked_y = load_prop_int_s(&prop, -1, 32767);
        break;

      case BPROP_PLAYER_LAST_DIR:
        cur_board->player_last_dir = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_LAZWALL_START:
        cur_board->lazwall_start = load_prop_int(&prop);
        break;

      case BPROP_LAST_KEY:
        cur_board->last_key = load_prop_int(&prop);
        break;

      case BPROP_NUM_INPUT:
        cur_board->num_input = load_prop_int(&prop);
        break;

      case BPROP_INPUT_SIZE:
        cur_board->input_size = load_prop_int(&prop);
        break;

      case BPROP_INPUT_STRING:
        size = MIN(size, ROBOT_MAX_TR-1);
        board_set_input_string(cur_board, (const char *)prop.start, size);
        break;

      case BRPOP_BOTTOM_MESG:
        size = MIN(size, ROBOT_MAX_TR-1);
        mfread(cur_board->bottom_mesg, size, 1, &prop);
        cur_board->bottom_mesg[size] = 0;
        break;

      case BPROP_BOTTOM_MESG_TIMER:
        cur_board->b_mesg_timer = load_prop_int(&prop);
        break;

      case BPROP_BOTTOM_MESG_ROW:
        cur_board->b_mesg_row = load_prop_int_u(&prop, 0, 24);
        break;

      case BPROP_BOTTOM_MESG_COL:
        cur_board->b_mesg_col = load_prop_int_s(&prop, -1, 79);
        break;

      case BPROP_VOLUME:
        cur_board->volume = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_VOLUME_INC:
        cur_board->volume_inc = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_VOLUME_TARGET:
        cur_board->volume_target = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_BLIND_DUR:
        if(*file_version >= V293)
          cur_board->blind_dur_v1 = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_FIREWALKER_DUR:
        if(*file_version >= V293)
          cur_board->firewalker_dur_v1 = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_FREEZE_TIME_DUR:
        if(*file_version >= V293)
          cur_board->freeze_time_dur_v1 = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_SLOW_TIME_DUR:
        if(*file_version >= V293)
          cur_board->slow_time_dur_v1 = load_prop_int_u(&prop, 0, 255);
        break;

      case BPROP_WIND_DUR:
        if(*file_version >= V293)
          cur_board->wind_dur_v1 = load_prop_int_u(&prop, 0, 255);
        break;

      default:
        break;
    }
    last_ident = ident;
  }

  err_if_missing(BPROP_FILE_VERSION);

  size = cur_board->board_width * cur_board->board_height;

  if(size <= 0 || size > MAX_BOARD_SIZE)
    goto err_free;

  // Fix viewport width/height (these might have been out-of-order).
  cur_board->viewport_width  = CLAMP(cur_board->viewport_width, 1, 80 - cur_board->viewport_x);
  cur_board->viewport_width  = CLAMP(cur_board->viewport_width, 1, cur_board->board_width);
  cur_board->viewport_height = CLAMP(cur_board->viewport_height, 1, 25 - cur_board->viewport_y);
  cur_board->viewport_height = CLAMP(cur_board->viewport_height, 1, cur_board->board_height);

  free(buffer);
  return 0;

err_free:
  free(buffer);
  return 1;
}


static int cmp_robots(const void *dest, const void *src)
{
  struct robot *rsrc = *((struct robot **)src);
  struct robot *rdest = *((struct robot **)dest);
  return strcasecmp(rdest->robot_name, rsrc->robot_name);
}

__editor_maybe_static
int load_board_direct(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id)
{
  unsigned int file_id;
  unsigned int board_id_read;
  unsigned int robot_id_read;

  size_t board_size = 0;

  struct robot **robot_list = NULL;
  struct robot **robot_list_name_sorted = NULL;
  struct scroll **scroll_list = NULL;
  struct sensor **sensor_list = NULL;

  char found_robots[256] = {0};
  char found_scrolls[256] = {0};
  char found_sensors[256] = {0};

  unsigned int num_robots = 0;
  unsigned int num_scrolls = 0;
  unsigned int num_sensors = 0;
  unsigned int num_robots_active = 0;

  unsigned int i;

  int has_base = 0;
  int has_bid = 0;
  int has_bpr = 0;
  int has_bco = 0;
  int has_uid = 0;
  int has_upr = 0;
  int has_uco = 0;
  int has_och = 0;
  int has_oco = 0;

  default_board_settings(mzx_world, cur_board);

  while(ZIP_SUCCESS == zip_get_next_mzx_file_id(zp, &file_id, &board_id_read, &robot_id_read))
  {
    if(board_id_read != board_id)
      break;

    switch(file_id)
    {
      case FILE_ID_BOARD_INFO:
      {
        if(load_board_info(mzx_world, cur_board, zp, savegame, &file_version))
          goto err_invalid;

        has_base = 1;
        board_size = cur_board->board_width * cur_board->board_height;

        cur_board->level_id = ccalloc(1, board_size);
        cur_board->level_param = ccalloc(1, board_size);
        cur_board->level_color = ccalloc(1, board_size);
        cur_board->level_under_id = ccalloc(1, board_size);
        cur_board->level_under_param = ccalloc(1, board_size);
        cur_board->level_under_color = ccalloc(1, board_size);

        if(cur_board->overlay_mode)
        {
          cur_board->overlay = ccalloc(1, board_size);
          cur_board->overlay_color = ccalloc(1, board_size);
        }

        num_robots = cur_board->num_robots;
        num_scrolls = cur_board->num_scrolls;
        num_sensors = cur_board->num_sensors;

        robot_list = ccalloc(num_robots + 1, sizeof(struct robot *));

        robot_list_name_sorted =
         ccalloc(num_robots, sizeof(struct robot *));

        scroll_list = ccalloc(num_scrolls + 1, sizeof(struct scroll *));

        sensor_list = ccalloc(num_sensors + 1, sizeof(struct sensor *));

        cur_board->robot_list = robot_list;
        cur_board->robot_list_name_sorted = robot_list_name_sorted;
        cur_board->scroll_list = scroll_list;
        cur_board->sensor_list = sensor_list;

        break;
      }

      case FILE_ID_BOARD_BID:
      {
        if(!has_base)
          goto err_invalid;

        has_bid = 1;
        zip_read_file(zp, cur_board->level_id, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_BPR:
      {
        if(!has_base)
          goto err_invalid;

        has_bpr = 1;
        zip_read_file(zp, cur_board->level_param, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_BCO:
      {
        if(!has_base)
          goto err_invalid;

        has_bco = 1;
        zip_read_file(zp, cur_board->level_color, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_UID:
      {
        if(!has_base)
          goto err_invalid;

        has_uid = 1;
        zip_read_file(zp, cur_board->level_under_id, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_UPR:
      {
        if(!has_base)
          goto err_invalid;

        has_upr = 1;
        zip_read_file(zp, cur_board->level_under_param, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_UCO:
      {
        if(!has_base)
          goto err_invalid;

        has_uco = 1;
        zip_read_file(zp, cur_board->level_under_color, board_size, NULL);
        break;
      }

      case FILE_ID_BOARD_OCH:
      {
        if(!has_base)
          goto err_invalid;

        if(cur_board->overlay_mode)
        {
          has_och = 1;
          zip_read_file(zp, cur_board->overlay, board_size, NULL);
        }
        else
        {
          zip_skip_file(zp);
        }

        break;
      }

      case FILE_ID_BOARD_OCO:
      {
        if(!has_base)
          goto err_invalid;

        if(cur_board->overlay_mode)
        {
          has_oco = 1;
          zip_read_file(zp, cur_board->overlay_color, board_size, NULL);
        }
        else
        {
          zip_skip_file(zp);
        }

        break;
      }

      case FILE_ID_ROBOT:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_robots && !robot_list[robot_id_read])
        {
          struct robot *cur_robot;

          cur_robot = load_robot_allocate(mzx_world, zp, savegame, file_version);

          robot_list[robot_id_read] = cur_robot;
          robot_list_name_sorted[num_robots_active] = cur_robot;

          num_robots_active++;
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      case FILE_ID_SCROLL:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_scrolls && !scroll_list[robot_id_read])
        {
          scroll_list[robot_id_read] = load_scroll_allocate(zp);
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      case FILE_ID_SENSOR:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_sensors && !sensor_list[robot_id_read])
        {
          sensor_list[robot_id_read] = load_sensor_allocate(zp);
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      default:
      {
        // Whatever it is, it can't be loaded here.
        zip_skip_file(zp);
        break;
      }
    }
  }

  if(!has_base)
    goto err_invalid;

  if(!has_bid)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bid");

  if(!has_bpr)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bpr");

  if(!has_bco)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bco");

  if(!has_uid)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "uid");

  if(!has_upr)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "upr");

  if(!has_uco)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "uco");

  if(!has_och && cur_board->overlay_mode)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "och");

  if(!has_oco && cur_board->overlay_mode)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "oco");

  /* Sort the name sorted list.
   * Do not shorten it; the name sorted list is expected to be num_robots in
   * length at all times, though it's not obvious from the way the legacy loader
   * works. After loading, the legacy loader will perform optimize_null_objects,
   * which removes the unused robots. However, this loader does not, to preserve
   * robot IDs when loading saves. Just leave nulls in the spaces at the end.
   */
  if(num_robots_active > 0)
  {
    qsort(robot_list_name_sorted, num_robots_active,
     sizeof(struct robot *), cmp_robots);
  }

  cur_board->num_robots_active = num_robots_active;

  // Insert the global robot.
  robot_list[0] = &(mzx_world->global_robot);

  // Validate the data on the board.
  if(has_bid)
  {
    char *level_id = cur_board->level_id;
    char *level_param = cur_board->level_param;
    char *level_color = cur_board->level_color;
    char *level_under_id = cur_board->level_under_id;
    unsigned char id;
    unsigned char pr;
    int err;

    // These IDs aren't allowed.
    found_robots[0] = 1;
    found_scrolls[0] = 1;
    found_sensors[0] = 1;

    for(i = 0; i < board_size; i++)
    {
      id = level_id[i];

      switch(id)
      {
        case ROBOT:
        case ROBOT_PUSHABLE:
        {
          pr = level_param[i];

          if(!found_robots[pr] && pr<=num_robots && robot_list[pr])
          {
            found_robots[pr] = 1;
          }
          else
          {
            err = (board_id << 8)|pr;

            if(found_robots[pr])
              error_message(E_ZIP_ROBOT_DUPLICATED, err, NULL);
            else
              error_message(E_ZIP_ROBOT_MISSING_FROM_DATA, err, NULL);

            level_id[i] = CUSTOM_BLOCK;
            level_param[i] = 'R';
            level_color[i] = 0xCF;
          }

          break;
        }

        case SIGN:
        case SCROLL:
        {
          pr = level_param[i];

          if(!found_scrolls[pr] && pr<=num_scrolls && scroll_list[pr])
          {
            found_scrolls[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_id[i] = CUSTOM_BLOCK;
            level_param[i] = 'S';
            level_color[i] = 0xCF;
          }
          break;
        }

        case SENSOR:
        {
          pr = level_param[i];

          if(!found_sensors[pr] && pr<=num_sensors && sensor_list[pr])
          {
            found_sensors[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_id[i] = CUSTOM_FLOOR;
            level_param[i] = 'S';
            level_color[i] = 0xDF;
          }
          break;
        }

        default:
        {
          // If for any reason extkinds find their way into a 2.90+ file...
          if(id > 127)
            level_id[i] = CUSTOM_BLOCK;
          break;
        }
      }

      id = level_under_id[i];

      switch(id)
      {
        case SENSOR:
        {
          pr = cur_board->level_under_param[i];

          if(!found_sensors[pr] && pr<=num_sensors && sensor_list[pr])
          {
            found_sensors[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_under_id[i] = CUSTOM_FLOOR;
            cur_board->level_param[i] = 'S';
            cur_board->level_color[i] = 0xDF;
          }
          break;
        }

        default:
        {
          if(level_under_id[i] > 127)
            level_under_id[i] = CUSTOM_FLOOR;
          break;
        }
      }
    }
  }

  // Now, make sure everything loaded was found on the board.
  for(i = 1; i <= num_robots; i++)
  {
    if(robot_list[i] && !found_robots[i])
    {
      // Deleting these is a pain. Mark them to be cleaned on save.
      error_message(E_ZIP_ROBOT_MISSING_FROM_BOARD, (board_id << 8)|i, NULL);
      robot_list[i]->used = 0;
    }
  }

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i] && !found_scrolls[i])
    {
      // Silently fix.
      clear_scroll(scroll_list[i]);
      scroll_list[i] = NULL;
    }
  }

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i] && !found_sensors[i])
    {
      // Silently fix.
      clear_sensor(sensor_list[i]);
      sensor_list[i] = NULL;
    }
  }

  // Correct scroll/sensor counts.
  while(num_scrolls && !scroll_list[num_scrolls])
    num_scrolls--;

  while(num_sensors && !sensor_list[num_sensors])
    num_sensors--;

  cur_board->num_scrolls = num_scrolls;
  cur_board->num_sensors = num_sensors;
  return VAL_SUCCESS;

err_invalid:
  error_message(E_ZIP_BOARD_CORRUPT, (int)board_id, NULL);
  dummy_board(mzx_world, cur_board);

  return VAL_INVALID;
}

struct board *load_board_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id)
{
  struct board *cur_board = cmalloc(sizeof(struct board));

  load_board_direct(mzx_world, cur_board, zp, savegame, file_version, board_id);

  return cur_board;
}

struct board *duplicate_board(struct world *mzx_world,
 struct board *src_board)
{
  // Create a deep copy of a board into a new board
  struct board *dest_board;
  struct robot **dest_robot_list;
  struct robot **dest_robot_name_list;
  struct scroll **dest_scroll_list;
  struct sensor **dest_sensor_list;

  struct robot *src_robot;
  struct robot *dest_robot;
  struct scroll *src_scroll;
  struct scroll *dest_scroll;
  struct sensor *src_sensor;
  struct sensor *dest_sensor;

  int size = src_board->board_width * src_board->board_height;
  int num_robots_active = 0;
  int i;

  dest_board = cmalloc(sizeof(struct board));
  memcpy(dest_board, src_board, sizeof(struct board));

  // Level data
  dest_board->level_id = cmalloc(size);
  dest_board->level_param = cmalloc(size);
  dest_board->level_color = cmalloc(size);
  dest_board->level_under_id = cmalloc(size);
  dest_board->level_under_param = cmalloc(size);
  dest_board->level_under_color = cmalloc(size);

  memcpy(dest_board->level_id, src_board->level_id, size);
  memcpy(dest_board->level_param, src_board->level_param, size);
  memcpy(dest_board->level_color, src_board->level_color, size);
  memcpy(dest_board->level_under_id, src_board->level_under_id, size);
  memcpy(dest_board->level_under_param, src_board->level_under_param, size);
  memcpy(dest_board->level_under_color, src_board->level_under_color, size);

  if(src_board->overlay_mode)
  {
    dest_board->overlay = cmalloc(size);
    dest_board->overlay_color = cmalloc(size);

    memcpy(dest_board->overlay, src_board->overlay, size);
    memcpy(dest_board->overlay_color, src_board->overlay_color, size);
  }

  // Buffers
  if(src_board->input_string)
  {
    dest_board->input_string = NULL;
    dest_board->input_allocated = 0;
    board_set_input_string(dest_board, src_board->input_string,
     src_board->input_allocated);
  }

  if(src_board->charset_path)
  {
    dest_board->charset_path = NULL;
    dest_board->charset_path_allocated = 0;
    board_set_charset_path(dest_board, src_board->charset_path,
     src_board->charset_path_allocated);
  }

  if(src_board->palette_path)
  {
    dest_board->palette_path = NULL;
    dest_board->palette_path_allocated = 0;
    board_set_palette_path(dest_board, src_board->palette_path,
     src_board->palette_path_allocated);
  }

  // Robots
  dest_robot_list =
   ccalloc(src_board->num_robots_allocated + 1, sizeof(struct robot *));

  dest_robot_name_list =
   ccalloc(src_board->num_robots_allocated, sizeof(struct robot *));

  dest_board->robot_list = dest_robot_list;
  dest_board->robot_list_name_sorted = dest_robot_name_list;

  for(i = 1; i <= src_board->num_robots; i++)
  {
    src_robot = src_board->robot_list[i];

    if(src_robot)
    {
      dest_robot = cmalloc(sizeof(struct robot));

      duplicate_robot_direct(mzx_world, src_robot, dest_robot,
       src_robot->xpos, src_robot->ypos, 0);

      dest_robot_list[i] = dest_robot;
      dest_robot_name_list[num_robots_active] = dest_robot;
      num_robots_active++;
    }
  }

  // Sort the new list.
  qsort(dest_robot_name_list, num_robots_active,
   sizeof(struct robot *), cmp_robots);

  // Should be the same, but just in case:
  dest_board->num_robots_active = num_robots_active;

  // Global robot
  dest_board->robot_list[0] = src_board->robot_list[0];

  // Scrolls
  dest_scroll_list =
   ccalloc(src_board->num_scrolls_allocated + 1, sizeof(struct scroll *));

  dest_board->scroll_list = dest_scroll_list;

  for(i = 1; i <= src_board->num_scrolls; i++)
  {
    src_scroll = src_board->scroll_list[i];

    if(src_scroll)
    {
      dest_scroll = cmalloc(sizeof(struct scroll));
      dest_scroll_list[i] = dest_scroll;

      duplicate_scroll_direct(src_scroll, dest_scroll);
    }
  }

  // Sensors
  dest_sensor_list =
   ccalloc(src_board->num_sensors_allocated + 1, sizeof(struct sensor *));

  dest_board->sensor_list = dest_sensor_list;

  for(i = 1; i <= src_board->num_sensors; i++)
  {
    src_sensor = src_board->sensor_list[i];

    if(src_sensor)
    {
      dest_sensor = cmalloc(sizeof(struct sensor));
      dest_sensor_list[i] = dest_sensor;

      duplicate_sensor_direct(src_sensor, dest_sensor);
    }
  }

  return dest_board;
}

void clear_board(struct board *cur_board)
{
  int i;
  int num_robots_active = cur_board->num_robots_active;
  int num_scrolls = cur_board->num_scrolls;
  int num_sensors = cur_board->num_sensors;
  struct robot **robot_list = cur_board->robot_list;
  struct robot **robot_name_list = cur_board->robot_list_name_sorted;
  struct scroll **scroll_list = cur_board->scroll_list;
  struct sensor **sensor_list = cur_board->sensor_list;

  free(cur_board->level_id);
  free(cur_board->level_param);
  free(cur_board->level_color);
  free(cur_board->level_under_id);
  free(cur_board->level_under_param);
  free(cur_board->level_under_color);

  free(cur_board->input_string);
  free(cur_board->charset_path);
  free(cur_board->palette_path);

  if(cur_board->overlay_mode)
  {
    free(cur_board->overlay);
    free(cur_board->overlay_color);
  }

  for(i = 0; i < num_robots_active; i++)
    if(robot_name_list[i])
      clear_robot(robot_name_list[i]);

  free(robot_name_list);
  free(robot_list);

  for(i = 1; i <= num_scrolls; i++)
    if(scroll_list[i])
      clear_scroll(scroll_list[i]);

  free(scroll_list);

  for(i = 1; i <= num_sensors; i++)
    if(sensor_list[i])
      clear_sensor(sensor_list[i]);

  free(sensor_list);
  free(cur_board);
}

// Just a linear search. Boards aren't addressed by name very often.
int find_board(struct world *mzx_world, char *name)
{
  struct board **board_list = mzx_world->board_list;
  struct board *current_board;
  int num_boards = mzx_world->num_boards;
  int i;
  for(i = 0; i < num_boards; i++)
  {
    current_board = board_list[i];
    if(current_board && !strcasecmp(name, current_board->board_name))
      break;
  }

  if(i != num_boards)
  {
    return i;
  }
  else
  {
    return NO_BOARD;
  }
}
