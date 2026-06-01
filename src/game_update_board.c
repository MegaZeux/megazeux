/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#include <stdlib.h>
#include <string.h>

#include "const.h"
#include "core.h"
#include "counter.h"
#include "data.h"
#include "game_ops.h"
#include "game_player.h"
#include "game_update.h"
#include "idarray.h"
#include "idput.h"
#include "robot.h"
#include "sfx.h"
#include "world.h"
#include "util.h"

// For missile turning (directions)

static const int cwturndir[4] = { 2, 3, 1, 0 };
static const int ccwturndir[4] = { 3, 2, 0, 1 };

// OPEN DOOR movement directions, use bits 1,2,4,8,16 to index it.
// 0ffh=no movement.

static const char open_door_movement[] =
{
  3   , 0   , 2   , 0   , 3   , 1   , 2   , 1   ,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  2   , 1   , 3   , 1   , 2   , 0   , 3   , 0   ,
  1   , 2   , 1   , 3   , 0   , 2   , 0   , 3
};

// Bits for WAIT, in proper bit form, for opening doors. Use bits 1-16.

static const char open_door_max_wait[] =
{
  32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 ,
  224, 224, 224, 224, 224, 224, 224, 224,
  224, 224, 224, 224, 224, 224, 224, 224,
  32 , 32 , 32 , 32 , 32 , 32 , 32 , 32
};

// Increases current parameter and stores

static int inc_param(int param, int max)
{
  if(param == max)
  {
    return 0;
  }
  else
  {
    return param + 1;
  }
}

// This is the big one. Update all of the stuff on the screen..

