/* MegaZeux
 *
 * Copyright (C) 2017-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_WORLD_FORMAT_H
#define MEGAZEUX_WORLD_FORMAT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "io/memfile.h"
#include "io/zip.h"
#include "util.h" // CLAMP

// Data and functions for the ZIP-based world/board/robot format.


/*********/
/* Files */
/*********/

enum world_file_id
{
  FILE_ID_NONE                    = 0x0000,
  FILE_ID_WORLD_INFO              = 0x0001, // properties file
  FILE_ID_WORLD_GLOBAL_ROBOT      = 0x0004, // properties file
  FILE_ID_WORLD_SFX               = 0x0007, // properties file or data, 50 * 69
  FILE_ID_WORLD_CHARS             = 0x0008, // data, 3584*15
  FILE_ID_WORLD_PAL               = 0x0009, // data, PAL_SIZE * 3
  FILE_ID_WORLD_PAL_SMZX          = 0x000A, // data, SMZX_PAL_SIZE * 3
  FILE_ID_WORLD_PAL_INDEX         = 0x000B, // data, 1024
  FILE_ID_WORLD_VCO               = 0x000C, // data
  FILE_ID_WORLD_VCH               = 0x000D, // data
  FILE_ID_WORLD_PAL_INTENSITY     = 0x000E, // data, PAL_SIZE * 4
  FILE_ID_WORLD_PAL_INTENSITY_SMZX= 0x000F, // data, SMZX_PAL_SIZE * 4

  FILE_ID_WORLD_SPRITES           = 0x0080, // properties file
  FILE_ID_WORLD_COUNTERS          = 0x0081, // counter format, use stream
  FILE_ID_WORLD_STRINGS           = 0x0082, // string format, use stream

  FILE_ID_BOARD_INFO              = 0x0100, // properties file (board_id)
  FILE_ID_BOARD_BID               = 0x0101, // data
  FILE_ID_BOARD_BPR               = 0x0102, // data
  FILE_ID_BOARD_BCO               = 0x0103, // data
  FILE_ID_BOARD_UID               = 0x0104, // data
  FILE_ID_BOARD_UPR               = 0x0105, // data
  FILE_ID_BOARD_UCO               = 0x0106, // data
  FILE_ID_BOARD_OCH               = 0x0107, // data
  FILE_ID_BOARD_OCO               = 0x0108, // data

  FILE_ID_ROBOT                   = 0x1000, // prop. file (board_id + robot_id)
  FILE_ID_SCROLL                  = 0x2000, // prop. file (board_id + robot_id)
  FILE_ID_SENSOR                  = 0x3000  // prop. file (board_id + robot_id)
};


/*******************/
/* Properties Data */
/*******************/

#define PROP_HEADER_SIZE  6
#define PROP_EOF_SIZE     2

#define COUNT_STATCTR_PROPS (NUM_STATUS_COUNTERS * (2))
#define BOUND_STATCTR_PROPS (NUM_STATUS_COUNTERS * (1 + COUNTER_NAME_SIZE))

#define STATCTR_PROP_SIZE                   \
(                                           \
  BOUND_STATCTR_PROPS +                     \
  COUNT_STATCTR_PROPS * PROP_HEADER_SIZE +  \
  PROP_EOF_SIZE                             \
)

enum status_counters_prop
{
  STATCTRPROP_SET_ID              = 0x0001, //  1
  STATCTRPROP_NAME                = 0x0002, //  COUNTER_NAME_SIZE
};

// IF YOU ADD ANYTHING, MAKE SURE THIS GETS UPDATED!

#define COUNT_WORLD_PROPS (              1 + 3 +   4 + 16 + 4 +                 1)
#define BOUND_WORLD_PROPS (BOARD_NAME_SIZE + 5 + 455 + 24 + 9 + STATCTR_PROP_SIZE)

#define COUNT_SAVE_PROPS  ( 2 +  33 +          3 +        1)
#define BOUND_SAVE_PROPS  ( 2 + 121 + 3*MAX_PATH + NUM_KEYS)

// For world files, use WORLD_PROP_SIZE
// For save files, use WORLD_PROP_SIZE + SAVE_PROP_SIZE

#define WORLD_PROP_SIZE                   \
(                                         \
  BOUND_WORLD_PROPS +                     \
  COUNT_WORLD_PROPS * PROP_HEADER_SIZE +  \
  PROP_EOF_SIZE                           \
)

#define SAVE_PROP_SIZE                    \
(                                         \
  BOUND_SAVE_PROPS +                      \
  COUNT_SAVE_PROPS * PROP_HEADER_SIZE     \
)

enum world_prop
{
  WPROP_EOF                       = 0x0000, // Size

