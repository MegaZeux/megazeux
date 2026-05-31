/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick
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

/* Constants */
#ifndef MEGAZEUX_CONST_H
#define MEGAZEUX_CONST_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

enum
{
  EXPL_LEAVE_SPACE,
  EXPL_LEAVE_ASH,
  EXPL_LEAVE_FIRE
};

enum
{
  CAN_SAVE,
  CANT_SAVE,
  CAN_SAVE_ON_SENSOR
};

enum
{
  FOREST_TO_EMPTY,
  FOREST_TO_FLOOR
};

enum
{
  FIRE_BURNS_LIMITED,
  FIRE_BURNS_FOREVER
};

enum
{
  OVERLAY_OFF,
  OVERLAY_ON,
  OVERLAY_STATIC,
  OVERLAY_TRANSPARENT,
  OVERLAY_FLAG_HIDE_BOARD = (1 << 6),
  OVERLAY_FLAG_HIDE_OVERLAY = (1 << 7),
  OVERLAY_MODE_MASK = 0x03
};

enum spittingtiger_moves
{
  SPITTINGTIGER_MOVES_NORMALLY              = 0,
  SPITTINGTIGER_MOVES_SLOWLY                = 1,
  SPITTINGTIGER_MOVES_SLOWLY_SELF_DESTRUCTS = 2
};

enum
{
  PLAYER_BULLET,
  NEUTRAL_BULLET,
  ENEMY_BULLET
};

#define NO_BOARD              255
#define NO_ENDGAME_BOARD      255
#define NO_DEATH_BOARD        255
#define TEMPORARY_BOARD       255
#define DEATH_SAME_POS        254

#define NO_KEY                127

// Length of time to display the message and the intro message.
#define MESG_TIMEOUT          160

// "BOARD_NAME_SIZE"/"ROBOT_NAME_SIZE"/"COUNTER_NAME_SIZE" include terminator
#define MAX_BOARDS            250
#define MAX_BOARD_SIZE        16 * 1024 * 1024
#define BOARD_NAME_SIZE       25
#define ROBOT_NAME_SIZE       15
#define COUNTER_NAME_SIZE     15 // This is legacy, for status counters only
#define NUM_KEYS              16

// Safe duplicates of the above strings in case those limits are lifted.
#define LEGACY_BOARD_NAME_SIZE 25
#define LEGACY_ROBOT_NAME_SIZE 15

// Attribute flags
#define A_PUSHNS              (1 << 0)
#define A_PUSHEW              (1 << 1)
#define A_PUSHABLE            (A_PUSHNS | A_PUSHEW)
#define A_ITEM                (1 << 2)
#define A_UPDATE              (1 << 3)
#define A_HURTS               (1 << 4)
#define A_UNDER               (1 << 5)
#define A_ENTRANCE            (1 << 6)
#define A_EXPLODE             (1 << 7)
#define A_BLOW_UP             (1 << 8)
#define A_SHOOTABLE           (1 << 9)
#define A_ENEMY               (1 << 10)
#define A_AFFECT_IF_STOOD     (1 << 11)
#define A_SPEC_SHOT           (1 << 12)
#define A_SPEC_PUSH           (1 << 13)
#define A_SPEC_BOMB           (1 << 14)
#define A_SPEC_STOOD          (1 << 15)

#define NUM_STATUS_COUNTERS   6

#define ROBOT_MAX_TR 512

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_CONST_H */
