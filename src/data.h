/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_DATA_H
#define MEGAZEUX_DATA_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "const.h"
#include <stdio.h>
#include <stdlib.h>

CORE_LIBSPEC extern char curr_file[MAX_PATH];
CORE_LIBSPEC extern char curr_sav[MAX_PATH];
CORE_LIBSPEC extern char current_dir[MAX_PATH];
CORE_LIBSPEC extern char config_dir[MAX_PATH];

CORE_LIBSPEC extern unsigned char scroll_color;
CORE_LIBSPEC extern const char *const thing_names[128];
CORE_LIBSPEC extern const unsigned int flags[128];

enum thing
{
  SPACE           = 0,
  NORMAL          = 1,
  SOLID           = 2,
  TREE            = 3,
  LINE            = 4,
  CUSTOM_BLOCK    = 5,
  BREAKAWAY       = 6,
  CUSTOM_BREAK    = 7,
  BOULDER         = 8,
  CRATE           = 9,
  CUSTOM_PUSH     = 10,
  BOX             = 11,
  CUSTOM_BOX      = 12,
  FAKE            = 13,
  CARPET          = 14,
  FLOOR           = 15,
  TILES           = 16,
  CUSTOM_FLOOR    = 17,
  WEB             = 18,
  THICK_WEB       = 19,
  STILL_WATER     = 20,
  N_WATER         = 21,
  S_WATER         = 22,
  E_WATER         = 23,
  W_WATER         = 24,
  ICE             = 25,
  LAVA            = 26,
  CHEST           = 27,
  GEM             = 28,
  MAGIC_GEM       = 29,
  HEALTH          = 30,
  RING            = 31,
  POTION          = 32,
  ENERGIZER       = 33,
  GOOP            = 34,
  AMMO            = 35,
  BOMB            = 36,
  LIT_BOMB        = 37,
  EXPLOSION       = 38,
  KEY             = 39,
  LOCK            = 40,
  DOOR            = 41,
  OPEN_DOOR       = 42,
  STAIRS          = 43,
  CAVE            = 44,
  CW_ROTATE       = 45,
  CCW_ROTATE      = 46,
  GATE            = 47,
  OPEN_GATE       = 48,
  TRANSPORT       = 49,
  COIN            = 50,
  N_MOVING_WALL   = 51,
  S_MOVING_WALL   = 52,
  E_MOVING_WALL   = 53,
  W_MOVING_WALL   = 54,
  POUCH           = 55,
  PUSHER          = 56,
  SLIDER_NS       = 57,
  SLIDER_EW       = 58,
  LAZER           = 59,
  LAZER_GUN       = 60,
  BULLET          = 61,
  MISSILE         = 62,
  FIRE            = 63,
  FOREST          = 65,
  LIFE            = 66,
  WHIRLPOOL_1     = 67,
  WHIRLPOOL_2     = 68,
  WHIRLPOOL_3     = 69,
  WHIRLPOOL_4     = 70,
  INVIS_WALL      = 71,
  RICOCHET_PANEL  = 72,
  RICOCHET        = 73,
  MINE            = 74,
  SPIKE           = 75,
  CUSTOM_HURT     = 76,
  TEXT_ID         = 77,
  SHOOTING_FIRE   = 78,
  SEEKER          = 79,
  SNAKE           = 80,
  EYE             = 81,
  THIEF           = 82,
  SLIMEBLOB       = 83,
  RUNNER          = 84,
  GHOST           = 85,
  DRAGON          = 86,
  FISH            = 87,
  SHARK           = 88,
  SPIDER          = 89,
  GOBLIN          = 90,
  SPITTING_TIGER  = 91,
  BULLET_GUN      = 92,
  SPINNING_GUN    = 93,
  BEAR            = 94,
  BEAR_CUB        = 95,
  MISSILE_GUN     = 97,
  SPRITE          = 98,
  SPR_COLLISION   = 99,
  IMAGE_FILE      = 100,
  SENSOR          = 122,
  ROBOT_PUSHABLE  = 123,
  ROBOT           = 124,
  SIGN            = 125,
  SCROLL          = 126,
  PLAYER          = 127,
  NO_ID           = 255
};