  // Header redundant properties      4 (6)     5 + BOARD_NAME_SIZE (+2)
  WPROP_WORLD_NAME                = 0x0001, //  BOARD_NAME_SIZE
  WPROP_WORLD_VERSION             = 0x0002, //   2
  WPROP_FILE_VERSION              = 0x0003, //   2
  WPROP_SAVE_START_BOARD          = 0x0004, //  (1)
  WPROP_SAVE_TEMPORARY_BOARD      = 0x0005, //  (1)
  WPROP_NUM_BOARDS                = 0x0008, //   1

  // ID Chars                            4     455
  WPROP_ID_CHARS                  = 0x0010, // 323
  WPROP_ID_MISSILE_COLOR          = 0x0011, //   1
  WPROP_ID_BULLET_COLOR           = 0x0012, //   3
  WPROP_ID_DMG                    = 0x0013, // 128

  // Status counters
  WPROP_STATUS_COUNTERS           = 0x0018,

  // Global properties                  16      24
  WPROP_EDGE_COLOR                = 0x0020, //   1
  WPROP_FIRST_BOARD               = 0x0021, //   1
  WPROP_ENDGAME_BOARD             = 0x0022, //   1
  WPROP_DEATH_BOARD               = 0x0023, //   1
  WPROP_ENDGAME_X                 = 0x0024, //   2
  WPROP_ENDGAME_Y                 = 0x0025, //   2
  WPROP_GAME_OVER_SFX             = 0x0026, //   1
  WPROP_DEATH_X                   = 0x0027, //   2
  WPROP_DEATH_Y                   = 0x0028, //   2
  WPROP_STARTING_LIVES            = 0x0029, //   2
  WPROP_LIVES_LIMIT               = 0x002A, //   2
  WPROP_STARTING_HEALTH           = 0x002B, //   2
  WPROP_HEALTH_LIMIT              = 0x002C, //   2
  WPROP_ENEMY_HURT_ENEMY          = 0x002D, //   1
  WPROP_CLEAR_ON_EXIT             = 0x002E, //   1
  WPROP_ONLY_FROM_SWAP            = 0x002F, //   1

  // Global properties II                4       9
  WPROP_SMZX_MODE                 = 0x8030, //   1
  WPROP_VLAYER_WIDTH              = 0x8031, //   2
  WPROP_VLAYER_HEIGHT             = 0x8032, //   2
  WPROP_VLAYER_SIZE               = 0x8033, //   4

  // Save properties                  32+4     105 + 3 MAX_PATH + NUM_KEYS
  WPROP_REAL_MOD_PLAYING          = 0x8040, // MAX_PATH
  WPROP_MZX_SPEED                 = 0x8041, //   1
  WPROP_LOCK_SPEED                = 0x8042, //   1
  WPROP_COMMANDS                  = 0x8043, //   4
  WPROP_COMMANDS_STOP             = 0x8044, //   4
  WPROP_SAVED_POSITIONS           = 0x8048, //  40 (2+2+1)*8
  WPROP_UNDER_PLAYER              = 0x8049, //   3 (1+1+1)
  WPROP_PLAYER_RESTART_X          = 0x804A, //   2
  WPROP_PLAYER_RESTART_Y          = 0x804B, //   2
  WPROP_SAVED_PL_COLOR            = 0x804C, //   1
  WPROP_KEYS                      = 0x804D, // NUM_KEYS
  WPROP_BLIND_DUR                 = 0x8050, //   4
  WPROP_FIREWALKER_DUR            = 0x8051, //   4
  WPROP_FREEZE_TIME_DUR           = 0x8052, //   4
  WPROP_SLOW_TIME_DUR             = 0x8053, //   4
  WPROP_WIND_DUR                  = 0x8054, //   4
  WPROP_SCROLL_BASE_COLOR         = 0x8058, //   1
  WPROP_SCROLL_CORNER_COLOR       = 0x8059, //   1
  WPROP_SCROLL_POINTER_COLOR      = 0x805A, //   1
  WPROP_SCROLL_TITLE_COLOR        = 0x805B, //   1
  WPROP_SCROLL_ARROW_COLOR        = 0x805C, //   1
  WPROP_MESG_EDGES                = 0x8060, //   1
  WPROP_BI_SHOOT_STATUS           = 0x8061, //   1
  WPROP_BI_MESG_STATUS            = 0x8062, //   1
  WPROP_FADED                     = 0x8063, //   1
  WPROP_INPUT_FILE_NAME           = 0x8070, // MAX_PATH
  WPROP_INPUT_POS                 = 0x8074, //   4
  WPROP_FREAD_DELIMITER           = 0x8075, //   4
  WPROP_OUTPUT_FILE_NAME          = 0x8078, // MAX_PATH
  WPROP_OUTPUT_POS                = 0x807C, //   4
  WPROP_FWRITE_DELIMITER          = 0x807D, //   4
  WPROP_OUTPUT_MODE               = 0x807E, //   1
  WPROP_MULTIPLIER                = 0x8080, //   4
  WPROP_DIVIDER                   = 0x8081, //   4
  WPROP_C_DIVISIONS               = 0x8082, //   4
  WPROP_MAX_SAMPLES               = 0x8090, //   4
  WPROP_SMZX_MESSAGE              = 0x8091, //   1
  WPROP_JOY_SIMULATE_KEYS         = 0x8092, //   1
};


