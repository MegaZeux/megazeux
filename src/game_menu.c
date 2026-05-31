/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "about.h"
#include "configure.h"
#include "const.h"
#include "core.h"
#include "counter.h"
#include "game_menu.h"
#include "game_player.h"
#include "graphics.h"
#include "settings.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "world_struct.h"
#include "event/event.h"

#include <string.h>

#define MENU_COL            0x19
#define MENU_COL_DARK       0x10
#define MENU_COL_CORNER     0x18
#define MENU_COL_TITLE      0x1E
#define MENU_COL_TEXT       0x1F
#define MENU_COL_NO_SELECT  0x19
#define MENU_COL_SELECTED   0xFC
#define MENU_COL_COUNTER    0x1B

#define GAME_MENU_X         8
#define GAME_MENU_Y         4
#define GAME_MENU_WIDTH     29

#define GAME_STATUS_X       (GAME_MENU_X + GAME_MENU_WIDTH + 1)
#define GAME_STATUS_Y       GAME_MENU_Y
#define GAME_STATUS_WIDTH   30
#define GAME_STATUS_HEIGHT  18
#define GAME_STATUS_INDENT  16

#define MAIN_MENU_X         28
#define MAIN_MENU_Y         4
#define MAIN_MENU_WIDTH     24

#define KEY_CHAR            '\x0C'

static const char main_menu_title[] = "MegaZeux " VERSION;

enum main_menu_opts
{
  M_NONE,
  M_RETURN,
  M_EXIT,
  M_HELP,
  M_SETTINGS,
  M_LOAD_WORLD,
  M_RESTORE_SAVE,
  M_PLAY_WORLD,
  M_UPDATER,
  M_NEW_WORLD,
  M_EDIT_WORLD,
  M_QUICKLOAD,
  M_ABOUT,
  M_MAX_OPTS
};

const char *main_menu_labels[] =
{
  [M_RETURN]        = "Enter- Close menu",
  [M_EXIT]          = "Esc-   Exit MegaZeux",
  [M_HELP]          = "F1/H - Help",
  [M_SETTINGS]      = "F2/S - Settings",
  [M_LOAD_WORLD]    = "F3/L - Load world",
  [M_RESTORE_SAVE]  = "F4/R - Restore game",
  [M_PLAY_WORLD]    = "F5/P - Play world",
  [M_UPDATER]       = "F7/U - Updater",
  [M_NEW_WORLD]     = "F8/N - New world",
  [M_EDIT_WORLD]    = "F9/E - Edit world",
  [M_QUICKLOAD]     = "F10  - Quickload",
  [M_ABOUT]         = "-= About MegaZeux =-",
};

// Keys to pass to the parent key handler, or 0 if none.
const enum keycode main_menu_keys[] =
{
  [M_RETURN]        = 0,
  [M_EXIT]          = IKEY_ESCAPE,
  [M_HELP]          = IKEY_F1,
  [M_SETTINGS]      = IKEY_F2,
  [M_LOAD_WORLD]    = IKEY_F3,
  [M_RESTORE_SAVE]  = IKEY_F4,
  [M_PLAY_WORLD]    = IKEY_F5,
  [M_UPDATER]       = IKEY_F7,
  [M_NEW_WORLD]     = IKEY_F8,
  [M_EDIT_WORLD]    = IKEY_F9,
  [M_QUICKLOAD]     = IKEY_F10,
  [M_ABOUT]         = 0,
};

static const char game_menu_title[] = "Game Menu";

enum game_menu_opts
{
  G_NONE,
  G_RETURN,
  G_EXIT,
  G_HELP,
  G_SETTINGS,
  G_SAVE,
  G_LOAD,
  G_TOGGLE_BOMBS,
  G_QUICKSAVE,
  G_QUICKLOAD,
  G_MOVE,
  G_SHOOT,
  G_BOMB,
  G_BLANK,
  G_DEBUG_WINDOW,
  G_DEBUG_VARIABLES,
  G_DEBUG_ROBOTS,
  G_MAX_OPTS
};

