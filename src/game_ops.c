/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
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

#include "const.h"
#include "counter.h"
#include "game_ops.h"
#include "game_player.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "robot.h"
#include "sfx.h"
#include "util.h"
#include "window.h"
#include "world_struct.h"

/**
 * Utility functions for gameplay.
 */

/**
 * Update a robot's position after moving it. This was never done by older
 * versions, so don't update the robot's compatibility position values.
 */
static void fix_robot_pos(struct board *cur_board, int id, int x, int y)
{
  struct robot *cur_robot = cur_board->robot_list[id];
  cur_robot->xpos = x;
  cur_robot->ypos = y;
}

/**
 * Update a robot's position after moving it. This was never done by older
 * versions, so don't update the robot's compatibility position values.
 */
static void fix_robot_pos_offs(struct board *cur_board, int id, int offset)
{
  struct robot *cur_robot = cur_board->robot_list[id];
  cur_robot->xpos = offset % cur_board->board_width;
  cur_robot->ypos = offset / cur_board->board_width;
}

//Bit 1- +1
//Bit 2- -1
//Bit 4- +width
//Bit 8- -width
static const char cw_offs[8] = { 10, 2, 6, 4, 5, 1, 9, 8 };
static const char ccw_offs[8] = { 10, 8, 9, 1, 5, 4, 6, 2 };

// Rotate an area
void rotate(struct world *mzx_world, int x, int y, int dir)
{
  struct board *src_board = mzx_world->current_board;
  const char *offsp = cw_offs;
  int offs[8];
  int offset, i;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int cw, ccw;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  enum thing id;
  char param, color;
  enum thing cur_id;
  char cur_param, cur_color;
  int d_flag;
  int cur_offset, next_offset;

  offset = x + (y * board_width);
  if((x == 0) || (y == 0) || (x == (board_width - 1)) ||
   (y == (board_height - 1))) return;

  if(dir)
    offsp = ccw_offs;

  // Fix offsets
  for(i = 0; i < 8; i++)
  {
    int offsval = offsp[i];
    if(offsval & 1)
      offs[i] = 1;
    else
      offs[i] = 0;

    if(offsval & 2)
      offs[i]--;

    if(offsval & 4)
      offs[i] += board_width;

    if(offsval & 8)
      offs[i] -= board_width;
  }

  for(i = 0; i < 8; i++)
  {
    cur_id = (enum thing)level_id[offset + offs[i]];
    if((flags[(int)cur_id] & A_UNDER) && (cur_id != GOOP))
      break;
  }

  if(i == 8)
  {
    // No surrounding floors were found, so swap temporarily off of the board.
    // For some reason this case doesn't track rotation state with update_done.
    for(i = 0; i < 8; i++)
    {
      cur_id = (enum thing)level_id[offset + offs[i]];
      d_flag = flags[(int)cur_id];

      if(!((d_flag & A_PUSHABLE) || (d_flag & A_SPEC_PUSH)))
      {
        /**
         * From 2.80X through 2.91X, this would also exit for anything with
         * A_SPEC_PUSH. This regression was most likely introduced because
         * of a comment about transports not being pushable. If something
         * seriously relies on A_SPEC_PUSH objects not rotating in this edge
         * case, add a compatibility check.
         *
         * This also originally checked for the GATE thing, but GATE never had
         * either pushable flag set. MZX 1.xx appears to have blacklisted
         * A_SPEC_PUSH and this may have been a (broken) attempt to prevent
         * transports from rotating after that bug was fixed in 2.00.
         */
        break;
      }
    }

    if(i == 8)
    {
      // All surrounding objects are pushable and can be rotated.
      cur_offset = offset + offs[0];
      id = (enum thing)level_id[cur_offset];
      color = level_color[cur_offset];
      param = level_param[cur_offset];

      for(i = 0; i < 7; i++)
      {
        cur_offset = offset + offs[i];
        next_offset = offset + offs[i + 1];
        level_id[cur_offset] = level_id[next_offset];
        level_color[cur_offset] = level_color[next_offset];
        level_param[cur_offset] = level_param[next_offset];

        if(level_id[cur_offset] == ROBOT_PUSHABLE)
          fix_robot_pos_offs(src_board, level_param[cur_offset], cur_offset);
      }

      cur_offset = offset + offs[7];
      level_id[cur_offset] = (char)id;
      level_color[cur_offset] = color;
      level_param[cur_offset] = param;

      if(id == ROBOT_PUSHABLE)
        fix_robot_pos_offs(src_board, param, cur_offset);
    }
  }
  else
  {
    // A floor was found, so start the rotation from the floor.
    cw = i - 1;

    if(cw == -1)
      cw = 7;

    do
    {
      ccw = i + 1;
      if(ccw == 8)
        ccw = 0;

      cur_offset = offset + offs[ccw];
      next_offset = offset + offs[i];
      cur_id = (enum thing)level_id[cur_offset];
      d_flag = flags[(int)cur_id];

      /**
       * This originally checked for the GATE thing, but GATE never had
       * either pushable flag set. MZX 1.xx appears to have blacklisted
       * A_SPEC_PUSH and this may have been a (broken) attempt to prevent
       * transports from rotating after that bug was fixed in 2.00.
       */
      if(((d_flag & A_PUSHABLE) || (d_flag & A_SPEC_PUSH)) &&
       (!(mzx_world->update_done[cur_offset] & 2)))
      {
        cur_param = level_param[cur_offset];
        cur_color = level_color[cur_offset];
        offs_place_id(mzx_world, next_offset, cur_id, cur_color, cur_param);
        offs_remove_id(mzx_world, cur_offset);
        mzx_world->update_done[next_offset] |= 2;
        i = ccw;

        if(cur_id == ROBOT_PUSHABLE)
          fix_robot_pos_offs(src_board, cur_param, next_offset);
      }
      else
      {
        i = ccw;
        while(i != cw)
        {
          cur_id = (enum thing)level_id[offset + offs[i]];
          if((flags[(int)cur_id] & A_UNDER) && (cur_id != GOOP))
            break;

          i++;
          if(i == 8)
            i = 0;
        }
      }
    } while(i != cw);
  }
}

