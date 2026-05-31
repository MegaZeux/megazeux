/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#include "../core.h"
#include "../idput.h"
#include "../graphics.h"
#include "../util.h"
#include "../window.h"
#include "../world_struct.h"

#include "../event/event.h"

#include "buffer_struct.h"
#include "edit.h"
#include "edit_menu.h"
#include "window.h"

#include <stdint.h>
#include <string.h>

#define NUM_MENUS 6

#define ROBOT_MEMORY_TIMER_MAX  120
#define BOARD_MOD_TIMER_MAX     300

struct edit_menu_subcontext
{
  subcontext ctx;

  int current_menu;
  int robot_memory_timer;
  int board_mod_timer;
  size_t robot_mem;

  // Provided by edit.c
  struct buffer_info *buffer;
  enum editor_mode mode;
  enum cursor_mode cursor_mode;
  int cursor_x;
  int cursor_y;
  int screen_height;
  boolean use_default_color;
};

static const char menu_names[NUM_MENUS][9] =
{
  " WORLD ", " BOARD ", " THING ", " CURSOR ", " SHOW ", " MISC "
};

static const char menu_positions[] =
  "11111112222222333333344444444555555666666";

static const char *const menu_lines[NUM_MENUS][2]=
{
  {
    " L:Load   S:Save  G:Global Info  Alt+G:Global Robot  Alt+R:Reset   Alt+T:Test",
    " Alt+C:Char Edit  Alt+E:Palette  Alt+S:Status Info   Alt+V:Vlayer  Alt+F:SFX"
  },
  {
    " Alt+Z:Clear  Alt+I:Import  Alt+P:Size/Pos  I:Info   M:Move  A:Add  D:Delete",
    " Ctrl+G:Goto  Alt+X:Export  Alt+O:Overlay   X:Exits  V:View  B:Select Board"
  },
  {
    " F3:Terrain  F4:Item      F5:Creature  F6:Puzzle  F7:Transport  F8:Element",
    " F9:Misc     F10:Objects  P:Parameter  C:Color"
  },
  {
    " \x12\x1d:Move  Space:Place  Enter:Modify+Grab  Alt+M:Modify  Ins:Grab  Del:Delete",
          " F:Fill   Tab:Draw     F2:Text            Alt+B:Block   Alt+\x12\x1d:Move 10"
  },
  {
    " Shift+F1:Show InvisWalls   Shift+F2:Show Robots   Alt+F11:Robot Debugger",
    " Shift+F3:Show Fakes        Shift+F4:Show Spaces   Alt+Y:Debug Window"
  },
  {
    " F1:Help    Home/End:Corner  Alt+A:Select Char Set  Alt+D:Default Colors",
    " ESC:Exit   F11:Screen Mode  Alt+L:Test SAM         Alt+N:Music  *:Mod *"
  }
};

static const char *const overlay_menu_lines[3] =
{
  " OVERLAY EDITING- (Alt+O to end)",
  " \x12\x1d:Move  Enter:Modify+Grab  Space:Place  Ins:Grab  Del:Delete        F:Fill",
  " C:Color  Alt+\x12\x1d:Move 10     Alt+B:Block  Tab:Draw  Alt+S:Show level  F2:Text"
};

static const char *const vlayer_menu_lines[3] =
{
  " VLAYER EDITING- (Alt+V to end)",
  " \x12\x1d:Move  Enter:Modify+Grab  Space:Place  Ins:Grab  Del:Delete  F:Fill",
  " C:Color  Alt+\x12\x1d:Move 10     Alt+B:Block  Tab:Draw  Alt+P:Size  F2:Text"
};

static const char *const minimal_help_mode_mesg[3] =
{
  "Alt+H : Editor Menu",
  "Alt+H : Overlay Menu",
  "Alt+H : Vlayer Menu",
};

static const char cursor_mode_names[MAX_CURSOR_MODE][10] =
{
  " Current:",
  " Drawing:",
  "    Text:",
  "   Block:",
  "   Block:",
  "  Import:",
  "  Import:",
};

static const char cursor_mode_help[MAX_CURSOR_MODE][32] =
{
  "",
  "",
  "Type to place text",
  "Press ENTER on other corner",
  "Press ENTER to place block",
  "Press ENTER to place MZM",
  "Press ENTER to place ANSi",
};

#define num2hex(x) ((x) > 9 ? 87 + (x) : 48 + (x))

