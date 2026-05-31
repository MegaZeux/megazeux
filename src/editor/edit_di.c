/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2017-2023 Alice Rowan <petrifiedrowan@gmail.com>
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

// Dialogs for the world editor.

#include <string.h>

#include "../board.h"
#include "../const.h"
#include "../core.h"
#include "../counter.h"
#include "../idput.h"
#include "../robot.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"
#include "../world_struct.h"

#include "../event/event.h"

#include "board.h"
#include "configure.h"
#include "edit_di.h"
#include "param.h"
#include "window.h"

// The 8th bit set indicates that it's a color, not a char
static const int char_values[8][24] =
{
  {
    0,
    1,
    2,
    3,
    6,
    8,
    9,
    11,
    13,
    14,
    15,
    16,
    20,
    21,
    22,
    23,
    24,
    27,
    28,
    29,
    30,
    31,
    32,
    33
  },
  {
    34,
    36,
    38,
    39,
    40,
    43,
    44,
    47,
    48,
    50,
    55,
    57,
    58,
    60,
    65,
    67,
    68,
    69,
    70,
    71,
    73,
    80,
    81,
    82
  },
  {
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    94,
    95,
    125,
    126,
    160,
    161,
    162,
    163,
    164,
    165,
    166,
    167,
    168,
    169, // Special
    176 | 512
  },
  {
    177 | 512,
    178 | 512,
    179 | 512,
    180 | 512,
    181 | 512,
    182 | 512,
    183 | 512,
    184 | 512,
    185 | 512,
    186 | 512,
    187 | 512,
    188, // Special
    189, // Special
    198, // Special
    200, // Special
    190,
    191,
    192,
    193,
    194,
    195,
    196,
    197,
    230
  },
  {
    231,
    232,
    233,
    234,
    235,
    236,
    237,
    238,
    239,
    240,
    241,
    242,
    243,
    244,
    245,
    246,
    247,
    248,
    249,
    250,
    251,
    252,
    253,
    254
  },
  {
    255,
    256,
    257,
    258,
    259,
    260,
    261,
    262,
    263,
    264,
    265,
    266,
    267,
    268,
    269,
    270,
    271,
    272 | 512,
    273 | 512,
    274 | 512,
    275 | 512,
    276 | 512,
    277 | 512,
    278
  },
  {
    279,
    280,
    281,
    282 | 512,
    283 | 512,
    284 | 512,
    285 | 512,
    286,
    287,
    288,
    289,
    290,
    291,
    292 | 512,
    293 | 512,
    294,
    295,
    296,
    297,
    298 | 512,
    299 | 512,
    300 | 512,
    301 | 512,
    302 | 512
  },
  {
    303 | 512,
    304 | 512,
    305 | 512,
    306,
    307,
    308,
    309,
    310,
    311,
    312,
    313,
    314,
    315,
    316,
    317,
    318,
    319,
    320,
    321,
    322 | 512,
    323 | 512,
    324 | 512,
    325 | 512,
    326 | 512
  }
};