void calculate_xytop(struct world *mzx_world, int *x, int *y)
{
  struct board *src_board = mzx_world->current_board;
  int nx, ny;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  int locked_y = src_board->locked_y;

  // Calculate xy top from player position and scroll view pos, or
  // as static position if set.
  if(locked_y != -1)
  {
    nx = src_board->locked_x + src_board->scroll_x;
    ny = locked_y + src_board->scroll_y;
  }
  else
  {
    // Calculate from player position
    // Center screen around player, add scroll factor
    nx = mzx_world->player_x - (viewport_width / 2);
    ny = mzx_world->player_y - (viewport_height / 2);

    if(nx < 0)
      nx = 0;

    if(ny < 0)
      ny = 0;

    if(nx > (board_width - viewport_width))
     nx = board_width - viewport_width;

    if(ny > (board_height - viewport_height))
     ny = board_height - viewport_height;

    nx += src_board->scroll_x;
    ny += src_board->scroll_y;
  }
  // Prevent from going offscreen
  if(nx < 0)
    nx = 0;

  if(ny < 0)
    ny = 0;

  if(nx > (board_width - viewport_width))
    nx = board_width - viewport_width;

  if(ny > (board_height - viewport_height))
    ny = board_height - viewport_height;

  *x = nx;
  *y = ny;
}

// Return the seek dir relative to the player.

