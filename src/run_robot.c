/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "block.h"
#include "board.h"
#include "const.h"
#include "core.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "expr.h"
#include "game.h"
#include "game_ops.h"
#include "game_player.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "intake.h"
#include "mzm.h"
#include "robot.h"
#include "scrdisp.h"
#include "sprite.h"
#include "str.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "io/fsafeopen.h"

#include "audio/audio.h"
#include "audio/sfx.h"
#include "event/event.h"

#define parsedir(a, b, c, d) \
 parsedir(mzx_world, a, b, c, d, _bl[0], _bl[1], _bl[2], _bl[3])

static const char *const item_to_counter[9] =
{
  "GEMS",
  "AMMO",
  "TIME",
  "SCORE",
  "HEALTH",
  "LIVES",
  "LOBOMBS",
  "HIBOMBS",
  "COINS"
};

// Default parameters (-2 = 0 and no edit, -3 = character, -4 = board)
__editor_maybe_static const int def_params[128] =
{
  -2, -2, -2, -2, -2, -3, -2, -3, -2, -2, -3, -2, -3, -2, -2, -2, // 0x00 - 0x0F
  -2, -3, -2, -2, -2, -2, -2, -2, -2, -2, -2,  0, -2, -2, 10,  0, // 0x10 - 0x1F
   0, -2, -2,  5,  0,128, 32, -2, -2,  0, -2, -4, -4, -2, -2,  0, // 0x20 - 0x2F
  -2,  0, -2, -3, -3, -3, -3, 17,  0, -2, -2, -2,  0,  4,  0, -2, // 0x30 - 0x3F
  -2, -2, -2, -4, -4, -4, -4, -2,  0, -2, 48,  0, -3, -3,  0,127, // 0x40 - 0x4F
  32, 26,  2,  1,  0,  2, 65,  2, 66,  2,129, 34, 66, 66, 12, 10, // 0x50 - 0x5F
  -2,194, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, // 0x60 - 0x6F
  -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,  0,  0,  0,  0,  0, -2  // 0x70 - 0x7F
};

static void magic_load_mod(struct world *mzx_world, char *filename)
{
  struct board *src_board = mzx_world->current_board;
  size_t mod_name_size;

  if(load_game_module(mzx_world, filename, false))
    strcpy(src_board->mod_playing, filename);

  // If a * was provided, set the current board mod to *.
  mod_name_size = strlen(filename);
  if(mod_name_size && filename[mod_name_size - 1] == '*')
  {
    strcpy(src_board->mod_playing, "*");
  }
}

/**
 * MegaZeux 1.x performed the board changes for TELEPORT, RESTORE PLAYER
 * POSITION, and EXCHANGE PLAYER POSITION immediately. To prevent executing
 * part of the new board, TELEPORT would clear the label received flags
 * instruct the board scan to abort. Unfortunately, this needs to be supported.
 * The RESTORE/EXCHANGE commands do not do this and can only be supported in
 * a partial manner by canceling the cycle if the boards are different.
 */
static void immediate_board_change_compat_v1(struct world *mzx_world,
 boolean is_teleport)
{
  if(mzx_world->version < V200 &&
   (is_teleport || mzx_world->current_board_id != mzx_world->target_board))
  {
    mzx_world->change_game_state = CHANGE_STATE_INTERRUPT_CYCLE;
  }
}

static void save_player_position(struct world *mzx_world, int pos)
{
  mzx_world->pl_saved_x[pos] = mzx_world->player_x;
  mzx_world->pl_saved_y[pos] = mzx_world->player_y;
  mzx_world->pl_saved_board[pos] = mzx_world->current_board_id;
}

static void restore_player_position(struct world *mzx_world, int pos)
{
  mzx_world->target_x = mzx_world->pl_saved_x[pos];
  mzx_world->target_y = mzx_world->pl_saved_y[pos];
  mzx_world->target_board = mzx_world->pl_saved_board[pos];
  mzx_world->target_where = TARGET_POSITION;
  immediate_board_change_compat_v1(mzx_world, false);
}

static void calculate_blocked(struct world *mzx_world, int x, int y, int id,
 int bl[4])
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  char *level_id = src_board->level_id;

  if(id)
  {
    int i, offset, new_x, new_y;
    for(i = 0; i < 4; i++)
    {
      new_x = x;
      new_y = y;

      if(!move_dir(src_board, &new_x, &new_y, int_to_dir(i)))
      {
        offset = new_x + (new_y * board_width);
        // Not edge... blocked?
        if((flags[(int)level_id[offset]] & A_UNDER) != A_UNDER)
          bl[i] = 1;
        else
          bl[i] = 0;
      }
      else
      {
        bl[i] = 1; // Edge is considered blocked
      }
    }
  }
}

// Turns a color (including those w/??) to a real color (0-255)
static int fix_color(int color, int def)
{
  if(color < 256)
    return color;
  if(color < 272)
    return (color & 0x0F) + (def & 0xF0);
  if(color < 288)
    return ((color - 272) << 4) + (def & 0x0F);

  return def;
}

int place_at_xy(struct world *mzx_world, enum thing id, int color, int param,
 int x, int y)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  enum thing old_id = (enum thing)src_board->level_id[offset];

  // 2.80+: wildcard parameter will preserve the parameter of the
  // destination if both things are currently configured to have an ID char
  // of 255 (use parameter for char). Otherwise, the wildcard expands to the
  // default parameter of the replacement thing instead.
  if(param == 256 && mzx_world->version >= V280)
  {
    if((id_chars[old_id] != 255) || (id_chars[id] != 255))
    {
      if(def_params[id] > 0)
        param = def_params[id];
      else
        param = 0;
    }
    else
    {
      param = src_board->level_param[offset];
    }
  }
  // Invalid parameters and p?? (pre-2.80) evaluate modulo 256.
  param &= 0xff;

  if(old_id == SENSOR)
  {
    // Okay to put player on sensor; it won't destroy the sensor
    if(id != PLAYER)
    {
      int old_param = src_board->level_param[offset];
      clear_sensor_id(src_board, old_param);
    }
  }
  else

  if(is_signscroll(old_id))
  {
    int old_param = src_board->level_param[offset];
    clear_scroll_id(src_board, old_param);
  }
  else

  if(is_robot(old_id))
  {
    int old_param = src_board->level_param[offset];
    clear_robot_id(src_board, old_param);
  }
  else

  if(old_id == PLAYER)
  {
    return 0; // Don't allow the player to be overwritten
  }

  id_place(mzx_world, x, y, id,
   fix_color(color, src_board->level_color[offset]), param);

  return 1;
}

static int place_under_xy(struct world *mzx_world, struct board *src_board,
 enum thing id, int color, int param, int x, int y)
{
  int board_width = src_board->board_width;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;

  int offset = x + (y * board_width);
  // 2.80+: wildcard parameter will preserve the parameter of whatever is
  // beneath, regardless of the destination and replacement things' ID chars.
  // Interesting oversight!
  if(param == 256 && mzx_world->version >= V280)
    param = level_under_param[offset];

  color = fix_color(color, level_under_color[offset]);
  if(level_under_id[offset] == SENSOR)
    clear_sensor_id(src_board, level_under_param[offset]);

  level_under_id[offset] = (char)id;
  level_under_color[offset] = color;
  level_under_param[offset] = param;

  return 1;
}

static int place_dir_xy(struct world *mzx_world, enum thing id, int color,
 int param, int x, int y, enum dir direction, struct robot *cur_robot,
 int *_bl)
{
  struct board *src_board = mzx_world->current_board;

  // Check under
  if(direction == BENEATH)
  {
    return place_under_xy(mzx_world, src_board, id, color, param, x, y);
  }
  else
  {
    int new_x = x;
    int new_y = y;
    direction = parsedir(direction, x, y, cur_robot->walk_dir);

    if(is_cardinal_dir(direction))
    {
      if(!move_dir(src_board, &new_x, &new_y, direction))
      {
        return place_at_xy(mzx_world, id, color, param,
         new_x, new_y);
      }
    }
    return 0;
  }
}

int place_player_xy(struct world *mzx_world, int x, int y)
{
  if((mzx_world->player_x != x) || (mzx_world->player_y != y))
  {
    struct board *src_board = mzx_world->current_board;
    int offset = x + (y * src_board->board_width);
    int did = src_board->level_id[offset];
    int dparam = src_board->level_param[offset];

    if(is_robot(did))
    {
      clear_robot_id(src_board, dparam);
    }
    else

    if(is_signscroll(did))
    {
      clear_scroll_id(src_board, dparam);
    }
    else

    if(did == SENSOR)
    {
      step_sensor(mzx_world, dparam);
    }

    id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
    id_place(mzx_world, x, y, PLAYER, 0, 0);
    mzx_world->player_x = x;
    mzx_world->player_y = y;

    return 1;
  }

  return 0;
}

static void send_at_xy(struct world *mzx_world, int id, int x, int y,
 char *label)
{
  struct board *src_board = mzx_world->current_board;
  int offset = x + (y * src_board->board_width);
  enum thing d_id = (enum thing)src_board->level_id[offset];

  if(is_robot(d_id))
  {
    char label_buffer[ROBOT_MAX_TR];
    int d_param = src_board->level_param[offset];

    tr_msg(mzx_world, label, id, label_buffer);

    if(d_param == id)
    {
      send_robot_self(mzx_world,
       mzx_world->current_board->robot_list[id], label_buffer, 0);
    }
    else
    {
      send_robot_id(mzx_world, src_board->level_param[offset],
       label_buffer, 0);
    }
  }
}

static int get_random_range(int min_value, int max_value)
{
  int result;
  uint64_t difference;

  if(min_value == max_value)
  {
    result = min_value;
  }
  else
  {
    if(max_value > min_value)
    {
      difference = (int64_t)max_value - (int64_t)min_value;
    }
    else
    {
      difference = (int64_t)min_value - (int64_t)max_value;
      min_value = max_value;
    }

    result = Random(difference + 1) + min_value;
  }

  return result;
}

static int send_self_label_tr(struct world *mzx_world, char *param, int id)
{
  char label_buffer[ROBOT_MAX_TR];
  tr_msg(mzx_world, param, id, label_buffer);

  if(send_robot_self(mzx_world,
   mzx_world->current_board->robot_list[id], label_buffer, 1))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

static void split_colors(int color, int *fg, int *bg)
{
  if(color & 256)
  {
    color ^= 256;
    if(color == 32)
    {
      *fg = 16;
      *bg = 16;
    }
    else

    if(color < 16)
    {
      *bg = 16;
      *fg = color;
    }
    else
    {
      *bg = color - 16;
      *fg = 16;
    }
  }
  else
  {
    *bg = color >> 4;
    *fg = color & 0x0F;
  }
}

/**
 * The change command has the same broken semantics on the replacement
 * thing as on the match thing prior to 2.80. This nasty hack converts
 * invalid edge cases to be compatible with fix_colors/param modulo.
 */
static void replacement_version_fix(struct world *mzx_world, int *color,
 int *param, boolean *weird_color)
{
  if(mzx_world->version < VERSION_PORT)
  {
    if(*color < 0 || *color > 288)
    {
      int fg, bg;
      split_colors(*color, &fg, &bg);

      /* DOS has a very weird edge case with color<0, bit 8 set where it ends
       * up ORing the source background color with the replacement. */
      if(fg < 0 && bg == 16)
      {
        *color &= 0xff;
        *weird_color = true;
      }
      else

      if(fg >= 16 && bg >= 16)
        *color = 288;
      else
      if(fg >= 16)
        *color = 272 | (bg & 0x0f);
      else
      if(bg >= 16)
        *color = 256 | (fg & 0x0f);
      else
        *color &= 0xff;
    }
    if(*param > 256)
      *param = 256;
  }
}

/**
 * Prior to 2.80, match parameters >256 act identically to p??.
 * Post-split_colors fg and bg >16 act identically to a ? for that component.
 */
static void match_version_fix(struct world *mzx_world, int *fg, int *bg, int *param)
{
  if(mzx_world->version < VERSION_PORT)
  {
    if(*fg > 16)
      *fg = 16;
    if(*bg > 16)
      *bg = 16;
    if(param && *param > 256)
      *param = 256;
  }
}

/* Some nuances here that need to be preserved in the off-chance that something
 * accidentally relied on them:
 *
 * - The match param may be <0 or >256, which always fails the match.
 * - The match colors may be <0 or >16, which always fails the match
 *   (see split_colors). Pre-2.80, params >256 and components >16 acted like
 *   wildcards (see match_version_fix).
 */
static inline boolean match_thing(enum thing d_id, int dcolor, int dparam,
 enum thing match_id, int match_fg, int match_bg, int match_param)
{
  if((d_id == match_id) &&
   (((dcolor & 0x0F) == match_fg) || (match_fg == 16)) &&
   (((dcolor >> 4) == match_bg) || (match_bg == 16)) &&
   ((dparam == match_param) || (match_param == 256)))
  {
    return true;
  }

  return false;
}

/* The colors here can also be <0 or >16 (see split_colors). */
static inline boolean match_overlay_color(int d_color, int fg, int bg)
{
  if(((bg == 16) || (bg == (d_color >> 4))) &&
   ((fg == 16) || (fg == (d_color & 0x0F))))
    return true;

  return false;
}

static int check_at_xy(struct board *src_board, enum thing id, int fg, int bg,
 int param, int offset)
{
  enum thing d_id = (enum thing)src_board->level_id[offset];
  int dcolor = src_board->level_color[offset];
  int dparam = src_board->level_param[offset];

  // Whirlpool matches down
  if(is_whirlpool(d_id))
    d_id = WHIRLPOOL_1;

  return match_thing(d_id, dcolor, dparam, id, fg, bg, param);
}

static int check_under_xy(struct board *src_board, enum thing id,
 int fg, int bg, int param, int offset)
{
  enum thing did = (enum thing)src_board->level_under_id[offset];
  int dcolor = src_board->level_under_color[offset];
  int dparam = src_board->level_under_param[offset];

  return match_thing(did, dcolor, dparam, id, fg, bg, param);
}

static int check_dir_xy(struct world *mzx_world, enum thing id, int color,
 int param, int x, int y, enum dir direction, struct robot *cur_robot,
 int *_bl)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int fg, bg;
  int offset;

  split_colors(color, &fg, &bg);
  match_version_fix(mzx_world, &fg, &bg, &param);
  // TODO: fix whirlpool id

  // Check under
  if(direction == BENEATH)
  {
    offset = x + (y * board_width);
    if(check_under_xy(src_board, id, fg, bg, param, offset))
      return 1;

    return 0;
  }
  else
  {
    int new_x = x;
    int new_y = y;

    if((direction & 0x1F) == SEEK)
    {
      get_robot_position(cur_robot, &x, &y);
    }

    direction = parsedir(direction, x, y, cur_robot->walk_dir);

    if(!move_dir(src_board, &new_x, &new_y, direction))
    {
      offset = new_x + (new_y * board_width);

      if(check_at_xy(src_board, id, fg, bg, param, offset))
        return 1;
    }
    return 0;
  }
}

static void copy_xy_to_xy(struct world *mzx_world, int src_x, int src_y,
 int dest_x, int dest_y)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int src_offset = src_x + (src_y * board_width);
  int dest_offset = dest_x + (dest_y * board_width);
  enum thing src_id = (enum thing)src_board->level_id[src_offset];
  enum thing dest_id = (enum thing)src_board->level_id[dest_offset];

  // Cannot copy from the player or onto the player. Check for the player at
  // the destination now so this doesn't pointlessly waste storage object IDs.
  if((src_id != PLAYER) && (dest_id != PLAYER))
  {
    int src_param = src_board->level_param[src_offset];

    if(is_robot(src_id))
    {
      struct robot *src_robot = src_board->robot_list[src_param];
      src_param = duplicate_robot(mzx_world, src_board, src_robot,
       dest_x, dest_y, 1);
    }
    else

    if(is_signscroll(src_id))
    {
      struct scroll *src_scroll = src_board->scroll_list[src_param];
      src_param = duplicate_scroll(src_board, src_scroll);
    }
    else

    if(src_id == SENSOR)
    {
      struct sensor *src_sensor = src_board->sensor_list[src_param];
      src_param = duplicate_sensor(src_board, src_sensor);
    }

    // Now perform the copy; this should perform any necessary
    // deletions at the destination as well.
    if(src_param != -1)
    {
      place_at_xy(mzx_world, src_id,
       src_board->level_color[src_offset], src_param, dest_x, dest_y);
    }
  }
}

void setup_overlay(struct board *src_board, int mode)
{
  if(!mode && src_board->overlay_mode)
  {
    // Deallocate overlay
    free(src_board->overlay);
    free(src_board->overlay_color);
  }

  if(!src_board->overlay_mode && mode)
  {
    int board_size = src_board->board_width * src_board->board_height;
    // Allocate an overlay
    src_board->overlay = cmalloc(board_size);
    src_board->overlay_color = cmalloc(board_size);
    memset(src_board->overlay, 32, board_size);
    memset(src_board->overlay_color, 7, board_size);
  }
  src_board->overlay_mode = mode;
}

