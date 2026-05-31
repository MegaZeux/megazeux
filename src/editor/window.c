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

#include <string.h>

#include "board.h"
#include "configure.h"
#include "window.h"

#include "../board.h"
#include "../core.h"
#include "../extmem.h"
#include "../graphics.h"
#include "../intake.h"
#include "../window.h"

#include "../event/event.h"
#include "../platform/platform.h"

#define list_button " \x1F "

#define check_on "[\xFB]"
#define check_off "[ ]"
#define char_custom '\x3F'

//Foreground colors that look nice for each background color
static const char fg_per_bk[16] =
 { 15, 15, 15, 15, 15, 15, 15, 0, 15, 15, 0, 0, 15, 15, 0, 0 };

// For list/choice menus-

// List/choice menu colors- Box same as dialog, elements same as
// inactive question, current element same as active question, pointer
// same as text, title same as title.

// Put up a box listing choices. Move pointer and enter to pick. choice
// size is the size in bytes per choice in the array of choices. This size
// includes null terminator and any junk padding. All work is done on
// page given, nopage flipping. Choices are drawn using color_string.
// On ESC, returns negative current choice, unless current choice is 0,
// then returns -32767. Returns OUT_OF_WIN_MEM if out of screen storage space.

// A progress column is displayed directly to the right of the list, using
// the following characters. This is mainly used for mouse support.

