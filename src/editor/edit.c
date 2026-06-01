/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../board.h"
#include "../caption.h"
#include "../const.h"
#include "../core.h"
#include "../counter.h"
#include "../data.h"
#include "../error.h"
#include "../extmem.h"
#include "../game.h"
#include "../game_player.h"
#include "../graphics.h"
#include "../idarray.h"
#include "../idput.h"
#include "../mzm.h"
#include "../robot.h"
#include "../scrdisp.h"
#include "../sfx.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"
#include "../io/path.h"
#include "../io/vio.h"

#include "../audio/audio.h"
#include "../event/event.h"
#include "../platform/platform.h"

#include "ansi.h"
#include "block.h"
#include "board.h"
#include "buffer.h"
#include "char_ed.h"
#include "configure.h"
#include "debug.h"
#include "edit.h"
#include "edit_di.h"
#include "edit_export.h"
#include "edit_menu.h"
#include "fill.h"
#include "graphics.h"
#include "pal_ed.h"
#include "param.h"
#include "robo_debug.h"
#include "robot.h"
#include "select.h"
#include "sfx_edit.h"
#include "undo.h"
#include "window.h"
#include "world.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#define NEW_WORLD_TITLE       "Untitled world"
#define TEST_WORLD_FILENAME   "__test.mzx"
#define TEST_WORLD_PATTERN    "__test%d.mzx"

static const char *const world_ext[] = { ".MZX", NULL };
static const char *const mzb_ext[] = { ".MZB", NULL };
static const char *const mzm_ext[] = { ".MZM", NULL };
static const char *const chr_ext[] = { ".CHR", NULL };
static const char *const ans_ext[] = { ".ANS", ".TXT", NULL };
static const char *const mod_ext[] =
{ ".ogg", ".mod", ".s3m", ".xm", ".it",
  ".669", ".amf", ".dsm", ".far", ".gdm",
  ".med", ".mtm", ".nst", ".oct", ".okt",
  ".stm", ".ult", ".wow", ".rad",
  NULL
};
static const char *const sam_ext[] =
{ ".WAV", ".SAM", ".OGG",
  NULL
};

static const char *const image_ext[] =
{
  ".PNG",
  NULL
};

struct editor_context
{
  context ctx;
  subcontext *edit_menu;

  char current_world[MAX_PATH];
  char mzb_name_buffer[MAX_PATH];
  char mzm_name_buffer[MAX_PATH];
  char chr_name_buffer[MAX_PATH];
  char current_listening_dir[MAX_PATH];
  char current_listening_mod[MAX_PATH];

  // Editor state
  enum editor_mode mode;
  boolean modified;
  boolean modified_prev;
  boolean first_board_prompt;
  boolean listening_mod_active;
  boolean show_board_under_overlay; // = 1
  int backup_timestamp;             // = get_ticks()
  int backup_num;
  int screen_height;

  // Buffer
  struct buffer_info buffer;
  struct robot buffer_robot;
  struct scroll buffer_scroll;
  struct sensor buffer_sensor;
  boolean use_default_color;        // = 1
  enum thing stored_board_id;
  char stored_board_color;
  char stored_board_param;
  char stored_overlay_char;
  char stored_overlay_color;
  char stored_vlayer_char;
  char stored_vlayer_color;

  // Modify in-place buffer
  struct buffer_info temp_buffer;
  struct robot temp_robot;
  struct scroll temp_scroll;
  struct sensor temp_sensor;
  boolean modified_storage;
  int modify_param;

  // Current board
  int board_width;
  int board_height;

  // Cursor
  int cursor_x;
  int cursor_y;
  int scroll_x;
  int scroll_y;
  int debug_x;                      // = 60
  int text_start_x;
  struct block_info block;
  enum cursor_mode cursor_mode;
  int mouse_last_x;
  int mouse_last_y;

  // Temporary stored cursor positions
  int stored_board_x;
  int stored_board_y;
  int stored_vlayer_x;
  int stored_vlayer_y;
  int stored_scroll_x;
  int stored_scroll_y;
  int stored_vscroll_x;
  int stored_vscroll_y;

  // Undo history
  struct undo_history *cur_history;
  struct undo_history *board_history;
  struct undo_history *overlay_history;
  struct undo_history *vlayer_history;
  boolean continue_mouse_history;

  // Flash thing
  boolean flashing;
  enum thing flash_start;
  int flash_len;
  int flash_char_a;
  int flash_char_b;
  int flash_timer;
  int flash_timer_swap;
  int flash_timer_max;
  uint32_t flash_layer;

  // ANSi settings
  int ansi_line_wrap_column; // = 80
  boolean ansi_line_wrap;
  boolean ansi_save_text_only;
  boolean ansi_save_doorway_mode;
  char ansi_save_title[36];
  char ansi_save_author[21];

  // Testing
  int test_reload_board;
  int test_reload_version;
  char test_reload_dir[MAX_PATH];
  char test_reload_file[MAX_PATH];
  boolean reload_after_testing;
};

/**
 * Wrapper for reload_world that also loads world-specific editor config files.
 */
static boolean editor_reload_world(struct editor_context *editor,
 const char *file)
{
  struct world *mzx_world = ((context *)editor)->world;

  char config_file_name[MAX_PATH];
  boolean ignore;

  if(!reload_world(mzx_world, file, &ignore))
    return false;

  // Part 1: Reset the config file.
  load_editor_config_backup();

  // Part 2: Now load the new world.editor.cnf.
  if(get_local_editor_config_name(config_file_name, MAX_PATH, file) > 0)
    set_config_from_file(GAME_EDITOR_CNF, config_file_name);

  edit_menu_show_board_mod(editor->edit_menu);
  return true;
}

/**
 * Get the filename for the test world file that will be reloaded after testing.
 * Attempts to ensure that previous test files left behind due to crashes or
 * other instances of MegaZeux that are also testing aren't overwritten.
 */
static void get_test_world_filename(struct editor_context *editor)
{
  struct stat file_info;
  int i;

  strcpy(editor->test_reload_file, TEST_WORLD_FILENAME);

  // If the regular file exists, attempt numbered names.
  for(i = 2; !vstat(editor->test_reload_file, &file_info); i++)
    snprintf(editor->test_reload_file, MAX_PATH, TEST_WORLD_PATTERN, i);
}

/**
 * Update the editor to the current undo history.
 */
static void fix_history(struct editor_context *editor)
{
  switch(editor->mode)
  {
    case EDIT_BOARD:
      editor->cur_history = editor->board_history;
      return;

    case EDIT_OVERLAY:
      editor->cur_history = editor->overlay_history;
      return;

    case EDIT_VLAYER:
      editor->cur_history = editor->vlayer_history;
      return;

    default:
      editor->cur_history = NULL;
      return;
  }
}

/**
 * Clear or reset the current board undo history.
 */
static void clear_board_history(struct editor_context *editor)
{
  destruct_undo_history(editor->board_history);
  editor->board_history = NULL;
  fix_history(editor);
}

/**
 * Clear or reset the current overlay undo history.
 */
static void clear_overlay_history(struct editor_context *editor)
{
  destruct_undo_history(editor->overlay_history);
  editor->overlay_history = NULL;
  fix_history(editor);
}

/**
 * Clear or reset the current vlayer undo history.
 */
static void clear_vlayer_history(struct editor_context *editor)
{
  destruct_undo_history(editor->vlayer_history);
  editor->vlayer_history = NULL;
  fix_history(editor);
}

/**
 * Set the buffer to the default id/color/param for the current mode.
 */
static void default_buffer(struct editor_context *editor)
{
  struct buffer_info *buffer = &(editor->buffer);

  if(editor->mode == EDIT_BOARD)
  {
    buffer->id = SPACE;
    buffer->color = 7;
    buffer->param = 0;
  }
  else
  {
    buffer->id = SPACE;
    buffer->color = 7;
    buffer->param = 32;
  }
}

/**
 * Switch the current editing mode. If switching to or from the vlayer,
 * the current cursor and scroll position needs to be stored.
 */
static void set_editor_mode(struct editor_context *editor,
 enum editor_mode new_mode)
{
  if(editor->mode != EDIT_VLAYER && new_mode == EDIT_VLAYER)
  {
    editor->stored_board_x = editor->cursor_x;
    editor->stored_board_y = editor->cursor_y;
    editor->stored_scroll_x = editor->scroll_x;
    editor->stored_scroll_y = editor->scroll_y;

    editor->cursor_x = editor->stored_vlayer_x;
    editor->cursor_y = editor->stored_vlayer_y;
    editor->scroll_x = editor->stored_vscroll_x;
    editor->scroll_y = editor->stored_vscroll_y;
  }
  else

  if(editor->mode == EDIT_VLAYER && new_mode != EDIT_VLAYER)
  {
    editor->stored_vlayer_x = editor->cursor_x;
    editor->stored_vlayer_y = editor->cursor_y;
    editor->stored_vscroll_x = editor->scroll_x;
    editor->stored_vscroll_y = editor->scroll_y;

    editor->cursor_x = editor->stored_board_x;
    editor->cursor_y = editor->stored_board_y;
    editor->scroll_x = editor->stored_scroll_x;
    editor->scroll_y = editor->stored_scroll_y;
  }

  // When switching modes, store the buffer for the old mode and load
  // the buffer for the new mode. Previously, switching modes would
  // (inconsistently) set the default buffer instead.
  if(editor->mode != new_mode)
  {
    struct buffer_info *buffer = &(editor->buffer);

    switch(editor->mode)
    {
      case EDIT_BOARD:
        editor->stored_board_id = buffer->id;
        editor->stored_board_color = buffer->color;
        editor->stored_board_param = buffer->param;
        break;

      case EDIT_OVERLAY:
        editor->stored_overlay_char = buffer->param;
        editor->stored_overlay_color = buffer->color;
        break;

      case EDIT_VLAYER:
        editor->stored_vlayer_char = buffer->param;
        editor->stored_vlayer_color = buffer->color;
        break;
    }

    switch(new_mode)
    {
      case EDIT_BOARD:
        buffer->id = editor->stored_board_id;
        buffer->color = editor->stored_board_color;
        buffer->param = editor->stored_board_param;
        break;

      case EDIT_OVERLAY:
        buffer->id = SPACE;
        buffer->param = editor->stored_overlay_char;
        buffer->color = editor->stored_overlay_color;
        break;

      case EDIT_VLAYER:
        buffer->id = SPACE;
        buffer->param = editor->stored_vlayer_char;
        buffer->color = editor->stored_vlayer_color;
        break;
    }
  }

  editor->mode = new_mode;
  fix_history(editor);
}

/**
 * Update the editor board values to match the current board.
 */
static void synchronize_board_values(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;

  editor->board_width = cur_board->board_width;
  editor->board_height = cur_board->board_height;

  if(editor->mode == EDIT_VLAYER)
  {
    // During vlayer editing, treat the vlayer as if it's the board.
    editor->board_width = mzx_world->vlayer_width;
    editor->board_height = mzx_world->vlayer_height;
  }
}

/**
 * Fix the world title after an operation that may have modified the current
 * board's title. This should be done before fix_caption.
 */
static void fix_world_title(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;

  if(mzx_world->current_board_id == 0)
  {
    snprintf(mzx_world->name, BOARD_NAME_SIZE, "%s", cur_board->board_name);
    mzx_world->name[BOARD_NAME_SIZE - 1] = '\0';
  }
}

/**
 * Set the caption for the editor.
 */
static void fix_caption(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;

  caption_set_board(mzx_world, cur_board);
}

/**
 * Bound the cursor within the current board and update the debug window
 * position.
 */
static void fix_cursor(struct editor_context *editor)
{
  if(editor->cursor_x >= editor->board_width)
    editor->cursor_x = (editor->board_width - 1);

  if(editor->cursor_y >= editor->board_height)
    editor->cursor_y = (editor->board_height - 1);

  if((editor->cursor_x - editor->scroll_x) < (editor->debug_x + 25))
    editor->debug_x = 60;

  if((editor->cursor_x - editor->scroll_x) > (editor->debug_x - 5))
    editor->debug_x = 0;
}