#define COUNT_SFX_PROPS           (MAX_NUM_SFX * 3)
#define BOUND_SFX_PROPS           (MAX_NUM_SFX * (1 + 11 + MAX_SFX_SIZE))

#define SFX_PROPS_SIZE                  \
(                                       \
  BOUND_SFX_PROPS +                     \
  COUNT_SFX_PROPS * PROP_HEADER_SIZE +  \
  PROP_EOF_SIZE                         \
)

enum sfx_prop
{
  SFXPROP_EOF                     = 0x0000,
  SFXPROP_SET_ID                  = 0x0001, // 1
  SFXPROP_STRING                  = 0x0002, // MAX_SFX_SIZE
  SFXPROP_LABEL                   = 0x0003, // 11
};


// All sprite fields are saved as dwords (except SET_ID), so bound them as such

#define COUNT_SPRITE_PROPS        16
#define BOUND_SPRITE_PROPS        61
#define COUNT_SPRITE_ONCE_PROPS   5
#define BOUND_SPRITE_ONCE_PROPS   16 // + (collision_count * 4)

#define SPRITE_PROPS_SIZE                       \
(                                               \
  (                                             \
    BOUND_SPRITE_PROPS +                        \
    COUNT_SPRITE_PROPS * PROP_HEADER_SIZE       \
  ) * MAX_SPRITES +                             \
  (                                             \
    BOUND_SPRITE_ONCE_PROPS +                   \
    COUNT_SPRITE_ONCE_PROPS * PROP_HEADER_SIZE  \
  ) +                                           \
  PROP_EOF_SIZE                                 \
)

enum sprite_prop
{
  SPROP_EOF                       = 0x00,

  // For each sprite
  SPROP_SET_ID                    = 0x01, // 1, used to select a sprite #
  SPROP_X                         = 0x02, // 2 (ignore size; we save as dwords)
  SPROP_Y                         = 0x03, // 2
  SPROP_REF_X                     = 0x04, // 2
  SPROP_REF_Y                     = 0x05, // 2
  SPROP_COLOR                     = 0x06, // 1
  SPROP_FLAGS                     = 0x07, // 1
  SPROP_WIDTH                     = 0x08, // 1
  SPROP_HEIGHT                    = 0x09, // 1
  SPROP_COL_X                     = 0x0A, // 1
  SPROP_COL_Y                     = 0x0B, // 1
  SPROP_COL_WIDTH                 = 0x0C, // 1
  SPROP_COL_HEIGHT                = 0x0D, // 1
  SPROP_TRANSPARENT_COLOR         = 0x0E, // 4
  SPROP_CHARSET_OFFSET            = 0x0F, // 4
  SPROP_Z                         = 0x10, // 4

  // Only once
  SPROP_ACTIVE_SPRITES            = 0x8000, // 1
  SPROP_SPRITE_Y_ORDER            = 0x8001, // 1
  SPROP_COLLISION_COUNT           = 0x8002, // 2
  SPROP_COLLISION_LIST            = 0x8003, // collision count * 4
  SPROP_SPRITE_NUM                = 0x8004, // 4
};


#define COUNT_BOARD_PROPS (              1 +  7 +          3 + 27)
#define BOUND_BOARD_PROPS (BOARD_NAME_SIZE + 10 + 3*MAX_PATH + 28)

#define COUNT_BOARD_SAVE_PROPS (             2 + 20)
#define BOUND_BOARD_SAVE_PROPS (2*ROBOT_MAX_TR + 30)

// For world files, use BOARD_PROPS_SIZE
// For save files, use BOARD_PROPS_SIZE + BOARD_SAVE_PROPS_SIZE

#define BOARD_PROPS_SIZE                      \
(                                             \
  BOUND_BOARD_PROPS +                         \
  COUNT_BOARD_PROPS * PROP_HEADER_SIZE +      \
  PROP_EOF_SIZE                               \
)

#define BOARD_SAVE_PROPS_SIZE                 \
(                                             \
  BOUND_BOARD_SAVE_PROPS +                    \
  COUNT_BOARD_SAVE_PROPS * PROP_HEADER_SIZE   \
)

enum board_prop
{
  BPROP_EOF                       = 0x0000,

  // Essential                           8     10 + BOARD_NAME_SIZE
  BPROP_BOARD_NAME                = 0x0001, // BOARD_NAME_SIZE
  BPROP_BOARD_WIDTH               = 0x0002, // 2
  BPROP_BOARD_HEIGHT              = 0x0003, // 2
  BPROP_OVERLAY_MODE              = 0x0004, // 1
  BPROP_NUM_ROBOTS                = 0x0005, // 1
  BPROP_NUM_SCROLLS               = 0x0006, // 1
  BPROP_NUM_SENSORS               = 0x0007, // 1
  BPROP_FILE_VERSION              = 0x0008, // 2

