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

// New counter.cpp. Sorted lists make for faster searching.
// Builtins are also cleaned up by being put on a seperate list.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#include "board.h"
#include "configure.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "expr.h"
#include "game_ops.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "memcasecmp.h"
#include "rasm.h"
#include "robot.h"
#include "sprite.h"
#include "str.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"
#include "io/fsafeopen.h"
#include "io/vio.h"

#include "audio/audio.h"
#include "event/event.h"
#include "platform/platform.h"

/**
 * TODO: Counter lookups are currently case-insensitive, which is somewhat of
 * a performance concern. A good future (3.xx) feature might be to lowercase
 * all counter names.
 */

#ifdef CONFIG_COUNTER_HASH_TABLES
#include "hashtable.h"
HASH_SET_INIT(COUNTER, struct counter *, name, name_length)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct function_counter
{
  const char *const name;
  int minimum_version;
  int (*const function_read)(struct world *mzx_world,
   const struct function_counter *counter, const char *name, int id);
  void (*const function_write)(struct world *mzx_world,
   const struct function_counter *counter, const char *name, int value,
   int id);
};

static unsigned int get_board_x_board_y_offset(struct world *mzx_world, int id)
{
  int board_x = get_counter(mzx_world, "board_x", id);
  int board_y = get_counter(mzx_world, "board_y", id);

  board_x = CLAMP(board_x, 0, mzx_world->current_board->board_width - 1);
  board_y = CLAMP(board_y, 0, mzx_world->current_board->board_height - 1);

  return board_y * mzx_world->current_board->board_width + board_x;
}

//TODO: make this work with any number of params
static int get_counter_params(const char *src, //unsigned int num,
 int *v1, int *v2)
{
  char *next;

  *v1 = strtol(src, &next, 10);
  if(*next == ',')
  {
    *v2 = strtol(next + 1, NULL, 10);
    return 0;
  }
  else
  {
    return -1;
  }
}

static int translate_coordinates(const char *src, unsigned int *x,
                                 unsigned int *y)
{
  char *next;

  *x = strtol(src, &next, 10);
  if(*next == ',')
  {
    *y = strtol(next + 1, NULL, 10);
    return 0;
  }
  else
  {
    return -1;
  }
}

static int string_counter_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  // TODO: the only uses of get_counter() that can make it here should be using
  // a temporary buffer already, so this cast should be safe... however, it is
  // still very tacky.
  return string_read_as_counter(mzx_world, (char *)name, id);
}

static void string_counter_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  // TODO: the only uses of set_counter() that can make it here should be using
  // a temporary buffer already, so this cast should be safe... however, it is
  // still very tacky.
  string_write_as_counter(mzx_world, (char *)name, value, id);
}

static int local_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->local[0];
}

static void local_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->local[0] = value;
}

static int localn_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  return (mzx_world->current_board->robot_list[id])->local[local_num];
}

static void localn_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  (mzx_world->current_board->robot_list[id])->local[local_num] = value;
}

static int loopcount_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->loop_count;
}

static void loopcount_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->loop_count = value;
}

static int playerdist_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];

  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int thisx, thisy;
  get_robot_position(cur_robot, &thisx, &thisy);

  return abs(player_x - thisx) + abs(player_y - thisy);
}

static int sin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(sin(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int cos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(cos(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int tan_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(tan(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int asin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((asinf((float)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int acos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((acosf((float)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int atan_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((atan2f((float)val, (float)mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int atan2_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int dy = 0, dx = 0;
  get_counter_params(name + 6, &dy, &dx);
  return (int)((atan2f((float)dy, (float)dx) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int c_divisions_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->c_divisions;
}

static int divider_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->divider;
}

static int multiplier_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->multiplier;
}

static void c_divisions_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->c_divisions = value;
}

static void divider_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->divider = value;
}

static void multiplier_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->multiplier = value;
}

static int thisx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  int thisx, thisy;
  get_robot_position(cur_robot, &thisx, &thisy);

  if(mzx_world->mid_prefix == REL_TO_PLAYER)
    return thisx - mzx_world->player_x;

  return thisx;
}

static int thisy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  int thisx, thisy;
  get_robot_position(cur_robot, &thisx, &thisy);

  if(mzx_world->mid_prefix == REL_TO_PLAYER)
    return thisy - mzx_world->player_y;

  return thisy;
}

static int playerx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->player_x;
}

static int playery_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->player_y;
}

static int this_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->robot_char;
}

static int this_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];
  int thisx, thisy;
  int offset;
  get_robot_position(cur_robot, &thisx, &thisy);
  offset = thisx + (thisy * src_board->board_width);

  // No global
  if(id == 0)
    return -1;

  return src_board->level_color[offset];
}

static int sqrt_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)(sqrt(val));
}

static int abs_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 3, NULL, 10);
  return abs(val);
}

static int maxval_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int v1 = 0, v2 = 0;
  get_counter_params(name + 3, &v1, &v2);

  if(v1 > v2)
    return v1;
  else
    return v2;
}

static int minval_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int v1 = 0, v2 = 0;
  get_counter_params(name + 3, &v1, &v2);

  if (v1 < v2)
    return v1;
  else
    return v2;
}

static int playerfacedir_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->player_last_dir >> 4;
}

static void playerfacedir_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 3)
    value = 3;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0x0F) | (value << 4);
}

static int playerlastdir_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->player_last_dir & 0x0F;
}

static void playerlastdir_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 4)
    value = 4;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0xF0) | value;
}

static int horizpld_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  int thisx, thisy;
  get_robot_position(cur_robot, &thisx, &thisy);

  return abs(mzx_world->player_x - thisx);
}

static int vertpld_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  int thisx, thisy;
  get_robot_position(cur_robot, &thisx, &thisy);

  return abs(mzx_world->player_y - thisy);
}

static int board_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return get_id_char(mzx_world->current_board, offset);
}

static int board_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  int ignore_under = 1;

  if((mzx_world->version >= V280) && (mzx_world->version <= V283))
    ignore_under = 0;

  return get_id_board_color(mzx_world->current_board, offset, ignore_under);
}

static int board_w_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->board_width;
}

static int board_h_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->board_height;
}

static int board_id_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return mzx_world->current_board->level_id[offset];
}

static void board_id_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  struct board *src_board = mzx_world->current_board;
  char cvalue = value;

  if((cvalue < SENSOR) && (src_board->level_id[offset] < SENSOR))
    src_board->level_id[offset] = cvalue;
}

static int board_param_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return mzx_world->current_board->level_param[offset];
}

static void board_param_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  struct board *src_board = mzx_world->current_board;

  if(src_board->level_id[offset] < 122)
    src_board->level_param[offset] = value;
}

static int red_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  return get_red_component(cur_color);
}

static int green_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  return get_green_component(cur_color);
}

static int blue_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  return get_blue_component(cur_color);
}

static void red_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  set_red_component(cur_color, value);
}

static void green_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  set_green_component(cur_color, value);
}

static void blue_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & (SMZX_PAL_SIZE - 1);
  set_blue_component(cur_color, value);
}

static int overlay_mode_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->overlay_mode;
}

static int mzx_speed_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->mzx_speed;
}

static void mzx_speed_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value == 0)
  {
    mzx_world->lock_speed = 0;
  }
  else if(value >= 1 && value <= 16)
  {
    mzx_world->mzx_speed = value;
    mzx_world->lock_speed = 1;
  }
}

static int overlay_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay[offset];

  return -1;
}

static int overlay_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay_color[offset];

  return -1;
}

static int smzx_mode_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_screen_mode();
}

static void smzx_mode_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  set_screen_mode(value);
}

static int smzx_r_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  return get_red_component(cur_color);
}

static int smzx_g_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  return get_green_component(cur_color);
}

static int smzx_b_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  return get_blue_component(cur_color);
}

static void smzx_r_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  set_red_component(cur_color, value);
}

static void smzx_g_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  set_green_component(cur_color, value);
}

static void smzx_b_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10) & (SMZX_PAL_SIZE - 1);
  set_blue_component(cur_color, value);
}

static int smzx_idx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int col = 0, offset = 0;
  get_counter_params(name + 8, &col, &offset);
  return get_smzx_index(col, offset);
}

static void smzx_idx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int col = 0, offset = 0;
  get_counter_params(name + 8, &col, &offset);
  set_smzx_index(col, offset, value);
}

static int smzx_message_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->smzx_message;
}

static void smzx_message_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value)
    mzx_world->smzx_message = 1;
  else
    mzx_world->smzx_message = 0;
}

static int spr_clist_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int clist_num = strtol(name + 9, NULL, 10) & (MAX_SPRITES - 1);
  return mzx_world->collision_list[clist_num];
}

static int spr_collisions_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->collision_count;
}

static int spr_num_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  // This was a signed char in versions prior to 2.70.
  // Note the 2.69c source code claims it's an unsigned char too, but it can
  // be verified that the build everyone relied on still treats it as signed.
  if(mzx_world->version < V270)
    return (signed char)mzx_world->sprite_num;

  return mzx_world->sprite_num;
}

static int spr_cx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_x;
}

static int spr_cy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_y;
}

static int spr_tcol_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->transparent_color;
}

static int spr_offset_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->offset;
}

static int spr_unbound_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->flags & SPRITE_UNBOUND ? 1 : 0;
}

static int spr_width_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->width;
}

static int spr_height_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->height;
}

static int spr_refx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->ref_x;
}

static int spr_refy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->ref_y;
}

static int spr_x_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->x;
}

static int spr_y_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->y;
}

static int spr_z_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->z;
}

static int spr_off_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];

  // This counter has existed as long as sprites have but was never
  // actually readable until 2.92.
  if(mzx_world->version >= V292 && !(cur_sprite->flags & SPRITE_INITIALIZED))
    return 1;

  return 0;
}

static int spr_offonexit_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if((mzx_world->sprite_list[spr_num])->flags & SPRITE_OFF_ON_EXIT)
    return 1;

  return 0;
}

