/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2018-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

/* PARAM.CPP- Millions of dialog boxes for inputting parameters of
          a specified id. If a parameter is already available,
          it is edited. Special functions are called in other
          files for robots, scrolls, and signs. */

#include <string.h>

#include "../core.h"
#include "../data.h"
#include "../idput.h"
#include "../robot.h"
#include "../scrdisp.h"
#include "../util.h"
#include "../window.h"

#include "../event/event.h"

#include "edit_di.h"
#include "param.h"
#include "robo_ed.h"
#include "window.h"
#include "world.h"

static const char *const potion_fx[16] =
{
  [POTION_DUD]          = "No Effect   ",
  [POTION_INVINCO]      = "Invinco     ",
  [POTION_BLAST]        = "Blast       ",
  [POTION_SMALL_HEALTH] = "Health x10  ",
  [POTION_BIG_HEALTH]   = "Health x50  ",
  [POTION_POISON]       = "Poison      ",
  [POTION_BLIND]        = "Blind       ",
  [POTION_KILL]         = "Kill Enemies",
  [POTION_FIREWALK]     = "Lava Walker ",
  [POTION_DETONATE]     = "Detonate    ",
  [POTION_BANISH]       = "Banish      ",
  [POTION_SUMMON]       = "Summon      ",
  [POTION_AVALANCHE]    = "Avalanche   ",
  [POTION_FREEZE]       = "Freeze Time ",
  [POTION_WIND]         = "Wind        ",
  [POTION_SLOW]         = "Slow Time   "
};

static int pe_chest(struct world *mzx_world, int param)
{
  int type = param & 0x0F;
  int var = param >> 4;

  const char *list[] =
  {
    [CHEST_EMPTY]   = "Empty               ",
    [CHEST_KEY]     = "Key                 ",
    [CHEST_COINS]   = "Coins               ",
    [CHEST_LIVES]   = "Lives               ",
    [CHEST_AMMO]    = "Ammo                ",
    [CHEST_GEMS]    = "Gems                ",
    [CHEST_HEALTH]  = "Health              ",
    [CHEST_POTION]  = "Potion              ",
    [CHEST_RING]    = "Ring                ",
    [CHEST_LOBOMBS] = "Lo Bombs            ",
    [CHEST_HIBOMBS] = "Hi Bombs            "
  };

  // First, pick chest contents
  type =
   list_menu(list, 21, "Choose chest contents", type, ARRAY_SIZE(list), 27, 0);

  if(type < 0)
    return -1;

  switch(type)
  {
    case CHEST_EMPTY:
    {
      var = 0;
      break;
    }

    case CHEST_KEY:
    {
      var = color_selection(var, 0);
      if(var > 0)
        var = var & 0x0F;
      break;
    }

    case CHEST_POTION:
    case CHEST_RING:
    {
      var = list_menu(potion_fx, 13, "Choose effect", var, 16, 31, 0);
      break;
    }

    case CHEST_COINS:
    case CHEST_LIVES:
    case CHEST_AMMO:
    case CHEST_GEMS:
    case CHEST_HEALTH:
    case CHEST_LOBOMBS:
    case CHEST_HIBOMBS: // Choose amount
    {
      struct dialog di;
      struct element *elements[3];
      int dialog_result;
      enum number_box_type input_type = NUMBER_BOX_MULT_FIVE;
      const char *question = "Amount (multiple of five): ";

      set_confirm_buttons(elements);
      if(type == CHEST_LIVES)
      {
        question = "Number of lives: ";
        input_type = NUMBER_BOX;
      }

      elements[2] = construct_number_box(5, 6, question,
       0, 15, input_type, &var);

      construct_dialog(&di, "Set Quantity", 10, 5, 60, 18,
       elements, 3, 2);

      dialog_result = run_dialog(mzx_world, &di);

      if(dialog_result)
        var = -1;

      destruct_dialog(&di);
      break;
    }
  }

  if(var < 0)
    return -1;

  return (var << 4) | type;
}