const char *game_menu_labels[] =
{
  [G_RETURN]          = "Enter  - Return to game",
  [G_EXIT]            = "Esc    - Exit game",
  [G_HELP]            = "F1     - Help",
  [G_SETTINGS]        = "F2     - Settings",
  [G_SAVE]            = "F3     - Save",
  [G_LOAD]            = "F4     - Load",
  [G_TOGGLE_BOMBS]    = "F5/Ins - Toggle bomb type",
  [G_QUICKSAVE]       = "F9     - Quicksave",
  [G_QUICKLOAD]       = "F10    - Quickload",
  [G_MOVE]            = "Arrows - Move",
  [G_SHOOT]           = "Space  - Shoot",
  [G_BOMB]            = "Delete - Bomb",
  [G_BLANK]           = "",
  [G_DEBUG_WINDOW]    = "F6     - Debug Window",
  [G_DEBUG_VARIABLES] = "F11    - Debug Variables",
  [G_DEBUG_ROBOTS]    = "Alt+F11- Debug Robots",
};

// Keys to pass to the parent key handler, or 0 if none.
const enum keycode game_menu_keys[] =
{
  [G_RETURN]          = 0,
  [G_EXIT]            = IKEY_ESCAPE,
  [G_HELP]            = IKEY_F1,
  [G_SETTINGS]        = IKEY_F2,
  [G_SAVE]            = IKEY_F3,
  [G_LOAD]            = IKEY_F4,
  [G_TOGGLE_BOMBS]    = IKEY_F5,
  [G_QUICKSAVE]       = IKEY_F9,
  [G_QUICKLOAD]       = IKEY_F10,
  [G_MOVE]            = 0,
  [G_SHOOT]           = 0,
  [G_BOMB]            = 0,
  [G_BLANK]           = 0,
  [G_DEBUG_WINDOW]    = IKEY_F6,
  [G_DEBUG_VARIABLES] = IKEY_F11,
  [G_DEBUG_ROBOTS]    = IKEY_F11,
};

static enum main_menu_opts main_menu_last_selected = M_NONE;
static enum game_menu_opts game_menu_last_selected = G_NONE;

struct menu_opt
{
  const char *label;
  enum keycode return_key;
  int which;
  boolean is_selectable;
};

struct game_menu_context
{
  context ctx;
  boolean *return_alt;
  enum keycode *return_key;
  const char *title;
  struct menu_opt options[MAX((int)M_MAX_OPTS, (int)G_MAX_OPTS)];
  int num_options;
  int x;
  int y;
  int width;
  int height;
  int current_option;
  int last_opened_opt_type;
  boolean is_game_menu;
  boolean show_status;
};

/**
 * Is the exit menu currently allowed?
 */
boolean allow_exit_menu(struct world *mzx_world, boolean is_titlescreen)
{
  // ESCAPE_MENU (2.90+) only works on the titlescreen if standalone_mode is
  // enabled.
  if(is_titlescreen && !get_config()->standalone_mode)
    return true;

  if(mzx_world->version < V290 || get_counter(mzx_world, "ESCAPE_MENU", 0))
    return true;

  return false;
}

/**
 * Are the main or game menus currently allowed?
 */
boolean allow_enter_menu(struct world *mzx_world, boolean is_titlescreen)
{
  // ENTER_MENU only works on the titlescreen if standalone_mode is enabled.
  if(is_titlescreen && !get_config()->standalone_mode)
    return true;

  if(mzx_world->version < V260 || get_counter(mzx_world, "ENTER_MENU", 0))
    return true;

  return false;
}

/**
 * Is the help system currently allowed by the running world?
 * Additional restrictions also apply to when this can open in the main loop.
 */
boolean allow_help_system(struct world *mzx_world, boolean is_titlescreen)
{
  // If the help file doesn't exist, don't allow this...
  if(!mzx_world->help_file)
    return false;

  // HELP_MENU only works on the titlescreen if standalone_mode is enabled.
  if(is_titlescreen && !get_config()->standalone_mode)
    return true;

  if(mzx_world->version < V260 || get_counter(mzx_world, "HELP_MENU", 0))
    return true;

  return false;
}

/**
 * Is the settings menu currently allowed?
 * Additional restrictions also apply to when this can open in the main loop.
 */