static int spr_cwidth_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_width;
}

static int spr_cheight_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_height;
}

static void spr_num_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  // This only used the lowest byte prior to 2.92.
  if(mzx_world->version < V292)
    value &= 0xFF;

  mzx_world->sprite_num = value;
}

static void spr_yorder_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->sprite_y_order = value & 1;
}

static void spr_ccheck_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];

  if(mzx_world->version < V290)
  {  // Old (somewhat dodgy) behaviour tied to <2.90
    value %= 3;
    switch(value)
    {
      case 0:
      {
        cur_sprite->flags &= ~(SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2);
        break;
      }

      case 1:
      {
        cur_sprite->flags |= SPRITE_CHAR_CHECK;
        break;
      }

      case 2:
      {
        cur_sprite->flags |= SPRITE_CHAR_CHECK2;
        break;
      }
    }
  }
  else
  { // 2.90 makes use of both flags to allow 3 different ccheck modes
    value %= 4;
    cur_sprite->flags &= ~(SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2);
    switch(value)
    {
      case 0: break;
      case 1: cur_sprite->flags |= SPRITE_CHAR_CHECK; break;
      case 2: cur_sprite->flags |= SPRITE_CHAR_CHECK2; break;
      case 3: cur_sprite->flags |= SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2; break;
    }
  }
}

static void spr_clist_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];
  sprite_colliding_xy(mzx_world, cur_sprite, cur_sprite->x,
   cur_sprite->y);
}

static void spr_cx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (signed char) value;
  (mzx_world->sprite_list[spr_num])->col_x = value;
}

static void spr_cy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (signed char) value;
  (mzx_world->sprite_list[spr_num])->col_y = value;
}

static void spr_tcol_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->transparent_color = value;
}

static void spr_offset_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(layer_renderer_check(true))
    (mzx_world->sprite_list[spr_num])->offset = value;
}

static void spr_unbound_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(layer_renderer_check(true))
  {
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_UNBOUND;
    (mzx_world->sprite_list[spr_num])->flags |= value ? SPRITE_UNBOUND : 0;
  }
}

static void spr_height_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (char) value;
  (mzx_world->sprite_list[spr_num])->height = value;
}

static void spr_width_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (char) value;
  (mzx_world->sprite_list[spr_num])->width = value;
}

static void spr_refx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_x = value;
}

static void spr_refy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_y = value;
}

static void spr_x_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->x = value;
}

static void spr_y_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->y = value;
}

static void spr_z_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->z = value;
}

static void spr_vlayer_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_VLAYER;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_VLAYER;
}

static void spr_static_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_STATIC;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_STATIC;
}

static void spr_overlaid_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_OVER_OVERLAY;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_OVER_OVERLAY;
}

static void spr_off_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];

  // In DOS versions of MZX, this would be ignored if set to 0.
  if(value || mzx_world->version >= VERSION_PORT)
  {
    if(cur_sprite->flags & SPRITE_INITIALIZED)
    {
      // Note- versions prior to 2.92 wouldn't decrement active_sprites here.
      cur_sprite->flags &= ~SPRITE_INITIALIZED;
      mzx_world->active_sprites--;
    }
  }
}

static void spr_offonexit_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_OFF_ON_EXIT;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_OFF_ON_EXIT;
}

static void spr_swap_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *src;
  struct sprite *dest;

  value &= (MAX_SPRITES - 1);

  src = mzx_world->sprite_list[spr_num];
  dest = mzx_world->sprite_list[value];
  mzx_world->sprite_list[value] = src;
  mzx_world->sprite_list[spr_num] = dest;
}

static void spr_cwidth_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (char) value;
  (mzx_world->sprite_list[spr_num])->col_width = value;
}

static void spr_cheight_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(mzx_world->version < V290) // Before 2.90 these fields were chars.
    value = (char) value;
  (mzx_world->sprite_list[spr_num])->col_height = value;
}

static void spr_setview_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;
  int n_scroll_x, n_scroll_y;
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];
  src_board->scroll_x = 0;
  src_board->scroll_y = 0;
  calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
  if(cur_sprite->flags & SPRITE_UNBOUND)
  {
    src_board->scroll_x = ((cur_sprite->x + (signed)cur_sprite->width * 4
     - src_board->viewport_width * 4) - n_scroll_x * 8) / 8;
    src_board->scroll_y = ((cur_sprite->y + (signed)cur_sprite->height * 7
     - src_board->viewport_height * 7) - n_scroll_x * 14) / 14;
  }
  else
  {
    src_board->scroll_x =
    (cur_sprite->x + (cur_sprite->width >> 1)) -
    (src_board->viewport_width >> 1) - n_scroll_x;
    src_board->scroll_y =
    (cur_sprite->y + (cur_sprite->height >> 1)) -
    (src_board->viewport_height >> 1) - n_scroll_y;
  }
  return;
}

static int bullettype_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->bullet_type;
}

static void bullettype_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->bullet_type = value;
}

static int inputsize_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (int)mzx_world->current_board->input_size;
}

static void inputsize_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->current_board->input_size = value;
}

static int input_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->num_input;
}

static void input_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  char buf[12];
  sprintf(buf, "%d", value);
  board_set_input_string(mzx_world->current_board, buf, strlen(buf));
  mzx_world->current_board->num_input = value;
}

static int key_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  // In DOS versions key was a board counter
  if(mzx_world->version < VERSION_PORT)
  {
    return mzx_world->current_board->last_key;
  }
  return toupper(get_last_key(keycode_internal));
}

static void key_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  // In DOS versions key was a board counter
  if(mzx_world->version < VERSION_PORT)
  {
    mzx_world->current_board->last_key = CLAMP(value, 0, 255);
  }
  force_last_key(keycode_internal, toupper(value));
}

static int keyn_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int key_num = strtol(name + 3, NULL, 10);
  return get_key_status(keycode_pc_xt, key_num);
}

static int key_code_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_key(keycode_pc_xt);
}

static int key_pressed_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  // In 2.82X through 2.84X, this erroneously applied
  // numlock translations to the keycode.
  if(mzx_world->version >= V282 && mzx_world->version <= V284)
    return get_key(keycode_internal_wrt_numlock);

  return get_key(keycode_internal);
}

static int key_release_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_last_key_released(keycode_pc_xt);
}

static int key_pressedn_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int key_num = strtol(name + 11, NULL, 10);
  return get_key_status(keycode_internal, key_num) > 0;
}

static int joyn_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  char *dot_ptr;
  int joystick = strtol(name + 3, &dot_ptr, 10) - 1;
  boolean is_active;
  int16_t value;

  if(*dot_ptr == '.')
  {
    if(joystick_is_active(joystick, &is_active) && is_active &&
     joystick_get_status(joystick, dot_ptr + 1, &value))
    {
      // -1 should always mean the status is invalid or unavailable,
      // so remap a legitimate value of -1 to -2. This only affects axes,
      // which should rarely return this exact value anyway.
      if(value == -1)
        value--;

      return value;
    }
  }
  return -1;
}

static int joyn_active_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int joystick = strtol(name + 3, NULL, 10) - 1;
  boolean is_active;

  if(joystick_is_active(joystick, &is_active))
    return is_active;
  return -1;
}

static void joy_simulate_keys_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->joystick_simulate_keys = !!value;
  joystick_set_game_bindings(!!value);
}

static int scrolledx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_x;
}

static int scrolledy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_y;
}

static int timereset_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->time_limit;
}

static void timereset_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->current_board->time_limit = CLAMP(value, 0, 32767);
}

static void refresh_time(struct world *mzx_world)
{
  if(!(mzx_world->command_cache & COMMAND_CACHE_CURRENT_TIME))
  {
    mzx_world->command_cache |= COMMAND_CACHE_CURRENT_TIME;

    memset(&(mzx_world->current_time), 0, sizeof(mzx_world->current_time));
    mzx_world->current_time_epoch = 0;
    mzx_world->current_time_nano = 0;

    platform_system_time(&(mzx_world->current_time),
     &(mzx_world->current_time_epoch), &(mzx_world->current_time_nano));
  }
}

static int date_day_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_mday;
}

static int date_year_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_year + 1900;
}

static int date_month_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_mon + 1;
}

static int date_weekday_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_wday;
}

static int time_hours_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_hour;
}

static int time_minutes_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_min;
}

static int time_seconds_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time.tm_sec;
}

static int time_millis_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  refresh_time(mzx_world);
  return mzx_world->current_time_nano / 1000000;
}

static int random_seed_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int idx = strtol(name + 11, NULL, 10) % 2;
  unsigned int seed = rng_get_seed() >> (32 * idx) & 0xFFFFFFFF;
  return *((signed int *)&seed);
}

static void random_seed_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int idx = strtol(name + 11, NULL, 10) % 2;
  uint64_t seed = rng_get_seed();
  uint64_t mask = ~(0xFFFFFFFFULL << (32 * idx));
  uint64_t insert = *((unsigned int *)&value);
  seed = (seed & mask) | (insert << (32 * idx));
  rng_set_seed(seed);
}

static int vch_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    return mzx_world->vlayer_chars[offset];
  }

  return -1;
}

static void vch_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_chars[offset] = value;
  }
}

static int vco_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    return mzx_world->vlayer_colors[offset];
  }

  return -1;
}

static void vco_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_colors[offset] = value;
  }
}

/***** BOARD READING COUNTERS START *****/
static int och_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset = 0;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((board->overlay_mode > 0) && (x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return board->overlay[offset];
  }

  return -1;
}

static int oco_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset = 0;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((board->overlay_mode > 0) && (x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return board->overlay_color[offset];
  }

  return -1;
}

// vco-syle board access
static int bch_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset = 0;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return get_id_char(board, offset);
  }

  return -1;
}

static int bco_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset = 0;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return get_id_board_color(board, offset, 1); //1 == ignore_under
  }

  return -1;
}

static int bid_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return board->level_id[offset];
  }

  return -1;
}