/**
 * Bound the editor viewport within the board.
 */
static void fix_scroll(struct editor_context *editor)
{
  // Ensure the cursor is visible in the editor viewport.
  if(editor->cursor_x - editor->scroll_x < 5)
    editor->scroll_x = editor->cursor_x - 5;

  if(editor->cursor_x - editor->scroll_x > 74)
    editor->scroll_x = editor->cursor_x - 74;

  if(editor->cursor_y - editor->scroll_y < 3)
    editor->scroll_y = editor->cursor_y - 3;

  if(editor->cursor_y - editor->scroll_y > (editor->screen_height - 5))
    editor->scroll_y = editor->cursor_y - editor->screen_height + 5;

  // The edge bounds checks need to be done in this order.
  if(editor->scroll_x + 80 > editor->board_width)
    editor->scroll_x = (editor->board_width - 80);

  if(editor->scroll_x < 0)
    editor->scroll_x = 0;

  if(editor->scroll_y + editor->screen_height > editor->board_height)
    editor->scroll_y = (editor->board_height - editor->screen_height);

  if(editor->scroll_y < 0)
    editor->scroll_y = 0;

  fix_cursor(editor);
}

/**
 * Play the current board's mod if the listening mod isn't playing.
 * This will restart the board mod even if it's already playing.
 */
static void fix_mod(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;

  if(!editor->listening_mod_active)
  {
    if(!strcmp(mzx_world->current_board->mod_playing, "*"))
    {
      mzx_world->real_mod_playing[0] = '\0';
      audio_end_module();
    }

    else
    {
      load_board_module(mzx_world);
    }
  }
}

/**
 * Set the active world's current board.
 */
static void editor_set_current_board(struct editor_context *editor,
 int new_board, boolean is_extram)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board;

  if(new_board >= 0)
  {
    // Hopefully we won't be getting anything out of range but just in case
    if(new_board >= mzx_world->num_boards)
      new_board = mzx_world->num_boards - 1;

    mzx_world->current_board_id = new_board;
    cur_board = mzx_world->board_list[new_board];

    if(is_extram)
      set_current_board_ext(mzx_world, cur_board);
    else
      set_current_board(mzx_world, cur_board);

    if((editor->mode == EDIT_OVERLAY && !cur_board->overlay_mode) ||
     editor->mode == EDIT_VLAYER)
    {
      set_editor_mode(editor, EDIT_BOARD);

      // If there's a block action active, cancel it.
      if(editor->block.selected)
      {
        editor->block.selected = false;
        editor->cursor_mode = CURSOR_PLACE;
      }
    }

    synchronize_board_values(editor);
    fix_scroll(editor);
    fix_caption(editor);

    // We want mod switching from changing boards to act the same as gameplay.
    if(!editor->listening_mod_active)
      load_game_module(mzx_world, cur_board->mod_playing, true);

    // Load the board charset and palette
    if(editor_conf->editor_load_board_assets)
      change_board_load_assets(mzx_world);

    // Reset the local undo histories
    clear_board_history(editor);
    clear_overlay_history(editor);

    edit_menu_show_board_mod(editor->edit_menu);
    editor->modified = true;
  }
}

/**
 * Move the cursor on the board.
 */
static void move_edit_cursor(struct editor_context *editor,
 int move_x, int move_y)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;
  struct buffer_info *buffer = &(editor->buffer);

  // Bound the movement within the board
  if(editor->cursor_x + move_x < 0)
    move_x = -editor->cursor_x;

  if(move_x + editor->cursor_x > editor->board_width - 1)
    move_x = editor->board_width - editor->cursor_x - 1;

  if(editor->cursor_y + move_y < 0)
    move_y = -editor->cursor_y;

  if(move_y + editor->cursor_y > editor->board_height - 1)
    move_y = editor->board_height - editor->cursor_y - 1;

  if(move_x)
    move_y = 0;

  if(move_x || move_y)
  {
    int offset = editor->cursor_x + (editor->cursor_y * editor->board_width);
    int step_x = SGN(move_x);
    int step_y = SGN(move_y);
    int width = abs(move_x) + 1;
    int height = abs(move_y) + 1;

    if(step_x < 0)
      offset -= (width - 1);

    if(step_y < 0)
      offset -= editor->board_width * (height - 1);

    if(editor->cursor_mode == CURSOR_DRAW)
    {
      switch(editor->mode)
      {
        case EDIT_BOARD:
          add_block_undo_frame(
           mzx_world, editor->cur_history, cur_board, offset, width, height);
          break;

        case EDIT_OVERLAY:
          add_layer_undo_frame(editor->cur_history, cur_board->overlay,
           cur_board->overlay_color, editor->board_width, offset, width, height);
          break;

        case EDIT_VLAYER:
          add_layer_undo_frame(editor->cur_history, mzx_world->vlayer_chars,
           mzx_world->vlayer_colors, editor->board_width, offset, width, height);
          break;
      }
    }

    do
    {
      editor->cursor_x += step_x;
      editor->cursor_y += step_y;
      move_x -= step_x;
      move_y -= step_y;

      if(editor->cursor_mode == CURSOR_DRAW)
      {
        // We're creating a history frame manually, so don't pass it.
        buffer->param = place_current_at_xy(mzx_world, buffer,
         editor->cursor_x, editor->cursor_y, editor->mode, NULL);
      }
    }
    while(move_x || move_y);

    if(editor->cursor_mode == CURSOR_DRAW)
    {
      update_undo_frame(editor->cur_history);
      editor->modified = true;

      if(editor_conf->editor_tab_focuses_view)
      {
        // Out-of-bounds will be fixed by fix_scroll
        editor->scroll_x = editor->cursor_x - 40;
        editor->scroll_y = editor->cursor_y - (editor->screen_height / 2);
      }
    }

    fix_scroll(editor);
  }
}

/**
 * Place typed text on the board.
 */
static void place_text(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct buffer_info temp_buffer;
  int num_placed = 0;

  temp_buffer.id = TEXT_ID;
  temp_buffer.color = editor->buffer.color;

  while(num_placed < KEY_UNICODE_MAX)
  {
    temp_buffer.param = get_key(keycode_text_ascii);
    if(temp_buffer.param)
    {
      place_current_at_xy(mzx_world, &temp_buffer,
       editor->cursor_x, editor->cursor_y, editor->mode, editor->cur_history);

      if(editor->cursor_x < (editor->board_width - 1))
        editor->cursor_x++;

      num_placed++;
      editor->modified = true;
    }
    else
      break;
  }
  fix_scroll(editor);
}

/**
 * Context to view the current board as if currently playing the board.
 */
struct view_board_context
{
  context ctx;
  int x;
  int y;
  int max_x;
  int max_y;
};

static boolean view_board_draw(context *ctx)
{
  struct view_board_context *vb = (struct view_board_context *)ctx;
  struct world *mzx_world = ctx->world;

  blank_layers();
  draw_viewport(mzx_world->current_board, mzx_world->edge_color);
  draw_game_window(mzx_world->current_board, vb->x, vb->y);
  return true;
}

static boolean view_board_key(context *ctx, int *key)
{
  struct view_board_context *vb = (struct view_board_context *)ctx;
  int move_times = 1;

  if(get_alt_status(keycode_internal))
    move_times = 10;

  if(get_exit_status())
    *key = IKEY_ESCAPE;

  switch(*key)
  {
    case IKEY_ESCAPE:
    {
      destroy_context(ctx);
      return true;
    }

    case IKEY_LEFT:
    {
      for(; move_times > 0; move_times--)
        if(vb->x)
          (vb->x)--;
      return true;
    }

    case IKEY_RIGHT:
    {
      for(; move_times > 0; move_times--)
        if(vb->x < vb->max_x)
          (vb->x)++;
      return true;
    }

    case IKEY_UP:
    {
      for(; move_times > 0; move_times--)
        if(vb->y)
          (vb->y)--;
      return true;
    }

    case IKEY_DOWN:
    {
      for(; move_times > 0; move_times--)
        if(vb->y < vb->max_y)
          (vb->y)++;
      return true;
    }
  }
  return false;
}

static void view_board(struct editor_context *editor)
{
  struct view_board_context *vb = cmalloc(sizeof(struct view_board_context));
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;
  struct context_spec spec;

  vb->max_x = cur_board->board_width - cur_board->viewport_width;
  vb->max_y = cur_board->board_height - cur_board->viewport_height;
  vb->x = CLAMP(editor->scroll_x, 0, vb->max_x);
  vb->y = CLAMP(editor->scroll_y, 0, vb->max_y);

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw = view_board_draw;
  spec.key  = view_board_key;

  create_context((context *)vb, (context *)editor, &spec,
   CTX_EDITOR_VIEW_BOARD);

  cursor_off();
  m_hide();
}

enum flash_thing_type
{
  FLASH_THING_NORMAL,
  FLASH_THING_CLOSE_COLORS,
  FLASH_THING_CLOSE_CHAR,
  FLASH_THING_SMZX,
};

/**
 * Flash a range of types to make them more visible onscreen.
 */
static void flash_thing(struct editor_context *editor,
 enum thing start, enum thing end, int flash_char_a, int flash_char_b)
{
  const struct editor_config_info *editor_conf = get_editor_config();

  // If the same type of flashing is already enabled, toggle it off instead.
  if(editor_conf->editor_show_thing_toggles && editor->flashing &&
   (editor->flash_start == start))
  {
    editor->flashing = false;
    return;
  }

  editor->flashing = true;
  editor->flash_start = start;
  editor->flash_len = end - start + 1;
  editor->flash_char_a = flash_char_a;
  editor->flash_char_b = flash_char_b;
  editor->flash_timer = 0;
  editor->flash_timer_swap = MAX(1, editor_conf->editor_show_thing_blink_speed);
  editor->flash_timer_max = MAX(1, editor_conf->editor_show_thing_blink_speed * 2);
}

/**
 * Initialize flashing for the current cycle and return the current flash char.
 */
static int flash_init(struct editor_context *editor)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  int chr = editor->flash_char_a;

  if(editor->flash_timer >= editor->flash_timer_swap)
    chr = editor->flash_char_b;

  editor->flash_layer = 0;
  if(!editor_conf->editor_show_thing_toggles)
    cursor_off();
  return chr;
}

/**
 * Increment the flash timer and clear any flash values that shouldn't persist.
 */
static void flash_done(struct editor_context *editor)
{
  editor->flash_timer = (editor->flash_timer + 1) % editor->flash_timer_max;
  editor->flash_layer = 0;
}

/**
 * Determine if a protected palette char flash is required.
 */
static enum flash_thing_type flash_get_type(struct editor_context *editor,
 struct board *cur_board, uint8_t chr, uint8_t color, uint8_t flash_chr)
{
  int screen_mode = get_screen_mode();
  if(!screen_mode)
  {
    unsigned int fg = color & 0x0F;
    unsigned int bg = (color & 0xF0) >> 4;

    ssize_t diff;

    // Are the MZX mode colors the same?
    if(fg == bg)
      return FLASH_THING_CLOSE_COLORS;

    // Are the MZX mode colors too close?
    diff = (ssize_t)get_color_luma(fg) - (ssize_t)get_color_luma(bg);
    if(diff >= -64 && diff <= 64)
      return FLASH_THING_CLOSE_COLORS;
  }

  // If one of the chars to display is the board char, is it too close to the
  // flash char?
  if(!editor->flash_char_a || !editor->flash_char_b)
    if(compare_char(chr, (uint16_t)flash_chr + PRO_CH) >= (112*3/4))
      return FLASH_THING_CLOSE_CHAR;

  // SMZX modes should always display protected...
  if(screen_mode)
    return FLASH_THING_SMZX;

  return FLASH_THING_NORMAL;
}

/**
 * Check if the current board element should be flashed. If it should be,
 * flash it. This should be performed after id_put.
 */