boolean allow_settings_menu(struct world *mzx_world, boolean is_titlescreen,
 boolean is_override)
{
  // F2_MENU only works on the titlescreen if standalone_mode is enabled.
  // Also, if certain modifiers are pressed the F2_MENU counter is ignored
  // altogether outside of standalone_mode.
  if((is_override || is_titlescreen) && !get_config()->standalone_mode)
    return true;

  if(mzx_world->version < V260 || get_counter(mzx_world, "F2_MENU", 0))
    return true;

  return false;
}

/**
 * Is the load world menu on the titlescreen currently allowed?
 */
boolean allow_load_world_menu(struct world *mzx_world)
{
  if(!get_config()->standalone_mode)
    return true;

  return false;
}

/**
 * Are the save menu and quicksave currently allowed?
 */
boolean allow_save_menu(struct world *mzx_world)
{
  // The save enabled settings aren't actually available on the titlescreen...
  if(!mzx_world->dead && player_can_save(mzx_world))
    return true;

  return false;
}

/**
 * Are the load menu and quickload currently allowed?
 */
boolean allow_load_menu(struct world *mzx_world, boolean is_titlescreen)
{
  // LOAD_MENU only works on the titlescreen if standalone_mode is enabled.
  if(is_titlescreen && !get_config()->standalone_mode)
    return true;

  if(mzx_world->version < V282 || get_counter(mzx_world, "LOAD_MENU", 0))
    return true;

  return false;
}

/**
 * Is player movement currently allowed?
 * This only affects the shortcut display for movement.
 */
static boolean allow_movement(struct world *mzx_world)
{
  struct board *cur_board = mzx_world->current_board;

  if(mzx_world->active && !mzx_world->dead && cur_board &&
   !(cur_board->player_ns_locked && cur_board->player_ew_locked))
    return true;

  return false;
}

/**
 * Is attacking and bomb switching allowed from the menu allowed?
 * This only affects the shortcut display for attacking and bomb switching
 * from the game menu.
 */
static boolean allow_attacking(struct world *mzx_world)
{
  if(mzx_world->active && !mzx_world->dead && mzx_world->current_board &&
   !(mzx_world->current_board->player_attack_locked))
    return true;

  return false;
}

/**
 * Are the debug menus currently allowed?
 */
boolean allow_debug_menu(struct world *mzx_world)
{
  // The only thing that can enable these is the editing flag.
  // Note these also check to see if their hooks are available individually.
  if(mzx_world->editing)
    return true;

  return false;
}

static void set_main_menu_opt(struct menu_opt *opt, enum main_menu_opts which)
{
  opt->label = main_menu_labels[which];
  opt->return_key = main_menu_keys[which];
  opt->which = which;
  opt->is_selectable = true;
}

/**
 * Build the main menu. The menu passed here should be at least M_MAX_OPTS long.
 */
static void construct_main_menu(struct world *mzx_world, struct menu_opt *menu,
 int *_menu_length)
{
  boolean show_load_options;
  int menu_length = 0;

  set_main_menu_opt(menu++, M_RETURN);
  menu_length++;

  set_main_menu_opt(menu++, M_EXIT);
  menu_length++;

  if(allow_help_system(mzx_world, true))
  {
    set_main_menu_opt(menu++, M_HELP);
    menu_length++;
  }

  if(allow_settings_menu(mzx_world, true, true))
  {
    set_main_menu_opt(menu++, M_SETTINGS);
    menu_length++;
  }

  if(allow_load_world_menu(mzx_world))
  {
    set_main_menu_opt(menu++, M_LOAD_WORLD);
    menu_length++;
  }

  show_load_options = allow_load_menu(mzx_world, true);
  if(show_load_options)
  {
    set_main_menu_opt(menu++, M_RESTORE_SAVE);
    menu_length++;
  }

  set_main_menu_opt(menu++, M_PLAY_WORLD);
  menu_length++;

  if(check_for_updates)
  {
    set_main_menu_opt(menu++, M_UPDATER);
    menu_length++;
  }

  if(edit_world)
  {
    set_main_menu_opt(menu++, M_NEW_WORLD);
    set_main_menu_opt(menu++, M_EDIT_WORLD);
    menu_length += 2;
  }

  if(show_load_options)
  {
    set_main_menu_opt(menu++, M_QUICKLOAD);
    menu_length++;
  }

  set_main_menu_opt(menu++, M_ABOUT);
  menu_length++;

  *_menu_length = menu_length;
}