static void copy_block(struct world *mzx_world, int id, int x, int y,
 int src_type, int dest_type, int src_x, int src_y, int width, int height,
 int dest_x, int dest_y, char *dest, int dest_param,
 char dest_name_buffer[ROBOT_MAX_TR])
{
  struct board *src_board = mzx_world->current_board;
  int src_width = src_board->board_width;
  int src_height = src_board->board_height;
  int dest_width = src_board->board_width;
  int dest_height = src_board->board_height;
  int src_offset;
  int dest_offset;

  // Process for source type and handle prefixing
  switch(src_type)
  {
    case 2:
    {
      src_width = mzx_world->vlayer_width;
      src_height = mzx_world->vlayer_height;
      break;
    }
    case 1:
    {
      if(!src_board->overlay_mode)
        setup_overlay(src_board, 3);
    }
  }

  prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
   src_width, src_height);

  // Clip and verify; the prefixer already handled the
  // base coordinates, so just handle the width/height
  if(width < 1)
    width = 1;
  if(height < 1)
    height = 1;
  if((src_x + width) > src_width)
    width = src_width - src_x;
  if((src_y + height) > src_height)
    height = src_height - src_y;

  // Get the offset that will generally be used.
  src_offset = src_x + (src_y * src_width);

  // Process for dest type and handle prefixing if we aren't already done
  switch(dest_type)
  {
    //string - dest_param is the terminator
    case 3:
    {
      switch(src_type)
      {
        case 0:
        {
          // Board to string
          load_string_board(mzx_world, dest_name_buffer,
           src_board->level_param + src_offset, src_width,
           width, height, dest_param);
          break;
        }
        case 1:
        {
          // Overlay to string
          load_string_board(mzx_world, dest_name_buffer,
           src_board->overlay + src_offset, src_width,
           width, height, dest_param);
          break;
        }
        case 2:
        {
          // Vlayer to string
          load_string_board(mzx_world, dest_name_buffer,
           mzx_world->vlayer_chars + src_offset, src_width,
           width, height, dest_param);
          break;
        }
      }
      return;
    }
    //mzm - if dest_param is !=0, the board is saved like a layer.
    case 4:
    {
      char *translated_name = cmalloc(MAX_PATH);
      int err;

      switch(src_type)
      {
        case 0:
          if(dest_param)
            src_type = MZM_BOARD_TO_LAYER_STORAGE;
          else
            src_type = MZM_BOARD_TO_BOARD_STORAGE;
          break;

        case 1:
          src_type = MZM_OVERLAY_TO_LAYER_STORAGE;
          break;

        case 2:
          src_type = MZM_VLAYER_TO_LAYER_STORAGE;
          break;
      }

      // Save MZM to string (2.90+)
      if(mzx_world->version >= V290 && is_string(dest_name_buffer))
      {
        save_mzm_string(mzx_world, dest_name_buffer, src_x, src_y,
         width, height, src_type, 1, id);
      }

      // Save MZM to file
      else
      {
        err = fsafetranslate(dest_name_buffer, translated_name, MAX_PATH);
        if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
        {
          save_mzm(mzx_world, translated_name, src_x, src_y,
          width, height, src_type, 1);
        }
      }
      free(translated_name);
      return;
    }
    case 2:
    {
      dest_width = mzx_world->vlayer_width;
      dest_height = mzx_world->vlayer_height;
      break;
    }
    case 1:
    {
      if(!src_board->overlay_mode)
        setup_overlay(src_board, 3);
    }
  }

  prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
   dest_width, dest_height);

  // Clip to destination dimensions as well
  if((dest_x + width) > dest_width)
    width = dest_width - dest_x;
  if((dest_y + height) > dest_height)
    height = dest_height - dest_y;

  // Get the offset that will generally be used.
  dest_offset = dest_x + (dest_y * dest_width);

  switch((dest_type << 2) | (src_type))
  {
    // Board to board
    case 0:
    {
      copy_board_to_board(mzx_world,
       src_board, src_offset,
       src_board, dest_offset,
       width, height);

      break;
    }
    // Overlay to board
    case 1:
    {
      copy_layer_to_board(
       src_board->overlay, src_board->overlay_color, src_width, src_offset,
       src_board, dest_offset,
       width, height, CUSTOM_BLOCK);

      break;
    }
    // Vlayer to board
    case 2:
    {
      copy_layer_to_board(
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, src_width, src_offset,
       src_board, dest_offset,
       width, height, CUSTOM_BLOCK);

      break;
    }

    // Board to overlay
    case 4:
    {
      copy_board_to_layer(
       src_board, src_offset,
       src_board->overlay, src_board->overlay_color, dest_width, dest_offset,
       width, height);

      break;
    }
    // Overlay to overlay
    case 5:
    {
      copy_layer_to_layer(
       src_board->overlay, src_board->overlay_color, src_width, src_offset,
       src_board->overlay, src_board->overlay_color, dest_width, dest_offset,
       width, height);

      break;
    }
    // Vlayer to overlay
    case 6:
    {
      copy_layer_to_layer(
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, src_width, src_offset,
       src_board->overlay, src_board->overlay_color, dest_width, dest_offset,
       width, height);

      break;
    }

    // Board to vlayer
    case 8:
    {
      copy_board_to_layer(
       src_board, src_offset,
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, dest_width, dest_offset,
       width, height);

      break;
    }
    // Overlay to vlayer
    case 9:
    {
      copy_layer_to_layer(
       src_board->overlay, src_board->overlay_color, src_width, src_offset,
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, dest_width, dest_offset,
       width, height);

      break;
    }
    // Vlayer to vlayer
    case 10:
    {
      copy_layer_to_layer(
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, src_width, src_offset,
       mzx_world->vlayer_chars, mzx_world->vlayer_colors, dest_width, dest_offset,
       width, height);

      break;
    }
  }
}

static void copy_xy_to_xy_wrapper(struct world *mzx_world, int id, int x, int y,
 int src_type, int dest_type, int src_x, int src_y, int dest_x, int dest_y)
{
  struct board *cur_board = mzx_world->current_board;
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  int vlayer_width = mzx_world->vlayer_width;
  int vlayer_height = mzx_world->vlayer_height;
  int src_offset;
  int dest_offset;

  switch(src_type | (dest_type << 4))
  {
    // Board-to-board. Use the original implementation...
    case 0x00:
      prefix_first_last_xy(mzx_world, &src_x, &src_y, &dest_x, &dest_y, x, y);
      copy_xy_to_xy(mzx_world, src_x, src_y, dest_x, dest_y);
      break;

    // Overlay-to-overlay.
    case 0x11:
      if(!cur_board->overlay_mode)
        setup_overlay(cur_board, 3);

      prefix_first_last_xy(mzx_world, &src_x, &src_y, &dest_x, &dest_y, x, y);
      src_offset = src_y * board_width + src_x;
      dest_offset = dest_y * board_width + dest_x;
      cur_board->overlay[dest_offset] = cur_board->overlay[src_offset];
      cur_board->overlay_color[dest_offset] = cur_board->overlay_color[src_offset];
      break;

    // Vlayer-to-overlay.
    case 0x12:
      if(!cur_board->overlay_mode)
        setup_overlay(cur_board, 3);

      prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
       vlayer_width, vlayer_height);
      prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
       board_width, board_height);

      src_offset = src_y * vlayer_width + src_x;
      dest_offset = dest_y * board_width + dest_x;
      cur_board->overlay[dest_offset] = mzx_world->vlayer_chars[src_offset];
      cur_board->overlay_color[dest_offset] = mzx_world->vlayer_colors[src_offset];
      break;

    // Overlay-to-vlayer.
    case 0x21:
      if(!cur_board->overlay_mode)
        setup_overlay(cur_board, 3);

      prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
       board_width, board_height);
      prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
       vlayer_width, vlayer_height);

      src_offset = src_y * board_width + src_x;
      dest_offset = dest_y * vlayer_width + dest_x;
      mzx_world->vlayer_chars[dest_offset] = cur_board->overlay[src_offset];
      mzx_world->vlayer_colors[dest_offset] = cur_board->overlay_color[src_offset];
      break;

    // Vlayer-to-vlayer.
    case 0x22:
      prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
       vlayer_width, vlayer_height);
      prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
       vlayer_width, vlayer_height);

      src_offset = src_y * vlayer_width + src_x;
      dest_offset = dest_y * vlayer_width + dest_x;
      mzx_world->vlayer_chars[dest_offset] = mzx_world->vlayer_chars[src_offset];
      mzx_world->vlayer_colors[dest_offset] = mzx_world->vlayer_colors[src_offset];
      break;

    // All others should just use copy_block.
    case 0x01:
    case 0x02:
    case 0x10:
    case 0x20:
      copy_block(mzx_world, id, x, y, src_type, dest_type, src_x, src_y, 1, 1,
       dest_x, dest_y, NULL, 0, NULL);
      break;
  }
}

// Gets the type for a single copy block param.
static int copy_block_param(struct world *mzx_world, int id, char *param,
 int *coord)
{
  int type = 0;
  if(*param)
  {
    if(*(param + 1) == '+')
    {
      type = 1;
    }
    else

    if(*(param + 1) == '#')
    {
      type = 2;
    }
  }

  if(type == 0)
  {
    *coord = parse_param(mzx_world, param, id);
  }
  else

  if((type == 1) || (type == 2))
  {
    char src_char_buffer[ROBOT_MAX_TR];
    tr_msg(mzx_world, param + 2, id, src_char_buffer);
    *coord = strtol(src_char_buffer, NULL, 10);
  }
  return type;
}

// Gets the type for the X coord of the copy block destination.
// This argument has two special cases: it can be a $string (after tr_msg) or
// an MZM (always prefixed with @). These cases need to also return the name
// of their destination string/MZM.
static int copy_block_param_special(struct world *mzx_world, int id,
 char *param, int *coord, char dest_name_buffer[ROBOT_MAX_TR])
{
  char first = *(param + 1);
  if(!(*param) || first == '(' || first == '+' || first == '#')
    return copy_block_param(mzx_world, id, param, coord);

  if(first == '@')
  {
    tr_msg(mzx_world, param + 2, id, dest_name_buffer);
    return 4;
  }

  tr_msg(mzx_world, param + 1, id, dest_name_buffer);
  if(is_string(dest_name_buffer))
    return 3;

  *coord = get_counter(mzx_world, dest_name_buffer, id);
  return 0;
}

void replace_player(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int dx, dy, offset;

  for(dy = 0, offset = 0; dy < board_height; dy++)
  {
    for(dx = 0; dx < board_width; dx++, offset++)
    {
      if(A_UNDER & flags[(int)level_id[offset]])
      {
        // Place the player here
        mzx_world->player_x = dx;
        mzx_world->player_y = dy;
        id_place(mzx_world, dx, dy, PLAYER, 0, 0);
        return;
      }
    }
  }

  // Place the player here
  mzx_world->player_x = 0;
  mzx_world->player_y = 0;
  place_at_xy(mzx_world, PLAYER, 0, 0, 0, 0);
}

// Returns the numeric value pointed to OR the numeric value represented
// by the counter string pointed to. (the ptr is at the param within the
// command)
// Sign extends the result, for now...

int parse_param(struct world *mzx_world, char *program, int id)
{
  char ibuff[ROBOT_MAX_TR];

  if(program[0] == 0)
  {
    // Numeric
    return (signed short)((int)program[1] | (int)(program[2] << 8));
  }

  // Expressions - Exo
  if((program[1] == '(') && mzx_world->version >= V268)
  {
    char *e_ptr = program + 2;
    int val, error;

    val = parse_expression(mzx_world, &e_ptr, &error, id);
    if(!error && !(*e_ptr))
      return val;
  }

  tr_msg(mzx_world, program + 1, id, ibuff);

  return get_counter(mzx_world, ibuff, id);
}

// Check for the cases of parse_param that treat the param as a name.
// Use this if tr_msg is required to check for a string in a place where an
// expression is valid (currently only the IF and COPY BLOCK $string commands).
static boolean is_name_param(struct world *mzx_world, char *program)
{
  return (program[0] != 0) &&
  ((program[1] != '(') || mzx_world->version < V268);
}

// These will always return numeric values
enum thing parse_param_thing(struct world *mzx_world, char *program)
{
  return (enum thing)
   ((int)program[1] | (int)(program[2] << 8));
}

enum dir parse_param_dir(struct world *mzx_world, char *program)
{
  return (enum dir)
   ((int)program[1] | (int)(program[2] << 8));
}

enum equality parse_param_eq(struct world *mzx_world, char *program)
{
  return (enum equality)
   ((int)program[1] | (int)(program[2] << 8));
}

enum condition parse_param_cond(struct world *mzx_world, char *program,
 enum dir *direction)
{
  *direction = (enum dir)program[2];
  return (enum condition)program[1];
}

// Returns location of next parameter (pos is loc of current parameter)
int next_param(char *ptr, int pos)
{
  if(ptr[pos])
  {
    return ptr[pos] + 1;
  }
  else
  {
    return 3;
  }
}

char *next_param_pos(char *ptr)
{
  int index = *ptr;
  if(index)
  {
    return ptr + index + 1;
  }
  else
  {
    return ptr + 3;
  }
}

static void advance_line(struct robot *cur_robot, char *program)
{
  cur_robot->cur_prog_line += program[cur_robot->cur_prog_line] + 2;
  cur_robot->pos_within_line = 0;
}

static void end_cycle(struct world *mzx_world, struct robot *cur_robot,
 int lines_run, int x, int y)
{
#ifdef CONFIG_EDITOR
  cur_robot->commands_total += lines_run;
  cur_robot->commands_cycle = lines_run;
#endif

  cur_robot->cycle_count = 0; // In case a label changed it

  // Older versions have a really sloppy method of updating the robot pos
  // that causes a lot of bugs, and sadly this needs to be emulated.
  cur_robot->compat_xpos = x;
  cur_robot->compat_ypos = y;

  // Send commands in 1.x always set the label sent flag, but this
  // robot already received whatever label it sent itself, so clear it.
  if(mzx_world->version < V200)
    cur_robot->status = 0;
}

static void end_program(struct robot *cur_robot)
{
  cur_robot->cur_prog_line = 0;
  cur_robot->pos_within_line = 0;
}

#define ADVANCE_LINE do{ advance_line(cur_robot, program); }while(0);

// NOTE: Should always return after using one of these.
#define END_CYCLE do{ end_cycle(mzx_world, cur_robot, lines_run, x, y); }while(0);
#define END_PROGRAM do{ END_CYCLE; end_program(cur_robot); }while(0);