static void flash_draw(struct editor_context *editor, struct board *cur_board,
 int x, int y, int array_x, int array_y, int flash_chr)
{
  int offset = array_y * cur_board->board_width + array_x;
  enum thing id = (enum thing)cur_board->level_id[offset];

  if(id >= editor->flash_start && id < editor->flash_start + editor->flash_len)
  {
    uint8_t color = get_id_color(cur_board, offset);
    uint8_t chr = get_id_char(cur_board, offset);
    enum flash_thing_type type;

    type = flash_get_type(editor, cur_board, chr, color, flash_chr);
    if(type != FLASH_THING_NORMAL)
    {
      int avg_luma = get_char_average_luma(chr, color, -1, flash_chr + PRO_CH);

      if(!editor->flash_layer)
      {
        editor->flash_layer = create_layer(0, 0, 80, editor->screen_height,
         LAYER_DRAWORDER_GAME_UI - 10, graphics.protected_pal_position + 5,
         PRO_CH, true);
        set_layer_mode(editor->flash_layer, 0);
      }

      if(type == FLASH_THING_CLOSE_CHAR)
      {
        // Blank out the robot char so there's less visual conflict...
        select_layer(BOARD_LAYER);
        draw_char_ext(' ', color, x, y, PRO_CH, 0);
      }

      color = (avg_luma < 128) ? 0x5F : 0x50;

      select_layer(editor->flash_layer);
      draw_char(flash_chr, color, x, y);
    }
    else
    {
      select_layer(BOARD_LAYER);
      draw_char_ext(flash_chr, color, x, y, PRO_CH, 0);
    }
  }
}

/**
 * Draw the editing window (board version).
 */
static void draw_edit_window(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;
  int viewport_width = 80;
  int viewport_height = editor->screen_height;
  int x, y;
  int a_x, a_y;
  int flash_char = 0;

  if(editor->flashing)
    flash_char = flash_init(editor);

  blank_layers();

  if(viewport_width > cur_board->board_width)
    viewport_width = cur_board->board_width;

  if(viewport_height > cur_board->board_height)
    viewport_height = cur_board->board_height;

  for(y = 0, a_y = editor->scroll_y; y < viewport_height; y++, a_y++)
  {
    for(x = 0, a_x = editor->scroll_x; x < viewport_width; x++, a_x++)
    {
      id_put(cur_board, x, y, a_x, a_y, a_x, a_y);

      // Don't display flashing unless in board edit mode.
      if(flash_char && editor->mode == EDIT_BOARD)
        flash_draw(editor, cur_board, x, y, a_x, a_y, flash_char);
    }
  }
  select_layer(UI_LAYER);

  if(editor->flashing)
    flash_done(editor);
}

/**
 * Draw the editing window (vlayer version).
 */
static void draw_vlayer_window(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  int offset = editor->scroll_x + (editor->scroll_y * mzx_world->vlayer_width);
  int skip;

  char *vlayer_chars = mzx_world->vlayer_chars;
  char *vlayer_colors = mzx_world->vlayer_colors;
  int viewport_height = editor->screen_height;
  int viewport_width = 80;
  int x;
  int y;

  blank_layers();

  select_layer(BOARD_LAYER);

  if(viewport_width > mzx_world->vlayer_width)
    viewport_width = mzx_world->vlayer_width;

  if(viewport_height > mzx_world->vlayer_height)
   viewport_height = mzx_world->vlayer_height;

  skip = mzx_world->vlayer_width - viewport_width;

  for(y = 0; y < viewport_height; y++)
  {
    for(x = 0; x < viewport_width; x++)
    {
      draw_char_ext(vlayer_chars[offset], vlayer_colors[offset], x, y, 0, 0);
      offset++;
    }
    offset += skip;
  }

  select_layer(UI_LAYER);
}

/**
 * Draw the part of the editor viewport that is outside of the board/vlayer.
 */
static void draw_out_of_bounds(int in_x, int in_y, int in_width, int in_height)
{
  int after_x;
  int after_y;
  int after_width;
  int y;

  if(in_x < 0 || in_x >= SCREEN_W || in_y < 0 || in_y >= SCREEN_H)
    return;

  if(in_x + in_width > SCREEN_W)
    in_width = SCREEN_W - in_x;

  if(in_y + in_height > SCREEN_H)
    in_height = SCREEN_H - in_y;

  after_x = in_x + in_width;
  after_y = in_y + in_height;
  after_width = SCREEN_W - after_x;

  // Clear any lines prior to the first line.
  for(y = 0; y < in_y; y++)
    fill_line(SCREEN_W, 0, y, 177, 1);

  // Clear any area before and after the in-bounds area.
  for(; y < after_y; y++)
  {
    if(in_x > 0)
      fill_line(in_x, 0, y, 177, 1);

    if(after_width > 0)
      fill_line(after_width, after_x, y, 177, 1);
  }

  // Clear any lines after the last line.
  for(; y < SCREEN_H; y++)
    fill_line(SCREEN_W, 0, y, 177, 1);
}

/**
 * Draw the editor viewport and handle some other things that need
 * to happen at draw time.
 */
static boolean editor_draw(context *ctx)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);
  struct block_info *block = &(editor->block);
  struct world *mzx_world = ctx->world;
  struct board *cur_board = mzx_world->current_board;

  int cursor_screen_x = editor->cursor_x - editor->scroll_x;
  int cursor_screen_y = editor->cursor_y - editor->scroll_y;
  int saved_overlay_mode = cur_board->overlay_mode;
  int i;

  if(get_fade_status())
    insta_fadein();

  m_show();
  cursor_solid(cursor_screen_x, cursor_screen_y);

  // Draw the board/vlayer
  if(editor->mode == EDIT_BOARD)
  {
    cur_board->overlay_mode |= OVERLAY_FLAG_HIDE_OVERLAY;
    draw_edit_window(editor);
  }
  else

  if(editor->mode == EDIT_OVERLAY)
  {
    cur_board->overlay_mode = OVERLAY_ON;

    if(!editor->show_board_under_overlay)
      cur_board->overlay_mode |= OVERLAY_FLAG_HIDE_BOARD;

    draw_edit_window(editor);
  }
  else // EDIT_VLAYER
  {
    draw_vlayer_window(editor);
  }

  draw_out_of_bounds(0, 0, editor->board_width, editor->board_height);

  cur_board->overlay_mode = saved_overlay_mode;

  // Highlight block for block selection
  if(editor->cursor_mode == CURSOR_BLOCK_SELECT)
  {
    int block_screen_x = block->src_x - editor->scroll_x;
    int block_screen_y = block->src_y - editor->scroll_y;
    int start_x, start_y;
    int block_width, block_height;

    if(block_screen_x < 0)
      block_screen_x = 0;

    if(block_screen_y < 0)
      block_screen_y = 0;

    if(block_screen_x >= 80)
      block_screen_x = 79;

    if(block_screen_y >= editor->screen_height)
      block_screen_y = editor->screen_height - 1;

    if(block_screen_x < cursor_screen_x)
    {
      start_x = block_screen_x;
      block_width = cursor_screen_x - block_screen_x + 1;
    }
    else
    {
      start_x = cursor_screen_x;
      block_width = block_screen_x - cursor_screen_x + 1;
    }

    if(block_screen_y < cursor_screen_y)
    {
      start_y = block_screen_y;
      block_height = cursor_screen_y - block_screen_y + 1;
    }
    else
    {
      start_y = cursor_screen_y;
      block_height = block_screen_y - cursor_screen_y + 1;
    }

    for(i = 0; i < block_height; i++)
    {
      color_line(block_width, start_x, start_y + i, 0x9F);
    }
  }

  if(mzx_world->debug_mode)
  {
    draw_debug_box(mzx_world, editor->debug_x, editor->screen_height - 6,
      editor->cursor_x, editor->cursor_y, 0);
  }

  if(editor->modified != editor->modified_prev)
  {
    caption_set_modified(editor->modified);
    editor->modified_prev = editor->modified;
  }

  // Fix invalid buffer params before they get drawn.
  if(buffer->param == -1)
    default_buffer(editor);

  // Give the edit menu up-to-date information before it is drawn.
  update_edit_menu(editor->edit_menu, editor->mode, editor->cursor_mode,
   editor->cursor_x, editor->cursor_y, editor->screen_height,
   &(editor->buffer), editor->use_default_color);

  return true;
}

/**
 * Cancel drawing with the mouse.
 */
static void cancel_mouse_draw(struct editor_context *editor)
{
  if(editor->continue_mouse_history)
    update_undo_frame(editor->cur_history);

  editor->continue_mouse_history = false;
}

/**
 * Draw at a position with the mouse.
 */
static void mouse_draw_at_position(struct editor_context *editor, int x, int y)
{
  struct buffer_info *buffer = &(editor->buffer);
  struct world *mzx_world = ((context *)editor)->world;
  struct board *cur_board = mzx_world->current_board;
  enum editor_mode mode = editor->mode;

  if(!editor->continue_mouse_history)
  {
    // Add new frame
    switch(mode)
    {
      case EDIT_BOARD:
        add_board_undo_frame(mzx_world, editor->cur_history, buffer, x, y);
        break;

      case EDIT_OVERLAY:
        add_layer_undo_pos_frame(editor->cur_history, cur_board->overlay,
         cur_board->overlay_color, cur_board->board_width, buffer, x, y);
        break;

      case EDIT_VLAYER:
        add_layer_undo_pos_frame(editor->cur_history,
         mzx_world->vlayer_chars, mzx_world->vlayer_colors,
         mzx_world->vlayer_width, buffer, x, y);
        break;
    }
    editor->continue_mouse_history = true;
  }
  else
  {
    // Continue frame
    add_undo_position(editor->cur_history, x, y);
  }

  // Tracking history separately, so don't pass it here.
  buffer->param = place_current_at_xy(mzx_world, buffer, x, y, mode, NULL);
  editor->mouse_last_x = x;
  editor->mouse_last_y = y;
  editor->modified = true;
}

/**
 * Draw to a position with the mouse, starting from the previous position
 * of the mouse if it is being dragged.
 */
static void mouse_draw(struct editor_context *editor, int x, int y)
{
  if(editor->continue_mouse_history)
  {
    // Linear draw from previous position to current
    int mouse_last_x = editor->mouse_last_x;
    int mouse_last_y = editor->mouse_last_y;
    int x_diff = x - mouse_last_x;
    int y_diff = y - mouse_last_y;
    int iter = MAX(abs(x_diff), abs(y_diff));
    int i;

    if(x == mouse_last_x && y == mouse_last_y)
      return;

    // Draw every position up to the current
    for(i = 1; i < iter; i++)
    {
      mouse_draw_at_position(editor,
        mouse_last_x + i * x_diff / iter,
        mouse_last_y + i * y_diff / iter
      );
    }
  }

  // Draw at the current mouse position
  mouse_draw_at_position(editor, x, y);
}

/**
 * Update the editor at the start of each frame.
 */