static void set_game_menu_opt(struct menu_opt *opt, enum game_menu_opts which,
 boolean is_selectable)
{
  opt->label = game_menu_labels[which];
  opt->return_key = game_menu_keys[which];
  opt->which = which;
  opt->is_selectable = is_selectable;
}

/**
 * Build the game menu. The menu passed here should be at least G_MAX_OPTS long.
 */
static void construct_game_menu(struct world *mzx_world, struct menu_opt *menu,
 int *_menu_length)
{
  boolean show_save_options = allow_save_menu(mzx_world);
  boolean show_load_options = allow_load_menu(mzx_world, false);
  int menu_length = 0;

  set_game_menu_opt(menu++, G_RETURN, true);
  menu_length++;

  set_game_menu_opt(menu++, G_EXIT, true);
  menu_length++;

  if(allow_help_system(mzx_world, false))
  {
    set_game_menu_opt(menu++, G_HELP, true);
    menu_length++;
  }

  if(allow_settings_menu(mzx_world, false, true))
  {
    set_game_menu_opt(menu++, G_SETTINGS, true);
    menu_length++;
  }

  if(show_save_options)
  {
    set_game_menu_opt(menu++, G_SAVE, true);
    menu_length++;
  }

  if(show_load_options)
  {
    set_game_menu_opt(menu++, G_LOAD, true);
    menu_length++;
  }

  if(allow_attacking(mzx_world))
  {
    set_game_menu_opt(menu++, G_TOGGLE_BOMBS, true);
    menu_length++;
  }

  if(show_save_options)
  {
    set_game_menu_opt(menu++, G_QUICKSAVE, true);
    menu_length++;
  }

  if(show_load_options)
  {
    set_game_menu_opt(menu++, G_QUICKLOAD, true);
    menu_length++;
  }

  if(allow_movement(mzx_world))
  {
    set_game_menu_opt(menu++, G_MOVE, false);
    menu_length++;
  }

  if(allow_attacking(mzx_world))
  {
    set_game_menu_opt(menu++, G_SHOOT, false);
    set_game_menu_opt(menu++, G_BOMB, false);
    menu_length += 2;
  }

  if(allow_debug_menu(mzx_world))
  {
    set_game_menu_opt(menu++, G_BLANK, false);
    set_game_menu_opt(menu++, G_DEBUG_WINDOW, true);
    set_game_menu_opt(menu++, G_DEBUG_VARIABLES, true);
    set_game_menu_opt(menu++, G_DEBUG_ROBOTS, true);
    menu_length += 4;
  }

  *_menu_length = menu_length;
}

/**
 * Draw a menu border and title to the screen.
 */
static void draw_menu_box(int x, int y, int width, int height,
 const char *title)
{
  draw_window_box(x, y, width + x - 1, height + y - 1,
   MENU_COL, MENU_COL_DARK, MENU_COL_CORNER, true, true);

  if(title)
  {
    int title_width = strlen(title);
    int title_x = (width - title_width)/2 + x;

    write_string(title, title_x, y, MENU_COL_TITLE, WR_NONE);
    draw_char(' ', MENU_COL_TITLE, title_x - 1, y);
    draw_char(' ', MENU_COL_TITLE, title_x + title_width, y);
  }
}

/**
 * Write a single counter to the status screen.
 */

static void show_counter(struct world *mzx_world, const char *counter_name,
 int x, int y, boolean skip_if_zero)
{
  int counter_value = get_counter(mzx_world, counter_name, 0);

  if((skip_if_zero) && (!counter_value))
    return;

  write_string(counter_name, x, y, MENU_COL_COUNTER, false);
  write_number(counter_value, MENU_COL_TEXT, x + GAME_STATUS_INDENT, y,
   1, false, 10);
}

/**
 * Draw the game status window. This window displays information to the player
 * such as built-in counters, keys, custom status counters, and active effects.
 */