  // Non-essential                      27     28 + 3 MAX_PATH
  BPROP_MOD_PLAYING               = 0x0010, // MAX_PATH
  BPROP_VIEWPORT_X                = 0x0011, // 1
  BPROP_VIEWPORT_Y                = 0x0012, // 1
  BPROP_VIEWPORT_WIDTH            = 0x0013, // 1
  BPROP_VIEWPORT_HEIGHT           = 0x0014, // 1
  BPROP_CAN_SHOOT                 = 0x0015, // 1
  BPROP_CAN_BOMB                  = 0x0016, // 1
  BPROP_FIRE_BURN_BROWN           = 0x0017, // 1
  BPROP_FIRE_BURN_SPACE           = 0x0018, // 1
  BPROP_FIRE_BURN_FAKES           = 0x0019, // 1
  BPROP_FIRE_BURN_TREES           = 0x001A, // 1
  BPROP_EXPLOSIONS_LEAVE          = 0x001B, // 1
  BPROP_SAVE_MODE                 = 0x001C, // 1
  BPROP_FOREST_BECOMES            = 0x001D, // 1
  BPROP_COLLECT_BOMBS             = 0x001E, // 1
  BPROP_FIRE_BURNS                = 0x001F, // 1
  BPROP_BOARD_N                   = 0x0020, // 1
  BPROP_BOARD_S                   = 0x0021, // 1
  BPROP_BOARD_E                   = 0x0022, // 1
  BPROP_BOARD_W                   = 0x0023, // 1
  BPROP_RESTART_IF_ZAPPED         = 0x0024, // 1
  BPROP_TIME_LIMIT                = 0x0025, // 2
  BPROP_PLAYER_NS_LOCKED          = 0x0026, // 1
  BPROP_PLAYER_EW_LOCKED          = 0x0027, // 1
  BPROP_PLAYER_ATTACK_LOCKED      = 0x0028, // 1
  BPROP_RESET_ON_ENTRY            = 0x0029, // 1
  BPROP_CHARSET_PATH              = 0x002A, // MAX_PATH
  BPROP_PALETTE_PATH              = 0x002B, // MAX_PATH
  BPROP_RESET_ON_ENTRY_SAME_BOARD = 0x002C, // 1
  BPROP_DRAGONS_CAN_RANDOMLY_MOVE = 0x002D, // 1

  // Save                               17     25 + 2 ROBOT_MAX_TR
  BPROP_SCROLL_X                  = 0x0100, // 2
  BPROP_SCROLL_Y                  = 0x0101, // 2
  BPROP_LOCKED_X                  = 0x0102, // 2
  BPROP_LOCKED_Y                  = 0x0103, // 2
  BPROP_PLAYER_LAST_DIR           = 0x0104, // 1
  BPROP_LAZWALL_START             = 0x010A, // 1
  BPROP_LAST_KEY                  = 0x010B, // 1
  BPROP_NUM_INPUT                 = 0x010C, // 4
  BPROP_INPUT_SIZE                = 0x010D, // 4 (IS SEPARATE FROM INPUT_STRING)
  BPROP_INPUT_STRING              = 0x010E, // ROBOT_MAX_TR
  BRPOP_BOTTOM_MESG               = 0x0110, // ROBOT_MAX_TR
  BPROP_BOTTOM_MESG_TIMER         = 0x0111, // 1
  BPROP_BOTTOM_MESG_ROW           = 0x0112, // 1
  BPROP_BOTTOM_MESG_COL           = 0x0113, // 1
  BPROP_VOLUME                    = 0x0114, // 1
  BPROP_VOLUME_INC                = 0x0115, // 1
  BPROP_VOLUME_TARGET             = 0x0116, // 1
  BPROP_BLIND_DUR                 = 0x0117, // 1
  BPROP_FIREWALKER_DUR            = 0x0118, // 1
  BPROP_FREEZE_TIME_DUR           = 0x0119, // 1
  BPROP_SLOW_TIME_DUR             = 0x011a, // 1
  BPROP_WIND_DUR                  = 0x011b, // 1
};


#define COUNT_ROBOT_PROPS (1 + 3 + 1)
#define BOUND_ROBOT_PROPS (0 + 5 + 0) // +name, +prog OR source

#define COUNT_ROBOT_SAVE_PROPS (11 + 2 +    1 + 1 + 1)
#define BOUND_ROBOT_SAVE_PROPS (17 + 8 + 4*32 + 0 + 1) // +stack

// For world files, use ROBOT_PROPS_SIZE
// For save files, use ROBOT_PROPS_SIZE + ROBOT_SAVE_PROPS_SIZE