static boolean editor_idle(context *ctx)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  struct editor_context *editor = (struct editor_context *)ctx;
  struct world *mzx_world = ctx->world;

  find_player(mzx_world);

  // The user may be prompted to create a second board at the start
  // of editing.
  if(editor->first_board_prompt)
  {
    if(add_board(mzx_world, 1) >= 0)
    {
      // Also name the title board for disambiguation.
      strcpy(mzx_world->name, NEW_WORLD_TITLE);
      strcpy(mzx_world->board_list[0]->board_name, NEW_WORLD_TITLE);
      mzx_world->first_board = 1;
      editor_set_current_board(editor, 1, false);
    }

    editor->first_board_prompt = false;
    return true;
  }

  // Save a backup world.
  if(editor_conf->backup_count)
  {
    int ticks_delta = get_ticks() - editor->backup_timestamp;

    if(ticks_delta > (editor_conf->backup_interval * 1000))
    {
      char backup_name_formatted[MAX_PATH];
      int backup_num = editor->backup_num + 1;
      int ret;

      snprintf(backup_name_formatted, MAX_PATH, "%s%d%s",
       editor_conf->backup_name, backup_num, editor_conf->backup_ext);

      // Ensure any subdirectories exist before saving.
      ret = path_create_parent_recursively(editor_conf->backup_name);
      if(ret != PATH_CREATE_SUCCESS)
        warn("Failed to create backup directory for backup_name='%s' (ret=%d)!\n",
         editor_conf->backup_name, ret);

      save_world(mzx_world, backup_name_formatted, false, MZX_VERSION);
      editor->backup_num = backup_num % editor_conf->backup_count;
      editor->backup_timestamp = get_ticks();
    }
  }

  // Create undo history stacks if they don't currently exist.
  if(!editor->board_history)
  {
    editor->board_history =
     construct_board_undo_history(editor_conf->undo_history_size);
  }

  if(!editor->overlay_history)
  {
    editor->overlay_history =
     construct_layer_undo_history(editor_conf->undo_history_size);
  }

  if(!editor->vlayer_history)
  {
    editor->vlayer_history =
     construct_layer_undo_history(editor_conf->undo_history_size);
  }

  fix_history(editor);

  // Preempt inputs to cancel flashing mode if active.
  if(editor->flashing && !editor_conf->editor_show_thing_toggles &&
   (get_mouse_status() || get_exit_status() || get_key(keycode_internal_wrt_numlock)))
  {
    editor->flashing = false;
    return true;
  }

  // Cancel mouse draw if the left mouse button isn't being held.
  if(editor->continue_mouse_history && !get_mouse_held(MOUSE_BUTTON_LEFT))
  {
    cancel_mouse_draw(editor);
  }

  return false;
}

/**
 * Handle mouse clicks and drags for the editor.
 */
static boolean editor_mouse(context *ctx, int *key, int button, int x, int y)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);
  struct world *mzx_world = ctx->world;

  // Allow keys to take precedence
  if(*key)
    return true;

  if(y < editor->screen_height)
  {
    x += editor->scroll_x;
    y += editor->scroll_y;

    editor->cursor_x = x;
    editor->cursor_y = y;
    fix_cursor(editor);

    if((editor->cursor_x == x) && (editor->cursor_y == y))
    {
      // Mouse is in-bounds

      if(get_mouse_held(MOUSE_BUTTON_RIGHT))
      {
        cancel_mouse_draw(editor);
        grab_at_xy(mzx_world, buffer, x, y, editor->mode);
        return true;
      }
      else

      if(get_mouse_held(MOUSE_BUTTON_LEFT))
      {
        // Mouse draw. Only start if this is an initial click to prevent it
        // from carrying through other interfaces.
        if(editor->continue_mouse_history || !get_mouse_drag())
          mouse_draw(editor, x, y);
        return true;
      }
    }
    else

    if(editor->continue_mouse_history && get_mouse_held(MOUSE_BUTTON_LEFT))
    {
      // We want mouse draw to continue even if the cursor goes out of bounds.
      mouse_draw(editor, editor->cursor_x, editor->cursor_y);
      return true;
    }
  }
  return false;
}

/**
 * Handle joystick actions for the editor.
 * Currently support here is fairly weak and only exists to allow basic
 * features like moving the cursor, peeking at robots, or exiting.
 */
static boolean editor_joystick(context *ctx, int *key, int action)
{
  struct editor_context *editor = (struct editor_context *)ctx;

  switch(action)
  {
    // Place
    case JOY_A:
      *key = IKEY_SPACE;
      return true;

    // Delete
    case JOY_B:
      *key = IKEY_DELETE;
      return true;

    // Modify+Grab
    case JOY_X:
      *key = IKEY_RETURN;
      return true;

    // Grab
    case JOY_Y:
      *key = IKEY_INSERT;
      return true;

    // Board menu
    case JOY_START:
      *key = IKEY_b;
      return true;

    // Global info
    case JOY_LSHOULDER:
      *key = IKEY_g;
      return true;

    // Board info
    case JOY_RSHOULDER:
      *key = IKEY_i;
      return true;

    // Previous menu
    case JOY_LTRIGGER:
    {
      if(editor->screen_height != EDIT_SCREEN_NORMAL)
      {
        editor->screen_height = EDIT_SCREEN_NORMAL;
        fix_scroll(editor);
      }
      else
        *key = IKEY_PAGEUP;

      return true;
    }

    // Next menu
    case JOY_RTRIGGER:
    {
      if(editor->screen_height != EDIT_SCREEN_NORMAL)
      {
        editor->screen_height = EDIT_SCREEN_NORMAL;
        fix_scroll(editor);
      }
      else
        *key = IKEY_PAGEDOWN;

      return true;
    }

    // Defaults for select, arrows, etc.
    default:
    {
      enum keycode ui_key = get_joystick_ui_key();
      if(ui_key)
      {
        *key = ui_key;
        return true;
      }
      break;
    }
  }
  return false;
}

/**
 * Edit the parameter of the buffer and update it if the user doesn't cancel.
 */
static void change_buffer_param_callback(context *ctx, context_callback_param *p)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);

  if(is_storageless(buffer->id) && editor->modify_param >= 0)
  {
    buffer->param = editor->modify_param;
  }
}

static void change_buffer_param(struct editor_context *editor)
{
  change_param((context *)editor, &(editor->buffer), &(editor->modify_param));
  context_callback((context *)editor, NULL, change_buffer_param_callback);
}

/**
 * Get the thing at the cursor position on the board and edit it. If it was
 * modified, place it back on the board.
 */
static void get_modify_place_callback(context *ctx, context_callback_param *p)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);
  int new_param = editor->modify_param;

  // Ignore non-storage objects with no change.
  if((new_param >= 0 && new_param != buffer->param) || editor->modified_storage)
  {
    // Update the buffer for the new param.
    buffer->param = new_param;

    // Place the buffer back on the board/layer.
    // Use place_current_at_xy wrapper that ensures the under is untouched.
    new_param = replace_current_at_xy(ctx->world, buffer, editor->cursor_x,
     editor->cursor_y, editor->mode, editor->cur_history);

    // Placement might have required that we change the buffer param again.
    buffer->param = new_param;

    editor->modified = true;
  }
}

static void get_modify_place(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct buffer_info *buffer = &(editor->buffer);

  grab_at_xy(mzx_world, buffer, editor->cursor_x, editor->cursor_y,
   editor->mode);

  if(editor->mode == EDIT_BOARD)
  {
    // Board- param
    editor->modified_storage = !is_storageless(buffer->id);
    buffer->robot->xpos = editor->cursor_x;
    buffer->robot->ypos = editor->cursor_y;

    change_param((context *)editor, buffer, &(editor->modify_param));
  }
  else
  {
    // Overlay and vlayer- char
    editor->modified_storage = false;
    editor->modify_param = char_selection(buffer->param);
  }
  context_callback((context *)editor, NULL, get_modify_place_callback);
}

/**
 * Modifies the thing at a given position without altering the buffer.
 * This is done with a temporary buffer to avoid replicating much of the
 * code of place_current_at_xy.
 */
static void modify_thing_callback(context *ctx, context_callback_param *p)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *temp_buffer = &(editor->temp_buffer);
  int new_param = editor->modify_param;

  // Ignore non-storage objects with no change.
  if((new_param >= 0 && new_param != temp_buffer->param) ||
   editor->modified_storage)
  {
    // Update the buffer for the new param.
    temp_buffer->param = new_param;

    // place_current_at_xy wrapper that ensures the under is untouched.
    replace_current_at_xy(ctx->world, temp_buffer, editor->cursor_x,
     editor->cursor_y, editor->mode, editor->cur_history);

    editor->modified = true;
  }
  free_edit_buffer(temp_buffer);
}

static void modify_thing_at_cursor(struct editor_context *editor)
{
  struct world *mzx_world = ((context *)editor)->world;
  struct buffer_info *temp_buffer = &(editor->temp_buffer);

  memset(temp_buffer, 0, sizeof(struct buffer_info));
  temp_buffer->robot = &(editor->temp_robot);
  temp_buffer->scroll = &(editor->temp_scroll);
  temp_buffer->sensor = &(editor->temp_sensor);

  grab_at_xy(mzx_world, temp_buffer, editor->cursor_x, editor->cursor_y,
   editor->mode);

  if(editor->mode == EDIT_BOARD)
  {
    // Board- param
    editor->modified_storage = !is_storageless(temp_buffer->id);
    temp_buffer->robot->xpos = editor->cursor_x;
    temp_buffer->robot->ypos = editor->cursor_y;

    change_param((context *)editor, temp_buffer, &(editor->modify_param));
  }
  else
  {
    // Overlay and vlayer- char
    editor->modified_storage = false;
    editor->modify_param = char_selection(temp_buffer->param);
  }
  context_callback((context *)editor, NULL, modify_thing_callback);
}

/**
 * Editor keyhandler.
 */