// Info for char funcs, 192 selections (24 per func, 8 funcs)
static const char *const char_strs[8][24] =
{
  {
    "Space (000)-",
    "Normal (001)-",
    "Solid (002)-",
    "Tree (003)-",
    "Breakaway (006)-",
    "Boulder (008)-",
    "Crate (009)-",
    "Box (011)-",
    "Fake (013)-",
    "Carpet (014)-",
    "Floor (015)-",
    "Tiles (016)-",
    "Still Water (020)-",
    "N Water (021)-",
    "S Water (022)-",
    "E Water (023)-",
    "W Water (024)-",
    "Chest (027)-",
    "Gem (028)-",
    "Magic Gem (029)-",
    "Health (030)-",
    "Ring (031)-",
    "Potion (032)-",
    "Energizer (033)-"
  },
  {
    "Goop (034)-",
    "Bomb (036)-",
    "Explosion (038)-",
    "Key (039)-",
    "Lock (040)-",
    "Stairs (043)-",
    "Cave (044)-",
    "Gate (047)-",
    "Open Gate (048)-",
    "Coin (050)-",
    "Pouch (055)-",
    "Slider NS (057)-",
    "Slider EW (058)-",
    "Lazer Gun (060)-",
    "Forest (065)-",
    "Whirlpool 1 (067)-",
    "Whirlpool 2 (068)-",
    "Whirlpool 3 (069)-",
    "Whirlpool 4 (070)-",
    "Invis. Wall (071)-",
    "Ricochet (073)-",
    "Snake (080)-",
    "Eye (081)-",
    "Thief (082)-"
  },
  {
    "Slime Blob (083)-",
    "Runner (084)-",
    "Ghost (085)-",
    "Dragon (086)-",
    "Fish (087)-",
    "Shark (088)-",
    "Spider (089)-",
    "Goblin (090)-",
    "Spitting Tiger (091)-",
    "Bear (094)-",
    "Bear Cub (095)-",
    "Sign (125)-",
    "Scroll (126)-",
    "Blank Ice (160)-",
    "Ice Anim 1 (161)-",
    "Ice Anim 2 (162)-",
    "Ice Anim 3 (163)-",
    "Lava Anim 1 (164)-",
    "Lava Anim 2 (165)-",
    "Lava Anim 3 (166)-",
    "Small Ammo (167)-",
    "Large Ammo (168)-",
    "Lit Bomb Anim 1 (169)-", // Special
    "Energizer Color 1 (176)-"
  },
  {
    "Energizer Color 2 (177)-",
    "Energizer Color 3 (178)-",
    "Energizer Color 4 (179)-",
    "Energizer Color 5 (180)-",
    "Energizer Color 6 (181)-",
    "Energizer Color 7 (182)-",
    "Energizer Color 8 (183)-",
    "Explosion Stage 1 (184)-",
    "Explosion Stage 2 (185)-",
    "Explosion Stage 3 (186)-",
    "Explosion Stage 4 (187)-",
    "Horizontal Door (\?\?\?)-",//Special
    "Vertical Door (\?\?\?)-",//Special
    "Diagonal Door / (\?\?\?)-",//Special
    "Diagonal Door \\ (\?\?\?)-",//Special
    "CW Anim 1 (190)-",
    "CW Anim 2 (191)-",
    "CW Anim 3 (192)-",
    "CW Anim 4 (193)-",
    "CCW Anim 1 (194)-",
    "CCW Anim 2 (195)-",
    "CCW Anim 3 (196)-",
    "CCW Anim 4 (197)-",
    "N Transport Anim 1 (230)-"
  },
  {
    "N Transport Anim 2 (231)-",
    "N Transport Anim 3 (232)-",
    "N Transport Anim 4 (233)-",
    "S Transport Anim 1 (234)-",
    "S Transport Anim 2 (235)-",
    "S Transport Anim 3 (236)-",
    "S Transport Anim 4 (237)-",
    "E Transport Anim 1 (238)-",
    "E Transport Anim 2 (239)-",
    "E Transport Anim 3 (240)-",
    "E Transport Anim 4 (241)-",
    "W Transport Anim 1 (242)-",
    "W Transport Anim 2 (243)-",
    "W Transport Anim 3 (244)-",
    "W Transport Anim 4 (245)-",
    "A.Transport Anim 1 (246)-",
    "A.Transport Anim 2 (247)-",
    "A.Transport Anim 3 (248)-",
    "A.Transport Anim 4 (249)-",
    "N Thick Arrow (250)-",
    "S Thick Arrow (251)-",
    "E Thick Arrow (252)-",
    "W Thick Arrow (253)-",
    "N Thin Arrow (254)-"
  },
  {
    "S Thin Arrow (255)-",
    "E Thin Arrow (256)-",
    "W Thin Arrow (257)-",
    "Horiz Lazer Anim 1 (258)-",
    "Horiz Lazer Anim 2 (259)-",
    "Horiz Lazer Anim 3 (260)-",
    "Horiz Lazer Anim 4 (261)-",
    "Vert Lazer Anim 1 (262)-",
    "Vert Lazer Anim 2 (263)-",
    "Vert Lazer Anim 3 (264)-",
    "Vert Lazer Anim 4 (265)-",
    "Fire Anim 1 (266)-",
    "Fire Anim 2 (267)-",
    "Fire Anim 3 (268)-",
    "Fire Anim 4 (269)-",
    "Fire Anim 5 (270)-",
    "Fire Anim 6 (271)-",
    "Fire Color 1 (272)-",
    "Fire Color 2 (273)-",
    "Fire Color 3 (274)-",
    "Fire Color 4 (275)-",
    "Fire Color 5 (276)-",
    "Fire Color 6 (277)-",
    "Life Anim 1 (278)-"
  },
  {
    "Life Anim 2 (279)-",
    "Life Anim 3 (280)-",
    "Life Anim 4 (281)-",
    "Life Color 1 (282)-",
    "Life Color 2 (283)-",
    "Life Color 3 (284)-",
    "Life Color 4 (285)-",
    "Ricochet Panel \\ (286)-",
    "Ricochet Panel / (287)-",
    "Mine Anim 1 (288)-",
    "Mine Anim 2 (289)-",
    "Spit Fire Anim 1 (290)-",
    "Spit Fire Anim 2 (291)-",
    "Spit Fire Color 1 (292)-",
    "Spit Fire Color 2 (293)-",
    "Seeker Anim 1 (294)-",
    "Seeker Anim 2 (295)-",
    "Seeker Anim 3 (296)-",
    "Seeker Anim 4 (297)-",
    "Seeker Color 1 (298)-",
    "Seeker Color 2 (299)-",
    "Seeker Color 3 (300)-",
    "Seeker Color 4 (301)-",
    "Whirlpool Color 1 (302)-"
  },
  {
    "Whirlpool Color 2 (303)-",
    "Whirlpool Color 3 (304)-",
    "Whirlpool Color 4 (305)-",
    "N Player Bullet (306)-",
    "S Player Bullet (307)-",
    "E Player Bullet (308)-",
    "W Player Bullet (309)-",
    "N Neutral Bullet (310)-",
    "S Neutral Bullet (311)-",
    "E Neutral Bullet (312)-",
    "W Neutral Bullet (313)-",
    "N Enemy Bullet (314)-",
    "S Enemy Bullet (315)-",
    "E Enemy Bullet (316)-",
    "W Enemy Bullet (317)-",
    "N Player Char (318)-",
    "S Player Char (319)-",
    "E Player Char (320)-",
    "W Player Char (321)-",
    "Player Color (322)-",
    "Missile Color (323)-",
    "Player Bullet Color (324)-",
    "Neutral Bullet Color (325)-",
    "Enemy Bullet Color (326)-"
  }
};