static void bid_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  char cvalue = value;
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    if((cvalue < SENSOR) && (board->level_id[offset] < SENSOR))
      board->level_id[offset] = cvalue;
  }
}

static int bpr_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    return board->level_param[offset];
  }

  return -1;
}

static void bpr_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < board_width) && (y < board_height))
  {
    offset = x + (y * board_width);
    if(board->level_id[offset] < 122)
      board->level_param[offset] = value;
  }
}

//vco style under-board access.  Ignore all attempts if upper-board is A_UNDER
static int uch_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
    offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER))
    return get_id_under_char(board, offset);

  return -1;
}

static int uco_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
  offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER))
    return get_id_under_color(board, offset);

  return -1;
}

static int uid_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
  offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER))
    return board->level_under_id[offset];

  return -1;
}

static void uid_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  char cvalue = value;
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
  offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER) &&
   (cvalue < SENSOR) && (board->level_under_id[offset] < SENSOR))
    board->level_under_id[offset] = cvalue;
}

static int upr_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
  offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER))
    return board->level_under_param[offset];

  return -1;
}

static void upr_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *board = mzx_world->current_board;
  unsigned int x = 0, y = 0, offset;
  unsigned int board_width = board->board_width;
  unsigned int board_height = board->board_height;
  translate_coordinates(name + 3, &x, &y);
  offset = x + (y * board_width);

  if((x < board_width) && (y < board_height) &&
   !(flags[(int)board->level_id[offset]] & A_UNDER) &&
   (board->level_under_id[offset] < 122))
    board->level_under_param[offset] = value;
}

/***** END THE STUPID BOARD ACCESS COUNTERS *****/

static int char_byte_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  uint16_t char_num = get_counter(mzx_world, "CHAR", id);
  uint8_t byte_num = get_counter(mzx_world, "BYTE", id);

  // Prior to 2.90 char params are clipped.
  if(mzx_world->version < V290)
    char_num &= 0xFF;

  if(byte_num >= 14)
  {
    // Old port releases are missing a bounds check (see: Day of Zeux Invitation).
    if(mzx_world->version >= VERSION_PORT && mzx_world->version < V292)
      char_num += byte_num / 14;

    byte_num %= 14;
  }

  return ec_read_byte(char_num, byte_num);
}

static void char_byte_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  uint16_t char_num = get_counter(mzx_world, "CHAR", id);
  uint8_t byte_num = get_counter(mzx_world, "BYTE", id);

  // Prior to 2.90 char params are clipped.
  if(mzx_world->version < V290)
    char_num &= 0xFF;

  if(byte_num >= 14)
  {
    // Old port releases are missing a bounds check (see: Day of Zeux Invitation).
    // This may write the byte into the extended charsets, which doesn't really
    // matter for these games (and is better than corrupting the protected set).
    if(mzx_world->version >= VERSION_PORT && mzx_world->version < V292)
      char_num += byte_num / 14;

    byte_num %= 14;
  }

  ec_change_byte(char_num, byte_num, value);
}

static int pixel_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int pixel_x = CLAMP(get_counter(mzx_world, "CHAR_X", id), 0, 255);
  int pixel_y = CLAMP(get_counter(mzx_world, "CHAR_Y", id), 0, 111);
  char sub_x, sub_y, current_byte, current_char;
  int pixel_mask;

  sub_x = pixel_x % 8;
  sub_y = pixel_y % 14;
  current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
  current_byte = ec_read_byte(current_char, sub_y);
  pixel_mask = (128 >> sub_x);

  return (current_byte & pixel_mask) >> (7 - sub_x);
}

static void pixel_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int pixel_x = CLAMP(get_counter(mzx_world, "CHAR_X", id), 0, 255);
  int pixel_y = CLAMP(get_counter(mzx_world, "CHAR_Y", id), 0, 111);
  char sub_x, sub_y, current_byte, current_char;

  sub_x = pixel_x & 7;
  sub_y = pixel_y % 14;
  current_char = ((pixel_y / 14) << 5) + (pixel_x >> 3);
  current_byte = ec_read_byte(current_char, sub_y);

  switch(value)
  {
    case 0:
    {
      //clear
      current_byte &= ~(128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
    case 1:
    {
      //set
      current_byte |= (128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
    case 2:
    {
      //toggle
      current_byte ^= (128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
  }
  return;
}

static int int2bin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int place = get_counter(mzx_world, "BIT_PLACE", id) & 15;
  return ((get_counter(mzx_world, "INT", id) >> place) & 1);
}

static void int2bin_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int integer;
  int place;
  place = (get_counter(mzx_world, "BIT_PLACE", id) & 15);
  integer = get_counter(mzx_world, "INT", id);

  switch(value & 2)
  {
    case 0:
    {
      // clear
      integer &= ~(1 << place);
      break;
    }

    case 1:
    {
      // set
      integer |= (1 << place);
      break;
    }

    case 2:
    {
      // toggle
      integer ^= (1 << place);
      break;
    }
  }

  set_counter(mzx_world, "INT", integer, id);
}

static int commands_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->commands;
}

static void commands_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->commands = value;
}

static int commands_stop_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->commands_stop;
}

static void commands_stop_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->commands_stop = value;
}

static int fread_open_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FREAD;
  return 0;
}

static int fwrite_open_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FWRITE;
  return 0;
}

static int fwrite_append_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FAPPEND;
  return 0;
}

static int fwrite_modify_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FMODIFY;
  return 0;
}

static int smzx_palette_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SMZX_PALETTE;
  return 0;
}

static int smzx_indices_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SMZX_INDICES;
  return 0;
}

static void exit_game_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value)
  {
    // Signal the main loop that the game state should change.
    mzx_world->change_game_state = CHANGE_STATE_EXIT_GAME_ROBOTIC;
  }
}

static void play_game_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value && get_config()->standalone_mode)
  {
    // Signal the main loop that the game state should change.
    mzx_world->change_game_state = CHANGE_STATE_PLAY_GAME_ROBOTIC;
  }
}

static int load_game_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_GAME;
  return 0;
}

static int save_game_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_GAME;
  return 0;
}

static int load_counters_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_COUNTERS;
  return 0;
}

static int save_counters_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_COUNTERS;
  return 0;
}

static int load_robot_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

static int load_bc_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_BC;
  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

static int save_robot_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

static int save_bc_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_BC;
  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

static int fread_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(!mzx_world->input_is_dir && mzx_world->input_file)
    return vfgetc(mzx_world->input_file);
  return -1;
}

static int fread_counter_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(!mzx_world->input_is_dir && mzx_world->input_file)
  {
    if(mzx_world->version < V282)
      return vfgetw(mzx_world->input_file);
    else
      return vfgetd(mzx_world->input_file);
  }
  return -1;
}

static int fread_pos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(!mzx_world->input_is_dir && mzx_world->input_file)
  {
    int64_t pos = vftell(mzx_world->input_file);
    return CLAMP(pos, -1, INT_MAX);
  }
  else

  if(mzx_world->input_is_dir)
  {
    return vdir_tell(mzx_world->input_directory);
  }
  else
    return -1;
}

static void fread_pos_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(!mzx_world->input_is_dir && mzx_world->input_file)
  {
    if(value == -1)
      vfseek(mzx_world->input_file, 0, SEEK_END);
    else
      vfseek(mzx_world->input_file, value, SEEK_SET);
  }
  else if(mzx_world->input_is_dir)
  {
    if(value >= 0)
      vdir_seek(mzx_world->input_directory, value);
  }
}

static int fread_length_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->input_is_dir)
    return vdir_length(mzx_world->input_directory);

  if(mzx_world->input_file)
  {
    int64_t len = vfilelength(mzx_world->input_file, false);
    return CLAMP(len, -1, INT_MAX);
  }

  return -1;
}

static void fread_delim_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->fread_delimiter = value % 256;
}

static void fwrite_delim_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->fwrite_delimiter = value % 256;
}

static int fwrite_pos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->output_file)
  {
    int64_t pos = vftell(mzx_world->output_file);
    return CLAMP(pos, -1, INT_MAX);
  }
  else
    return -1;
}

static void fwrite_pos_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
  {
    if(value == -1)
      vfseek(mzx_world->output_file, 0, SEEK_END);
    else
      vfseek(mzx_world->output_file, value, SEEK_SET);
  }
}

static void fwrite_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
    vfputc(value, mzx_world->output_file);
}

static void fwrite_counter_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
  {
    if(mzx_world->version < V282)
      vfputw(value, mzx_world->output_file);
    else
      vfputd(value, mzx_world->output_file);
  }
}

static int fwrite_length_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->output_file)
  {
    int64_t len = vfilelength(mzx_world->output_file, false);
    return CLAMP(len, -1, INT_MAX);
  }
  return -1;
}

static int lava_walk_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->can_lavawalk;
}

static void lava_walk_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->can_lavawalk = value;
}

static int goop_walk_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->can_goopwalk;
}

static void goop_walk_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->can_goopwalk = value;
}

static int robot_id_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return id;
}

static int robot_id_n_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 9);
}

static int rid_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 3);
}

static int r_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  char *next;
  unsigned int robot_num = strtol(name + 1, &next, 10);

  if((robot_num <= (unsigned int)src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
  {
    return get_counter(mzx_world, next + 1, robot_num);
  }

  return -1;
}

static void r_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;
  char *next;
  unsigned int robot_num = strtol(name + 1, &next, 10);

  if((robot_num <= (unsigned int)src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
  {
    set_counter(mzx_world, next + 1, value, robot_num);
  }
}

static int viewport_x_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->viewport_x;
}

static int viewport_y_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->viewport_y;
}

static int viewport_width_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->viewport_width;
}

static int viewport_height_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->viewport_height;
}

static int vlayer_size_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_size;
}

static int vlayer_height_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_height;
}

static int vlayer_width_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_width;
}

