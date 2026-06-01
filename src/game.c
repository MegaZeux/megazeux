/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

// Main title screen/gaming code

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "caption.h"
#include "configure.h"
#include "const.h"
#include "core.h"
#include "data.h"
#include "error.h"
#include "game.h"
#include "game_menu.h"
#include "game_player.h"
#include "game_update.h"
#include "graphics.h"
#include "robot.h"
#include "sfx.h"
#include "window.h"
#include "world.h"
#include "world_struct.h"
#include "io/fsafeopen.h"
#include "io/vio.h"

#include "audio/audio.h"
#include "event/event.h"

static const char *const world_ext[] = { ".MZX", NULL };
static const char *const save_ext[] = { ".SAV", NULL };

struct game_context
{
  context ctx;
  boolean fade_in;
  boolean need_reload;
  boolean load_dialog_on_failed_load;
  boolean is_title;
  boolean allow_cheats;

  // Menu return values.
  enum keycode menu_key;
  boolean menu_alt;
};

// As nice as this would be to put in the title context, it's annoying
// to get the context to clear_intro_mesg right now.
static unsigned int intro_mesg_timer = MESG_TIMEOUT;

/**
 * Activate the title screen intro message.
 */

static void enable_intro_mesg(void)
{
  intro_mesg_timer = MESG_TIMEOUT;
}

/**
 * Disable the title screen intro message.
 */

void clear_intro_mesg(void)
{
  intro_mesg_timer = 0;
}

/**
 * Draw the title screen intro message.
 */

void draw_intro_mesg(struct world *mzx_world)
{
  static const char mesg1[] = "F1: Help   ";
  static const char mesg2[] = "Enter: Menu   Ctrl-Alt-Enter: Fullscreen";

  if(intro_mesg_timer == 0)
    return;

  intro_mesg_timer--;

  if(mzx_world->help_file)
  {
    write_string(mesg1, 14, 24, scroll_color, 0);
    write_string(mesg2, 25, 24, scroll_color, 0);
  }
  else
  {
    write_string(mesg2, 20, 24, scroll_color, 0);
  }
}

/**
 * Load a module for gameplay.
 */

boolean load_game_module(struct world *mzx_world, char *filename,
 boolean fail_if_same)
{
  struct board *cur_board = mzx_world->current_board;
  char translated_name[MAX_PATH];
  size_t mod_name_size = strlen(filename);
  boolean mod_star = false;
  int n_result;

  // Special case: mod "" ends the module.
  if(!filename[0])
  {
    mzx_world->real_mod_playing[0] = 0;
    audio_end_module();
    return false;
  }

  // Do nothing.
  if(!strcmp(filename, "*"))
    return false;

  // Temporarily get rid of the star.
  if(mod_name_size && filename[mod_name_size - 1] == '*')
  {
    filename[mod_name_size - 1] = 0;
    mod_star = true;
  }

  // Get the translated name (the one we want to compare against later)
  n_result = fsafetranslate(filename, translated_name, MAX_PATH);
  if(n_result != FSAFE_SUCCESS)
    n_result = audio_legacy_translate(filename, translated_name, MAX_PATH);

  // Add * back
  if(mod_star)
    filename[mod_name_size - 1] = '*';

  if(n_result == FSAFE_SUCCESS)
  {
    // In certain situations, play the mod only if the names are different.
    if((mod_star || fail_if_same) &&
     !strcasecmp(translated_name, mzx_world->real_mod_playing))
      return false;

    // If the mod actually changes, update real mod playing.
    // The safety check has already been done, so don't do it again ('false').
    if(audio_play_module(translated_name, false, cur_board->volume))
    {
      strcpy(mzx_world->real_mod_playing, translated_name);
      return true;
    }
  }
  return false;
}

/**
 * Load the current board's module, even if it's already playing.
 */

void load_board_module(struct world *mzx_world)
{
  load_game_module(mzx_world, mzx_world->current_board->mod_playing, false);
}