// Info for damage editing
static const char dmg_ids[22] =
{
  26,
  38,
  59,
  61,
  62,
  63,
  75,
  76,
  78,
  79,
  80,
  83,
  84,
  85,
  86,
  87,
  88,
  89,
  90,
  91,
  94,
  95
};

static const char *const dmg_strs[22] =
{
  "Lava-",
  "Explosion-",
  "Lazer-",
  "Bullet-",
  "Missile-",
  "Fire-",
  "Spike-",
  "Custom Hurt-",
  "Shooting Fire-",
  "Seeker-",
  "Snake-",
  "Slime Blob-",
  "Runner-",
  "Ghost-",
  "Dragon-",
  "Fish-",
  "Shark-",
  "Spider-",
  "Goblin-",
  "Spitting Tiger-",
  "Bear-",
  "Bear Cub-"
};

void set_confirm_buttons(struct element **elements)
{
  elements[0] = construct_button(15, 15, "OK", 0);
  elements[1] = construct_button(37, 15, "Cancel", 1);
}

void status_counter_info(struct world *mzx_world)
{
  struct dialog di;
  struct element *elements[2 + NUM_STATUS_COUNTERS];
  const char *status_counters_strings[NUM_STATUS_COUNTERS] =
  {
    "Status counter 1: ", "2: ", "3: ",
    "4: ", "5: ", "6: "
  };
  int i;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_confirm_buttons(elements);
  set_context(CTX_STATUS_COUNTERS);

  elements[2] = construct_input_box(12, 5,
   status_counters_strings[0], COUNTER_NAME_SIZE - 1,
   mzx_world->status_counters_shown[0]);

  for(i = 1; i < NUM_STATUS_COUNTERS; i++)
  {
    elements[i + 2] = construct_input_box(27, 5 + i,
     status_counters_strings[i], COUNTER_NAME_SIZE - 1,
     mzx_world->status_counters_shown[i]);
  }

  construct_dialog(&di, "Status Counters", 10, 4, 60, 18,
   elements, 2 + NUM_STATUS_COUNTERS, 2);

  run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  pop_context();
}

void board_exits(struct world *mzx_world)
{
  struct dialog di;
  struct element *elements[6];
  struct board *src_board = mzx_world->current_board;
  int dialog_result;
  int exits[4];

  memcpy(exits, src_board->board_dir, sizeof(int) * 4);

  set_confirm_buttons(elements);
  set_context(CTX_BOARD_EXITS);

  elements[2] = construct_board_list(12, 4, "Board to north:",
   1, exits + 0);
  elements[3] = construct_board_list(12, 6, "Board to south:",
   1, exits + 1);
  elements[4] = construct_board_list(12, 8, "Board to east:",
   1, exits + 2);
  elements[5] = construct_board_list(12, 10, "Board to west:",
   1, exits + 3);
  construct_dialog(&di, "Board Exits", 10, 4, 60, 18,
   elements, 6, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!dialog_result)
  {
    memcpy(src_board->board_dir, exits, sizeof(int) * 4);
  }

  pop_context();
}

static void bound_board_size(int *width, int *height)
{
  if((*width) * (*height) > MAX_BOARD_SIZE)
  {
    if(*width > *height)
      *height = MAX_BOARD_SIZE / *width;

    else
      *width = MAX_BOARD_SIZE / *height;
  }
}

// Size/pos of board/viewport
// Returns 1 if the board was resized
int size_pos(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int dialog_result;
  struct element *elements[9];
  struct dialog di;
  int resized = 0;

  int redo = 1;

  int results[6] =
  {
    src_board->viewport_x, src_board->viewport_y,
    src_board->viewport_width, src_board->viewport_height,
    src_board->board_width, src_board->board_height
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BOARD_SIZES);

  do
  {
    elements[0] = construct_button(13, 15, "OK", 0);
    elements[1] = construct_button(19, 15, "Center", 1);
    elements[2] = construct_button(29, 15, "Set as defaults", 2);
    elements[3] = construct_number_box(15, 4, "Viewport X pos: ",
     0, 79, NUMBER_BOX, results + 0);
    elements[4] = construct_number_box(15, 5, "Viewport Y pos: ",
     0, 24, NUMBER_BOX, results + 1);
    elements[5] = construct_number_box(15, 6, "Viewport Width: ",
     1, 80, NUMBER_BOX, results + 2);
    elements[6] = construct_number_box(15, 7, "Viewport Height:",
     1, 25, NUMBER_BOX, results + 3);
    elements[7] = construct_number_box(15, 11, "Board Width:    ",
     1, 32767, NUMBER_BOX, results + 4);
    elements[8] = construct_number_box(15, 12, "Board Height:   ",
     1, 32767, NUMBER_BOX, results + 5);

    construct_dialog(&di, "Board Sizes/Positions", 10, 4, 60, 18,
     elements, 9, 3);

    dialog_result = run_dialog(mzx_world, &di);
    destruct_dialog(&di);

    // Prevent previous keys from carrying through.
    force_release_all_keys();

    // Center
    if(dialog_result == 1)
    {
      results[0] = 40 - (results[2] / 2);
      results[1] = 12 - (results[3] / 2);
    }

    // Fix sizes
    if(results[2] > results[4])
      results[2] = results[4];
    if(results[3] > results[5])
      results[3] = results[5];
    if((results[2] + results[0]) > 80)
      results[2] = 80 - results[0];
    if((results[3] + results[1]) > 25)
      results[3] = 25 - results[1];

    switch(dialog_result)
    {
      case -1:
      {
        redo = 0;
        break;
      }

      case 0:
      {
        // Change the size; if it's not an enlargement, make sure
        // the player confirms first.

        // Hack to prevent multiplies of 256, for now - only relevant
        // with overlay off, but we want to avoid complications if
        // the user turns the overlay off later

        // As of the new format, this isn't really necessary.
        if((results[4] % 256) == 0)
          results[4]++;

        bound_board_size(results + 4, results + 5);

        if(((results[4] >= src_board->board_width) &&
          (results[5] >= src_board->board_height)) ||
          !confirm(mzx_world, "Reduce board size- Are you sure?"))
        {
          if(results[4] != src_board->board_width ||
           results[5] != src_board->board_height)
          {
            change_board_size(src_board, results[4], results[5]);
            resized = 1;
          }

          src_board->viewport_x = results[0];
          src_board->viewport_y = results[1];
          src_board->viewport_width = results[2];
          src_board->viewport_height = results[3];
          redo = 0;
        }
        break;
      }
      // Set defaults
      case 2:
      {
        struct editor_config_info *conf = get_editor_config();
        conf->viewport_x = results[0];
        conf->viewport_y = results[1];
        conf->viewport_w = results[2];
        conf->viewport_h = results[3];
        conf->board_width = results[4];
        conf->board_height = results[5];
        save_local_editor_config(conf, curr_file);
        break;
      }
    }
  } while(redo);

  pop_context();

  return resized;
}