// Mouse support- Click on a choice to move there auto, click on progress
// column arrows to move one line, click on progress meter itself to move
// there percentage-wise. If you click on the current choice, it exits.
// If you click on a choice to move there, the mouse cursor moves with it.
int list_menu(const char *const *choices, int choice_size, const char *title,
 int current, int num_choices, int xpos, int ypos)
{
  int exit;
  char key_buffer[64];
  int mouse_press;
  int key_position = 0;
  int last_keypress_time = 0;
  int ticks;
  int width = choice_size + 6;
  int joystick_key;
  int key;
  int i;
  int j;

  cursor_off();

  if(title && strlen(title) > (unsigned int)choice_size)
    width = (int)strlen(title) + 6;

  save_screen();

  // Display box
  draw_window_box(xpos, ypos + 2, xpos + width - 1, 22, DI_MAIN, DI_DARK,
   DI_CORNER, 1, 1);

  if(title)
  {
    // Add title
    write_string(title, xpos + 3, ypos + 2, DI_TITLE, 0);
    draw_char(' ', DI_TITLE, xpos + 2, ypos + 2);
    draw_char(' ', DI_TITLE, xpos + 3 + (unsigned int)strlen(title), ypos + 2);
  }

  // Add pointer
  draw_char(arrow_char, DI_TEXT, xpos + 2, (ypos >> 1) + 12);

  // Add meter arrows
  if(num_choices > 1)
  {
    draw_char(pc_top_arrow, DI_PCARROW, xpos + 4 + choice_size, ypos + 3);
    draw_char(pc_bottom_arrow, DI_PCARROW, xpos + 4 + choice_size, 21);
  }

  do
  {
    // Fill over old menu
    draw_window_box(xpos + 3, ypos + 3, xpos + 3 + choice_size, 21,
     DI_DARK, DI_MAIN, DI_CORNER, 0, 1);

    // Draw current
    color_string(choices[current], xpos + 4, (ypos >> 1) + 12, DI_ACTIVE);
    cursor_hint(xpos + 4, ypos / 2 + 12);

    // Draw above current
    for(i = 1; i < 9 - (ypos >> 1); i++)
    {
      if((current - i) < 0)
        break;
      color_string(choices[current - i], xpos + 4,
       (ypos >> 1) + 12 - i, DI_NONACTIVE);
    }

    // Draw below current
    for(i = 1; i < 9 - (ypos >> 1); i++)
    {
      if((current + i) >= num_choices)
        break;
      color_string(choices[current + i], xpos + 4,
       (ypos >> 1) + 12 + i, DI_NONACTIVE);
    }

    // Draw meter (xpos 9 + choice_size, ypos 4 thru 20)
    if(num_choices > 1)
    {
      for(i = 4; i < 21 - ypos; i++)
        draw_char(pc_filler, DI_PCFILLER, xpos + 4 + choice_size, ypos + i);
      // Add progress dot
      i = (current * (16 - ypos)) / (num_choices - 1);
      draw_char(pc_dot, DI_PCDOT, xpos + 4 + choice_size, ypos + i + 4);
    }

    update_screen();

    // Get keypress
    update_event_status_delay();

    // Act upon it
    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    exit = get_exit_status();

    mouse_press = get_mouse_press_ext();

    if(mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      // Clicking on- List, column, arrow, or nothing?
      if((mouse_x == xpos + 4 + choice_size) && (mouse_y > ypos + 3) &&
       (mouse_y < 21))
      {
        // Column
        int ny = mouse_y - (ypos + 4);
        current = (ny * (num_choices - 1)) / (16 - ypos);
      }
      else

      if((mouse_y > ypos + 3) && (mouse_y < 21) &&
       (mouse_x > xpos + 3) && (mouse_x < xpos + 3 + choice_size))
      {
        // List
        if(mouse_y == ypos + 12)
          key = IKEY_RETURN;

        current += mouse_y - (ypos + 12);
        if(current < 0)
          current = 0;
        if(current >= num_choices)
          current = num_choices - 1;
        // Move mouse with choices
        warp_mouse(mouse_x, ypos + 12);
      }
      else

      if((mouse_x == xpos + 4 + choice_size) && (mouse_y == ypos + 3))
      {
        // Top arrow
        if(current > 0)
          current--;
      }
      else

      if((mouse_x == xpos + 4 + choice_size) && (mouse_y == 21))
      {
        // Bottom arrow
        if(current < (num_choices - 1))
         current++;
      }
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      key = IKEY_UP;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      key = IKEY_DOWN;
    }

    switch(key)
    {
      case IKEY_ESCAPE:
      {
        exit = 1;
        break;
      }

      case IKEY_BACKSPACE:
      {
        // Goto .. if it's there
        for(i = 0; i < num_choices; i++)
        {
          if(!strncmp(choices[i], " .. ", 4))
            break;
        }

        if(i == num_choices)
          break;

        current = i;
        restore_screen();
        return current;
      }

      case IKEY_RETURN:
      {
        // Selected
        restore_screen();
        return current;
      }

      case IKEY_UP:
      {
        if(current > 0)
          current--;
        break;
      }

      case IKEY_DOWN:
      {
        if(current < (num_choices - 1))
          current++;
        break;
      }

      case IKEY_PAGEUP:
      {
        current -= 8;
        if(current < 0)
          current = 0;
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current += 8;
        if(current >= num_choices)
          current = num_choices - 1;
        break;
      }

      case IKEY_HOME:
      {
        current = 0;
        break;
      }

      case IKEY_END:
      {
        current = num_choices - 1;
        break;
      }

      default:
      {
        int key_char = get_key(keycode_text_ascii);

        if(!get_alt_status(keycode_internal) &&
         !get_ctrl_status(keycode_internal) && (key_char >= 32))
        {
          ticks = get_ticks();
          if(((ticks - last_keypress_time) >= TIME_SUSPEND) ||
           (key_position == 63))
          {
            // Go back to zero
            key_position = 0;
          }
          last_keypress_time = ticks;

          key_buffer[key_position] = key_char;
          key_position++;
          key_buffer[key_position] = 0;

          if(key_buffer[0] == ' ')
          {
            const char *key_buffer_nospace = key_buffer;

            // Skip prefixed spaces for the buffer...
            while(*key_buffer_nospace == ' ')
              key_buffer_nospace++;

            if(!key_buffer_nospace[0])
              break;

            for(i = 0; i < num_choices; i++)
            {
              // Skip prefixed spaces for the choices...
              j = 0;
              while(choices[i][j] == ' ')
                j++;

              if(!strncasecmp(choices[i] + j, key_buffer_nospace,
               strlen(key_buffer_nospace)))
              {
                current = i;
                break;
              }
            }
          }
          else
          {
            for(i = 0; i < num_choices; i++)
            {
              if(!strncasecmp(choices[i], key_buffer, key_position))
              {
                current = i;
                break;
              }
            }
          }
        }
        break;
      }
    }

    // Exit event or Escape
    if(exit)
    {
      restore_screen();
      cursor_off();
      if(current == 0)
        return -32767;
      else
        return -current;
    }

  } while(1);
}