int find_seek(struct world *mzx_world, int x, int y)
{
  int dir;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;

  if(y == player_y)
  {
    dir = 0;                // Go horizontally
  }
  else
  if(x == player_x)
  {
    dir = 1;                // Go vertically
  }
  else
  {
    dir = Random(2);        // Go random horizontal or vertical
  }

  if(dir)
  {
    // Dir becomes up or down.
    if(player_y < y)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
  else
  {
    // Dir becomes left or right.
    if(player_x < x)
    {
      return 3;
    }
    else
    {
      return 2;
    }
  }
}

int flip_dir(int dir)
{
  dir ^= 1;                 // Toggle the lsb to change direction
  return dir;
}

// Push/transport currently have no recursion protection. As far as
// I'm aware, there's only so large you can make a push/transport chain
// (at most around 400). This is probably too much for an MS-DOS stack,
// but any decent 32bit OS should be able to tolerate it.

// See if a transport can be made to that x/y position
// This is an additional helper function to try to sanitize transport
// Returns 1 if successful, 0 if not

static int try_transport(struct world *mzx_world, int x, int y, int push_status,
 int can_push, enum thing id, int dir)
{
  struct board *src_board = mzx_world->current_board;
  int d_offset = xy_to_offset(src_board, x, y);
  enum thing d_id = (enum thing)src_board->level_id[d_offset];

  // Not gonna be happening with goop or a transport
  if((d_id != GOOP) && (d_id != TRANSPORT))
  {
    // Get flags for the ID
    int d_flag = flags[d_id];

    // Can it be moved under? It's a good destination then
    if(d_flag & A_UNDER)
      return 1;

    // Is it pushable?
    if(d_flag & (push_status | A_SPEC_PUSH))
    {
      // Player can move onto sensor.. sensor is not under
      // but pushable.
      if((d_id == SENSOR) && (id == PLAYER))
      {
        return 1;
      }
      else
      {
        // Okay, try pushing it.. if it can be pushed
        if(can_push == 1)
        {
          int px = 0, py = 0;
          xy_shift_dir(src_board, x, y, &px, &py, flip_dir(dir));
          // Try to push it
          if(push(mzx_world, px, py, dir, 0) == 0)
            return 1;
        }
      }
    }
  }

  return 0;
}

int transport(struct world *mzx_world, int x, int y, int dir, enum thing id,
 int param, int color, int can_push)
{
  struct board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  // East/west or north/south
  int push_status = A_PUSHEW;
  int offset = xy_to_offset(src_board, x, y);
  int dx, dy, d_offset;
  char t_param = level_param[offset];
  int found = 0;

  // Get the direction of the transporter
  int t_dir = t_param & 0x07;

  // The direction must either be the given one or anydir
  if((t_dir != dir) && (t_dir != 4))
    return 1;

  if(dir < 2)
  {
    push_status = A_PUSHNS;
  }

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
    return 1;

  // See if the first one will work

  found = try_transport(mzx_world, dx, dy, push_status,
   can_push, id, dir);

  // Until found or exited, loop on
  while(!found)
  {
    // The next one must be a transporter
    d_offset = xy_to_offset(src_board, dx, dy);
    // Get next dir

    if(xy_shift_dir(src_board, dx, dy, &dx, &dy, dir))
      return 1;

    if(level_id[d_offset] == TRANSPORT)
    {
      t_dir = level_param[d_offset] & 0x07;
      if((t_dir == 4) || (t_dir == flip_dir(dir)))
      {
        // Try the new location, after the transport
        found = try_transport(mzx_world, dx, dy,
         push_status, can_push, id, dir);
      }
    }
  }

  // Okay, can put it at dx, dy.. if we're not just checking
  if(id != SPACE)
  {
    // Place it
    play_sfx(mzx_world, SFX_TRANSPORT);
    id_place(mzx_world, dx, dy, id, color, param);

    if(is_robot(id))
      fix_robot_pos(src_board, param, dx, dy);
  }

  // Successful return..
  return 0;
}

static void push_player_sensor(struct world *mzx_world, int p_offset,
 int d_offset, char param, char color)
{
  struct board *src_board = mzx_world->current_board;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;

  // Record the old sensor's details
  color = src_board->level_color[p_offset];
  param = src_board->level_param[p_offset];

  // Move the bottom layer under the sensor to the top,
  // eliminating the sensor
  src_board->level_id[p_offset] = level_under_id[p_offset];
  src_board->level_color[p_offset] = level_under_color[p_offset];
  src_board->level_param[p_offset] = level_under_param[p_offset];

  // Do we now have a sensor? If so, swap it with our current sensor
  if (src_board->level_id[p_offset] == SENSOR)
  {
    char tmp_color = src_board->level_color[p_offset];
    char tmp_param = src_board->level_param[p_offset];
    src_board->level_color[p_offset] = color;
    src_board->level_param[p_offset] = param;
    color = tmp_color;
    param = tmp_param;
  }

  // Restore the previous under with this stuff
  level_under_id[p_offset] = mzx_world->under_player_id;
  level_under_color[p_offset] = mzx_world->under_player_color;
  level_under_param[p_offset] = mzx_world->under_player_param;

  // Get new under stuff
  mzx_world->under_player_id = level_under_id[d_offset];
  mzx_world->under_player_color = level_under_color[d_offset];
  mzx_world->under_player_param = level_under_param[d_offset];

  // Move the sensor under the player
  level_under_id[d_offset] = (char)SENSOR;
  level_under_color[d_offset] = color;
  level_under_param[d_offset] = param;

  push_sensor(mzx_world, level_under_param[d_offset]);
}

int push(struct world *mzx_world, int x, int y, int dir, int checking)
{
  struct board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;

  int push_status = A_PUSHEW;
  int dx = x, dy = y, d_offset, d_flag;
  enum thing d_id = NO_ID;
  char d_param, d_color;
  enum thing d_under_id = NO_ID;
  enum thing p_id = NO_ID;
  char p_param = 0xFF, p_color = 0xFF;
  enum thing p_under_id = NO_ID;
  char sensor_param = 0xFF, sensor_color = 0xFF;
  int p_offset = -1;

  if(dir < 2)
  {
    push_status = A_PUSHNS;
  }

  // See if the block can be pushed; if not, exit.
  while(1)
  {
    // Previous ID
    p_id = d_id;
    p_under_id = d_under_id;
    // Get the next in the dir, out of bounds returns 1
    if(xy_shift_dir(src_board, dx, dy, &dx, &dy, dir))
      return 1;
    d_offset = xy_to_offset(src_board, dx, dy);
    d_id = (enum thing)level_id[d_offset];
    d_under_id = (enum thing)level_under_id[d_offset];
    d_flag = flags[(int)d_id];

    // If a push can be made here, the destination has been found

    if(d_flag & A_UNDER)
    {
      // FIXME - In game2.asm you can't push things over water, even though
      // it still works somehow. Look into this.
      // Lava and goop are automatic failures even though it's "underable"
      if((d_id == LAVA) || (d_id == GOOP))
        return 1;
      // Otherwise, we're good
      break;
    }
    else
    {
      // The search may continue... if it's pushable in that dir, continue
      if(!(d_flag & push_status))
      {
        // "spec push" can be pushed too, otherwise no push
        if(!(d_flag & A_SPEC_PUSH))
          return 1;
        // Player can be pushed onto sensor, otherwise it's pushable
        if((d_id == SENSOR) && (p_id == PLAYER) && (p_under_id != SENSOR))
          break;
        else

        // Can be pushed onto a transport if transport checks out
        if(d_id == TRANSPORT)
        {
          if(transport(mzx_world, dx, dy, dir, SPACE, 0, 0, 1))
            return 1;
          else
            break;
        }
      }
    }
  }

  // The push can be made now, make sure we're not just checking though
  if(!checking)
  {
    // Start back at the beginning...
    dx = x;
    dy = y;
    p_id = NO_ID;

    while(1)
    {
      // Get thing at next location
      xy_shift_dir(src_board, dx, dy, &dx, &dy, dir);
      d_offset = xy_to_offset(src_board, dx, dy);
      d_id = (enum thing)level_id[d_offset];
      d_param = level_param[d_offset];
      d_color = level_color[d_offset];
      d_under_id = (enum thing)level_under_id[d_offset];
      d_flag = flags[(int)d_id];

      // Sensor? Mark it
      if(d_under_id == SENSOR)
      {
        sensor_color = level_under_color[d_offset];
        sensor_param = level_under_param[d_offset];
      }

      // Can the destination be moved under and thus pushed onto?
      // A sensor will also work if the player is what's being pushed onto it.
      // (but only if what's under the player isn't another sensor)
      if((d_flag & A_UNDER) ||
       ((p_id == PLAYER) && (d_id == SENSOR) && (p_under_id != SENSOR)))
      {
        // Place the previous thing here
        id_place(mzx_world, dx, dy, p_id, p_color, p_param);

        if((p_id == PLAYER) && (p_under_id == SENSOR))
        {
          push_player_sensor(mzx_world, p_offset, d_offset, sensor_param,
           sensor_color);
        }
        else

        if(p_id == ROBOT_PUSHABLE)
          fix_robot_pos(src_board, p_param, dx, dy);

        // If this is a sensor, the player was pushed onto it
        if(d_id == SENSOR)
        {
          // The player is just stepping on it.. alert sensor
          step_sensor(mzx_world, d_param);
        }
        // All under can stop
        break;
      }

      // Is it pushable?
      if(d_flag & (push_status | A_SPEC_PUSH))
      {
        if(d_id == TRANSPORT)
        {
          // If it's a transport, transport it
          transport(mzx_world, dx, dy, dir, p_id, p_param, p_color, 1);
          break;
        }

        // No previous ID yet?
        if(p_id == NO_ID)
        {
          // Remove what was there
          id_remove_top(mzx_world, dx, dy);
        }
        else
        {
          // Otherwise, put the last one in the new one, and make
          // the current one the new last one.
          level_id[d_offset] = p_id;
          level_param[d_offset] = p_param;
          level_color[d_offset] = p_color;

          // FIXME: this is bugged, see issue 632
          // Was a player/sensor sandwich pushed?
          if((p_id == PLAYER) && (p_under_id == SENSOR))
          {
            push_player_sensor(mzx_world, p_offset, d_offset,
             sensor_param, sensor_color);
          }

          if(p_id == ROBOT_PUSHABLE)
            fix_robot_pos(src_board, p_param, dx, dy);
        }

        // How about a pushable robot?
        if(d_id == ROBOT_PUSHABLE)
        {
          // Send the default label for pushing
          send_robot_def(mzx_world, d_param, LABEL_PUSHED);
        }

        // Load current into previous
        p_id = d_id;
        p_under_id = d_under_id;
        p_param = d_param;
        p_color = d_color;
        p_offset = d_offset;

        // Is it a sensor that was pushed? Flag it.
        if(d_id == SENSOR)
        {
          push_sensor(mzx_world, d_param);
        }
      }
    }
  }

  play_sfx(mzx_world, SFX_PUSH);

  return 0;
}

void shoot(struct world *mzx_world, int x, int y, int dir, int type)
{
  int dx, dy, d_offset, d_flag;
  char d_id, d_param;
  struct board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;

  // If enemy hurts enemy then enemy bullet becomes neutral
  if((mzx_world->enemy_hurt_enemy == 1) && (type == ENEMY_BULLET))
  {
    type = NEUTRAL_BULLET;
  }

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
  {
    return;
  }

  d_offset = xy_to_offset(src_board, dx, dy);
  d_id = src_board->level_id[d_offset];
  d_param = src_board->level_param[d_offset];
  d_flag = flags[(int)d_id];

  // If it's a player bullet maybe give some points
  // Has to be an enemy ID 80-95 but not 83, 85, 92, or 93
  if((type == PLAYER_BULLET) && (d_id >= SNAKE) &&
   (d_id <= BEAR_CUB) && (d_id != SLIMEBLOB) && (d_id != GHOST) &&
   (d_id != BULLET_GUN) && (d_id != SPINNING_GUN))
  {
    // Score isn't supposed to overflow.. but the overflow
    // detection in game2.asm is broken anyway....
    // Just let it wrap around for now.
    inc_counter(mzx_world, "SCORE", 3, 0);
  }

  // If it's underable the bullet can move here
  if(d_flag & A_UNDER)
  {
    char n_param = (type << 2) | dir;
    id_place(mzx_world, dx, dy, BULLET, bullet_color[type], n_param);
    return;
  }

  // Is it shootable?
  if(d_flag & A_SHOOTABLE)
  {
    // However, enemies can only shoot these things:
    if((type != ENEMY_BULLET) || (d_id == BULLET) || (d_id == GEM) ||
     (d_id == MAGIC_GEM) || (d_id == BREAKAWAY) ||
     (d_id == CUSTOM_BREAK))
    {
      // Can be removed
      id_remove_top(mzx_world, dx, dy);
      play_sfx(mzx_world, SFX_BREAK);
    }
    return;
  }

  // Must be a special shot for anything to happen now
  if(d_flag & A_SPEC_SHOT)
  {
    // And these are the ones
    switch(d_id)
    {
      case RICOCHET_PANEL:
      {
        play_sfx(mzx_world, SFX_RICOCHET);
        // Fanangle the panel orientation into a shoot direction
        if(d_param == 1)
        {
          dir ^= 2;
        }
        else
        {
          dir ^= 3;
        }
        shoot(mzx_world, dx, dy, dir, NEUTRAL_BULLET);
        break;
      }

      case RICOCHET:
      {
        int pdir = flip_dir(dir);
        play_sfx(mzx_world, SFX_RICOCHET);
        // Shoot backwards
        shoot(mzx_world, dx, dy, pdir, NEUTRAL_BULLET);
        break;
      }

      case MINE:
      {
        // Turn into explosion
        level_id[d_offset] = EXPLOSION;
        // Get rid of count and anim fields in param
        level_param[d_offset] = d_param & 0xF0;
        play_sfx(mzx_world, SFX_EXPLOSION);
        break;
      }

      case PLAYER:
      {
        // Player bullets don't hurt player
        if(type == PLAYER_BULLET) break;
        hurt_player(mzx_world, BULLET);
        send_robot_def(mzx_world, 0, LABEL_PLAYERHIT);
        break;
      }

      case ROBOT:
      case ROBOT_PUSHABLE:
      {
        int idx = d_param;
        // Send the current shot label to the robot
        // 4 is PLAYERSHOT, 5 is NEUTRAL, 6 is ENEMY
        send_robot_def(mzx_world, d_param, type + 4);
        // Set its last shot dir..
        (src_board->robot_list[idx])->last_shot_dir =
         int_to_dir(flip_dir(dir));
        break;
      }

      case EYE:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;
        // Turn into explosion
        level_id[d_offset] = (char)EXPLOSION;
        level_param[d_offset] = (d_param & 0x38) << 1;
        play_sfx(mzx_world, SFX_EXPLOSION);
        break;
      }

      case SLIMEBLOB:
      {
        // Can't be shot by enemy bullet
        if(type != ENEMY_BULLET)
        {
          // Check if not invincible
          if(!(d_param & 0x80))
          {
            // Kill it
            id_remove_top(mzx_world, dx, dy);
            play_sfx(mzx_world, SFX_BREAK);
          }
        }
        break;
      }

      case RUNNER:
      {
        // Can't be shot by enemy bullet
        if(type != ENEMY_BULLET)
        {
          // Check if it has 0 HP
          if(d_param & 0xC0)
          {
            // Otherwise, take an HP
            level_param[d_offset] = d_param - 0x40;
            play_sfx(mzx_world, SFX_ENEMY_HP_DOWN);
          }
          else
          {
            // Kill it
            id_remove_top(mzx_world, dx, dy);
            play_sfx(mzx_world, SFX_BREAK);
          }
        }

        break;
      }

      case GHOST:
      {
        // Can't be shot by enemy bullet
        if(type != ENEMY_BULLET)
        {
          // Is it not invincible?
          if(!(d_param & 0x08))
          {
            // Kill it
            id_remove_top(mzx_world, dx, dy);
            play_sfx(mzx_world, SFX_BREAK);
          }
        }

        break;
      }

      case DRAGON:
      {
        // Can't be shot by enemy bullet
        if(type != ENEMY_BULLET)
        {
          // Out of HP?
          if(!(d_param & 0xE0))
          {
            // Kill it
            id_remove_top(mzx_world, dx, dy);
            play_sfx(mzx_world, SFX_BREAK);
          }
          else
          {
            // Take away a hit point
            level_param[d_offset] = d_param - 0x20;
            play_sfx(mzx_world, SFX_ENEMY_HP_DOWN);
          }
        }

        break;
      }

      case FISH:
      case SPIDER:
      case BEAR:
      {
        // Zero HP?
        if(!(d_param & 0x80))
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, SFX_BREAK);
        }
        else
        {
          // Remove HP
          level_param[d_offset] = d_param ^ 0x80;
          play_sfx(mzx_world, SFX_ENEMY_HP_DOWN);
        }
        break;
      }
    }
    return;
  }
}