static boolean editor_key(context *ctx, int *key)
{
  struct editor_config_info *editor_conf = get_editor_config();
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);
  struct block_info *block = &(editor->block);
  struct world *mzx_world = ctx->world;
  struct board *cur_board = mzx_world->current_board;

  // Temporary vars
  struct buffer_info temp_buffer;
  boolean exit_status;
  int i;

  memset(&temp_buffer, 0, sizeof(struct buffer_info));

  // If the mouse is trying to draw something, stop it first.
  if(editor->continue_mouse_history)
    cancel_mouse_draw(editor);

  // Exit event - ignore other input
  exit_status = get_exit_status();
  if(exit_status)
    *key = 0;

  // Saved positions
  if((*key >= IKEY_0) && (*key <= IKEY_9))
  {
    char mesg[80];

    int s_num = *key - IKEY_0;
    struct saved_position *s;

    if(editor->mode == EDIT_VLAYER)
      s = &(editor_conf->vlayer_positions[s_num]);
    else
      s = &(editor_conf->saved_positions[s_num]);

    if(get_ctrl_status(keycode_internal) &&
     !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
    {
      if(editor->mode == EDIT_VLAYER)
      {
        sprintf(mesg, "Save vlayer @ %d,%d to pos %d?",
         editor->cursor_x, editor->cursor_y, s_num);
      }
      else
      {
        sprintf(mesg, "Save '%s' @ %d,%d to pos %d?",
         mzx_world->current_board->board_name, editor->cursor_x,
         editor->cursor_y, s_num);
      }

      if(!confirm(mzx_world, mesg))
      {
        s->board_id = mzx_world->current_board_id;
        s->cursor_x = editor->cursor_x;
        s->cursor_y = editor->cursor_y;
        s->scroll_x = editor->scroll_x;
        s->scroll_y = editor->scroll_y;
        s->debug_x = editor->debug_x;
        save_local_editor_config(editor_conf, curr_file);
      }
      return true;
    }
    else

    if(get_alt_status(keycode_internal) &&
     !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
    {
      int s_board = s->board_id;

      if(editor->mode == EDIT_VLAYER || (s_board < mzx_world->num_boards &&
       mzx_world->board_list[s_board]))
      {
        if(editor->mode == EDIT_VLAYER)
        {
          sprintf(mesg, "Go to vlayer @ %d,%d (pos %d)?",
           s->cursor_x, s->cursor_y, s_num);
        }
        else
        {
          sprintf(mesg, "Go to '%s' @ %d,%d (pos %d)?",
           mzx_world->board_list[s_board]->board_name,
           s->cursor_x, s->cursor_y, s_num);
        }

        if(!confirm(mzx_world, mesg))
        {
          editor->cursor_x = s->cursor_x;
          editor->cursor_y = s->cursor_y;
          editor->scroll_x = s->scroll_x;
          editor->scroll_y = s->scroll_y;
          editor->debug_x = s->debug_x;

          if(editor->mode != EDIT_VLAYER && mzx_world->current_board_id != s_board)
          {
            editor_set_current_board(editor, s_board, true);
          }
          else
          {
            // The saved position might be out of bounds. Board changing
            // fixes this separately.
            fix_scroll(editor);
          }
        }
      }
      return true;
    }
  }

  switch(*key)
  {
    case IKEY_UP:
    {
      int move_y = -1;

      if(get_shift_status(keycode_internal))
      {
        // Move to adjacent board
        if(cur_board->board_dir[0] != NO_BOARD && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, cur_board->board_dir[0], true);
        return true;
      }
      else

      if(get_alt_status(keycode_internal))
        move_y = -10;

      if(get_ctrl_status(keycode_internal) && block->selected)
        move_y = -block->height;

      move_edit_cursor(editor, 0, move_y);
      return true;
    }

    case IKEY_DOWN:
    {
      int move_y = 1;

      if(get_shift_status(keycode_internal))
      {
        // Move to adjacent board
        if(cur_board->board_dir[1] != NO_BOARD && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, cur_board->board_dir[1], true);
        return true;
      }

      if(get_alt_status(keycode_internal))
        move_y = 10;

      if(get_ctrl_status(keycode_internal) && block->selected)
        move_y = block->height;

      move_edit_cursor(editor, 0, move_y);
      return true;
    }

    case IKEY_LEFT:
    {
      int move_x = -1;

      if(get_shift_status(keycode_internal))
      {
        // Move to adjacent board
        if(cur_board->board_dir[3] != NO_BOARD && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, cur_board->board_dir[3], true);
        return true;
      }

      if(get_alt_status(keycode_internal))
        move_x = -10;

      if(get_ctrl_status(keycode_internal) && block->selected)
        move_x = -block->width;

      move_edit_cursor(editor, move_x, 0);
      return true;
    }

    case IKEY_RIGHT:
    {
      int move_x = 1;

      if(get_shift_status(keycode_internal))
      {
        // Move to adjacent board
        if(cur_board->board_dir[2] != NO_BOARD && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, cur_board->board_dir[2], true);
        return true;
      }

      if(get_alt_status(keycode_internal))
        move_x = 10;

      if(get_ctrl_status(keycode_internal) && block->selected)
        move_x = block->width;

      move_edit_cursor(editor, move_x, 0);
      return true;
    }

    case IKEY_SPACE:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        /* There are two editor placement modes. This behavior can be
         * toggled with the "editor_space_toggles" config.txt option.
         *
         * The legacy mode (editor_space_toggles = 1) allows users to
         * toggle between their param buffer and space, under any
         * situation.
         *
         * The new mode (editor_space_toggles = 0) disallows accidental
         * rewriting of robots (you must press Delete instead) and otherwise
         * implicitly writes the param. This makes placing new blocks with
         * the same ID (but different colour, say) faster.
         */
        int offset =
         editor->cursor_x + (editor->cursor_y * editor->board_width);

        /* If it's not an overlay edit, and we're placing an identical
         * block, and the user has selected legacy placement mode, toggle
         * the current param type with SPACE.
         */
        int at_match = (editor->mode == EDIT_BOARD) &&
         (buffer->id == cur_board->level_id[offset]) &&
         (buffer->color == cur_board->level_color[offset]);

        if(at_match && editor_conf->editor_space_toggles)
        {
          temp_buffer.id = SPACE;
          temp_buffer.param = 0;
          temp_buffer.color = 7;
          place_current_at_xy(mzx_world, &temp_buffer,
           editor->cursor_x, editor->cursor_y, editor->mode,
           editor->cur_history);
        }
        else
        {
          /* If what we're trying to overwrite is a robot, allow the
           * user to overwrite it only if legacy placement is enabled.
           */
          int at_robot = (editor->mode == EDIT_BOARD) &&
           is_robot(cur_board->level_id[offset]);

          if(!at_robot || editor_conf->editor_space_toggles)
          {
            buffer->param = place_current_at_xy(mzx_world, buffer,
             editor->cursor_x, editor->cursor_y, editor->mode,
             editor->cur_history);
          }
        }
      }
      else
      {
        place_text(editor);
      }
      editor->modified = true;
      return true;
    }

    case IKEY_BACKSPACE:
    {
      if(editor->cursor_mode == CURSOR_TEXT)
      {
        if(editor->cursor_x)
          editor->cursor_x--;

        if(((editor->cursor_x - editor->scroll_x) < 5) && editor->scroll_x)
          editor->scroll_x--;

        temp_buffer.id = TEXT_ID;
        temp_buffer.param = ' ';
        temp_buffer.color = buffer->color;
        place_current_at_xy(mzx_world, &temp_buffer,
         editor->cursor_x, editor->cursor_y, editor->mode,
         editor->cur_history);

        editor->modified = true;
      }
      else
      {
        int offset =
         editor->cursor_x + (editor->cursor_y * editor->board_width);

        if(editor->mode != EDIT_BOARD)
        {
          // Delete under cursor (overlay and vlayer)
          temp_buffer.id = SPACE;
          temp_buffer.param = 32;
          temp_buffer.color = 7;
          place_current_at_xy(mzx_world, &temp_buffer,
           editor->cursor_x, editor->cursor_y, editor->mode,
           editor->cur_history);

          editor->modified = true;
        }
        else

        if(cur_board->level_id[offset] != PLAYER)
        {
          // Remove from top (board only)
          // Actually just places the under on top, but it's functionally
          // equivalent.
          enum thing id = cur_board->level_under_id[offset];
          int color = cur_board->level_under_color[offset];
          int param = cur_board->level_under_param[offset];

          add_block_undo_frame(mzx_world, editor->board_history,
           cur_board, offset, 1, 1);

          place_at_xy(mzx_world, id, color, param,
           editor->cursor_x, editor->cursor_y);

          update_undo_frame(editor->board_history);
          editor->modified = true;
        }
      }
      return true;
    }

    case IKEY_TAB:
    {
      if(!get_alt_status(keycode_internal))
      {
        if(editor->cursor_mode)
        {
          editor->cursor_mode = CURSOR_PLACE;
          block->selected = false;
        }
        else
        {
          buffer->param = place_current_at_xy(mzx_world, buffer,
           editor->cursor_x, editor->cursor_y, editor->mode,
           editor->cur_history);

          editor->cursor_mode = CURSOR_DRAW;
          editor->modified = true;
        }
      }

      return true;
    }

    case IKEY_INSERT:
    {
      grab_at_xy(mzx_world, buffer, editor->cursor_x, editor->cursor_y,
       editor->mode);
      return true;
    }

    case IKEY_DELETE:
    {
      if(editor->cursor_mode == CURSOR_TEXT)
      {
        temp_buffer.id = TEXT_ID;
        temp_buffer.param = ' ';
        temp_buffer.color = buffer->color;
        place_current_at_xy(mzx_world, &temp_buffer,
         editor->cursor_x, editor->cursor_y, editor->mode,
         editor->cur_history);
      }
      else
      {
        int new_param = 0;

        // On layers, the blank char is a space char
        if(editor->mode != EDIT_BOARD)
          new_param = 32;

        temp_buffer.id = SPACE;
        temp_buffer.param = new_param;
        temp_buffer.color = 7;
        place_current_at_xy(mzx_world, &temp_buffer,
         editor->cursor_x, editor->cursor_y, editor->mode,
         editor->cur_history);
      }
      editor->modified = true;
      return true;
    }

    case IKEY_HOME:
    {
      editor->cursor_x = 0;
      editor->cursor_y = 0;
      fix_scroll(editor);
      return true;
    }

    case IKEY_END:
    {
      editor->cursor_x = editor->board_width - 1;
      editor->cursor_y = editor->board_height - 1;
      fix_scroll(editor);
      return true;
    }

    case IKEY_ESCAPE:
    {
      if(editor->cursor_mode)
      {
        editor->cursor_mode = CURSOR_PLACE;
        block->selected = false;
        key = 0;
        return true;
      }
      else

      if(editor->mode)
      {
        // Exit overlay/vlayer editing
        set_editor_mode(editor, EDIT_BOARD);
        synchronize_board_values(editor);
        fix_scroll(editor);
        key = 0;
        return true;
      }
      else

      if(editor->flashing)
      {
        editor->flashing = false;
        return true;
      }

      // Defer to exit handler.
      exit_status = true;
      break;
    }

    case IKEY_F1:
    {
      if(get_shift_status(keycode_internal))
      {
        // Show invisible walls
        if(editor->mode == EDIT_BOARD)
        {
          flash_thing(editor, INVIS_WALL, INVIS_WALL, 178, 176);
        }
        return true;
      }
      break;
    }

    case IKEY_F2:
    {
      // Ignore Alt+F2, Ctrl+F2
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
        break;

      if(get_shift_status(keycode_internal))
      {
        // Show robots
        if(editor->mode == EDIT_BOARD)
        {
          flash_thing(editor, ROBOT_PUSHABLE, ROBOT, '!', 0);
        }
      }
      else
      {
        // Toggle text mode
        if(editor->cursor_mode != CURSOR_TEXT)
        {
          block->selected = false;
          editor->cursor_mode = CURSOR_TEXT;
          editor->text_start_x = editor->cursor_x;
        }
        else
        {
          editor->cursor_mode = CURSOR_PLACE;
        }
      }
      return true;
    }

    // Terrain
    case IKEY_F3:
    {
      if(editor->mode == EDIT_BOARD)
      {
        if(get_shift_status(keycode_internal))
        {
          // Show fakes
          flash_thing(editor, FAKE, THICK_WEB, '#', 177);
        }
        else
        {
          thing_menu(ctx, THING_MENU_TERRAIN, buffer,
           editor->use_default_color, editor->cursor_x, editor->cursor_y,
           editor->board_history);
          editor->modified = true;
        }
      }
      return true;
    }

    // Item
    case IKEY_F4:
    {
      // ALT+F4 - do nothing.
      if(get_alt_status(keycode_internal))
        break;

      if(editor->mode == EDIT_BOARD)
      {
        if(get_shift_status(keycode_internal))
        {
          // Show spaces
          flash_thing(editor, SPACE, SPACE, 'O', '*');
        }
        else
        {
          thing_menu(ctx, THING_MENU_ITEMS, buffer,
           editor->use_default_color, editor->cursor_x, editor->cursor_y,
           editor->board_history);
          editor->modified = true;
        }
      }
      return true;
    }

    // Creature
    case IKEY_F5:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_CREATURES, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    // Puzzle
    case IKEY_F6:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_PUZZLE, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    // Transport
    case IKEY_F7:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_TRANSPORT, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    // Element
    case IKEY_F8:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_ELEMENTS, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    // Misc
    case IKEY_F9:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_MISC, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    // Object
    case IKEY_F10:
    {
      if(editor->mode == EDIT_BOARD)
      {
        thing_menu(ctx, THING_MENU_OBJECTS, buffer,
         editor->use_default_color, editor->cursor_x, editor->cursor_y,
         editor->board_history);
        editor->modified = true;
      }
      return true;
    }

    case IKEY_F11:
    {
      if(get_alt_status(keycode_internal))
      {
        // Robot debugger configuration
        debug_robot_config(mzx_world);
      }
      else
      {
        // SMZX Mode
        int selected_mode = select_screen_mode(mzx_world);

        if(selected_mode >= 0)
        {
          set_screen_mode(selected_mode);

          editor->modified = true;
        }
      }
      return true;
    }

    case IKEY_F12:
    {
      // don't block in text mode :(
      break;
    }

    case IKEY_8:
    case IKEY_KP_MULTIPLY:
    {
      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }
      else

      if(get_shift_status(keycode_internal) ||
       (*key == IKEY_KP_MULTIPLY))
      {
        cur_board->mod_playing[0] = '*';
        cur_board->mod_playing[1] = 0;
        fix_mod(editor);
        edit_menu_show_board_mod(editor->edit_menu);

        editor->modified = true;
      }

      return true;
    }

    case IKEY_KP_MINUS:
    case IKEY_MINUS:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Previous board
        // Doesn't make sense on the vlayer.
        if(mzx_world->current_board_id > 0 && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, mzx_world->current_board_id - 1, true);
      }
      else
        place_text(editor);

      return true;
    }

    case IKEY_KP_PLUS:
    case IKEY_EQUALS:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Next board
        // Doesn't make sense on the vlayer.
        if(mzx_world->current_board_id < mzx_world->num_boards - 1
         && editor->mode != EDIT_VLAYER)
          editor_set_current_board(editor, mzx_world->current_board_id + 1, true);
      }
      else
        place_text(editor);

      return true;
    }

    case IKEY_RETURN:
    {
      if(editor->cursor_mode == CURSOR_BLOCK_SELECT)
      {
        // Block selection
        if(!select_block_command(mzx_world, block, editor->mode))
          return true;

        // Correct the coordinates so x,y are the lowest values
        if(block->src_x > editor->cursor_x)
        {
          block->width = block->src_x - editor->cursor_x + 1;
          block->src_x = editor->cursor_x;
        }
        else
        {
          block->width = editor->cursor_x - block->src_x + 1;
        }

        if(block->src_y > editor->cursor_y)
        {
          block->height = block->src_y - editor->cursor_y + 1;
          block->src_y = editor->cursor_y;
        }
        else
        {
          block->height = editor->cursor_y - block->src_y + 1;
        }

        // Several commands have the same source and destination.
        block->src_board = cur_board;
        block->dest_board = cur_board;
        block->dest_x = block->src_x;
        block->dest_y = block->src_y;

        if(block->dest_mode == EDIT_OVERLAY && !cur_board->overlay_mode)
        {
          error("Overlay mode is not on (see Board Info)",
           ERROR_T_WARNING, ERROR_OPT_OK, 0x1103);
          editor->cursor_mode = CURSOR_PLACE;
          block->selected = false;
          return true;
        }

        // Some of these are done automatically
        switch(block->command)
        {
          case BLOCK_CMD_NONE:
          {
            editor->cursor_mode = CURSOR_PLACE;
            block->selected = false;
            break;
          }

          case BLOCK_CMD_COPY:
          {
            if(block->dest_mode != block->src_mode)
            {
              set_editor_mode(editor, block->dest_mode);
              synchronize_board_values(editor);
              fix_scroll(editor);
            }
          }

          /* fall-through */

          case BLOCK_CMD_COPY_REPEATED:
          case BLOCK_CMD_MOVE:
          {
            editor->cursor_mode = CURSOR_BLOCK_PLACE;
            break;
          }

          case BLOCK_CMD_CLEAR:
          case BLOCK_CMD_MIRROR:
          case BLOCK_CMD_FLIP:
          case BLOCK_CMD_PAINT:
          {
            do_block_command(mzx_world, block, editor->cur_history,
             editor->mzm_name_buffer, buffer->color);

            block->selected = false;
            editor->cursor_mode = CURSOR_PLACE;
            editor->modified = true;
            break;
          }

          case BLOCK_CMD_SAVE_MZM:
          {
            // Save as MZM
            char export_name[MAX_PATH];
            strcpy(export_name, editor->mzm_name_buffer);

            if(!new_file(mzx_world, mzm_ext, ".mzm", export_name,
             "Export MZM", ALLOW_ALL_DIRS))
            {
              enum mzm_save_mode save_mode;

              switch(editor->mode)
              {
                default:
                case EDIT_BOARD:
                  save_mode = MZM_BOARD_TO_BOARD_STORAGE;
                  break;

                case EDIT_OVERLAY:
                  save_mode = MZM_OVERLAY_TO_LAYER_STORAGE;
                  break;

                case EDIT_VLAYER:
                  save_mode = MZM_VLAYER_TO_LAYER_STORAGE;
                  break;
              }
              strcpy(editor->mzm_name_buffer, export_name);
              save_mzm(mzx_world, editor->mzm_name_buffer, block->src_x,
               block->src_y, block->width, block->height, save_mode, false);
            }
            block->selected = false;
            editor->cursor_mode = CURSOR_PLACE;
            break;
          }

          case BLOCK_CMD_SAVE_ANSI:
          {
            // Save as ANSi.
            char title[ARRAY_SIZE(editor->ansi_save_title)];
            char author[ARRAY_SIZE(editor->ansi_save_author)];
            char export_name[MAX_PATH];
            int text_only = editor->ansi_save_text_only;
            int doorway_mode = editor->ansi_save_doorway_mode;
            static const char *radio_strings[] =
            {
              "Save ANSi",
              "Save TXT"
            };
            static const char *checkbox_strings[] =
            {
              "Doorway mode"
            };
            struct element *elements[] =
            {
              construct_radio_button(4, 19, radio_strings, ARRAY_SIZE(radio_strings),
               9, &text_only),
              construct_input_box(23, 19, "Title (ANSi):", ARRAY_SIZE(title) - 1, title),
              construct_input_box(22, 20, "Author (ANSi):", ARRAY_SIZE(author) - 1, author),
              construct_check_box(58, 20, checkbox_strings, ARRAY_SIZE(checkbox_strings),
               12, &doorway_mode),
            };

            memcpy(export_name, editor->mzm_name_buffer, MAX_PATH);
            memcpy(title, editor->ansi_save_title, ARRAY_SIZE(title));
            memcpy(author, editor->ansi_save_author, ARRAY_SIZE(author));

            if(!file_manager(mzx_world, ans_ext, NULL, export_name,
             "Export ANSi or TXT", 1, 1, elements, ARRAY_SIZE(elements), 3))
            {
              path_force_ext(export_name, sizeof(export_name),
               text_only ? ".txt" : ".ans");
              memcpy(editor->mzm_name_buffer, export_name, MAX_PATH);
              memcpy(editor->ansi_save_title, title, ARRAY_SIZE(title));
              memcpy(editor->ansi_save_author, author, ARRAY_SIZE(author));
              editor->ansi_save_text_only = text_only;
              editor->ansi_save_doorway_mode = doorway_mode;

              export_ansi(mzx_world, export_name, editor->mode, block->src_x,
               block->src_y, block->width, block->height, text_only, doorway_mode,
               title, author);
            }
            block->selected = false;
            editor->cursor_mode = CURSOR_PLACE;
            break;
          }

          case BLOCK_CMD_LOAD_MZM:
          case BLOCK_CMD_LOAD_ANSI:
          {
            // ignore
            break;
          }
        }
      }
      else

      if(editor->cursor_mode == CURSOR_BLOCK_PLACE ||
       editor->cursor_mode == CURSOR_MZM_PLACE ||
       editor->cursor_mode == CURSOR_ANSI_PLACE)
      {
        // Block placement
        block->dest_board = cur_board;
        block->dest_x = editor->cursor_x;
        block->dest_y = editor->cursor_y;
        block->convert_id = CUSTOM_BLOCK;

        if(block->dest_mode == EDIT_BOARD && block->src_mode != EDIT_BOARD)
        {
          block->convert_id = layer_to_board_object_type(mzx_world);
          if(block->convert_id == NO_ID)
            break;
        }

        do_block_command(mzx_world, block, editor->cur_history,
         editor->mzm_name_buffer, 0);

        // Reset the block mode unless this is repeat copy
        if(block->command != BLOCK_CMD_COPY_REPEATED)
        {
          block->selected = false;
          editor->cursor_mode = CURSOR_PLACE;
        }

        editor->modified = true;
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        // Go to next line
        editor->cursor_x = editor->text_start_x;

        if(editor->cursor_y < (editor->board_height - 1))
          editor->cursor_y++;

        fix_scroll(editor);
      }

      // Normal/draw - modify+get
      else
      {
        get_modify_place(editor);
      }
      return true;
    }

    case IKEY_a:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        int charset_load = choose_char_set(mzx_world);

        switch(charset_load)
        {
          case 0:
          {
            ec_load_mzx();
            break;
          }

          case 1:
          {
            ec_load_ascii();
            break;
          }

          case 2:
          {
            ec_load_smzx();
            break;
          }

          case 3:
          {
            ec_load_smzx2();
            break;
          }

          case 4:
          {
            ec_load_blank();
            break;
          }
        }

        editor->modified = true;
      }
      else

      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Add board, find first free
        for(i = 0; i < mzx_world->num_boards; i++)
        {
          if(mzx_world->board_list[i] == NULL)
            break;
        }

        if(i < MAX_BOARDS)
        {
          if(add_board(mzx_world, i) >= 0)
            editor_set_current_board(editor, i, false);
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_b:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          block->src_x = editor->cursor_x;
          block->src_y = editor->cursor_y;
          editor->cursor_mode = CURSOR_BLOCK_SELECT;
        }
        else
        {
          int change_to_board =
           choose_board(mzx_world, mzx_world->current_board_id,
           "Select current board:", 0);

          editor_set_current_board(editor, change_to_board, true);
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_c:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        cursor_off();
        if(get_alt_status(keycode_internal))
        {
          char_editor(mzx_world);
          editor->modified = true;
        }
        else
        {
          int new_color = color_selection(buffer->color, 0);
          if(new_color >= 0)
            buffer->color = new_color;
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_d:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          // Toggle default built-in colors
          editor->use_default_color ^= 1;
        }
        else
        {
          int del_board =
           choose_board(mzx_world, mzx_world->current_board_id,
           "Delete board:", 0);

          if((del_board > 0) &&
           !confirm(mzx_world, "DELETE BOARD - Are you sure?"))
          {
            int current_board_id = mzx_world->current_board_id;
            clear_board(mzx_world->board_list[del_board]);
            mzx_world->board_list[del_board] = NULL;

            // Remove null board from list
            optimize_null_boards(mzx_world);

            if(del_board == current_board_id)
            {
              mzx_world->current_board = NULL;
              editor_set_current_board(editor, 0, true);
            }
            else
            {
              synchronize_board_values(editor);
            }
          }
        }

        editor->modified = true;
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_e:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          palette_editor(ctx);
          editor->modified = true;
        }
      }
      else
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_f:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          sfx_edit(mzx_world);
          editor->modified = true;
        }
        else
        {
          fill_area(mzx_world, buffer, editor->cursor_x, editor->cursor_y,
           editor->mode, editor->cur_history);

          editor->modified = true;
        }
      }
      else
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_g:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Goto board position
          if(!board_goto(mzx_world, editor->mode,
           &(editor->cursor_x), &(editor->cursor_y)))
          {
            // This will get bounded by fix_scroll
            editor->scroll_x = editor->cursor_x - 39;
            editor->scroll_y = editor->cursor_y - editor->screen_height / 2;
            fix_scroll(editor);
          }
          return true;
        }
        else

        if(get_alt_status(keycode_internal))
        {
          edit_global_robot(ctx);
        }

        else
        {
          global_info(ctx);
        }

        editor->modified = true;
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_h:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(editor->screen_height == EDIT_SCREEN_NORMAL)
        {
          editor->screen_height = EDIT_SCREEN_MINIMAL;
        }
        else
        {
          editor->screen_height = EDIT_SCREEN_NORMAL;
        }
        fix_scroll(editor);
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_i:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        int import_number = import_type(mzx_world);
        if(import_number >= 0)
        {
          char import_name[MAX_PATH];
          import_name[0] = 0;

          switch(import_number)
          {
            case 0:
            {
              strcpy(import_name, editor->mzb_name_buffer);
              if(!choose_file(mzx_world, mzb_ext, import_name,
               "Choose board to import", ALLOW_ALL_DIRS))
              {
                strcpy(editor->mzb_name_buffer, import_name);
                replace_current_board(mzx_world, import_name);

                // Exit vlayer mode if necessary.
                set_editor_mode(editor, EDIT_BOARD);
                synchronize_board_values(editor);
                fix_world_title(editor);
                fix_scroll(editor);
                fix_caption(editor);

                // This check works because cur_board is still the old pointer.
                if(block->selected && (block->src_board == cur_board))
                {
                  block->selected = false;
                  editor->cursor_mode = CURSOR_PLACE;
                }

                // Need the new pointer here, though.
                cur_board = mzx_world->current_board;
                if(strcmp(cur_board->mod_playing, "*") &&
                 strcasecmp(cur_board->mod_playing,
                 mzx_world->real_mod_playing))
                  fix_mod(editor);

                clear_board_history(editor);
                clear_overlay_history(editor);

                editor->modified = true;
              }
              break;
            }

            case 1:
            {
              // Character set
              int char_offset = 0;
              struct element *elements[] =
              {
                construct_number_box(21, 20, "Offset:  ",
                 0, (PRO_CH - 1), NUMBER_BOX, &char_offset),
              };

              strcpy(import_name, editor->chr_name_buffer);
              if(!file_manager(mzx_world, chr_ext, NULL, import_name,
               "Choose character set to import", ALLOW_ALL_DIRS, NO_NEW_FILES,
               elements, ARRAY_SIZE(elements), 2))
              {
                strcpy(editor->chr_name_buffer, import_name);
                ec_load_set_var(import_name, char_offset, MZX_VERSION);
              }
              editor->modified = true;
              break;
            }

            case 2:
            {
              // World file
              if(!choose_file(mzx_world, world_ext, import_name,
               "Choose world to import", ALLOW_ALL_DIRS))
              {
                // FIXME: Check retval?
                append_world(mzx_world, import_name);
              }

              if(block->selected)
              {
                block->selected = false;
                editor->cursor_mode = CURSOR_PLACE;
              }

              editor->modified = true;
              break;
            }

            case 3:
            {
              // Palette (let the palette editor handle this one)
              import_palette(ctx);
              break;
            }

            case 4:
            {
              // Sound effects
              import_sfx(ctx, &editor->modified);
              break;
            }

            case 5:
            {
              // ANSi file.
              int wrap_width = editor->ansi_line_wrap_column;
              int enable_wrap = editor->ansi_line_wrap;
              static const char *labels[] = { "Force wrap" };
              struct element *elements[] =
              {
                construct_check_box(14, 20, labels, 1, strlen(labels[0]), &enable_wrap),
                construct_number_box(37, 20, "Wrap width: ", 1, 32767, NUMBER_BOX, &wrap_width)
              };

              strcpy(import_name, editor->mzm_name_buffer);
              if(!file_manager(mzx_world, ans_ext, NULL, import_name,
               "Choose ANSi or TXT file to import", 1, 0,
               elements, ARRAY_SIZE(elements), 2))
              {
                int width = -1;
                int height = -1;
                editor->ansi_line_wrap = enable_wrap;
                editor->ansi_line_wrap_column = wrap_width;

                if(validate_ansi(import_name, enable_wrap ? wrap_width : -1,
                 &width, &height) && width > 0 && height > 0)
                {
                  strcpy(editor->mzm_name_buffer, import_name);
                  editor->cursor_mode = CURSOR_ANSI_PLACE;
                  block->command = BLOCK_CMD_LOAD_ANSI;
                  block->selected = true;
                  block->src_board = NULL;
                  block->src_mode = EDIT_OVERLAY; // Force ID selection for board.
                  block->dest_mode = editor->mode;
                  block->width = width;
                  block->height = height;
                }
              }
              break;
            }

            case 6:
            {
              // MZM file
              strcpy(import_name, editor->mzm_name_buffer);
              if(!choose_file(mzx_world, mzm_ext, import_name,
               "Choose image file to import", ALLOW_ALL_DIRS))
              {
                struct mzm_header mzm;
                if(load_mzm_header(import_name, &mzm))
                {
                  strcpy(editor->mzm_name_buffer, import_name);
                  editor->cursor_mode = CURSOR_MZM_PLACE;
                  block->command = BLOCK_CMD_LOAD_MZM;
                  block->selected = true;
                  block->src_board = NULL;
                  block->src_mode = editor->mode;
                  block->dest_mode = editor->mode;
                  block->width = mzm.width;
                  block->height = mzm.height;

                  // Enable conversion ID selection menu upon placement.
                  if(editor->mode == EDIT_BOARD &&
                   mzm.storage_mode == MZM_STORAGE_MODE_LAYER)
                    block->src_mode = EDIT_OVERLAY;
                }
              }
              break;
            }
          }
        }
      }
      else

      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(editor->mode != EDIT_VLAYER)
        {
          board_info(mzx_world);

          synchronize_board_values(editor);
          fix_world_title(editor);
          fix_caption(editor);

          if(!cur_board->overlay_mode && editor->mode == EDIT_OVERLAY)
          {
            clear_overlay_history(editor);
            editor->mode = EDIT_BOARD;
          }

          editor->modified = true;
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_l:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        char test_wav[MAX_PATH] = { 0, };

        if(!choose_file(mzx_world, sam_ext, test_wav,
         "Choose a wav file", ALLOW_ALL_DIRS))
        {
          audio_play_sample(test_wav, false, 0);
        }
      }
      else

      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(!editor->modified || !confirm(mzx_world,
         "Load: World has not been saved, are you sure?"))
        {
          char prev_world[MAX_PATH];
          char load_world[MAX_PATH];
          strcpy(prev_world, editor->current_world);
          strcpy(load_world, editor->current_world);

          if(!choose_file_ch(mzx_world, world_ext, load_world,
           "Load World", ALLOW_ALL_DIRS))
          {
            // Load world curr_file
            strcpy(editor->current_world, load_world);
            if(!editor_reload_world(editor, load_world))
            {
              strcpy(editor->current_world, prev_world);

              fix_mod(editor);
              return true;
            }
            strcpy(curr_file, load_world);

            mzx_world->current_board_id = mzx_world->first_board;
            set_current_board_ext(mzx_world,
             mzx_world->board_list[mzx_world->current_board_id]);
            cur_board = mzx_world->current_board;

            insta_fadein();

            // Switch to board editing if necessary.
            if(!cur_board->overlay_mode || editor->mode == EDIT_VLAYER)
              set_editor_mode(editor, EDIT_BOARD);

            synchronize_board_values(editor);
            fix_scroll(editor);
            fix_mod(editor);
            fix_caption(editor);

            if(block->selected)
            {
              block->selected = false;
              editor->cursor_mode = CURSOR_PLACE;
            }

            clear_board_history(editor);
            clear_overlay_history(editor);
            clear_vlayer_history(editor);

            editor->modified = false;
          }
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_m:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Modify
        if(get_alt_status(keycode_internal))
        {
          modify_thing_at_cursor(editor);
        }
        else

        // Move current board
        // Doesn't make sense on the vlayer
        if(editor->mode != EDIT_VLAYER)
        {
          int new_position;

          if(mzx_world->current_board_id == 0)
            return true;

          new_position =
           choose_board(mzx_world, mzx_world->current_board_id,
           "Move board to position:", 0);

          if((new_position > 0) && (new_position < mzx_world->num_boards) &&
           (new_position != mzx_world->current_board_id))
          {
            move_current_board(mzx_world, new_position);
            editor_set_current_board(editor, new_position, true);
            editor->modified = true;
          }
        }
      }
      else
        place_text(editor);

      return true;
    }

    case IKEY_n:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Board mod
        // Doesn't make sense on the vlayer
        if(get_alt_status(keycode_internal) && editor->mode != EDIT_VLAYER)
        {
          if(!cur_board->mod_playing[0])
          {
            char new_mod[MAX_PATH] = { 0 };

            if(!choose_file(mzx_world, mod_ext, new_mod,
             "Choose a module file", ALLOW_SUBDIRS))
            {
              const char *ext_pos = new_mod + strlen(new_mod) - 4;
              if(ext_pos >= new_mod && !strcasecmp(ext_pos, ".WAV"))
              {
                error("Using OGG instead of WAV is recommended.",
                 ERROR_T_WARNING, ERROR_OPT_OK, 0xc0c5);
              }

              strcpy(cur_board->mod_playing, new_mod);
              strcpy(mzx_world->real_mod_playing, new_mod);
              fix_mod(editor);
              edit_menu_show_board_mod(editor->edit_menu);
            }
          }
          else
          {
            cur_board->mod_playing[0] = 0;
            mzx_world->real_mod_playing[0] = 0;
            fix_mod(editor);
            edit_menu_show_board_mod(editor->edit_menu);
          }
          editor->modified = true;
        }
        else

        if(get_ctrl_status(keycode_internal))
        {
          if(!editor->listening_mod_active)
          {
            char current_dir[MAX_PATH];
            char new_mod[MAX_PATH] = { 0 } ;

            vgetcwd(current_dir, MAX_PATH);
            vchdir(editor->current_listening_dir);

            if(!choose_file(mzx_world, mod_ext, new_mod,
             "Choose a module file (listening only)", ALLOW_ALL_DIRS))
            {
              audio_play_module(new_mod, false, 255);
              strcpy(editor->current_listening_mod, new_mod);
              path_get_directory(editor->current_listening_dir, MAX_PATH,
               new_mod);
              editor->listening_mod_active = true;
            }

            vchdir(current_dir);
          }
          else
          {
            audio_end_module();
            editor->listening_mod_active = false;

            fix_mod(editor);
          }
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_o:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          if(editor->mode != EDIT_OVERLAY)
          {
            if(!cur_board->overlay_mode)
            {
              error("Overlay mode is not on (see Board Info)",
               ERROR_T_WARNING, ERROR_OPT_OK, 0x1103);
            }
            else
            {
              set_editor_mode(editor, EDIT_OVERLAY);
              synchronize_board_values(editor);
              fix_scroll(editor);

              editor->cursor_mode = CURSOR_PLACE;
              block->selected = false;
            }
          }
          else
          {
            set_editor_mode(editor, EDIT_BOARD);
            editor->cursor_mode = CURSOR_PLACE;
            block->selected = false;
          }
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_p:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          // Size and position
          if(editor->mode == EDIT_VLAYER)
          {
            if(size_pos_vlayer(mzx_world))
              clear_vlayer_history(editor);
          }
          else
          {
            if(size_pos(mzx_world))
            {
              set_update_done_current(mzx_world);
              clear_board_history(editor);
              clear_overlay_history(editor);
            }
          }

          synchronize_board_values(editor);
          fix_scroll(editor);

          if(block->selected)
          {
            editor->cursor_mode = CURSOR_PLACE;
            block->selected = false;
          }

          // Uh oh, we might need a new player
          if((mzx_world->player_x >= cur_board->board_width) ||
           (mzx_world->player_y >= cur_board->board_height))
            replace_player(mzx_world);

          editor->modified = true;
        }
        else

        if(editor->mode == EDIT_BOARD)
        {
          // Board- buffer param
          if(is_storageless(buffer->id))
          {
            change_buffer_param(editor);
          }
        }

        else
        {
          // Layer- buffer char
          int new_param = char_selection(buffer->param);

          if(new_param >= 0)
            buffer->param = new_param;
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_r:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // Clear world
        if(!confirm(mzx_world, "Clear ALL - Are you sure?"))
        {
          clear_world(mzx_world);
          create_blank_world(mzx_world);

          if(block->selected)
          {
            editor->cursor_mode = CURSOR_PLACE;
            block->selected = false;
          }

          set_editor_mode(editor, EDIT_BOARD);
          synchronize_board_values(editor);
          fix_scroll(editor);
          fix_caption(editor);

          clear_board_history(editor);
          clear_overlay_history(editor);
          clear_vlayer_history(editor);

          audio_end_module();

          editor->modified = true;
        }
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_s:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          if(editor->mode == EDIT_OVERLAY)
          {
            editor->show_board_under_overlay ^= 1;
          }
          else
          {
            status_counter_info(mzx_world);
            editor->modified = true;
          }
        }
        else
        {
          char world_name[MAX_PATH];
          char new_path[MAX_PATH];
          strcpy(world_name, editor->current_world);
          if(!new_file(mzx_world, world_ext, ".mzx", world_name,
           "Save world", ALLOW_ALL_DIRS))
          {
            debug("Save path: %s\n", world_name);

            // It's now officially the current MZX version
            mzx_world->version = MZX_VERSION;

            // Save entire game
            strcpy(editor->current_world, world_name);
            strcpy(curr_file, world_name);
            save_world(mzx_world, world_name, false, MZX_VERSION);

            if(path_get_directory(new_path, MAX_PATH, world_name) > 0)
              vchdir(new_path);

            editor->modified = false;
          }
        }
      }
      else
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_t:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // Test world
        if(block->selected)
        {
          editor->cursor_mode = CURSOR_PLACE;
          block->selected = false;
        }

        clear_board_history(editor);
        clear_overlay_history(editor);
        clear_vlayer_history(editor);

        get_test_world_filename(editor);

        if(!save_world(mzx_world, editor->test_reload_file, false, MZX_VERSION))
        {
          vgetcwd(editor->test_reload_dir, MAX_PATH);
          editor->test_reload_version = mzx_world->version;
          editor->test_reload_board = mzx_world->current_board_id;
          editor->reload_after_testing = true;

          vquick_fadeout();
          cursor_off();

          set_update_done(mzx_world);

          // Changes to the board, duplicates if reset on entry
          change_board(mzx_world, mzx_world->current_board_id);
          change_board_set_values(mzx_world);
          change_board_load_assets(mzx_world);
          load_board_module(mzx_world);

          send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);
          send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

          debug_robot_reset(mzx_world);

          play_game(ctx, NULL);
        }
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_v:
    {
      if(editor->cursor_mode != CURSOR_TEXT)
      {
        if(get_alt_status(keycode_internal))
        {
          // Toggle vlayer editing
          if(editor->mode == EDIT_VLAYER)
          {
            set_editor_mode(editor, EDIT_BOARD);
          }
          else
          {
            set_editor_mode(editor, EDIT_VLAYER);
          }

          synchronize_board_values(editor);
          fix_scroll(editor);

          editor->cursor_mode = CURSOR_PLACE;
          block->selected = false;
        }
        else

        if(editor->mode != EDIT_VLAYER)
        {
          view_board(editor);
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_x:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        int export_number = export_type(mzx_world);
        if(export_number >= 0)
        {
          char export_name[MAX_PATH];
          export_name[0] = 0;

          switch(export_number)
          {
            case 0:
            {
              // Board file
              strcpy(export_name, editor->mzb_name_buffer);
              if(!new_file(mzx_world, mzb_ext, ".mzb", export_name,
               "Export board file", ALLOW_ALL_DIRS))
              {
                strcpy(editor->mzb_name_buffer, export_name);
                save_board_file(mzx_world, cur_board, export_name);
              }
              break;
            }

            case 1:
            {
              // Character set
              int char_offset = 0;
              int char_size = 256;
              struct element *elements[] =
              {
                construct_number_box(9, 20, "Offset:  ",
                 0, (PRO_CH - 1), NUMBER_BOX, &char_offset),
                construct_number_box(35, 20, "Size: ",
                 1, (PRO_CH), NUMBER_BOX, &char_size)
              };

              strcpy(export_name, editor->chr_name_buffer);
              if(!file_manager(mzx_world, chr_ext, NULL, export_name,
               "Export character set", ALLOW_ALL_DIRS, ALLOW_NEW_FILES,
               elements, ARRAY_SIZE(elements), 2))
              {
                path_force_ext(export_name, MAX_PATH, ".chr");
                ec_save_set_var(export_name, char_offset, char_size);
                strcpy(editor->chr_name_buffer, export_name);
              }

              break;
            }

            case 2:
            {
              // Palette (let the palette editor handle this one)
              export_palette(ctx);
              break;
            }

            case 3:
            {
              // Sound effects
              export_sfx(ctx);
              break;
            }

            case 4:
            {
              // Downver. world
              char title[80];

              sprintf(title, "Export world to previous version (%d.%d)",
               (MZX_VERSION_PREV >> 8) & 0xFF, MZX_VERSION_PREV & 0xFF);

              if(!new_file(mzx_world, world_ext, ".mzx", export_name,
               title, ALLOW_ALL_DIRS))
              {
                save_world(mzx_world, export_name, false, MZX_VERSION_PREV);
              }
              break;
            }

            case 5:
            {
              // Board/vlayer image
              const char *title = (editor->mode == EDIT_VLAYER) ?
               "Export vlayer image" : "Export board image";

              if(!new_file(mzx_world, image_ext, image_ext[0], export_name,
               title, ALLOW_ALL_DIRS))
              {
                if(editor->mode == EDIT_VLAYER)
                  export_vlayer_image(ctx, mzx_world, export_name);
                else
                  export_board_image(ctx, cur_board, export_name);
              }
              break;
            }
          }
        }
      }
      else

      if(editor->cursor_mode != CURSOR_TEXT)
      {
        // Doesn't make sense on the vlayer
        if(editor->mode != EDIT_VLAYER)
        {
          board_exits(mzx_world);
          editor->modified = true;
        }
      }
      else
      {
        place_text(editor);
      }
      return true;
    }

    case IKEY_y:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // Redo
        editor->modified |= apply_redo(editor->cur_history);
      }
      else

      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        mzx_world->debug_mode = !(mzx_world->debug_mode);
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }

      return true;
    }

    case IKEY_z:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // Undo
        editor->modified |= apply_undo(editor->cur_history);
      }
      else

      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(editor->mode == EDIT_VLAYER)
        {
          // Clear vlayer
          if(!confirm(mzx_world, "Clear vlayer - Are you sure?"))
          {
            memset(mzx_world->vlayer_chars, 0, mzx_world->vlayer_size);
            memset(mzx_world->vlayer_colors, 0, mzx_world->vlayer_size);

            if(block->selected)
            {
              editor->cursor_mode = CURSOR_PLACE;
              block->selected = false;
            }

            clear_vlayer_history(editor);
          }
        }
        else

        // Clear board
        if(!confirm(mzx_world, "Clear board - Are you sure?"))
        {
          clear_board(cur_board);
          cur_board = create_blank_board(editor_conf);
          mzx_world->current_board = cur_board;
          mzx_world->current_board->robot_list[0] = &mzx_world->global_robot;
          mzx_world->board_list[mzx_world->current_board_id] = cur_board;
          synchronize_board_values(editor);
          fix_scroll(editor);

          audio_end_module();
          cur_board->mod_playing[0] = 0;
          strcpy(mzx_world->real_mod_playing,
           cur_board->mod_playing);

          fix_world_title(editor);
          fix_caption(editor);

          clear_board_history(editor);
          clear_overlay_history(editor);

          if(block->selected)
          {
            editor->cursor_mode = CURSOR_PLACE;
            block->selected = false;
          }
        }

        editor->modified = true;
      }
      else

      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
      }
      return true;
    }

    case 0:
    {
      break;
    }

    default:
    {
      if(editor->cursor_mode == CURSOR_TEXT)
      {
        place_text(editor);
        return true;
      }
      break;
    }
  }

  // Exit event and Escape
  if(exit_status)
  {
    if(!editor->modified ||
     !confirm(mzx_world, "Exit: World has not been saved, are you sure?"))
    {
      destroy_context(ctx);
    }
  }
  return false;
}