/**
 * Fade out before the world load and clear the screen. The Emscripten port and
 * anything using the meter without a protected palette need special handling.
 */
static inline void load_vquick_fadeout_and_clear(void)
{
#ifdef __EMSCRIPTEN__
  // HACK: avoid putting load_world_*/load_savegame on the asyncify whitelist
  insta_fadeout();
#else
  vquick_fadeout();
#endif

  clear_screen();

#ifdef CONFIG_LOADSAVE_METER
  // Fade back in to display the meter if, for whatever reason, it can't use
  // the protected palette.
  insta_fadein();
#endif
}

/**
 * Load a world and prepare it for gameplay.
 * On failure, make sure the title's fade setting is restored (if applicable).
 * If the provided start board parameter is a valid board in the loaded world,
 * gameplay will start on the given board.
 */
static boolean load_world_gameplay_ext(struct game_context *game, char *name,
 int start_board)
{
  struct world *mzx_world = ((context *)game)->world;
  boolean was_faded = get_fade_status();
  boolean ignore;

  struct board *cur_board;

  // Save the name of the current playing mod; we'll want it to continue playing
  // into gameplay if the first board has the same mod or mod *.
  char old_mod_playing[MAX_PATH];
  strcpy(old_mod_playing, mzx_world->real_mod_playing);

  load_vquick_fadeout_and_clear();

  game->fade_in = true;

  // Reset the joystick mappings to the defaults before loading a game config.
  joystick_reset_game_map();

  if(reload_world(mzx_world, name, &ignore))
  {
    if((start_board < 0) || (start_board >= mzx_world->num_boards) ||
     !(mzx_world->board_list[start_board]))
    {
      start_board = mzx_world->first_board;
    }

    // Do this even if it's the current board--this is necessary to set up
    // the temporary board if the title has Reset Board on Entry set.
    change_board(mzx_world, start_board);
    cur_board = mzx_world->current_board;

    // Send both JUSTENTERED and JUSTLOADED; the JUSTLOADED label will
    // take priority if a robot defines it.
    send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);
    send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

    change_board_set_values(mzx_world);
    change_board_load_assets(mzx_world);

    // Load the mod unless it's the mod from the title screen or *.
    strcpy(mzx_world->real_mod_playing, old_mod_playing);
    load_game_module(mzx_world, cur_board->mod_playing, true);
    sfx_clear_queue();

    caption_set_world(mzx_world);
    return true;
  }

  if(was_faded)
    game->fade_in = false;

  return false;
}

/**
 * Load a world and prepare it for gameplay.
 * On failure, make sure the title's fade setting is restored (if applicable).
 */
static boolean load_world_gameplay(struct game_context *game, char *name)
{
  return load_world_gameplay_ext(game, name, -1);
}

/**
 * Load a world and prepare it for the title screen.
 * On failure, display a blank screen.
 */

static boolean load_world_title(struct game_context *game, char *name)
{
  struct world *mzx_world = ((context *)game)->world;
  boolean ignore;

  load_vquick_fadeout_and_clear();
  enable_intro_mesg();
  game->fade_in = true;

  // Reset the joystick mappings to the defaults before loading a game config.
  joystick_reset_game_map();

  if(reload_world(mzx_world, name, &ignore))
  {
    // Load was successful, so set curr_file
    if(curr_file != name)
      strcpy(curr_file, name);

    // Only send JUSTLOADED on the title screen.
    send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

    // Do this even though it should be the initial board--this is necessary to
    // set up the temporary board if the title has Reset Board on Entry set.
    change_board(mzx_world, 0);
    change_board_set_values(mzx_world);
    change_board_load_assets(mzx_world);

    // Music may be playing from the previous world or from editing. End the
    // module explicitly first in case the title module fails to load.
    audio_end_module();
    load_board_module(mzx_world);
    sfx_clear_queue();

    caption_set_world(mzx_world);
    return true;
  }
  else

  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }

  return false;
}

/**
 * Load a saved game and prepare it for gameplay.
 * On failure, make sure the title world's fade setting is restored.
 */