#define ROBOT_PROPS_SIZE                          \
(                                                 \
  BOUND_ROBOT_PROPS +                             \
  COUNT_ROBOT_PROPS * PROP_HEADER_SIZE +          \
  PROP_EOF_SIZE                                   \
)

#define ROBOT_SAVE_PROPS_SIZE                     \
(                                                 \
  COUNT_ROBOT_SAVE_PROPS * PROP_HEADER_SIZE +     \
  BOUND_ROBOT_SAVE_PROPS                          \
)

enum robot_prop
{
  RPROP_EOF                       = 0x0000,
  RPROP_ROBOT_NAME                = 0x0001, // ROBOT_NAME_SIZE
  RPROP_ROBOT_CHAR                = 0x0002, // 1
  RPROP_XPOS                      = 0x0003, // 2
  RPROP_YPOS                      = 0x0004, // 2
//RPROP_PROGRAM_ID                = 0x0005, // 4

#ifdef CONFIG_DEBYTECODE
  RPROP_PROGRAM_LABEL_ZAPS        = 0x00FD, // variable
  RPROP_PROGRAM_SOURCE            = 0x00FE, // variable
#endif
  RPROP_PROGRAM_BYTECODE          = 0x00FF, // variable

  // Legacy world, now save
  RPROP_CUR_PROG_LINE             = 0x0100, // 4
  RPROP_POS_WITHIN_LINE           = 0x0101, // 4
  RPROP_ROBOT_CYCLE               = 0x0102, // 1
  RPROP_CYCLE_COUNT               = 0x0103, // 1
  RPROP_BULLET_TYPE               = 0x0104, // 1
  RPROP_IS_LOCKED                 = 0x0105, // 1
  RPROP_CAN_LAVAWALK              = 0x0106, // 1
  RPROP_WALK_DIR                  = 0x0107, // 1
  RPROP_LAST_TOUCH_DIR            = 0x0108, // 1
  RPROP_LAST_SHOT_DIR             = 0x0109, // 1
  RPROP_STATUS                    = 0x010A, // 1

  // Legacy save
  RPROP_LOOP_COUNT                = 0x010B, // 4
  RPROP_LOCALS                    = 0x010C, // 4*32
  RPROP_STACK_POINTER             = 0x0110, // 4
  RPROP_STACK                     = 0x0111, // variable

  // New
  RPROP_CAN_GOOPWALK              = 0x0120, // 1
};


#define SCROLL_PROPS_SIZE   \
(                           \
  2 +                       \
  2 * PROP_HEADER_SIZE +    \
  PROP_EOF_SIZE             \
)

enum scroll_prop
{
  SCRPROP_EOF                     = 0x00,
  SCRPROP_NUM_LINES               = 0x01, // 2
  SCRPROP_MESG                    = 0x02, // variable
};


#define SENSOR_PROPS_SIZE     \
(                             \
  (2 * ROBOT_NAME_SIZE) + 1 + \
  3 * PROP_HEADER_SIZE +      \
  PROP_EOF_SIZE               \
)

enum sensor_prop
{
  SENPROP_EOF                     = 0x00,
  SENPROP_SENSOR_NAME             = 0x01, // ROBOT_NAME_SIZE
  SENPROP_SENSOR_CHAR             = 0x02, //  1
  SENPROP_ROBOT_TO_MESG           = 0x03, // ROBOT_NAME_SIZE
};


/**
 * ZIP world format functions.
 */

/**
 * To quickly match world filenames, they are converted to uint64_t. This
 * works because no internal world format files use names longer than 8 chars
 * and (hopefully) they won't ever need to. This seems to be a bit faster than
 * strcmp et al. optimizations even for 32-bit.
 */
#define FILE_ID_8(a,b,c,d,e,f,g,h) \
 (((uint64_t)a <<  0) | \
  ((uint64_t)b <<  8) | \
  ((uint64_t)c << 16) | \
  ((uint64_t)d << 24) | \
  ((uint64_t)e << 32) | \
  ((uint64_t)f << 40) | \
  ((uint64_t)g << 48) | \
  ((uint64_t)h << 56))

#define FILE_ID_7(a,b,c,d,e,f,g) FILE_ID_8(a,b,c,d,e,f,g,'\0')
#define FILE_ID_6(a,b,c,d,e,f) FILE_ID_7(a,b,c,d,e,f,'\0')
#define FILE_ID_5(a,b,c,d,e) FILE_ID_6(a,b,c,d,e,'\0')
#define FILE_ID_4(a,b,c,d) FILE_ID_5(a,b,c,d,'\0')
#define FILE_ID_3(a,b,c) FILE_ID_4(a,b,c,'\0')
#define FILE_ID_2(a,b) FILE_ID_3(a,b,'\0')
#define FILE_ID_1(a) FILE_ID_2(a,'\0')

/**
 * Since only ASCII values are ever valid IDs, double bit 6 to bit 5 as a
 * cheap and tacky tolower replacement.
 */