static void draw_game_status(struct world *mzx_world)
{
  char *keys = mzx_world->keys;
  int x;
  int y;
  int i;

  draw_menu_box(
   GAME_STATUS_X, GAME_STATUS_Y, GAME_STATUS_WIDTH, GAME_STATUS_HEIGHT,
   NULL
  );

  x = GAME_STATUS_X + 2;
  y = GAME_STATUS_Y + 1;

  show_counter(mzx_world, "Gems",     x, y++, false);
  show_counter(mzx_world, "Ammo",     x, y++, false);
  show_counter(mzx_world, "Health",   x, y++, false);
  show_counter(mzx_world, "Lives",    x, y++, false);
  show_counter(mzx_world, "Lobombs",  x, y++, false);
  show_counter(mzx_world, "Hibombs",  x, y++, false);
  show_counter(mzx_world, "Coins",    x, y++, false);
  show_counter(mzx_world, "Score",    x, y++, false);

  // Bomb selection
  write_string("(cur.)", (x + 9), y - (mzx_world->bomb_type ? 3 : 4),
   MENU_COL, false);

  // Keys
  write_string("Keys", x, y, MENU_COL_COUNTER, false);

  for(i = 0; i < 8; i++)
  {
    if(keys[i] != NO_KEY)
      draw_char(KEY_CHAR, 0x10 + keys[i], (x + i + GAME_STATUS_INDENT), y);

    if(keys[i+8] != NO_KEY)
      draw_char(KEY_CHAR, 0x10 + keys[i+8], (x + i + GAME_STATUS_INDENT), y+1);
  }

  y += 2;

  // Custom status counters
  for(i = 0; i < NUM_STATUS_COUNTERS; i++)
  {
    char *name = mzx_world->status_counters_shown[i];
    if(name[0])
      show_counter(mzx_world, name, x, y, true);

    y++;
  }

  // Active effects
  y = GAME_STATUS_Y + GAME_STATUS_HEIGHT - 1;

  if(mzx_world->firewalker_dur > 0)
    write_string("-W-", x+4, y, 0x1C, false);

  if(mzx_world->freeze_time_dur > 0)
    write_string("-F-", x+8, y, 0x1B, false);

  if(mzx_world->slow_time_dur > 0)
    write_string("-S-", x+12, y, 0x1E, false);

  if(mzx_world->wind_dur > 0)
    write_string("-W-", x+16, y, 0x1F, false);

  if(mzx_world->blind_dur > 0)
    write_string("-B-", x+20, y, 0x19, false);
}

/**
 * Draw the main or game menu.
 */
static boolean menu_draw(context *ctx)
{
  struct game_menu_context *game_menu = (struct game_menu_context *)ctx;
  unsigned int color;
  int x;
  int y;
  int i;

  draw_menu_box(
    game_menu->x, game_menu->y, game_menu->width, game_menu->height,
    game_menu->title
  );

  x = game_menu->x + 2;
  y = game_menu->y + 1;

  for(i = 0; i < game_menu->num_options; i++)
  {
    color = MENU_COL_TEXT;
    if(game_menu->current_option >= 0)
      if(!game_menu->options[i].is_selectable)
        color = MENU_COL_NO_SELECT;

    write_string(game_menu->options[i].label, x, y, color, false);
    y++;
  }

  // Selected option
  i = game_menu->current_option;
  if(i >= 0 && i < game_menu->num_options)
  {
    x = game_menu->x + 1;
    y = + game_menu->y + 1 + i;
    color_line(game_menu->width - 2, x, y, MENU_COL_SELECTED);
    cursor_hint(x + 1, y);
    //write_string(game_menu->options[i].label, x, y, MENU_COL_SELECTED, false);

    x = game_menu->x + 1;
    //draw_char(' ', MENU_COL_SELECTED, x, y);

    x = game_menu->width + game_menu->x - 1;
    //draw_char(' ', MENU_COL_SELECTED, x, y);
  }

  if(game_menu->show_status)
    draw_game_status(ctx->world);

  return true;
}

static void focus_status(void)
{
  int fx = (GAME_STATUS_WIDTH / 2) + GAME_STATUS_X;
  int fy = (GAME_STATUS_HEIGHT / 2) + GAME_STATUS_Y;
  focus_screen(fx, fy);
}

static void focus_menu(struct game_menu_context *game_menu)
{
  int fx = (game_menu->width / 2) + game_menu->x;
  int fy = (game_menu->height / 2) + game_menu->y;

  if(game_menu->current_option >= 0)
  {
    fy += game_menu->current_option - (game_menu->num_options / 2);
  }
  else

  if(game_menu->show_status)
  {
    focus_status();
    return;
  }

  focus_screen(fx, fy);
}