// Color selection screen colors- see char selection screen.

// Put up a color selection box. Returns selected, or negative selected
// for ESC, -512 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 16 across, 16 down. All work done on given page. Current
// uses bit 256 for the wild bit.

// Mouse support- Click on a color to select it. If it is the current color,
// it exits.

int color_selection(int current, int allow_wild)
{
  int x, y;
  int key;
  int joystick_key;
  int selected;
  int currx, curry;
  int last_keypress_time = 0;
  int last_input = 0;

  char palette_char = get_screen_mode() ? CHAR_PAL_SMZX : CHAR_PAL_REG;

  // Save screen
  save_screen();

  if(get_context(NULL) == CTX_MAIN)
    set_context(CTX_DIALOG_BOX);
  else
    set_context(get_context(NULL));

  // Ensure allow_wild is 0 or 1
  if(allow_wild)
    allow_wild = 1;

  // Calculate current x/y
  if((current & 256) && (allow_wild))
  {
    // Wild
    current -= 256;
    if(current < 16)
    {
      currx = current;
      curry = 16;
    }
    else
    {
      curry = current - 16;
      currx = 16;
    }
  }
  else

  if(current & 256)
  {
    // Wild when not allowed to be
    current -= 256;
    currx = current & 15;
    curry = current >> 4;
  }
  else
  {
    // Normal
    currx = current & 15;
    curry = current >> 4;
  }

  // Draw box
  draw_window_box(12, 2, 33 + allow_wild, 21 + allow_wild,
   DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  // Add title
  write_string(" Select a color ", 14, 2, DI_TITLE, 0);

  do
  {
    // Draw outer edge
    draw_window_box(14, 3, 31 + allow_wild, 20 + allow_wild,
     DI_DARK, DI_MAIN, DI_CORNER, 0, 0);

    // Clear place for main palette
    for(y = 0; y < 16; y++)
      for(x = 0; x < 16; x++)
        erase_char(x + 15, y + 4);

    select_layer(GAME_UI_LAYER);

    // Draw main palette
    for(y = 0; y < 16; y++)
      for(x = 0; x < 16; x++)
        draw_char_ext(palette_char, x + (y * 16), x + 15, y + 4, PRO_CH, 0);

    select_layer(UI_LAYER);

    // Draw wildcards
    if(allow_wild)
    {
      int wild_pal = get_screen_mode() ? 16 : 0;

      for(x = 1, y = 16; x < 16; x++)
        draw_char_ext(CHAR_PAL_WILD, x, x + 15, 20, PRO_CH, wild_pal);

      for(y = 0, x = 16; y < 16; y++)
        draw_char_ext(CHAR_PAL_WILD, fg_per_bk[y] + y * 16,
         31, y + 4, PRO_CH, wild_pal);

      // x = 0, y = 16
      draw_char_ext(CHAR_PAL_WILD, 128, 15, 20, PRO_CH, wild_pal);

      // x = 16, y = 16
      draw_char_ext(CHAR_PAL_WILD, 135, 31, 20, PRO_CH, wild_pal);
    }

    // Add selection box
    draw_window_box(currx + 14, curry + 3, currx + 16,
     curry + 5, DI_MAIN, DI_MAIN, DI_MAIN, 0, 0);
    cursor_hint(currx + 15, curry + 4);

    // Write number of color
    if((allow_wild) && ((currx == 16) || (curry == 16)))
    {
      // Convert wild
      if(currx == 16)
      {
        if(curry == 16)
          selected = 288;
        else
          selected = 272 + curry;
      }
      else
      {
        selected = 256 + currx;
      }
    }
    else
    {
      selected = currx + (curry * 16);
    }

    write_number(selected, DI_MAIN, 28 + allow_wild, 21 + allow_wild, 3, 0, 10);
    update_screen();

    // Get key

    update_event_status_delay();
    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x >= 15) && (mouse_x <= (30 + allow_wild)) &&
       (mouse_y >= 4) && (mouse_y <= (19 + allow_wild)))
      {
        int new_x = mouse_x - 15;
        int new_y = mouse_y - 4;
        if((currx == new_x) && (curry == new_y))
          key = IKEY_RETURN;

        currx = new_x;
        curry = new_y;
      }
    }

    // Process
    switch(key)
    {
      // ESC
      case IKEY_ESCAPE:
      case IKEY_SPACE:
      case IKEY_RETURN:
      {
        pop_context();
        // Selected
        restore_screen();
        cursor_off();
        if((allow_wild) && ((currx == 16) || (curry == 16)))
        {
          // Convert wild
          if(currx == 16)
          {
            if(curry == 16)
              current = 288;
            else
              current = 272 + curry;
          }
          else
          {
            current = 256 + currx;
          }
        }
        else
        {
          current = currx + curry * 16;
        }

        if(key == IKEY_ESCAPE)
        {
          if(current == 0)
            current = -512;
          else
            current = -current;
        }
        return current;
      }

      case IKEY_UP:
      {
        // Up
        if(curry > 0)
          curry--;

        break;
      }

      case IKEY_DOWN:
      {
        // Down
        if(curry < (15 + allow_wild))
          curry++;

        break;
      }

      case IKEY_LEFT:
      {
        // Left
        if(currx > 0)
          currx--;

        break;
      }

      case IKEY_RIGHT:
      {
        // Right
        if(currx < (15 + allow_wild))
          currx++;

        break;
      }

      case IKEY_HOME:
      {
        // Home
        currx = 0;
        curry = 0;
        break;
      }

      case IKEY_END:
      {
        // End
        currx = 15 + allow_wild;
        curry = 15 + allow_wild;
        break;
      }

      default:
      {
        int value = -1;

        if(key >= IKEY_0 && key <= IKEY_9)
        {
          value = key - IKEY_0;
        }
        else

        if(key >= IKEY_a && key <= IKEY_f)
        {
          value = 10 + (key - IKEY_a);
        }

        if(key == IKEY_SLASH && allow_wild)
          value = 16;

        if(value >= 0)
        {
          int ticks = get_ticks();
          if(ticks - last_keypress_time >= TIME_SUSPEND)
          {
            last_input = 0;
            currx = 0;
            curry = 0;
          }

          switch(last_input)
          {
            case 0:
              curry = value;
              break;

            case 1:
              currx = value;
              break;
          }

          last_keypress_time = ticks;
          last_input++;
        }
        break;
      }
    }
  } while(1);
}