void update_board(context *ctx)
{
  struct world *mzx_world = ctx->world;
  int i;
  int x, y;
  int level_offset;
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  boolean slow_down;
  enum thing current_id;
  char current_param;
  char current_color;
  enum thing current_under_id;
  char *update_done = mzx_world->update_done;

  // NOTE: already toggled.
  slow_down = mzx_world->current_cycle_odd;

  // Clear the status code of all robots
  for(i = 0; i < src_board->num_robots_active; i++)
  {
    cur_robot = src_board->robot_list_name_sorted[i];
    cur_robot->status = 0;
  }

  memset(update_done, 0, board_width * board_height);

  // The big update loop
  for(y = 0, level_offset = 0; y < board_height; y++)
  {
    for(x = 0; x < board_width; x++, level_offset++)
    {
      current_id = (enum thing)level_id[level_offset];

      // If the char's update done value is set or the id is < 25
      // (space trough W water) then there's nothing to do here;
      // go to the next one.

      if((update_done[level_offset] & 1) || (current_id < 25) ||
       !(flags[(int)current_id] & A_UPDATE))
      {
        continue;
      }

      current_param = level_param[level_offset];

      switch(current_id)
      {
        case ROBOT:
        case ROBOT_PUSHABLE:
        {
          run_robot(ctx, current_param, x, y);

          // On a game state change, we need to return to the main game loop.
          if(mzx_world->change_game_state)
            return;

          break;
        }

        case ICE:
        {
          // Start ice animation
          if(current_param == 0)
          {
            int rval = Random(256);
            if(rval == 0)
            {
              // Start anim at 1
              level_param[level_offset] = 1;
            }
          }
          else

          // End ice animation
          if(current_param == 3)
          {
            // Reset animation
            level_param[level_offset] = 0;
          }
          else
          {
            // Increase animation
            level_param[level_offset] = current_param + 1;
          }

          break;
        }

        case LAVA:
        {
          int r_val;

          if(slow_down) break;

          r_val = Random(256);

          if(r_val >= 20)
          {
            if(current_param == 2)
            {
              level_param[level_offset] = 0;
            }
            else
            {
              level_param[level_offset] = current_param + 1;
            }
          }
          break;
        }

        case FIRE:
        {
          // Get a random number
          int rval = Random(256);
          int new_x, new_y;
          // If it's less than 20, don't animate
          if(rval >= 20)
          {
            // If not at the end of the animation, increment it
            if(current_param != 5)
            {
              level_param[level_offset] = current_param + 1;
            }
            else
            {
              // Reset the animation
              level_param[level_offset] = 0;
            }
          }

          // Check under for water, ice, or lava; these all kill the fire.

          current_under_id = (enum thing)level_under_id[level_offset];
          if((current_under_id >= STILL_WATER) &&
           (current_under_id <= LAVA))
          {
            // Ice also melts into stillwater
            if(current_under_id == ICE)
            {
              level_under_id[level_offset] = (char)STILL_WATER;
              level_under_color[level_offset] = 25;
            }
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Otherwise the fire hurts and spreads and stuff, maybe dies
          // Random number says it might die or spread
          rval = Random(256);
          // No spread here
          if(rval > 8) break;
          // If it's 1, it should die, if fire doesn't burn forever
          // You would think dead fire can't spread, but in MZX it can!
          // Makes you wonder what the point of "fire burns forever" is.
          // This should possibly be changed later.
          if((rval == 1) && (src_board->fire_burns != FIRE_BURNS_FOREVER))
          {
            // Fire turns into dark grey ash
            level_id[level_offset] = (char)FLOOR;
            level_color[level_offset] = 8;
          }

          // Put fire all around
          for(i = 3; i >= 0; i--)
          {
            if(!xy_shift_dir(src_board, x, y, &new_x, &new_y, i))
            {
              // Get offset
              int offset = xy_to_offset(src_board, new_x, new_y);
              int place = 0;
              // Save ID and param there
              enum thing new_id = (enum thing)level_id[offset];
              char new_color = level_color[offset];

              // Fire does things to certain ID's
              if((new_id == SPACE) &&
               (src_board->fire_burn_space == 1))
              {
                place = 1;
              }

              // Fake
              if((new_id >= FAKE) && (new_id <= THICK_WEB) &&
               (src_board->fire_burn_fakes == 1))
              {
                place = 1;
              }

              if((new_id == TREE) &&
               (src_board->fire_burn_trees == 1))
              {
                place = 1;
              }

              // Brown stuff
              // Don't burn scrolls, signs, sensors, robots, or the player
              if((new_color == 6) &&
               (src_board->fire_burn_brown == 1) && (new_id < SENSOR))
              {
                place = 1;
              }

              // Player
              if(new_id == PLAYER)
              {
                // Make sure player can't.. walk on fire.
                if(mzx_world->firewalker_dur == 0)
                {
                  hurt_player(mzx_world, FIRE);
                }
              }

              if(place == 1)
              {
                // Place a new fire
                id_place(mzx_world, new_x, new_y, FIRE, 0, 0);
              }
            }
          }

          break;
        }

        case BULLET:
        {
          int bullettype = current_param >> 2;
          int direction = current_param & 0x03;
          // Erase at the current position
          id_remove_top(mzx_world, x, y);
          // Place a new one
          shoot(mzx_world, x, y, direction, bullettype);

          break;
        }

        case EXPLOSION:
        {
          int stage = current_param & 0x0F;
          // Stage zero means climb outwards
          if(!stage)
          {
            // Move the explosion outward if it has any more to go.
            if(current_param & 0xF0)
            {
              int new_x, new_y;
              // Decrease stage
              current_param -= 0x10;
              level_param[level_offset] = current_param;

              // Put explosion at each direction
              for(i = 3; i >= 0; i--)
              {
                if(!xy_shift_dir(src_board, x, y, &new_x, &new_y, i))
                {
                  // Get offset
                  int offset = xy_to_offset(src_board, new_x, new_y);
                  // Save ID and param there
                  enum thing new_id = (enum thing)level_id[offset];
                  char new_param = level_param[offset];
                  // Get the flags for that ID
                  int flag = flags[(int)new_id];

                  // Should a new explosion be placed here?
                  // Also, some further things can happen.
                  // Does what's there..
                  // Blow up (removed by explosion)?
                  // Slimes, ghosts, and dragons might also die if the
                  // bit isn't set
                  if((flag & A_BLOW_UP) ||
                   ((new_id == SLIMEBLOB) && !(new_param & 0x80)) ||
                   ((new_id == GHOST) && !(new_param & 0x08)) ||
                   ((new_id == DRAGON) && !(new_param & 0xE0)))
                  {
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a new explosion (one level down)
                    id_place(mzx_world, new_x, new_y, EXPLOSION,
                     0, current_param);
                    continue;
                  }

                  // Explode? (create a new explosion)
                  if(flag & A_EXPLODE)
                  {
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a fresh new explosion
                    id_place(mzx_world, new_x, new_y, EXPLOSION, 0, 64);
                    continue;
                  }

                  // If it can be pushed under, place an explosion over it
                  if(flag & A_UNDER)
                  {
                    // Place a new explosion over it.
                    id_place(mzx_world, new_x, new_y, EXPLOSION,
                     0, current_param);
                    continue;
                  }

                  // Now, see if it's something that reacts to explosions
                  // Player takes damage
                  if(new_id == PLAYER)
                  {
                    hurt_player(mzx_world, EXPLOSION);
                    continue;
                  }

                  // Mine explodes
                  if(new_id == MINE)
                  {
                    // Take explosion radius from param
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a fresh new explosion
                    id_place(mzx_world, new_x, new_y, EXPLOSION,
                     0, new_param & 0xF0);
                    continue;
                  }

                  // Robot gets sent a label
                  if(is_robot(new_id))
                  {
                    // Send bombed label
                    send_robot_def(mzx_world, new_param, LABEL_BOMBED);
                    continue;
                  }

                  // Dragon takes damage, if it wasn't already killed
                  if(new_id == DRAGON)
                  {
                    // Decrease HP
                    level_param[offset] = new_param - 0x20;
                    continue;
                  }
                }
              }
            }
          }

          // See if it's at stage 3, if so it's done
          if(stage == 3)
          {
            // Get what's underneath it
            current_under_id =
             (enum thing)level_under_id[level_offset];
            // Leave space if over goop, water, lava, or ice
            if((current_under_id == GOOP) ||
             ((current_under_id >= STILL_WATER) &&
             (current_under_id <= LAVA)))
            {
              // Leave space
              id_remove_top(mzx_world, x, y);
              break;
            }

            // Is under an entrance? Explosion leaves space? Leave space.
            if((flags[(int)current_under_id] & A_ENTRANCE)
             || src_board->explosions_leave == EXPL_LEAVE_SPACE)
            {
              // Leave space
              id_remove_top(mzx_world, x, y);
              break;
            }
            // Leave ash?
            if(src_board->explosions_leave == EXPL_LEAVE_ASH)
            {
              // Leave ash if the params say so
              level_id[level_offset] = (char)FLOOR;
              level_color[level_offset] = 8;
              break;
            }
            // Otherwise leave fire
            level_id[level_offset] = (char)FIRE;
            level_param[level_offset] = 0;
          }
          else
          {
            // Otherwise go to the next stage
            level_param[level_offset] = current_param + 1;
          }
          break;
        }

        case CW_ROTATE:
        case CCW_ROTATE:
        {
          if(slow_down)
            break;

          level_param[level_offset] = inc_param(current_param, 3);
          rotate(mzx_world, x, y, current_id - CW_ROTATE);
          break;
        }

        case TRANSPORT:
        {
          if(slow_down)
            break;

          // Is it still animating?
          if((current_param & 0x18) != 0x18)
          {
            // Increase animation
            level_param[level_offset] = current_param + 0x08;
          }
          else
          {
            // Otherwise, erase anim bits
            level_param[level_offset] = current_param & 0xE7;
          }
          break;
        }

        case SHOOTING_FIRE:
        {
          enum move_status status;
          // Get direction
          int m_dir = current_param >> 1;
          // Flip animation and store
          current_param ^= 1;
          level_param[level_offset] = current_param;
          // Try moving

          /* Note: directions >3 always result in a self-collision. */
          status = move(mzx_world, x, y, m_dir,
           CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK |
           REACT_PLAYER | CAN_GOOPWALK | SPITFIRE);

          if(status == HIT_PLAYER)
          {
            // Hit player; hurt the player and die
            hurt_player(mzx_world, SHOOTING_FIRE);
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            if(status != NO_HIT)
            {
              // Didn't hit the player.. check stuff
              current_under_id =
               (enum thing)level_under_id[level_offset];
              // See if it hit an entrance
              if((current_under_id == STAIRS) ||
               (current_under_id == CAVE) ||
               ((current_under_id >= WHIRLPOOL_1) &&
               (current_under_id <= WHIRLPOOL_4)))
              {
                id_remove_top(mzx_world, x, y);
              }
              else
              {
                // Put fire
                level_id[level_offset] = (char)FIRE;
                level_param[level_offset] = 0;
              }
            }
          }

          break;
        }

        case MISSILE:
        {
          enum move_status status = HIT_PLAYER;
          int move_params =
           CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK |
           REACT_PLAYER | CAN_GOOPWALK;

          // Param is the direction
          /* Note: directions >3 always result in a self collision.
           * Rotating an invalid direction is also an invalid direction,
           * so skip all of the checks and just explode if this happens.
           * Rotating an invalid direction would read junk prior to 2.93d */
          if((unsigned)current_param <= 3)
            status = move(mzx_world, x, y, current_param, move_params);

          // Did it hit something that's not the player?
          if((status == HIT) || (status == HIT_EDGE))
          {
            // Otherwise change direction; try cw then ccw
            int new_direction = cwturndir[(int)current_param];
            level_param[level_offset] = new_direction;
            status = move(mzx_world, x, y, new_direction, move_params);
            // Did it hit something that's not the player? Try ccw.
            if((status == HIT) || (status == HIT_EDGE))
            {
              new_direction = ccwturndir[(int)current_param];
              level_param[level_offset] = new_direction;
              status = move(mzx_world, x, y, new_direction, move_params);
              if(status)
                status = HIT_PLAYER;
            }
          }

          // Did it hit the player?
          if(status == HIT_PLAYER)
          {
            // If so, leave explosion
            level_id[level_offset] = (char)EXPLOSION;
            level_param[level_offset] = 48;
            play_sfx(mzx_world, SFX_EXPLOSION);
          }

          break;
        }

        case SEEKER:
        {
          int seek_dir;

          if(slow_down) break;
          if(current_param == 0)
          {
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            level_param[level_offset] = current_param - 1;
            seek_dir = find_seek(mzx_world, x, y);
            if(move(mzx_world, x, y, seek_dir,
             CAN_PUSH | CAN_LAVAWALK | CAN_FIREWALK |
             CAN_WATERWALK | REACT_PLAYER | CAN_GOOPWALK) ==
             HIT_PLAYER)
            {
              hurt_player(mzx_world, SEEKER);
              id_remove_top(mzx_world, x, y);
            }
          }
          break;
        }

        case LAZER:
        {
          // Decrease time until death
          // If it's less than 8, kill it
          current_param -= 8;
          if(current_param < 8)
          {
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            if(slow_down) break;
            // Animate every other cycle
            if((current_param & 0x06) == 0x06)
            {
              // Otherwise, erase anim bits
              level_param[level_offset] = current_param & 0xF9;
            }
            else
            {
              // Increase animation
              level_param[level_offset] = current_param + 2;
            }
          }
          break;
        }

        case PUSHER:
        {
          /* Note: directions >3 always result in a self-collision. */
          if(!slow_down)
            move(mzx_world, x, y, current_param, CAN_PUSH);

          break;
        }

        case SNAKE:
        {
          enum move_status status;

          // Flip count and store
          current_param ^= 8;
          level_param[level_offset] = current_param;

          // If the flipflop or fast-movement is set, move
          if((current_param & 0x08) || !(current_param & 0x04))
          {
            // Try move
            status = move(mzx_world, x, y, current_param & 0x03,
             REACT_PLAYER);
            // See if it hit the player
            if(status == HIT_PLAYER)
            {
              // If so, take damage and die
              hurt_player(mzx_world, SNAKE);
              id_remove_top(mzx_world, x, y);
              break;
            }

            // Otherwise, if it didn't move, get a new direction
            if(status != NO_HIT)
            {
              int intelligence = (current_param & 0x70) >> 4;
              int rval = Random(8);
              int m_dir;

              // Mask out direction
              current_param &= 0xFC;

              // If it's less than intelligence, make a "smart" move
              if(rval < intelligence)
              {
                m_dir = find_seek(mzx_world, x, y);
              }
              else
              {
                // Get a number between 0 and 3
                m_dir = Random(4);
              }

              // Set direction
              current_param |= m_dir;
              level_param[level_offset] = current_param;
            }
          }
          break;
        }

        case EYE:
        {
          // Flip count and store
          current_param ^= 0x80;
          level_param[level_offset] = current_param;

          // If the flipflop or fast-movement is set, move
          if((current_param & 0x80) || !(current_param & 0x40))
          {
            int intelligence = current_param & 7;
            int rval = Random(8);
            int m_dir;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              // Get a number between 0 and 3
              m_dir = Random(4);
            }

            if(move(mzx_world, x, y, m_dir,
             CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK | REACT_PLAYER |
             CAN_GOOPWALK) == HIT_PLAYER)
            {
              // Hit the player. Get the blast radius.
              int radius = (current_param & 0x38) << 1;
              // Explode (place explosion)
              level_id[level_offset] = EXPLOSION;
              level_param[level_offset] = radius;
              play_sfx(mzx_world, SFX_EXPLOSION);
            }
          }
          break;
        }

        case THIEF:
        {
          // Movement rate, 1-4
          int move_speed = current_param & 0x18;
          // Movement counter
          int move_cycle = (current_param & 0x60) >> 2;

          // Should the thief move?
          if(move_cycle == move_speed)
          {
            int intelligence = current_param & 0x07;
            int rval = Random(8);
            int m_dir;

            // Zero out move cycle
            level_param[level_offset] = current_param & 0x9F;

            // See if a "smart move" should be made
            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = Random(4);
            }

            // Move and see if it hit the player
            if(move(mzx_world, x, y, m_dir, REACT_PLAYER) == HIT_PLAYER)
            {
              // Get amount of gems to take
              int gems_take = ((current_param & 0x80) >> 7) + 1;
              // Take them
              dec_counter(mzx_world, "GEMS", gems_take, 0);
              play_sfx(mzx_world, SFX_STOLEN_GEM);
            }
          }
          else
          {
            // Increment move cycle
            level_param[level_offset] = current_param + 0x20;
          }

          break;
        }

        case SLIMEBLOB:
        {
          if(!slow_down)
          {
            int spread_speed = current_param & 0x03;
            int current_cycle = (current_param & 0x3C) >> 2;

            // This was changed from speed*5 some time between 2.04 and 2.07.
            // Despite MegaZeux 2.x having changed the speed setting range
            // from 1-4 to 1-8, this setting has always been two bits.
            // BUG: The newer behavior seems to be a broken attempt at speed*3.
            if(mzx_world->version >= V251)
              spread_speed |= spread_speed << 1;
            else
              spread_speed |= spread_speed << 2;

            if(spread_speed == current_cycle)
            {
              int new_x, new_y;
              current_color = level_color[level_offset];
              // Clear cycle
              // BUG: This leaves the lowest cycle count bit set.
              // This bug exists even in MegaZeux 1.x.
              current_param &= 0xC7;

              // Put slimes all around
              for(i = 3; i >= 0; i--)
              {
                if(!xy_shift_dir(src_board, x, y, &new_x, &new_y, i))
                {
                  // Get offset
                  int offset = xy_to_offset(src_board, new_x, new_y);
                  // Save ID and param there
                  enum thing new_id = (enum thing)level_id[offset];

                  // See if it hits a fake
                  if(is_fake(new_id))
                  {
                    // Put a slime
                    id_place(mzx_world, new_x, new_y, SLIMEBLOB,
                     current_color, current_param);
                  }
                  else
                  {
                    // See if it hits the player and hurts the player
                    if((new_id == PLAYER) && (current_param & 0x40))
                    {
                      // Take damage
                      hurt_player(mzx_world, SLIMEBLOB);
                    }
                  }
                  // Put a breakaway
                  id_place(mzx_world, x, y, BREAKAWAY, current_color, 0);
                }
              }
            }
            else
            {
              // Increase cycle
              // BUG: corrupting overflow.
              level_param[level_offset] = current_param + 4;
            }
          }
          break;
        }

        case RUNNER:
        {
          int speed = current_param & 0x0C;
          int cycle = (current_param & 0x30) >> 2;

          if(cycle == speed)
          {
            // Get direction
            int direction = current_param & 0x03;
            enum move_status status;
            // Clear cycle
            level_param[level_offset] = current_param & 0xCF;

            status = move(mzx_world, x, y, direction,
             CAN_PUSH | CAN_TRANSPORT | REACT_PLAYER);
            // Did the move not go through?
            if(status != NO_HIT)
            {
              // Did it hit the player?
              if(status == HIT_PLAYER)
              {
                // Hurt the player and die
                hurt_player(mzx_world, RUNNER);
                id_remove_top(mzx_world, x, y);
                break;
              }
              else
              {
                // Change direction
                level_param[level_offset] = current_param ^ 1;
              }
            }
          }
          else
          {
            // Increase cycle
            level_param[level_offset] = current_param + 0x10;
          }

          break;
        }

        case GHOST:
        {
          int speed = current_param & 0x30;
          int cycle = (current_param & 0xC0) >> 2;

          if(speed == cycle)
          {
            // Get intelligence
            int intelligence = current_param & 0x07;
            int rval = Random(8);
            int m_dir;
            // Clear cycle
            level_param[level_offset] = current_param & 0x3F;

            if(rval < intelligence)
            {
              // Smart move
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = Random(4);
            }

            // Try move, did it hit the player?
            if(move(mzx_world, x, y, m_dir,
             CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK |
             REACT_PLAYER | CAN_GOOPWALK) == HIT_PLAYER)
            {
              // Take damage
              hurt_player(mzx_world, GHOST);
              // Only die if not invincible
              if(!(current_param & 0x08))
              {
                id_remove_top(mzx_world, x, y);
                break;
              }
            }
          }
          else
          {
            // Increase cycle
            level_param[level_offset] = current_param + 0x40;
          }

          break;
        }

        case DRAGON:
        {
          int fire_rate;
          int rval = -1;

          // Can it move?
          if(current_param & 0x04)
          {
            int cycle = current_param & 0x18;
            if(cycle == 0x18)
            {
              int m_dir;
              enum move_status status;
              // Zero out movement
              level_param[level_offset] = current_param & 0xE7;

              // One out of 8 moves is random
              if(src_board->dragons_can_randomly_move)
                rval = Random(8);

              if(!(rval & 0x07))
              {
                m_dir = Random(4);
              }
              else
              {
                // Otherwise make a smart move
                m_dir = find_seek(mzx_world, x, y);
              }

              status = move(mzx_world, x, y, m_dir,
               CAN_LAVAWALK | CAN_FIREWALK | REACT_PLAYER);

              // Is it blocked?
              if(status != NO_HIT)
              {
                // Did it hit the player?
                if(status == HIT_PLAYER)
                {
                  // Dragons don't die when hit by the player
                  hurt_player(mzx_world, DRAGON);
                }

                // Can't shoot, so get out of here
                break;
              }
            }
            else
            {
              level_param[level_offset] = current_param + 8;
            }
          }

          // Shoot; get the fire rate
          fire_rate = current_param & 0x03;
          rval = Random(16);

          // Should it fire?
          if(rval < fire_rate)
          {
            // Shoot in the direction of the player
            int fire_dir = find_seek(mzx_world, x, y);
            shoot_fire(mzx_world, x, y, fire_dir);
            play_sfx(mzx_world, SFX_DRAGON_FIRE);
          }

          break;
        }

        case FISH:
        {
          // Is the cycle count ready or is fast movement on?
          if((current_param & 0x10) || !(current_param & 0x08))
          {
            unsigned m_dir = (unsigned)(level_under_id[level_offset] - N_WATER);

            // Toggle cycle flipflop off
            level_param[level_offset] = current_param & 0xEF;

            // Is there not water there or is it not affected by it?
            if(!(current_param & 0x20) || (m_dir > 3))
            {
              int intelligence = current_param & 0x07;
              int rval = Random(8);
              if(rval < intelligence)
              {
                m_dir = find_seek(mzx_world, x, y);
              }
              else
              {
                m_dir = Random(4);
              }
            }

            // Move. Did it hit the player and does the
            // player hurt fish?
            if((move(mzx_world, x, y, m_dir, CAN_WATERWALK |
             REACT_PLAYER | MUST_WATER) == HIT_PLAYER) &&
             (current_param & 0x40))
            {
              hurt_player(mzx_world, FISH);
              id_remove_top(mzx_world, x, y);
            }
          }
          else
          {
            // Toggle cycle flipflop on
            level_param[level_offset] = current_param | 0x10;
          }

          break;
        }

        case SHARK:
        {
          int intelligence;
          int rval;
          int m_dir;
          enum move_status status;
          int new_x = 0, new_y = 0;
          int fire_rate;
          int shoot_type;

          if(slow_down) break;

          intelligence = current_param & 0x07;
          rval = Random(8);

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            m_dir = Random(4);
          }

          // Hit player and die
          status = move(mzx_world, x, y, m_dir,
           MUST_LAVAGOOP | REACT_PLAYER | CAN_GOOPWALK | CAN_LAVAWALK);

          if(status == HIT_PLAYER)
          {
            hurt_player(mzx_world, SHARK);
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Should shoot in the direction it moved.

          // If blocked, keep old x/y, otherwise update
          if(status != NO_HIT)
          {
            new_x = x;
            new_y = y;
          }
          else
          {
            xy_shift_dir(src_board, x, y, &new_x, &new_y, m_dir);
          }

          // Get shark fire rate
          fire_rate = (current_param & 0xE0) >> 5;
          // Get the type of thing it shoots
          shoot_type = current_param & 0x18;
          // Get a number between 0 and 31
          rval = Random(32);

          // Is the random number <= the rate? Also, does it shoot?
          if((rval <= fire_rate) && (shoot_type != 24))
          {
            // Shoot seeker
            if(shoot_type == 8)
            {
              shoot_seeker(mzx_world, new_x, new_y, m_dir);
            }
            else

            // Shoot fire
            if(shoot_type == 16)
            {
              shoot_fire(mzx_world, new_x, new_y, m_dir);
              play_sfx(mzx_world, SFX_DRAGON_FIRE);
            }
            else
            {
              // Shoot bullet
              shoot(mzx_world, new_x, new_y, m_dir, ENEMY_BULLET);
            }
          }

          break;
        }

        case SPIDER:
        {
          // Toggle cycle flipflop
          current_param ^= 0x40;
          level_param[level_offset] = current_param;

          // Is the cycle count ready or is fast movement on?
          if((current_param & 0x40) || !(current_param & 0x20))
          {
            int intelligence = current_param & 0x07;
            int rval = Random(8);
            int m_dir;
            // Set these flags
            int flags = 128;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = Random(4);
            }
            flags |= ((current_param & 0x18) << 2) + 32;

            if(move(mzx_world, x, y, m_dir, flags) == HIT_PLAYER)
            {
              hurt_player(mzx_world, SPIDER);
              id_remove_top(mzx_world, x, y);
            }
          }

          break;
        }

        case GOBLIN:
        {
          int move_rate;
          int move_cycle;

          if(slow_down) break;

          // Should it pause?
          if(current_param & 0x20)
          {
            move_rate = ((current_param & 0x03) << 1) + 1;
            move_cycle = ((current_param & 0x1C) >> 2);

            if(move_rate == move_cycle)
            {
              // Zero out move cycle and move bit
              level_param[level_offset] = current_param & 0xC3;
            }
            else
            {
              // Increase move cycle
              level_param[level_offset] = current_param + 4;
            }
          }
          else
          {
            int intelligence = (current_param & 0xC0) >> 6;
            int rval = Random(4);
            int m_dir;

            // Otherwise, move
            current_param += 4;
            // If the non-pause cycle is maxed out
            if((current_param & 0x1C) == 0x14)
            {
              // Pause next time
              current_param = (current_param & 0xE3) | 0x20;
            }

            level_param[level_offset] = current_param;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = Random(4);
            }

            // Try to move, does it hit the player, etc.
            if(move(mzx_world, x, y, m_dir, CAN_WATERWALK |
             REACT_PLAYER) == HIT_PLAYER)
            {
              hurt_player(mzx_world, GOBLIN);
              id_remove_top(mzx_world, x, y);
            }
          }

          break;
        }

        case SPITTING_TIGER:
        {
          int intelligence;
          int rval;
          int m_dir;
          enum move_status status;
          int new_x = 0, new_y = 0;
          int fire_rate;
          int shoot_type;
          boolean no_shoot = false;

          if(slow_down) break;

          intelligence = current_param & 0x07;
          rval = Random(8);

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            // From 2.80 until 2.94, SpittingTigers used the wrong value here,
            // resulting in directions 4-7. Invalid dirs result in no movement
            // and self-destruction with bullets when Enemies Hurt Enemies is on.
            if(src_board->spittingtiger_moves >= SPITTINGTIGER_MOVES_SLOWLY)
            {
              m_dir = Random(8);
              // 2.94+: allow enabling slow tigers without the self-destruct bug.
              if(m_dir > 3 && src_board->spittingtiger_moves <
               SPITTINGTIGER_MOVES_SLOWLY_SELF_DESTRUCTS)
                no_shoot = true;
            }
            else
              m_dir = Random(4);
          }

          // Hit player and die
          status = move(mzx_world, x, y, m_dir,
           CAN_WATERWALK | REACT_PLAYER);

          if(status == HIT_PLAYER)
          {
            hurt_player(mzx_world, SPITTING_TIGER);
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Should shoot in the direction it moved.

          // If blocked, keep old x/y, otherwise update
          if(status)
          {
            new_x = x;
            new_y = y;
          }
          else
          {
            xy_shift_dir(src_board, x, y, &new_x, &new_y, m_dir);
          }

          // Get tiger fire rate
          fire_rate = (current_param & 0xE0) >> 5;
          // Get the type of thing it shoots
          shoot_type = current_param & 0x18;
          // Get a number between 0 and 31
          rval = Random(32);

          // Is the random number <= the rate? Also, does it shoot?
          if((rval <= fire_rate) && (shoot_type != 24) && !no_shoot)
          {
            // Shoot seeker
            if(shoot_type == 8)
            {
              shoot_seeker(mzx_world, new_x, new_y, m_dir);
            }
            else

            // Shoot fire
            if(shoot_type == 16)
            {
              shoot_fire(mzx_world, new_x, new_y, m_dir);
              play_sfx(mzx_world, SFX_DRAGON_FIRE);
            }
            else
            {
              // Shoot bullet
              shoot(mzx_world, new_x, new_y, m_dir, ENEMY_BULLET);
            }
          }

          break;
        }

        case SPINNING_GUN:
        {
          // A spinnning gun is a bulletgun that spins. So it can fall
          // through for now, but this might get changed with refactoring..
          if(slow_down)
            break;

          // Update animation and direction

          if((current_param & 0x18) == 0x18)
          {
            current_param &= 0xE7;
          }
          else
          {
            current_param += 8;
          }

          level_param[level_offset] = current_param;
        }

        /* fallthrough */

        case BULLET_GUN:
        {
          int shoot_rate;
          int rval;

          if(slow_down)
            break;

          shoot_rate = current_param & 0x07;
          rval = Random(16);

          // Shoot now.. maybe
          if(rval <= shoot_rate)
          {
            int intelligence = current_param & 0x60;
            int should_shoot = 0;
            int direction = (current_param & 0x18) >> 3;
            rval = Random(4) << 5;

            // Is the player aligned with the gun?
            if(find_seek(mzx_world, x, y) == direction)
            {
              // If so, should shoot
              should_shoot = 1;
            }

            // Unless the gun is stupid, then do the opposite
            if(rval > intelligence)
            {
              should_shoot ^= 1;
            }

            if(should_shoot)
            {
              // See what to shoot
              if(current_param & 0x80)
              {
                // Shoot fire
                shoot_fire(mzx_world, x, y, direction);
              }
              else
              {
                // Shoot bullet
                shoot(mzx_world, x, y, direction, ENEMY_BULLET);
              }
            }
          }

          break;
        }

        case BEAR:
        {
          int move_rate = current_param & 0x18;
          int move_cycle = (current_param & 0x60) >> 2;

          if(move_rate == move_cycle)
          {
            int sensitivity = ((current_param & 0x07) + 1) << 1;
            int player_distance;
            int player_dist_x = mzx_world->player_x - x;
            int player_dist_y = mzx_world->player_y - y;

            // Zero out move cycle
            level_param[level_offset] &= 0x9F;

            if(player_dist_x < 0) player_dist_x = -player_dist_x;
            if(player_dist_y < 0) player_dist_y = -player_dist_y;
            player_distance = player_dist_x + player_dist_y;

            if(player_distance <= sensitivity)
            {
              // Move in the player's direction, always
              if(move(mzx_world, x, y, find_seek(mzx_world, x, y),
               CAN_WATERWALK | REACT_PLAYER) == HIT_PLAYER)
              {
                hurt_player(mzx_world, BEAR);
                id_remove_top(mzx_world, x, y);
              }
            }
          }
          else
          {
            level_param[level_offset] = current_param + 0x20;
          }

          break;
        }

        case BEAR_CUB:
        {
          int switch_rate;
          int switch_cycle;
          int intelligence;
          int rval;
          int m_dir;

          if(slow_down) break;

          switch_rate = ((current_param & 0x0C) >> 1) + 1;
          switch_cycle = (current_param & 0x70) >> 4;

          // Should it switch from chasing/running away from the player?
          if(switch_cycle == switch_rate)
          {
            // Zero out the switch cycle
            current_param &= 0x8F;
            // Toggle the switch
            current_param ^= 0x80;
          }
          else
          {
            // Increase switch time
            current_param += 16;
          }

          level_param[level_offset] = current_param;

          intelligence = current_param & 0x03;
          rval = Random(4);

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            m_dir = Random(4);
          }

          // Should it run in that direction or the opposite one?
          if(current_param & 0x80)
          {
            m_dir = flip_dir(m_dir);
          }

          if(move(mzx_world, x, y, m_dir, CAN_WATERWALK |
           REACT_PLAYER) == HIT_PLAYER)
          {
            hurt_player(mzx_world, BEAR_CUB);
            id_remove_top(mzx_world, x, y);
          }

          break;
        }

        case ENERGIZER:
        {
          level_param[level_offset] = inc_param(current_param, 7);
          break;
        }

        case LIT_BOMB:
        {
          if(!slow_down)
          {
            if((current_param & 6) == 6)
            {
              // Is it a highbomb?
              if(current_param & 0x80)
              {
                level_param[level_offset] = 0x40;
              }
              else
              {
                level_param[level_offset] = 0x20;
              }

              level_id[level_offset] = (char)EXPLOSION;
              play_sfx(mzx_world, SFX_EXPLOSION);
            }
            else
            {
              level_param[level_offset] = current_param + 1;
            }
          }
          break;
        }

        case OPEN_DOOR:
        {
          int curr_wait = current_param & 0xE0;
          // Get the door stage
          int stage = current_param & 0x1F;
          int door_wait = open_door_max_wait[stage];
          int door_move = open_door_movement[stage];

          if(curr_wait == door_wait)
          {
            // Is it at stage 3?
            if((current_param & 0x18) == 0x18)
            {
              // Turn into a regular door
              level_param[level_offset] = current_param & 0x07;
              level_id[level_offset] = (char)DOOR;
            }
            else
            {
              // Otherwise, add to, reset the wait the stage
              level_param[level_offset] = stage + 8;
            }

            // Only do this if movement is possible
            if(door_move != 0xFF)
            {
              if(move(mzx_world, x, y, door_move, CAN_PUSH |
               CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK) != NO_HIT)
              {
                // Reset the param and make the door open
                level_id[level_offset] = OPEN_DOOR;
                level_param[level_offset] = current_param;
              }
            }
          }
          else
          {
            level_param[level_offset] = current_param + 0x20;
          }
          break;
        }

        case OPEN_GATE:
        {
          // End of wait? Close it.
          if(current_param == 0)
          {
            // Make it closed gate
            level_id[level_offset] = (char)GATE;
            play_sfx(mzx_world, SFX_GATE_CLOSE);
          }
          else
          {
            // Decrease wait
            level_param[level_offset] = current_param - 1;
          }

          break;
        }

        case N_MOVING_WALL:
        case S_MOVING_WALL:
        case E_MOVING_WALL:
        case W_MOVING_WALL:
        {
          // Get direction
          int m_dir = (int)current_id - N_MOVING_WALL;
          // Try the move
          if(move(mzx_world, x, y, m_dir, CAN_PUSH |
           CAN_TRANSPORT | CAN_LAVAWALK | CAN_FIREWALK |
           CAN_WATERWALK) != NO_HIT)
          {
            // Can't move; try other direction
            level_id[level_offset] = flip_dir(m_dir) + N_MOVING_WALL;
          }
          break;
        }

        case LAZER_GUN:
        {
          // Look at the param
          int start_time = (current_param & 0x1C) >> 2;
          // Must be the correct start time
          if(start_time == src_board->lazwall_start)
          {
            char length = ((current_param & 0xE0) >> 5) + 1;
            char direction = (current_param & 0x03);
            current_color = level_color[level_offset];
            shoot_lazer(mzx_world, x, y, direction, length, current_color);
          }
          break;
        }

        case LIFE:
        {
          if(!slow_down)
            level_param[level_offset] = inc_param(current_param, 3);

          break;
        }

        // Whirlpool
        case WHIRLPOOL_1:
        case WHIRLPOOL_2:
        case WHIRLPOOL_3:
        case WHIRLPOOL_4:
        {
          if(!slow_down)
          {
            // Is it the last phase?
            if(current_id == WHIRLPOOL_4)
            {
              // Loop back
              level_id[level_offset] = WHIRLPOOL_1;
            }
            else
            {
              // Increase "frame"
              level_id[level_offset] = current_id + 1;
            }
          }

          break;
        }

        case MINE:
        {
          // Should it go to the next phase?
          if((current_param & 0x0E) != 0x0E)
          {
            level_param[level_offset] = current_param + 2;
          }
          else
          {
            // Animate
            level_param[level_offset] = (current_param & 0xF1) ^ 1;
          }
          break;
        }

        case MISSILE_GUN:
        {
          // Apparently, missile guns have intelligence, but this
          // doesn't even affect anything. Hrm...

          int fire_rate;
          int m_dir;
          int rval;

          if(!slow_down)
          {
            fire_rate = current_param & 0x07;
            rval = Random(2) << 5;
            m_dir = find_seek(mzx_world, x, y);

            // Fire rate of seven means always shoot
            // Only shoot if the gun is facing the player
            if(((rval < fire_rate) || (fire_rate == 7)) &&
             (m_dir == ((current_param >> 3) & 0x03)))
            {
              shoot_missile(mzx_world, x, y, m_dir);
              // Should it die now?
              if(!(current_param & 0x80))
              {
                id_remove_top(mzx_world, x, y);
              }
            }
          }

          break;
        }

        default:
        {
          break;
        }
      }
    }
  }

  // Run all of the robots _again_, this time in reverse order.
  level_offset = (board_width * board_height) - 1;

  for(y = board_height - 1; y >= 0; y--)
  {
    for(x = board_width - 1; x >= 0; x--)
    {
      current_id = (enum thing)level_id[level_offset];
      if(is_robot(current_id))
      {
        current_param = level_param[level_offset];

        // May change the source board (with swap world or load game)
        run_robot(ctx, -current_param, x, y);

        // On a game state change, we need to return to the main game loop.
        if(mzx_world->change_game_state)
          return;
      }
      level_offset--;
    }
  }

  find_player(mzx_world);
}