/**
 * Find the last selected option in the menu, if it exists. Otherwise, this
 * selects the first option.
 */
static int find_last_option(struct game_menu_context *game_menu, int which)
{
  struct menu_opt *current;
  int i;

  for(i = 0; i < game_menu->num_options; i++)
  {
    current = &(game_menu->options[i]);

    if(current->is_selectable && current->which == which)
      return i;
  }
  return 0;
}

/**
 * Select the previous menu option.
 */
static void menu_select_prev(struct game_menu_context *game_menu)
{
  int i = 0;

  if(game_menu->current_option >= 0)
  {
    int current = game_menu->current_option;
    i = current;

    do
    {
      i--;
      if(i < 0)
        i = game_menu->num_options - 1;

      if(game_menu->options[i].is_selectable)
        break;
    }
    while(i != current);

    game_menu->current_option = i;
  }
  else
    game_menu->current_option =
     find_last_option(game_menu, game_menu->last_opened_opt_type);

  focus_menu(game_menu);
}

/**
 * Select the next menu option.
 */
static void menu_select_next(struct game_menu_context *game_menu)
{
  int i = 0;

  if(game_menu->current_option >= 0)
  {
    int current = game_menu->current_option;
    i = current;

    do
    {
      i++;
      if(i >= game_menu->num_options)
        i = 0;

      if(game_menu->options[i].is_selectable)
        break;
    }
    while(i != current);

    game_menu->current_option = i;
  }
  else
    game_menu->current_option =
     find_last_option(game_menu, game_menu->last_opened_opt_type);

  focus_menu(game_menu);
}

/**
 * Set the return key for the parent.
 */
static void set_return_key(struct game_menu_context *game_menu,
 enum keycode return_key, boolean return_alt)
{
  if(game_menu->return_alt)
    *(game_menu->return_alt) = return_alt;

  if(game_menu->return_key)
    *(game_menu->return_key) = return_key;
}

/**
 * Activate the main menu. Return true if the menu should exit.
 */
static boolean main_menu_activate(struct game_menu_context *game_menu, int *key)
{
  int current = game_menu->current_option;
  enum main_menu_opts which;
  enum keycode return_key;

  if(current < 0 || current >= game_menu->num_options)
    return false;

  which = (enum main_menu_opts)game_menu->options[current].which;
  return_key = game_menu->options[current].return_key;

  switch(which)
  {
    case M_HELP:
    case M_SETTINGS:
    {
      // These go to the main loop instead.
      *key = return_key;
      return false;
    }

    case M_ABOUT:
    {
      // Special: display "About MegaZeux".
      about_megazeux(&game_menu->ctx);
      return true;
    }

    case M_EXIT:
    {
      // Special: if the exit menu isn't available, set the exit status.
      if(!allow_exit_menu(game_menu->ctx.world, false))
        set_exit_status(true);
    }

    /* fall-through */

    default:
    {
      set_return_key(game_menu, return_key, false);
      return true;
    }
  }
}

/**
 * Activate the game menu. Return true if the menu should exit.
 */
static boolean game_menu_activate(struct game_menu_context *game_menu, int *key)
{
  struct world *mzx_world = game_menu->ctx.world;
  int current = game_menu->current_option;
  enum game_menu_opts which;
  enum keycode return_key;
  boolean return_alt = false;

  if(current < 0 || current >= game_menu->num_options)
    return false;

  which = (enum game_menu_opts)game_menu->options[current].which;
  return_key = game_menu->options[current].return_key;

  switch(which)
  {
    case G_HELP:
    {
      // This always go to the main loop instead.
      *key = return_key;
      return false;
    }

    case G_SETTINGS:
    {
      // TODO: right now this can't really tell the main loop to force an
      // override so whatever, just open it right here
      game_settings(mzx_world);
      return false;
    }

    case G_EXIT:
    {
      if(!allow_exit_menu(mzx_world, false))
        set_exit_status(true);

      set_return_key(game_menu, return_key, return_alt);
      return true;
    }

    case G_TOGGLE_BOMBS:
    {
      // Pretty much no point in this one closing the menu...
      if(!mzx_world->dead)
        player_switch_bomb_type(mzx_world);

      return false;
    }

    case G_DEBUG_ROBOTS:
    {
      return_alt = true;
    }

    /* fall-through */

    default:
    {
      set_return_key(game_menu, return_key, return_alt);
      return true;
    }
  }
}