static boolean load_savegame(struct game_context *game, char *name)
{
  struct world *mzx_world = ((context *)game)->world;
  boolean was_faded = get_fade_status();
  boolean save_is_faded;

  load_vquick_fadeout_and_clear();
  game->fade_in = true;

  if(reload_savegame(mzx_world, name, &save_is_faded))
  {
    if(curr_sav != name)
      strcpy(curr_sav, name);

    // Only send JUSTLOADED for savegames.
    send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
    find_player(mzx_world);

    load_game_module(mzx_world, mzx_world->real_mod_playing, false);
    sfx_clear_queue();

    if(save_is_faded)
      game->fade_in = false;

    caption_set_world(mzx_world);
    return true;
  }

  if(was_faded)
    game->fade_in = false;

  return false;
}

/**
 * Open a user interface to select a world to load on the title.
 */

static boolean load_world_title_selection(struct game_context *game)
{
  struct world *mzx_world = ((context *)game)->world;
  char world_name[MAX_PATH] = { 0 };

  if(!choose_file_ch(mzx_world, world_ext, world_name, "Load World",
   ALLOW_ALL_DIRS))
  {
    return load_world_title(game, world_name);
  }

  return false;
}

/**
 * Open a user interface to select a save to load.
 */

static boolean load_savegame_selection(struct game_context *game)
{
  struct world *mzx_world = ((context *)game)->world;
  char save_file_name[MAX_PATH] = { 0 };
  int slot_result = SLOTSEL_FILE_MANAGER_RESULT;

  if(get_config()->save_slots)
  {
    slot_result = slot_manager(mzx_world, save_file_name,
     "Choose game to restore", false);

    if(slot_result == SLOTSEL_CANCEL_RESULT)
    {
      return false;
    }
  }

  if(slot_result == SLOTSEL_OK_RESULT ||
   !choose_file_ch(mzx_world, save_ext, save_file_name,
    "Choose game to restore", ALLOW_ALL_DIRS))
  {
    return load_savegame(game, save_file_name);
  }

  return false;
}

/**
 * Draw function for the title and gameplay. This actually includes most of the
 * game update cycle, including player input and updating the board.
 */

static boolean game_draw(context *ctx)
{
  struct game_context *game = (struct game_context *)ctx;
  const struct config_info *conf = get_config();
  struct world *mzx_world = ctx->world;

  // No game state change has happened (yet)
  mzx_world->change_game_state = CHANGE_STATE_NONE;

  if(game->is_title && game->fade_in)
  {
    // Focus on center
    int x, y;
    set_screen_coords(640/2, 350/2, &x, &y);
    focus_pixel(x, y);
  }

  if(!mzx_world->active)
  {
    // There is no MZX_SPEED to derive a framerate from, so use the UI rate.
    set_context_framerate_mode(ctx, FRAMERATE_UI);
    if(!conf->standalone_mode)
    {
      // Animate the intro message and periodically reset the timer so it's
      // obvious that MZX is still working and hasn't e.g. frozen. Do this at
      // (effectively) MZX speed 4 so it doesn't hurt to look at.
      if(!(intro_mesg_timer % 3))
      {
        if(intro_mesg_timer < 10)
          enable_intro_mesg();
        update_scroll_color();
      }
      draw_intro_mesg(mzx_world);
    }
    m_show();
    return true;
  }

  set_context_framerate_mode(ctx, FRAMERATE_MZX_SPEED);
  update_world(ctx, game->is_title);
  return draw_world(ctx, game->is_title);
}

// Forward declaration since this is used for both game and title screen.
__editor_maybe_static
void play_game(context *ctx, boolean *_fade_in);

/**
 * Idle function for the title and gameplay. Executes parts of the update
 * cycle that need to occur immediately after the delay.
 */