static inline boolean is_fake(enum thing id)
{
  return (id == SPACE) || ((id >= FAKE) && (id <= THICK_WEB));
}

static inline boolean is_robot(enum thing id)
{
  return (id == ROBOT) || (id == ROBOT_PUSHABLE);
}

static inline boolean is_signscroll(enum thing id)
{
  return (id == SIGN) || (id == SCROLL);
}

static inline boolean is_water(enum thing id)
{
  return (id >= STILL_WATER) && (id <= W_WATER);
}

static inline boolean is_whirlpool(enum thing id)
{
  return (id >= WHIRLPOOL_1) && (id <= WHIRLPOOL_4);
}

static inline boolean is_enemy(enum thing id)
{
  return (id >= SNAKE) && (id <= BEAR_CUB) &&
   (id != BULLET_GUN) && (id != SPINNING_GUN);
}

static inline boolean is_storageless(enum thing id)
{
  return id < SENSOR;
}

enum dir
{
  IDLE    = 0,
  NORTH   = 1,
  SOUTH   = 2,
  EAST    = 3,
  WEST    = 4,
  RANDNS  = 5,
  RANDEW  = 6,
  RANDNE  = 7,
  RANDNB  = 8,
  SEEK    = 9,
  RANDANY = 10,
  BENEATH = 11,
  ANYDIR  = 12,
  FLOW    = 13,
  NODIR   = 14,
  RANDB   = 15,
  RANDP   = 16,
  CW      = 32,
  OPP     = 64,
  RANDNOT = 128
};

static inline boolean is_cardinal_dir(enum dir d)
{
  return (d >= NORTH) && (d <= WEST);
}

static inline int dir_to_int(enum dir d)
{
  return (int)d - 1;
}

static inline enum dir int_to_dir(int d)
{
  return (enum dir)(d + 1);
}

#define CAN_PUSH      (1 << 0)
#define CAN_TRANSPORT (1 << 1)
#define CAN_LAVAWALK  (1 << 2)
#define CAN_FIREWALK  (1 << 3)
#define CAN_WATERWALK (1 << 4)
#define MUST_WEB      (1 << 5)
#define MUST_THICKWEB (1 << 6)
#define REACT_PLAYER  (1 << 7)
#define MUST_WATER    (1 << 8)
#define MUST_LAVAGOOP (1 << 9)
#define CAN_GOOPWALK  (1 << 10)
#define SPITFIRE      (1 << 11)

enum move_status
{
  NO_HIT      = 0,
  HIT         = 1,
  HIT_PLAYER  = 2,
  HIT_EDGE    = 3
};

enum equality
{
  EQUAL                 = 0,
  LESS_THAN             = 1,
  GREATER_THAN          = 2,
  GREATER_THAN_OR_EQUAL = 3,
  LESS_THAN_OR_EQUAL    = 4,
  NOT_EQUAL             = 5,
  EXACTLY_EQUAL         = 6,
  WILD_EQUAL            = 7,
  WILD_EXACTLY_EQUAL    = 8
};

enum condition
{
  WALKING               = 0,
  SWIMMING              = 1,
  FIRE_WALKING          = 2,
  TOUCHING              = 3,
  BLOCKED               = 4,
  ALIGNED               = 5,
  ALIGNED_NS            = 6,
  ALIGNED_EW            = 7,
  LASTSHOT              = 8,
  LASTTOUCH             = 9,
  RIGHTPRESSED          = 10,
  LEFTPRESSED           = 11,
  UPPRESSED             = 12,
  DOWNPRESSED           = 13,
  SPACEPRESSED          = 14,
  DELPRESSED            = 15,
  MUSICON               = 16,
  SOUNDON               = 17
};