static void vlayer_size_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value <= 0)
    value = 1;

  if(value <= MAX_BOARD_SIZE)
  {
    int vlayer_width = mzx_world->vlayer_width;
    int vlayer_height = mzx_world->vlayer_height;
    int old_size = mzx_world->vlayer_size;

    mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, value);
    mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, value);
    mzx_world->vlayer_size = value;

    if(old_size > value)
    {
      vlayer_height = value / vlayer_width;
      mzx_world->vlayer_height = vlayer_height;

      if(vlayer_height == 0)
      {
        mzx_world->vlayer_width = value;
        mzx_world->vlayer_height = 1;
      }
    }
    else

    if(old_size < value && mzx_world->version >= V291)
    {
      // Clear new vlayer area (2.91+)
      memset(mzx_world->vlayer_chars + old_size, 32, value - old_size);
      memset(mzx_world->vlayer_colors + old_size, 7, value - old_size);
    }
  }
}

static void vlayer_height_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int new_width;
  int new_height;

  if(value <= 0)
    value = 1;
  if(value > mzx_world->vlayer_size)
    value = mzx_world->vlayer_size;

  new_width = mzx_world->vlayer_size / value;
  new_height = value;

  if(mzx_world->version >= V291)
  {
    // Manipulate vlayer data so its contents change little as possible (2.91+)
    remap_vlayer(mzx_world, new_width, new_height);
  }
  else
  {
    mzx_world->vlayer_width = new_width;
    mzx_world->vlayer_height = new_height;
  }
}

static void vlayer_width_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int new_width;
  int new_height;

  if(value <= 0)
    value = 1;
  if(value > mzx_world->vlayer_size)
    value = mzx_world->vlayer_size;

  new_width = value;
  new_height = mzx_world->vlayer_size / value;

  if(mzx_world->version >= V291)
  {
    // Manipulate vlayer data so its contents change little as possible (2.91+)
    remap_vlayer(mzx_world, new_width, new_height);
  }
  else
  {
    mzx_world->vlayer_width = new_width;
    mzx_world->vlayer_height = new_height;
  }
}

static int buttons_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int buttons_formatted = 0;

  // This is sufficient for the buttons.
  int raw_status = get_mouse_status();

  // Wheel presses are immediately released. We need this too.
  int raw_status_ext = get_mouse_press_ext();

  /* Since button values are inconsistent even for the same SDL
   * version across platforms, and we also want right to be the
   * second bit and middle the third, we'll map every button.
   */

  if(raw_status & MOUSE_BUTTON(MOUSE_BUTTON_LEFT))
    buttons_formatted |= MOUSE_BUTTON(1);

  if(raw_status & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
    buttons_formatted |= MOUSE_BUTTON(2);

  if(raw_status & MOUSE_BUTTON(MOUSE_BUTTON_MIDDLE))
    buttons_formatted |= MOUSE_BUTTON(3);

  // For 2.90+, also map the wheel and side buttons
  if(mzx_world->version >= V290)
  {
    if(raw_status_ext == MOUSE_BUTTON_WHEELUP)
      buttons_formatted |= MOUSE_BUTTON(4);

    if(raw_status_ext == MOUSE_BUTTON_WHEELDOWN)
      buttons_formatted |= MOUSE_BUTTON(5);

    if(raw_status_ext == MOUSE_BUTTON_WHEELLEFT)
      buttons_formatted |= MOUSE_BUTTON(6);

    if(raw_status_ext == MOUSE_BUTTON_WHEELRIGHT)
      buttons_formatted |= MOUSE_BUTTON(7);

    if(raw_status & MOUSE_BUTTON(MOUSE_BUTTON_X1))
      buttons_formatted |= MOUSE_BUTTON(8);

    if(raw_status & MOUSE_BUTTON(MOUSE_BUTTON_X2))
      buttons_formatted |= MOUSE_BUTTON(9);
  }

  return buttons_formatted;
}

static int mousex_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_x();
}

static int mousey_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_y();
}

static void mousex_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 79)
    value = 79;

  if(value < 0)
    value = 0;

  warp_mouse_x(value);
}

static void mousey_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 24)
    value = 24;

  if(value < 0)
    value = 0;

  warp_mouse_y(value);
}

static int mousepx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_pixel_x();
}

static int mousepy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_pixel_y();
}

static void mousepx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 639)
    value = 639;

  if(value < 0)
    value = 0;

  warp_mouse_pixel_x(value);
}

static void mousepy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 349)
    value = 349;

  if(value < 0)
    value = 0;

  warp_mouse_pixel_y(value);
}

static int mboardx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_x() +
   scroll_x - mzx_world->current_board->viewport_x;
}

static int mboardy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_y() +
   scroll_y - mzx_world->current_board->viewport_y;
}

static void spacelock_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->bi_shoot_status = value & 1;
}

static void bimesg_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->bi_mesg_status = value & 1;
}

static int mod_order_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_order();
}

static void mod_order_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  audio_set_module_order(value);
}

static int mod_position_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_position();
}

static void mod_position_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  audio_set_module_position(value);
}

static int mod_length_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_length();
}

static int mod_loopend_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_loop_end();
}

static void mod_loopend_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value >= 0)
    audio_set_module_loop_end(value);
}

static int mod_loopstart_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_loop_start();
}

static void mod_loopstart_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value >= 0)
    audio_set_module_loop_start(value);
}

static int mod_freq_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_module_frequency();
}

static void mod_freq_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if((value == 0) || ((value >= 16) && (value <= (2 << 20))))
    audio_set_module_frequency(value);
}

static int max_samples_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return audio_get_max_samples();
}

static void max_samples_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  // -1 for unlimited samples, or 0+ for an explicit number of samples
  value = MAX(-1, value);

  mzx_world->max_samples = value;
  audio_set_max_samples(value);
}

// Make sure this is in the right alphabetical (non-case sensitive) order
// ? specifies 0 or more numbers
// ! specifies 1 or more numbers
// * skips any characters at or after this point, including a null terminator
// This must be sorted in a case insensitive way, that has special applications
// for non-letters:
// _ comes after letters
// ? comes before letters
// numbers come before letters (and before ?)

/* FIXME: "MOD *" will still work, even in worlds < 2.51s1 */

/* NOTE: FREAD_LENGTH and FWRITE_LENGTH were removed in 2.65 and no
 *       compatibility layer was implemented. The reimplementation of these
 *       counters is versioned to 2.92.
 *       ABS_VALUE, R_PLAYERDIST, SQRT_VALUE, WRAP, VALUE were removed in
 *       2.68 and no compatibility layer was implemented.
 *       FREAD_PAGE, FWRITE_PAGE were removed in 2.80 and no compatibility
 *       layer was implemented.
 *       SAVE_WORLD was removed in 2.90 and no compatibility layer was
 *       implemented.
 */

/* NOTE: fread_counter/fwrite_counter read only a single DOS word (16bit)
 *       in MZX versions prior to 2.82. Newer versions read in a single
 *       DOS dword (32bit). Since the counter was introduced in versions
 *       prior to 2.82, it is not versioned as such below (see the read/write
 *       functions for the exact semantics).
 */

/* NOTE: Compatibility with worlds made between 2.51s2 and 2.61 (inclusive)
 *       is only partial, due to the MZX/SAV magic not being incremented
 *       in these versions. This is unfortunately not fixable.
 */