static boolean game_idle(context *ctx)
{
  struct game_context *game = (struct game_context *)ctx;
  const struct config_info *conf = get_config();
  struct world *mzx_world = ctx->world;

  if(!mzx_world->active)
    return false;

  if(game->fade_in)
  {
    vquick_fadein();
    game->fade_in = false;
  }

  switch(mzx_world->change_game_state)
  {
    case CHANGE_STATE_NONE:
    case CHANGE_STATE_INTERRUPT_CYCLE:
      break;

    case CHANGE_STATE_SWAP_WORLD:
    {
      // The SWAP WORLD command was used by a robot.
      // TODO: the game has already been loaded at this point, but maybe
      // should be loaded here instead of in run_robot.c?
      caption_set_world(mzx_world);

      // Load the new board's mod
      load_board_module(mzx_world);

      // Send both JUSTLOADED and JUSTENTERED; the JUSTENTERED label will take
      // priority if a robot defines it (instead of JUSTLOADED like on the title
      // screen).
      send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
      send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);

      return true;
    }

    case CHANGE_STATE_LOAD_GAME_ROBOTIC:
    {
      // The LOAD_GAME counter was used by a robot.
      // TODO: the game has already been loaded at this point, but maybe
      // should be loaded here instead of in counter.c?

      // real_mod_playing was set during the savegame load but the mod hasn't
      // started playing yet.
      load_game_module(mzx_world, mzx_world->real_mod_playing, false);

      // Only send JUSTLOADED for savegames.
      send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

      return true;
    }

    case CHANGE_STATE_PLAY_GAME_ROBOTIC:
    {
      if(!game->is_title)
        break;

      if(load_world_gameplay(game, curr_file))
      {
        play_game(ctx, NULL);
        game->need_reload = true;
        return true;
      }
      break;
    }

    case CHANGE_STATE_EXIT_GAME_ROBOTIC:
    {
      // The EXIT_GAME counter was used by a robot. This works during gameplay,
      // but also on the titlescreen if standalone mode is active.
      if(!game->is_title || conf->standalone_mode)
      {
        destroy_context(ctx);
        return true;
      }
      break;
    }

    case CHANGE_STATE_REQUEST_EXIT:
    {
      // The user halted the program while a robot was executing.
      destroy_context(ctx);
      return true;
    }
  }

  // A board change or other form of teleport may need to occur.
  // This may require a fade in the next time this function is run (next cycle).
  if(update_resolve_target(mzx_world, &(game->fade_in)))
    return true;

  // The SAVE_GAME counter might have been used this cycle.
  if(!game->is_title && mzx_world->robotic_save_type == SAVE_GAME)
  {
    save_world(mzx_world, mzx_world->robotic_save_path, true, MZX_VERSION);
    mzx_world->robotic_save_type = SAVE_NONE;
  }

  return false;
}

static boolean game_key(context *ctx, int *key); // Forward declaration.

/**
 * Callback for handling the game menu key.
 */
static void game_menu_callback(context *ctx, context_callback_param *ignore)
{
  struct game_context *game = (struct game_context *)ctx;
  int key = game->menu_key;

  game->menu_key = 0;

  if(key)
    game_key(ctx, &(key));
}

/**
 * Joystick function for the gameplay context. Gameplay only detects UI
 * escape presses and ignores everything else.
 */
static boolean game_joystick(context *ctx, int *key, int action)
{
  struct game_context *game = (struct game_context *)ctx;

  switch(action)
  {
    case JOY_START:
    case JOY_SELECT:
    {
      // Special: open the game menu if the enter or escape menu is allowed.
      if(allow_enter_menu(ctx->world, false) ||
       allow_exit_menu(ctx->world, false))
      {
        game_menu(ctx, true, &(game->menu_key), &(game->menu_alt));
        context_callback(ctx, NULL, game_menu_callback);
      }
      return true;
    }
  }
  return false;
}

/**
 * Key function for the gameplay context. This handles interface keys and some
 * key labels; some other keys are handled in the update function.
 */