void shoot_fire(struct world *mzx_world, int x, int y, int dir)
{
  struct board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  enum thing d_id;
  char d_param, d_flag;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
    return;

  d_offset = xy_to_offset(src_board, dx, dy);
  d_id = (enum thing)level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[(int)d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, SHOOTING_FIRE, 0, dir << 1);
  }
  else

  // Did it hit a robot?
  if(is_robot(d_id))
  {
    // Send the spitfire label
    send_robot_def(mzx_world, d_param, LABEL_SPITFIRE);
  }
  else

  // Did it hit the player?
  if(d_id == PLAYER)
  {
    hurt_player(mzx_world, SHOOTING_FIRE);
  }
}

void shoot_seeker(struct world *mzx_world, int x, int y, int dir)
{
  struct board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  enum thing d_id;
  char d_flag;
  char *level_id = src_board->level_id;

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
    return;

  d_offset = xy_to_offset(src_board, dx, dy);
  d_id = (enum thing)level_id[d_offset];
  d_flag = flags[(int)d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, SEEKER, 0, 127);
  }
  else

  // Did it hit the player?
  if(d_id == PLAYER)
  {
    hurt_player(mzx_world, SEEKER);
  }
}

void shoot_missile(struct world *mzx_world, int x, int y, int dir)
{
  struct board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  enum thing d_id;
  char d_flag;
  char *level_id = src_board->level_id;

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
    return;

  d_offset = xy_to_offset(src_board, dx, dy);
  d_id = (enum thing)level_id[d_offset];
  d_flag = flags[(int)d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, MISSILE, missile_color, dir);
  }

  // Did it hit the player?
  if(d_id == PLAYER)
  {
    hurt_player(mzx_world, MISSILE);
  }
}