// Short function to display a color as a colored box
void draw_color_box(int color, int q_bit, int x, int y, int x_limit)
{
  char palette_char = get_screen_mode() ? CHAR_PAL_SMZX : CHAR_PAL_REG;

  // If q_bit is set, there are unknowns
  if(q_bit)
  {
    if(color < 16)
    {
      // Unknown background
      // Use black except for black fg, which uses grey
      if(color == 0)
        color = 8;

      if(x < x_limit)
        draw_char_ext(CHAR_PAL_WILD, color, x, y, PRO_CH, 0);

      if(x + 1 < x_limit)
        draw_char_ext(CHAR_PAL_REG, color, x + 1, y, PRO_CH, 0);

      if(x + 2 < x_limit)
        draw_char_ext(CHAR_PAL_WILD, color, x + 2, y, PRO_CH, 0);
    }
    else

    if(color < 32)
    {
      // Unkown foreground
      // Use foreground from array
      color -= 16;
      color = (color << 4) + fg_per_bk[color];

      if(x < x_limit)
        draw_char_ext(CHAR_PAL_WILD, color, x, y, PRO_CH, 0);

      if(x + 1 < x_limit)
        draw_char_ext(CHAR_PAL_WILD, color, x + 1, y, PRO_CH, 0);

      if(x + 2 < x_limit)
        draw_char_ext(CHAR_PAL_WILD, color, x + 2, y, PRO_CH, 0);
    }
    else
    {
      // Both unknown
      if(x < x_limit)
        draw_char(CHAR_PAL_WILD, 8, x, y);

      if(x + 1 < x_limit)
        draw_char(CHAR_PAL_WILD, 135, x + 1, y);

      if(x + 2 < x_limit)
        draw_char(CHAR_PAL_WILD, 127, x + 2, y);
    }
  }
  else
  {
    // To respect SMZX, this needs to draw on the game UI layer.
    // If a color box is ever planned to be drawn NOT on the UI layer,
    // this needs to change.

    if(x < x_limit)
    {
      erase_char(x, y);
      select_layer(GAME_UI_LAYER);
      draw_char_ext(0, color, x, y, PRO_CH, 0);
      select_layer(UI_LAYER);
    }

    if(x + 1 < x_limit)
    {
      erase_char(x+1, y);
      select_layer(GAME_UI_LAYER);
      draw_char_ext(palette_char, color, x + 1, y, PRO_CH, 0);
      select_layer(UI_LAYER);
    }

    if(x + 2 < x_limit)
    {
      erase_char(x+2, y);
      select_layer(GAME_UI_LAYER);
      draw_char_ext(0, color, x + 2, y, PRO_CH, 0);
      select_layer(UI_LAYER);
    }
  }
}