static boolean game_key(context *ctx, int *key)
{
  struct game_context *game = (struct game_context *)ctx;
  const struct config_info *conf = get_config();
  struct world *mzx_world = ctx->world;
  struct board *cur_board = mzx_world->current_board;
  char keylbl[] = "KEY?";

  int key_status = get_key_status(keycode_internal_wrt_numlock, *key);
  boolean exit_status = get_exit_status();
  boolean confirm_exit = false;

  if(*key && !exit_status)
  {
    // Get the char for the KEY? labels. If there is no relevant unicode
    // keypress, we want to use the regular code instead.
    int key_unicode = get_key(keycode_text_ascii);
    int key_char = *key;

    if(key_unicode > 0 && key_unicode < 256)
      key_char = key_unicode;

    if(key_char > 0 && key_char < 256)
    {
      // KEY? label and pre-2.80 KEY counter.
      // Values over 256 have no meaning here.
      key_char = toupper(key_char);

      // 1.xx only supported A-Z. <2.60 only supported 1-9 and A-Z.
      // Since 2.60 magic overlaps with a lot of older games, 2.62 came out
      // a week after it, and this counter frequently broke old games,
      // apply the new behavior conservatively to >=2.62 instead.
      if(mzx_world->version >= V262 ||
       (key_char >= 'A' && key_char <= 'Z') ||
       (key_char >= '1' && key_char <= '9' && mzx_world->version >= V200))
      {
        // Send the KEY? label.
        keylbl[3] = key_char;
        send_robot_all_def(mzx_world, keylbl);

        // In pre-2.80 MZX versions, KEY was a board counter.
        if(mzx_world->version < VERSION_PORT)
          cur_board->last_key = key_char;
      }
    }

    switch(*key)
    {
      case IKEY_F3:
      {
        // Save game
        if(allow_save_menu(mzx_world))
        {
          char save_game[MAX_PATH];
          int slot_result = SLOTSEL_FILE_MANAGER_RESULT;
          strcpy(save_game, curr_sav);

          if(get_config()->save_slots)
          {
            slot_result = slot_manager(mzx_world, save_game, "Save game", true);

            if(slot_result == SLOTSEL_CANCEL_RESULT)
            {
              return true;
            }
          }

          if(slot_result == SLOTSEL_OK_RESULT ||
           !new_file(mzx_world, save_ext, ".sav", save_game, "Save game",
            ALLOW_ALL_DIRS))
          {
            strcpy(curr_sav, save_game);
            save_world(mzx_world, curr_sav, true, MZX_VERSION);
          }
        }
        return true;
      }

      case IKEY_F4:
      {
        // ALT+F4 - do nothing.
        if(get_alt_status(keycode_internal))
          break;

        // Restore saved game
        if(allow_load_menu(mzx_world, false))
          load_savegame_selection(game);

        return true;
      }

      case IKEY_F5:
      case IKEY_INSERT:
      {
        // Change bomb type
        if(!mzx_world->dead)
          player_switch_bomb_type(mzx_world);

        return true;
      }

      // Toggle debug mode
      case IKEY_F6:
      {
        if(edit_world && allow_debug_menu(mzx_world))
          mzx_world->debug_mode = !(mzx_world->debug_mode);

        return true;
      }

      // Cheat
      case IKEY_F7:
      {
        if(game->allow_cheats || mzx_world->editing)
          player_cheat_give_all(mzx_world);

        return true;
      }

      // Cheat More
      case IKEY_F8:
      {
        if(game->allow_cheats || mzx_world->editing)
          player_cheat_zap(mzx_world);

        return true;
      }

      // Quick save
      case IKEY_F9:
      {
        if(allow_save_menu(mzx_world))
          save_world(mzx_world, curr_sav, true, MZX_VERSION);

        return true;
      }

      // Quick load saved game
      case IKEY_F10:
      {
        if(allow_load_menu(mzx_world, false))
        {
          struct stat file_info;

          if(!vstat(curr_sav, &file_info))
            load_savegame(game, curr_sav);
        }
        return true;
      }

      case IKEY_F11:
      {
        if(allow_debug_menu(mzx_world))
        {
          // Breakpoint editor
          if(get_alt_status(keycode_internal) || game->menu_alt)
          {
            if(debug_robot_config)
              debug_robot_config(mzx_world);
          }
          // Counter debugger
          else
          {
            if(debug_counters)
              debug_counters(ctx);
          }
        }
        game->menu_alt = false;
        return true;
      }

      case IKEY_RETURN:
      {
        // KeyEnter label was added in 2.60. See the KEY? comments above for
        // why this is being applied to >=2.62 instead of 2.60.
        if(mzx_world->version >= V262)
          send_robot_all_def(mzx_world, "KeyEnter");

        // Ignore if this isn't a fresh press
        if(key_status != 1)
          return true;

        if(allow_enter_menu(mzx_world, false))
        {
          game_menu(ctx, false, &(game->menu_key), &(game->menu_alt));
          context_callback(ctx, NULL, game_menu_callback);
        }
        return true;
      }

      case IKEY_ESCAPE:
      {
        // Ignore if this isn't a fresh press
        // NOTE: disabled because it breaks the joystick action.
        //if(key_status != 1)
          //return true;

        // ESCAPE_MENU (2.90+)
        if(allow_exit_menu(mzx_world, false))
          confirm_exit = true;

        break;
      }
    }
  }

  // Quit
  if(exit_status || confirm_exit)
  {
    // Special behaviour in standalone- only escape exits
    // ask for confirmation. Exit events instead terminate MegaZeux.
    if(conf->standalone_mode && !confirm_exit)
    {
      core_full_exit(ctx);
    }
    else
    {
      if(!confirm(mzx_world, "Quit playing- Are you sure?"))
        destroy_context(ctx);
    }
    return true;
  }

  return false;
}