static const struct function_counter builtin_counters[] =
{
  { "$*",               V262,   string_counter_read,  string_counter_write },
  { "abs!",             V268,   abs_read,             NULL },
  { "acos!",            V268,   acos_read,            NULL },
  { "arctan!,!",        V284,   atan2_read,           NULL },
  { "asin!",            V268,   asin_read,            NULL },
  { "atan!",            V268,   atan_read,            NULL },
  { "bch!,!",           V284,   bch_read,             NULL },
  { "bco!,!",           V284,   bco_read,             NULL },
  { "bid!,!",           V284,   bid_read,             bid_write },
  { "bimesg",           V251s3, NULL,                 bimesg_write }, // s3.2
  { "blue_value",       V260,   blue_value_read,      blue_value_write },
  { "board_char",       V260,   board_char_read,      NULL },
  { "board_color",      V260,   board_color_read,     NULL },
  { "board_h",          V265,   board_h_read,         NULL },
  { "board_id",         V265,   board_id_read,        board_id_write },
  { "board_param",      V265,   board_param_read,     board_param_write },
  { "board_w",          V265,   board_w_read,         NULL },
  { "bpr!,!",           V284,   bpr_read,             bpr_write },
  { "bullettype",       V100,   bullettype_read,      bullettype_write },
  { "buttons",          V251s1, buttons_read,         NULL },
  { "char_byte",        V260,   char_byte_read,       char_byte_write },
  { "commands",         V260,   commands_read,        commands_write },
  { "commands_stop",    V290,   commands_stop_read,   commands_stop_write },
  { "cos!",             V268,   cos_read,             NULL },
  { "c_divisions",      V268,   c_divisions_read,     c_divisions_write },
  { "date_day",         V260,   date_day_read,        NULL },
  { "date_month",       V260,   date_month_read,      NULL },
  { "date_weekday",     V293,   date_weekday_read,    NULL },
  { "date_year",        V260,   date_year_read,       NULL },
  { "divider",          V268,   divider_read,         divider_write },
  { "exit_game",        V290,   NULL,                 exit_game_write },
  { "fread",            V260,   fread_read,           NULL },
  { "fread_counter",    V265,   fread_counter_read,   NULL },
  { "fread_delimiter",  V284,   NULL,                 fread_delim_write },
  { "fread_length",     V292,   fread_length_read,    NULL },
  { "fread_open",       V260,   fread_open_read,      NULL },
  { "fread_pos",        V260,   fread_pos_read,       fread_pos_write },
  { "fwrite",           V260,   NULL,                 fwrite_write },
  { "fwrite_append",    V260,   fwrite_append_read,   NULL },
  { "fwrite_counter",   V265,   NULL,                 fwrite_counter_write },
  { "fwrite_delimiter", V284,   NULL,                 fwrite_delim_write },
  { "fwrite_length",    V292,   fwrite_length_read,   NULL },
  { "fwrite_modify",    V269c,  fwrite_modify_read,   NULL },
  { "fwrite_open",      V260,   fwrite_open_read,     NULL },
  { "fwrite_pos",       V260,   fwrite_pos_read,      fwrite_pos_write },
  { "goop_walk",        V290,   goop_walk_read,       goop_walk_write },
  { "green_value",      V260,   green_value_read,     green_value_write },
  { "horizpld",         V100,   horizpld_read,        NULL },
  { "input",            V100,   input_read,           input_write },
  { "inputsize",        V100,   inputsize_read,       inputsize_write },
  { "int2bin",          V260,   int2bin_read,         int2bin_write },
  { "joy!.*",           V292,   joyn_read,            NULL },
  { "joy!active",       V292,   joyn_active_read,     NULL },
  { "joy_simulate_keys",V292,   NULL,                 joy_simulate_keys_write },
  { "key",              V100,   key_read,             key_write },
  { "key!",             V269,   keyn_read,            NULL },
  { "key_code",         V269,   key_code_read,        NULL },
  { "key_pressed",      V260,   key_pressed_read,     NULL },
  { "key_pressed!",     V293,   key_pressedn_read,    NULL },
  { "key_release",      V269,   key_release_read,     NULL },
  { "lava_walk",        V260,   lava_walk_read,       lava_walk_write },
  { "load_bc?",         V270,   load_bc_read,         NULL },
  { "load_counters",    V290,   load_counters_read,   NULL },
  { "load_game",        V268,   load_game_read,       NULL },
  { "load_robot?",      V270,   load_robot_read,      NULL },
  { "local",            V251s1, local_read,           local_write },
  { "local!",           V269b,  localn_read,          localn_write },
  { "loopcount",        V200,   loopcount_read,       loopcount_write },
  { "max!,!",           V284,   maxval_read,          NULL },
  { "max_samples",      V291,   max_samples_read,     max_samples_write },
  { "mboardx",          V251s1, mboardx_read,         NULL },
  { "mboardy",          V251s1, mboardy_read,         NULL },
  { "min!,!",           V284,   minval_read,          NULL },
  { "mod_frequency",    V281,   mod_freq_read,        mod_freq_write },
  { "mod_length",       V291,   mod_length_read,      NULL },
  { "mod_loopend",      V292,   mod_loopend_read,     mod_loopend_write },
  { "mod_loopstart",    V292,   mod_loopstart_read,   mod_loopstart_write },
  { "mod_order",        V262,   mod_order_read,       mod_order_write },
  { "mod_position",     V281,   mod_position_read,    mod_position_write },
  { "mousepx",          V282,   mousepx_read,         mousepx_write },
  { "mousepy",          V282,   mousepy_read,         mousepy_write },
  { "mousex",           V251s1, mousex_read,          mousex_write },
  { "mousey",           V251s1, mousey_read,          mousey_write },
  { "multiplier",       V268,   multiplier_read,      multiplier_write },
  { "mzx_speed",        V260,   mzx_speed_read,       mzx_speed_write },
  { "och!,!",           V284,   och_read,             NULL },
  { "oco!,!",           V284,   oco_read,             NULL },
  { "overlay_char",     V260,   overlay_char_read,    NULL },
  { "overlay_color",    V260,   overlay_color_read,   NULL },
  { "overlay_mode",     V260,   overlay_mode_read,    NULL },
  { "pixel",            V260,   pixel_read,           pixel_write },
  { "playerdist",       V100,   playerdist_read,      NULL },
  { "playerfacedir",    V200,   playerfacedir_read,   playerfacedir_write },
  { "playerlastdir",    V200,   playerlastdir_read,   playerlastdir_write },
  { "playerx",          V251s1, playerx_read,         NULL },
  { "playery",          V251s1, playery_read,         NULL },
  { "play_game",        V290,   NULL,                 play_game_write },
  { "r!.*",             V265,   r_read,               r_write },
  { "random_seed!",     V291,   random_seed_read,     random_seed_write },
  { "red_value",        V260,   red_value_read,       red_value_write },
  { "rid*",             V269b,  rid_read,             NULL },
  { "robot_id",         V260,   robot_id_read,        NULL },
  { "robot_id_*",       V265,   robot_id_n_read,      NULL },
  { "save_bc?",         V270,   save_bc_read,         NULL },
  { "save_counters",    V290,   save_counters_read,   NULL },
  { "save_game",        V268,   save_game_read,       NULL },
  { "save_robot?",      V270,   save_robot_read,      NULL },
  { "scrolledx",        V251s1, scrolledx_read,       NULL },
  { "scrolledy",        V251s1, scrolledy_read,       NULL },
  { "sin!",             V268,   sin_read,             NULL },
  { "smzx_b!",          V269,   smzx_b_read,          smzx_b_write },
  { "smzx_g!",          V269,   smzx_g_read,          smzx_g_write },
  { "smzx_idx!,!",      V290,   smzx_idx_read,        smzx_idx_write },
  { "smzx_indices",     V291,   smzx_indices_read,    NULL },
  { "smzx_message",     V291,   smzx_message_read,    smzx_message_write },
  { "smzx_mode",        V269,   smzx_mode_read,       smzx_mode_write },
  { "smzx_palette",     V269,   smzx_palette_read,    NULL },
  { "smzx_r!",          V269,   smzx_r_read,          smzx_r_write },
  { "spacelock",        V284,   NULL,                 spacelock_write },
  { "spr!_ccheck",      V265,   NULL,                 spr_ccheck_write },
  { "spr!_cheight",     V265,   spr_cheight_read,     spr_cheight_write },
  { "spr!_clist",       V265,   NULL,                 spr_clist_write },
  { "spr!_cwidth",      V265,   spr_cwidth_read,      spr_cwidth_write },
  { "spr!_cx",          V265,   spr_cx_read,          spr_cx_write },
  { "spr!_cy",          V265,   spr_cy_read,          spr_cy_write },
  { "spr!_height",      V265,   spr_height_read,      spr_height_write },
  { "spr!_off",         V265,   spr_off_read,         spr_off_write },
  { "spr!_offonexit",   V293,   spr_offonexit_read,   spr_offonexit_write },
  { "spr!_offset",      V290,   spr_offset_read,      spr_offset_write },
  { "spr!_overlaid",    V265,   NULL,                 spr_overlaid_write },
  { "spr!_overlay",     V269c,  NULL,                 spr_overlaid_write },
  { "spr!_refx",        V265,   spr_refx_read,        spr_refx_write },
  { "spr!_refy",        V265,   spr_refy_read,        spr_refy_write },
  { "spr!_setview",     V265,   NULL,                 spr_setview_write },
  { "spr!_static",      V265,   NULL,                 spr_static_write },
  { "spr!_swap",        V265,   NULL,                 spr_swap_write },
  { "spr!_tcol",        V290,   spr_tcol_read,        spr_tcol_write },
  { "spr!_unbound",     V290,   spr_unbound_read,     spr_unbound_write },
  { "spr!_vlayer",      V269c,  NULL,                 spr_vlayer_write },
  { "spr!_width",       V265,   spr_width_read,       spr_width_write },
  { "spr!_x",           V265,   spr_x_read,           spr_x_write },
  { "spr!_y",           V265,   spr_y_read,           spr_y_write },
  { "spr!_z",           V292,   spr_z_read,           spr_z_write },
  { "spr_clist!",       V265,   spr_clist_read,       NULL },
  { "spr_collisions",   V265,   spr_collisions_read,  NULL },
  { "spr_num",          V265,   spr_num_read,         spr_num_write },
  { "spr_yorder",       V265,   NULL,                 spr_yorder_write },
  { "sqrt!",            V268,   sqrt_read,            NULL },
  { "tan!",             V268,   tan_read,             NULL },
  { "thisx",            V200,   thisx_read,           NULL },
  { "thisy",            V200,   thisy_read,           NULL },
  { "this_char",        V260,   this_char_read,       NULL },
  { "this_color",       V260,   this_color_read,      NULL },
  { "timereset",        V200,   timereset_read,       timereset_write },
  { "time_hours",       V260,   time_hours_read,      NULL },
  { "time_millis",      V292,   time_millis_read,     NULL },
  { "time_minutes",     V260,   time_minutes_read,    NULL },
  { "time_seconds",     V260,   time_seconds_read,    NULL },
  { "uch!,!",           V284,   uch_read,             NULL },
  { "uco!,!",           V284,   uco_read,             NULL },
  { "uid!,!",           V284,   uid_read,             uid_write },
  { "upr!,!",           V284,   upr_read,             upr_write },
  { "vch!,!",           V269c,  vch_read,             vch_write },
  { "vco!,!",           V269c,  vco_read,             vco_write },
  { "vertpld",          V100,   vertpld_read,         NULL },
  { "viewport_height",  V293,   viewport_height_read, NULL },
  { "viewport_width",   V293,   viewport_width_read,  NULL },
  { "viewport_x",       V293,   viewport_x_read,      NULL },
  { "viewport_y",       V293,   viewport_y_read,      NULL },
  { "vlayer_height",    V269c,  vlayer_height_read,   vlayer_height_write },
  { "vlayer_size",      V281,   vlayer_size_read,     vlayer_size_write },
  { "vlayer_width",     V269c,  vlayer_width_read,    vlayer_width_write },
};

static const int num_builtin_counters = ARRAY_SIZE(builtin_counters);
static int counter_first_letter[512];

void counter_fsg(void)
{
  char cur_char = (builtin_counters[0]).name[0];
  char old_char;
  int i, i2;

  for(i = 0, i2 = 0; i < 256; i++)
  {
    if(i != cur_char)
    {
      counter_first_letter[i * 2] = -1;
      counter_first_letter[(i * 2) + 1] = -1;
    }
    else
    {
      counter_first_letter[i * 2] = i2;
      old_char = cur_char;

      while(cur_char == old_char)
      {
        i2++;
        if(i2 == num_builtin_counters)
          break;
        cur_char = builtin_counters[i2].name[0];
      }

      counter_first_letter[(i * 2) + 1] = i2 - 1;
    }
  }
}