static void write_hex_byte(char byte, char color, int x, int y)
{
  int t1, t2;
  t1 = (byte & 240) >> 4;
  t1 = num2hex(t1);
  t2 = byte & 15;
  t2 = num2hex(t2);
  draw_char(t1, color, x, y);
  draw_char(t2, color, x + 1, y);
}

static void draw_menu_status(struct edit_menu_subcontext *edit_menu, int line)
{
  struct world *mzx_world = ((context *)edit_menu)->world;
  struct board *cur_board = mzx_world->current_board;
  struct buffer_info *buffer = edit_menu->buffer;
  unsigned int display_next_pos;
  const char *str;

  str = cursor_mode_names[edit_menu->cursor_mode];
  write_string(str, 42, line, EC_MODE_STR, false);
  display_next_pos = 42 + strlen(str);

  switch(edit_menu->cursor_mode)
  {
    case MAX_CURSOR_MODE:
      return;

    case CURSOR_TEXT:
    case CURSOR_BLOCK_SELECT:
    case CURSOR_BLOCK_PLACE:
    case CURSOR_MZM_PLACE:
    case CURSOR_ANSI_PLACE:
    {
      write_string(cursor_mode_help[edit_menu->cursor_mode],
       display_next_pos, line, EC_MODE_HELP, false);
      break;
    }

    case CURSOR_PLACE:
    case CURSOR_DRAW:
    {
      int display_color;
      int display_char;

      if(edit_menu->mode == EDIT_BOARD)
      {
        if(buffer->id == SENSOR)
        {
          display_char = buffer->sensor->sensor_char;
          display_color = buffer->color;
        }
        else

        if(is_robot(buffer->id))
        {
          display_char = buffer->robot->robot_char;
          display_color = buffer->color;
        }
        else
        {
          // TODO Clean this up. Requires a refactor of idput.c
          int temp_char = cur_board->level_id[0];
          int temp_param = cur_board->level_param[0];
          int temp_color = cur_board->level_color[0];
          cur_board->level_id[0] = buffer->id;
          cur_board->level_param[0] = buffer->param;
          cur_board->level_color[0] = buffer->color;

          display_char = get_id_char(cur_board, 0);
          display_color = get_id_board_color(cur_board, 0, true);

          cur_board->level_id[0] = temp_char;
          cur_board->level_param[0] = temp_param;
          cur_board->level_color[0] = temp_color;
        }
      }
      else
      {
        display_char = buffer->param;
        display_color = buffer->color;
      }

      draw_char(' ', 7, display_next_pos, line);
      erase_char(display_next_pos + 1, line);

      select_layer(GAME_UI_LAYER);
      draw_char_ext(display_char, display_color, display_next_pos + 1, line, 0, 0);
      select_layer(UI_LAYER);

      draw_char(' ', 7, display_next_pos + 2, line);
      display_next_pos += 4;

      draw_char('(', EC_CURR_THING, display_next_pos, line);
      draw_color_box(display_color, 0, display_next_pos + 1, line, 80);
      display_next_pos += 5;

      if(edit_menu->mode != EDIT_BOARD)
      {
        // "Character" is in the overlay/vlayer arrays
        write_string("Character", display_next_pos, line, EC_CURR_THING, 0);
        display_next_pos += strlen("Character") + 1;

        write_hex_byte(buffer->param, EC_CURR_PARAM, display_next_pos, line);
        display_next_pos += 2;
      }
      else
      {
        char unknown[20];

        if(buffer->id < ARRAY_SIZE(thing_names))
        {
          str = thing_names[buffer->id];
        }
        else
        {
          sprintf(unknown, "unknown_%02x", buffer->id);
          str = unknown;
        }

        write_string(str, display_next_pos, line, EC_CURR_THING, false);
        display_next_pos += strlen(str) + 1;

        draw_char('p', EC_CURR_PARAM, display_next_pos, line);
        display_next_pos++;

        write_hex_byte(buffer->param, EC_CURR_PARAM, display_next_pos, line);
        display_next_pos += 2;
      }

      draw_char(')', EC_CURR_THING, display_next_pos, line);
      display_next_pos++;

      if(!edit_menu->use_default_color)
      {
        draw_char('\x07', EC_DEFAULT_COLOR, display_next_pos, line);
      }
      break;
    }
  }
}