/**
 * Destroy a game context.
 */

static void game_destroy(context *ctx)
{
  vquick_fadeout();
  clear_screen();
  audio_end_module();
  sfx_clear_queue();
}

/**
 * Create and run the gameplay context.
 */

__editor_maybe_static
void play_game(context *parent, boolean *_fade_in)
{
  const struct config_info *conf = get_config();
  struct game_context *game;
  struct context_spec spec;

  game = ccalloc(1, sizeof(struct game_context));
  game->fade_in = _fade_in ? * _fade_in : true;
  game->is_title = false;
  game->allow_cheats = false;

  // Not used for gameplay...
  game->need_reload = false;
  game->load_dialog_on_failed_load = false;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = game_draw;
  spec.idle     = game_idle;
  spec.key      = game_key;
  spec.joystick = game_joystick;
  spec.destroy  = game_destroy;

  spec.framerate_mode = FRAMERATE_MZX_SPEED;

  create_context((context *)game, parent, &spec, CTX_PLAY_GAME);

  if(conf->allow_cheats == ALLOW_CHEATS_ALWAYS ||
   (conf->allow_cheats == ALLOW_CHEATS_MZXRUN && !edit_world))
  {
    // Enable cheating outside of the editor.
    game->allow_cheats = true;
  }

  clear_intro_mesg();
}

/**
 * When control returns to the title screen from gameplay or the editor, the
 * current world needs to be reloaded. When starting MegaZeux, if the startup
 * world can't be loaded, the load menu needs to be displayed instead.
 */

static void title_resume(context *ctx)
{
  struct game_context *title = (struct game_context *)ctx;
  struct config_info *conf = get_config();

  if(title->need_reload)
  {
    if(!load_world_title(title, curr_file))
    {
      // World failed to load and no video? Hard exit so MZX doesn't stay
      // stuck on a dialog or the blank title screen...
      if(!has_video_initialized())
      {
        destroy_context(ctx);
        return;
      }

      conf->standalone_mode = false;

      // Do this to avoid some UI fade bugs...
      insta_fadein();

      if(title->load_dialog_on_failed_load)
      {
        load_world_title_selection(title);
      }
    }
  }

  title->load_dialog_on_failed_load = false;
  title->need_reload = false;
}

static boolean title_key(context *ctx, int *key); // Forward declaration.

/**
 * Callback for handling the main menu key.
 */