// Size of vlayer
// Returns 1 if the vlayer was resized
int size_pos_vlayer(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[4];
  struct dialog di;
  int resized = 0;

  int results[2] = {
    mzx_world->vlayer_width,
    mzx_world->vlayer_height
  };

  int redo = 1;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_VLAYER_SIZES);

  do
  {
    elements[0] = construct_button(23, 8, "OK", 0);
    elements[1] = construct_button(29, 8, "Cancel", -1);

    elements[2] = construct_number_box(15, 4, "Vlayer Width:    ",
     1, 32767, NUMBER_BOX, results + 0);
    elements[3] = construct_number_box(15, 5, "Vlayer Height:   ",
     1, 32767, NUMBER_BOX, results + 1);

    construct_dialog(&di, "Vlayer Size", 10, 7, 60, 11,
     elements, 4, 2);

    dialog_result = run_dialog(mzx_world, &di);
    destruct_dialog(&di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    switch(dialog_result)
    {
      // Cancel
      case -1:
      {
        redo = 0;
        break;
      }

      // OK
      case 0:
      {
        // The vlayer has the same size restrictions as boards.
        bound_board_size(results + 0, results + 1);

        if(results[0] == mzx_world->vlayer_width &&
         results[1] == mzx_world->vlayer_height)
        {
          resized = 0;
          redo = 0;
        }
        else

        if(((results[0] >= mzx_world->vlayer_width) &&
          (results[1] >= mzx_world->vlayer_height)) ||
          !confirm(mzx_world, "Reduce vlayer size- Are you sure?"))
        {
          int size = results[0] * results[1];
          int old_size = mzx_world->vlayer_size;

          redo = 0;

          // Decreasing size-- remap first
          if(size < old_size)
            remap_vlayer(mzx_world, results[0], results[1]);

          mzx_world->vlayer_size = size;
          mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, size);
          mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, size);

          // Increasing size-- remap after
          if(size >= old_size)
          {
            // Clear new area
            memset(mzx_world->vlayer_chars + old_size, 32, size - old_size);
            memset(mzx_world->vlayer_colors + old_size, 7, size - old_size);
            remap_vlayer(mzx_world, results[0], results[1]);
          }

          resized = 1;
        }
        break;
      }
    }
  }
  while(redo);

  pop_context();

  return resized;
}

//Dialog- (board info)
//----------------------------------------------------------//
//       Board name- __________________________
//
//  [ ] Can shoot               ( ) Explosions to space     //
//  [ ] Can bomb                ( ) Explosions to ash
//  [ ] Fire burns space        ( ) Explosions to fire
//  [ ] Fire burns fakes
//  [ ] Fire burns trees        ( ) Can save
//  [ ] Fire burns brown        ( ) Can't save
//  [ ] Forest to floor         ( ) Can save on sensors
//  [ ] Collect bombs
//  [ ] Fire burns forever
//  [ ] Dragons randomly move   ( ) No overlay
//  [ ] Restart if hurt         ( ) Normal overlay
//  [ ] Reset board on entry    ( ) Static overlay
//  [ ] Player locked N/S       ( ) Transparent overlay
//  [ ] Player locked E/W
//  [ ] Player attack locked    Time limit- _00000_[!][!]   //
//
//  Charset to load-            Palette to load-
//  _______________________[v]  _______________________[v]  //
//     [ ] Reset/load when entering from the same board
//
//            _OK_  _Cancel_  _Set as defaults_             //
//
//----------------------------------------------------------//