static void draw_menu_normal(struct edit_menu_subcontext *edit_menu)
{
  draw_window_box(0, 19, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);
  draw_window_box(0, 21, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);

  if(edit_menu->mode == EDIT_BOARD)
  {
    int i, write_color, x;
    x = 1; // X position

    for(i = 0; i < NUM_MENUS; i++)
    {
      if(i == edit_menu->current_menu)
        write_color = EC_CURR_MENU_NAME;
      else
        write_color = EC_MENU_NAME; // Pick the color

      // Write it
      write_string(menu_names[i], x, 20, write_color, 0);
      // Add to x
      x += (int)strlen(menu_names[i]);
    }

    write_string(menu_lines[edit_menu->current_menu][0], 1, 22, EC_OPTION, 1);
    write_string(menu_lines[edit_menu->current_menu][1], 1, 23, EC_OPTION, 1);
    write_string("Pgup/Pgdn:Menu", 64, 24, EC_CURR_PARAM, 1);
  }
  else

  if(edit_menu->mode == EDIT_OVERLAY)
  {
    write_string(overlay_menu_lines[0], 1, 20, EC_MENU_NAME, 1);
    write_string(overlay_menu_lines[1], 1, 22, EC_OPTION, 1);
    write_string(overlay_menu_lines[2], 1, 23, EC_OPTION, 1);
  }

  else // EDIT_VLAYER
  {
    write_string(vlayer_menu_lines[0], 1, 20, EC_MENU_NAME, 1);
    write_string(vlayer_menu_lines[1], 1, 22, EC_OPTION, 1);
    write_string(vlayer_menu_lines[2], 1, 23, EC_OPTION, 1);
  }

  draw_menu_status(edit_menu, EDIT_SCREEN_NORMAL + 1);

  draw_char(196, EC_MAIN_BOX_CORNER, 78, 21);
  draw_char(217, EC_MAIN_BOX_DARK, 79, 21);
}

static void draw_menu_minimal(struct edit_menu_subcontext *edit_menu)
{
  struct world *mzx_world = ((context *)edit_menu)->world;
  struct board *cur_board = mzx_world->current_board;

  fill_line(80, 0, EDIT_SCREEN_MINIMAL, ' ', EC_MAIN_BOX);

  // Display robot memory where the Alt+H message would usually go.
  if(edit_menu->robot_memory_timer > 0)
  {
    int robot_mem_kb = (edit_menu->robot_mem + 512) / 1024;

    write_string("Robot mem:       kb", 2, EDIT_SCREEN_MINIMAL, EC_MODE_STR,
     false);
    write_number(robot_mem_kb, 31, 2+11, EDIT_SCREEN_MINIMAL, 6, 0, 10);
  }
  else

  // Display the board mod where the Alt+H message would usually go.
  if(edit_menu->board_mod_timer > 0)
  {
    char *mod_name = cur_board->mod_playing;
    int len = strlen(mod_name);
    int off = 0;
    char temp;

    write_string("Mod:               ", 2, EDIT_SCREEN_MINIMAL, EC_MODE_STR,
     false);

    if(len > 14)
    {
      off = (BOARD_MOD_TIMER_MAX - edit_menu->board_mod_timer)/10;
      if(off > len - 14)
        off = len - 14;
    }

    temp = mod_name[off+14];
    mod_name[off+14] = 0;
    write_string(mod_name+off, 2+5, EDIT_SCREEN_MINIMAL, 31, 1);
    mod_name[off+14] = temp;
  }

  // Display the Alt+H message.
  else
  {
    write_string(minimal_help_mode_mesg[edit_menu->mode], 2,
     EDIT_SCREEN_MINIMAL, EC_OPTION, false);
  }

  write_string("X/Y:      /     ", 3+21, EDIT_SCREEN_MINIMAL,
   EC_MODE_STR, false);

  write_number(edit_menu->cursor_x, 31, 3+21+5, EDIT_SCREEN_MINIMAL,
   5, false, 10);
  write_number(edit_menu->cursor_y, 31, 3+21+11, EDIT_SCREEN_MINIMAL,
   5, false, 10);

  draw_menu_status(edit_menu, EDIT_SCREEN_MINIMAL);
}

static boolean edit_menu_draw(subcontext *ctx)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;

  if(edit_menu->screen_height == EDIT_SCREEN_NORMAL)
  {
    draw_menu_normal(edit_menu);
  }
  else
  {
    draw_menu_minimal(edit_menu);
  }
  return true;
}