enum chest_contents
{
  CHEST_EMPTY            = 0,
  CHEST_KEY              = 1,
  CHEST_COINS            = 2,
  CHEST_LIVES            = 3,
  CHEST_AMMO             = 4,
  CHEST_GEMS             = 5,
  CHEST_HEALTH           = 6,
  CHEST_POTION           = 7,
  CHEST_RING             = 8,
  CHEST_LOBOMBS          = 9,
  CHEST_HIBOMBS          = 10
};

enum give_item
{
  GIVE_GEMS             = 0,
  GIVE_AMMO             = 1,
  GIVE_TIME             = 2,
  GIVE_SCORE            = 3,
  GIVE_HEALTH           = 4,
  GIVE_LIVES            = 5,
  GIVE_LOBOMBS          = 6,
  GIVE_HIBOMBS          = 7,
  GIVE_COINS            = 8
};

enum potion
{
  POTION_DUD            = 0,
  POTION_INVINCO        = 1,
  POTION_BLAST          = 2,
  POTION_SMALL_HEALTH   = 3,
  POTION_BIG_HEALTH     = 4,
  POTION_POISON         = 5,
  POTION_BLIND          = 6,
  POTION_KILL           = 7,
  POTION_FIREWALK       = 8,
  POTION_DETONATE       = 9,
  POTION_BANISH         = 10,
  POTION_SUMMON         = 11,
  POTION_AVALANCHE      = 12,
  POTION_FREEZE         = 13,
  POTION_WIND           = 14,
  POTION_SLOW           = 15
};

enum board_target
{
  TARGET_NONE           = 0,
  TARGET_POSITION       = 1,
  TARGET_ENTRANCE       = 2,
  TARGET_TELEPORT       = 3
};