/**
 * The editor needs to reload a temporary world after testing.
 */
static void editor_resume(context *ctx)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct world *mzx_world = ctx->world;
  boolean ignore;

  if(editor->reload_after_testing)
  {
    editor->reload_after_testing = false;
    vchdir(editor->test_reload_dir);

    if(!reload_world(mzx_world, editor->test_reload_file, &ignore))
    {
      if(!editor_reload_world(editor, editor->current_world))
        create_blank_world(mzx_world);

      set_editor_mode(editor, EDIT_BOARD);

      editor->modified = false;
    }

    else
    {
      // Set it back to the original version.
      mzx_world->version = editor->test_reload_version;
    }

    scroll_color = 15;
    mzx_world->current_board_id = editor->test_reload_board;
    set_current_board_ext(mzx_world,
     mzx_world->board_list[editor->test_reload_board]);

    synchronize_board_values(editor);
    fix_scroll(editor);

    if(editor->listening_mod_active)
    {
      audio_play_module(editor->current_listening_mod, false, 255);
    }
    else
    {
      fix_mod(editor);
    }

    vunlink(editor->test_reload_file);
  }

  // These may have changed if a robot was edited.
  edit_menu_show_robot_memory(editor->edit_menu);
  fix_caption(editor);
}

/**
 * Handle exiting the editor.
 */