int match_function_counter(const char *dest, const char *src)
{
  int difference;
  char cur_src, cur_dest;

  while(1)
  {
    cur_src = *src;
    cur_dest = *dest;

    // Skip 1 or more letters
    switch(cur_src)
    {
      // Make sure the first character is a number
      case '!':
      {
        if(((cur_dest < '0') || (cur_dest > '9')) &&
         (cur_dest != '-'))
        {
          // Don't want to accidentally match this char...
          if(cur_dest == '!')
            return 1;

          break;
        }

        dest++;
        cur_dest = *dest;

        // Fall through, skip remaining numbers
      }

      /* fallthrough */

      // Skip 0 or more number characters
      case '?':
      {
        src++;
        cur_src = *src;

        while(((cur_dest >= '0') && (cur_dest <= '9')) ||
         (cur_dest == '-'))
        {
          dest++;
          cur_dest = *dest;
        }
        break;
      }

      // Match anything, instant winner
      case '*':
      {
        return 0;
      }
    }

    if((cur_src | cur_dest) == 0)
    {
      // Both hit the null terminator
      return 0;
    }

    difference = (int)((cur_dest & 0xDF) - (cur_src & 0xDF));

    if(difference)
      return difference;

    src++;
    dest++;
  }

  return 0;
}

static const struct function_counter *find_function_counter(const char *name)
{
  const struct function_counter *base = builtin_counters;
  int first_letter = memtolower((unsigned char)name[0]) * 2;
  int bottom, top, middle;
  int cmpval;

  bottom = counter_first_letter[first_letter];
  top = counter_first_letter[first_letter + 1];

  if(bottom != -1)
  {
    while(bottom <= top)
    {
      middle = (top + bottom) / 2;
      cmpval = match_function_counter(name + 1, (base[middle]).name + 1);

      if(cmpval > 0)
        bottom = middle + 1;
      else

      if(cmpval < 0)
        top = middle - 1;
      else
        return base + middle;
    }
  }

  return NULL;
}

static struct robot *get_robot_by_id(struct world *mzx_world, int id)
{
  if(id >= 0 && id <= mzx_world->current_board->num_robots)
    return mzx_world->current_board->robot_list[id];
  else
    return NULL;
}

static void fread_close(struct world *mzx_world)
{
  if(!mzx_world->input_is_dir && mzx_world->input_file)
    vfclose(mzx_world->input_file);

  if(mzx_world->input_is_dir && mzx_world->input_directory)
    vdir_close(mzx_world->input_directory);

  mzx_world->input_file_name[0] = '\0';
  mzx_world->input_file = NULL;
  mzx_world->input_directory = NULL;
  mzx_world->input_is_dir = false;
}

static void fwrite_close(struct world *mzx_world)
{
  if(mzx_world->output_file)
    vfclose(mzx_world->output_file);

  mzx_world->output_file_name[0] = '\0';
  mzx_world->output_file = NULL;
  mzx_world->output_mode = FWRITE_MODE_UNKNOWN;
}

int set_counter_special(struct world *mzx_world, char *char_value,
 int value, int id)
{
  struct robot *cur_robot = get_robot_by_id(mzx_world, id);

  if(strlen(char_value) >= MAX_PATH)
    return 0; // haha nope

  switch(mzx_world->special_counter_return)
  {
    case FOPEN_FREAD:
    {
      fread_close(mzx_world);

      if(char_value[0])
      {
        char *translated_path = (char *)cmalloc(MAX_PATH);
        int err;

        if(!translated_path)
          return 0;

        err = fsafetranslate(char_value, translated_path, MAX_PATH);

        if(err == -FSAFE_MATCHED_DIRECTORY)
        {
          mzx_world->input_directory = vdir_open(translated_path);
          if(mzx_world->input_directory)
            mzx_world->input_is_dir = true;
        }
        else

        if(err == -FSAFE_SUCCESS)
          mzx_world->input_file = vfopen_unsafe(translated_path, "rb");

        if(mzx_world->input_file || mzx_world->input_is_dir)
          strcpy(mzx_world->input_file_name, translated_path);

        free(translated_path);
      }
      break;
    }

    case FOPEN_FWRITE:
    {
      fwrite_close(mzx_world);

      if(char_value[0])
      {
        mzx_world->output_file = fsafeopen(char_value, "wb");
        if(mzx_world->output_file)
        {
          strcpy(mzx_world->output_file_name, char_value);
          mzx_world->output_mode = FWRITE_MODE_TRUNCATE;
        }
      }
      break;
    }

    case FOPEN_FAPPEND:
    {
      fwrite_close(mzx_world);

      if(char_value[0])
      {
        mzx_world->output_file = fsafeopen(char_value, "ab");
        if(mzx_world->output_file)
        {
          strcpy(mzx_world->output_file_name, char_value);
          mzx_world->output_mode = FWRITE_MODE_APPEND;
        }
      }
      break;
    }

    case FOPEN_FMODIFY:
    {
      fwrite_close(mzx_world);

      if(char_value[0])
      {
        mzx_world->output_file = fsafeopen(char_value, "r+b");
        if(mzx_world->output_file)
        {
          strcpy(mzx_world->output_file_name, char_value);
          mzx_world->output_mode = FWRITE_MODE_MODIFY;
        }
      }
      break;
    }

    case FOPEN_SMZX_PALETTE:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      err = fsafetranslate(char_value, translated_path, MAX_PATH);

      if(err == -FSAFE_SUCCESS)
        load_palette(translated_path);

      free(translated_path);
      break;
    }

    case FOPEN_SMZX_INDICES:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      err = fsafetranslate(char_value, translated_path, MAX_PATH);

      if(err == -FSAFE_SUCCESS)
        load_index_file(translated_path);

      free(translated_path);
      break;
    }

    case FOPEN_SAVE_COUNTERS:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      err = fsafetranslate(char_value, translated_path, MAX_PATH);
      if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
        save_counters_file(mzx_world, translated_path);

      free(translated_path);
      break;
    }

    case FOPEN_LOAD_COUNTERS:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      err = fsafetranslate(char_value, translated_path, MAX_PATH);
      if(err == -FSAFE_SUCCESS)
        load_counters_file(mzx_world, translated_path);

      free(translated_path);
      break;
    }

    case FOPEN_SAVE_GAME:
    {
      char *translated_path;
      int err;

      if(mzx_world->version < V290)
      {
        // Prior to 2.90, save_game took place immediately
        translated_path = cmalloc(MAX_PATH);
        if(cur_robot)
        {
          // Advance the program so that loading a SAV doesn't re-run this line
          cur_robot->cur_prog_line +=
          cur_robot->program_bytecode[cur_robot->cur_prog_line] + 2;
        }

        err = fsafetranslate(char_value, translated_path, MAX_PATH);
        if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
          save_world(mzx_world, translated_path, true, MZX_VERSION);

        free(translated_path);
      }
      else
      {
        // From 2.90 onward, save_game takes place at the end
        // of the cycle
        mzx_world->robotic_save_type = SAVE_NONE;
        err = fsafetranslate(char_value, mzx_world->robotic_save_path, MAX_PATH);
        if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
          mzx_world->robotic_save_type = SAVE_GAME;
      }
      break;
    }

    case FOPEN_LOAD_GAME:
    {
      char *translated_path = cmalloc(MAX_PATH);
      boolean faded;

      if(!fsafetranslate(char_value, translated_path, MAX_PATH))
      {
        if(reload_savegame(mzx_world, translated_path, &faded))
        {
          if(faded)
            insta_fadeout();
          else
            insta_fadein();

          // Let the main loop know we're loading a save from Robotic
          mzx_world->change_game_state = CHANGE_STATE_LOAD_GAME_ROBOTIC;
        }
      }

      free(translated_path);
      break;
    }