// Board info
void board_info(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int dialog_result;
  struct element *elements[12];
  struct dialog di;
  int check_box_results[15] =
  {
    src_board->can_shoot, src_board->can_bomb,
    src_board->fire_burn_space, src_board->fire_burn_fakes,
    src_board->fire_burn_trees, src_board->fire_burn_brown,
    src_board->forest_becomes, src_board->collect_bombs,
    src_board->fire_burns,
    src_board->dragons_can_randomly_move,
    src_board->restart_if_zapped,
    src_board->reset_on_entry, src_board->player_ns_locked,
    src_board->player_ew_locked, src_board->player_attack_locked
  };
  int check_box_results_2[] =
  {
    src_board->reset_on_entry_same_board,
  };
  const char *check_box_strings[] =
  {
    "Can shoot", "Can bomb", "Fire burns space",
    "Fire burns fakes", "Fire burns trees", "Fire burns brown",
    "Forest to floor", "Collect bombs", "Fire burns forever",
    "Dragons move randomly",
    "Restart if hurt", "Reset board on entry", "Player locked N/S",
    "Player locked E/W", "Player attack locked"
  };
  const char *check_box_strings_2[] =
  {
    "Reset/load when entering from the same board"
  };
  const char *radio_strings_1[] =
  {
    "Explosions to space", "Explosions to ash",
    "Explosions to fire"
  };
  const char *radio_strings_2[] =
  {
    "Can save", "Can't save", "Can save on sensors"
  };
  const char *radio_strings_3[] =
  {
    "No overlay", "Normal overlay", "Static overlay",
    "Transparent overlay"
  };
  int radio_result_1 = src_board->explosions_leave;
  int radio_result_2 = src_board->save_mode;
  int radio_result_3 = src_board->overlay_mode;
  int time_limit = src_board->time_limit;
  char title_string[BOARD_NAME_SIZE];
  const char *charset_exts[] =
  {
    ".chr", NULL
  };
  const char *palette_exts[] =
  {
    ".pal", NULL
  };
  char charset_string[MAX_PATH];
  char palette_string[MAX_PATH];

  int pos = 3;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BOARD_INFO);

  strcpy(title_string, src_board->board_name);

  charset_string[0] = '\0';
  palette_string[0] = '\0';
  if(src_board->charset_path)
  {
    snprintf(charset_string, MAX_PATH, "%s", src_board->charset_path);
    charset_string[MAX_PATH - 1] = '\0';
  }
  if(src_board->palette_path)
  {
    snprintf(palette_string, MAX_PATH, "%s", src_board->palette_path);
    palette_string[MAX_PATH - 1] = '\0';
  }

  do
  {
    elements[0] = construct_button(13, 22, "OK", 0);
    elements[1] = construct_button(19, 22, "Cancel", -1);
    elements[2] = construct_button(29, 22, "Set as defaults", 1);

    elements[3] = construct_input_box(7, 1, "Board name- ",
     BOARD_NAME_SIZE - 1, title_string);

    elements[4] = construct_check_box(3, 2, check_box_strings,
     15, 21, check_box_results);

    elements[5] = construct_radio_button(31, 3, radio_strings_1,
     3, 19, &radio_result_1);
    elements[6] = construct_radio_button(31, 7, radio_strings_2,
     3, 19, &radio_result_2);
    elements[7] = construct_radio_button(31, 11, radio_strings_3,
     4, 19, &radio_result_3);

    elements[8] = construct_number_box(31, 16, "Time limit- ",
     0, 32767, NUMBER_BOX, &time_limit);

    elements[9] = construct_file_selector(3, 18, "Load charset on entry-",
     "Select a character set...", charset_exts, "(none)", 23, 1, "",
     charset_string, 2);

    elements[10] = construct_file_selector(31, 18, "Load palette on entry-",
     "Select a palette...", palette_exts, "(none)", 23, 1, "",
     palette_string, 2);

    elements[11] = construct_check_box(6, 20, check_box_strings_2,
     1, 46, check_box_results_2);

    construct_dialog(&di, "Board Settings", 10, 0, 60, 24,
     elements, ARRAY_SIZE(elements), pos);

    dialog_result = run_dialog(mzx_world, &di);
    pos = di.current_element;
    destruct_dialog(&di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    // Save defaults
    if(dialog_result == 1)
    {
      struct editor_config_info *conf = get_editor_config();
      conf->can_shoot = check_box_results[0];
      conf->can_bomb = check_box_results[1];
      conf->fire_burns_spaces = check_box_results[2];
      conf->fire_burns_fakes = check_box_results[3];
      conf->fire_burns_trees = check_box_results[4];
      conf->fire_burns_brown = check_box_results[5];
      conf->forest_to_floor = check_box_results[6];
      conf->collect_bombs = check_box_results[7];
      conf->fire_burns_forever = check_box_results[8];
      conf->dragons_can_randomly_move = check_box_results[9];
      conf->restart_if_hurt = check_box_results[10];
      conf->reset_on_entry = check_box_results[11];
      conf->player_locked_ns = check_box_results[12];
      conf->player_locked_ew = check_box_results[13];
      conf->player_locked_att = check_box_results[14];
      conf->reset_on_entry_same_board = check_box_results_2[0];
      conf->explosions_leave = radio_result_1;
      conf->saving_enabled = radio_result_2;
      conf->overlay_enabled = radio_result_3;
      conf->time_limit = time_limit;
      strcpy(conf->charset_path, charset_string);
      strcpy(conf->palette_path, palette_string);
      save_local_editor_config(conf, curr_file);
      // Setting focus back to the board name shows the user feedback.
      pos = 3;
    }
  } while(dialog_result > 0);

  if(!dialog_result)
  {
    strcpy(src_board->board_name, title_string);
    src_board->can_shoot = check_box_results[0];
    src_board->can_bomb = check_box_results[1];
    src_board->fire_burn_space = check_box_results[2];
    src_board->fire_burn_fakes = check_box_results[3];
    src_board->fire_burn_trees = check_box_results[4];
    src_board->fire_burn_brown = check_box_results[5];
    src_board->forest_becomes = check_box_results[6];
    src_board->collect_bombs = check_box_results[7];
    src_board->fire_burns = check_box_results[8];
    src_board->dragons_can_randomly_move = check_box_results[9];
    src_board->restart_if_zapped = check_box_results[10];
    src_board->reset_on_entry = check_box_results[11];
    src_board->player_ns_locked = check_box_results[12];
    src_board->player_ew_locked = check_box_results[13];
    src_board->player_attack_locked = check_box_results[14];
    src_board->reset_on_entry_same_board = check_box_results_2[0];
    src_board->explosions_leave = radio_result_1;
    src_board->save_mode = radio_result_2;

    if(src_board->overlay_mode != radio_result_3)
    {
      setup_overlay(src_board, radio_result_3);
      src_board->overlay_mode = radio_result_3;
    }

    src_board->time_limit = time_limit;
    board_set_charset_path(src_board, charset_string, strlen(charset_string));
    board_set_palette_path(src_board, palette_string, strlen(palette_string));
  }

  pop_context();
}