static void editor_destroy(context *ctx)
{
  struct editor_context *editor = (struct editor_context *)ctx;
  struct buffer_info *buffer = &(editor->buffer);
  struct world *mzx_world = ctx->world;

  strcpy(curr_file, editor->current_world);

  mzx_world->editing = false;
  mzx_world->debug_mode = false;

  clear_world(mzx_world);
  clear_global_data(mzx_world);
  audio_end_module();

  // Clear any stored buffer data.
  free_edit_buffer(buffer);

  // Free the undo data
  clear_board_history(editor);
  clear_overlay_history(editor);
  clear_vlayer_history(editor);

  // Free the robot debugger data
  free_breakpoints();

  insta_fadeout();
  set_screen_mode(0);
  default_palette();
  clear_screen();
  cursor_off();
  m_hide();

  // Reset the caption
  caption_set_world(mzx_world);
}

/**
 * Initialize and run the editor context.
 */
static void __edit_world(context *parent, boolean reload_curr_file)
{
  struct editor_context *editor = ccalloc(1, sizeof(struct editor_context));
  struct buffer_info *buffer = &(editor->buffer);
  struct block_info *block = &(editor->block);
  struct context_spec spec;

  const struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ((context *)parent)->world;

  struct stat stat_res;

  vgetcwd(editor->current_listening_dir, MAX_PATH);

  if(editor_conf->board_editor_hide_help)
    editor->screen_height = EDIT_SCREEN_MINIMAL;
  else
    editor->screen_height = EDIT_SCREEN_NORMAL;

  editor->use_default_color = true;
  editor->show_board_under_overlay = true;
  editor->backup_timestamp = get_ticks();
  editor->debug_x = 60;
  editor->ansi_line_wrap_column = 80;

  buffer->robot = &(editor->buffer_robot);
  buffer->scroll = &(editor->buffer_scroll);
  buffer->sensor = &(editor->buffer_sensor);
  editor->stored_overlay_char = 32;
  editor->stored_overlay_color = 7;
  editor->stored_vlayer_char = 32;
  editor->stored_vlayer_color = 7;
  default_buffer(editor);

  block->command = BLOCK_CMD_NONE;
  block->selected = false;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.resume   = editor_resume;
  spec.draw     = editor_draw;
  spec.idle     = editor_idle;
  spec.key      = editor_key;
  spec.joystick = editor_joystick;
  spec.click    = editor_mouse;
  spec.drag     = editor_mouse;
  spec.destroy  = editor_destroy;

  create_context((context *)editor, parent, &spec, CTX_EDITOR);

  editor->edit_menu = create_edit_menu((context *)editor);

  // Reload the current world or create a blank world.
  if(curr_file[0] && vstat(curr_file, &stat_res))
    curr_file[0] = '\0';

  if(reload_curr_file && curr_file[0] &&
   editor_reload_world(editor, curr_file))
  {
    memcpy(editor->current_world, curr_file, MAX_PATH);

    mzx_world->current_board_id = mzx_world->first_board;
    set_current_board_ext(mzx_world,
     mzx_world->board_list[mzx_world->current_board_id]);

    fix_mod(editor);
  }
  else
  {
    editor->current_world[0] = 0;

    if(mzx_world->active)
    {
      clear_world(mzx_world);
      clear_global_data(mzx_world);
    }

    create_blank_world(mzx_world);

    // Prompt for the creation of a first board.
    // Holding alt while opening the editor bypasses this.
    if(!get_alt_status(keycode_internal))
      editor->first_board_prompt = true;

    audio_end_module();
    default_palette();
  }

  synchronize_board_values(editor);
  fix_caption(editor);

  set_palette_intensity(100);

  mzx_world->editing = true;
}

void editor_init(void)
{
  edit_world = __edit_world;
  draw_debug_box = __draw_debug_box;
  debug_counters = __debug_counters;
  debug_robot_break = __debug_robot_break;
  debug_robot_watch = __debug_robot_watch;
  debug_robot_config = __debug_robot_config;
  debug_robot_reset = __debug_robot_reset;
  load_editor_charsets();
}

boolean is_editor(void)
{
  return true;
}