void shoot_lazer(struct world *mzx_world, int x, int y, int dir, int length,
 int color)
{
  struct board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;

  int lx = x;
  int ly = y;
  int offset;
  enum thing id;
  char param;

  while(1)
  {
    if(xy_shift_dir(src_board, lx, ly, &lx, &ly, dir))
    {
      // Out of bounds
      return;
    }

    offset = xy_to_offset(src_board, lx, ly);
    id = (enum thing)level_id[offset];

    if(id == PLAYER)
    {
      hurt_player(mzx_world, LAZER);

      // Restart if zapped?
      if(src_board->restart_if_zapped != 1)
      {
        int p_dir = (src_board->player_last_dir) & 0x0F;
        if(p_dir != 0)
        {
          // Move the player in the opposite direction
          move(mzx_world, lx, ly, flip_dir((p_dir - 1)),
           CAN_PUSH | CAN_TRANSPORT | CAN_WATERWALK);
        }
      }

      break;
    }
    else

    // Robot
    if(is_robot(id))
    {
      param = level_param[offset];
      // Send the robot the lazer label
      send_robot_def(mzx_world, param, LABEL_LAZER);

      break;
    }
    else
    // Otherwise...
    {
      int flag = flags[(int)id];
      char n_param;
      // Blocked. Get out of here
      if(!(flag & A_UNDER)) return;

      // Combine length and dir in lazer param
      n_param = length << 3;

      // Check vertical/horizontal
      if(dir <= 1)
      {
        n_param |= 1;
      }

      id_place(mzx_world, lx, ly, LAZER, color, n_param);
    }
  }
}