// Run a single robot through a single cycle.
// If id is negative, only run it if status is 2
void run_robot(context *ctx, int id, int x, int y)
{
  struct world *mzx_world = ctx->world;
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot;
  int cmd; // Command to run
  int lines_run = 0;
  int gotoed;

  int old_pos; // Old position to verify gotos DID something
  int last_label = -1;
  // Whether blocked in a given direction (2 = OUR bullet)
  int _bl[4] = { 0, 0, 0, 0 };
  char *program;
  char *cmd_ptr;
  char done = 0;
  char update_blocked = 0;
  char first_cmd = 1;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if((id < 0) && ((src_board->robot_list[-id])->status != 2))
    return;

  // Reset global prefixes
  mzx_world->first_prefix = 0;
  mzx_world->mid_prefix = 0;
  mzx_world->last_prefix = 0;
  mzx_world->command_cache = 0;

  if(id < 0)
  {
    id = -id;
    cur_robot = src_board->robot_list[id];
    cur_robot->xpos = x;
    cur_robot->ypos = y;
    cur_robot->compat_xpos = x;
    cur_robot->compat_ypos = y;
    cur_robot->cycle_count = 0;
    cur_robot->status = 0;
  }
  else
  {
    enum dir walk_dir;
    cur_robot = src_board->robot_list[id];

    walk_dir = cur_robot->walk_dir;

    // Reset x/y
    cur_robot->xpos = x;
    cur_robot->ypos = y;
    cur_robot->compat_xpos = x;
    cur_robot->compat_ypos = y;
    cur_robot->status = 0; // Clear this here too for 1.x compat.

#ifdef CONFIG_EDITOR
    cur_robot->commands_cycle = 0;
    cur_robot->commands_caught = 0;
#endif

    // Update cycle count
    cur_robot->cycle_count++;
    if(cur_robot->cycle_count < cur_robot->robot_cycle)
    {
      cur_robot->status = 1;
      return;
    }

    cur_robot->cycle_count = 0;

#ifdef CONFIG_DEBYTECODE
    prepare_robot_bytecode(mzx_world, cur_robot);
#endif

    // Does program exist?
    if(cur_robot->program_bytecode_length < 3)
    {
      return; // (nope)
    }

    // Walk?
    if(id && is_cardinal_dir(walk_dir))
    {
      enum move_status status = move(mzx_world, x, y, dir_to_int(walk_dir),
       CAN_PUSH | CAN_TRANSPORT | CAN_FIREWALK | CAN_WATERWALK |
       CAN_LAVAWALK * cur_robot->can_lavawalk |
       (cur_robot->can_goopwalk ? CAN_GOOPWALK : 0));

      if(status == HIT_EDGE)
      {
        // Send to edge, if no response, then to thud.
        if(send_robot_id_def(mzx_world, id, "edge", 1))
          send_robot_id_def(mzx_world, id, "thud", 1);
      }
      else if(status == NO_HIT)
      {
        enum thing l_id;

        if(mzx_world->version >= V292)
        {
          // Source the current position off of the robot's new real position.
          x = cur_robot->xpos;
          y = cur_robot->ypos;
        }
        else
        {
          // Source the current position off of a hack that doesn't really work.
          move_dir(src_board, &x, &y, walk_dir);
        }

        /* Normally, WALK doesn't end the cycle. But due to long-standing
         * bugs in the transport() and push() functions, the board is actually
         * updated without updating the x,y co-ordinates. Presumably, move_dir
         * was designed to get things "back in sync". Unfortunately, because
         * robots can enter transports, they may not be in the x,y location
         * that move_dir recomputes. Because there is no easy way to get the
         * updated x,y location of the robot for this cycle, we simply end the
         * cycle if the robot enters a transport.
         */
        l_id = (enum thing)level_id[x + (y * board_width)];
        if(l_id == TRANSPORT)
        {
          END_CYCLE;
          return;
        }
      }
      else
        send_robot_id_def(mzx_world, id, "thud", 1);
    }

    if(cur_robot->cur_prog_line == 0)
    {
      // Robot is inactive
      cur_robot->status = 1;
      END_CYCLE;
      return;
    }
  }

  // Get program location
  program = cur_robot->program_bytecode;

  // NOW quit if inactive (had to do walk first)
  if(cur_robot->cur_prog_line == 0)
  {
    cur_robot->status = 1;
    END_PROGRAM;
    return;
  }

  // Figure blocked vars (accurate until robot program ends OR a put
  // or change command is issued)

  calculate_blocked(mzx_world, x, y, id, _bl);

  // Update player x/y if necessary
  find_player(mzx_world);

  // Main robot loop

  do
  {
    gotoed = 0;
    old_pos = cur_robot->cur_prog_line;

    // Get ptr to command
    cmd_ptr = program + old_pos + 1;

    // Get command number
    cmd = cmd_ptr[0];

#ifdef CONFIG_EDITOR
    // Check to see if the current command triggers a breakpoint.
    if(mzx_world->editing && debug_robot_break)
    {
      switch(debug_robot_break(ctx, cur_robot, id, lines_run))
      {
        case DEBUG_EXIT:
          break;

        case DEBUG_GOTO:
          // Restart the loop on the new destination command.
          // Don't count the goto as a command.
          lines_run--;
          continue;

        case DEBUG_HALT:
          END_CYCLE;
          return;
      }
    }
#endif

    // Act according to command
    switch(cmd)
    {
      case ROBOTIC_CMD_END: // End
      {
        if(first_cmd)
          cur_robot->status = 1;

        END_PROGRAM;
        return;
      }

      case ROBOTIC_CMD_DIE: // Die
      {
        if(id)
        {
          clear_robot_id(src_board, id);
          id_remove_top(mzx_world, x, y);
        }
        // Robot no longer exists; exit.
        return;
      }

      case ROBOTIC_CMD_DIE_ITEM: // Die item
      {
        if(id)
        {
          clear_robot_id(src_board, id);
          id_remove_top(mzx_world, x, y);
          place_player_xy(mzx_world, x, y);
        }
        // Robot no longer exists; exit.
        return;
      }

      case ROBOTIC_CMD_WAIT: // Wait
      {
        int wait_time = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        if(wait_time <= cur_robot->pos_within_line)
          break;

        cur_robot->pos_within_line++;
        if(first_cmd)
          cur_robot->status = 1;

        END_CYCLE;
        return;
      }

      case ROBOTIC_CMD_CYCLE: // Cycle
      {
        cur_robot->robot_cycle = parse_param(mzx_world, cmd_ptr + 1,id);
        done = 1;
        break;
      }

      case ROBOTIC_CMD_GO: // Go dir num
      {
        if(id)
        {
          enum dir direction =
           parsedir((enum dir)cmd_ptr[2], x, y, cur_robot->walk_dir);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int num = parse_param(mzx_world, p2, id);

          // Inc. pos. or break
          if(num > cur_robot->pos_within_line)
          {
            cur_robot->pos_within_line++;

            // Parse dir
            if(is_cardinal_dir(direction))
            {
              enum move_status status;

              status = move(mzx_world, x, y, dir_to_int(direction),
               CAN_PUSH | CAN_TRANSPORT | CAN_FIREWALK | CAN_WATERWALK |
               CAN_LAVAWALK * cur_robot->can_lavawalk |
               (cur_robot->can_goopwalk ? CAN_GOOPWALK : 0));

              if(status == NO_HIT)
              {
                move_dir(src_board, &x, &y, direction);
              }
            }
            END_CYCLE;
            return;
          }
        }
        break;
      }

      case ROBOTIC_CMD_TRY_DIR: // Try dir label
      {
        if(id)
        {
          // Parse dir
          enum dir direction = parsedir((enum dir)cmd_ptr[2], x, y,
           cur_robot->walk_dir);

          if(is_cardinal_dir(direction))
          {
            enum move_status status;

            status = move(mzx_world, x, y, dir_to_int(direction),
             CAN_PUSH | CAN_TRANSPORT | CAN_FIREWALK | CAN_WATERWALK |
             CAN_LAVAWALK * cur_robot->can_lavawalk |
             (cur_robot->can_goopwalk ? CAN_GOOPWALK : 0));

            if(status)
            {
              // blocked- send to label
              char *p2 = next_param_pos(cmd_ptr + 1);
              gotoed = send_self_label_tr(mzx_world,  p2 + 1, id);
            }
            else
            {
              move_dir(src_board, &x, &y, direction);
              // not blocked- make sure only moves once!
              done = 1;
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_WALK: // Walk dir
      {
        if(id)
        {
          enum dir direction =
           parsedir((enum dir)cmd_ptr[2], x, y, cur_robot->walk_dir);

          if(!is_cardinal_dir(direction))
          {
            cur_robot->walk_dir = IDLE;
          }
          else
          {
            cur_robot->walk_dir = direction;
          }
        }
        break;
      }

      case ROBOTIC_CMD_BECOME: // Become color thing param
      {
        if(id)
        {
          int offset = x + (y * board_width);
          int color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum thing new_id = parse_param_thing(mzx_world, p2);
          int param = parse_param(mzx_world, p2 + 3, id);
          color = fix_color(color, level_color[offset]);
          level_color[offset] = color;
          level_id[offset] = new_id;

          // Param- Only change if not becoming a robot
          if(!is_robot(new_id))
          {
            level_param[offset] = param;
            clear_robot_id(src_board, id);

            // If became a scroll, sensor, or sign...
            if((new_id >= SENSOR) && (new_id <= PLAYER))
              id_remove_top(mzx_world, x, y);

            // Delete "under"? (if became another type of "under")
            if(flags[new_id] & A_UNDER)
            {
              // Became an under, so delete under.
              src_board->level_under_param[offset] = SPACE;
              src_board->level_under_id[offset] = 0;
              src_board->level_under_color[offset] = 7;
            }

            // Robot no longer exists; exit
            return;
          }
        }

        // Became a robot.
        break;
      }

      case ROBOTIC_CMD_CHAR: // Char
      {
        cur_robot->robot_char = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_COLOR: // Color
      {
        if(id)
        {
          int offset = x + (y * board_width);
          level_color[offset] =
           fix_color(parse_param(mzx_world, cmd_ptr + 1, id),
           level_color[offset]);
        }
        break;
      }

      case ROBOTIC_CMD_GOTOXY: // Gotoxy
      {
        if(id)
        {
          int new_x = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int new_y = parse_param(mzx_world, p2, id);
          prefix_mid_xy(mzx_world, &new_x, &new_y, x, y);

          if((new_x != x) || (new_y != y))
          {
            // delete at x y
            int offset = x + (y * board_width);
            enum thing new_id = (enum thing)level_id[offset];
            int color = level_color[offset];
            if(place_at_xy(mzx_world, new_id, color, id, new_x, new_y))
            {
              id_remove_top(mzx_world, x, y);
              cur_robot->xpos = new_x;
              cur_robot->ypos = new_y;
              x = new_x;
              y = new_y;
            }
          }
          done = 1;
        }
        break;
      }

      case ROBOTIC_CMD_SET: // set counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char src_buffer[ROBOT_MAX_TR];
        char dest_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        // Setting a string
        if(is_string(dest_buffer))
        {
          struct string dest;

          // Is it a non-immediate
          if(*src_string)
          {
            // Translate into src buffer
            tr_msg(mzx_world, src_string + 1, id, src_buffer);

            if(is_string(src_buffer))
            {
              // Is it another string?
              if(!get_string(mzx_world, src_buffer, &dest, id))
              {
                dest.value = src_buffer;
                src_buffer[0] = '\0';
              }
            }
            else
            {
              dest.value = src_buffer;
              dest.length = strlen(src_buffer);
            }
          }
          else
          {
            // Set it to immediate representation
            size_t tmp;
            dest.value = tr_int_to_string(src_buffer,
             parse_param(mzx_world, src_string, id), &tmp);
            dest.length = tmp;
          }

          gotoed = set_string(mzx_world, dest_buffer, &dest, id);

          // Loading source/robots from strings might have changed these
          if(gotoed)
          {
            program = cur_robot->program_bytecode;
            cmd_ptr = program + cur_robot->cur_prog_line;
          }
        }
        else
        {
          // Set to counter
          int value;
          mzx_world->special_counter_return = FOPEN_NONE;
          value = parse_param(mzx_world, src_string, id);

          if(mzx_world->special_counter_return != FOPEN_NONE)
          {
            gotoed = set_counter_special(mzx_world, dest_buffer, value, id);

            // On a game state change, we need to return to the main game loop.
            if(mzx_world->change_game_state)
              return;

            // Some specials might have changed these
#ifdef CONFIG_DEBYTECODE
            prepare_robot_bytecode(mzx_world, cur_robot);
#endif
            program = cur_robot->program_bytecode;
            cmd_ptr = program + cur_robot->cur_prog_line;

            // Prior to 2.90, SAVE_GAME works immediately and requires the
            // robot's cycle to be ended for it to safely work. After 2.90
            // SAVE_GAME takes place at the end of the cycle, so this is
            // no longer necessary.
            if(mzx_world->special_counter_return == FOPEN_SAVE_GAME)
            {
              if(mzx_world->version < V290)
              {
                if(!program[cur_robot->cur_prog_line])
                  cur_robot->cur_prog_line = 0;

                END_CYCLE;
                return;
              }
            }
          }
          else
          {
            set_counter(mzx_world, dest_buffer, value, id);
          }
        }
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_INC: // inc counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char src_buffer[ROBOT_MAX_TR];
        char dest_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        // Incrementing a string
        if(is_string(dest_buffer))
        {
          // Must be a non-immediate
          if(*src_string)
          {
            struct string dest;
            // Translate into src buffer
            tr_msg(mzx_world, src_string + 1, id, src_buffer);

            if(is_string(src_buffer))
            {
              // Is it another string? Grab it
              if(!get_string(mzx_world, src_buffer, &dest, id))
                break;
            }
            else
            {
              dest.value = src_buffer;
              dest.length = strlen(src_buffer);
            }
            // Set it
            inc_string(mzx_world, dest_buffer, &dest, id);
          }
        }
        else
        {
          // Set to counter
          int value = parse_param(mzx_world, src_string, id);
          inc_counter(mzx_world, dest_buffer, value, id);
        }
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_DEC: // dec counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char dest_buffer[ROBOT_MAX_TR];
        int value;

        tr_msg(mzx_world, dest_string, id, dest_buffer);
        value = parse_param(mzx_world, src_string, id);

        // Decrementing a string
        if(is_string(dest_buffer))
        {
          // Set it to immediate representation
          dec_string_int(mzx_world, dest_buffer, value, id);
        }
        else
        {
          // Set to counter
          dec_counter(mzx_world, dest_buffer, value, id);
        }
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_IF: // if c?n l
      {
        int dest_value = 0, src_value = 0;
        char *dest_string = cmd_ptr + 1;
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *src_string = p2 + 3;
        enum equality comparison = parse_param_eq(mzx_world, p2);
        char src_buffer[ROBOT_MAX_TR];
        char dest_buffer[ROBOT_MAX_TR];
        int success = 0;
        boolean has_dest_buffer = false;

        // NOTE: versions prior to 2.92 never did this before is_string.
        if(is_name_param(mzx_world, dest_string))
        {
          tr_msg(mzx_world, dest_string + 1, id, dest_buffer);
          has_dest_buffer = true;
        }

        if(has_dest_buffer && is_string(dest_buffer))
        {
          struct string dest;
          struct string src;

          // NOTE: versions prior to 2.92 did tr_msg here instead of above.

          // Get a pointer to the dest string
          get_string(mzx_world, dest_buffer, &dest, id);

          // Is the second argument immediate?
          if(*src_string)
          {
            tr_msg(mzx_world, src_string + 1, id, src_buffer);
            if(is_string(src_buffer))
            {
              get_string(mzx_world, src_buffer, &src, id);
            }
            else
            {
              src.value = src_buffer;
              src.length = strlen(src_buffer);
            }
          }
          else
          {
            size_t tmp;
            src.value = tr_int_to_string(src_buffer,
             parse_param(mzx_world, src_string, id), &tmp);
            src.length = tmp;
          }

          // Non-terminated string compares (2.81+)
          if(mzx_world->version >= V281)
          {
            boolean exact_case = false;
            boolean wildcards = false;

            // String equality extensions (2.91+)
            if(mzx_world->version >= V291)
            {
              if(comparison == EXACTLY_EQUAL || comparison == WILD_EXACTLY_EQUAL)
                exact_case = true;

              if(comparison == WILD_EQUAL || comparison == WILD_EXACTLY_EQUAL)
                wildcards = true;
            }

            src_value = 0;
            dest_value = compare_strings(&dest, &src, exact_case, wildcards);
          }
          // Null-terminated string compares (2.80X and prior).
          else
          {
            src_value = 0;
            dest_value = compare_strings_null_terminated(&dest, &src);
          }
        }
        else

        if(has_dest_buffer)
        {
          dest_value = get_counter(mzx_world, dest_buffer, id);
          src_value = parse_param(mzx_world, src_string, id);
        }

        else
        {
          dest_value = parse_param(mzx_world, dest_string, id);
          src_value = parse_param(mzx_world, src_string, id);
        }

        if(mzx_world->version < V290)
        {
          // In port releases prior to 2.90b there was a horrible
          // bug stopping comparisons between numbers with a
          // difference greater than 2^31-1.
          // To our great regret we must support this functionality
          // for older worlds.
          dest_value = dest_value - src_value;
          src_value = 0;
        }

        switch(comparison)
        {
          case EQUAL:
          case EXACTLY_EQUAL:
          case WILD_EQUAL:
          case WILD_EXACTLY_EQUAL:
          {
            if(dest_value == src_value)
              success = 1;
            break;
          }

          case LESS_THAN:
          {
            if(dest_value < src_value)
              success = 1;
            break;
          }

          case GREATER_THAN:
          {
            if(dest_value > src_value)
              success = 1;
            break;
          }

          case GREATER_THAN_OR_EQUAL:
          {
            if(dest_value >= src_value)
              success = 1;
            break;
          }

          case LESS_THAN_OR_EQUAL:
          {
            if(dest_value <= src_value)
              success = 1;
            break;
          }

          case NOT_EQUAL:
          {
            if(dest_value != src_value)
              success = 1;
            break;
          }
        }

        if(success)
        {
          char *p3 = next_param_pos(p2 + 3);
          gotoed = send_self_label_tr(mzx_world,  p3 + 1, id);
        }

        break;
      }

      case ROBOTIC_CMD_IF_CONDITION: // if condition label
      case ROBOTIC_CMD_IF_NOT_CONDITION: // if not cond. label
      {
        enum dir direction;
        enum condition condition =
         parse_param_cond(mzx_world, cmd_ptr + 1, &direction);
        int success = 0;
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        switch(condition)
        {
          case WALKING:
          {
            if(direction < RANDNS)
            {
              if(cur_robot->walk_dir == direction)
                success = 1;
              break;
            }
            else

            if(direction == NODIR)
            {
              if(cur_robot->walk_dir == 0)
                success = 1;
            }
            else

            // assumed anydir
            if(cur_robot->walk_dir != 0)
              success = 1;
            break;
          }

          case SWIMMING:
          {
            if(id)
            {
              int offset = x + (y * board_width);
              int under = level_under_id[offset];

              if(is_water(under))
                success = 1;
            }
            break;
          }

          case FIRE_WALKING:
          {
            if(id)
            {
              int offset = x + (y * board_width);
              enum thing under = (enum thing)level_under_id[offset];

              if((under == LAVA) || (under == FIRE))
                success = 1;
            }
            break;
          }

          case TOUCHING:
          {
            if(id)
            {
              int new_x;
              int new_y;

              if(is_cardinal_dir(direction))
              {
                new_x = x;
                new_y = y;
                if(!move_dir(src_board, &new_x, &new_y, direction))
                {
                  if((mzx_world->player_x == new_x) &&
                   (mzx_world->player_y == new_y))
                  {
                    success = 1;
                  }
                }
              }
              else
              {
                if(direction >= ANYDIR)
                {
                  int i;
                  // either anydir or nodir
                  // is player touching at all?

                  for(i = 0; i < 4; i++)
                  {
                    // try all dirs
                    new_x = x;
                    new_y = y;
                    if(!move_dir(src_board, &new_x, &new_y, int_to_dir(i)))
                    {
                      if((mzx_world->player_x == new_x) &&
                       (mzx_world->player_y == new_y))
                      {
                        success = 1;
                      }
                    }
                  }

                  // We want NODIR though, so reverse success
                  if(direction == NODIR)
                    success ^= 1;
                }
                else
                {
                  success = 0;
                }
              }
            }

            break;
          }

          case BLOCKED:
          {
            int new_bl[4];

            // If REL PLAYER or REL COUNTERS, use special code
            switch(mzx_world->mid_prefix)
            {
              case REL_TO_PLAYER:
              {
                // Give an ID of -1 to throw it off from not
                // allowing global robot.
                calculate_blocked(mzx_world, mzx_world->player_x,
                 mzx_world->player_y, -1, new_bl);
                update_blocked = 1;
                break;
              }

              case REL_TO_XPOS_YPOS:
              {
                // Give an ID of -1 to throw it off from not
                // allowing global robot.
                calculate_blocked(mzx_world,
                 get_counter(mzx_world, "XPOS", 0),
                 get_counter(mzx_world, "YPOS", 0), -1, new_bl);
                update_blocked = 1;
                break;
              }

              default:
              {
                // Use the blocked list that's already there
                memcpy(new_bl, _bl, sizeof(int) * 4);
              }
            }

            if(is_cardinal_dir(direction))
            {
              success = new_bl[dir_to_int(direction)];
            }
            else
            {
              // either anydir or nodir
              // is blocked any dir at all?
              success = new_bl[0] | new_bl[1] | new_bl[2] | new_bl[3];
              // success = 1 for anydir

              // We want NODIR though, so reverse success

              if((direction == NODIR) || (direction == IDLE))
                success ^= 1;
            }
            break;
          }

          case ALIGNED:
          {
            if(id)
            {
              if((mzx_world->player_x == x) || (mzx_world->player_y == y))
                success = 1;
            }
            break;
          }

          case ALIGNED_NS:
          {
            if(id)
            {
              if(mzx_world->player_x == x)
                success = 1;
            }
            break;
          }

          case ALIGNED_EW:
          {
            if(id)
            {
              if(mzx_world->player_y == y)
                success = 1;
            }
            break;
          }

          case LASTSHOT:
          {
            if(id)
            {
              if(is_cardinal_dir(direction))
              {
                if(direction == cur_robot->last_shot_dir)
                  success = 1;
              }
            }
            break;
          }

          case LASTTOUCH:
          {
            if(id)
            {
              direction = parsedir(direction, x, y,
               cur_robot->walk_dir);
              if(is_cardinal_dir(direction))
              {
                if(direction == cur_robot->last_touch_dir)
                  success = 1;
              }
            }
            break;
          }

          case RIGHTPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_RIGHT) > 0;
            break;
          }

          case LEFTPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_LEFT) > 0;
            break;
          }

          case UPPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_UP) > 0;
            break;
          }

          case DOWNPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_DOWN) > 0;
            break;
          }

          case SPACEPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_SPACE) > 0;
            break;
          }

          case DELPRESSED:
          {
            success =
             get_key_status(keycode_internal_wrt_numlock, IKEY_DELETE) > 0;
            break;
          }

          case MUSICON:
          {
            success = audio_get_music_on();
            break;
          }

          case SOUNDON:
          {
            success = audio_get_pcs_on();
            break;
          }
        }

        // Reverse truth if NOT is present
        if(cmd == ROBOTIC_CMD_IF_NOT_CONDITION)
          success ^= 1;

        if(success)
        {
          // jump
          gotoed = send_self_label_tr(mzx_world, cmd_ptr + 5, id);
        }
        break;
      }

      case ROBOTIC_CMD_IF_ANY: // if any thing label
      {
        // Get foreg/backg allowed in fb/bg
        int offset;
        int color = parse_param(mzx_world, cmd_ptr + 1, id);
        int fg, bg;
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing check_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        // Param
        int check_param = parse_param(mzx_world, p3, id);

        split_colors(color, &fg, &bg);
        match_version_fix(mzx_world, &fg, &bg, &check_param);
        // TODO: fix whirlpool id

        for(offset = 0; offset < (board_width * board_height); offset++)
        {
          if(check_at_xy(src_board, check_id, fg, bg, check_param, offset))
          {
            char *p4 = next_param_pos(p3);
            gotoed = send_self_label_tr(mzx_world,  p4 + 1, id);

            // The port up through 2.84 allowed this to iterate the entire board.
            if(mzx_world->version < VERSION_PORT || mzx_world->version > V284)
              break;
          }
        }

        break;
      }

      case ROBOTIC_CMD_IF_NO: // if no thing label
      {
        // Get foreg/backg allowed in fb/bg
        int offset;
        int color = parse_param(mzx_world, cmd_ptr + 1, id);
        int fg, bg;
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing check_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        // Param
        int check_param = parse_param(mzx_world, p3, id);

        split_colors(color, &fg, &bg);
        match_version_fix(mzx_world, &fg, &bg, &check_param);
        // TODO: fix whirlpool id

        for(offset = 0; offset < (board_width * board_height); offset++)
        {
          if(check_at_xy(src_board, check_id, fg, bg, check_param, offset))
            break;
        }

        if(offset == (board_width * board_height))
        {
          char *p4 = next_param_pos(p3);
          gotoed = send_self_label_tr(mzx_world,  p4 + 1, id);
        }

        break;
      }

      case ROBOTIC_CMD_IF_THING_DIR: // if thing dir
      {
        if(id)
        {
          int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum thing check_id = parse_param_thing(mzx_world, p2);
          char *p3 = next_param_pos(p2);
          int check_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          enum dir direction = parse_param_dir(mzx_world, p4);

          // Note: beneath never goes to label prior to 2.51s2.
          if(check_dir_xy(mzx_world, check_id, check_color,
           check_param, x, y, direction, cur_robot, _bl))
          {
            char *p5 = next_param_pos(p4);
            gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
          }
        }
        break;
      }

      case ROBOTIC_CMD_IF_NOT_THING_DIR: // if NOT thing dir
      {
        if(id)
        {
          int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum thing check_id = parse_param_thing(mzx_world, p2);
          char *p3 = next_param_pos(p2);
          int check_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          enum dir direction = parse_param_dir(mzx_world, p4);

          // Note: beneath always goes to label prior to 2.51s2.
          if(!check_dir_xy(mzx_world, check_id, check_color,
           check_param, x, y, direction, cur_robot, _bl))
          {
            char *p5 = next_param_pos(p4);
            gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
          }
        }
        break;
      }

      case ROBOTIC_CMD_IF_THING_XY: // if thing x y
      {
        int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing check_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        unsigned int check_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int check_x = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        int check_y = parse_param(mzx_world, p5, id);
        int fg, bg;
        int offset;

        if(check_id == SPRITE)
        {
          int ret = 0;

          if(check_param < MAX_SPRITES &&
           mzx_world->sprite_list[check_param]->flags & SPRITE_UNBOUND)
            prefix_mid_xy_unbound(mzx_world, &check_x, &check_y, x, y);
          else
            prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);

          // Versions from 2.69c to 2.82b would use sprite_num instead of the
          // param if the provided color was c??. This was unintuitive and
          // redundant with newer language extensions, so it was removed.
          if((check_color == 288) &&
           (mzx_world->version >= V269c) && (mzx_world->version <= V282))
            check_param = (unsigned int)mzx_world->sprite_num;

          /* 256 == p?? */
          if(check_param == 256)
          {
            int i;

            // This check was added in 2.84 to fix the removal of the above
            // check in 2.83. In 2.83, using "if c?? Sprite" always failed as
            // check_color would always be higher than MAX_SPRITES initially.
            // Add a version check if this breaks something.
            if(check_color == 288)
              check_color = 0;

            for(i = check_color; i < MAX_SPRITES; i++)
            {
              if(sprite_at_xy(mzx_world, mzx_world->sprite_list[i],
               check_x, check_y))
                break;
            }
            if(i == MAX_SPRITES)
            {
              i = -1;
              ret = 0;
            }
            else
            {
              mzx_world->sprite_num = i;
              ret = 1;
            }
          }
          else
          {
            if((unsigned int)check_param < 256)
            {
              ret = sprite_at_xy(mzx_world, mzx_world->sprite_list[check_param],
               check_x, check_y);
            }
          }

          if(ret)
          {
            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }
        else

        // Check collision detection for sprite - Exo

        if(check_id == SPR_COLLISION)
        {
          struct sprite *check_sprite;

          int ret;
          if(check_param >= 256)
          {
            check_param = (unsigned int)mzx_world->sprite_num;

            if(check_param >= 256)
              break;
          }

          check_sprite = mzx_world->sprite_list[check_param];

          if(check_color == 288)
          {
            check_x += check_sprite->x;
            check_y += check_sprite->y;
          }
          if(check_sprite->flags & SPRITE_UNBOUND)
            prefix_mid_xy_unbound(mzx_world, &check_x, &check_y, x, y);
          else
            prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          offset = check_x + (check_y * board_width);

          ret = sprite_colliding_xy(mzx_world, check_sprite,
           check_x, check_y);

          if(ret > 0)
          {
            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }
        else
        {
          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          offset = check_x + (check_y * board_width);

          split_colors(check_color, &fg, &bg);
          match_version_fix(mzx_world, &fg, &bg, (int *)&check_param);
          // TODO: fix whirlpool id
          if(check_at_xy(src_board, check_id, fg, bg, check_param, offset))
          {
            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }

        break;
      }

      case ROBOTIC_CMD_IF_AT: // if at x y label
      {
        if(id)
        {
          int check_x = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int check_y = parse_param(mzx_world, p2, id);

          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          if((check_x == x) && (check_y == y))
          {
            char *p3 = next_param_pos(p2);
            gotoed = send_self_label_tr(mzx_world, p3 + 1, id);
          }
        }
        break;
      }

      case ROBOTIC_CMD_IF_DIR_OF_PLAYER: // if dir of player is thing, "label"
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_color = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        enum thing check_id = parse_param_thing(mzx_world, p3);
        char *p4 = next_param_pos(p3);
        int check_param = parse_param(mzx_world, p4, id);

        if(check_dir_xy(mzx_world, check_id, check_color,
         check_param, mzx_world->player_x, mzx_world->player_y,
         direction, cur_robot, _bl))
        {
          char *p5 = next_param_pos(p4);
          gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_DOUBLE: // double c
      {
        char dest_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        mul_counter(mzx_world, dest_buffer, 2, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_HALF: // half c
      {
        char dest_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        div_counter(mzx_world, dest_buffer, 2, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_GOTO: // Goto
      {
        gotoed = send_self_label_tr(mzx_world, cmd_ptr + 2, id);
        break;
      }

      case ROBOTIC_CMD_SEND: // Send robot label
      {
        char robot_name_buffer[ROBOT_MAX_TR];
        char label_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);
        tr_msg(mzx_world, p2 + 1, id, label_buffer);

        send_robot(mzx_world, robot_name_buffer, label_buffer, 0);

        // Did the position get changed? (send to self)
        if(old_pos != cur_robot->cur_prog_line)
          gotoed = 1;

        break;
      }

      case ROBOTIC_CMD_EXPLODE: // Explode
      {
        if(id)
        {
          int offset = x + (y * board_width);
          level_param[offset] =
           (parse_param(mzx_world, cmd_ptr + 1, id) - 1) * 16;
          level_id[offset] = (char)EXPLOSION;
          clear_robot_id(src_board, id);
        }

        // Robot no longer exists; exit
        return;
      }

      case ROBOTIC_CMD_PUT_DIR: // put thing dir
      {
        if(id)
        {
          int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum thing put_id = parse_param_thing(mzx_world, p2);
          char *p3 = next_param_pos(p2);
          int put_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          enum dir direction = parse_param_dir(mzx_world, p4);

          if(put_id < SENSOR)
          {
            if(mzx_world->version < V270 && direction == BENEATH)
              break;
            place_dir_xy(mzx_world, put_id, put_color, put_param, x, y,
             direction, cur_robot, _bl);
          }
          update_blocked = 1;
        }
        break;
      }

      case ROBOTIC_CMD_GIVE: // Give # item
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);
        inc_counter(mzx_world, item_to_counter[item_number], amount, 0);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_TAKE: // Take # item
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);

        if(get_counter(mzx_world, item_to_counter[item_number], 0) >=
         amount)
        {
          dec_counter(mzx_world, item_to_counter[item_number], amount, 0);
        }

        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_TAKE_OR: // Take # item "label"
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);

        if(get_counter(mzx_world, item_to_counter[item_number], 0) >=
         amount)
        {
          dec_counter(mzx_world, item_to_counter[item_number], amount, 0);
        }
        else
        {
          gotoed = send_self_label_tr(mzx_world,  p2 + 4, id);
        }

        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_ENDGAME: // Endgame
      {
        set_counter(mzx_world, "LIVES", 0, 0);
        set_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      case ROBOTIC_CMD_ENDLIFE: // Endlife
      {
        set_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      case ROBOTIC_CMD_MOD: // Mod
      {
        char mod_name_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, cmd_ptr + 2, id, mod_name_buffer);
        magic_load_mod(mzx_world, mod_name_buffer);
        audio_set_module_volume(src_board->volume);
        break;
      }

      case ROBOTIC_CMD_SAM: // sam
      {
        char sam_name_buffer[ROBOT_MAX_TR];
        int frequency = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        tr_msg(mzx_world, p2 + 1, id, sam_name_buffer);

        if(frequency < 0)
          frequency = 0;

        audio_play_sample(sam_name_buffer, true, frequency);

        break;
      }

      case ROBOTIC_CMD_VOLUME2:
      case ROBOTIC_CMD_VOLUME: // Volume
      {
        int volume = parse_param(mzx_world, cmd_ptr + 1, id);

        // Pre-port versions bounded volume by using a char.
        if(mzx_world->version < VERSION_PORT)
          volume &= 255;

        else
          volume = CLAMP(volume, 0, 255);

        src_board->volume = volume;
        src_board->volume_target = volume;

        audio_set_module_volume(volume);
        break;
      }

      case ROBOTIC_CMD_END_MOD: // End mod
      {
        audio_end_module();
        src_board->mod_playing[0] = 0;
        mzx_world->real_mod_playing[0] = 0;
        break;
      }

      case ROBOTIC_CMD_END_SAM: // End sam
      {
        audio_end_sample();
        break;
      }

      case ROBOTIC_CMD_PLAY: // Play notes
      {
        play_string(cmd_ptr + 2, 0);
        break;
      }

      case ROBOTIC_CMD_END_PLAY: // End play
      {
        sfx_clear_queue();
        break;
      }

      // FIXME - This probably needs a different implementation
      case ROBOTIC_CMD_WAIT_THEN_PLAY: // wait play "str"
      {
        int index_dif = sfx_length_left();

        if(index_dif > 10)
        {
          END_CYCLE;
          return;
        }

        play_string(cmd_ptr + 2, 0);

        break;
      }

      case ROBOTIC_CMD_WAIT_PLAY: // wait play
      {
        int index_dif = sfx_length_left();

        if(index_dif > 10)
        {
          END_CYCLE;
          return;
        }

        break;
      }

      case ROBOTIC_CMD_SFX: // sfx num
      {
        int sfx_num = parse_param(mzx_world, cmd_ptr + 1, id);
        play_sfx(mzx_world, sfx_num);
        break;
      }

      case ROBOTIC_CMD_PLAY_IF_SILENT: // play sfx notes
      {
        if(!sfx_is_playing())
          play_string(cmd_ptr + 2, 0);

        break;
      }

      case ROBOTIC_CMD_OPEN: // open
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          parsedir(direction, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(direction))
          {
            int new_x = x;
            int new_y = y;
            if(!move_dir(src_board, &new_x, &new_y, direction))
            {
              int new_offset = new_x + (new_y * board_width);
              enum thing new_id = (enum thing)level_id[new_offset];

              if((new_id == DOOR) || (new_id == GATE))
              {
                // Become pushable for right now
                int offset = x + (y * board_width);
                enum thing old_id = (enum thing)level_id[offset];
                level_id[offset] = (char)ROBOT_PUSHABLE;
                grab_item(mzx_world, new_x, new_y, 0);

                // If a door opened in the direction of the robot, the robot
                // was pushed and its position needs to be updated. This might
                // have been through a transport so just use xpos/ypos.
                if(level_id[offset] != ROBOT_PUSHABLE)
                {
                  x = cur_robot->xpos;
                  y = cur_robot->ypos;
                  offset = x + (y * board_width);
                }

                level_id[offset] = old_id;
                update_blocked = 1;
              }
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_LOCKSELF: // Lockself
      {
        cur_robot->is_locked = 1;
        break;
      }

      case ROBOTIC_CMD_UNLOCKSELF: // Unlockself
      {
        cur_robot->is_locked = 0;
        break;
      }

      case ROBOTIC_CMD_SEND_DIR: // Send DIR "label"
      {
        if(id)
        {
          int send_x = x;
          int send_y = y;
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          char *p2 = next_param_pos(cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(direction))
          {
            if(!move_dir(src_board, &send_x, &send_y, direction))
            {
              send_at_xy(mzx_world, id, send_x, send_y, p2 + 1);
              // Did the position get changed? (send to self)
              if(old_pos != cur_robot->cur_prog_line)
                gotoed = 1;
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_ZAP: // Zap label num
      {
        char label_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int num_times = parse_param(mzx_world, p2, id);
        int i;

        tr_msg(mzx_world, cmd_ptr + 2, id, label_buffer);
        for(i = 0; i < num_times; i++)
        {
          if(!zap_label(cur_robot, label_buffer))
            break;
        }
        break;
      }

      case ROBOTIC_CMD_RESTORE: // Restore label num
      {
        char label_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int num_times = parse_param(mzx_world, p2, id);
        int i;

        tr_msg(mzx_world, cmd_ptr + 2, id, label_buffer);
        for(i = 0; i < num_times; i++)
        {
          if(!restore_label(cur_robot, label_buffer))
            break;
        }
        break;
      }

      case ROBOTIC_CMD_LOCKPLAYER: // Lockplayer
      {
        src_board->player_ns_locked = 1;
        src_board->player_ew_locked = 1;
        src_board->player_attack_locked = 1;
        break;
      }

      case ROBOTIC_CMD_UNLOCKPLAYER: // unlockplayer
      {
        src_board->player_ns_locked = 0;
        src_board->player_ew_locked = 0;
        src_board->player_attack_locked = 0;
        break;
      }

      case ROBOTIC_CMD_LOCKPLAYER_NS: // lockplayer ns
      {
        src_board->player_ns_locked = 1;
        break;
      }

      case ROBOTIC_CMD_LOCKPLAYER_EW: // lockplayer ew
      {
        src_board->player_ew_locked = 1;
        break;
      }

      case ROBOTIC_CMD_LOCKPLAYER_ATTACK: // lockplayer attack
      {
        src_board->player_attack_locked = 1;
        break;
      }

      case ROBOTIC_CMD_MOVE_PLAYER_DIR: // move player dir
      case ROBOTIC_CMD_MOVE_PLAYER_DIR_OR: // move pl dir "label"
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if(is_cardinal_dir(direction))
        {
          int old_x = mzx_world->player_x;
          int old_y = mzx_world->player_y;
          int old_board = mzx_world->current_board_id;
          enum board_target old_target = mzx_world->target_where;

          // Have to fix vars and move to next command NOW, in case player
          // is sent to another screen!
          mzx_world->first_prefix = 0;
          mzx_world->mid_prefix = 0;
          mzx_world->last_prefix = 0;
          cur_robot->pos_within_line = 0;
          cur_robot->cur_prog_line +=
           program[cur_robot->cur_prog_line] + 2;

          if(!program[cur_robot->cur_prog_line])
            cur_robot->cur_prog_line = 0;

          // Move player
          move_player(mzx_world, dir_to_int(direction));

          if((cmd == ROBOTIC_CMD_MOVE_PLAYER_DIR_OR) &&
           (mzx_world->player_x == old_x) &&
           (mzx_world->player_y == old_y) &&
           (mzx_world->current_board_id == old_board) &&
           (mzx_world->target_where == old_target))
          {
            char *p2 = next_param_pos(cmd_ptr + 1);
            gotoed = send_self_label_tr(mzx_world,  p2 + 1, id);
          }

          END_CYCLE;
          return;
        }
        break;
      }

      case ROBOTIC_CMD_PUT_PLAYER_XY: // Put player x y
      {
        int put_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int put_y = parse_param(mzx_world, p2, id);

        prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);

        if(place_player_xy(mzx_world, put_x, put_y))
        {
          done = 1;
          if((mzx_world->player_x == x) && (mzx_world->player_y == y))
          {
            // Robot no longer exists; exit
            return;
          }
        }
        break;
      }

      case ROBOTIC_CMD_IF_PLAYER_XY: // if player x y
      {
        int check_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_y = parse_param(mzx_world, p2, id);
        prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);

        if((check_x == mzx_world->player_x) &&
         (check_y == mzx_world->player_y))
        {
          char *p3 = next_param_pos(p2);
          gotoed = send_self_label_tr(mzx_world, p3 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_PUT_PLAYER_DIR: // put player dir
      {
        enum dir put_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
        put_dir = parsedir(put_dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(put_dir))
        {
          int put_x = x;
          int put_y = y;
          if(!move_dir(src_board, &put_x, &put_y, put_dir))
          {
            if(place_player_xy(mzx_world, put_x, put_y))
            {
              if((mzx_world->player_x == x) && (mzx_world->player_y == y))
              {
                // Robot no longer exists; exit
                return;
              }

              done = 1;
            }
          }
        }

        break;
      }

      case ROBOTIC_CMD_ROTATECW: // rotate cw
      {
        if(id)
        {
          rotate(mzx_world, x, y, 0);
          // Figure blocked vars
          update_blocked = 1;
        }
        break;
      }

      case ROBOTIC_CMD_ROTATECCW: // rotate ccw
      {
        if(id)
        {
          rotate(mzx_world, x, y, 1);
          // Figure blocked vars
          update_blocked = 1;
          break;
        }
        break;
      }

      case ROBOTIC_CMD_SWITCH: // switch dir dir
      {
        if(id)
        {
          int src_x = x;
          int dest_x = x;
          int src_y = y;
          int dest_y = y;
          enum dir dest_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum dir src_dir = parse_param_dir(mzx_world, p2);

          src_dir = parsedir(src_dir, x, y, cur_robot->walk_dir);
          dest_dir = parsedir(dest_dir, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(src_dir) && is_cardinal_dir(dest_dir))
          {
            int status = move_dir(src_board, &src_x, &src_y, src_dir);

            status |= move_dir(src_board, &dest_x, &dest_y, dest_dir);
            if(!status)
            {
              // Switch src_x, src_y with dest_x, dest_y
              // The nice thing is that nothing gets deleted,
              // nothing gets copied.
              if((src_x != dest_x) || (src_y != dest_y))
              {
                struct robot *src_robot;
                int src_offset = src_x + (src_y * board_width);
                int dest_offset = dest_x + (dest_y * board_width);
                enum thing cp_id = (enum thing)level_id[src_offset];
                int cp_param = level_param[src_offset];
                int cp_color = level_color[src_offset];
                level_id[src_offset] = level_id[dest_offset];
                level_param[src_offset] = level_param[dest_offset];
                level_color[src_offset] = level_color[dest_offset];
                level_id[dest_offset] = cp_id;
                level_param[dest_offset] = cp_param;
                level_color[dest_offset] = cp_color;
                // Figure blocked vars
                update_blocked = 1;

                // This might have moved robots. Fix their xpos/ypos values.
                // Old versions didn't fix these, so don't touch the compat pos.
                if(is_robot(cp_id))
                {
                  src_robot = src_board->robot_list[cp_param];
                  src_robot->xpos = dest_x;
                  src_robot->ypos = dest_y;
                }
                if(is_robot(level_id[src_offset]))
                {
                  cp_param = level_param[src_offset];
                  src_robot = src_board->robot_list[cp_param];
                  src_robot->xpos = src_x;
                  src_robot->ypos = src_y;
                }
              }
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_SHOOT: // Shoot
      {
        if(id)
        {
          enum dir direction =
           parsedir((enum dir)cmd_ptr[2], x, y, cur_robot->walk_dir);
          if(is_cardinal_dir(direction) && !(_bl[dir_to_int(direction)] & 2))
          {
            // Block
            shoot(mzx_world, x, y, dir_to_int(direction),
             MIN(cur_robot->bullet_type, 2));

            // Versions 2.80 through 2.91X had the logic inverted here, meaning
            // the blocked array would never get updated when shooting. If this
            // fix breaks something, exclude those versions from setting this.
            if(!_bl[dir_to_int(direction)])
              _bl[dir_to_int(direction)] = 3;
          }
        }
        // MZX 2.83 erroneously ended the cycle here, some games depend on it...
        if(mzx_world->version == V283)
        {
          // Continue to the next line.
          ADVANCE_LINE;
          END_CYCLE;
          return;
        }

        break;
      }

      case ROBOTIC_CMD_LAYBOMB: // laybomb
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          place_dir_xy(mzx_world, LIT_BOMB, 8, 0, x, y, direction,
           cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case ROBOTIC_CMD_LAYBOMB_HIGH: // laybomb high
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          place_dir_xy(mzx_world, LIT_BOMB, 8, 128, x, y, direction,
           cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case ROBOTIC_CMD_SHOOTMISSILE: // shoot missile
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if(is_cardinal_dir(direction))
          {
            shoot_missile(mzx_world, x, y, dir_to_int(direction));
            calculate_blocked(mzx_world, x, y, id, _bl);
          }
        }
        // MZX 2.83 erroneously ended the cycle here, some games depend on it...
        if(mzx_world->version == V283)
        {
          // Continue to the next line.
          ADVANCE_LINE;
          END_CYCLE;
          return;
        }
        break;
      }

      case ROBOTIC_CMD_SHOOTSEEKER: // shoot seeker
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if(is_cardinal_dir(direction))
          {
            shoot_seeker(mzx_world, x, y, dir_to_int(direction));
            calculate_blocked(mzx_world, x, y, id, _bl);
          }
        }
        // MZX 2.83 erroneously ended the cycle here, some games depend on it...
        if(mzx_world->version == V283)
        {
          // Continue to the next line.
          ADVANCE_LINE;
          END_CYCLE;
          return;
        }
        break;
      }

      case ROBOTIC_CMD_SPITFIRE: // spit fire
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if(is_cardinal_dir(direction))
          {
            shoot_fire(mzx_world, x, y, dir_to_int(direction));
            calculate_blocked(mzx_world, x, y, id, _bl);
          }
        }
        // MZX 2.83 erroneously ended the cycle here, some games depend on it...
        if(mzx_world->version == V283)
        {
          // Continue to the next line.
          ADVANCE_LINE;
          END_CYCLE;
          return;
        }
        break;
      }

      case ROBOTIC_CMD_LAZERWALL: // lazer wall
      {
        if(id)
        {
          enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(direction))
          {
            char *p2 = next_param_pos(cmd_ptr + 1);
            int duration = parse_param(mzx_world, p2, id);
            shoot_lazer(mzx_world, x, y, dir_to_int(direction),
             duration, level_color[x + (y * board_width)]);
            // Figure blocked vars
            update_blocked = 1;
          }
        }
        break;
      }

      case ROBOTIC_CMD_PUT_XY: // put at xy
      {
        // Defer initialization of color until later because it
        // might be an MZM name string instead.
        int put_color;
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing put_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        int put_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_x = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        int put_y = parse_param(mzx_world, p5, id);

        // MZM image file
        if((put_id == IMAGE_FILE) &&
         *(cmd_ptr + 1) && (*(cmd_ptr + 2) == '@'))
        {
          int dest_width, dest_height;
          char mzm_name_buffer[ROBOT_MAX_TR];
          char *translated_name = cmalloc(MAX_PATH);

          // "Type" must be 0, 1, or 2; board, overlay, or vlayer
          put_param %= 3;

          if(put_param == 2)
          {
            // Use vlayer dimensions
            dest_width = mzx_world->vlayer_width;
            dest_height = mzx_world->vlayer_height;
          }
          else
          {
            dest_width = src_board->board_width;
            dest_height = src_board->board_height;
          }

          // Note: due to a bug between 2.80 and 2.93, using a REL FIRST
          // command after a REL command prevented this from working.
          prefix_mid_xy_var(mzx_world, &put_x, &put_y, x, y,
           dest_width, dest_height);

          tr_msg(mzx_world, cmd_ptr + 3, id, mzm_name_buffer);

          if(mzx_world->version >= V290 && is_string(mzm_name_buffer))
          {
            struct string src;

            if(get_string(mzx_world, mzm_name_buffer, &src, id))
              load_mzm_memory(mzx_world, mzm_name_buffer, put_x, put_y,
               put_param, 1, CUSTOM_BLOCK, src.value, src.length);
          }
          else
          {
            if(!fsafetranslate(mzm_name_buffer, translated_name, MAX_PATH))
            {
              load_mzm(mzx_world, translated_name, put_x, put_y,
               put_param, 1, CUSTOM_BLOCK);
            }
          }
          free(translated_name);
          if(id)
          {
            int offset = x + (y * board_width);
            enum thing d_id = (enum thing)level_id[offset];

            if(!is_robot(d_id))
            {
              // Robot no longer exists; exit
              return;
            }

            id = level_param[offset];
            cur_robot = src_board->robot_list[id];

            // Update position
            program = cur_robot->program_bytecode;
            cmd_ptr = program + cur_robot->cur_prog_line;

            update_blocked = 1;
          }
        }
        else

        // Sprite
        if(put_id == SPRITE)
        {
          put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          if(put_param == 256)
            put_param = mzx_world->sprite_num;

          if((unsigned int)put_param < 256)
          {
            if(mzx_world->sprite_list[put_param]->flags & SPRITE_UNBOUND)
              prefix_mid_xy_unbound(mzx_world, &put_x, &put_y, x, y);
            else
              prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);

            plot_sprite(mzx_world, mzx_world->sprite_list[put_param],
             put_color, put_x, put_y);
          }
        }
        else
        {
          // Shouldn't be able to place robots, scrolls etc this way!
          if(put_id < SENSOR)
          {
            put_color = parse_param(mzx_world, cmd_ptr + 1, id);
            prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);
            place_at_xy(mzx_world, put_id, put_color, put_param,
             put_x, put_y);
            // Still alive?
            if((put_x == x) && (put_y == y))
            {
              // Robot no longer exists; exit
              return;
            }

            update_blocked = 1;
          }
        }
        break;
      }

      case ROBOTIC_CMD_SEND_XY: // Send x y "label"
      {
        int send_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int send_y = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        prefix_mid_xy(mzx_world, &send_x, &send_y, x, y);

        send_at_xy(mzx_world, id, send_x, send_y, p3 + 1);
        // Did the position get changed? (send to self)
        if(old_pos != cur_robot->cur_prog_line)
          gotoed = 1;

        break;
      }

      case ROBOTIC_CMD_COPYROBOT_NAMED: // copyrobot ""
      {
        int first, last;
        char robot_name_buffer[ROBOT_MAX_TR];
        // Get the robot name
        tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);

        // Check the global robot
        if(!strcasecmp(mzx_world->global_robot.robot_name,
         robot_name_buffer))
        {
          replace_robot(mzx_world, src_board, &mzx_world->global_robot, id);
        }
        else
        {
          // Find the first robot that matches
          if(find_robot(src_board, robot_name_buffer, &first, &last))
          {
            struct robot *found_robot =
             src_board->robot_list_name_sorted[first];

            if(found_robot != cur_robot)
            {
              replace_robot(mzx_world, src_board, found_robot, id);
            }
          }
        }
        cur_robot = src_board->robot_list[id];
        END_CYCLE;
        return;
      }

      case ROBOTIC_CMD_COPYROBOT_XY: // copyrobot x y
      {
        int copy_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int copy_y = parse_param(mzx_world, p2, id);
        int offset;
        int d_id;

        prefix_mid_xy(mzx_world, &copy_x, &copy_y, x, y);
        offset = copy_x + (copy_y * board_width);
        d_id = (enum thing)level_id[offset];

        if(is_robot(d_id))
        {
          int idx = level_param[offset];
          replace_robot(mzx_world, src_board, src_board->robot_list[idx], id);
        }

        cur_robot = src_board->robot_list[id];
        END_CYCLE;
        return;
      }

      case ROBOTIC_CMD_COPYROBOT_DIR: // copyrobot dir
      {
        enum dir copy_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
        int offset;
        enum thing d_id;
        int copy_x = x;
        int copy_y = y;

        copy_dir = parsedir(copy_dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(copy_dir))
        {
          if(!move_dir(src_board, &copy_x, &copy_y, copy_dir))
          {
            offset = copy_x + (copy_y * board_width);
            d_id = (enum thing)level_id[offset];

            if(is_robot(d_id))
            {
              int idx = level_param[offset];
              replace_robot(mzx_world, src_board, src_board->robot_list[idx],
               id);
            }
          }
        }
        cur_robot = src_board->robot_list[id];
        END_CYCLE;
        return;
      }

      case ROBOTIC_CMD_DUPLICATE_SELF_DIR: // dupe self dir
      {
        if(id)
        {
          enum dir duplicate_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
          int dest_id;
          enum thing duplicate_id;
          int duplicate_color, offset;
          int duplicate_x = x;
          int duplicate_y = y;

          duplicate_dir = parsedir(duplicate_dir, x, y,
           cur_robot->walk_dir);

          if(is_cardinal_dir(duplicate_dir))
          {
            offset = x + (y * board_width);
            duplicate_color = level_color[offset];
            duplicate_id = (enum thing)level_id[offset];

            if(!move_dir(src_board, &duplicate_x, &duplicate_y, duplicate_dir))
            {
              // Fail if the player is at the destination; this would create a
              // buggy robot that doesn't exist on the board.
              if(level_id[duplicate_x + (duplicate_y * board_width)] == PLAYER)
                break;

              dest_id = duplicate_robot(mzx_world, src_board, cur_robot,
               duplicate_x, duplicate_y, 0);

              if(dest_id != -1)
                place_at_xy(mzx_world, duplicate_id, duplicate_color,
                 dest_id, duplicate_x, duplicate_y);
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_DUPLICATE_SELF_XY: // dupe self xy
      {
        int duplicate_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int duplicate_y = parse_param(mzx_world, p2, id);
        int dest_id;
        enum thing duplicate_id;
        int duplicate_color, offset;

        prefix_mid_xy(mzx_world, &duplicate_x, &duplicate_y, x, y);

        // Fail if the player is at the destination; this would create a
        // buggy robot that doesn't exist on the board.
        if(level_id[duplicate_x + (duplicate_y * board_width)] == PLAYER)
          break;

        if((duplicate_x != x) || (duplicate_y != y))
        {
          if(id)
          {
            offset = x + (y * board_width);
            duplicate_color = level_color[offset];
            duplicate_id = (enum thing)level_id[offset];
          }
          else
          {
            duplicate_color = 7;
            duplicate_id = ROBOT;
          }

          dest_id = duplicate_robot(mzx_world, src_board, cur_robot,
           duplicate_x, duplicate_y, 0);

          if(dest_id != -1)
          {
            place_at_xy(mzx_world, duplicate_id,
             duplicate_color, dest_id, duplicate_x, duplicate_y);
          }
        }
        else
        {
          cur_robot->cur_prog_line = 1;
          gotoed = 1;
        }
        break;
      }

      case ROBOTIC_CMD_BULLETN: // bulletn
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        id_chars[bullet_char + 0] = new_char;
        id_chars[bullet_char + 4] = new_char;
        id_chars[bullet_char + 8] = new_char;
        break;
      }

      case ROBOTIC_CMD_BULLETS: // bullets
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        id_chars[bullet_char + 1] = new_char;
        id_chars[bullet_char + 5] = new_char;
        id_chars[bullet_char + 9] = new_char;
        break;
      }

      case ROBOTIC_CMD_BULLETE: // bullete
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        id_chars[bullet_char + 2] = new_char;
        id_chars[bullet_char + 6] = new_char;
        id_chars[bullet_char + 10] = new_char;
        break;
      }

      case ROBOTIC_CMD_BULLETW: // bulletw
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        id_chars[bullet_char + 3] = new_char;
        id_chars[bullet_char + 7] = new_char;
        id_chars[bullet_char + 11] = new_char;
        break;
      }

      case ROBOTIC_CMD_GIVEKEY: // givekey col
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        give_key(mzx_world, key_num);
        break;
      }

      case ROBOTIC_CMD_GIVEKEY_OR: // givekey col "l"
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        if(give_key(mzx_world, key_num))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_TAKEKEY: // takekey col
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        take_key(mzx_world, key_num);
        break;
      }

      case ROBOTIC_CMD_TAKEKEY_OR: // takekey col "l"
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        if(take_key(mzx_world, key_num))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_INC_RANDOM: // inc c r
      {
        char dest_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        inc_counter(mzx_world, dest_buffer, result, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_DEC_RANDOM: // dec c r
      {
        char dest_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        dec_counter(mzx_world, dest_buffer, result, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_SET_RANDOM: // set c r
      {
        char dest_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        set_counter(mzx_world, dest_buffer, result, id);
        last_label = -1;
        break;
      }

      // Trade givenum givetype takenum taketype poorlabel
      case ROBOTIC_CMD_TRADE:
      {
        int give_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int give_type = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int take_num = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int take_type = parse_param(mzx_world, p4, id);
        int amount_held = get_counter(mzx_world, item_to_counter[take_type], 0);

        if(amount_held < take_num)
        {
          char *p5 = next_param_pos(p4);
          gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
        }
        else
        {
          dec_counter(mzx_world, item_to_counter[take_type], take_num, 0);
          inc_counter(mzx_world, item_to_counter[give_type], give_num, 0);
        }

        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_SEND_DIR_PLAYER: // Send DIR of player "label"
      {
        int send_x = mzx_world->player_x;
        int send_y = mzx_world->player_y;
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        direction = parsedir(direction, send_x, send_y,
         cur_robot->walk_dir);

        if(!move_dir(src_board, &send_x, &send_y, direction))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          send_at_xy(mzx_world, id, send_x, send_y, p2 + 1);
          // Did the position get changed? (send to self)
          if(old_pos != cur_robot->cur_prog_line)
            gotoed = 1;
        }
        break;
      }

      case ROBOTIC_CMD_PUT_DIR_PLAYER: // put thing dir of player
      {
        int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing put_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        int put_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        enum dir direction = parse_param_dir(mzx_world, p4);

        if(put_id < SENSOR)
        {
          int player_bl[4];

          calculate_blocked(mzx_world, mzx_world->player_x,
           mzx_world->player_y, 1, player_bl);

          place_dir_xy(mzx_world, put_id, put_color, put_param,
           mzx_world->player_x, mzx_world->player_y, direction,
           cur_robot, player_bl);

          /* Ensure that if the put involves overwriting this robot,
           * we immediately abort execution of it. However, the global
           * robot should not have this check done, since it doesn't
           * exist on the board.
           */
          if(id && !is_robot(level_id[x + (y * board_width)]))
          {
            // Robot no longer exists; exit
            return;
          }
        }

        update_blocked = 1;
        break;
      }

      case ROBOTIC_CMD_SLASH: // /"dirs"
      case ROBOTIC_CMD_PERSISTENT_GO: // Persistent go [str]
      {
        if(id)
        {
          int current_char;
          int direction;
          char dir_str_buffer[ROBOT_MAX_TR];

          tr_msg(mzx_world, cmd_ptr + 2, id, dir_str_buffer);

          // get next dir char, if none, next cmd
          current_char = dir_str_buffer[cur_robot->pos_within_line];
          cur_robot->pos_within_line++;
          // current_char must be 'n', 's', 'w', 'e' or 'i'
          switch(current_char)
          {
            case 'n':
            case 'N':
              direction = NORTH;
              break;
            case 's':
            case 'S':
              direction = SOUTH;
              break;
            case 'e':
            case 'E':
              direction = EAST;
              break;
            case 'w':
            case 'W':
              direction = WEST;
              break;
            case 'i':
            case 'I':
              direction = IDLE;
              break;
            default:
              direction = -1;
          }

          if(is_cardinal_dir(direction))
          {
            if((move(mzx_world, x, y, dir_to_int(direction),
             CAN_PUSH | CAN_TRANSPORT | CAN_FIREWALK | CAN_WATERWALK |
             CAN_LAVAWALK * cur_robot->can_lavawalk |
             (cur_robot->can_goopwalk ? CAN_GOOPWALK : 0))) &&
             (cmd == ROBOTIC_CMD_PERSISTENT_GO))
            {
              cur_robot->pos_within_line--; // persistent...
            }

            move_dir(src_board, &x, &y, direction);
          }

          if(direction != -1)
          {
            END_CYCLE;
            return;
          }
        }
        break;
      }

      case ROBOTIC_CMD_MESSAGE_LINE: // Mesg
      {
        char message_buffer[ROBOT_MAX_TR];

        tr_msg(mzx_world, cmd_ptr + 2, id, message_buffer);
        set_mesg_direct(src_board, message_buffer);
        break;
      }

      case ROBOTIC_CMD_MESSAGE_BOX_LINE:
      case ROBOTIC_CMD_MESSAGE_BOX_OPTION:
      case ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION:
      case ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE:
      case ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE:
      {
        // Box messages!
        char label_buffer[ROBOT_MAX_TR];
        int next_prog_line, next_cmd;

        robot_box_display(mzx_world, cmd_ptr - 1, label_buffer, id);

        // Move to end of all box mesg cmds.
        while(1)
        {
          // jump command length, command length bytes, command length
          next_prog_line = cur_robot->cur_prog_line +
                           program[cur_robot->cur_prog_line] + 2;

          // At next line- check type
          if(!program[next_prog_line])
          {
            END_PROGRAM;
            return;
          }

          next_cmd = program[next_prog_line + 1];
          if(!is_robot_box_command(next_cmd))
            break;

          cur_robot->cur_prog_line = next_prog_line;
        }

        // Send label
        if(label_buffer[0])
          gotoed = send_self_label_tr(mzx_world, label_buffer, id);

        /* If this isn't a label jump, or the jump failed, don't
         * execute the workaround for subroutines. Subroutine jumps
         * play tricks with the cur_prog_line variable, so we must not
         * increment it ourselves. Non-subroutine jumps don't care, so
         * we don't need to detect these.
         */
        if(!gotoed)
          ADVANCE_LINE;

        END_CYCLE;
        return;
      }

      case ROBOTIC_CMD_COMMENT: // comment-do nothing! Maybe.
      {
        // (unless first char is a @)
        if(cmd_ptr[2] == '@')
        {
          char name_buffer[ROBOT_MAX_TR];
          tr_msg(mzx_world, cmd_ptr + 3, id, name_buffer);
          name_buffer[ROBOT_NAME_SIZE - 1] = 0;

          if(id)
          {
            change_robot_name(src_board, cur_robot, name_buffer);
          }
          else
          {
            // Special code for global robot
            strcpy(cur_robot->robot_name, name_buffer);
          }
        }
        break;
      }

      case ROBOTIC_CMD_TELEPORT: // teleport
      {
        char board_dest_buffer[ROBOT_MAX_TR];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int teleport_x = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int teleport_y = parse_param(mzx_world, p3, id);
        int board_id;

        tr_msg(mzx_world, cmd_ptr + 2, id, board_dest_buffer);
        board_id = find_board(mzx_world, board_dest_buffer);

        if(board_id != NO_BOARD)
        {
          prefix_mid_xy_ext(mzx_world, mzx_world->board_list[board_id],
           &teleport_x, &teleport_y, x, y);

          mzx_world->target_board = board_id;
          mzx_world->target_x = teleport_x;
          mzx_world->target_y = teleport_y;
          mzx_world->target_where = TARGET_TELEPORT;
          done = 1;

          // MegaZeux 1.x performs the board change IMMEDIATELY.
          immediate_board_change_compat_v1(mzx_world, true);
        }

        break;
      }

      case ROBOTIC_CMD_SCROLLVIEW: // scrollview dir num
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(direction))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          int num = parse_param(mzx_world, p2, id);

          switch(direction)
          {
            case 1:
              src_board->scroll_y -= num;
              break;
            case 2:
              src_board->scroll_y += num;
              break;
            case 3:
              src_board->scroll_x += num;
              break;
            case 4:
              src_board->scroll_x -= num;
              break;

            default:
              break;
          }
        }
        break;
      }

      case ROBOTIC_CMD_INPUT: // input string
      {
        char input_buffer[ROBOT_MAX_TR];
        char title_buffer[ROBOT_MAX_TR];
        char *break_pos;
        size_t len;

        tr_msg(mzx_world, cmd_ptr + 2, id, title_buffer);
        title_buffer[71] = '\0';

        // Newlines break the UI. :(
        break_pos = strchr(title_buffer, '\n');
        if(break_pos)
          *break_pos = '\0';

        input_buffer[0] = 0;

        dialog_fadein();
        input_window(mzx_world, title_buffer, input_buffer, 70);

        // Due to a faulty check, 2.83 through 2.91f always stay faded in here.
        // If something is found that relies on that, make this conditional.
        dialog_fadeout();

        len = strlen(input_buffer);
        board_set_input_string(src_board, input_buffer, len);
        src_board->input_size = len;
        src_board->num_input = len ? atoi(src_board->input_string) : 0;
        break;
      }

      case ROBOTIC_CMD_IF_INPUT: // If string "" "l"
      {
        const char *input_string = src_board->input_string ? src_board->input_string : "";
        char cmp_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        if(!strcasecmp(cmp_buffer, input_string))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_IF_INPUT_NOT: // If string not "" "l"
      {
        const char *input_string = src_board->input_string ? src_board->input_string : "";
        char cmp_buffer[ROBOT_MAX_TR];
        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        if(strcasecmp(cmp_buffer, input_string))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_IF_INPUT_MATCHES: // If string matches "" "l"
      {
        // compare
        const char *input_string = src_board->input_string ? src_board->input_string : "";
        char cmp_buffer[ROBOT_MAX_TR];
        size_t i, cmp_len;
        char current_char;
        char cmp_char;

        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        cmp_len = strlen(cmp_buffer);

        for(i = 0; i < cmp_len; i++)
        {
          cmp_char = cmp_buffer[i];
          current_char = input_string[i];

          if((cmp_char >= 'a') && (cmp_char <= 'z'))
            cmp_char -= 32;
          if((current_char >= 'a') && (current_char <= 'z'))
            current_char -= 32;

          if(cmp_char == '?') continue;
          if(cmp_char == '#')
          {
            if((current_char < '0') || (current_char > '9'))
              break;
            continue;
          }
          if(cmp_char == '_')
          {
            if((current_char < 'A') || (current_char > 'Z'))
              break;
            continue;
          }
          if(cmp_char == '*')
          {
            i = 1000000;
            break;
          }

          if(cmp_char != current_char) break;
        }

        if(i >= cmp_len)
        {
          // Matches
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case ROBOTIC_CMD_PLAYER_CHAR: // Player char
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        int i;

        for(i = 0; i < 4; i++)
        {
          id_chars[player_char + i] = new_char;
        }
        break;
      }

      case ROBOTIC_CMD_MOVE_ALL: // move all thing dir
      {
        int move_color = parse_param(mzx_world, cmd_ptr + 1, id); // Color
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing move_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        int move_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        enum dir move_dir = parse_param_dir(mzx_world, p4);
        int int_dir;
        int lx, ly;
        int fg, bg;
        int offset;

        move_dir = parsedir(move_dir, x, y, cur_robot->walk_dir);
        split_colors(move_color, &fg, &bg);
        match_version_fix(mzx_world, &fg, &bg, &move_param);

        // TODO: fix whirlpool id AND level id(!)

        if(is_cardinal_dir(move_dir))
        {
          int_dir = dir_to_int(move_dir);
          // if dir is 1 or 4, search top to bottom.
          // if dir is 2 or 3, search bottom to top.
          if((int_dir == 0) || (int_dir == 3))
          {
            for(ly = 0, offset = 0; ly < board_height; ly++)
            {
              for(lx = 0; lx < board_width; lx++, offset++)
              {
                if(match_thing(level_id[offset], level_color[offset],
                 level_param[offset], move_id, fg, bg, move_param))
                {
                  move(mzx_world, lx, ly, int_dir,
                   CAN_PUSH | CAN_TRANSPORT | CAN_LAVAWALK |
                   CAN_FIREWALK | CAN_WATERWALK);
                }
              }
            }
          }
          else
          {
            offset = (board_width * board_height) - 1;
            for(ly = board_height - 1; ly >= 0; ly--)
            {
              for(lx = board_width - 1; lx >= 0; lx--, offset--)
              {
                if(match_thing(level_id[offset], level_color[offset],
                 level_param[offset], move_id, fg, bg, move_param))
                {
                  move(mzx_world, lx, ly, int_dir,
                   CAN_PUSH | CAN_TRANSPORT | CAN_LAVAWALK |
                   CAN_FIREWALK | CAN_WATERWALK);
                }
              }
            }
          }
        }
        done = 1;
        break;
      }

      case ROBOTIC_CMD_COPY: // copy x y x y
      {
        char *p1 = cmd_ptr + 1;
        char *p2 = next_param_pos(p1);
        char *p3 = next_param_pos(p2);
        char *p4 = next_param_pos(p3);
        int src_x, src_y, dest_x, dest_y;

        int src_type = -1, dest_type = 1;
        int type[4] = { -1 };

        type[0] = copy_block_param(mzx_world, id, p1, &src_x);
        type[1] = copy_block_param(mzx_world, id, p2, &src_y);
        type[2] = copy_block_param(mzx_world, id, p3, &dest_x);
        type[3] = copy_block_param(mzx_world, id, p4, &dest_y);

        if((type[0] == type[1]) && (type[0] <= 2))
          src_type = type[0];
        if((type[2] == type[3]) && (type[2] <= 2))
          dest_type = type[2];

        // Do the copy. This may invoke copy_block internally.
        copy_xy_to_xy_wrapper(mzx_world, id, x, y, src_type, dest_type,
         src_x, src_y, dest_x, dest_y);

        // If this robot was deleted, exit. NOTE: all port versions prior
        // to 2.92 had a faulty check here that would only check dest_x
        // and dest_y and not whether or not the robot was actually
        // overwritten. If something actually relied on this, add a
        // version check.
        if(id && dest_type == 0)
        {
          int offset = x + (y * board_width);
          int d_id = (enum thing)level_id[offset];
          int d_param = level_param[offset];

          if((d_id != ROBOT && d_id != ROBOT_PUSHABLE) || (d_param != id))
            return;

          update_blocked = 1;
        }
        break;
      }

      case ROBOTIC_CMD_SET_EDGE_COLOR: // set edge color
      {
        int new_color = parse_param(mzx_world, cmd_ptr + 1, id);
        mzx_world->edge_color =
         fix_color(new_color, mzx_world->edge_color);
        break;
      }

      case ROBOTIC_CMD_BOARD: // board dir
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if(is_cardinal_dir(direction))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          char board_name_buffer[ROBOT_MAX_TR];
          int board_number;

          tr_msg(mzx_world, p2 + 1, id, board_name_buffer);
          board_number = find_board(mzx_world, board_name_buffer);

          if(board_number != NO_BOARD)
            src_board->board_dir[dir_to_int(direction)] = board_number;
        }
        break;
      }

      case ROBOTIC_CMD_BOARD_IS_NONE: // board dir is none
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if(is_cardinal_dir(direction))
        {
          src_board->board_dir[dir_to_int(direction)] = NO_BOARD;
        }
        break;
      }

      case ROBOTIC_CMD_CHAR_EDIT: // char edit
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *next_param = next_param_pos(cmd_ptr + 1);
        char char_buffer[14];
        int i;

        for(i = 0; i < 14; i++)
        {
          char_buffer[i] = parse_param(mzx_world, next_param, id);
          next_param = next_param_pos(next_param);
        }
        // Prior to 2.90 char params are clipped
        if(mzx_world->version < V290)
          char_num &= 0xFF;

        ec_change_char(char_num, char_buffer);
        break;
      }

      case ROBOTIC_CMD_BECOME_PUSHABLE: // Become push
      {
        if(id)
        {
          level_id[x + (y * board_width)] = ROBOT_PUSHABLE;
        }
        break;
      }

      case ROBOTIC_CMD_BECOME_NONPUSHABLE: // Become nonpush
      {
        if(id)
        {
          level_id[x + (y * board_width)] = ROBOT;
        }
        break;
      }

      case ROBOTIC_CMD_BLIND: // blind
      {
        mzx_world->blind_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_FIREWALKER: // firewalker
      {
        mzx_world->firewalker_dur =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_FREEZETIME: // freezetime
      {
        mzx_world->freeze_time_dur =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_SLOWTIME: // slow time
      {
        mzx_world->slow_time_dur =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_WIND: // wind
      {
        mzx_world->wind_dur =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_AVALANCHE: // avalanche
      {
        int placement_period = 18;
        int x, y, offset, d_flag;

        for(y = 0, offset = 0; y < board_height; y++)
        {
          for(x = 0; x < board_width; x++, offset++)
          {
            d_flag = flags[(int)level_id[offset]];

            if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE) &&
             (Random(placement_period)) == 0)
            {
              id_place(mzx_world, x, y, BOULDER, 7, 0);
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_COPY_DIR: // copy dir dir
      {
        if(id)
        {
          enum dir src_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
          char *p2 = next_param_pos(cmd_ptr + 1);
          enum dir dest_dir = parse_param_dir(mzx_world, p2);

          src_dir = parsedir(src_dir, x, y, cur_robot->walk_dir);
          dest_dir = parsedir(dest_dir, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(src_dir) && is_cardinal_dir(dest_dir))
          {
            int src_x = x;
            int src_y = y;
            int dest_x = x;
            int dest_y = y;

            if(!move_dir(src_board, &src_x, &src_y, src_dir) &&
             !move_dir(src_board, &dest_x, &dest_y, dest_dir))
            {
              copy_xy_to_xy(mzx_world, src_x, src_y, dest_x, dest_y);

              // If this robot was deleted, exit. NOTE: all port versions prior
              // to 2.92 had a faulty check here that would only check dest_x
              // and dest_y and not whether or not the robot was actually
              // overwritten. If something actually relied on this, add a
              // version check. That said, this command overwriting the current
              // robot does not seem to be possible anyway.
              {
                int offset = x + (y * board_width);
                int d_id = (enum thing)level_id[offset];
                int d_param = level_param[offset];

                if((d_id != ROBOT && d_id != ROBOT_PUSHABLE) || (d_param != id))
                  return;
              }
              update_blocked = 1;
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_BECOME_LAVAWALKER: // become lavawalker
      {
        if(id)
        {
          cur_robot->can_lavawalk = 1;
        }
        break;
      }

      case ROBOTIC_CMD_BECOME_NONLAVAWALKER: // become non lavawalker
      {
        if(id)
        {
          cur_robot->can_lavawalk = 0;
        }
        break;
      }

      case ROBOTIC_CMD_CHANGE: // change color thing param color thing param
      {
        int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum thing check_id = parse_param_thing(mzx_world, p2);
        char *p3 = next_param_pos(p2);
        int check_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_color = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        enum thing put_id = parse_param_thing(mzx_world, p5);
        char *p6 = next_param_pos(p5);
        int put_param = parse_param(mzx_world, p6, id);
        int check_fg, check_bg;
        enum thing d_id;
        int offset, d_param, d_color;
        boolean weird_color = false;

        split_colors(check_color, &check_fg, &check_bg);
        match_version_fix(mzx_world, &check_fg, &check_bg, &check_param);
        replacement_version_fix(mzx_world, &put_color, &put_param, &weird_color);

        // Make sure the change isn't incompatable
        // Only robots (pushable or otherwise) can be changed to
        // robots. The same goes for scrolls/signs and sensors.
        // Players cannot be changed at all

        if(((put_id != SENSOR) || (check_id == SENSOR)) &&
         (((put_id != ROBOT_PUSHABLE) || ((check_id == ROBOT)
         || (check_id == ROBOT_PUSHABLE))) &&
          ((put_id != ROBOT) || ((check_id == ROBOT_PUSHABLE) ||
          (check_id == ROBOT))) &&
          ((put_id != SIGN) || (check_id == SCROLL))) &&
          ((put_id != SCROLL) || (check_id == SIGN)) &&
          ((put_id != PLAYER) && (check_id != PLAYER)))
        {
          // Clear out params for param-less objects,
          // lava, fire, ice, energizer, rotates, or life
          if((put_id == ICE) || (put_id == LAVA) ||
           (put_id == ENERGIZER) || (put_id == CW_ROTATE) ||
           (put_id == CCW_ROTATE) || (put_id == FIRE) ||
           (put_id == LIFE))
          {
            put_param = 0;
          }

          // Ignore upper stages for explosions
          if(put_id == EXPLOSION)
            put_param &= 0xF3;

          // Open door becomes door
          if(check_id == OPEN_DOOR)
            check_id = DOOR;

          // Whirlpool becomes base whirlpool
          if(is_whirlpool(check_id))
            check_id = WHIRLPOOL_1;

          // Cannot change param on these
          if(put_id > SENSOR)
            put_param = 256;

          for(offset = 0; offset < (board_width * board_height); offset++)
          {
            d_id = (enum thing)level_id[offset];
            d_param = level_param[offset];
            d_color = level_color[offset];

            // open door becomes door
            if(d_id == OPEN_DOOR)
              d_id = DOOR;

            // Whirpool becomes base one
            if(is_whirlpool(d_id))
              d_id = WHIRLPOOL_1;

            if(match_thing(d_id, d_color, d_param,
             check_id, check_fg, check_bg, check_param))
            {
              // Change the color and the ID
              level_color[offset] = fix_color(put_color, d_color);
              level_id[offset] = put_id;

              // Regrettably supporting extreme edge case DOS-era nonsense
              if(weird_color)
                level_color[offset] = (d_color & 0xf0) | put_color;

              if(((d_id == ROBOT_PUSHABLE) || (d_id == ROBOT)) &&
               (put_id != ROBOT) && (put_id != ROBOT_PUSHABLE))
              {
                // delete robot if not changing to a robot
                clear_robot_id(src_board, d_param);
              }
              else

              if(((d_id == SIGN) || (d_id == SCROLL)) &&
               (put_id != SIGN) && (put_id != SCROLL))
              {
                // delete scroll if not changing to a scroll/sign
                clear_scroll_id(src_board, d_param);
              }
              else

              if((d_id == SENSOR) && (put_id != SENSOR))
              {
                // delete sensor if not changing to a sensor
                clear_sensor_id(src_board, d_param);
              }

              if(put_id == 0)
              {
                offs_remove_id(mzx_world, offset);
                // If this LEAVES a space, use given color
                if(level_id[offset] == 0)
                  level_color[offset] = fix_color(put_color, d_color);
              }
              else
              {
                if(put_param != 256)
                  level_param[offset] = put_param;
              }
            }
          }
          // If we got deleted, exit

          if(id)
          {
            d_id = (enum thing)level_id[x + (y * board_width)];

            if(!is_robot(d_id))
              return;

            update_blocked = 1;
          }
        }

        break;
      }

      case ROBOTIC_CMD_PLAYERCOLOR: // player color
      {
        int new_color = parse_param(mzx_world, cmd_ptr + 1, id);
        if(get_counter(mzx_world, "INVINCO", 0))
        {
          mzx_world->saved_pl_color =
           fix_color(new_color, mzx_world->saved_pl_color);
        }
        else
        {
          id_chars[player_color] = fix_color(new_color,
           id_chars[player_color]);
        }
        break;
      }

      case ROBOTIC_CMD_BULLETCOLOR: // bullet color
      {
        int new_color = parse_param(mzx_world, cmd_ptr + 1, id);
        int i;

        for(i = 0; i < 3; i++)
        {
          bullet_color[i] =
           fix_color(new_color, bullet_color[i]);
        }
        break;
      }

      case ROBOTIC_CMD_MISSILECOLOR: // missile color
      {
        missile_color =
         fix_color(parse_param(mzx_world, cmd_ptr + 1, id), missile_color);
        break;
      }

      case ROBOTIC_CMD_MESSAGE_ROW: // message row
      {
        int b_mesg_row = parse_param(mzx_world, cmd_ptr + 1, id);
        if(b_mesg_row > 24)
          b_mesg_row = 24;

        if(b_mesg_row < 0)
          b_mesg_row = 0;

        src_board->b_mesg_row = b_mesg_row;
        break;
      }

      case ROBOTIC_CMD_REL_SELF: // rel self
      {
        if(id)
        {
          mzx_world->first_prefix = REL_TO_SELF;
          mzx_world->mid_prefix = REL_TO_SELF;
          mzx_world->last_prefix = REL_TO_SELF;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case ROBOTIC_CMD_REL_PLAYER: // rel player
      {
        mzx_world->first_prefix = REL_TO_PLAYER;
        mzx_world->mid_prefix = REL_TO_PLAYER;
        mzx_world->last_prefix = REL_TO_PLAYER;
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_REL_COUNTERS: // rel counters
      {
        mzx_world->first_prefix = REL_TO_XPOS_YPOS;
        mzx_world->mid_prefix = REL_TO_XPOS_YPOS;
        mzx_world->last_prefix = REL_TO_XPOS_YPOS;

        // The REL COUNTERS commands are broken in 2.00+ - the usage of
        // XPOS/YPOS and FIRSTXPOS/FIRSTYPOS/LASTXPOS/LASTYPOS are reversed.
        // This seems to be intentional but is completely backwards from what
        // the 2.00 help file claimed (and what would be sensible).
        if(mzx_world->version >= V200)
        {
          mzx_world->first_prefix = REL_TO_XPOS_YPOS_FIRST_OR_LAST;
          mzx_world->last_prefix = REL_TO_XPOS_YPOS_FIRST_OR_LAST;
        }
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_SET_ID_CHAR: // set id char # to 'c'
      {
        int id_char = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int id_value = parse_param(mzx_world, p2, id);

        set_id_char_by_legacy_index(id_char, id_value);
        break;
      }

      case ROBOTIC_CMD_JUMP_MOD_ORDER: // jump mod order #
      {
        audio_set_module_order(parse_param(mzx_world, cmd_ptr + 1, id));
        break;
      }

      case ROBOTIC_CMD_ASK: // ask yes/no
      {
        char question_buffer[ROBOT_MAX_TR];
        char *break_pos;
        int send_status;

        dialog_fadein();

        tr_msg(mzx_world, cmd_ptr + 2, id, question_buffer);

        // Kick da line break in da pants!
        if((break_pos = strchr(question_buffer, '\n')))
          *break_pos = '\0';

        if(!ask_yes_no(mzx_world, question_buffer))
          send_status = send_robot_id(mzx_world, id, "YES", 1);
        else
          send_status = send_robot_id(mzx_world, id, "NO", 1);

        if(!send_status)
          gotoed = 1;

        // Due to a faulty check, 2.83 through 2.91f always stay faded in here.
        // If something is found that relies on that, make this conditional.
        dialog_fadeout();
        break;
      }

      case ROBOTIC_CMD_FILLHEALTH: // fill health
      {
        set_counter(mzx_world, "HEALTH", mzx_world->health_limit, 0);
        break;
      }

      case ROBOTIC_CMD_THICK_ARROW: // thick arrow dir char
      {
        enum dir dir = parse_param_dir(mzx_world, cmd_ptr + 1);
        char *p2 = next_param_pos(cmd_ptr + 1);
        dir = parsedir(dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(dir))
        {
          id_chars[249 + dir] = parse_param(mzx_world, p2, id);
        }
        break;
      }

      case ROBOTIC_CMD_THIN_ARROW: // thin arrow dir char
      {
        enum dir dir = parse_param_dir(mzx_world, cmd_ptr + 1);
        char *p2 = next_param_pos(cmd_ptr + 1);
        dir = parsedir(dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(dir))
        {
          id_chars[253 + dir] =
           parse_param(mzx_world, p2, id);
        }
        break;
      }

      case ROBOTIC_CMD_SET_MAX_HEALTH: // set max health
      {
        mzx_world->health_limit = parse_param(mzx_world, cmd_ptr + 1, id);
        inc_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      case ROBOTIC_CMD_SAVE_PLAYER_POSITION: // save pos
      {
        save_player_position(mzx_world, 0);
        break;
      }

      case ROBOTIC_CMD_RESTORE_PLAYER_POSITION: // restore
      {
        restore_player_position(mzx_world, 0);
        done = 1;
        break;
      }

      case ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION: // exchange
      {
        restore_player_position(mzx_world, 0);
        save_player_position(mzx_world, 0);
        done = 1;
        break;
      }

      case ROBOTIC_CMD_MESSAGE_COLUMN: // mesg col
      {
        int b_mesg_col = parse_param(mzx_world, cmd_ptr + 1, id);

        if(b_mesg_col > 79)
          b_mesg_col = 79;

        if(b_mesg_col < 0)
          b_mesg_col = 0;

        src_board->b_mesg_col = b_mesg_col;
        break;
      }

      case ROBOTIC_CMD_CENTER_MESSAGE: // center mesg
      {
        src_board->b_mesg_col = -1;
        break;
      }

      case ROBOTIC_CMD_CLEAR_MESSAGE: // clear mesg
      {
        src_board->b_mesg_timer = 0;
        clear_intro_mesg();
        break;
      }

      case ROBOTIC_CMD_RESETVIEW: // resetview
      {
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        break;
      }

      case ROBOTIC_CMD_MOD_SAM: // modsam freq num
      {
        // Lock this to DOS worlds because it only ever had a use in DOS vers.
        // Also, no port worlds ever used it because it never worked.

        if((mzx_world->version < VERSION_PORT) && audio_get_music_on())
        {
          int frequency = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int sam_num = parse_param(mzx_world, p2, id);

          audio_spot_sample(frequency, sam_num);
        }
        break;
      }

      case ROBOTIC_CMD_SCROLLBASE: // scrollbase
      {
        mzx_world->scroll_base_color =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_SCROLLCORNER: // scrollcorner
      {
        mzx_world->scroll_corner_color =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_SCROLLTITLE: // scrolltitle
      {
        mzx_world->scroll_title_color =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_SCROLLPOINTER: // scrollpointer
      {
        mzx_world->scroll_pointer_color =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_SCROLLARROW: // scrollarrow
      {
        mzx_world->scroll_arrow_color =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_VIEWPORT: // viewport x y
      {
        int viewport_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int viewport_y = parse_param(mzx_world, p2, id);
        unsigned int viewport_width = src_board->viewport_width;
        unsigned int viewport_height = src_board->viewport_height;

        if(viewport_x < 0 || (viewport_x + viewport_width) > 80)
          viewport_x = 80 - viewport_width;
        if(viewport_y < 0 || (viewport_y + viewport_height) > 25)
          viewport_y = 25 - viewport_height;

        src_board->viewport_x = viewport_x;
        src_board->viewport_y = viewport_y;

        break;
      }

      case ROBOTIC_CMD_VIEWPORT_WIDTH: // viewport width height
      {
        int viewport_width = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int viewport_height = parse_param(mzx_world, p2, id);
        int viewport_x = src_board->viewport_x;
        int viewport_y = src_board->viewport_y;

        if((viewport_width < 0) || (viewport_width > 80))
          viewport_width = 80;

        if((viewport_height < 0) || (viewport_height > 25))
          viewport_height = 25;

        if(viewport_width == 0)
          viewport_width = 1;

        if(viewport_height == 0)
          viewport_height = 1;

        if(viewport_width > src_board->board_width)
          viewport_width = src_board->board_width;

        if(viewport_height > src_board->board_height)
          viewport_height = src_board->board_height;

        if((viewport_x + viewport_width) > 80)
          src_board->viewport_x = 80 - viewport_width;

        if((viewport_y + viewport_height) > 25)
          src_board->viewport_y = 25 - viewport_height;

        src_board->viewport_width = viewport_width;
        src_board->viewport_height = viewport_height;

        break;
      }

      case ROBOTIC_CMD_SAVE_PLAYER_POSITION_N: // save pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        save_player_position(mzx_world, pos);
        break;
      }

      case ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N: // restore pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        done = 1;
        break;
      }

      case ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N: // exchange pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        save_player_position(mzx_world, pos);
        done = 1;
        break;
      }

      // restore pos # duplicate self
      case ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N_DUPLICATE_SELF:
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        int duplicate_x = mzx_world->player_x;
        int duplicate_y = mzx_world->player_y;
        int duplicate_color, duplicate_id, dest_id;
        int offset;

        if((pos < 0) || (pos > 7))
          pos = 0;

        restore_player_position(mzx_world, pos);
        // Duplicate the robot to where the player was

        if(id)
        {
          offset = x + (y * board_width);
          duplicate_color = level_color[offset];
          duplicate_id = (enum thing)level_id[offset];
        }
        else
        {
          duplicate_color = 7;
          duplicate_id = ROBOT;
        }

        offset = duplicate_x + (duplicate_y * board_width);
        dest_id =
         duplicate_robot(mzx_world, src_board, cur_robot,
          duplicate_x, duplicate_y, 0);

        if(dest_id != -1)
        {
          level_id[offset] = duplicate_id;
          level_color[offset] = duplicate_color;
          level_param[offset] = dest_id;

          // This robot doesn't actually move. Who knows why this is here, but
          // removing it might be a compatibility problem with xpos/ypos...
          x = duplicate_x;
          y = duplicate_y;

          replace_player(mzx_world);

          done = 1;
        }
        break;
      }

      // exchange pos # duplicate self
      case ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N_DUPLICATE_SELF:
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        int duplicate_x = mzx_world->player_x;
        int duplicate_y = mzx_world->player_y;
        int duplicate_color, duplicate_id, dest_id;
        int offset;

        if((pos < 0) || (pos > 7))
          pos = 0;

        restore_player_position(mzx_world, pos);
        save_player_position(mzx_world, pos);
        // Duplicate the robot to where the player was

        if(id)
        {
          offset = x + (y * board_width);
          duplicate_color = level_color[offset];
          duplicate_id = (enum thing)level_id[offset];
        }
        else
        {
          duplicate_color = 7;
          duplicate_id = ROBOT;
        }

        offset = duplicate_x + (duplicate_y * board_width);
        dest_id =
         duplicate_robot(mzx_world, src_board, cur_robot,
          duplicate_x, duplicate_y, 0);

        if(dest_id != -1)
        {
          level_id[offset] = duplicate_id;
          level_color[offset] = duplicate_color;
          level_param[offset] = dest_id;

          // This robot doesn't actually move. Who knows why this is here, but
          // removing it might be a compatibility problem with xpos/ypos...
          x = duplicate_x;
          y = duplicate_y;

          replace_player(mzx_world);

          done = 1;
        }
        break;
      }

      case ROBOTIC_CMD_PLAYER_BULLETN: // Pl bulletn
      case ROBOTIC_CMD_PLAYER_BULLETS: // Pl bullets
      case ROBOTIC_CMD_PLAYER_BULLETE: // Pl bullete
      case ROBOTIC_CMD_PLAYER_BULLETW: // Pl bulletw
      case ROBOTIC_CMD_NEUTRAL_BULLETN: // Nu bulletn
      case ROBOTIC_CMD_NEUTRAL_BULLETS: // Nu bullets
      case ROBOTIC_CMD_NEUTRAL_BULLETE: // Nu bullete
      case ROBOTIC_CMD_NEUTRAL_BULLETW: // Nu bulletw
      case ROBOTIC_CMD_ENEMY_BULLETN: // En bulletn
      case ROBOTIC_CMD_ENEMY_BULLETS: // En bullets
      case ROBOTIC_CMD_ENEMY_BULLETE: // En bullete
      case ROBOTIC_CMD_ENEMY_BULLETW: // En bulletw
      {
        id_chars[bullet_char + (cmd - ROBOTIC_CMD_PLAYER_BULLETN)] =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_PLAYER_BULLET_COLOR: // Pl bcolor
      case ROBOTIC_CMD_NEUTRAL_BULLET_COLOR: // Nu bcolor
      case ROBOTIC_CMD_ENEMY_BULLET_COLOR: // En bcolor
      {
        bullet_color[cmd - ROBOTIC_CMD_PLAYER_BULLET_COLOR] =
         parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case ROBOTIC_CMD_REL_SELF_FIRST: // Rel self first
      {
        if(id)
        {
          mzx_world->first_prefix = REL_TO_SELF_FIRST_OR_LAST;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case ROBOTIC_CMD_REL_SELF_LAST: // Rel self last
      {
        if(id)
        {
          mzx_world->last_prefix = REL_TO_SELF_FIRST_OR_LAST;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case ROBOTIC_CMD_REL_PLAYER_FIRST: // Rel player first
      {
        mzx_world->first_prefix = REL_TO_PLAYER_FIRST_OR_LAST;
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_REL_PLAYER_LAST: // Rel player last
      {
        mzx_world->last_prefix = REL_TO_PLAYER_FIRST_OR_LAST;
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_REL_COUNTERS_FIRST: // Rel counters first
      {
        // BUG: should be REL_TO_XPOS_YPOS_FIRST_OR_LAST (see REL COUNTERS).
        mzx_world->first_prefix = REL_TO_XPOS_YPOS;
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_REL_COUNTERS_LAST: // Rel counters last
      {
        // BUG: should be REL_TO_XPOS_YPOS_FIRST_OR_LAST (see REL COUNTERS).
        mzx_world->last_prefix = REL_TO_XPOS_YPOS;
        lines_run--;
        goto next_cmd_prefix;
      }

      case ROBOTIC_CMD_MOD_FADE_OUT: // Mod fade out
      {
        src_board->volume_inc = -8;
        src_board->volume_target = 0;
        break;
      }

      case ROBOTIC_CMD_MOD_FADE_IN: // Mod fade in
      {
        char name_buffer[ROBOT_MAX_TR];
        src_board->volume_inc = 8;
        src_board->volume_target = 255;
        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);

        magic_load_mod(mzx_world, name_buffer);
        src_board->volume = 0;
        audio_set_module_volume(0);
        break;
      }

      case ROBOTIC_CMD_COPY_BLOCK: // Copy block sx sy width height dx dy
      case ROBOTIC_CMD_COPY_OVERLAY_BLOCK: // Copy overlay block etc
      {
        char dest_name_buffer[ROBOT_MAX_TR];
        char *p1 = cmd_ptr + 1;
        char *p2 = next_param_pos(p1);
        char *p3 = next_param_pos(p2);
        char *p4 = next_param_pos(p3);
        char *p5 = next_param_pos(p4);
        char *p6 = next_param_pos(p5);
        // These will always be set, but the compiler doesn't think so.
        int src_x = 0;
        int src_y = 0;
        int dest_x = 0;
        int dest_y = 0;
        int width = parse_param(mzx_world, p3, id);
        int height = parse_param(mzx_world, p4, id);

        // 0 is board, 1 is overlay, 2 is vlayer (dest: 3 is string, 4 is mzm)
        int type[4] = { 0 };
        int src_type = -1, dest_type = -1;
        type[0] = copy_block_param(mzx_world, id, p1, &src_x);
        type[1] = copy_block_param(mzx_world, id, p2, &src_y);
        type[2] = copy_block_param_special(mzx_world, id, p5, &dest_x,
         dest_name_buffer);
        type[3] = copy_block_param(mzx_world, id, p6, &dest_y);

        if((type[0] == type[1]) && (type[0] <= 2))
          src_type = type[0];

        if((type[2] == type[3]) || ((type[2] > 2) && (type[3] == 0)))
          dest_type = type[2];

        // something is wrong with the params, abort
        if((src_type < 0) || (dest_type < 0))
          break;

        if(cmd == ROBOTIC_CMD_COPY_OVERLAY_BLOCK)
        {
          if(src_type < 2)
            src_type ^= 1;
          if(dest_type < 2)
            dest_type ^= 1;
        }

        copy_block(mzx_world, id, x, y, src_type, dest_type, src_x, src_y,
         width, height, dest_x, dest_y, p5, dest_y, dest_name_buffer);

        // If we got deleted, exit
        if(id)
        {
          int offset = x + (y * board_width);
          int d_id = (enum thing)level_id[offset];
          int d_param = level_param[offset];

          if(!is_robot(d_id) || (d_param != id))
            return;

          update_blocked = 1;
        }

        break;
      }

      case ROBOTIC_CMD_CLIP_INPUT: // Clip input
      {
        const char *input_string = src_board->input_string ? src_board->input_string : "";
        size_t input_size = src_board->input_size;
        size_t i = 0;

        // Chop up to and through first section of whitespace.
        // First, until non space or end
        while(*input_string && i < input_size)
        {
          if(*input_string == ' ')
            break;
          input_string++;
          i++;
        }

        while(*input_string == ' ' && i < input_size)
          input_string++, i++;

        i = strlen(input_string);
        board_set_input_string(src_board, input_string, i);
        src_board->input_size = i;
        src_board->num_input = i ? atoi(src_board->input_string) : 0;
        break;
      }

      case ROBOTIC_CMD_PUSH: // Push dir
      {
        if(id)
        {
          enum dir push_dir = parse_param_dir(mzx_world, cmd_ptr + 1);
          push_dir = parsedir(push_dir, x, y, cur_robot->walk_dir);

          if(is_cardinal_dir(push_dir))
          {
            int push_x = x;
            int push_y = y;
            int int_dir = dir_to_int(push_dir);

            if(!move_dir(src_board, &push_x, &push_y, push_dir))
            {
              int offset = push_x + (push_y * board_width);
              int d_id = (enum thing)level_id[offset];
              int d_flag = flags[d_id];

              if((d_id == ROBOT_PUSHABLE) || (d_id == PLAYER) ||
               ((int_dir < 2) && (d_flag & A_PUSHNS)) ||
               ((int_dir >= 2) && (d_flag & A_PUSHEW)) ||
               (d_id == SENSOR))
              {
                push(mzx_world, x, y, int_dir, 0);
                update_blocked = 1;
              }
            }
          }
        }
        break;
      }

      case ROBOTIC_CMD_SCROLL_CHAR: // Scroll char dir
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum dir scroll_dir = parse_param_dir(mzx_world, p2);
        scroll_dir = parsedir(scroll_dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(scroll_dir))
        {
          char char_buffer[14];
          int i;

          // Prior to 2.90 char params are clipped
          if(mzx_world->version < V290) char_num &= 0xFF;

          ec_read_char(char_num, char_buffer);

          switch(scroll_dir)
          {
            case 1:
            {
              char wrap_row = char_buffer[0];

              for(i = 0; i < 13; i++)
              {
                char_buffer[i] = char_buffer[i + 1];
              }
              char_buffer[13] = wrap_row;

              break;
            }

            case 2:
            {
              char wrap_row = char_buffer[13];

              for(i = 13; i > 0; i--)
              {
                char_buffer[i] = char_buffer[i - 1];
              }
              char_buffer[0] = wrap_row;

              break;
            }

            case 3:
            {
              char wrap_bit;

              for(i = 0; i < 14; i++)
              {
                wrap_bit = char_buffer[i] & 1;
                char_buffer[i] >>= 1;
                if(wrap_bit)
                  char_buffer[i] |= 0x80;
              }
              break;
            }

            case 4:
            {
              char wrap_bit;

              for(i = 0; i < 14; i++)
              {
                wrap_bit = char_buffer[i] & 0x80;
                char_buffer[i] <<= 1;
                if(wrap_bit)
                  char_buffer[i] |= 1;
              }
            }

            default:
              break;
          }

          ec_change_char(char_num, char_buffer);
        }
        break;
      }

      case ROBOTIC_CMD_FLIP_CHAR: // Flip char dir
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        enum dir flip_dir = parse_param_dir(mzx_world, p2);
        char char_buffer[14];
        char current_row;
        int i;

        flip_dir = parsedir(flip_dir, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(flip_dir))
        {
          // Prior to 2.90 char params are clipped
          if(mzx_world->version < V290) char_num &= 0xFF;

          ec_read_char(char_num, char_buffer);

          switch(flip_dir)
          {
            case 1:
            case 2:
            {
              for(i = 0; i < 7; i++)
              {
                current_row = char_buffer[i];
                char_buffer[i] = char_buffer[13 - i];
                char_buffer[13 - i] = current_row;
              }
              break;
            }

            case 3:
            case 4:
            {
              for(i = 0; i < 14; i++)
              {
                current_row = char_buffer[i];
                current_row = (current_row << 4) | (current_row >> 4);
                current_row = ((current_row & 0xCC) >> 2) |
                 ((current_row & 0x33) << 2);
                current_row = ((current_row & 0xAA) >> 1) |
                 ((current_row & 0x55) << 1);

                char_buffer[i] = current_row;
              }
              break;
            }

            default:
              break;
          }

          ec_change_char(char_num, char_buffer);
        }
        break;
      }

      case ROBOTIC_CMD_COPY_CHAR: // copy char char
      {
        int src_char = parse_param(mzx_world, cmd_ptr + 1, id);
        char char_buffer[14];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int dest_char = parse_param(mzx_world, p2, id);

        // Prior to 2.90 char params are clipped
        if(mzx_world->version < V290)
        {
          src_char &= 0xFF;
          dest_char &= 0xFF;
        }

        ec_read_char(src_char, char_buffer);
        ec_change_char(dest_char, char_buffer);
        break;
      }

      case ROBOTIC_CMD_CHANGE_SFX: // change sfx
      {
        int fx_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);

        sfx_set_string(&mzx_world->custom_sfx, fx_num, p2, strlen(p2));
        break;
      }

      case ROBOTIC_CMD_COLOR_INTENSITY_ALL: // color intensity #%
      {
        int intensity = parse_param(mzx_world, cmd_ptr + 1, id);
        if(intensity < 0)
          intensity = 0;

        set_palette_intensity(intensity);
        break;
      }

      case ROBOTIC_CMD_COLOR_INTENSITY_N: // color intensity # #%
      {
        int color = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int intensity = parse_param(mzx_world, p2, id);
        if(intensity < 0)
          intensity = 0;

        set_color_intensity(color, intensity);
        break;
      }

      case ROBOTIC_CMD_COLOR_FADE_OUT: // color fade out
      {
        vquick_fadeout();
        break;
      }

      case ROBOTIC_CMD_COLOR_FADE_IN: // color fade in
      {
        vquick_fadein();
        break;
      }

      case ROBOTIC_CMD_SET_COLOR: // set color # r g b
      {
        // Now you can set all 256 colors this way (for SMZX)
        int pal_number = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int r = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int g = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int b = parse_param(mzx_world, p4, id);

        // It's pretty sad that this is necessary, but some people don't
        // know how to use set color apparently (see SM4MZX for details)
        r = CLAMP(r, 0, 63);
        g = CLAMP(g, 0, 63);
        b = CLAMP(b, 0, 63);

        set_rgb(pal_number, r, g, b);
        break;
      }

      case ROBOTIC_CMD_LOAD_CHAR_SET: // Load char set ""
      {
        char charset_name[ROBOT_MAX_TR];
        char *translated_name = cmalloc(MAX_PATH);
        char *src_name;
        int pos;

        tr_msg(mzx_world, cmd_ptr + 2, id, charset_name);

        // This will load a charset to a different position - Exo
        if(charset_name[0] == '+')
        {
          char tempc = charset_name[3];

          charset_name[3] = 0;
          pos = (int)strtol(charset_name + 1, &src_name, 16);
          charset_name[3] = tempc;
        }
        else

        if(charset_name[0] == '@')
        {
          char tempc;
          int maxlen;
          // The offset string length was increased in 2.90 to
          // allow for accessing extended char sets.
          if(mzx_world->version < V290)
            maxlen = 3;
          else
            maxlen = 4;
          tempc = charset_name[maxlen+1];
          charset_name[maxlen+1] = 0;
          pos = (int)strtol(charset_name + 1, &src_name, 10);
          charset_name[maxlen+1] = tempc;
        }
        else
        {
          src_name = charset_name;
          pos = 0;
        }

        // Load from string (2.90+)
        if(mzx_world->version >= V290 && is_string(src_name))
        {
          struct string src;

          if(get_string(mzx_world, src_name, &src, id))
            ec_mem_load_set_var(src.value, src.length, pos, mzx_world->version);
        }
        else

        // Load from file
        if(!fsafetranslate(src_name, translated_name, MAX_PATH))
        {
          ec_load_set_var(translated_name, pos, mzx_world->version);
        }

        free(translated_name);
        break;
      }

      case ROBOTIC_CMD_MULTIPLY: // multiply counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char dest_buffer[ROBOT_MAX_TR];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        mul_counter(mzx_world, dest_buffer, value, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_DIVIDE: // divide counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char dest_buffer[ROBOT_MAX_TR];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        div_counter(mzx_world, dest_buffer, value, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_MODULO: // mod counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char dest_buffer[ROBOT_MAX_TR];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        mod_counter(mzx_world, dest_buffer, value, id);
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_PLAYER_CHAR_DIR: // Player char dir 'c'
      {
        enum dir direction = parse_param_dir(mzx_world, cmd_ptr + 1);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int new_char = parse_param(mzx_world, p2, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        if(is_cardinal_dir(direction))
        {
          id_chars[player_char + dir_to_int(direction)] = new_char;
        }
        break;
      }

      // Can load full 256 color ones too now. Will use file size to
      // determine how many to load.
      case ROBOTIC_CMD_LOAD_PALETTE: // Load palette
      {
        char name_buffer[ROBOT_MAX_TR];
        char *translated_name = cmalloc(MAX_PATH);

        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);

        // Load palette from string (2.90+)
        if(mzx_world->version >= V290 && is_string(name_buffer))
        {
          struct string src;

          if(get_string(mzx_world, name_buffer, &src, id))
            load_palette_mem(src.value, src.length);
        }
        else

        // Load palette from file
        if(!fsafetranslate(name_buffer, translated_name, MAX_PATH))
        {
          load_palette(translated_name);
        }

        free(translated_name);
        break;
      }

      case ROBOTIC_CMD_MOD_FADE_TO: // Mod fade #t #s
      {
        int volume_target = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int volume = src_board->volume;
        int volume_inc = parse_param(mzx_world, p2, id);

        // Pre-port versions bounded volume_target by using a char.
        if(mzx_world->version < VERSION_PORT)
          volume_target &= 255;

        else
          volume_target = CLAMP(volume_target, 0, 255);

        if(volume_target == volume)
        {
          src_board->volume_inc = 0;
          src_board->volume_target = 0;
        }
        else
        {
          if(volume_inc == 0)
            volume_inc = 1;
          if((volume < volume_target) && (volume_inc < 0))
            volume_inc = -volume_inc;
          if((volume > volume_target) && (volume_inc > 0))
            volume_inc = -volume_inc;

          src_board->volume_target = volume_target;
          src_board->volume_inc = volume_inc;
        }
        break;
      }

      case ROBOTIC_CMD_SCROLLVIEW_XY: // Scrollview x y
      {
        int scroll_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int scroll_y = parse_param(mzx_world, p2, id);
        int n_scroll_x, n_scroll_y;

        prefix_mid_xy(mzx_world, &scroll_x, &scroll_y, x, y);

        // scroll_x/scrolly target to put in upper left corner
        // offset off of player position
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
        src_board->scroll_x = scroll_x - n_scroll_x;
        src_board->scroll_y = scroll_y - n_scroll_y;
        break;
      }

      case ROBOTIC_CMD_SWAP_WORLD: // Swap world str
      {
        char name_buffer[ROBOT_MAX_TR];
        char *translated_name = cmalloc(MAX_PATH);
        int redo_load;

        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);

        // Couldn't find world to swap to; abort cleanly
        if(fsafetranslate(name_buffer, translated_name, MAX_PATH))
        {
          free(translated_name);
          break;
        }

        /* If the swap world fails, give them Fail and Retry.
         * World validation prevents the bad swap from clearing data. */
        do
        {
          boolean ignore; // FIXME: Hack!
          redo_load = 0;
          if(!reload_swap(mzx_world, translated_name, &ignore))
          {
            redo_load = error("Error swapping to next world",
             ERROR_T_ERROR, ERROR_OPT_FAIL|ERROR_OPT_RETRY, 0x2C01);
          }
        } while(redo_load == ERROR_OPT_RETRY);
        free(translated_name);

        // User asked to "Fail" on error message above
        if(redo_load == ERROR_OPT_FAIL)
          break;

        mzx_world->change_game_state = CHANGE_STATE_SWAP_WORLD;
        return;
      }

      case ROBOTIC_CMD_IF_ALIGNEDROBOT: // If allignedrobot str str
      {
        if(id)
        {
          struct robot *dest_robot;
          int dest_x, dest_y;
          int first, last;
          char robot_name_buffer[ROBOT_MAX_TR];
          tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);

          if(!find_robot(src_board, robot_name_buffer, &first, &last))
          {
            /* Versions 2.80 through 2.93 didn't bother checking the return
             * value of find_robot. When find_robot returns 0, no robot with
             * that name exists and the values of both first and last contain
             * the index where a robot with that name should be inserted into
             * the name sorted list. In other words, this command checked the
             * robot alphabetically following the searched name or the stale
             * pointer at the end of the list (resulting in a crash).
             * It's doubtful anything relies on this, but allow just in case.
             */
            if(mzx_world->version < VERSION_PORT || mzx_world->version >= V293 ||
             last >= src_board->num_robots_active)
              break;
          }

          while(first <= last)
          {
            dest_robot = src_board->robot_list_name_sorted[first];
            get_robot_position(dest_robot, &dest_x, &dest_y);

            if(dest_robot && ((dest_x == x) || (dest_y == y)))
            {
              char *p2 = next_param_pos(cmd_ptr + 1);
              gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
              // DOS versions only send once here.
              if(mzx_world->version < VERSION_PORT)
                break;
            }
            first++;
          }
        }
        break;
      }

      case ROBOTIC_CMD_LOCKSCROLL: // Lockscroll
      {
        // The scrolling is locked at the current position.
        // Further scrollview cmds can change it.
        int scroll_x = src_board->scroll_x;
        int scroll_y = src_board->scroll_y;
        int n_scroll_x, n_scroll_y;
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
        src_board->locked_x = n_scroll_x;
        src_board->locked_y = n_scroll_y;
        src_board->scroll_x = scroll_x;
        src_board->scroll_y = scroll_y;
        break;
      }

      case ROBOTIC_CMD_UNLOCKSCROLL: // Unlockscroll
      {
        src_board->locked_x = -1;
        src_board->locked_y = -1;
        break;
      }

      case ROBOTIC_CMD_IF_FIRST_INPUT: // If first string str str
      {
        char tmp[2] = "";
        char *input_string = src_board->input_string;
        char match_string_buffer[ROBOT_MAX_TR];
        ssize_t i = 0;

        if(!input_string)
          input_string = tmp;

        if(src_board->input_size)
        {
          do
          {
            if(input_string[i] == 32) break;
            i++;
          } while(i < (ssize_t)src_board->input_size && input_string[i]);
        }

        if(input_string[i] == 32)
          input_string[i] = 0;
        else
          i = -1;

        tr_msg(mzx_world, cmd_ptr + 2, id, match_string_buffer);

        // Compare
        if(!strcasecmp(input_string, match_string_buffer))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }

        if(i >= 0)
          input_string[i] = 32;

        break;
      }

      case ROBOTIC_CMD_WAIT_MOD_FADE: // Wait mod fade
      {
        if(src_board->volume != src_board->volume_target)
        {
          END_CYCLE;
          return;
        }
        break;
      }

      case ROBOTIC_CMD_ENABLE_SAVING: // Enable saving
      {
        src_board->save_mode = CAN_SAVE;
        break;
      }

      case ROBOTIC_CMD_DISABLE_SAVING: // Disable saving
      {
        src_board->save_mode = CANT_SAVE;
        break;
      }

      case ROBOTIC_CMD_ENABLE_SENSORONLY_SAVING: // Enable sensoronly saving
      {
        src_board->save_mode = CAN_SAVE_ON_SENSOR;
        break;
      }

      case ROBOTIC_CMD_STATUS_COUNTER: // Status counter ## str
      {
        int counter_slot = parse_param(mzx_world, cmd_ptr + 1, id);
        if((counter_slot >= 1) && (counter_slot <= 6))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          char counter_name[ROBOT_MAX_TR];
          tr_msg(mzx_world, p2 + 1, id, counter_name);

          if(strlen(counter_name) >= COUNTER_NAME_SIZE)
            counter_name[COUNTER_NAME_SIZE - 1] = 0;

          strcpy(mzx_world->status_counters_shown[counter_slot - 1],
           counter_name);
        }
        break;
      }

      case ROBOTIC_CMD_OVERLAY_ON: // Overlay on
      {
        setup_overlay(src_board, 1);
        break;
      }

      case ROBOTIC_CMD_OVERLAY_STATIC: // Overlay static
      {
        setup_overlay(src_board, 2);
        break;
      }

      case ROBOTIC_CMD_OVERLAY_TRANSPARENT: // Overlay transparent
      {
        setup_overlay(src_board, 3);
        break;
      }

      case ROBOTIC_CMD_OVERLAY_PUT_OVERLAY: // put col ch overlay x y
      {
        int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int put_char = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int put_x = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_y = parse_param(mzx_world, p4, id);
        int offset;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);
        offset = put_x + (put_y * board_width);

        put_color =
          fix_color(put_color, src_board->overlay_color[offset]);

        src_board->overlay[offset] = put_char;
        src_board->overlay_color[offset] = put_color;
        break;
      }

      case ROBOTIC_CMD_CHANGE_OVERLAY: // Change overlay col ch col ch
      {
        int src_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int src_char = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int dest_color = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int dest_char = parse_param(mzx_world, p4, id);
        int src_fg, src_bg, i;
        char *overlay, *overlay_color;
        int d_color;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;

        split_colors(src_color, &src_fg, &src_bg);
        match_version_fix(mzx_world, &src_fg, &src_bg, NULL);

        for(i = 0; i < (board_width * board_height); i++)
        {
          d_color = overlay_color[i];

          if((overlay[i] == src_char) &&
           match_overlay_color(d_color, src_fg, src_bg))
          {
            overlay[i] = dest_char;
            overlay_color[i] = fix_color(dest_color, d_color);
          }
        }

        break;
      }

      case ROBOTIC_CMD_CHANGE_OVERLAY_COLOR: // Change overlay col col
      {
        int src_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int dest_color = parse_param(mzx_world, p2, id);
        int src_fg, src_bg, i;
        char *overlay_color;
        int d_color;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay_color = src_board->overlay_color;

        split_colors(src_color, &src_fg, &src_bg);
        match_version_fix(mzx_world, &src_fg, &src_bg, NULL);

        for(i = 0; i < (board_width * board_height); i++)
        {
          d_color = overlay_color[i];
          if(match_overlay_color(d_color, src_fg, src_bg))
          {
            overlay_color[i] = fix_color(dest_color, d_color);
          }
        }
        break;
      }

      case ROBOTIC_CMD_WRITE_OVERLAY: // Write overlay col str # #
      {
        int write_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *write_string = p2 + 1;
        char *p3 = next_param_pos(p2);
        int write_x = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int write_y = parse_param(mzx_world, p4, id);
        int offset;
        char string_buffer[ROBOT_MAX_TR];
        char current_char;
        char *overlay, *overlay_color;

        prefix_mid_xy(mzx_world, &write_x, &write_y, x, y);
        offset = write_x + (write_y * board_width);

        tr_msg(mzx_world, write_string, id, string_buffer);

        write_string = string_buffer;
        current_char = *write_string;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay_color = src_board->overlay_color;
        overlay = src_board->overlay;

        while(current_char != 0)
        {
          overlay_color[offset] = write_color;
          overlay[offset] = current_char;
          write_x++;
          if(write_x == board_width)
            break;
          write_string++;
          current_char = *write_string;
          offset++;
        }
        break;
      }

      case ROBOTIC_CMD_LOOP_START: // Loop start
      {
        cur_robot->loop_count = 0;
        break;
      }

      case ROBOTIC_CMD_LOOP_FOR: // Loop #
      {
        int loop_amount = parse_param(mzx_world, cmd_ptr + 1, id);
        int loop_count = cur_robot->loop_count;
        int back_cmd;

        if(loop_count < loop_amount)
        {
          back_cmd = cur_robot->cur_prog_line;
          do
          {
            if(program[back_cmd - 1] == 0xFF)
              break;

            back_cmd -= program[back_cmd - 1] + 2;
          } while(program[back_cmd + 1] != ROBOTIC_CMD_LOOP_START);

          cur_robot->cur_prog_line = back_cmd;
        }

        cur_robot->loop_count = loop_count + 1;
        last_label = -1;
        break;
      }

      case ROBOTIC_CMD_ABORT_LOOP: // Abort loop
      {
        int forward_cmd = cur_robot->cur_prog_line;

        do
        {
          if(!program[forward_cmd])
            break;

          forward_cmd += program[forward_cmd] + 2;
        } while(program[forward_cmd + 1] != ROBOTIC_CMD_LOOP_FOR);

        if(program[forward_cmd])
          cur_robot->cur_prog_line = forward_cmd;

        break;
      }

      case ROBOTIC_CMD_DISABLE_MESG_EDGE: // Disable mesg edge
      {
        mzx_world->mesg_edges = 0;
        break;
      }


      case ROBOTIC_CMD_ENABLE_MESG_EDGE: // Enable mesg edge
      {
        mzx_world->mesg_edges = 1;
        break;
      }

      case ROBOTIC_CMD_LABEL:
      {
        // Prior to the port, MZX would end the cycle if a label was
        // seen more than once in that cycle. This behaviour was
        // exploited in certain pre-port games such as "Kya's Sword" and
        // "Stones & Roks II", where it would interact with the SHOOT
        // command and alter the timing when SHOOT was in a tight loop.

        // We don't really care for this behaviour in the port, and the
        // compatibility has been broken forever, so only do this for
        // old worlds.

        if(mzx_world->version < VERSION_PORT)
        {
          if(last_label == cur_robot->cur_prog_line)
          {
            END_CYCLE;
            return;
          }
          last_label = cur_robot->cur_prog_line;
        }

        lines_run--;
        break;
      }

      case ROBOTIC_CMD_ZAPPED_LABEL:
      {
        lines_run--;
        break;
      }
    }

    // Go to next command! First erase prefixes...
    mzx_world->first_prefix = 0;
    mzx_world->mid_prefix = 0;
    mzx_world->last_prefix = 0;
    mzx_world->command_cache = 0;

    next_cmd_prefix:
    // Next line
    first_cmd = 0;

    if(!cur_robot->cur_prog_line)
    {
      cur_robot->pos_within_line = 0;
      break;
    }

#ifdef CONFIG_EDITOR
    // Check to see if a watchpoint triggered before incrementing the program.
    if(mzx_world->editing && debug_robot_watch)
    {
      // Returns 1 if the user chose to stop the program.
      switch(debug_robot_watch(ctx, cur_robot, id, lines_run))
      {
        case DEBUG_EXIT:
          break;

        case DEBUG_GOTO:
          // If this robot received a label, don't advance the program.
          if(old_pos != cur_robot->cur_prog_line)
            gotoed = 1;
          break;

        case DEBUG_HALT:
          END_CYCLE;
          return;
      }
    }
#endif

    // If we're returning from a subroutine, we don't want to set the
    // pos_within_line. Other sends will set it to zero anyway.
    if(!gotoed)
      ADVANCE_LINE;

    if(!program[cur_robot->cur_prog_line])
    {
      //End of program
      END_PROGRAM;
      return;
    }

    if(update_blocked)
    {
      calculate_blocked(mzx_world, x, y, id, _bl);
    }
    find_player(mzx_world);

    // Some commands can decrement lines_run, putting it at -1 here,
    // so add 2 to lines_run for the check. Originally this checked every 1 mil
    // commands, but it turns out checking every 64k commands is faster due to
    // better x86 optimizations using the lower word of lines_run.
    if(((lines_run + 2) & 0xffff) == 0)
    {
      // Only check after 1 mil commands to reduce false positives.
      if(lines_run >= 1000000 && peek_exit_input())
      {
        update_event_status();

        if(get_exit_status() || get_key_status(keycode_internal, IKEY_ESCAPE))
        {
          boolean exit_game;
          dialog_fadein();
          exit_game = !confirm(mzx_world,
           "MegaZeux appears to have frozen. Do you want to exit?");
          dialog_fadeout();
          update_screen();

          if(exit_game)
          {
            mzx_world->change_game_state = CHANGE_STATE_REQUEST_EXIT;
            break;
          }
        }
      }
    }

  } while(((++lines_run) < mzx_world->commands) && (!done));

  END_CYCLE;
}