#ifdef CONFIG_DEBYTECODE

    case FOPEN_LOAD_ROBOT:
    {
      // Load legacy source code.

      if(value >= 0)
        cur_robot = get_robot_by_id(mzx_world, value);

      if(cur_robot)
      {
        int new_length = 0;
        char *new_source = NULL;

        // Source world? Assume new source. Otherwise, assume old source.
        // TODO issues caused by this will be resolved when these counters get
        // translated into actual commands eventually.
        if(mzx_world->version >= VERSION_SOURCE)
        {
          vfile *vf = fsafeopen(char_value, "rb");
          if(vf)
          {
            int64_t vf_len = vfilelength(vf, true);
            if(vf_len < 0 || vf_len > INT_MAX)
            {
              vfclose(vf);
              break;
            }

            new_length = (int)vf_len;
            new_source = cmalloc(new_length + 1);
            if(!new_source)
            {
              vfclose(vf);
              break;
            }
            new_source[new_length] = 0;

            if(vfread(new_source, 1, new_length, vf) != (size_t)new_length)
            {
              free(new_source);
              new_source = NULL;
            }
            vfclose(vf);
          }
        }
        else
          new_source = legacy_convert_file(char_value, &new_length,
           SAVE_ROBOT_DISASM_EXTRAS, SAVE_ROBOT_DISASM_BASE);

        if(new_source)
        {
          if(cur_robot->program_source)
            free(cur_robot->program_source);

          cur_robot->program_source = new_source;
          cur_robot->program_source_length = new_length;

          // TODO: Move this outside of here.
          if(cur_robot->program_bytecode)
          {
            free(cur_robot->program_bytecode);
            cur_robot->program_bytecode = NULL;
          }

          cur_robot->stack_pointer = 0;
          cur_robot->cur_prog_line = 1;
          prepare_robot_bytecode(mzx_world, cur_robot);

          // Restart this robot if either it was just a LOAD_ROBOT
          // OR LOAD_ROBOTn was used where n is &robot_id&.
          if(value == -1 || value == id)
            return 1;
        }
      }
      break;
    }

    case FOPEN_LOAD_BC:
    {
      // Although this pseudo-command is still usable, it'll no longer be
      // possible to actually export bytecode. So this means that the bytecode
      // is always from a version that legacy_disassemble_program

      // It's basically like LOAD_ROBOT except that it first has to disassmble
      // the bytecode.

      vfile *bc_file = fsafeopen(char_value, "rb");

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          int64_t bc_len = vfilelength(bc_file, true);
          char *program_legacy_bytecode;
          int program_bytecode_length;

          if(bc_len < 0 || bc_len > MAX_OBJ_SIZE)
            break;

          program_bytecode_length = (int)bc_len;
          program_legacy_bytecode = cmalloc(program_bytecode_length + 1);
          if(!program_legacy_bytecode)
            break;

          if(vfread(program_legacy_bytecode, 1, program_bytecode_length,
           bc_file) != (size_t)program_bytecode_length)
          {
            free(program_legacy_bytecode);
            break;
          }

          if(!validate_legacy_bytecode(&program_legacy_bytecode,
           &program_bytecode_length, NULL))
          {
            error_message(E_LOAD_BC_CORRUPT, 0, char_value);
            free(program_legacy_bytecode);
            break;
          }

          cur_robot->program_source =
           legacy_disassemble_program(program_legacy_bytecode,
           program_bytecode_length, &(cur_robot->program_source_length),
           SAVE_ROBOT_DISASM_EXTRAS, SAVE_ROBOT_DISASM_BASE);
          free(program_legacy_bytecode);

          // TODO: Move this outside of here.
          if(cur_robot->program_bytecode)
          {
            free(cur_robot->program_bytecode);
            cur_robot->program_bytecode = NULL;
          }

          cur_robot->cur_prog_line = 1;
          cur_robot->stack_pointer = 0;
          prepare_robot_bytecode(mzx_world, cur_robot);

          // Restart this robot if either it was just a LOAD_BC
          // OR LOAD_BCn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            vfclose(bc_file);
            return 1;
          }
        }

        vfclose(bc_file);
      }
      break;
    }

    case FOPEN_SAVE_ROBOT:
    {
      // FIXME old worlds will save new source. Fixing this would ideally
      // involve allowing new source, old source, or old bytecode to all be
      // considered the current program "source", but doing this in a clean
      // way probably relies on separating programs from robots internally.

      if(value >= 0)
        cur_robot = get_robot_by_id(mzx_world, value);

      if(cur_robot && cur_robot->program_source)
      {
        vfile *vf = fsafeopen(char_value, "wb");
        size_t len = cur_robot->program_source_length;

        if(vf)
        {
          // TODO: this doesn't apply zaps...
          vfwrite(cur_robot->program_source, len, 1, vf);
          vfclose(vf);
        }
      }
      break;
    }

    /* This can't really exist anymore... In practice I doubt any games
     * use it, but they can't  be supported - there's no really easy fix
     * for this.
     */
    case FOPEN_SAVE_BC:
    {
      error_message(E_DBC_SAVE_ROBOT_UNSUPPORTED, 0, NULL);
      set_error_suppression(E_DBC_SAVE_ROBOT_UNSUPPORTED, 1);
      break;
    }

#else // !CONFIG_DEBYTECODE

    case FOPEN_LOAD_ROBOT:
    {
      char *new_program;
      int new_size;

      new_program = assemble_file(char_value, &new_size);
      if(new_program)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          reallocate_robot(cur_robot, new_size);
          clear_label_cache(cur_robot);

          memcpy(cur_robot->program_bytecode, new_program, new_size);
          cur_robot->stack_pointer = 0;
          cur_robot->cur_prog_line = 1;
          cache_robot_labels(cur_robot);

          // Free the robot's source and command map
          free(cur_robot->program_source);
          cur_robot->program_source = NULL;
          cur_robot->program_source_length = 0;
#ifdef CONFIG_EDITOR
          free(cur_robot->command_map);
          cur_robot->command_map_length = 0;
          cur_robot->command_map = NULL;
#endif

          // Restart this robot if either it was just a LOAD_ROBOT
          // OR LOAD_ROBOTn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            free(new_program);
            return 1;
          }
        }

        free(new_program);
      }
      break;
    }

    case FOPEN_LOAD_BC:
    {
      vfile *bc_file = fsafeopen(char_value, "rb");

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          int64_t bc_len = vfilelength(bc_file, true);
          char *program_bytecode;
          int new_size;

          if(bc_len < 0 || bc_len > MAX_OBJ_SIZE)
            break;

          new_size = (int)bc_len;
          program_bytecode = cmalloc(new_size + 1);
          if(!program_bytecode)
            break;

          if(vfread(program_bytecode, 1, new_size, bc_file) != (size_t)new_size)
          {
            free(program_bytecode);
            break;
          }

          if(!validate_legacy_bytecode(&program_bytecode, &new_size, NULL))
          {
            error_message(E_LOAD_BC_CORRUPT, 0, char_value);
            free(program_bytecode);
            break;
          }

          clear_label_cache(cur_robot);
          free(cur_robot->program_bytecode);
          cur_robot->program_bytecode = program_bytecode;
          cur_robot->program_bytecode_length = new_size;
          cur_robot->cur_prog_line = 1;
          cur_robot->stack_pointer = 0;
          cache_robot_labels(cur_robot);

          // Free the robot's source and command map
          free(cur_robot->program_source);
          cur_robot->program_source = NULL;
          cur_robot->program_source_length = 0;
#ifdef CONFIG_EDITOR
          free(cur_robot->command_map);
          cur_robot->command_map_length = 0;
          cur_robot->command_map = NULL;
#endif

          // Restart this robot if either it was just a LOAD_BC
          // OR LOAD_BCn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            vfclose(bc_file);
            return 1;
          }
        }

        vfclose(bc_file);
      }
      break;
    }

    case FOPEN_SAVE_ROBOT:
    {
      if(value >= 0)
        cur_robot = get_robot_by_id(mzx_world, value);

      if(cur_robot)
      {
        disassemble_file(char_value, cur_robot->program_bytecode,
         cur_robot->program_bytecode_length, SAVE_ROBOT_DISASM_EXTRAS,
         SAVE_ROBOT_DISASM_BASE);
      }
      break;
    }

    case FOPEN_SAVE_BC:
    {
      vfile *bc_file = fsafeopen(char_value, "wb");

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          vfwrite(cur_robot->program_bytecode,
           cur_robot->program_bytecode_length, 1, bc_file);
        }

        vfclose(bc_file);
      }
      break;
    }

#endif // !CONFIG_DEBYTECODE

    default:
      debug("Invalid special_counter_return!\n");
      break;
  }

  return 0;
}

static struct counter *find_counter(struct counter_list *counter_list,
 const char *name, int *next)
{
  struct counter *current = NULL;

#ifdef CONFIG_COUNTER_HASH_TABLES
  size_t name_length = strlen(name);
  HASH_FIND(COUNTER, counter_list->hash_table, name, name_length, current);
  *next = counter_list->num_counters;
  return current;

#else
  struct counter **base = counter_list->counters;
  int bottom = 0, top = (counter_list->num_counters) - 1, middle = 0;
  int cmpval = 0;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return current;
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
#endif
}

static int hurt_player(struct world *mzx_world, int value)
{
  // Must not be invincible
  if(get_counter(mzx_world, "INVINCO", 0) <= 0)
  {
    struct board *src_board = mzx_world->current_board;
    send_robot_def(mzx_world, 0, LABEL_PLAYERHURT);
    if(src_board->restart_if_zapped)
    {
      int player_restart_x = mzx_world->player_restart_x;
      int player_restart_y = mzx_world->player_restart_y;
      int player_x = mzx_world->player_x;
      int player_y = mzx_world->player_y;
      //Restart since we were hurt
      if((player_restart_x != player_x) || (player_restart_y != player_y))
      {
        id_remove_top(mzx_world, player_x, player_y);
        player_x = player_restart_x;
        player_y = player_restart_y;
        id_place(mzx_world, player_x, player_y, PLAYER, 0, 0);
        mzx_world->was_zapped = true;
        mzx_world->player_x = player_x;
        mzx_world->player_y = player_y;
      }
    }
  }
  else
  {
    value = 0;
  }
  return value;
}

typedef int (*gateway_write_function)(struct world *mzx_world,
 struct counter *counter, const char *name, int value, int id, boolean is_dec);

static int health_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  // Trying to decrease health? (legacy support)
  // In pre-port MZX worlds, only the dec command triggers PLAYERHURT/zap.
  // Additionally, any decrement of >0 triggers this, so do it before the clamp.
  if((value < counter->value) && (mzx_world->version < VERSION_PORT) && is_dec)
  {
    value = counter->value - hurt_player(mzx_world, counter->value - value);
  }

  // Make sure health is within 0 and max health
  if(value > mzx_world->health_limit)
  {
    value = mzx_world->health_limit;
  }
  if(value < 0)
  {
    value = 0;
  }

  // Trying to decrease health?
  // In port MZX worlds, any method of reducing health triggers PLAYERHURT/zap.
  // This only happens if the clamped value is less than the old value.
  if((value < counter->value) && (mzx_world->version >= VERSION_PORT))
  {
    value = counter->value - hurt_player(mzx_world, counter->value - value);
  }

  return value;
}

static int lives_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  // Make sure lives is within 0 and max lives
  if(value > mzx_world->lives_limit)
  {
    value = mzx_world->lives_limit;
  }
  if(value < 0)
  {
    value = 0;
  }

  return value;
}

static int invinco_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  if(!counter->value)
  {
    mzx_world->saved_pl_color = id_chars[player_color];
  }
  else
  {
    if(!value)
    {
      sfx_clear_queue();
      play_sfx(mzx_world, SFX_INVINCO_END);
      id_chars[player_color] = mzx_world->saved_pl_color;
    }
  }

  return value;
}