static boolean edit_menu_idle(subcontext *ctx)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;

  if(edit_menu->robot_memory_timer > 0)
    edit_menu->robot_memory_timer--;

  if(edit_menu->board_mod_timer > 0)
    edit_menu->board_mod_timer--;

  return false;
}

static boolean edit_menu_mouse(subcontext *ctx, int *key, int button,
 int x, int y)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;
  struct buffer_info *buffer = edit_menu->buffer;
  int status_y = EDIT_SCREEN_MINIMAL;
  boolean minimal = true;

  if(edit_menu->screen_height == EDIT_SCREEN_NORMAL)
  {
    status_y = EDIT_SCREEN_NORMAL + 1;
    minimal = false;
  }

  // Mouse actions on buffer row
  if(y == status_y)
  {
    if(!minimal && (x >= 1) && (x <= 41))
    {
      // Select current help menu
      edit_menu->current_menu = menu_positions[x - 1] - '1';
      return true;
    }
    else

    if((x >= 56) && (x <= 58))
    {
      // Change current color
      int new_color = color_selection(buffer->color, 0);
      if(new_color >= 0)
        buffer->color = new_color;

      return true;
    }
  }
  return false;
}

static boolean edit_menu_key(subcontext *ctx, int *key)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;

  switch(*key)
  {
    case IKEY_PAGEDOWN:
    {
      edit_menu->current_menu++;

      if(edit_menu->current_menu == NUM_MENUS)
        edit_menu->current_menu = 0;

      return true;
    }

    case IKEY_PAGEUP:
    {
      if(edit_menu->current_menu == 0)
        edit_menu->current_menu = NUM_MENUS;

      edit_menu->current_menu--;
      return true;
    }
  }

  return false;
}

subcontext *create_edit_menu(context *parent)
{
  struct edit_menu_subcontext *edit_menu =
   ccalloc(1, sizeof(struct edit_menu_subcontext));
  struct context_spec spec;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw   = edit_menu_draw;
  spec.idle   = edit_menu_idle;
  spec.key    = edit_menu_key;
  spec.click  = edit_menu_mouse;
  spec.drag   = edit_menu_mouse;

  create_subcontext((subcontext *)edit_menu, parent, &spec);
  return (subcontext *)edit_menu;
}

void update_edit_menu(subcontext *ctx, enum editor_mode mode,
 enum cursor_mode cursor_mode, int cursor_x, int cursor_y, int screen_height,
 struct buffer_info *buffer, boolean use_default_color)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;

  if(edit_menu->screen_height != screen_height)
  {
    edit_menu->board_mod_timer = 0;
    edit_menu->robot_memory_timer = 0;
  }

  edit_menu->mode = mode;
  edit_menu->cursor_mode = cursor_mode;
  edit_menu->cursor_x = cursor_x;
  edit_menu->cursor_y = cursor_y;
  edit_menu->screen_height = screen_height;
  edit_menu->buffer = buffer;
  edit_menu->use_default_color = use_default_color;
}

void edit_menu_show_board_mod(subcontext *ctx)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;
  edit_menu->board_mod_timer = BOARD_MOD_TIMER_MAX;
}

static size_t compute_robot_memory(struct edit_menu_subcontext *edit_menu)
{
  struct world *mzx_world = ((context *)edit_menu)->world;
  struct board *cur_board = mzx_world->current_board;
  size_t robot_mem = 0;
  int i;

  for(i = 0; i < cur_board->num_robots_active; i++)
  {
    robot_mem +=
#ifdef CONFIG_DEBYTECODE
     (cur_board->robot_list_name_sorted[i])->program_source_length;
#else
     (cur_board->robot_list_name_sorted[i])->program_bytecode_length;
#endif
  }

  return robot_mem;
}

void edit_menu_show_robot_memory(subcontext *ctx)
{
  struct edit_menu_subcontext *edit_menu = (struct edit_menu_subcontext *)ctx;
  size_t new_robot_mem = compute_robot_memory(edit_menu);

  // If the robot memory hasn't changed, there isn't much point it showing it.
  if(edit_menu->robot_mem != new_robot_mem)
  {
    edit_menu->robot_memory_timer = ROBOT_MEMORY_TIMER_MAX;
    edit_menu->robot_mem = new_robot_mem;
  }
}