static int pe_health(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Amount: ",
   0, 255, NUMBER_BOX, &param);

  construct_dialog(&di, "Set Health/Ammo", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_ring(struct world *mzx_world, int param)
{
  int p;
  if(param >= 16)
    param = 0;
  p = list_menu(potion_fx, 13, "Choose effect", param, 16, 31, 0);
  if(p < 0)
    return -1;
  else
    return p;
}

static int pe_bomb(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Low strength bomb", "High strength bomb"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   2, 18, &param);

  construct_dialog(&di, "Set Bomb", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_lit_bomb(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  int type = param >> 7;
  int stage = 7 - (param & 0x3F);
  const char *radio_strings[] =
  {
    "Low strength bomb", "High strength bomb"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   2, 18, &type);
  elements[3] = construct_number_box(15, 9, "Fuse length: ",
   1, 7, NUMBER_LINE, &stage);

  construct_dialog(&di, "Set Lit Bomb", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (type << 7) | (7 - stage);
}

static int pe_explosion(struct world *mzx_world, int param)
{
  int size = param >> 4;
  int stage = (param & 0x0F) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Explosion stage: ",
   1, 4, NUMBER_LINE, &stage);
  elements[3] = construct_number_box(15, 8, "Explosion size: ",
   0, 15, NUMBER_BOX, &size);

  construct_dialog(&di, "Set Explosion", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (size << 4) | (stage - 1);
}

static int pe_door(struct world *mzx_world, int param)
{
  int alignment = param & 1;
  int opens = (param >> 1) & 0x03;
  int check_results[1] = { (param >> 3) & 0x01 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "Horizontal", "Vertical"
  };
  const char *radio_strings_2[] =
  {
    "Opens N/W", "Opens N/E", "Opens S/W", "Opens S/E"
  };
  const char *check_strings[] = { "Locked door" };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 3, radio_strings_1,
   2, 18, &alignment);
  elements[3] = construct_radio_button(15, 6, radio_strings_2,
   4, 9, &opens);
  elements[4] = construct_check_box(15, 11, check_strings,
   1, 11, check_results);

  construct_dialog(&di, "Set Door", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return alignment | (opens << 1) | (check_results[0] << 3);
}

static int pe_gate(struct world *mzx_world, int param)
{
  int check_results[1] = { param };
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *check_strings[] = { "Locked gate" };

  set_confirm_buttons(elements);
  elements[2] = construct_check_box(15, 7, check_strings,
   1, 11, check_results);

  construct_dialog(&di, "Set Gate", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return check_results[0];
}

static int pe_transport(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West", "All"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   5, 5, &param);

  construct_dialog(&di, "Set Transport", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_pouch(struct world *mzx_world, int param)
{
  int coins = param >> 4;
  int gems = param & 0x0F;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(6, 6,
   "Number of gems (mult. of 5):  ", 0, 15, NUMBER_BOX_MULT_FIVE, &gems);
  elements[3] = construct_number_box(6, 8,
   "Number of coins (mult. of 5): ", 0, 15, NUMBER_BOX_MULT_FIVE, &coins);

  construct_dialog(&di, "Set Pouch", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (coins << 4) | gems;
}

static int pe_pusher(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   4, 5, &param);

  construct_dialog(&di, "Set Pusher/Missile/Spike", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_lazer_gun(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int start = ((param >> 2) & 0x07) + 1;
  int end = (param >> 5) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Start time: ",
   1, 8, NUMBER_LINE, &start);
  elements[4] = construct_number_box(15, 11, "Length on:  ",
   1, 7, NUMBER_LINE, &end);

  construct_dialog(&di, "Set 'Lazer' Gun", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | (((start - 1) << 2) + ((end - 1) << 5));
}

static int pe_bullet(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int type = param >> 2;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Player", "Neutral", "Enemy"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 5, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_radio_button(15, 10, radio_strings_2,
   3, 7, &type);

  construct_dialog(&di, "Set Bullet", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | (type << 2);
}

static int pe_ricochet_panel(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Orientation \\", "Orientation /"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 7, radio_strings,
   2, 13, &param);

  construct_dialog(&di, "Set Ricochet Panel", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_mine(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;

  param = (param >> 4) + 1;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Blast radius:  ",
   1, 8, NUMBER_LINE, &param);

  construct_dialog(&di, "Set Mine", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (param - 1) << 4;
}

static int pe_snake(struct world *mzx_world, int param)
{
  int dir = param & 0x3;
  int intel = (param >> 4) + 1;
  int check_results[1] = { ((param >> 2) & 1) ^ 1 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };
  const char *check_strings[] = { "Fast movement " };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Intelligence:  ",
   1, 8, NUMBER_LINE, &intel);
  elements[4] = construct_check_box(15, 11, check_strings,
   1, 13, check_results);

  construct_dialog(&di, "Set Snake", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | ((check_results[0] ^ 1) << 2) | ((intel - 1) << 4);
}

static int pe_eye(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int radius = ((param >> 3) & 0x07) + 1;
  int check_results[1] = { (param >> 6) ^ 1 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Fast movement " };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Intelligence: ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 8, "Blast radius: ",
   1, 8, NUMBER_LINE, &radius);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 13, check_results);

  construct_dialog(&di, "Set Eye", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((radius - 1) << 3) | ((check_results[0] ^ 1) << 6);
}

static int pe_thief(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int speed = ((param >> 3) & 0x03) + 1;
  int gems = (param >> 7) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6,  "Intelligence:   ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 8,  "Movement speed: ",
   1, 4, NUMBER_LINE, &speed);
  elements[4] = construct_number_box(15, 10, "Gems stolen:    ",
   1, 2, NUMBER_LINE, &gems);

  construct_dialog(&di, "Set Thief", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((speed - 1) << 3) | ((gems - 1) << 7);
}

static int pe_slime_blob(struct world *mzx_world, int param)
{
  int speed = (param & 0x07) + 1;
  int check_results[2] = { (param >> 6) & 0x01, (param >> 7) & 0x01 };
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *check_strings[] =
  {
    "Hurts player", "Invincible"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Spread speed: ",
   1, 8, NUMBER_LINE, &speed);
  elements[3] = construct_check_box(15, 8, check_strings,
   2, 12, check_results);

  construct_dialog(&di, "Set Slime Blob", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (speed - 1) | (check_results[0] << 6) |
   (check_results[1] << 7);
}

static int pe_runner(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int hp = (param >> 6) + 1;
  int speed = ((param >> 2) & 3) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Hit points: ",
   1, 4, NUMBER_LINE, &hp);
  elements[4] = construct_number_box(15, 11, "Movement speed: ",
   1, 4, NUMBER_LINE, &speed);

  construct_dialog(&di, "Set Runner", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | ((speed - 1) << 2) | ((hp - 1) << 6);
}

static int pe_ghost(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int speed = ((param >> 4) & 0x03) + 1;
  int check_results[] = { (param >> 3) & 0x01 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Invincible" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Intelligence:  ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 8, "Movement speed: ",
   1, 4, NUMBER_LINE, &speed);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 10, check_results);

  construct_dialog(&di, "Set Ghost", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((speed - 1) << 4) | (check_results[0] << 3);
}

static int pe_dragon(struct world *mzx_world, int param)
{
  int fire_rate = (param & 0x03) + 1;
  int hp = ((param >> 5) & 0x07) + 1;
  int check_results[1] = { ((param >> 2) & 0x01) };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Moves" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Firing rate: ",
   1, 4, NUMBER_LINE, &fire_rate);
  elements[3] = construct_number_box(15, 8, "Hit points:  ",
   1, 8, NUMBER_LINE, &hp);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 5, check_results);

  construct_dialog(&di, "Set Dragon", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | ((hp - 1) << 5) | (check_results[0] << 2);
}

static int pe_fish(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int check_results[4] =
  {
    (param >> 6) & 0x01, (param >> 5) & 0x01,
    (param >> 7) & 0x01, ((param >> 3) & 0x01) ^ 1
  };

  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *check_strings[] =
  {
    "Hurts player", "Affected by current",
    "2 hit points", "Fast movement"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 5, "Intelligence: ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_check_box(15, 7, check_strings,
   4, 19, check_results);

  construct_dialog(&di, "Set Fish", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | (check_results[0] << 6) |
   (check_results[1] << 5) | (check_results[2] << 7) |
   ((check_results[3] ^1) << 3);
}

static int pe_shark(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int fire_rate = ((param >> 5) & 0x07) + 1;
  int fires = ((param >> 3) & 0x03);
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Fires bullets", "Fires seekers",
    "Fires fire", "Fires nothing"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 4, "Intelligence: ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 6, "Firing rate:  ",
   1, 8, NUMBER_LINE, &fire_rate);
  elements[4] = construct_radio_button(15, 8, radio_strings,
   4, 13, &fires);

  construct_dialog(&di, "Set Shark/Spitting Tiger", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((fire_rate - 1) << 5) | (fires << 3);
}

static int pe_spider(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int web = ((param >> 3) & 0x03);
  int check_results[2] =
  {
    ((param >> 5) & 0x01) ^ 1, ((param >> 7) & 0x01)
  };

  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Thin web only", "Thick web only", "Any web", "Anywhere"
  };
  const char *check_strings[] =
  {
    "Fast movement", "2 hit points"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 3, "Intelligence: ",
   1, 8, NUMBER_LINE, &intel);
  elements[3] = construct_radio_button(15, 5, radio_strings,
   4, 14, &web);
  elements[4] = construct_check_box(15, 10, check_strings,
   2, 13, check_results);

  construct_dialog(&di, "Set Spider", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | (web << 3) |
   ((check_results[0] ^ 1) << 5) | (check_results[1] << 7);
}

static int pe_goblin(struct world *mzx_world, int param)
{
  int intel = ((param >> 6) & 0x03) + 1;
  int rest_len = (param & 0x03) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 9, "Rest length:  ",
   1, 4, NUMBER_LINE, &rest_len);

  construct_dialog(&di, "Set Goblin", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (rest_len - 1) | ((intel - 1) << 6);
}

static int pe_bullet_gun(struct world *mzx_world, int param)
{
  int dir = (param >> 3) & 0x03;
  int intel = ((param >> 5) & 0x03) + 1;
  int fire_rate = (param & 0x07) + 1;
  int type = (param >> 7);
  struct dialog di;
  struct element *elements[6];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Fires bullets", "Fires fire"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 2, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, NUMBER_LINE, &intel);
  elements[4] = construct_number_box(15, 9, "Firing rate:  ",
   1, 8, NUMBER_LINE, &fire_rate);
  elements[5] = construct_radio_button(15, 11, radio_strings_2,
   2, 13, &type);

  construct_dialog(&di, "Set Bullet Gun/Spinning Gun", 10, 5, 60, 18,
   elements, 6, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | ((dir << 3) + ((intel - 1) << 5) + (type << 7));
}

static int pe_bear(struct world *mzx_world, int param)
{
  int sens = (param & 0x07) + 1;
  int speed = ((param >> 3) & 0x03) + 1;
  int check_results[1] = { (param >> 7) };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "2 hit points" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Sensitivity:   ",
   1, 8, NUMBER_LINE, &sens);
  elements[3] = construct_number_box(15, 8, "Movement speed: ",
   1, 4, NUMBER_LINE, &speed);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 12, check_results);

  construct_dialog(&di, "Set Bear", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (sens - 1) | ((speed - 1) << 3) | (check_results[0] << 7);
}

static int pe_bear_cub(struct world *mzx_world, int param)
{
  int intel = (param & 0x03) + 1;
  int switch_rate = ((param >> 2) & 0x03) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, NUMBER_LINE, &intel);
  elements[3] = construct_number_box(15, 9, "Switch rate:  ",
   1, 4, NUMBER_LINE, &switch_rate);

  construct_dialog(&di, "Set Bear Cub", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((switch_rate - 1) << 2);
}

static int pe_missile_gun(struct world *mzx_world, int param)
{
  int dir = (param >> 3) & 0x03;
  int intel = ((param >> 5) & 0x03) + 1;
  int fire_rate = (param & 0x07) + 1;
  int type = (param >> 7);
  struct dialog di;
  struct element *elements[6];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Fires once", "Fires multiple"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 2, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 7, "Intelligence:  ",
   1, 4, NUMBER_LINE, &intel);
  elements[4] = construct_number_box(15, 9, "Firing rate:   ",
   1, 8, NUMBER_LINE, &fire_rate);
  elements[5] = construct_radio_button(15, 11, radio_strings_2,
   2, 14, &type);

  construct_dialog(&di, "Set Missile Gun", 10, 5, 60, 18,
   elements, 6, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | (dir << 3) |
   ((intel - 1) << 5) | (type << 7);
}

// Returns parameter or -1 for ESC. Must pass a legit parameter, or -1 to
// use default or load up a new robot/scroll/sign.
int edit_param(struct world *mzx_world, int id, int param)
{
  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // Switch default params to handle the basic stuff
  switch(def_params[id])
  {
    // No editing allowed
    case -2:
    {
      // If the char ID is 255 fall through; char ID 255 things act like customs
      if(id_chars[id] != 255)
      {
        // If has a param, return it, otherwise return 0
        if(param > -1)
          return param;

        return 0;
      }
    }

    /* fallthrough */

    // Character
    case -3:
    {
      int new_param = param;

      // Use default char.
      if(new_param < 0)
      {
        char def = get_default_id_char(id);

        // If this type is custom* by default (usually the case here), use 177
        if(def == 255)
          new_param = 177;
        else
          new_param = def;
      }

      new_param = char_selection(new_param);

      if(new_param < 0)
        new_param = param;

      return new_param;
    }

    // Board
    case -4:
    {
      int new_param = param;
      if(new_param < 0)
        new_param = 0;

      new_param = choose_board(mzx_world, new_param,
       "Choose destination board", 0);
      if(new_param < 0)
        new_param = param;

      return new_param;
    }
  }

  if(param < 0)
    param = def_params[id];

  // Switch according to thing
  switch(id)
  {
    case CHEST:
    {
      param = pe_chest(mzx_world, param);
      break;
    }

    case HEALTH:
    case AMMO:
    {
      param = pe_health(mzx_world, param);
      break;
    }

    case RING:
    case POTION:
    {
      param = pe_ring(mzx_world, param);
      break;
    }

    case BOMB:
    {
      param = pe_bomb(mzx_world, param);
      break;
    }

    case LIT_BOMB:
    {
      param = pe_lit_bomb(mzx_world, param);
      break;
    }

    case EXPLOSION:
    {
      param = pe_explosion(mzx_world, param);
      break;
    }

    case DOOR:
    {
      param = pe_door(mzx_world, param);
      break;
    }

    case GATE:
    {
      param = pe_gate(mzx_world, param);
      break;
    }

    case TRANSPORT:
    {
      param = pe_transport(mzx_world, param);
      break;
    }

    case POUCH:
    {
      param = pe_pouch(mzx_world, param);
      break;
    }

    case PUSHER:
    case MISSILE:
    case SPIKE:
    {
      param = pe_pusher(mzx_world, param);
      break;
    }

    case SHOOTING_FIRE:
    {
      param = pe_pusher(mzx_world, param >> 1) << 1;
      break;
    }

    case LAZER_GUN:
    {
      param = pe_lazer_gun(mzx_world, param);
      break;
    }

    case BULLET:
    {
      param = pe_bullet(mzx_world, param);
      break;
    }

    case RICOCHET_PANEL:
    {
      param = pe_ricochet_panel(mzx_world, param);
      break;
    }

    case MINE:
    {
      param = pe_mine(mzx_world, param);
      break;
    }

    case SNAKE:
    {
      param = pe_snake(mzx_world, param);
      break;
    }

    case EYE:
    {
      param = pe_eye(mzx_world, param);
      break;
    }

    case THIEF:
    {
      param = pe_thief(mzx_world, param);
      break;
    }

    case SLIMEBLOB:
    {
      param = pe_slime_blob(mzx_world, param);
      break;
    }

    case RUNNER:
    {
      param = pe_runner(mzx_world, param);
      break;
    }

    case GHOST:
    {
      param = pe_ghost(mzx_world, param);
      break;
    }

    case DRAGON:
    {
      param = pe_dragon(mzx_world, param);
      break;
    }

    case FISH:
    {
      param = pe_fish(mzx_world, param);
      break;
    }

    case SHARK:
    case SPITTING_TIGER:
    {
      param = pe_shark(mzx_world, param);
      break;
    }

    case SPIDER:
    {
      param = pe_spider(mzx_world, param);
      break;
    }

    case GOBLIN:
    {
      param = pe_goblin(mzx_world, param);
      break;
    }

    case BULLET_GUN:
    case SPINNING_GUN:
    {
      param = pe_bullet_gun(mzx_world, param);
      break;
    }

    case BEAR:
    {
      param = pe_bear(mzx_world, param);
      break;
    }

    case BEAR_CUB:
    {
      param = pe_bear_cub(mzx_world, param);
      break;
    }

    case MISSILE_GUN:
    {
      param = pe_missile_gun(mzx_world, param);
      break;
    }
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  // Return param
  return param;
}

int edit_sensor(struct world *mzx_world, struct sensor *cur_sensor)
{
  char sensor_name[ROBOT_NAME_SIZE];
  char sensor_robot[ROBOT_NAME_SIZE];
  int sensor_char = cur_sensor->sensor_char;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_SENSOR_EDITOR);
  strcpy(sensor_name, cur_sensor->sensor_name);
  strcpy(sensor_robot, cur_sensor->robot_to_mesg);

  set_confirm_buttons(elements);
  elements[2] = construct_input_box(15, 6, "Sensor's name:    ",
   ROBOT_NAME_SIZE - 1, sensor_name);
  elements[3] = construct_input_box(15, 8, "Robot to message: ",
   ROBOT_NAME_SIZE - 1, sensor_robot);
  elements[4] = construct_char_box(15, 10, "Sensor character: ",
   1, &sensor_char);

  construct_dialog(&di, "Set Sensor", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  pop_context();

  if(dialog_result)
    return -1;

  strcpy(cur_sensor->sensor_name, sensor_name);
  strcpy(cur_sensor->robot_to_mesg, sensor_robot);
  cur_sensor->sensor_char = sensor_char;
  return 0;
}

int edit_scroll(struct world *mzx_world, struct scroll *cur_scroll)
{
  scroll_edit(mzx_world, cur_scroll, 2);
  return 0;
}

void edit_robot(context *ctx, struct robot *cur_robot, int *ret_value)
{
  struct world *mzx_world = ctx->world;
  char *name = cur_robot->robot_name;
  int new_char;

  // Edit name.
  if(!input_window(mzx_world, "Name for robot:", name, ROBOT_NAME_SIZE - 1))
  {
    // Edit character.
    new_char = char_selection(cur_robot->robot_char);
    if(new_char < 0)
    {
      // ESC means keep char but don't edit robot
      if(new_char == -256)
        new_char = 0;
      cur_robot->robot_char = -new_char;
    }
    else
    {
      cur_robot->robot_char = new_char;
      // Now edit the program.
      robot_editor(ctx, cur_robot);
    }

    if(ret_value)
      *ret_value = 0;

    return;
  }

  if(ret_value)
    *ret_value = -1;
}

void edit_global_robot(context *ctx)
{
  struct world *mzx_world = ctx->world;
  struct robot *cur_robot = &(mzx_world->global_robot);
  char *name = cur_robot->robot_name;

  if(!input_window(mzx_world, "Name for robot:", name, ROBOT_NAME_SIZE - 1))
    robot_editor(ctx, cur_robot);
}