/**
 * Activate the current menu. Return true if the menu should exit.
 */
static boolean menu_activate(struct game_menu_context *game_menu, int *key)
{
  if(game_menu->is_game_menu)
    return game_menu_activate(game_menu, key);
  else
    return main_menu_activate(game_menu, key);
}

/**
 * Joystick input for the game menu. Use the default UI joystick mapping except
 * with an exception: JOY_LSHOULDER should open F2 since it will be available
 * more often than JOY_RSHOULDER on consoles.
 */
static boolean menu_joystick(context *ctx, int *key, int action)
{
  int default_key = get_joystick_ui_key();

  switch(action)
  {
    case JOY_START:
    case JOY_X:
    case JOY_Y:
    {
      // Since start opens the menu, it should also close it instead of
      // selecting things. Also let X and Y close the menu, just because.
      *key = IKEY_ESCAPE;
      return true;
    }

    case JOY_LSHOULDER:
    {
      *key = IKEY_F2;
      return true;
    }
  }

  if(default_key)
  {
    *key = default_key;
    return true;
  }
  return false;
}

/**
 * Close the menu on an exit event or an return/escape press.
 * NOTE: In DOS versions of MZX, clicking the menus would close the
 * menu and trigger the clicked function. This feature was removed in
 * the port and has not yet been restored.
 */
static boolean menu_key(context *ctx, int *key)
{
  struct game_menu_context *game_menu = (struct game_menu_context *)ctx;
  struct world *mzx_world = ctx->world;
  boolean activate_option = false;
  boolean exit_menu = false;

  if(get_exit_status())
    *key = IKEY_ESCAPE;

  switch(*key)
  {
    case IKEY_UP:
    case IKEY_LEFT:
    {
      // Previous option
      menu_select_prev(game_menu);
      return true;
    }

    case IKEY_DOWN:
    case IKEY_RIGHT:
    {
      // Next option
      menu_select_next(game_menu);
      return true;
    }

    case IKEY_HOME:
    case IKEY_PAGEUP:
    {
      // First option
      game_menu->current_option = 0;
      return true;
    }

    case IKEY_END:
    case IKEY_PAGEDOWN:
    {
      // Last option
      game_menu->current_option = 0;
      menu_select_prev(game_menu);
      return true;
    }

    case IKEY_SPACE:
    {
      // Activate the current option if there is a current option, otherwise
      // do nothing.
      if(game_menu->current_option >= 0)
        activate_option = true;

      break;
    }

    case IKEY_RETURN:
    {
      // Activate the current option if there is a current option, otherwise
      // exit the menu.
      if(game_menu->current_option >= 0)
      {
        activate_option = true;
      }
      else
        exit_menu = true;

      break;
    }

    case IKEY_ESCAPE:
    {
      // Exit the menu.
      exit_menu = true;
      break;
    }

    case IKEY_INSERT:
    case IKEY_F5:
    {
      if(game_menu->is_game_menu)
      {
        // Pretty much no point in closing the menu for this one...
        if(allow_attacking(mzx_world))
          player_switch_bomb_type(mzx_world);

        return true;
      }
    }

    /* fall-through */

    case IKEY_F3:
    case IKEY_F4:
    case IKEY_F6:
    case IKEY_F7:
    case IKEY_F8:
    case IKEY_F9:
    case IKEY_F10:
    case IKEY_F11:
    {
      // Return the pressed key...
      set_return_key(game_menu, *key, get_alt_status(keycode_internal));
      exit_menu = true;
      break;
    }
  }

  if(activate_option)
  {
    if(menu_activate(game_menu, key))
    {
      destroy_context(ctx);
      return true;
    }
  }

  if(exit_menu)
  {
    destroy_context(ctx);
    return true;
  }
  return false;
}

/**
 * Handle menu clicks.
 */