enum robot_command_name
{
  ROBOTIC_CMD_END                                       = 0,
  ROBOTIC_CMD_DIE                                       = 1,
  ROBOTIC_CMD_WAIT                                      = 2,
  ROBOTIC_CMD_CYCLE                                     = 3,
  ROBOTIC_CMD_GO                                        = 4,
  ROBOTIC_CMD_WALK                                      = 5,
  ROBOTIC_CMD_BECOME                                    = 6,
  ROBOTIC_CMD_CHAR                                      = 7,
  ROBOTIC_CMD_COLOR                                     = 8,
  ROBOTIC_CMD_GOTOXY                                    = 9,
  ROBOTIC_CMD_SET                                       = 10,
  ROBOTIC_CMD_INC                                       = 11,
  ROBOTIC_CMD_DEC                                       = 12,
  ROBOTIC_CMD_UNUSED_13                                 = 13,
  ROBOTIC_CMD_UNUSED_14                                 = 14,
  ROBOTIC_CMD_UNUSED_15                                 = 15,
  ROBOTIC_CMD_IF                                        = 16,
  ROBOTIC_CMD_UNUSED_17                                 = 17,
  ROBOTIC_CMD_IF_CONDITION                              = 18,
  ROBOTIC_CMD_IF_NOT_CONDITION                          = 19,
  ROBOTIC_CMD_IF_ANY                                    = 20,
  ROBOTIC_CMD_IF_NO                                     = 21,
  ROBOTIC_CMD_IF_THING_DIR                              = 22,
  ROBOTIC_CMD_IF_NOT_THING_DIR                          = 23,
  ROBOTIC_CMD_IF_THING_XY                               = 24,
  ROBOTIC_CMD_IF_AT                                     = 25,
  ROBOTIC_CMD_IF_DIR_OF_PLAYER                          = 26,
  ROBOTIC_CMD_DOUBLE                                    = 27,
  ROBOTIC_CMD_HALF                                      = 28,
  ROBOTIC_CMD_GOTO                                      = 29,
  ROBOTIC_CMD_SEND                                      = 30,
  ROBOTIC_CMD_EXPLODE                                   = 31,
  ROBOTIC_CMD_PUT_DIR                                   = 32,
  ROBOTIC_CMD_GIVE                                      = 33,
  ROBOTIC_CMD_TAKE                                      = 34,
  ROBOTIC_CMD_TAKE_OR                                   = 35,
  ROBOTIC_CMD_ENDGAME                                   = 36,
  ROBOTIC_CMD_ENDLIFE                                   = 37,
  ROBOTIC_CMD_MOD                                       = 38,
  ROBOTIC_CMD_SAM                                       = 39,
  ROBOTIC_CMD_VOLUME                                    = 40,
  ROBOTIC_CMD_END_MOD                                   = 41,
  ROBOTIC_CMD_END_SAM                                   = 42,
  ROBOTIC_CMD_PLAY                                      = 43,
  ROBOTIC_CMD_END_PLAY                                  = 44,
  ROBOTIC_CMD_WAIT_THEN_PLAY                            = 45,
  ROBOTIC_CMD_WAIT_PLAY                                 = 46,
  ROBOTIC_CMD_BLANK_LINE                                = 47,
  ROBOTIC_CMD_SFX                                       = 48,
  ROBOTIC_CMD_PLAY_IF_SILENT                            = 49,
  ROBOTIC_CMD_OPEN                                      = 50,
  ROBOTIC_CMD_LOCKSELF                                  = 51,
  ROBOTIC_CMD_UNLOCKSELF                                = 52,
  ROBOTIC_CMD_SEND_DIR                                  = 53,
  ROBOTIC_CMD_ZAP                                       = 54,
  ROBOTIC_CMD_RESTORE                                   = 55,
  ROBOTIC_CMD_LOCKPLAYER                                = 56,
  ROBOTIC_CMD_UNLOCKPLAYER                              = 57,
  ROBOTIC_CMD_LOCKPLAYER_NS                             = 58,
  ROBOTIC_CMD_LOCKPLAYER_EW                             = 59,
  ROBOTIC_CMD_LOCKPLAYER_ATTACK                         = 60,
  ROBOTIC_CMD_MOVE_PLAYER_DIR                           = 61,
  ROBOTIC_CMD_MOVE_PLAYER_DIR_OR                        = 62,
  ROBOTIC_CMD_PUT_PLAYER_XY                             = 63,
  ROBOTIC_CMD_OBSOLETE_IF_PLAYER_DIR                    = 64,
  ROBOTIC_CMD_OBSOLETE_IF_NOT_PLAYER_DIR                = 65,
  ROBOTIC_CMD_IF_PLAYER_XY                              = 66,
  ROBOTIC_CMD_PUT_PLAYER_DIR                            = 67,
  ROBOTIC_CMD_TRY_DIR                                   = 68,
  ROBOTIC_CMD_ROTATECW                                  = 69,
  ROBOTIC_CMD_ROTATECCW                                 = 70,
  ROBOTIC_CMD_SWITCH                                    = 71,
  ROBOTIC_CMD_SHOOT                                     = 72,
  ROBOTIC_CMD_LAYBOMB                                   = 73,
  ROBOTIC_CMD_LAYBOMB_HIGH                              = 74,
  ROBOTIC_CMD_SHOOTMISSILE                              = 75,
  ROBOTIC_CMD_SHOOTSEEKER                               = 76,
  ROBOTIC_CMD_SPITFIRE                                  = 77,
  ROBOTIC_CMD_LAZERWALL                                 = 78,
  ROBOTIC_CMD_PUT_XY                                    = 79,
  ROBOTIC_CMD_DIE_ITEM                                  = 80,
  ROBOTIC_CMD_SEND_XY                                   = 81,
  ROBOTIC_CMD_COPYROBOT_NAMED                           = 82,
  ROBOTIC_CMD_COPYROBOT_XY                              = 83,
  ROBOTIC_CMD_COPYROBOT_DIR                             = 84,
  ROBOTIC_CMD_DUPLICATE_SELF_DIR                        = 85,
  ROBOTIC_CMD_DUPLICATE_SELF_XY                         = 86,
  ROBOTIC_CMD_BULLETN                                   = 87,
  ROBOTIC_CMD_BULLETS                                   = 88,
  ROBOTIC_CMD_BULLETE                                   = 89,
  ROBOTIC_CMD_BULLETW                                   = 90,
  ROBOTIC_CMD_GIVEKEY                                   = 91,
  ROBOTIC_CMD_GIVEKEY_OR                                = 92,
  ROBOTIC_CMD_TAKEKEY                                   = 93,
  ROBOTIC_CMD_TAKEKEY_OR                                = 94,
  ROBOTIC_CMD_INC_RANDOM                                = 95,
  ROBOTIC_CMD_DEC_RANDOM                                = 96,
  ROBOTIC_CMD_SET_RANDOM                                = 97,
  ROBOTIC_CMD_TRADE                                     = 98,
  ROBOTIC_CMD_SEND_DIR_PLAYER                           = 99,
  ROBOTIC_CMD_PUT_DIR_PLAYER                            = 100,
  ROBOTIC_CMD_SLASH                                     = 101,
  ROBOTIC_CMD_MESSAGE_LINE                              = 102,
  ROBOTIC_CMD_MESSAGE_BOX_LINE                          = 103,
  ROBOTIC_CMD_MESSAGE_BOX_OPTION                        = 104,
  ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION                  = 105,
  ROBOTIC_CMD_LABEL                                     = 106,
  ROBOTIC_CMD_COMMENT                                   = 107,
  ROBOTIC_CMD_ZAPPED_LABEL                              = 108,
  ROBOTIC_CMD_TELEPORT                                  = 109,
  ROBOTIC_CMD_SCROLLVIEW                                = 110,
  ROBOTIC_CMD_INPUT                                     = 111,
  ROBOTIC_CMD_IF_INPUT                                  = 112,
  ROBOTIC_CMD_IF_INPUT_NOT                              = 113,
  ROBOTIC_CMD_IF_INPUT_MATCHES                          = 114,
  ROBOTIC_CMD_PLAYER_CHAR                               = 115,
  ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE                    = 116,
  ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE                   = 117,
  ROBOTIC_CMD_MOVE_ALL                                  = 118,
  ROBOTIC_CMD_COPY                                      = 119,
  ROBOTIC_CMD_SET_EDGE_COLOR                            = 120,
  ROBOTIC_CMD_BOARD                                     = 121,
  ROBOTIC_CMD_BOARD_IS_NONE                             = 122,
  ROBOTIC_CMD_CHAR_EDIT                                 = 123,
  ROBOTIC_CMD_BECOME_PUSHABLE                           = 124,
  ROBOTIC_CMD_BECOME_NONPUSHABLE                        = 125,
  ROBOTIC_CMD_BLIND                                     = 126,
  ROBOTIC_CMD_FIREWALKER                                = 127,
  ROBOTIC_CMD_FREEZETIME                                = 128,
  ROBOTIC_CMD_SLOWTIME                                  = 129,
  ROBOTIC_CMD_WIND                                      = 130,
  ROBOTIC_CMD_AVALANCHE                                 = 131,
  ROBOTIC_CMD_COPY_DIR                                  = 132,
  ROBOTIC_CMD_BECOME_LAVAWALKER                         = 133,
  ROBOTIC_CMD_BECOME_NONLAVAWALKER                      = 134,
  ROBOTIC_CMD_CHANGE                                    = 135,
  ROBOTIC_CMD_PLAYERCOLOR                               = 136,
  ROBOTIC_CMD_BULLETCOLOR                               = 137,
  ROBOTIC_CMD_MISSILECOLOR                              = 138,
  ROBOTIC_CMD_MESSAGE_ROW                               = 139,
  ROBOTIC_CMD_REL_SELF                                  = 140,
  ROBOTIC_CMD_REL_PLAYER                                = 141,
  ROBOTIC_CMD_REL_COUNTERS                              = 142,
  ROBOTIC_CMD_SET_ID_CHAR                               = 143,
  ROBOTIC_CMD_JUMP_MOD_ORDER                            = 144,
  ROBOTIC_CMD_ASK                                       = 145,
  ROBOTIC_CMD_FILLHEALTH                                = 146,
  ROBOTIC_CMD_THICK_ARROW                               = 147,
  ROBOTIC_CMD_THIN_ARROW                                = 148,
  ROBOTIC_CMD_SET_MAX_HEALTH                            = 149,
  ROBOTIC_CMD_SAVE_PLAYER_POSITION                      = 150,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION                   = 151,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION                  = 152,
  ROBOTIC_CMD_MESSAGE_COLUMN                            = 153,
  ROBOTIC_CMD_CENTER_MESSAGE                            = 154,
  ROBOTIC_CMD_CLEAR_MESSAGE                             = 155,
  ROBOTIC_CMD_RESETVIEW                                 = 156,
  ROBOTIC_CMD_MOD_SAM                                   = 157,
  ROBOTIC_CMD_VOLUME2                                   = 158,
  ROBOTIC_CMD_SCROLLBASE                                = 159,
  ROBOTIC_CMD_SCROLLCORNER                              = 160,
  ROBOTIC_CMD_SCROLLTITLE                               = 161,
  ROBOTIC_CMD_SCROLLPOINTER                             = 162,
  ROBOTIC_CMD_SCROLLARROW                               = 163,
  ROBOTIC_CMD_VIEWPORT                                  = 164,
  ROBOTIC_CMD_VIEWPORT_WIDTH                            = 165,
  ROBOTIC_CMD_UNUSED_166                                = 166,
  ROBOTIC_CMD_UNUSED_167                                = 167,
  ROBOTIC_CMD_SAVE_PLAYER_POSITION_N                    = 168,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N                 = 169,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N                = 170,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N_DUPLICATE_SELF  = 171,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N_DUPLICATE_SELF = 172,
  ROBOTIC_CMD_PLAYER_BULLETN                            = 173,
  ROBOTIC_CMD_PLAYER_BULLETS                            = 174,
  ROBOTIC_CMD_PLAYER_BULLETE                            = 175,
  ROBOTIC_CMD_PLAYER_BULLETW                            = 176,
  ROBOTIC_CMD_NEUTRAL_BULLETN                           = 177,
  ROBOTIC_CMD_NEUTRAL_BULLETS                           = 178,
  ROBOTIC_CMD_NEUTRAL_BULLETE                           = 179,
  ROBOTIC_CMD_NEUTRAL_BULLETW                           = 180,
  ROBOTIC_CMD_ENEMY_BULLETN                             = 181,
  ROBOTIC_CMD_ENEMY_BULLETS                             = 182,
  ROBOTIC_CMD_ENEMY_BULLETE                             = 183,
  ROBOTIC_CMD_ENEMY_BULLETW                             = 184,
  ROBOTIC_CMD_PLAYER_BULLET_COLOR                       = 185,
  ROBOTIC_CMD_NEUTRAL_BULLET_COLOR                      = 186,
  ROBOTIC_CMD_ENEMY_BULLET_COLOR                        = 187,
  ROBOTIC_CMD_UNUSED_188                                = 188,
  ROBOTIC_CMD_UNUSED_189                                = 189,
  ROBOTIC_CMD_UNUSED_190                                = 190,
  ROBOTIC_CMD_UNUSED_191                                = 191,
  ROBOTIC_CMD_UNUSED_192                                = 192,
  ROBOTIC_CMD_REL_SELF_FIRST                            = 193,
  ROBOTIC_CMD_REL_SELF_LAST                             = 194,
  ROBOTIC_CMD_REL_PLAYER_FIRST                          = 195,
  ROBOTIC_CMD_REL_PLAYER_LAST                           = 196,
  ROBOTIC_CMD_REL_COUNTERS_FIRST                        = 197,
  ROBOTIC_CMD_REL_COUNTERS_LAST                         = 198,
  ROBOTIC_CMD_MOD_FADE_OUT                              = 199,
  ROBOTIC_CMD_MOD_FADE_IN                               = 200,
  ROBOTIC_CMD_COPY_BLOCK                                = 201,
  ROBOTIC_CMD_CLIP_INPUT                                = 202,
  ROBOTIC_CMD_PUSH                                      = 203,
  ROBOTIC_CMD_SCROLL_CHAR                               = 204,
  ROBOTIC_CMD_FLIP_CHAR                                 = 205,
  ROBOTIC_CMD_COPY_CHAR                                 = 206,
  ROBOTIC_CMD_UNUSED_207                                = 207,
  ROBOTIC_CMD_UNUSED_208                                = 208,
  ROBOTIC_CMD_UNUSED_209                                = 209,
  ROBOTIC_CMD_CHANGE_SFX                                = 210,
  ROBOTIC_CMD_COLOR_INTENSITY_ALL                       = 211,
  ROBOTIC_CMD_COLOR_INTENSITY_N                         = 212,
  ROBOTIC_CMD_COLOR_FADE_OUT                            = 213,
  ROBOTIC_CMD_COLOR_FADE_IN                             = 214,
  ROBOTIC_CMD_SET_COLOR                                 = 215,
  ROBOTIC_CMD_LOAD_CHAR_SET                             = 216,
  ROBOTIC_CMD_MULTIPLY                                  = 217,
  ROBOTIC_CMD_DIVIDE                                    = 218,
  ROBOTIC_CMD_MODULO                                    = 219,
  ROBOTIC_CMD_PLAYER_CHAR_DIR                           = 220,
  ROBOTIC_CMD_UNUSED_221                                = 221,
  ROBOTIC_CMD_LOAD_PALETTE                              = 222,
  ROBOTIC_CMD_UNUSED_223                                = 223,
  ROBOTIC_CMD_MOD_FADE_TO                               = 224,
  ROBOTIC_CMD_SCROLLVIEW_XY                             = 225,
  ROBOTIC_CMD_SWAP_WORLD                                = 226,
  ROBOTIC_CMD_IF_ALIGNEDROBOT                           = 227,
  ROBOTIC_CMD_UNUSED_228                                = 228,
  ROBOTIC_CMD_LOCKSCROLL                                = 229,
  ROBOTIC_CMD_UNLOCKSCROLL                              = 230,
  ROBOTIC_CMD_IF_FIRST_INPUT                            = 231,
  ROBOTIC_CMD_PERSISTENT_GO                             = 232,
  ROBOTIC_CMD_WAIT_MOD_FADE                             = 233,
  ROBOTIC_CMD_UNUSED_234                                = 234,
  ROBOTIC_CMD_ENABLE_SAVING                             = 235,
  ROBOTIC_CMD_DISABLE_SAVING                            = 236,
  ROBOTIC_CMD_ENABLE_SENSORONLY_SAVING                  = 237,
  ROBOTIC_CMD_STATUS_COUNTER                            = 238,
  ROBOTIC_CMD_OVERLAY_ON                                = 239,
  ROBOTIC_CMD_OVERLAY_STATIC                            = 240,
  ROBOTIC_CMD_OVERLAY_TRANSPARENT                       = 241,
  ROBOTIC_CMD_OVERLAY_PUT_OVERLAY                       = 242,
  ROBOTIC_CMD_COPY_OVERLAY_BLOCK                        = 243,
  ROBOTIC_CMD_UNUSED_244                                = 244,
  ROBOTIC_CMD_CHANGE_OVERLAY                            = 245,
  ROBOTIC_CMD_CHANGE_OVERLAY_COLOR                      = 246,
  ROBOTIC_CMD_WRITE_OVERLAY                             = 247,
  ROBOTIC_CMD_UNUSED_248                                = 248,
  ROBOTIC_CMD_UNUSED_249                                = 249,
  ROBOTIC_CMD_UNUSED_250                                = 250,
  ROBOTIC_CMD_LOOP_START                                = 251,
  ROBOTIC_CMD_LOOP_FOR                                  = 252,
  ROBOTIC_CMD_ABORT_LOOP                                = 253,
  ROBOTIC_CMD_DISABLE_MESG_EDGE                         = 254,
  ROBOTIC_CMD_ENABLE_MESG_EDGE                          = 255
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_DATA_H */