// Chars #1-8
static void global_chars(struct world *mzx_world)
{
  int i;
  int current_id, dialog_result;
  int current_menu = 0;
  int results[24];
  struct element *elements[27];
  struct dialog di;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CHANGE_CHAR_IDS);

  do
  {
    elements[0] = construct_button(15, 15, "Next", 1);
    elements[1] = construct_button(25, 15, "Previous", 2);
    elements[2] = construct_button(39, 15, "Done", 3);

    for(i = 0; i < 12; i++)
    {
      if(char_values[current_menu][i] & 512)
      {
        elements[i + 3] = construct_color_box(2 +
         (25 - (int)strlen(char_strs[current_menu][i])), 2 + i,
         char_strs[current_menu][i], 0, results + i);
      }
      else
      {
        elements[i + 3] = construct_char_box(2 +
         (25 - (int)strlen(char_strs[current_menu][i])), 2 + i,
         char_strs[current_menu][i],
         char_values[current_menu][i] > 127, results + i);
      }

      current_id = char_values[current_menu][i] & 511;
      if(current_id == 323)
        results[i] = missile_color;
      else

      if((current_id >= 324) && (current_id <= 326))
        results[i] = bullet_color[current_id - 324];
      else
        results[i] = id_chars[current_id];
    }

    for(; i < 24; i++)
    {
      if(char_values[current_menu][i] & 512)
      {
        elements[i + 3] = construct_color_box(32 +
         (25 - (int)strlen(char_strs[current_menu][i])), i - 10,
         char_strs[current_menu][i], 0, results + i);
      }
      else
      {
        elements[i + 3] = construct_char_box(32 +
         (25 - (int)strlen(char_strs[current_menu][i])), i - 10,
         char_strs[current_menu][i],
         char_values[current_menu][i] > 127, results + i);
      }

      current_id = char_values[current_menu][i] & 511;
      if(current_id == 323)
        results[i] = missile_color;
      else

      if((current_id >= 324) && (current_id <= 326))
        results[i] = bullet_color[current_id - 324];
      else
        results[i] = id_chars[current_id];
    }

    construct_dialog(&di, "Edit Characters", 9, 4, 62, 18,
     elements, 27, 3);

    // Run
    dialog_result = run_dialog(mzx_world, &di);
    destruct_dialog(&di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    if(dialog_result == -1)
      break;

    // Get from storage
    for(i = 0; i < 24; i++)
    {
      current_id = char_values[current_menu][i] & 511;
      if(current_id == 323)
        missile_color = results[i];
      else

      if((current_id >= 324) && (current_id <= 326))
        bullet_color[current_id - 324] = results[i];
      else
        id_chars[current_id] = results[i];
    }

    // Setup lit bomb sequence or doors
    if(current_menu == 2)
    {
      id_chars[170] = id_chars[169] - 1;
      id_chars[171] = id_chars[170] - 1;
      id_chars[172] = id_chars[171] - 1;
      id_chars[173] = id_chars[172] - 1;
      id_chars[174] = id_chars[173] - 1;
      id_chars[175] = id_chars[174] - 1;
    }
    else

    if(current_menu == 3)
    {
      // Doors
      id_chars[199] = id_chars[198]; // '/'
      id_chars[201] = id_chars[200]; // '\'
      id_chars[202] = id_chars[200]; // '\'
      id_chars[203] = id_chars[200]; // '\'
      id_chars[204] = id_chars[198]; // '/'
      id_chars[205] = id_chars[198]; // '/'
      id_chars[206] = id_chars[189]; // '|'
      id_chars[207] = id_chars[188]; // '-'
      id_chars[208] = id_chars[189]; // '|'
      id_chars[209] = id_chars[188]; // '-'
      id_chars[210] = id_chars[189]; // '|'
      id_chars[211] = id_chars[188]; // '-'
      id_chars[212] = id_chars[189]; // '|'
      id_chars[213] = id_chars[188]; // '-'
      id_chars[214] = id_chars[189]; // '|'
      id_chars[215] = id_chars[188]; // '-'
      id_chars[216] = id_chars[189]; // '|'
      id_chars[217] = id_chars[188]; // '-'
      id_chars[218] = id_chars[189]; // '|'
      id_chars[219] = id_chars[188]; // '-'
      id_chars[220] = id_chars[189]; // '|'
      id_chars[221] = id_chars[188]; // '-'
      id_chars[222] = id_chars[198]; // '/'
      id_chars[223] = id_chars[198]; // '/'
      id_chars[224] = id_chars[200]; // '\'
      id_chars[225] = id_chars[200]; // '\'
      id_chars[226] = id_chars[200]; // '\'
      id_chars[227] = id_chars[200]; // '\'
      id_chars[228] = id_chars[198]; // '/'
      id_chars[229] = id_chars[198]; // '/'
    }

    // Next, prev, or done
    if(dialog_result == 1)
    {
      current_menu++;
      if((current_menu) > 7)
        current_menu = 0;
    }
    else

    if(dialog_result == 2)
    {
      current_menu--;

      if((current_menu) < 0)
        current_menu = 7;
    }
  } while((dialog_result == 1) || (dialog_result == 2));

  pop_context();
}