static boolean menu_click(context *ctx, int *key, int button, int x, int y)
{
  struct game_menu_context *game_menu = (struct game_menu_context *)ctx;

  if((x >= game_menu->x + 1) && (x < game_menu->x + game_menu->width - 1) &&
   (y >= game_menu->y + 1) && (y < game_menu->y + game_menu->height - 1))
  {
    int selected = y - game_menu->y - 1;

    if(selected < game_menu->num_options &&
     game_menu->options[selected].is_selectable)
    {
      if(selected == game_menu->current_option)
      {
        if(menu_activate(game_menu, key))
          destroy_context(ctx);
      }
      else
      {
        game_menu->current_option = selected;
        focus_menu(game_menu);
      }
    }
    return true;
  }
  return false;
}

/**
 * Block mouse drag activation...
 */
static boolean menu_drag(context *ctx, int *key, int button, int x, int y)
{
  return false;
}

/**
 * Restore the screen when the menu is closed and save the last option.
 */
static void menu_destroy(context *ctx)
{
  struct game_menu_context *game_menu = (struct game_menu_context *)ctx;
  int current = game_menu->current_option;
  int which;

  if(current >= 0)
  {
    which = game_menu->options[current].which;

    if(game_menu->is_game_menu)
      game_menu_last_selected = (enum game_menu_opts)which;
    else
      main_menu_last_selected = (enum main_menu_opts)which;
  }
  cursor_off();
  restore_screen();
}

/**
 * Create and run the game menu context for gameplay. This menu also displays
 * a status screen with the player's current stats.
 */
void game_menu(context *parent, boolean start_selected, enum keycode *retval,
 boolean *retval_alt)
{
  struct game_menu_context *game_menu =
   cmalloc(sizeof(struct game_menu_context));
  struct context_spec spec;

  save_screen();
  m_show();

  construct_game_menu(parent->world,
   game_menu->options, &game_menu->num_options);

  game_menu->title = game_menu_title;
  game_menu->x = GAME_MENU_X;
  game_menu->y = GAME_MENU_Y;
  game_menu->width = GAME_MENU_WIDTH;
  game_menu->height = game_menu->num_options + 2;
  game_menu->current_option = -1;
  game_menu->last_opened_opt_type = game_menu_last_selected;
  game_menu->is_game_menu = true;
  game_menu->show_status = allow_enter_menu(parent->world, false);
  game_menu->return_key = retval;
  game_menu->return_alt = retval_alt;

  if(start_selected)
  {
    game_menu->current_option =
     find_last_option(game_menu, game_menu_last_selected);
  }

  if(!game_menu->show_status)
  {
    // Center menu...
    game_menu->x = (SCREEN_W - GAME_MENU_WIDTH) / 2;
  }

  focus_menu(game_menu);
  if(retval)
    *retval = 0;
  if(retval_alt)
    *retval_alt = false;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = menu_draw;
  spec.key      = menu_key;
  spec.joystick = menu_joystick;
  spec.click    = menu_click;
  spec.drag     = menu_drag;
  spec.destroy  = menu_destroy;

  create_context((context *)game_menu, parent, &spec, CTX_GAME_MENU);
}

/**
 * Create and run the main menu context for the title screen.
 */
void main_menu(context *parent, boolean start_selected, enum keycode *retval)
{
  struct game_menu_context *game_menu =
   cmalloc(sizeof(struct game_menu_context));
  struct context_spec spec;

  save_screen();
  m_show();

  construct_main_menu(parent->world,
   game_menu->options, &game_menu->num_options);

  game_menu->title = main_menu_title;
  game_menu->x = MAIN_MENU_X;
  game_menu->y = MAIN_MENU_Y;
  game_menu->width = MAIN_MENU_WIDTH;
  game_menu->height = game_menu->num_options + 2;
  game_menu->current_option = -1;
  game_menu->last_opened_opt_type = main_menu_last_selected;
  game_menu->is_game_menu = false;
  game_menu->show_status = false;
  game_menu->return_key = retval;
  game_menu->return_alt = NULL;

  if(start_selected)
  {
    game_menu->current_option =
     find_last_option(game_menu, main_menu_last_selected);
  }

  focus_menu(game_menu);
  if(retval)
    *retval = 0;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = menu_draw;
  spec.key      = menu_key;
  spec.joystick = menu_joystick;
  spec.click    = menu_click;
  spec.drag     = menu_drag;
  spec.destroy  = menu_destroy;

  create_context((context *)game_menu, parent, &spec, CTX_MAIN_MENU);
}