enum move_status move(struct world *mzx_world, int x, int y, int dir,
 int move_flags)
{
  struct board *src_board = mzx_world->current_board;
  int dx, dy, d_offset, d_flag;
  enum thing d_id;
  char d_param;
  int p_offset = xy_to_offset(src_board, x, y);
  enum thing p_id;
  char p_param, p_color;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  /* Can't move to invalid directions. This was implicit prior to
   * 2.93d for all things capable of moving in invalid directions
   * (ShootingFire, Missile, Pusher, and SpittingTiger) */
  if((unsigned)dir > 3)
    return HIT;

  if(xy_shift_dir(src_board, x, y, &dx, &dy, dir))
  {
    // Hit edge
    return HIT_EDGE;
  }

  d_offset = xy_to_offset(src_board, dx, dy);
  d_id = (enum thing)level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[(int)d_id];

  p_id = (enum thing)level_id[p_offset];
  p_param = level_param[p_offset];
  p_color = level_color[p_offset];

  // Only if the player shouldn't be checked yet
  if((move_flags & REACT_PLAYER) && (d_id == PLAYER))
  {
    // Hit the player
    return HIT_PLAYER;
  }

  // Is it a spitfire? Is it moving onto a robot?
  if((move_flags & SPITFIRE) && is_robot(d_id))
  {
    // Send the label
    send_robot_def(mzx_world, d_param, LABEL_SPITFIRE);
    return HIT;
  }

  // Can move? However, flags may prohibit the move.
  if(d_flag & A_UNDER)
  {
    int web_flags = (move_flags & (MUST_WEB | MUST_THICKWEB)) >> 5;
    int web = (d_id == WEB) | ((d_id == THICK_WEB) << 1);

    // Web and thick web may be flagged separately, or together.
    // If they're both on, we must allow either WEB or THICK_WEB ids.
    if(web_flags && !(web_flags & web))
      return HIT;

    // Must be water?
    if((move_flags & MUST_WATER) && !(is_water(d_id)))
      return HIT;

    // Must be lava or goop?
    if((move_flags & MUST_LAVAGOOP) &&
     (d_id != LAVA) && (d_id != GOOP))
    {
      return HIT;
    }

    // Now CAN it move to lava if there's lava?
    if((d_id == LAVA) && !(move_flags & CAN_LAVAWALK))
      return HIT;

    // Same for fire
    if((d_id == FIRE) && !(move_flags & CAN_FIREWALK))
      return HIT;

    // Same for water
    if(is_water(d_id) && !(move_flags & CAN_WATERWALK))
      return HIT;

    // Only must goop can go on goop
    if((d_id == GOOP) && !(move_flags & CAN_GOOPWALK))
      return HIT;
  }
  else

  // Moving onto a transporter?
  if(d_id == TRANSPORT)
  {
    // Can it transport?
    if(move_flags & CAN_TRANSPORT)
    {
      int can_push = 0;

      // Transport it
      if(move_flags & CAN_PUSH)
        can_push = 1;

      // If it can transport, do it
      if(!transport(mzx_world, dx, dy, dir, p_id, p_param, p_color, can_push))
      {
        id_remove_top(mzx_world, x, y);
        return NO_HIT;
      }
    }
    // Otherwise blocked
    return HIT;
  }
  else

  // Is it pushable and can it be pushed?
  if((d_flag & (A_PUSHNS | A_PUSHEW | A_SPEC_PUSH)) &&
   (move_flags & CAN_PUSH))
  {
    // If it can't push, exit
      if(push(mzx_world, x, y, dir, 0))
        return HIT;
  }
  else
  {
    return HIT;
  }

  // Made it this far - actually move the thing.
  // Remove what was there
  id_remove_top(mzx_world, x, y);
  // Place a new one at the destination
  id_place(mzx_world, dx, dy, p_id, p_color, p_param);

  if(is_robot(p_id))
    fix_robot_pos(src_board, p_param, dx, dy);

  // Successfully return
  return NO_HIT;
}