#define FILE_ID_TOLOWER(id) ((id) | (((id) & (uint64_t)0x4040404040404040) >> 1))

static inline uint64_t world_file_id_value(const char *filename, size_t len)
{
  const char *f = filename;
  switch(len)
  {
    case 1: return FILE_ID_TOLOWER(FILE_ID_1(f[0]));
    case 2: return FILE_ID_TOLOWER(FILE_ID_2(f[0],f[1]));
    case 3: return FILE_ID_TOLOWER(FILE_ID_3(f[0],f[1],f[2]));
    case 4: return FILE_ID_TOLOWER(FILE_ID_4(f[0],f[1],f[2],f[3]));
    case 5: return FILE_ID_TOLOWER(FILE_ID_5(f[0],f[1],f[2],f[3],f[4]));
    case 6: return FILE_ID_TOLOWER(FILE_ID_6(f[0],f[1],f[2],f[3],f[4],f[5]));
    case 7: return FILE_ID_TOLOWER(FILE_ID_7(f[0],f[1],f[2],f[3],f[4],f[5],f[6]));
    // Currently no valid world format filenames with >=8 chars.
  }
  return 0;
}

static inline int world_file_id_cmp(const void *a, const void *b)
{
  struct zip_file_header *A = *(struct zip_file_header **)a;
  struct zip_file_header *B = *(struct zip_file_header **)b;
  int ab = A->mzx_board_id;
  int bb = B->mzx_board_id;
  int af = A->mzx_file_id;
  int bf = B->mzx_file_id;
  int ar = A->mzx_robot_id;
  int br = B->mzx_robot_id;
  uint32_t ao = A->offset;
  uint32_t bo = B->offset;

  return  (ab - bb) ? (ab - bb) :
          (af - bf) ? (af - bf) :
          (ar - br) ? (ar - br) :
          (ao > bo) ? 1 : (ao < bo) ? -1 : 0;
}

static inline boolean compression_method_allowed(uint16_t method)
{
  // Only store and deflate are supported universally in all MZX builds;
  // any other decompressors may be disabled for platforms that don't use them.
  // Thus, any file in an MZX/etc that doesn't use store or deflate is invalid.
  return (method == ZIP_M_NONE) || (method == ZIP_M_DEFLATE);
}

static inline void world_assign_file_ids_parse_board(const char *next, size_t len,
 unsigned int *_file_id, unsigned int *_board_id, unsigned int *_robot_id)
{
  unsigned int robot_id = 0;
  char temp[3] = { 0 };

  if(len < 3 || len > 7)
    return;

  temp[0] = next[1];
  temp[1] = next[2];

  *_board_id = strtoul(temp, NULL, 16);
  next += 3;
  len -= 3;

  if(next[0])
  {
    if(next[0] == 'r' || next[0] == 'R')
    {
      robot_id = strtoul(next + 1, NULL, 16);
      if(robot_id != 0)
      {
        *_file_id = FILE_ID_ROBOT;
      }

      *_robot_id = robot_id;
    }
    else

    if(next[0] == 's' || next[0] == 'S')
    {
      if(next[1] == 'c' || next[1] == 'C')
      {
        robot_id = strtoul(next + 2, NULL, 16);
        if(robot_id != 0)
        {
          *_file_id = FILE_ID_SCROLL;
        }
      }
      else

      if(next[1] == 'e' || next[1] == 'E')
      {
        robot_id = strtoul(next + 2, NULL, 16);
        if(robot_id != 0)
        {
          *_file_id = FILE_ID_SENSOR;
        }
      }

      *_robot_id = robot_id;
    }
    else

    switch(world_file_id_value(next, len))
    {
      case FILE_ID_3('b','i','d'):
        *_file_id = FILE_ID_BOARD_BID;
        break;
      case FILE_ID_3('b','p','r'):
        *_file_id = FILE_ID_BOARD_BPR;
        break;
      case FILE_ID_3('b','c','o'):
        *_file_id = FILE_ID_BOARD_BCO;
        break;
      case FILE_ID_3('u','i','d'):
        *_file_id = FILE_ID_BOARD_UID;
        break;
      case FILE_ID_3('u','p','r'):
        *_file_id = FILE_ID_BOARD_UPR;
        break;
      case FILE_ID_3('u','c','o'):
        *_file_id = FILE_ID_BOARD_UCO;
        break;
      case FILE_ID_3('o','c','h'):
        *_file_id = FILE_ID_BOARD_OCH;
        break;
      case FILE_ID_3('o','c','o'):
        *_file_id = FILE_ID_BOARD_OCO;
        break;
    }
  }
  else
  {
    *_file_id = FILE_ID_BOARD_INFO;
  }
  if(!*_file_id)
    *_board_id = 0;
}