static void global_dmg(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[24];
  struct dialog di;
  int results[22];
  int i;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CHANGE_DAMAGE);
  set_confirm_buttons(elements);

  for(i = 0; i < 11; i++)
  {
    results[i] = id_dmg[(int)dmg_ids[i]];
    elements[i + 2] = construct_number_box(2 +
     (14 - (int)strlen(dmg_strs[i])), 2 + i,
     dmg_strs[i], 0, 255, NUMBER_BOX, results + i);
  }

  for(; i < 22; i++)
  {
    results[i] = id_dmg[(int)dmg_ids[i]];
    elements[i + 2] = construct_number_box(31 +
     (14 - (int)strlen(dmg_strs[i])), i - 9,
     dmg_strs[i], 0, 255, NUMBER_BOX, results + i);
  }

  construct_dialog(&di, "Edit Damage", 10, 4, 60, 18,
   elements, 24, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!dialog_result)
  {
    for(i = 0; i < 22; i++)
    {
      id_dmg[(int)dmg_ids[i]] = results[i];
    }
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  pop_context();
}

void global_info(context *ctx)
{
  struct world *mzx_world = ctx->world;
  int death_board = mzx_world->death_board;
  int endgame_board = mzx_world->endgame_board;
  int death_x = mzx_world->death_x;
  int death_y = mzx_world->death_y;
  int endgame_x = mzx_world->endgame_x;
  int endgame_y = mzx_world->endgame_y;
  int first_board = mzx_world->first_board;
  int edge_color = mzx_world->edge_color;
  int starting_lives = mzx_world->starting_lives;
  int lives_limit = mzx_world->lives_limit;
  int starting_health = mzx_world->starting_health;
  int health_limit = mzx_world->health_limit;
  int radio_result_1 = 2;
  int radio_result_2 = 1;
  int check_box_results_1[1] = { mzx_world->game_over_sfx };
  int check_box_results_2[3] =
  {
    mzx_world->enemy_hurt_enemy, mzx_world->clear_on_exit,
    mzx_world->only_from_swap
  };
  const char *radio_strings_1[] =
  {
    "Death- Same position", "Death- Restart board",
    "Death- Teleport"
  };
  const char *radio_strings_2[] =
  {
    "Endgame- Game over", "Endgame- Teleport"
  };
  const char *check_box_strings_1[] = { "Play game over sfx" };
  const char *check_box_strings_2[] =
  {
    "Enemies' bullets hurt other enemies",
    "Clear messages and projectiles on exit",
    "Can only play world from a 'SWAP WORLD'"
  };
  struct dialog a_di;
  struct dialog b_di;
  struct element *a_elements[15];
  struct element *b_elements[12];
  int dialog_result;
  int redo = 0;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  do
  {
    set_confirm_buttons(a_elements);
    a_elements[2] = construct_board_list(5, 2, "First board-",
     0, &first_board);
    a_elements[3] = construct_color_box(38, 3, "Edging color- ",
     0, &edge_color);
    a_elements[4] = construct_label(5+10+4, 5, "Lives"),
    a_elements[5] = construct_label(32+10+3, 5, "Health"),
    a_elements[6] = construct_number_box(5, 6, "Starting- ",
     1, 32767, NUMBER_BOX, &starting_lives);
    a_elements[7] = construct_number_box(5, 7, "Maximum-  ",
     1, 32767, NUMBER_BOX, &lives_limit);
    a_elements[8] = construct_number_box(32, 6, "Starting- ",
     1, 32767, NUMBER_BOX, &starting_health);
    a_elements[9] = construct_number_box(32, 7, "Maximum-  ",
     1, 32767, NUMBER_BOX, &health_limit);
    a_elements[10] = construct_check_box(7, 9, check_box_strings_2,
     3, 39, check_box_results_2);
    a_elements[11] = construct_button(5, 13, "More", 2);
    a_elements[12] = construct_button(12, 13, "Edit Chars", 3);
    a_elements[13] = construct_button(25, 13, "Edit Dmg", 4);
    a_elements[14] = construct_button(36, 13, "Edit Global Robot", 5);

    construct_dialog(&a_di, "Global Settings", 10, 4, 60, 18,
     a_elements, 15, 2);

    redo = 0;
    set_context(CTX_GLOBAL_SETTINGS);

    dialog_result = run_dialog(mzx_world, &a_di);
    destruct_dialog(&a_di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    pop_context();

    switch(dialog_result)
    {
      case 2:
      {
        // Returns 1 for previous
        set_context(CTX_GLOBAL_SETTINGS_2);

        if(death_board == DEATH_SAME_POS)
        {
          death_board = 0;
          radio_result_1 = 0;
        }

        if(death_board == NO_DEATH_BOARD)
        {
          death_board = 0;
          radio_result_1 = 1;
        }

        if(endgame_board == NO_ENDGAME_BOARD)
        {
          endgame_board = 0;
          radio_result_2 = 0;
        }

        set_confirm_buttons(b_elements);
        b_elements[2] = construct_board_list(1, 2, "Death board-",
         0, &death_board);
        b_elements[3] = construct_number_box(1, 5, "Death X- ",
         0, 32767, NUMBER_BOX, &death_x);
        b_elements[4] = construct_number_box(1, 6, "Death Y- ",
         0, 32767, NUMBER_BOX, &death_y);
        b_elements[5] = construct_radio_button(1, 8, radio_strings_1,
         3, 20, &radio_result_1);
        b_elements[6] = construct_board_list(30, 2, "Endgame board-",
         0, &endgame_board);
        b_elements[7] = construct_number_box(30, 5, "Endgame X- ",
         0, 32767, NUMBER_BOX, &endgame_x);
        b_elements[8] = construct_number_box(30, 6, "Endgame Y- ",
         0, 32767, NUMBER_BOX, &endgame_y);
        b_elements[9] = construct_radio_button(30, 8, radio_strings_2,
         2, 18, &radio_result_2);
        b_elements[10] = construct_check_box(30, 11, check_box_strings_1,
         1, 18, check_box_results_1);

        b_elements[11] = construct_button(25, 13, "Previous", 2); //38, 9

        construct_dialog(&b_di, "Global Settings (Continued)", 10, 4,
         60, 18, b_elements, 12, 2);

        dialog_result = run_dialog(mzx_world, &b_di);
        destruct_dialog(&b_di);

        pop_context();

        if(dialog_result == 2)
        {
          redo = 1;
          break;
        }

        // If okay, fall through and apply settings
        if(dialog_result == 1)
          break;
      }

      /* fallthrough */

      case 0:
      {
        if(radio_result_1 == 2)
          mzx_world->death_board = death_board;
        else

        if(radio_result_1 == 1)
          mzx_world->death_board = NO_DEATH_BOARD;

        else
          mzx_world->death_board = DEATH_SAME_POS;

        if(radio_result_2 == 1)
          mzx_world->endgame_board = endgame_board;
        else
          mzx_world->endgame_board = NO_ENDGAME_BOARD;

        mzx_world->death_x = death_x;
        mzx_world->endgame_x = endgame_x;
        mzx_world->death_y = death_y;
        mzx_world->endgame_y = endgame_y;
        mzx_world->game_over_sfx = check_box_results_1[0];

        mzx_world->first_board = first_board;
        mzx_world->edge_color = edge_color;

        // These are the only parts of the default global data
        // that the editor can change, so it has to be updated
        // so testing won't screw things up.
        set_counter(mzx_world, "LIVES", starting_lives, 0);
        set_counter(mzx_world, "HEALTH", starting_health, 0);

        mzx_world->starting_lives = starting_lives;
        mzx_world->lives_limit = lives_limit;
        mzx_world->starting_health = starting_health;
        mzx_world->health_limit = health_limit;
        mzx_world->enemy_hurt_enemy = check_box_results_2[0];
        mzx_world->clear_on_exit = check_box_results_2[1];
        mzx_world->only_from_swap = check_box_results_2[2];

        if(starting_lives > lives_limit)
          mzx_world->starting_lives = lives_limit;

        if(starting_health > health_limit)
          mzx_world->starting_health = health_limit;

        break;
      }

      case 3:
      {
        global_chars(mzx_world); // Chars
        break;
      }

      case 4:
      {
        global_dmg(mzx_world); // Dmg
        break;
      }

      case 5:
      {
        edit_global_robot(ctx);
        break;
      }
    }
  } while(redo);
}

/*
* +-Goto-------------------------------+
* |
* |  X-[00000][-][+]  Y-[00000][-][+]  |
* |
* |      [  OK  ]        [Cancel]      |
* |
* +------------------------------------+
*/

int board_goto(struct world *mzx_world, int overlay_edit,
 int *cursor_board_x, int *cursor_board_y)
{
  int result = 0;
  int goto_x = *cursor_board_x;
  int goto_y = *cursor_board_y;
  struct element *elements[4];
  struct dialog di;

  struct board *cur_board = mzx_world->current_board;
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;

  const char * const titles[] =
  {
    "Goto board location",
    "Goto overlay location",
    "Goto vlayer location"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  if(overlay_edit == 2)
  {
    // Vlayer editing
    board_width = mzx_world->vlayer_width;
    board_height = mzx_world->vlayer_height;
  }

  elements[0] = construct_button( 7, 4, "  OK  ", 0);
  elements[1] = construct_button(24, 4, "Cancel", 1);
  elements[2] = construct_number_box( 3, 2, "X-",
   0, board_width - 1, NUMBER_BOX, &goto_x);
  elements[3] = construct_number_box(20, 2, "Y-",
   0, board_height - 1, NUMBER_BOX, &goto_y);

  construct_dialog(&di, titles[overlay_edit],
   21, 7, 38, 7, elements, ARRAY_SIZE(elements), 2);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!result)
  {
    *cursor_board_x = goto_x;
    *cursor_board_y = goto_y;
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return result;
}