static void main_menu_callback(context *ctx, context_callback_param *ignore)
{
  struct game_context *title = (struct game_context *)ctx;
  int key = title->menu_key;

  title->menu_key = 0;
  title->menu_alt = false;

  if(key)
    title_key(ctx, &(key));
}

/**
 * Joystick handler for the title screen.
 */
static boolean title_joystick(context *ctx, int *key, int action)
{
  struct game_context *title = (struct game_context *)ctx;
  struct world *mzx_world = ctx->world;

  switch(action)
  {
    case JOY_B:
    case JOY_SELECT:
    {
      // Special: open the main menu if the enter or escape menu is allowed.
      // The B button is the alternate for select instead of start because the
      // start button makes much more sense playing the game on the title.
      if(allow_enter_menu(mzx_world, true) || allow_exit_menu(mzx_world, true))
      {
        main_menu(ctx, true, &(title->menu_key));
        context_callback(ctx, NULL, main_menu_callback);
      }
      return true;
    }

    case JOY_A:         *key = IKEY_F5; return true;
    case JOY_X:         *key = IKEY_F3; return true;
    case JOY_Y:         *key = IKEY_F4; return true;
    case JOY_START:     *key = IKEY_F5; return true;
    case JOY_LSHOULDER: *key = IKEY_F2; return true;
    case JOY_RSHOULDER: *key = IKEY_F2; return true;
    case JOY_LTRIGGER:  *key = IKEY_F3; return true;
    case JOY_RTRIGGER:  *key = IKEY_F4; return true;
  }
  return false;
}

/**
 * Key handler for the title screen.
 */
static boolean title_key(context *ctx, int *key)
{
  const struct config_info *conf = get_config();
  struct game_context *title = (struct game_context *)ctx;
  struct world *mzx_world = ctx->world;

  // NOTE: disabled due to joystick support. See IKEY_RETURN and IKEY_ESCAPE.
  //int key_status = get_key_status(keycode_internal_wrt_numlock, *key);
  boolean exit_status = get_exit_status();

  boolean reload_curr_file_in_editor = true;
  boolean confirm_exit = false;

  switch(*key)
  {
#ifdef CONFIG_HELPSYS
    case IKEY_h:
    {
      // Help system alternate binding.
      *key = IKEY_F1;
      break;
    }
#endif

    case IKEY_s:
    {
      // Configure alternate binding.
      *key = IKEY_F2;
      break;
    }

    case IKEY_F3:
    case IKEY_l:
    {
      if(allow_load_world_menu(mzx_world))
        load_world_title_selection(title);

      return true;
    }

    case IKEY_F4:
    {
      // ALT+F4 - do nothing.
      if(get_alt_status(keycode_internal))
        break;
    }

    /* fallthrough */

    case IKEY_r:
    {
      // Restore saved game
      if(allow_load_menu(mzx_world, true))
      {
        if(load_savegame_selection(title))
        {
          play_game(ctx, &(title->fade_in));
          title->need_reload = true;
        }
      }
      return true;
    }

    case IKEY_F5:
    case IKEY_p:
    {
      // Play game
      if(mzx_world->active)
      {
        if(mzx_world->only_from_swap)
        {
          error("You can only play this game via a swap from another game",
           ERROR_T_WARNING, ERROR_OPT_OK, 0x3101);
          return true;
        }

        if(load_world_gameplay(title, curr_file))
        {
          play_game(ctx, NULL);
          title->need_reload = true;
        }
      }
      return true;
    }

    case IKEY_F7:
    case IKEY_u:
    {
      if(check_for_updates)
      {
        // FIXME this is garbage
        int current_music_vol = audio_get_music_volume();
        int current_pcs_vol = audio_get_pcs_volume();
        audio_set_music_volume(0);
        audio_set_pcs_volume(0);
        if(mzx_world->active)
          audio_set_module_volume(0);

        check_for_updates(ctx, false);

        audio_set_pcs_volume(current_pcs_vol);
        audio_set_music_volume(current_music_vol);
        if(mzx_world->active)
          audio_set_module_volume(mzx_world->current_board->volume);
      }
      return true;
    }

    case IKEY_F8:
    case IKEY_n:
    {
      reload_curr_file_in_editor = false;
    }

    /* fallthrough */

    case IKEY_F9:
    case IKEY_e:
    {
      if(edit_world)
      {
        // Editor
        sfx_clear_queue();
        vquick_fadeout();
        title->need_reload = true;
        title->fade_in = true;

        edit_world(ctx, reload_curr_file_in_editor);
      }
      return true;
    }

    // Quickload saved game
    case IKEY_F10:
    {
      if(allow_load_menu(mzx_world, true))
      {
        struct stat file_info;

        if(!vstat(curr_sav, &file_info) && load_savegame(title, curr_sav))
        {
          play_game(ctx, &(title->fade_in));
          title->need_reload = true;
        }
      }
      return true;
    }

    case IKEY_RETURN: // Enter
    {
      // Ignore if this isn't a fresh press
      // NOTE: disabled because it breaks the joystick actions.
      //if(key_status != 1)
        //return true;

      if(allow_enter_menu(mzx_world, true))
      {
        main_menu(ctx, false, &(title->menu_key));
        context_callback(ctx, NULL, main_menu_callback);
      }
      return true;
    }

    case IKEY_ESCAPE:
    {
      // Ignore if this isn't a fresh press
      // NOTE: disabled because it breaks the joystick actions.
      //if(key_status != 1)
        //return true;

      if(allow_exit_menu(mzx_world, true))
        confirm_exit = true;

      break;
    }
  }

  // Quit
  if(exit_status || confirm_exit)
  {
    // Special behaviour in standalone- only escape exits
    // ask for confirmation. Exit events instead terminate MegaZeux.
    if(conf->standalone_mode && !confirm_exit)
    {
      core_full_exit(ctx);
    }
    else
    {
      if(!confirm(mzx_world, "Exit MegaZeux - Are you sure?"))
        destroy_context(ctx);
    }
    return true;
  }

  return false;
}