/**
 * Assign world file ID values to files in the world ZIP archive.
 * This needs to be done once before every single world, board, and MZM load.
 *
 * TODO: maybe count number of misordered files and files with no ID, do an
 * insertion sort if it's low (<=log2(65536)=16) or no sort if there are none?
 */
static inline void world_assign_file_ids(struct zip_archive *zp, boolean is_a_world)
{
  // Assign world file IDs to zip headers if they don't already exist.
  struct zip_file_header **fh_list = zp->files;
  struct zip_file_header *fh;
  int num_fh = zp->num_files;

  unsigned int file_id;
  unsigned int board_id;
  unsigned int robot_id;
  const char *next;
  int len;
  int i;

  // Special handling for non-worlds.
  if(!is_a_world)
  {
    board_id = 0;

    for(i = 0; i < num_fh; i++)
    {
      fh = fh_list[i];
      next = fh->file_name;
      len = strlen(next);

      file_id = FILE_ID_NONE;
      robot_id = 0;

      if(len > 8 || !compression_method_allowed(fh->method))
      {
        fh->mzx_file_id = FILE_ID_NONE;
        continue;
      }
      else

      if(next[0] == 'r' || next[0] == 'R')
      {
        // Shorthand for robot on board 0
        // Goes first to speed up MZM loads.
        robot_id = strtoul(next + 1, NULL, 16);
        file_id = FILE_ID_ROBOT;
      }
      else

      if(next[0] == 'b' || next[0] == 'B')
      {
        world_assign_file_ids_parse_board(next, len, &file_id, &board_id, &robot_id);

        // Non-world files shouldn't boards > 0
        if(board_id)
        {
          file_id = FILE_ID_NONE;
          board_id = 0;
          robot_id = 0;
        }
      }

      // Set the properties
      fh->mzx_file_id = file_id;
      fh->mzx_board_id = board_id;
      fh->mzx_robot_id = robot_id;
    }
  }

  // Regular world/save files.
  else
  {
    for(i = 0; i < num_fh; i++)
    {
      fh = fh_list[i];
      next = fh->file_name;
      len = strlen(next);

      file_id = FILE_ID_NONE;
      board_id = 0;
      robot_id = 0;

      if(len > 8 || !compression_method_allowed(fh->method))
      {
        fh->mzx_file_id = FILE_ID_NONE;
        continue;
      }
      else

      if(next[0] == 'b' || next[0] == 'B')
      {
        world_assign_file_ids_parse_board(next, len, &file_id, &board_id, &robot_id);
      }
      else

      switch(world_file_id_value(next, len))
      {
        case FILE_ID_5('w','o','r','l','d'):
          file_id = FILE_ID_WORLD_INFO;
          break;
        case FILE_ID_2('g','r'):
          file_id = FILE_ID_WORLD_GLOBAL_ROBOT;
          break;
        case FILE_ID_3('s','f','x'):
          file_id = FILE_ID_WORLD_SFX;
          break;
        case FILE_ID_5('c','h','a','r','s'):
          file_id = FILE_ID_WORLD_CHARS;
          break;
        case FILE_ID_3('p','a','l'):
          file_id = FILE_ID_WORLD_PAL;
          break;
        case FILE_ID_7('p','a','l','s','m','z','x'):
          file_id = FILE_ID_WORLD_PAL_SMZX;
          break;
        case FILE_ID_6('p','a','l','i','d','x'):
          file_id = FILE_ID_WORLD_PAL_INDEX;
          break;
        case FILE_ID_6('p','a','l','i','n','t'):
          file_id = FILE_ID_WORLD_PAL_INTENSITY;
          break;
        case FILE_ID_7('p','a','l','i','n','t','s'):
          file_id = FILE_ID_WORLD_PAL_INTENSITY_SMZX;
          break;
        case FILE_ID_3('v','c','o'):
          file_id = FILE_ID_WORLD_VCO;
          break;
        case FILE_ID_3('v','c','h'):
          file_id = FILE_ID_WORLD_VCH;
          break;
        case FILE_ID_3('s','p','r'):
          file_id = FILE_ID_WORLD_SPRITES;
          break;
        case FILE_ID_7('c','o','u','n','t','e','r'):
          file_id = FILE_ID_WORLD_COUNTERS;
          break;
        case FILE_ID_6('s','t','r','i','n','g'):
          file_id = FILE_ID_WORLD_STRINGS;
          break;
      }

      // Set the properties
      fh->mzx_file_id = file_id;
      fh->mzx_board_id = board_id;
      fh->mzx_robot_id = robot_id;
    }
  }

  // Sort the archive and reset to the beginning
  qsort(fh_list, num_fh, sizeof(struct zip_file_header *), world_file_id_cmp);
  zp->pos = 0;
}

/**
 * These functions are used to save properties files in world saving. There
 * are no safety checks aside from the debug asserts in the memfile functions.
 * USE THE BOUNDING MACROS WHEN ALLOCATING.
 */
static inline void save_prop_eof(struct memfile *mf)
{
  mfputw(0, mf);
}