static void draw_check_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct check_box *src = (struct check_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int i;

  for(i = 0; i < src->num_choices; i++)
  {
    if(src->results[i])
      write_string(check_on, x, y + i, DI_NONACTIVE, 0);
    else
      write_string(check_off, x, y + i, DI_NONACTIVE, 0);

    write_string(src->choices[i], x + 4, y + i,
     DI_NONACTIVE, 0);
  }

  if(active)
  {
    color_line(src->max_length + 4, x, y + src->current_choice,
     DI_ACTIVE);
    cursor_hint(x + 1, y + src->current_choice);
  }
}

static void draw_char_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct char_box *src = (struct char_box *)e;
  unsigned char result = *(src->result);
  int x = di->x + e->x;
  int y = di->y + e->y;
  int question_len = (int)strlen(src->question) + di->pad_space;

  write_string(src->question, x, y, color, 0);
  if(active)
    cursor_hint(x, y);

  // Special case: display for char ID value 255 custom behavior
  if((result == 255) && !(src->allow_char_255))
  {
    draw_char(char_custom, 0x05, x + question_len + 0, y);
    draw_char(char_custom, 0x0D, x + question_len + 1, y);
    draw_char(char_custom, 0x05, x + question_len + 2, y);
  }

  else
  {
    draw_char_ext(*(src->result), DI_CHAR,
     x + question_len + 1, y, 0, 16);
    draw_char(' ', DI_CHAR, x + question_len, y);
    draw_char(' ', DI_CHAR, x + question_len + 2, y);
  }
}