/**
 * Create and run the title screen context. This context should only be started
 * when MegaZeux is opened, and MegaZeux should terminate if it is destroyed.
 * If startup_editor is enabled, also start the editor. If standalone_mode and
 * no_titlescreen mode are enabled, only attempt to start gameplay instead.
 */

void title_screen(context *parent)
{
  struct config_info *conf = get_config();
  struct game_context *title;
  struct game_context tmp;
  struct context_spec spec;

  tmp.ctx.world = parent->world;

  if(edit_world)
  {
    conf->standalone_mode = false;

    if(conf->test_mode)
    {
      if(load_world_gameplay_ext(&tmp, curr_file, conf->test_mode_start_board))
      {
        parent->world->editing = true;
        play_game(parent, NULL);
      }
      return;
    }
  }

  if(conf->standalone_mode && conf->no_titlescreen)
  {
    if(load_world_gameplay(&tmp, curr_file))
    {
      play_game(parent, NULL);
      return;
    }

    conf->standalone_mode = false;
  }

  title = ccalloc(1, sizeof(struct game_context));
  title->fade_in = true;
  title->need_reload = true;
  title->load_dialog_on_failed_load = true;
  title->is_title = true;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.resume   = title_resume;
  spec.draw     = game_draw;
  spec.idle     = game_idle;
  spec.key      = title_key;
  spec.joystick = title_joystick;
  spec.destroy  = game_destroy;

  create_context((context *)title, parent, &spec, CTX_TITLE_SCREEN);
  default_palette();

  if(edit_world && conf->startup_editor)
  {
    title->load_dialog_on_failed_load = false;
    edit_world((context *)title, true);
  }

#ifdef VERSION_PRERELEASE
  if(has_video_initialized())
  {
    error("Pre-release: created saves/worlds may be incompatible with future versions.",
     ERROR_T_WARNING, ERROR_OPT_OK, 0);
  }
#endif

  clear_screen();
}