static int score_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  // Protection for score < 0, as per the behavior in DOS MZX.
  if((value < 0) && (mzx_world->version < VERSION_PORT))
    return 0;

  return value;
}

static int time_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  return CLAMP(value, 0, 32767);
}

static int builtin_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id, boolean is_dec)
{
  // The other builtins simply can't go below 0
  return MAX(value, 0);
}

enum gateway_function_id
{
  NO_GATEWAY,
  GATEWAY_BUILTIN,
  GATEWAY_HEALTH,
  GATEWAY_INVINCO,
  GATEWAY_LIVES,
  GATEWAY_SCORE,
  GATEWAY_TIME,
  NUM_GATEWAYS
};

static gateway_write_function gateways[NUM_GATEWAYS] =
{
  [NO_GATEWAY]      = NULL,
  [GATEWAY_BUILTIN] = builtin_gateway,
  [GATEWAY_HEALTH]  = health_gateway,
  [GATEWAY_INVINCO] = invinco_gateway,
  [GATEWAY_LIVES]   = lives_gateway,
  [GATEWAY_SCORE]   = score_gateway,
  [GATEWAY_TIME]    = time_gateway
};

// Setup builtin gateway functions
static void set_gateway(struct counter_list *counter_list,
 const char *name, enum gateway_function_id gateway_id)
{
  int next;
  struct counter *cdest = find_counter(counter_list, name, &next);
  if(cdest)
    cdest->gateway_write = gateway_id;
}

void initialize_gateway_functions(struct world *mzx_world)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  set_gateway(counter_list, "AMMO", GATEWAY_BUILTIN);
  set_gateway(counter_list, "COINS", GATEWAY_BUILTIN);
  set_gateway(counter_list, "GEMS", GATEWAY_BUILTIN);
  set_gateway(counter_list, "HEALTH", GATEWAY_HEALTH);
  set_gateway(counter_list, "HIBOMBS", GATEWAY_BUILTIN);
  set_gateway(counter_list, "INVINCO", GATEWAY_INVINCO);
  set_gateway(counter_list, "LIVES", GATEWAY_LIVES);
  set_gateway(counter_list, "LOBOMBS", GATEWAY_BUILTIN);
  set_gateway(counter_list, "SCORE", GATEWAY_SCORE);
  set_gateway(counter_list, "TIME", GATEWAY_TIME);
}

static size_t get_counter_alloc_size(int name_length)
{
  // Attempt to reclaim any padding bytes at the end of the struct...
  return MAX(sizeof(struct counter),
   offsetof(struct counter, name) + name_length + 1);
}

static struct counter *allocate_new_counter(const char *name, size_t name_length,
 int value)
{
  struct counter *dest = (struct counter *)cmalloc(get_counter_alloc_size(name_length));
  if(!dest)
    return NULL;

  memcpy(dest->name, name, name_length);
  dest->name[name_length] = 0;

  dest->gateway_write = NO_GATEWAY;
  dest->name_length = name_length;
  dest->value = value;
  return dest;
}

static void add_counter(struct counter_list *counter_list, const char *name,
 int value, unsigned int position)
{
  unsigned int count = counter_list->num_counters;
  unsigned int allocated = counter_list->num_counters_allocated;
  struct counter **base = counter_list->counters;
  struct counter *dest;
  unsigned int name_length = strlen(name);

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
    {
      // Gracefully fail if this tries to go over 2b...
      if(allocated >= (size_t)(INT32_MAX))
        return;

      allocated *= 2;
    }
    else
      allocated = MIN_COUNTER_ALLOCATE;

    base = (struct counter **)crealloc(base, sizeof(struct counter *) * allocated);
    if(!base)
      return;

    counter_list->counters = base;
    counter_list->num_counters_allocated = allocated;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(struct counter *));
  }

  dest = allocate_new_counter(name, name_length, value);
  if(!dest)
    return;

  counter_list->counters[position] = dest;
  counter_list->num_counters = count + 1;

#ifdef CONFIG_COUNTER_HASH_TABLES
  HASH_ADD(COUNTER, counter_list->hash_table, dest);
#endif
}

void set_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && (mzx_world->version >= fdest->minimum_version))
  {
    // If we're a function counter and we have a write method,
    // use it. However, if we don't have a write method, this is
    // intentionally read-only counter and no storage space should
    // be allocated for it. Fall through without error in this case.
    if(fdest->function_write)
      fdest->function_write(mzx_world, fdest, name, value, id);
    else
      assert(fdest->function_read);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
    {
      // See if there's a gateway
      if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
      {
        gateway_write_function gateway_write = gateways[cdest->gateway_write];
        value = gateway_write(mzx_world, cdest, name, value, id, false);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(counter_list, name, value, next);
    }
  }
}

// Creates a new counter if it doesn't already exist; otherwise, sets the
// old counter's value. Basically, set_counter without the function check.
void new_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  struct counter *cdest;
  int next;

  cdest = find_counter(counter_list, name, &next);

  if(cdest)
  {
    // See if there's a gateway
    if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
    {
      gateway_write_function gateway_write = gateways[cdest->gateway_write];
      value = gateway_write(mzx_world, cdest, name, value, id, false);
    }

    cdest->value = value;
  }

  else
  {
    add_counter(counter_list, name, value, next);
  }
}

int get_counter(struct world *mzx_world, const char *name, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int next;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read &&
   (mzx_world->version >= fdest->minimum_version))
  {
    // Call read function
    if(fdest->function_read)
      return fdest->function_read(mzx_world, fdest, name, id);
    else
      return 0;
  }

  cdest = find_counter(counter_list, name, &next);

  if(cdest)
    return cdest->value;

  return 0;
}

/**
 * Get a counter by name and return a pointer to it. This function does not
 * work with function counters or other special counters; use get_string or
 * get_string_safe (editor) instead. The pointer this function returns is
 * guaranteed to be stable for the duration of the gameplay session.
 *
 * @param mzx_world   World data.
 * @param name        Name of counter to look up.
 * @param id          Current robot ID or 0 for global.
 * @return            counter pointer if found, otherwise NULL.
 */
const struct counter *get_counter_pointer(struct world *mzx_world,
 const char *name, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  int next;

  return find_counter(counter_list, name, &next);
}

void inc_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value + value, id);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
    {
      value += cdest->value;

      if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
      {
        gateway_write_function gateway_write = gateways[cdest->gateway_write];
        value = gateway_write(mzx_world, cdest, name, value, id, false);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(counter_list, name, value, next);
    }
  }
}

void dec_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value - value, id);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
    {
      value = cdest->value - value;

      if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
      {
        gateway_write_function gateway_write = gateways[cdest->gateway_write];
        value = gateway_write(mzx_world, cdest, name, value, id, true);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(counter_list, name, -value, next);
    }
  }
}

void mul_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value * value, id);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
    {
      value *= cdest->value;
      if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
      {
        gateway_write_function gateway_write = gateways[cdest->gateway_write];
        value = gateway_write(mzx_world, cdest, name, value, id, false);
      }

      cdest->value = value;
    }
  }
}

void div_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     safe_divide_32(current_value, value), id);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
    {
      value = safe_divide_32(cdest->value, value);

      if(cdest->gateway_write && cdest->gateway_write < NUM_GATEWAYS)
      {
        gateway_write_function gateway_write = gateways[cdest->gateway_write];
        value = gateway_write(mzx_world, cdest, name, value, id, false);
      }

      cdest->value = value;
    }
  }
}

void mod_counter(struct world *mzx_world, const char *name, int value, int id)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);

    fdest->function_write(mzx_world, fdest, name,
     safe_modulo_32(current_value, value), id);
  }
  else
  {
    cdest = find_counter(counter_list, name, &next);

    if(cdest)
      cdest->value = safe_modulo_32(cdest->value, value);
  }
}

// Create a new counter from loading a save file. This skips find_counter.
void load_new_counter(struct counter_list *counter_list, int index,
 const char *name, int name_length, int value)
{
  struct counter *dest = allocate_new_counter(name, name_length, value);

  counter_list->counters[index] = dest;

#ifdef CONFIG_COUNTER_HASH_TABLES
  HASH_ADD(COUNTER, counter_list->hash_table, dest);
#endif
}

static int counter_sort_fcn(const void *a, const void *b)
{
  return strcasecmp(
   (*(const struct counter **)a)->name,
   (*(const struct counter **)b)->name);
}

void sort_counter_list(struct counter_list *counter_list)
{
  qsort(counter_list->counters, (size_t)counter_list->num_counters,
   sizeof(struct counter *), counter_sort_fcn);
}

void clear_counter_list(struct counter_list *counter_list)
{
  size_t i;

#ifdef CONFIG_COUNTER_HASH_TABLES
  HASH_CLEAR(COUNTER, counter_list->hash_table);
  counter_list->hash_table = NULL;
#endif

  for(i = 0; i < counter_list->num_counters; i++)
    free(counter_list->counters[i]);

  free(counter_list->counters);

  counter_list->num_counters = 0;
  counter_list->num_counters_allocated = 0;
  counter_list->counters = NULL;
}

#ifdef CONFIG_EDITOR

void counter_list_size(struct counter_list *counter_list,
 size_t *list_size, size_t *table_size, size_t *counters_size)
{
  if(list_size)
    *list_size = counter_list->num_counters_allocated * sizeof(struct counter *);

  if(table_size)
  {
    *table_size = 0;
#ifdef CONFIG_COUNTER_HASH_TABLES
    HASH_MEMORY_USAGE(COUNTER, counter_list->hash_table, *table_size);
#endif
  }

  if(counters_size)
  {
    size_t total = 0;
    size_t i;

    if(counter_list->counters)
    {
      for(i = 0; i < counter_list->num_counters; i++)
      {
        struct counter *c = counter_list->counters[i];
        if(c)
          total += get_counter_alloc_size(c->name_length);
      }
    }
    *counters_size = total;
  }
}

#endif /* CONFIG_EDITOR */