// Write a (u)int8 property.
static inline void save_prop_c(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(1, mf);
  mfputc(value, mf);
}

// Write a (u)int16 property.
static inline void save_prop_w(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(2, mf);
  mfputw(value, mf);
}

// Write a (u)int32 property.
static inline void save_prop_d(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(4, mf);
  mfputd(value, mf);
}

// Write a (u)int64 property.
static inline void save_prop_q(int ident, int64_t value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(8, mf);
  mfputq(value, mf);
}

// Write an array property.
// `src` should point to an array of `count` members of `len` length each.
static inline void save_prop_a(int ident, const void *src, size_t len,
 size_t count, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(len * count, mf);
  mfwrite(src, len, count, mf);
}

// Write a null terminated string property.
static inline void save_prop_s(int ident, const char *src, struct memfile *mf)
{
  save_prop_a(ident, src, strlen(src), 1, mf);
}

// Some string properties had questionable termination behavior in older
// versions. This is a convenience macro that saves as a string for 2.93+ and
// as a block array for <2.93 (to help with downver export).
#define save_prop_s_293(ident, src, buffer_len, mf) do { \
  if(file_version >= V293)  save_prop_s(ident, src, mf); \
  else                      save_prop_a(ident, src, buffer_len, 1, mf); \
} while(0)

// Write an arbitrary-sized data property and return a memfile of it.
static inline void save_prop_v(int ident, size_t len, struct memfile *prop,
 struct memfile *mf)
{
  assert(mfhasspace(len, mf));
  mfputw(ident, mf);
  mfputd(len, mf);
  mfopen_wr(mf->current, len, prop);
  mf->current += len;
}

static inline int load_prop_int(struct memfile *prop)
{
  switch(prop->end - prop->start)
  {
    case 1:
      return mfgetc(prop);
    case 2:
      return mfgetw(prop);
    case 4:
      return mfgetd(prop);
    default:
      return 0;
  }
}

// Unsigned clamp.
static inline unsigned load_prop_int_u(struct memfile *prop, unsigned min, unsigned max)
{
  unsigned tmp = load_prop_int(prop);
  return CLAMP(tmp, min, max);
}

// Forces sign extension of byte and word values, signed clamp.
static inline int load_prop_int_s(struct memfile *prop, int min, int max)
{
  int tmp = 0;
  switch(prop->end - prop->start)
  {
    case 1:
      tmp = (signed char)mfgetc(prop);
      break;
    case 2:
      tmp = (signed short)mfgetw(prop);
      break;
    case 4:
      tmp = mfgetd(prop);
      break;
  }
  return CLAMP(tmp, min, max);
}

// Forces sign extension of byte and word values, signed clamp.
static inline int64_t load_prop_int_s64(struct memfile *prop, int64_t min, int64_t max)
{
  int64_t tmp = 0;
  switch(prop->end - prop->start)
  {
    case 1:
      tmp = (signed char)mfgetc(prop);
      break;
    case 2:
      tmp = (signed short)mfgetw(prop);
      break;
    case 4:
      tmp = mfgetd(prop);
      break;
    case 8:
      tmp = mfgetq(prop);
      break;
  }
  return CLAMP(tmp, min, max);
}

// All non-zero values -> 1.
static inline boolean load_prop_boolean(struct memfile *prop)
{
  return !!load_prop_int(prop);
}

// This function is used to read properties files in world loading.
static inline boolean next_prop(struct memfile *prop, int *ident, int *length,
 struct memfile *mf)
{
  unsigned char *end = mf->end;
  unsigned char *cur;
  unsigned int len;

  if((end - mf->current) < PROP_HEADER_SIZE)
  {
    prop->current = NULL;
    *ident = *length = 0;
    return false;
  }

  *ident = mfgetw(mf);
  len = mfgetud(mf);
  cur = mf->current;

  if((size_t)(end - cur) < len)
  {
    prop->current = NULL;
    *ident = *length = 0;
    return false;
  }

  *length = len;
  mfopen(cur, len, prop);

  mf->current += len;
  return true;
}

/**
 * Returns true if the properties file in `mf` is valid.
 * This can be used to distinguish whether or not an ambiguous file or field
 * actually contains properties.
 */
static inline boolean check_properties_file(struct memfile *mf,
 int maximum_ident)
{
  int ident;
  unsigned length;
  while(mf->end - mf->current >= 6)
  {
    ident = mfgetw(mf);
    length = mfgetud(mf);
    if(ident == 0 || ident > maximum_ident)
      goto err;

    if((unsigned)(mf->end - mf->current) < length)
      goto err;

    mf->current += length;
  }

  if((mf->end - mf->current == 2) && mfgetw(mf) == 0x0000) // EOF
  {
    mf->current = mf->start;
    return true;
  }

err:
  mf->current = mf->start;
  return false;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_WORLD_FORMAT_H */