static void draw_color_box_element(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct color_box *src = (struct color_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int current_color = *(src->result);

  write_string(src->question, x, y, color, 0);
  draw_color_box(current_color & 0xFF, current_color >> 8,
   x + (int)strlen(src->question) + di->pad_space, y, 80);

  if(active)
    cursor_hint(x, y);
}

static void draw_board_list(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct board_list *src = (struct board_list *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int current_board = *(src->result);

  struct board *src_board;
  char board_name[BOARD_NAME_SIZE] = "(none)";

  if(current_board || (!src->board_zero_as_none))
  {
    if(current_board <= mzx_world->num_boards)
    {
      src_board = mzx_world->board_list[current_board];
      snprintf(board_name, BOARD_NAME_SIZE, "%s", src_board->board_name);
    }
    else
    {
      snprintf(board_name, BOARD_NAME_SIZE, "(no board)");
    }
    board_name[BOARD_NAME_SIZE - 1] = '\0';
  }

  write_string(src->title, x, y, color, 0);
  fill_line(BOARD_NAME_SIZE + 1, x, y + 1, 32, DI_LIST);
  if(active)
    cursor_hint(x, y);

  color_string(board_name, x + 1, y + 1, DI_LIST);

  // Draw button
  write_string(list_button, x + BOARD_NAME_SIZE + 1, y + 1,
   DI_ARROWBUTTON, 0);
}

static int key_check_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct check_box *src = (struct check_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      src->results[src->current_choice] ^= 1;
      break;
    }

    case IKEY_PAGEUP:
    {
      src->current_choice = 0;
      break;
    }

    case IKEY_LEFT:
    case IKEY_UP:
    {
      if(src->current_choice)
        src->current_choice--;

      break;
    }

    case IKEY_PAGEDOWN:
    {
      src->current_choice = src->num_choices - 1;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_DOWN:
    {
      if(src->current_choice < (src->num_choices - 1))
        src->current_choice++;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_char_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct char_box *src = (struct char_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_char =
       char_selection_ext(*(src->result), src->allow_char_255,
        NULL, NULL, NULL, -1);

      if((current_char == 255) && !(src->allow_char_255))
      {
        // Don't change the char if the user cancels.
        if(confirm(mzx_world,
         "Use param for the char of this type (like CustomBlock)?"))
          current_char = -1;
      }

      if(current_char >= 0)
        *(src->result) = current_char;
      break;
    }

    default:
    {
      int key_char = get_key(keycode_text_ascii);

      if(key_char >= 32)
      {
        *(src->result) = key_char;
        break;
      }

      return key;
    }
  }

  return 0;
}

static int key_color_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct color_box *src = (struct color_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_color =
       color_selection(*(src->result), src->allow_wildcard);

      if(current_color >= 0)
        *(src->result) = current_color;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_board_list(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct board_list *src = (struct board_list *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_board =
       choose_board(mzx_world, *(src->result),
       src->title, src->board_zero_as_none);

      if(current_board >= 0)
        *(src->result) = current_board;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int click_check_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct check_box *src = (struct check_box *)e;

  src->current_choice = mouse_y;
  src->results[src->current_choice] ^= 1;

  return 0;
}

static int click_char_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

static int click_color_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

static int click_board_list(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

struct element *construct_check_box(int x, int y, const char **choices,
 int num_choices, int max_length, int *results)
{
  struct check_box *src = cmalloc(sizeof(struct check_box));
  src->current_choice = 0;
  src->choices = choices;
  src->num_choices = num_choices;
  src->results = results;
  src->max_length = max_length;
  construct_element(&(src->e), x, y, max_length + 4, num_choices,
   draw_check_box, key_check_box, click_check_box, NULL, NULL);

  return (struct element *)src;
}

struct element *construct_char_box(int x, int y, const char *question,
 int allow_char_255, int *result)
{
  struct char_box *src = cmalloc(sizeof(struct char_box));
  src->question = question;
  src->allow_char_255 = allow_char_255;
  src->result = result;
  construct_element(&(src->e), x, y, (int)strlen(question) + 4,
   1, draw_char_box, key_char_box, click_char_box, NULL, NULL);

  return (struct element *)src;
}

struct element *construct_color_box(int x, int y,
 const char *question, int allow_wildcard, int *result)
{
  struct color_box *src = cmalloc(sizeof(struct color_box));
  src->question = question;
  src->allow_wildcard = allow_wildcard;
  src->result = result;
  construct_element(&(src->e), x, y, (int)strlen(question) + 4,
   1, draw_color_box_element, key_color_box, click_color_box,
   NULL, NULL);

  return (struct element *)src;
}

struct element *construct_board_list(int x, int y,
 const char *title, int board_zero_as_none, int *result)
{
  struct board_list *src = cmalloc(sizeof(struct board_list));
  src->title = title;
  src->board_zero_as_none = board_zero_as_none;
  src->result = result;
  construct_element(&(src->e), x, y, BOARD_NAME_SIZE + 3,
   2, draw_board_list, key_board_list, click_board_list,
   NULL, NULL);

  return (struct element *)src;
}

int add_board(struct world *mzx_world, int current)
{
  struct board *new_board;
  char name[BOARD_NAME_SIZE];
  memset(name, 0, BOARD_NAME_SIZE);

  if(input_window(mzx_world, "Name for new board:", name, BOARD_NAME_SIZE - 1))
    return -1;

  if(mzx_world->num_boards == mzx_world->num_boards_allocated)
  {
    mzx_world->num_boards_allocated *= 2;
    mzx_world->board_list = crealloc(mzx_world->board_list,
     mzx_world->num_boards_allocated * sizeof(struct board *));
  }

  mzx_world->num_boards++;
  new_board = create_blank_board(get_editor_config());
  mzx_world->board_list[current] = new_board;
  memcpy(new_board->board_name, name, BOARD_NAME_SIZE);

  // Link global robot!
  new_board->robot_list[0] = &mzx_world->global_robot;

  return current;
}

// Shell for list_menu()
int choose_board(struct world *mzx_world, int current, const char *title,
 int board0_none)
{
  char **board_names = ccalloc(mzx_world->num_boards + 1, sizeof(char *));
  int num_boards = mzx_world->num_boards;
  int i;

  // Go through - blank boards get a (no board) marker. t2 keeps track
  // of the number of boards.
  for(i = 0; i < num_boards; i++)
  {
    board_names[i] = cmalloc(BOARD_NAME_SIZE);

    if(mzx_world->board_list[i] == NULL)
      snprintf(board_names[i], BOARD_NAME_SIZE, "(no board)");
    else
      snprintf(board_names[i], BOARD_NAME_SIZE, "%s",
       (mzx_world->board_list[i])->board_name);

    board_names[i][BOARD_NAME_SIZE - 1] = '\0';
  }

  board_names[i] = cmalloc(BOARD_NAME_SIZE);

  if((current < 0) || (current >= mzx_world->num_boards))
    current = 0;

  // Add (new board) to bottom
  if(i < MAX_BOARDS)
  {
    snprintf(board_names[i], BOARD_NAME_SIZE, "(add board)");
    board_names[i][BOARD_NAME_SIZE - 1] = '\0';
    i++;
  }

  // Change top to (none) if needed
  if(board0_none)
  {
    snprintf(board_names[0], BOARD_NAME_SIZE, "(no board)");
    board_names[0][BOARD_NAME_SIZE - 1] = '\0';
  }

  // Run the list_menu()
  current = list_menu((const char **)board_names,
   BOARD_NAME_SIZE, title, current, i, 27, 0);

  // New board? (if select no board or add board)
  if((current == num_boards) ||
   ((current >= 0) && (mzx_world->board_list[current] == NULL)))
  {
    current = add_board(mzx_world, current);
    if(current >= 0)
    {
      // This board is expected to reside in extra memory (if applicable).
      store_board_to_extram(mzx_world->board_list[current]);
    }
  }

  for(i = 0; i < num_boards + 1; i++)
  {
    free(board_names[i]);
  }

  free(board_names);

  if(board0_none && (current == 0))
  {
    return NO_BOARD;
  }

  return current;
}

int choose_file(struct world *mzx_world, const char *const *wildcards,
 char *ret, const char *title, enum allow_dirs allow_dirs)
{
  return file_manager(mzx_world, wildcards, NULL, ret, title, allow_dirs,
   NO_NEW_FILES, NULL, 0, 0);
}