enum dir parsedir(struct world *mzx_world, enum dir old_dir, int x, int y,
 enum dir flow_dir, int bln, int bls, int ble, int blw)
{
  int n_dir = (int)old_dir & 0x0F;

  switch(n_dir)
  {
    // idle, beneath, anydir, nodir don't get modified
    case IDLE:
    case BENEATH:
    case ANYDIR:
    case NODIR:
      return (enum dir)n_dir;

    case NORTH:
    case SOUTH:
    case EAST:
    case WEST:
      n_dir--;
      break;

    case RANDNS:
      n_dir = Random(2);
      break;

    case RANDEW:
      n_dir = (Random(2)) + 2;
      break;

    case RANDNE:
      n_dir = Random(2) << 1;
      break;

    case RANDNB:
      bln ^= 1;
      bls ^= 1;
      ble ^= 1;
      blw ^= 1;

      /* fallthrough */

    case RANDB:
    {
      int bl_sum = bln + bls + ble + blw;
      if(bl_sum)
      {
        n_dir = Random(bl_sum);
        if(!bln)
          n_dir++;
        if(!bls && n_dir)
          n_dir++;
        if(!ble && (n_dir > 1))
          n_dir++;
      }
      else
      {
        return NODIR;
      }
      break;
    }

    case SEEK:
      n_dir = find_seek(mzx_world, x, y);
      break;

    case RANDANY:
      n_dir = Random(4);
      break;

    case FLOW:
      if(flow_dir)
        n_dir = flow_dir - 1;
      else
        return IDLE;
      break;
  }

  // Rotate clockwise
  if(old_dir & CW)
  {
    if(n_dir >= SOUTH)
      n_dir ^= 1;

    n_dir ^= 2;
  }

  // Move opposite
  if(old_dir & OPP)
  {
    n_dir = (n_dir ^ 1);
  }

  // Randp
  if(old_dir & RANDP)
  {
    n_dir ^= 2;
    if(Random(2) == 1)
    {
      n_dir ^= 1;
    }
  }

  if(old_dir & RANDNOT)
  {
    int rval = Random(4);
    if(n_dir != rval)
    {
      n_dir = rval;
    }
    else
    {
      n_dir = 4;
    }
  }

  return (enum dir)(n_dir + 1);
}
