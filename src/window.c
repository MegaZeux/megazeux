/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2012-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

// Windowing code- Save/restore screen and draw windows and other elements,
//                 also displays and runs dialog boxes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>

#include "board.h"
#include "configure.h" // TODO for help file only
#include "const.h"
#include "core.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "game_menu.h"
#include "graphics.h"
#include "helpsys.h"
#include "intake.h"
#include "robot.h"
#include "window.h"
#include "world.h"
#include "util.h"
#include "io/path.h"
#include "io/vio.h"

#include "audio/sfx.h"
#include "event/event.h"
#include "platform/platform.h"

#ifdef CONFIG_NDS
// Should be sufficient with a disabled editor.
#define NUM_SAVSCR 3
#else
#define NUM_SAVSCR 6
#endif

static struct char_element screen_storage[NUM_SAVSCR][SET_SCREEN_SIZE];

// Current space for save_screen and restore_screen
static int cur_screen = 0;

// The last-used saved game slot, and the current slot prefix.
static int cur_slot = 0;
static char *cur_slot_prefix = NULL;

// Some slot selector definitions.
#define SLOT_DISABLED   -1
#define SLOT_NORMAL     0
#define SLOT_HIGHLIGHT  1

#define SLOTSEL_BAR_Y  2

#define SLOTSEL_MAX_ELEMENTS        4
#define SLOTSEL_OKAY_BUTTON         0
#define SLOTSEL_CANCEL_BUTTON       1
#define SLOTSEL_FILE_MANAGER_BUTTON 2
#define SLOTSEL_SELECTOR            3
#define SLOTSEL_NUM_SLOTS           10

// Free up memory.

// The following functions do NOT check to see if memory is reserved, in
// the interest of time, space, and simplicity. Make sure you have already
// called window_cpp_init.

// Saves current screen to buffer and increases buffer count. Returns
// non-0 if the buffer for screens is already full. (IE 6 count)

int save_screen(void)
{
  if(cur_screen >= NUM_SAVSCR)
  {
    cur_screen = 0;
    error("Windowing code bug", ERROR_T_FATAL, ERROR_OPT_EXIT, 0x1F01);
  }

  get_screen(screen_storage[cur_screen]);
  cur_screen++;
  return 0;
}

// Restores top screen from buffer to screen and decreases buffer count.
// Returns non-0 if there are no screens in the buffer.

int restore_screen(void)
{
  if(cur_screen == 0)
    error("Windowing code bug", ERROR_T_FATAL, ERROR_OPT_EXIT, 0x1F02);

  cur_screen--;
  set_screen(screen_storage[cur_screen]);
  return 0;
}

// Draws an outlined and filled box from x1/y1 to x2/y2. Color is used to
// fill box and for upper left lines. Dark_color is used for lower right
// lines. Corner_color is used for the single chars in the upper right and
// lower left corners. If Corner_color equals 0, these are auto calculated.
// Returns non-0 for invalid parameters. Shadow (black spaces) is drawn and
// clipped if specified. Operation on a size smaller than 3x3 is undefined.
// This routine is highly unoptimized. Center is not filled if fill_center
// is set to 0. (defaults to 1)

int draw_window_box(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, boolean shadow, boolean fill_center)
{
  return draw_window_box_ext(x1, y1, x2, y2, color, dark_color,
   corner_color, shadow, fill_center, PRO_CH, 16);
}

int draw_window_box_ext(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, boolean shadow, boolean fill_center,
 int offset, int c_offset)
{
  int t1, t2;
  //Validate parameters
  if((x1 < 0) || (y1 < 0) || (x1 > 79) || (y1 > 24)) return 1;
  if((x2 < 0) || (y2 < 0) || (x2 > 79) || (y2 > 24)) return 1;
  if(x2 < x1)
  {
    t1 = x1;
    x1 = x2;
    x2 = t1;
  }
  if(y2 < y1)
  {
    t1 = y1;
    y1 = y2;
    y2 = t1;
  }

  // Fill center
  if(fill_center)
  {
    for(t1 = x1 + 1; t1 < x2; t1++)
    {
      for(t2 = y1 + 1; t2 < y2; t2++)
      {
        draw_char_ext(' ', color, t1, t2, offset, c_offset);
      }
    }
  }

  // Draw top and bottom edges
  for(t1 = x1 + 1; t1 < x2; t1++)
  {
    draw_char_ext('\xC4', color, t1, y1, offset, c_offset);
    draw_char_ext('\xC4', dark_color, t1, y2, offset, c_offset);
  }

  // Draw left and right edges
  for(t2 = y1 + 1; t2 < y2; t2++)
  {
    draw_char_ext('\xB3', color, x1, t2, offset, c_offset);
    draw_char_ext('\xB3', dark_color, x2, t2, offset, c_offset);
  }

  // Draw corners
  draw_char_ext('\xDA', color, x1, y1, offset, c_offset);
  draw_char_ext('\xD9', dark_color, x2, y2, offset, c_offset);
  if(corner_color)
  {
    draw_char_ext('\xC0', corner_color, x1, y2, offset, c_offset);
    draw_char_ext('\xBF', corner_color, x2, y1, offset, c_offset);
  }
  else
  {
    draw_char_ext('\xC0', color, x1, y2, offset, c_offset);
    draw_char_ext('\xBF', dark_color, x2, y1, offset, c_offset);
  }

  // Draw shadow if applicable
  if(shadow)
  {
    // Right edge
    if(x2 < 79)
    {
      for(t2 = y1 + 1; t2 <= y2; t2++)
      {
        draw_char_ext(' ', 0, x2 + 1, t2, offset, c_offset);
      }
    }

    // Lower edge
    if(y2 < 24)
    {
      for(t1 = x1 + 1; t1 <= x2; t1++)
      {
        draw_char_ext(' ', 0, t1, y2 + 1, offset, c_offset);
      }
    }

    // Lower right corner
    if((y2 < 24) && (x2 < 79))
    {
      draw_char_ext(' ', 0, x2 + 1, y2 + 1, offset, c_offset);
    }
  }
  return 0;
}

// Strings for drawing different dialog box elements.
// All parts are assumed 3 wide.

#define radio_on "(\x07)"
#define radio_off "( )"
#define num_buttons " \x18  \x19 "
#define list_button " \x1F "

#define char_sel_arrows_0 '\x1E'
#define char_sel_arrows_1 '\x1F'
#define char_sel_arrows_2 '\x10'
#define char_sel_arrows_3 '\x11'

// Char selection screen colors- Window colors same as dialog, non-current
// characters same as nonactive, current character same as active, arrows
// along edges same as text, title same as title.

// Put up a character selection box. Returns selected, or negative selected
// for ESC, -256 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 32 across, 8 down. All work done on given page.

// Mouse support- Click on a character to select it. If it is the current
// character, it exits.

// allow_char_255 -- if this is set to zero, char 255 will have a special
// display corresponding to Custom* type behavior. This is meant for the char
// ID editor only.

// TODO this shouldn't really be here, but it's the best place for now.
__editor_maybe_static int char_select_next_tile(int current_char,
 int direction, int highlight_width, int highlight_height)
{
  // -1 is previous, 1 is next
  int x = current_char & 31;
  int y = (current_char & 0xFF) >> 5;

  int mod_x = x % highlight_width;
  int mod_y = y % highlight_height;

  int tiles_width = (32 - mod_x) / highlight_width;
  int tiles_height = (8 - mod_y) / highlight_height;

  int last_x = (tiles_width - 1) * highlight_width + mod_x;
  int last_y = (tiles_height - 1) * highlight_height + mod_y;

  if(direction > 0)
  {
    if(highlight_height == 1)
    {
      // No need for tiling with N x 1 selection
      x += highlight_width;
    }
    else

    if(x == last_x)
    {
      x = mod_x;

      if(y == last_y)
        y = mod_y;

      else
        y += highlight_height;
    }
    else
    {
      x += highlight_width;
    }
  }
  else

  if(direction < 0)
  {
    if(highlight_height == 1)
    {
      x -= highlight_width;
    }
    else

    if(x == mod_x)
    {
      x = last_x;

      if(y == mod_y)
        y = last_y;

      else
        y -= highlight_height;
    }
    else
    {
      x -= highlight_width;
    }
  }

  // Clear the char bits of current_char and replace with the new position
  current_char &= (~0xFF);
  current_char |= (x + (y * 32)) & 0xFF;

  return current_char;
}

__editor_maybe_static int char_selection_ext(int current, int allow_char_255,
 int *width_ptr, int *height_ptr, int *select_charset, int selection_pal)
{
  uint32_t pal_layer = OVERLAY_LAYER;
  uint32_t chars_layer = UI_LAYER;
  int allow_multichar = 0;
  int current_charset = 0;
  int screen_mode = 0;
  int shifted = 0;
  int highlight_x = 0;
  int highlight_y = 0;
  int start_x = 0;
  int start_y = 0;
  int width = 1;
  int height = 1;
  int exit = 0;
  int bottom = 16;
  int x, y;
  int i, i2;
  int joystick_key;
  int key;

  if(width_ptr && height_ptr)
  {
    allow_multichar = 1;
    width = *width_ptr;
    height = *height_ptr;
  }

  if(select_charset)
  {
    pal_layer = create_layer(0, 0, 80, 25, LAYER_DRAWORDER_UI - 2,
     -1, *select_charset * 256, 1);

    chars_layer = create_layer(0, 0, 80, 25, LAYER_DRAWORDER_UI - 1,
     -1, *select_charset * 256, 1);

    set_layer_mode(chars_layer, 0);

    current_charset = *select_charset;
    screen_mode = get_screen_mode();
    bottom++;
  }

  if(selection_pal < 0)
  {
    screen_mode = 0;
  }

  // Prevent previous keys from carrying through.
  force_release_all_keys();
  save_screen();

  do
  {
    // Draw box and inner box
    draw_window_box(20, 5, 57, bottom, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
    draw_window_box(22, 6, 55, 15, DI_DARK, DI_MAIN, DI_CORNER, 0, 0);

    // Add title
    if(allow_multichar)
      write_string(" Select characters ", 22, 5, DI_TITLE, 0);
    else
      write_string(" Select a character ", 22, 5, DI_TITLE, 0);

    if(select_charset)
    {
      // Shift layer offsets to display the current charset
      set_layer_offset(pal_layer, current_charset * 256);
      set_layer_offset(chars_layer, current_charset * 256);

      write_string("X PgUp \t\t\t\t PgDn X", 22, 16, DI_NONACTIVE, 1);
      draw_char(char_sel_arrows_3, DI_NONACTIVE, 22, 16);
      draw_char(char_sel_arrows_2, DI_NONACTIVE, 55, 16);

      if(current_charset)
      {
        write_string("Extended Set ## ", 32, 16, DI_TEXT_GREY, 0);
        write_number(current_charset, DI_TEXT_GREY, 46, 16, 2, 1, 10);
      }
      else
      {
        write_string(" Main Charset  ", 32, 16, DI_TEXT_GREY, 0);
      }

      // Clear the UI where the char display will appear
      for(x = 0; x < 32; x++)
        for(y = 0; y < 8; y++)
          erase_char(x + 23, y + 7);
    }

    select_layer(chars_layer);

    // Draw character set
    for(x = 0; x < 32; x++)
    {
      for(y = 0; y < 8; y++)
      {
        draw_char_ext(x + (y * 32), DI_NONACTIVE,
         x + 23, y + 7, 0, 16);
      }
    }

    x = (current & 31);
    y = (current >> 5);
    cursor_hint(x + 23, y + 7);

    if(get_shift_status(keycode_internal) && allow_multichar)
    {
      // Update selection
      if(!shifted)
      {
        highlight_x = x;
        highlight_y = y;
        start_x = x;
        start_y = y;
        shifted = 1;
      }
      else
      {
        start_x = highlight_x;
        start_y = highlight_y;

        if(start_x > x)
        {
          start_x = x;
          width = highlight_x - x + 1;
        }
        else
        {
          width = x - highlight_x + 1;
        }

        if(start_y > y)
        {
          start_y = y;
          height = highlight_y - y + 1;
        }
        else
        {
          height = y - highlight_y + 1;
        }

        for(i = 0; i < height; i++)
        {
          color_line(width, start_x + 23, start_y + i + 7, 0x9F);
        }
      }
    }
    else
    {
      // Highlight active character(s)
      int char_offset;
      int skip = 32 - width;
      int x2, y2;

      if(shifted == 1)
      {
        int size = width * height;

        if((size > 18) || (size == 7) || (size == 11) ||
         (size == 13) || (size == 14) || (size == 16) ||
         (size == 17))
        {
          width = 1;
          height = 1;
        }

        x = start_x;
        y = start_y;
        current = (y * 32) + x;
      }

      char_offset = current;
      shifted = 0;

      for(i = 0; i < height; i++, char_offset += skip)
      {
        for(i2 = 0; i2 < width; i2++, char_offset++)
        {
          x2 = (char_offset & 31) + 23;
          y2 = ((char_offset & 255) >> 5) + 7;

          if(screen_mode)
          {
            select_layer(chars_layer);
            erase_char(x2, y2);
            select_layer(pal_layer);
            draw_char_ext(char_offset, selection_pal, x2, y2, 0, 0);
          }
          else

          if(selection_pal >= 0)
          {
            draw_char_ext(char_offset, selection_pal, x2, y2, 0, 0);
          }

          else
          {
            draw_char_ext(char_offset, DI_ACTIVE, x2, y2, 0, 16);
          }
        }
      }
    }

    select_layer(UI_LAYER);

    // Special display for allow_char_255 == 0
    if(!allow_char_255)
    {
      int color = 0x05;

      if(current == 255)
        color = 0x0D;

      draw_char('?', color, 31+23, 7+7);
    }

    // Draw arrows
    draw_char(char_sel_arrows_0, DI_TEXT, x + 23, 15);
    draw_char(char_sel_arrows_1, DI_TEXT, x + 23, 6);
    draw_char(char_sel_arrows_2, DI_TEXT, 22, y + 7);
    draw_char(char_sel_arrows_3, DI_TEXT, 55, y + 7);

    // Write number of character
    write_number(current, DI_MAIN, 53, bottom, 3, 0, 10);

    update_screen();
    update_event_status_delay();
    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    if(get_exit_status())
      key = IKEY_ESCAPE;

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x >= 23) && (mouse_x <= 54) &&
       (mouse_y >= 7) && (mouse_y <= 14))
      {
        int new_pos = mouse_x - 23 + ((mouse_y - 7) << 5);
        if(current == new_pos)
        {
          pop_context();
          restore_screen();
          return current;
        }
        current = new_pos;
      }
    }

    // Process key

    switch(key)
    {
      case IKEY_ESCAPE:
      {
        if(current == 0)
          current = -256;

        else
          current = -current;

        exit = 1;
        break;
      }

      case IKEY_SPACE:
      case IKEY_RETURN:
      {
        if(get_shift_status(keycode_internal) && allow_multichar)
        {
          int size = width * height;

          if((size > 18) || (size == 7) || (size == 11) ||
           (size == 13) || (size == 14) || (size == 16) ||
           (size == 17))
          {
            width = 1;
            height = 1;
            break;
          }

          x = start_x;
          y = start_y;
          current = (y * 32) + x;
        }

        if(allow_multichar)
        {
          if(height > 1)
          {
            // Clip if the selection wraps around the edge
            int abort = 0;

            if(width + x > 32)
            {
              width = 32 - x;
              abort = 1;
            }

            if(height + y > 8)
            {
              height = 8 - y;
              abort = 1;
            }

            if(abort)
              break;
          }

          *width_ptr = width;
          *height_ptr = height;
        }

        if(select_charset)
        {
          *select_charset = current_charset;
        }

        exit = 1;
        break;
      }

      case IKEY_UP:
      {
        current = (current - 32) & 255;
        break;
      }

      case IKEY_DOWN:
      {
        current = (current + 32) & 255;
        break;
      }

      case IKEY_LEFT:
      {
        current = (current - 1) & 255;
        break;
      }

      case IKEY_RIGHT:
      {
        current = (current + 1) & 255;
        break;
      }

      case IKEY_HOME:
      {
        current = 0;
        break;
      }

      case IKEY_END:
      {
        current = 288 - width - (height * 32);
        break;
      }

      case IKEY_PAGEUP:
      {
        if(select_charset)
        {
          current_charset--;

          if(current_charset < 0)
            current_charset = NUM_CHARSETS - 2;
        }

        break;
      }

      case IKEY_PAGEDOWN:
      {
        if(select_charset)
        {
          current_charset++;

          if(current_charset == NUM_CHARSETS - 1)
            current_charset = 0;
        }

        break;
      }

      case IKEY_KP_MINUS:
      case IKEY_MINUS:
      {
        if(allow_multichar && (width != 1 || height != 1))
        {
          // Move backward in tile increment.
          if(!get_shift_status(keycode_internal))
            current = char_select_next_tile(current, -1, width, height);
          break;
        }
      }
      /* fall-through */

      case IKEY_KP_PLUS:
      case IKEY_EQUALS:
      {
        if(allow_multichar && (width != 1 || height != 1))
        {
          // Move forward in a tile increment.
          if(!get_shift_status(keycode_internal))
            current = char_select_next_tile(current, 1, width, height);
          break;
        }
      }
      /* fall-through */

      default:
      {
        if(!current_charset)
        {
          // If this is from 32 to 255, jump there.
          int key_char = get_key(keycode_text_ascii);

          if(allow_multichar && (width != 1 || height != 1))
          {
            // Ignore text from these keys in situations where they are used
            // for tile movement instead...
            if(key_char == '=' || key_char == '+' || key_char == '-' ||
             get_key_status(keycode_internal, IKEY_MINUS) ||
             get_key_status(keycode_internal, IKEY_EQUALS) ||
             get_key_status(keycode_internal, IKEY_KP_MINUS) ||
             get_key_status(keycode_internal, IKEY_KP_PLUS))
              break;
          }

          if(key_char >= 32 && key_char <= 255)
            current = key_char;
        }

        break;
      }
    }
  } while(!exit);

  // Get rid of the extra layers required for extended charsets
  if(select_charset)
  {
    destruct_extra_layers(chars_layer);
    destruct_extra_layers(pal_layer);
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();
  restore_screen();
  cursor_off();

  return current;
}

int char_selection(int current)
{
  return char_selection_ext(current, 1, NULL, NULL, NULL, -1);
}

// Display a simple string input window. Return 0 on enter, -1 if cancelled.
int input_window(struct world *mzx_world, const char *title,
 char *buffer, int max_len)
{
  boolean two_lines = false;
  int title_len = strlen(title);
  int x = 16;
  int y = 11;
  int w;
  int h = 3;
  int ret;

  m_show();
  save_screen();

  if(title_len > 71)
    title_len = 71;

  if(max_len > 70)
    max_len = 70;

  // Title + spacing + input length + extra char for intake cursor + 4 (border)
  w = title_len + 1 + max_len + 1 + 4;

  // Split label and input box onto separate lines if the box would be too wide.
  if(w > 74)
  {
    w = MAX(title_len, max_len + 1) + 4;
    two_lines = true;
    h++;
  }

  // Center wide boxes.
  if(x > (81 - w) / 2)
    x = (81 - w) / 2;

  draw_window_box(x, y, (x + w - 1), (y + h - 1),
   DI_INPUT_BOX, DI_INPUT_BOX_DARK, DI_INPUT_BOX_CORNER, true, true);

  x += 2;
  y += 1;
  write_string(title, x, y, DI_INPUT_BOX_LABEL, false);

  if(two_lines)
    y++;
  else
    x += title_len + 1;

  ret = intake(mzx_world, buffer, max_len, max_len, x, y, 15,
   INTK_EXIT_ENTER_ESC, true, NULL);

  restore_screen();

  if(ret == IKEY_ESCAPE || get_exit_status())
    return -1;

  return 0;
}

__editor_maybe_static void construct_element(struct element *e, int x, int y,
 int width, int height,
 void (* draw_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int color, int active),
 int (* key_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int key),
 int (* click_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int mouse_button, int mouse_x, int mouse_y,
  int new_active),
 int (* drag_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int mouse_button, int mouse_x, int mouse_y),
 int (* idle_function)(struct world *mzx_world, struct dialog *di,
  struct element *e))
{
  e->x = x;
  e->y = y;
  e->width = width;
  e->height = height;
  e->draw_function = draw_function;
  e->key_function = key_function;
  e->click_function = click_function;
  e->drag_function = drag_function;
  e->idle_function = idle_function;
}

static void fill_vid_usage(struct dialog *di, struct element *e,
 signed char *vid_usage, int vid_fill)
{
  int x = di->x + e->x;
  int y = di->y + e->y;
  signed char *vid_ptr = vid_usage + (y * 80) + x;
  int i;

  for(i = 0; i < e->height; i++)
  {
    memset(vid_ptr, vid_fill, e->width);
    vid_ptr += 80;
  }
}

// Prototype- Internal function that displays a dialog box element.
void display_element(struct world *mzx_world, int type, int x, int y,
 char *str, int p1, int p2, void *value, int active, int curr_check,
 int set_vid_usage, int space_label);

static void unhighlight_element(struct world *mzx_world, struct dialog *di,
 int current_element_num)
{
  (di->elements[current_element_num])->draw_function(mzx_world, di,
   di->elements[current_element_num], DI_NONACTIVE, 0);
}

static void highlight_element(struct world *mzx_world, struct dialog *di,
 int current_element_num)
{
  (di->elements[current_element_num])->draw_function(mzx_world, di,
   di->elements[current_element_num], DI_ACTIVE, 1);
}

static int find_first_element(struct world *mzx_world, struct dialog *di,
 int current_element_num)
{
  int i;

  unhighlight_element(mzx_world, di, current_element_num);

  for(i = 0; i < di->num_elements; i++)
  {
    if((di->elements[i])->key_function)
      return i;
  }

  highlight_element(mzx_world, di, i);

  return -1;
}

static int find_last_element(struct world *mzx_world, struct dialog *di,
 int current_element_num)
{
  int i;

  unhighlight_element(mzx_world, di, current_element_num);

  for(i = di->num_elements - 1; i >= 0; i--)
  {
    if((di->elements[i])->key_function)
      return i;
  }

  highlight_element(mzx_world, di, i);

  return -1;
}

static int change_current_element(struct world *mzx_world, struct dialog *di,
 int current_element_num, int displacement)
{
  int increment = 1;
  int i = 0;

  unhighlight_element(mzx_world, di, current_element_num);

  if(displacement < 0)
    increment = -1;

  while(i != displacement)
  {
    current_element_num += increment;

    if(current_element_num < 0)
      current_element_num = di->num_elements - 1;

    if(current_element_num == di->num_elements)
      current_element_num = 0;

    if((di->elements[current_element_num])->key_function)
      i += increment;
  }

  highlight_element(mzx_world, di, current_element_num);

  return current_element_num;
}

int run_dialog(struct world *mzx_world, struct dialog *di)
{
  int exit;
  int mouse_press;
  int mouse_drag_state;
  int x = di->x;
  int y = di->y;
  int title_x_offset = x + (di->width / 2) - ((int)strlen(di->title) / 2);
  struct element *current_element = di->elements[di->current_element];

  int current_element_num = di->current_element;
  signed char vid_usage[2000];
  int current_key, new_key;
  int i;

  if(get_context(NULL) == CTX_MAIN)
    set_context(CTX_DIALOG_BOX);
  else
    set_context(get_context(NULL));

  m_show();

  save_screen();

  di->done = 0;

  // Draw main box and title

  draw_window_box(x, y, x + di->width - 1,
   y + di->height - 1, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);

  write_string(di->title, title_x_offset, y, DI_TITLE, 0);
  draw_char(' ', DI_TITLE, title_x_offset - 1, y);
  draw_char(' ', DI_TITLE, title_x_offset + (unsigned int)strlen(di->title), y);

  memset(vid_usage, -1, 2000);

  // Draw elements and set vid usage
  for(i = 0; i < di->num_elements; i++)
  {
    unhighlight_element(mzx_world, di, i);
    fill_vid_usage(di, di->elements[i], vid_usage, i);
  }

  do
  {
    cursor_off();
    highlight_element(mzx_world, di, current_element_num);
    update_screen();

    current_element = di->elements[current_element_num];
    update_event_status_delay();
    current_key = get_key(keycode_internal_wrt_numlock);

    if(!current_key && has_unicode_input())
      current_key = IKEY_UNICODE;

    new_key = get_joystick_ui_key();
    if(new_key)
      current_key = new_key;

    new_key = 0;

    if(current_element->idle_function)
    {
      new_key =
       current_element->idle_function(mzx_world, di,
       current_element);

      if(new_key > 0)
        current_key = new_key;
    }

    exit = get_exit_status();

    mouse_press = get_mouse_press_ext();
    mouse_drag_state = get_mouse_drag();

    if(mouse_drag_state &&
     (mouse_press <= MOUSE_BUTTON_RIGHT) &&
     (current_element->drag_function))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      new_key = current_element->drag_function(mzx_world,
       di, current_element, mouse_press,
       mouse_x - di->x - current_element->x,
       mouse_y - di->y - current_element->y);

      if(new_key > 0)
        current_key = new_key;
    }
    else

    if((mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT)
     && !mouse_drag_state)
     || (new_key == -1))
    {
      do
      {
        int mouse_x, mouse_y;
        int element_under;
        get_mouse_position(&mouse_x, &mouse_y);

        element_under = vid_usage[(mouse_y * 80) + mouse_x];
        new_key = 0;

        if((element_under != -1) &&
         (element_under != current_element_num) &&
         ((di->elements[element_under])->click_function))
        {
          unhighlight_element(mzx_world, di, current_element_num);
          current_element_num = element_under;
          current_element = di->elements[element_under];
          highlight_element(mzx_world, di, element_under);

          new_key = current_element->click_function(mzx_world, di,
           current_element, mouse_press,
           mouse_x - di->x - current_element->x,
           mouse_y - di->y - current_element->y, 1);

          if(new_key > 0)
            current_key = new_key;
        }
        else

        if((element_under != -1) &&
         ((di->elements[element_under])->click_function))
        {
          current_element_num = element_under;
          current_element = di->elements[element_under];

          if(current_element->click_function)
          {
            new_key = current_element->click_function(mzx_world, di,
             current_element, mouse_press,
             mouse_x - di->x - current_element->x,
             mouse_y - di->y - current_element->y, 0);

            if(new_key > 0)
              current_key = new_key;
          }
        }
      } while(new_key == -1);

      if(get_exit_status())
        exit = 1;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      current_key = IKEY_UP;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      current_key = IKEY_DOWN;
    }

    if(current_element->key_function && current_key)
    {
      current_key =
       current_element->key_function(mzx_world, di,
       current_element, current_key);
   }

    di->current_element = current_element_num;

    if(di->idle_function)
    {
      current_key =
       di->idle_function(mzx_world, di, current_key);

      current_element_num = di->current_element;
    }

    switch(current_key)
    {
      case IKEY_TAB: // Tab
      {
        if(get_shift_status(keycode_internal))
        {
          current_element_num = change_current_element(mzx_world,
           di, current_element_num, -1);
        }
        else
        {
          current_element_num = change_current_element(mzx_world,
           di, current_element_num, 1);
        }
        break;
      }

      case IKEY_SPACE:
      case IKEY_RETURN:
      case IKEY_PAGEDOWN:
      case IKEY_RIGHT:
      case IKEY_DOWN:
      {
        current_element_num = change_current_element(mzx_world,
         di, current_element_num, 1);
        break;
      }

      case IKEY_PAGEUP:
      case IKEY_LEFT:
      case IKEY_UP:
      {
        current_element_num = change_current_element(mzx_world,
         di, current_element_num, -1);
        break;
      }

      case IKEY_HOME:
      {
        current_element_num = find_first_element(mzx_world, di,
         current_element_num);
        break;
      }

      case IKEY_END:
      {
        current_element_num = find_last_element(mzx_world, di,
         current_element_num);
        break;
      }

      case IKEY_ESCAPE: // ESC
      {
        // Only work on press.  Ignore autorepeat.
        //if(get_key_status(keycode_internal, IKEY_ESCAPE) == 1)
          exit = 1;

        break;
      }

#ifdef CONFIG_HELPSYS
      case IKEY_F1: // F1
      {
        if(allow_help_system(mzx_world, true))
        {
          // FIXME context
          help_system(NULL, mzx_world);
        }
        break;
      }
#endif

      default:
      {
        break;
      }
    }

    // ESC or exit event
    if(exit)
    {
      force_release_all_keys();
      pop_context();
      cursor_off();
      return -1;
    }

  } while(di->done != 1);

  pop_context();
  cursor_off();

  return di->return_value;
}

static int find_entry(const char **choices, char *name, int total_num)
{
  int current_entry;
  int cmpval = 0;
  int offset = 0;
  size_t name_length = strlen(name);

  // Hack so seeking works on the string counter debugger without
  // having to press '$'. Probably shouldn't be here, but oh well.
  // Most lists are alphabetically sorted or have things like (new)
  // at the end, so only enable if the first and last elements start
  // with '$'.

  if(total_num && choices[0][0] == '$' && choices[total_num - 1][0] == '$')
  {
    offset = 1;
  }

  for(current_entry = 0; current_entry < total_num; current_entry++)
  {
    // Hack to avoid seeking to drive letters under Windows. This
    // is a terrible place for it, and I'd like to tear it out, but
    // it would likely upset a lot of the existing code.
    // (note: Exo added this hack, not me!)
#ifdef __WIN32__
    if(choices[current_entry][1] == ':')
      continue;
    else
#endif

    cmpval = strncasecmp(name, choices[current_entry] + offset, name_length);

    if(cmpval == 0)
    {
      return current_entry;
    }
  }

  return -1;
}

// "Member" functions for GUI elements

static void draw_label(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct label_element *src = (struct label_element *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  if(src->respect_colors)
    color_string_ext(src->text, x, y, DI_TEXT, true, PRO_CH, 16);
  else
    write_string_ext(src->text, x, y, DI_TEXT, true, PRO_CH, 16);
}

static void draw_input_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct input_box *src = (struct input_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int question_length = (int)strlen(src->question) + di->pad_space;

  write_string(src->question, x, y, color, 0);
  fill_line(src->max_length + 1, x + question_length, y,
   32, DI_INPUT);
  write_string(src->result, x + question_length, y,
   DI_INPUT, 0);
}

static void draw_radio_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct radio_button *src = (struct radio_button *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int i;

  for(i = 0; i < src->num_choices; i++)
  {
    if(i != *(src->result))
    {
      write_string(radio_off, x, y + i, DI_NONACTIVE, 0);
      write_string(src->choices[i], x + 4, y + i,
       DI_NONACTIVE, 0);
    }
    else
    {
      write_string(radio_on, x, y + *(src->result),
       DI_NONACTIVE, 0);
      write_string(src->choices[i], x + 4, y + i,
       DI_NONACTIVE, 0);
    }
  }

  if(active)
  {
    color_line(src->max_length + 4, x, y + *(src->result),
     DI_ACTIVE);
    cursor_hint(x + 1, y + *(src->result));
  }
}

static void draw_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct button *src = (struct button *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  if(active)
    color = DI_ACTIVEBUTTON;
  else
    color = DI_BUTTON;

  write_string(src->label, x + 1, y, color, 0);
  draw_char(' ', color, x, y);
  draw_char(' ', color, x + (unsigned int)strlen(src->label) + 1, y);

  if(active)
    cursor_hint(x + 1, y);
}

static void draw_number_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct number_box *src = (struct number_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int increment = 1;
  int i;

  if(src->type == NUMBER_BOX_MULT_FIVE)
    increment = 5;

  write_string(src->question, x, y, color, 0);
  if(active)
    cursor_hint(x, y);

  x += (int)strlen(src->question) + di->pad_space;

  if(src->type == NUMBER_LINE)
  {
    // Draw a number line
    for(i = 1; i <= src->upper_limit; i++)
    {
      if(i == *(src->result))
        draw_char('0' + i, DI_ACTIVE, x + i - 1, y);
      else
        draw_char('0' + i, DI_NONACTIVE, x + i - 1, y);
    }
  }
  else

  if(src->type == NUMBER_SLIDER)
  {
    int upper_x = x + (src->upper_limit - src->lower_limit) + 2;
    int slider_x = x + (*(src->result) - src->lower_limit);
    int slider_col = active ? DI_ARROWBUTTON : DI_NUMERIC;

    for(i = x + 1; i < upper_x; i++)
      draw_char('\xC4', DI_DARK, i, y);

    write_number(src->lower_limit, DI_NONACTIVE, x, y, 0, false, 10);
    write_number(src->upper_limit, DI_NONACTIVE, upper_x, y, 0, false, 10);

    draw_char(' ', slider_col, slider_x, y);
    draw_char(' ', slider_col, slider_x + 2 + (*(src->result) >= 10), y);
    write_number(*(src->result), slider_col, slider_x + 1, y, 0, false, 10);
  }

  else
  {
    // Draw a number
    char num_buffer[32];
    if(!src->is_null)
      sprintf(num_buffer, "%d", *(src->result) * increment);
    else
      sprintf(num_buffer, " ");
    fill_line(7, x, y, 32, DI_NUMERIC);
    write_string(num_buffer, x + 6 - (unsigned int)strlen(num_buffer), y,
     DI_NUMERIC, 0);
    // Buttons
    write_string(num_buttons, x + 7, y, DI_ARROWBUTTON, 0);
  }
}

static int get_slot_state(int slot, boolean *highlighted_slots,
 boolean *disabled_slots)
{
  if(disabled_slots != NULL && disabled_slots[slot])
    return SLOT_DISABLED;

  if(highlighted_slots != NULL && highlighted_slots[slot])
    return SLOT_HIGHLIGHT;

  return SLOT_NORMAL;
}

static void draw_slot_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct slot_selector *src = (struct slot_selector *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int slot_x;
  int slot_y = y + SLOTSEL_BAR_Y;
  int i;
  int slot_color = DI_SLOTSEL_NORMAL;

  write_string(src->title, x, y, color, 0);

  for(i = 0; i < src->num_slots; i++)
  {
    slot_x = x + (i * 4);
    switch(get_slot_state(i, src->highlighted_slots, src->disabled_slots))
    {
      case SLOT_DISABLED:
      {
        slot_color = DI_SLOTSEL_DISABLE;
        break;
      }

      case SLOT_HIGHLIGHT:
      {
        slot_color = i == src->selected_slot
         ? DI_ACTIVEBUTTON
         : DI_SLOTSEL_HIGHLIGHT;
        break;
      }

      case SLOT_NORMAL:
      {
        slot_color = i == src->selected_slot
         ? DI_ARROWBUTTON
         : DI_SLOTSEL_NORMAL;
        break;
      }
    }

    fill_line(4, slot_x, slot_y, 0, slot_color);
    write_number(i + 1, slot_color, slot_x + 2, slot_y, 1, true, 10);

    if(active && i == src->selected_slot)
      cursor_hint(slot_x + 1, slot_y);
  }
}

static void draw_file_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct file_selector *src = (struct file_selector *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int width = e->width;
  char *path = src->result;
  int len = strlen(path);

  write_string(src->title, x, y, color, 0);
  fill_line(width, x, y + 1, 32, DI_LIST);
  if(active)
    cursor_hint(x, y);

  if(path[0])
  {
    if(len > width-5)
    {
      path = path + len - (width-5);
    }
    write_string(path, x + 1, y + 1, DI_LIST, 0);
  }
  else
  {
    write_string(src->none_mesg, x + 1, y + 1, DI_LIST, 0);
  }

  // Draw button
  write_string(list_button, x + width - 3, y + 1, DI_ARROWBUTTON, 0);
}

#define MAX_NAME_BUFFER 512

static void draw_list_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int color, int active)
{
  struct list_box *src = (struct list_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int choice_length = src->choice_length;
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;
  int current_choice = *(src->result);
  int scroll_offset = src->scroll_offset;
  const char **choices = src->choices;
  int i, num_draw;
  int current_in_window = current_choice - scroll_offset;
  char name_buffer[MAX_NAME_BUFFER];
  int draw_width = MIN(choice_length, MAX_NAME_BUFFER);

  num_draw = num_choices_visible;

  if(num_choices_visible >= num_choices)
    num_draw = num_choices;

  current_in_window = current_choice - scroll_offset;

  // Draw choices
  for(i = 0; i < num_draw; i++)
  {
    fill_line(choice_length, x, y + i, 32, DI_LIST);
    snprintf(name_buffer, draw_width, "%s", choices[scroll_offset + i]);
    name_buffer[draw_width - 1] = '\0';

    if(src->respect_color_codes)
      color_string(name_buffer, x, y + i, DI_LIST);
    else
      write_string(name_buffer, x, y + i, DI_LIST, false);
  }

  for(; i < num_choices_visible; i++)
  {
    fill_line(choice_length, x, y + i, 32, DI_LIST);
  }

  if(num_choices)
  {
    if(active)
      color = DI_ACTIVELIST;
    else
      color = DI_SEMIACTIVELIST;

    fill_line(choice_length, x, y + current_in_window,
     32, color);
    if(active)
      cursor_hint(x, y + current_in_window);

    snprintf(name_buffer, draw_width, "%s", choices[current_choice]);
    name_buffer[draw_width - 1] = '\0';

    if(src->respect_color_codes)
      color_string(name_buffer, x, y + current_in_window, color);
    else
      write_string(name_buffer, x, y + current_in_window, color, false);
  }

  if(num_choices > num_choices_visible)
  {
    int side_length = num_choices_visible - 2;
    int progress_bar_size = (side_length * side_length +
     (side_length / 2)) / num_choices;
    int progress_bar_offset = ((scroll_offset *
     (side_length - progress_bar_size)) +
     ((num_choices - num_choices_visible) / 2)) /
     (num_choices - num_choices_visible) + y + 1;

    draw_char(pc_top_arrow, DI_PCARROW, x + choice_length, y);
    draw_char(pc_bottom_arrow, DI_PCARROW, x + choice_length,
     y + side_length + 1);

    for(i = 0; i < side_length; i++)
    {
      draw_char(pc_filler, DI_PCFILLER, x + choice_length, y + i + 1);
    }

    for(i = 0; i < progress_bar_size; i++,
     progress_bar_offset++)
    {
      draw_char(pc_meter, DI_PCDOT, x + choice_length,
       progress_bar_offset);
    }
  }
}

static int key_input_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct input_box *src = (struct input_box *)e;

  if(get_alt_status(keycode_internal) && (key == IKEY_t) &&
   di->sfx_test_for_input)
  {
    // Play a sfx
    sfx_clear_queue();
    play_string(src->result, 0);
  }

  return key;
}

static int key_radio_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct radio_button *src = (struct radio_button *)e;

  switch(key)
  {
    case IKEY_PAGEUP:
    {
      *(src->result) = 0;
      break;
    }

    case IKEY_LEFT:
    case IKEY_UP:
    {
      if(*(src->result))
        (*(src->result))--;

      break;
    }

    case IKEY_PAGEDOWN:
    {
      *(src->result) = src->num_choices - 1;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_DOWN:
    {
      if(*(src->result) < (src->num_choices - 1))
        (*(src->result))++;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct button *src = (struct button *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      // 0=not pressed (i.e. this is a fake press from clicking)
      // 1=pressed
      // 2=repeat. We want to ignore that.
      if (get_key_status(keycode_internal_wrt_numlock, key) > 1)
        return 0;

      // Flag that the dialog is done processing
      di->done = 1;
      di->return_value = src->return_value;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_number_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct number_box *src = (struct number_box *)e;
  int increment_value = 0;
  int current_value = *(src->result);

  switch(key)
  {
    case IKEY_HOME:
    {
      *(src->result) = src->lower_limit;
      break;
    }

    case IKEY_END:
    {
      *(src->result) = src->upper_limit;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_UP:
    {
      if(get_alt_status(keycode_internal) ||
       get_ctrl_status(keycode_internal))
      {
        increment_value = 10;
      }
      else
      {
        increment_value = 1;
      }

      break;
    }

    case IKEY_PAGEUP:
    {
      increment_value = 100;
      break;
    }

    case IKEY_LEFT:
    case IKEY_DOWN:
    {
      if(get_alt_status(keycode_internal) ||
       get_ctrl_status(keycode_internal))
      {
        increment_value = -10;
      }
      else
      {
        increment_value = -1;
      }

      break;
    }

    case IKEY_PAGEDOWN:
    {
      increment_value = -100;
      break;
    }

    case IKEY_BACKSPACE:
    {
      int result = current_value / 10;
      if(result == 0 || result < src->lower_limit)
      {
        result = src->lower_limit;
        if(src->upper_limit > 9)
          src->is_null = true;
      }

      *(src->result) = result;

      // Fall through to allow for an empty box.
    }

    /* fallthrough */

    default:
    {
      int key_char = get_key(keycode_text_ascii);

      if((key_char >= '0') && (key_char <= '9'))
      {
        if(current_value == src->upper_limit || src->is_null)
        {
          current_value = (key_char - '0');
        }
        else
        {
          current_value = (current_value * 10) + (key_char - '0');
        }

        if(src->type == NUMBER_BOX_MULT_FIVE)
          current_value -= current_value % 5;

        if(current_value < src->lower_limit)
          current_value = src->lower_limit;

        if(current_value > src->upper_limit)
          current_value = src->upper_limit;

        *(src->result) = current_value;
        break;
      }

      if(key != IKEY_BACKSPACE &&
       !get_shift_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal))
        src->is_null = false;

      return key;
    }
  }

  if(increment_value > 0 && src->is_null)
    increment_value -= src->lower_limit;
  src->is_null = false;

  if(increment_value)
  {
    current_value += increment_value;

    if(current_value < src->lower_limit)
      current_value = src->lower_limit;

    if(current_value > src->upper_limit)
      current_value = src->upper_limit;

    *(src->result) = current_value;
  }
  return 0;
}

static int key_slot_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct slot_selector *src = (struct slot_selector *)e;
  int this_slot = src->selected_slot;
  int target_slot;
  int i;

  switch(key)
  {
    case IKEY_LEFT:
    {
      for(i = 0; i < SLOTSEL_NUM_SLOTS; i++)
      {
        if(--src->selected_slot < 0)
          src->selected_slot = src->num_slots - 1;

        if(src->selected_slot == this_slot)
          break;

        if(get_slot_state(src->selected_slot, src->highlighted_slots,
         src->disabled_slots) != SLOT_DISABLED)
          break;
      }
      break;
    }

    case IKEY_RIGHT:
    {
      for(i = 0; i < SLOTSEL_NUM_SLOTS; i++)
      {
        if(++src->selected_slot >= src->num_slots)
          src->selected_slot = 0;

        if(src->selected_slot == this_slot)
          break;

        if(get_slot_state(src->selected_slot, src->highlighted_slots,
         src->disabled_slots) != SLOT_DISABLED)
          break;
      }
      break;
    }

    case IKEY_1:
    case IKEY_2:
    case IKEY_3:
    case IKEY_4:
    case IKEY_5:
    case IKEY_6:
    case IKEY_7:
    case IKEY_8:
    case IKEY_9:
    {
      target_slot = key - IKEY_1;
      if(src->save || get_slot_state(target_slot, src->highlighted_slots,
       src->disabled_slots) != SLOT_DISABLED)
        src->selected_slot = target_slot;
      break;
    }

    case IKEY_0:
    {
      // Special case for the 10th element.
      if(src->save || get_slot_state(9, src->highlighted_slots,
       src->disabled_slots) != SLOT_DISABLED)
        src->selected_slot = 9;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_file_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct file_selector *src = (struct file_selector *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      char new_path[MAX_PATH] = { 0 };
      strcpy(new_path, src->base_path);

      if(!choose_file_ch(mzx_world, src->file_manager_exts, new_path,
       src->file_manager_title, ALLOW_SUBDIRS))
      {
        strcpy(src->result, new_path);
        di->done = 1;
        di->return_value = src->return_value;
      }

      break;
    }

    case IKEY_DELETE:
    case IKEY_BACKSPACE:
    {
      if(src->allow_unset)
        src->result[0] = 0;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_list_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int key)
{
  struct list_box *src = (struct list_box *)e;
  int current_choice = *(src->result);
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;

  switch(key)
  {
    case IKEY_UP:
    {
      if(current_choice)
      {
        if(src->scroll_offset == current_choice)
          (src->scroll_offset)--;
        current_choice--;
      }
      break;
    }

    case IKEY_DOWN:
    {
      if(current_choice < (num_choices - 1))
      {
        if(current_choice ==
         (src->scroll_offset + num_choices_visible - 1))
        {
          (src->scroll_offset)++;
        }
        current_choice++;
      }
      break;
    }

    case IKEY_LEFT:
    {
      return IKEY_LEFT;
    }

    case IKEY_RIGHT:
    {
      return IKEY_RIGHT;
    }

    case IKEY_HOME:
    {
      current_choice = 0;
      src->scroll_offset = 0;
      break;
    }

    case IKEY_END:
    {
      current_choice = num_choices - 1;
      src->scroll_offset =
       num_choices - num_choices_visible;
      if(src->scroll_offset < 0)
        src->scroll_offset = 0;
      break;
    }

    case IKEY_PAGEUP:
    {
      current_choice -= num_choices_visible;
      src->scroll_offset -= num_choices_visible;

      if(src->scroll_offset < 0)
      {
        src->scroll_offset = 0;
      }

      if(current_choice < 0)
      {
        current_choice = 0;
      }
      break;
    }

    case IKEY_PAGEDOWN:
    {
      current_choice += num_choices_visible;
      src->scroll_offset += num_choices_visible;

      if(src->scroll_offset + num_choices_visible >
       num_choices)
      {
        src->scroll_offset =
         num_choices - num_choices_visible;
        if(src->scroll_offset < 0)
          src->scroll_offset = 0;
      }

      if(current_choice >= num_choices)
      {
        current_choice = num_choices - 1;
      }

      break;
    }

    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      di->return_value = src->return_value;
      di->done = 1;
      break;
    }

    default:
    {
      int key_char = get_key(keycode_text_ascii);
      if(!get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && (key_char >= 32))
      {
        char *key_buffer = src->key_buffer;
        int key_position = src->key_position;
        int last_keypress_time = src->last_keypress_time;
        int ticks = get_ticks();
        int new_choice;

        if(((ticks - last_keypress_time) >= TIME_SUSPEND) ||
         (key_position == 63))
        {
          // Go back to zero
          key_position = 0;
        }

        src->last_keypress_time = ticks;
        key_buffer[key_position] = key_char;
        key_position++;
        key_buffer[key_position] = 0;
        src->key_position = key_position;

        new_choice =
         find_entry(src->choices, key_buffer, num_choices);

        if(new_choice != -1)
          current_choice = new_choice;

        if((current_choice < src->scroll_offset) ||
         (current_choice >= (src->scroll_offset +
         num_choices_visible)))
        {
          src->scroll_offset =
           current_choice - num_choices_visible / 2;

          if(src->scroll_offset < 0)
            src->scroll_offset = 0;

          if(src->scroll_offset + num_choices_visible >
           num_choices)
          {
            src->scroll_offset =
             num_choices - num_choices_visible;
            if(src->scroll_offset < 0)
              src->scroll_offset = 0;
          }
        }
      }
      else
      {
        return key;
      }
      break;
    }
  }

  *(src->result) = current_choice;

  if(src->result_offset)
    *(src->result_offset) = src->scroll_offset;

  return 0;
}

static int click_input_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct input_box *src = (struct input_box *)e;
  size_t question_len = strlen(src->question);
  int start_x = mouse_x - (int)question_len;
  int x = di->x + e->x;
  int y = di->y + e->y;

  if(start_x >= 0)
  {
    return intake(mzx_world, src->result, src->max_length, src->max_length,
     x + (int)question_len + di->pad_space, y, DI_INPUT, INTK_EXIT_ANY, true,
     &start_x);
  }
  else
  {
    return 0;
  }
}

static int click_radio_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct radio_button *src = (struct radio_button *)e;
  *(src->result) = mouse_y;

  return 0;
}

static int click_button(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  if(!new_active)
  {
    return IKEY_RETURN;
  }

  return 0;
}

static int click_number_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct number_box *src = (struct number_box *)e;
  mouse_x -= (int)strlen(src->question) + 7;

  if(src->type == NUMBER_LINE)
  {
    // Select number IF on the number line itself
    mouse_x += 7;
    if((mouse_x < src->upper_limit) && (mouse_x >= 0))
      *(src->result) = mouse_x + 1;
  }
  else

  if(src->type == NUMBER_SLIDER)
  {
    mouse_x += 7;
    if(mouse_x >= 0)
      *(src->result) = CLAMP(mouse_x - 1 + src->lower_limit,
       src->lower_limit, src->upper_limit);
  }
  else

  if((mouse_x >= 0) && (mouse_x <= 2))
  {
    return IKEY_UP;
  }
  else

  if((mouse_x >= 3) && (mouse_x <= 5))
  {
    return IKEY_DOWN;
  }

  return 0;
}

static int drag_number_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y)
{
  struct number_box *src = (struct number_box *)e;
  int mouse_press = get_mouse_press();

  mouse_x -= (int)strlen(src->question) + 7;

  if(src->type == NUMBER_LINE)
  {
    // Select number IF on the number line itself
    mouse_x += 7;
    if((mouse_x < src->upper_limit) && (mouse_x >= 0))
      *(src->result) = mouse_x + 1;
  }
  else

  if(src->type == NUMBER_SLIDER)
  {
    mouse_x += 7;
    if(mouse_x >= 0)
      *(src->result) = CLAMP(mouse_x - 1 + src->lower_limit,
       src->lower_limit, src->upper_limit);
  }
  else

  // get_mouse_press() has repeating, which we want here.
  if((mouse_x >= 0) && (mouse_x <= 2) && mouse_press)
  {
    return IKEY_UP;
  }
  else

  if((mouse_x >= 3) && (mouse_x <= 5) && mouse_press)
  {
    return IKEY_DOWN;
  }

  return 0;
}

static boolean slot_save_exists(const char *file, int slot)
{
  struct stat s;
  char path[MAX_PATH];

  snprintf(path, MAX_PATH, "%s%i%s",
   cur_slot_prefix, slot, get_config()->save_slots_ext);
  return !vstat(path, &s);
}

static int click_slot_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct slot_selector *src = (struct slot_selector *)e;
  int target_slot = 0;

  if((mouse_x < 0) || (mouse_x >= 40) || (mouse_y != SLOTSEL_BAR_Y))
    return 0;

  target_slot = mouse_x / 4;

  if(src->save || slot_save_exists(cur_slot_prefix, target_slot))
  {
    src->selected_slot = target_slot;
  }

  return 0;
}

static int drag_slot_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y)
{
  return click_slot_selector(mzx_world, di, e, mouse_button, mouse_x, mouse_y, 0);
}

static int click_file_selector(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

static int click_list_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  struct list_box *src = (struct list_box *)e;
  int scroll_offset = src->scroll_offset;
  int choice_length = src->choice_length;
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;
  int current_in_window = *(src->result) - scroll_offset;

  src->clicked_scrollbar = 0;

  if((num_choices > num_choices_visible) &&
   (mouse_x == choice_length))
  {
    if(mouse_y == 0)
      return IKEY_UP;

    if(mouse_y == (num_choices_visible - 1))
      return IKEY_DOWN;

    src->clicked_scrollbar = 1;

    scroll_offset =
     (mouse_y - 1) *
     (num_choices + ((num_choices_visible - 2) / 2)) /
     (num_choices_visible - 2) - (num_choices_visible / 2);

    if(scroll_offset < 0)
      scroll_offset = 0;

    if(scroll_offset + num_choices_visible >
     num_choices)
    {
      scroll_offset = num_choices - num_choices_visible;
      if(scroll_offset < 0)
        scroll_offset = 0;
    }

    src->scroll_offset = scroll_offset;
    *(src->result) = scroll_offset + (num_choices_visible / 2);
  }
  else
  {
    if(src->num_choices)
    {
      int offset = (mouse_y - current_in_window);
      if(offset)
      {
        int current_choice = *(src->result) + offset;
        if(current_choice < src->num_choices)
          *(src->result) = current_choice;
      }
      else
      {
        if(!new_active)
        {
          di->return_value = src->return_value;
          di->done = 1;
        }
      }
    }
  }

  if(src->result_offset)
    *(src->result_offset) = scroll_offset;

  return 0;
}

static int drag_list_box(struct world *mzx_world, struct dialog *di,
 struct element *e, int mouse_button, int mouse_x, int mouse_y)
{
  struct list_box *src = (struct list_box *)e;

  if(src->clicked_scrollbar)
  {
    if((mouse_y > 0) &&
     (mouse_y < src->num_choices_visible - 1))
    {
      click_list_box(mzx_world, di, e, mouse_button,
       src->choice_length, mouse_y, 0);
      src->clicked_scrollbar = 1;
    }
  }
  else

  if((mouse_y >= 0) && (mouse_y < e->height) &&
   (mouse_x >= 0))
  {
    if( ((mouse_x < e->width - 1) &&
     (mouse_y != *(src->result) - src->scroll_offset)) ||
     ((mouse_x == e->width - 1) && mouse_button)  )
    {
      return click_list_box(mzx_world, di, e, mouse_button,
       mouse_x, mouse_y, 0);
    }
  }

  return 0;
}

static int idle_input_box(struct world *mzx_world, struct dialog *di,
 struct element *e)
{
  struct input_box *src = (struct input_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  return intake(mzx_world, src->result, src->max_length, src->max_length,
   x + (int)strlen(src->question) + di->pad_space, y, DI_INPUT, INTK_EXIT_ANY,
   true, NULL);
}

void construct_dialog(struct dialog *src, const char *title, int x, int y,
 int width, int height, struct element **elements, int num_elements,
 int start_element)
{
  src->title = title;
  src->x = x;
  src->y = y;
  src->width = width;
  src->height = height;
  src->elements = elements;
  src->num_elements = num_elements;
  src->sfx_test_for_input = 0;
  src->pad_space = 0;
  src->done = 0;
  src->return_value = 0;
  src->current_element = start_element;
  src->idle_function = NULL;
}

__editor_maybe_static void construct_dialog_ext(struct dialog *src,
 const char *title, int x, int y, int width, int height,
 struct element **elements, int num_elements, int sfx_test_for_input,
 int pad_space, int start_element,
 int (* idle_function)(struct world *mzx_world, struct dialog *di, int key))
{
  src->title = title;
  src->x = x;
  src->y = y;
  src->width = width;
  src->height = height;
  src->elements = elements;
  src->num_elements = num_elements;
  src->sfx_test_for_input = sfx_test_for_input;
  src->pad_space = pad_space;
  src->done = 0;
  src->return_value = 0;
  src->current_element = start_element;
  src->idle_function = idle_function;
}

void destruct_dialog(struct dialog *src)
{
  int i;

  for(i = 0; i < src->num_elements; i++)
  {
    free(src->elements[i]);
  }

  restore_screen();
}

struct element *construct_label(int x, int y, const char *text)
{
  struct label_element *src = cmalloc(sizeof(struct label_element));
  src->text = text;
  src->respect_colors = true;
  construct_element(&(src->e), x, y, (int)strlen(text), 1,
   draw_label, NULL, NULL, NULL, NULL);

  return (struct element *)src;
}

__editor_maybe_static struct element *construct_input_box(int x, int y,
 const char *question, int max_length, char *result)
{
  struct input_box *src = cmalloc(sizeof(struct input_box));
  src->question = question;
  src->max_length = max_length;
  src->result = result;
  construct_element(&(src->e), x, y,
   max_length + (int)strlen(question) + 1, 1,
   draw_input_box, key_input_box, click_input_box,
   NULL, idle_input_box);

  return (struct element *)src;
}

struct element *construct_radio_button(int x, int y,
 const char **choices, int num_choices, int max_length, int *result)
{
  struct radio_button *src = cmalloc(sizeof(struct radio_button));
  src->choices = choices;
  src->num_choices = num_choices;
  src->result = result;
  src->max_length = max_length;
  construct_element(&(src->e), x, y, max_length + 4,
   num_choices, draw_radio_button, key_radio_button,
   click_radio_button, NULL, NULL);

  return (struct element *)src;
}

struct element *construct_button(int x, int y, const char *label,
 int return_value)
{
  struct button *src = cmalloc(sizeof(struct button));
  src->label = label;
  src->return_value = return_value;

  construct_element(&(src->e), x, y, (int)strlen(src->label) + 2,
   1, draw_button, key_button, click_button, NULL, NULL);

  return (struct element *)src;
}

struct element *construct_number_box(int x, int y,
 const char *question, int lower_limit, int upper_limit,
 enum number_box_type type, int *result)
{
  struct number_box *src = cmalloc(sizeof(struct number_box));
  int width;

  src->question = question;
  src->lower_limit = lower_limit;
  src->upper_limit = upper_limit;
  src->type = type;
  src->result = result;
  src->is_null = false;
  width = (int)strlen(question) + 1;

  if(src->type == NUMBER_LINE)
  {
    if(lower_limit != 1 || upper_limit >= 10)
      src->type = NUMBER_BOX;

    else
      width += upper_limit - 1;
  }
  else

  if(src->type == NUMBER_SLIDER)
  {
    // Currently only allow lower limits from 0 to 9.
    if(lower_limit < 0 || lower_limit >= 10)
      src->type = NUMBER_BOX;

    else
      width += (upper_limit - lower_limit) + (upper_limit >= 10) + 2;
  }

  if(src->type == NUMBER_BOX || src->type == NUMBER_BOX_MULT_FIVE)
  {
    width += 13;
  }

  construct_element(&(src->e), x, y, width, 1,
   draw_number_box, key_number_box, click_number_box, drag_number_box, NULL);

  return (struct element *)src;
}

struct element *construct_slot_selector(int x, int y,
 const char *title, int num_slots, boolean *highlighted_slots,
 boolean *disabled_slots, int default_slot, int save)
{
  struct slot_selector *src = cmalloc(sizeof(struct slot_selector));

  src->title = title;
  src->num_slots = num_slots;
  src->highlighted_slots = highlighted_slots;
  src->disabled_slots = disabled_slots;
  src->selected_slot = default_slot;
  src->save = save;

  construct_element(&(src->e), x, y, num_slots * 4, 3, draw_slot_selector,
   key_slot_selector, click_slot_selector, drag_slot_selector, NULL);

  return (struct element *)src;
}

struct element *construct_file_selector(int x, int y,
 const char *title, const char *file_manager_title,
 const char *const *file_manager_exts, const char *none_mesg,
 int show_width, int allow_unset, const char *base_path, char *result,
 int return_value)
{
  struct file_selector *src = cmalloc(sizeof(struct file_selector));

  src->title = title;
  src->file_manager_title = file_manager_title;
  src->file_manager_exts = file_manager_exts;
  src->base_path = base_path;
  src->none_mesg = none_mesg;
  src->allow_unset = allow_unset;
  src->return_value = return_value;
  src->result = result;

  construct_element(&(src->e), x, y, show_width + 3,
   2, draw_file_selector, key_file_selector, click_file_selector,
   NULL, NULL);

  return (struct element *)src;
}

struct element *construct_list_box(int x, int y,
 const char **choices, int num_choices, int num_choices_visible,
 int choice_length, int return_value, int *result, int *result_offset,
 boolean respect_color_codes)
{
  int scroll_offset;

  struct list_box *src = cmalloc(sizeof(struct list_box));
  src->choices = choices;
  src->num_choices = num_choices;
  src->num_choices_visible = num_choices_visible;
  src->choice_length = choice_length;
  src->result = result;
  src->result_offset = result_offset;
  src->return_value = return_value;
  src->key_position = 0;
  src->last_keypress_time = 0;
  src->clicked_scrollbar = 0;
  src->respect_color_codes = respect_color_codes;

  if(result_offset)
    scroll_offset = *result_offset;

  else
    scroll_offset = *result - (num_choices_visible / 2);

  if(scroll_offset < 0)
    scroll_offset = 0;

  if(scroll_offset + num_choices_visible > num_choices)
  {
    scroll_offset =
      num_choices - num_choices_visible;

    if(scroll_offset < 0)
      scroll_offset = 0;
  }

  src->scroll_offset = scroll_offset;

  construct_element(&(src->e), x, y, choice_length + 1,
   num_choices_visible, draw_list_box, key_list_box,
   click_list_box, drag_list_box, NULL);

  return (struct element *)src;
}

// Shell for run_dialog()
int confirm(struct world *mzx_world, const char *str)
{
  struct dialog di;
  struct element *elements[2];
  int dialog_result;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  elements[0] = construct_button(15, 2, "OK", 0);
  elements[1] = construct_button(37, 2, "Cancel", 1);
  construct_dialog(&di, str, 10, 9, 60, 5, elements,
   2, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return dialog_result;
}

// Shell for run_dialog()
int confirm_input(struct world *mzx_world, const char *name,
 const char *label, char *str)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;

  int input_length = 32;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // Don't pass anything through that isn't this big plz
  str[input_length] = '\0';

  elements[0] = construct_input_box(2, 2, label, input_length, str);
  elements[1] = construct_button(15, 4, "OK", 0);
  elements[2] = construct_button(37, 4, "Cancel", 1);
  construct_dialog(&di, name, 11, 8, 57, 7, elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return dialog_result;
}

// Shell for run_dialog()
int ask_yes_no(struct world *mzx_world, char *str)
{
  struct dialog di;
  struct element *elements[2];
  int dialog_result;
  int dialog_width = 60;
  size_t str_length = strlen(str);

  int yes_button_pos;
  int no_button_pos;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // Is this string too long for the normal ask dialog?
  if(str_length > 56)
  {
    // Is the string small enough for a resized ask dialog?
    if(str_length <= 76)
    {
      // Use a bigger ask dialog to fit the string
      dialog_width = (int)str_length + 4;
      // If the dialog width is odd, bump it up to the next
      // even number, otherwise it will look uneven
      if((dialog_width % 2) == 1)
      {
        dialog_width++;
      }
    }
    else
    {
      // Clip the string at the maximum (76 characters)
      str[76] = 0;
      dialog_width = 80;
    }
  }

  // The 'Yes' button should be about 1/3 along, compensating for
  // button width
  yes_button_pos = (dialog_width - 4) / 3 - 2;

  // The 'No' button should be about 2/3 along
  no_button_pos = (dialog_width - 4) * 2 / 3 - 1;

  elements[0] = construct_button(yes_button_pos, 2, "Yes", 0);
  elements[1] = construct_button(no_button_pos, 2, "No", 1);

  construct_dialog(&di, str, 40 - dialog_width / 2, 9, dialog_width,
    5, elements, 2, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return dialog_result;
}

static int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
  char *dest_str = *((char **)dest_str_ptr);
  char *src_str = *((char **)src_str_ptr);

  if(src_str[0] == '.')
    return 1;

  if(dest_str[0] == '.')
    return -1;

  return strcasecmp(dest_str, src_str);
}

static boolean update_slot_prefix(void)
{
  char *world_name;
  size_t i;
  size_t pos = 0;
  int token_pos = -1;
  const char *fmt;
  boolean parse_token = false;
  size_t fmt_len;
  size_t world_len;

  cur_slot_prefix = crealloc(cur_slot_prefix, sizeof(char) * MAX_PATH);

  fmt = get_config()->save_slots_name;
  fmt_len = strlen(fmt);
  for(i = 0; i < fmt_len; i++)
  {
    if(fmt[i] == '%')
    {
      token_pos = i;
      break;
    }
  }

  // If no tokens were found, just copy the string over as-is.
  if(token_pos < 0)
  {
    snprintf(cur_slot_prefix, MAX_PATH, "%s", fmt);
    cur_slot_prefix[fmt_len] = 0;
    debug("update_slot_prefix: slot prefix: %s\n", cur_slot_prefix);
    return true;
  }

  // Fetch the world name, sans extension.
  world_name = strrchr(curr_file, DIR_SEPARATOR_CHAR);
  world_name = world_name ? world_name + 1 : curr_file;
  world_len = strlen(world_name);

  if(world_len < 4 || strcasecmp(world_name + world_len - 4, ".mzx"))
  {
    debug("update_slot_prefix: filename not set or is invalid\n");
    return false;
  }
  world_len -= 4;

  // We already know where the first token is. Copy everything before that
  // if possible.
  if(token_pos > 0)
  {
    memcpy(cur_slot_prefix, fmt, token_pos);
    pos += token_pos;
  }

  // Walk through the format string and replace tokens when necessary.
  for(i = pos; i < fmt_len && pos < MAX_PATH - 1; i++)
  {
    if(!parse_token && fmt[i] != '%')
    {
      cur_slot_prefix[pos++] = fmt[i];
      continue;
    }

    if(parse_token)
    {
      switch(fmt[i])
      {
        case '%':
        {
          cur_slot_prefix[pos++] = '%';
          break;
        }

        case 'w':
        {
          if(pos + world_len >= MAX_PATH)
            break;

          memcpy(cur_slot_prefix + pos, world_name, world_len);
          pos += world_len;
          break;
        }

        default:
        {
          debug("update_slot_prefix: invalid token '%c'\n", fmt[i]);
          return false;
        }
      }

      parse_token = false;
      continue;
    }

    parse_token = true;
  }

  cur_slot_prefix[pos] = '\0';
  debug("update_slot_prefix: slot prefix: %s\n", cur_slot_prefix);
  return true;
}

int slot_manager(struct world *mzx_world, char *ret,
 const char *title, boolean save)
{
  int i;
  struct dialog di;
  struct element *elements[SLOTSEL_MAX_ELEMENTS];
  int dialog_result = 0;
  int return_value = 1;
  int selected_slot = cur_slot;
  boolean *highlighted_slots = NULL;
  boolean *disabled_slots = NULL;

  if(!update_slot_prefix())
    return SLOTSEL_FILE_MANAGER_RESULT;

  while(return_value > 0)
  {
    highlighted_slots = cmalloc(sizeof(boolean) * SLOTSEL_NUM_SLOTS);
    for(i = 0; i < SLOTSEL_NUM_SLOTS; i++)
    {
      highlighted_slots[i] = slot_save_exists(cur_slot_prefix, i);
    }

    if(!save)
    {
      disabled_slots = cmalloc(sizeof(boolean) * SLOTSEL_NUM_SLOTS);
      for(i = 0; i < SLOTSEL_NUM_SLOTS; i++)
      {
        disabled_slots[i] = !highlighted_slots[i];
      }

      // If the current slot is empty, try to set it to one that isn't.
      if(disabled_slots[selected_slot])
      {
        for(i = 0; i < SLOTSEL_NUM_SLOTS; i++)
        {
          if(!disabled_slots[i])
          {
            selected_slot = i;
            break;
          }
        }
      }
    }

    elements[SLOTSEL_SELECTOR] =
     construct_slot_selector(3, 2, "Choose a slot:", SLOTSEL_NUM_SLOTS,
     highlighted_slots, disabled_slots, selected_slot, save);
    elements[SLOTSEL_OKAY_BUTTON] =
     construct_button(7, 6, "OK", SLOTSEL_OK_RESULT);
    elements[SLOTSEL_CANCEL_BUTTON] =
     construct_button(14, 6, "Cancel", SLOTSEL_CANCEL_RESULT);
    elements[SLOTSEL_FILE_MANAGER_BUTTON] =
     construct_button(25, 6, "File Manager", SLOTSEL_FILE_MANAGER_RESULT);

    construct_dialog(&di, title, 17, 8, 46, 9, elements,
     SLOTSEL_MAX_ELEMENTS, SLOTSEL_SELECTOR);
    dialog_result = run_dialog(mzx_world, &di);

    switch(dialog_result)
    {
      case SLOTSEL_OK_RESULT:
      {
        cur_slot =
         ((struct slot_selector *)elements[SLOTSEL_SELECTOR])->selected_slot;
        return_value = 0;

        // Add one more quick check to make sure that we don't try to restore an
        // empty slot.
        if(!save && disabled_slots[cur_slot])
          return_value = -1;

        break;
      }

      default:
      {
        return_value = dialog_result;
        break;
      }
    }

    force_release_all_keys();

    destruct_dialog(&di);

    free(highlighted_slots);
    free(disabled_slots);
  }

  if(return_value != 0)
    return return_value;

  snprintf(ret, MAX_PATH, "%s%i%s",
   cur_slot_prefix, cur_slot, get_config()->save_slots_ext);

  return return_value;
}

// Shell for list_menu() (copies file chosen to ret and returns -1 for ESC)

#define FILESEL_MAX_ELEMENTS  64
#define FILESEL_BASE_ELEMENTS 7
#define FILESEL_OKAY_BUTTON   0
#define FILESEL_CANCEL_BUTTON 1
#define FILESEL_FILE_LIST     2
#define FILESEL_DIR_LIST      3
#define FILESEL_FILENAME      4
#define FILESEL_FILES_LABEL   5
#define FILESEL_DIRS_LABEL    6

#define MAX_FILE_LIST_DISPLAY 56
#define MAX_FILE_LIST_DISPLAY_MZX 30

struct file_list_entry
{
  // This is the string displayed by the file list interface. It must be the
  // first thing in this struct.
  char display[MAX_FILE_LIST_DISPLAY];

  boolean is_mzx_file;
  boolean loaded_world_name;
  char filename[1];
};

/**
 * Read the name of a world file from the file.
 * Conveniently, this is typically just the first 25 bytes of the world.
 */
static void file_list_get_mzx_world_name(struct file_list_entry *entry)
{
  // Don't add this file to the cache as there may be a LOT of these.
  vfile *mzx_file = vfopen_unsafe_ext(entry->filename, "rb", V_DONT_CACHE);
  char *world_name = entry->display + MAX_FILE_LIST_DISPLAY_MZX;
  if(!mzx_file)
    return;

  if(!vfread(world_name, 24, 1, mzx_file))
    strcpy(world_name, "@0~c\x10 name read failed \x11");
  else
  if(!memcmp(world_name, "PK\x03\x04", 4))
    strcpy(world_name, "@0~c\x10 rearchived world \x11");

  vfclose(mzx_file);

  entry->display[MAX_FILE_LIST_DISPLAY - 1] = '\0';
  entry->loaded_world_name = true;
}

/**
 * Create an entry in the file list.
 */
static struct file_list_entry *construct_file_list_entry(const char *file_name)
{
  size_t file_name_length = strlen(file_name);
  struct file_list_entry *entry =
   cmalloc(sizeof(struct file_list_entry) + file_name_length);

  memcpy(entry->filename, file_name, file_name_length + 1);

  if(file_name_length >= 4 &&
   !strcasecmp(file_name + file_name_length - 4, ".mzx"))
  {
    // Special handling for MZX worlds- display their world names.
    entry->is_mzx_file = true;
    entry->loaded_world_name = false;

    // Display the filename in a smaller area and space-pad the world name.
    snprintf(entry->display, sizeof(entry->display), "%-*.*s%*s",
     MAX_FILE_LIST_DISPLAY_MZX, MAX_FILE_LIST_DISPLAY_MZX, file_name,
     MAX_FILE_LIST_DISPLAY - MAX_FILE_LIST_DISPLAY_MZX, " ");

    // Display names that are too long with ...
    if(file_name_length >= MAX_FILE_LIST_DISPLAY_MZX)
      memcpy(entry->display + MAX_FILE_LIST_DISPLAY_MZX - 4, "... ", 4);

    // TODO it would be nice to be able to delay this on the 3DS but it's
    // annoying to implement right now.
    file_list_get_mzx_world_name(entry);
  }
  else
  {
    entry->is_mzx_file = false;
    snprintf(entry->display, MAX_FILE_LIST_DISPLAY, "%s", file_name);

    // Display names that are too long with ...
    if(file_name_length > (MAX_FILE_LIST_DISPLAY - 1))
      snprintf(entry->display + MAX_FILE_LIST_DISPLAY - 4, 4, "...");
  }
  return entry;
}

static int file_dialog_function(struct world *mzx_world, struct dialog *di,
 int key)
{
  int current_element_num = di->current_element;
  struct element *current_element = di->elements[current_element_num];

  switch(current_element_num)
  {
    case FILESEL_DIR_LIST:
    case FILESEL_FILE_LIST:
    {
      struct list_box *src = (struct list_box *)current_element;
      struct input_box *dest =
       (struct input_box *)di->elements[FILESEL_FILENAME];
      struct element *e = (struct element *)dest;

      if(src->num_choices)
      {
        const char *file_name = src->choices[*(src->result)];

        if(get_alt_status(keycode_internal) &&
         !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
        {
          switch(key)
          {
            case IKEY_r:
            {
              if(current_element_num == FILESEL_DIR_LIST)
                di->return_value = 7;
              else
                di->return_value = 5;

              di->done = 1;
              break;
            }

            case IKEY_d:
            {
              if(current_element_num == FILESEL_DIR_LIST)
                di->return_value = 6;
              else
                di->return_value = 4;

              di->done = 1;
              break;
            }

            case IKEY_n:
            {
              di->done = 1;
              di->return_value = 3;
              break;
            }
          }
        }

        if(current_element_num == FILESEL_FILE_LIST)
        {
          struct file_list_entry *entry = (struct file_list_entry *)file_name;
          snprintf(dest->result, dest->max_length, "%s", entry->filename);
          dest->result[dest->max_length - 1] = '\0';
          e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);
        }
      }
      else
      {
        dest->result[0] = 0;
        e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);
      }

      if(key == IKEY_DELETE)
      {
        if(current_element_num == FILESEL_DIR_LIST)
          di->return_value = 6;
        else
          di->return_value = 4;

        di->done = 1;
      }

      if(key == IKEY_BACKSPACE)
      {
        di->done = 1;
        di->return_value = -2;
        return 0;
      }

      break;
    }

    case FILESEL_FILENAME:
    {
      struct input_box *src = (struct input_box *)current_element;
      struct list_box *dest =
       (struct list_box *)di->elements[FILESEL_FILE_LIST];
      struct element *e = (struct element *)dest;
      int current_choice = *(dest->result);
      int new_choice =
       find_entry(dest->choices, src->result, dest->num_choices);

      if(new_choice != -1)
        current_choice = new_choice;

      if((current_choice < dest->scroll_offset) ||
       (current_choice >= (dest->scroll_offset +
       dest->num_choices_visible)))
      {
        dest->scroll_offset =
         current_choice - (dest->num_choices_visible / 2);

        if(dest->scroll_offset < 0)
          dest->scroll_offset = 0;

        if(dest->scroll_offset + dest->num_choices_visible >
         dest->num_choices)
        {
          dest->scroll_offset =
           dest->num_choices - dest->num_choices_visible;
          if(dest->scroll_offset < 0)
            dest->scroll_offset = 0;
        }
      }

      *(dest->result) = current_choice;
      e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);

      if(key == IKEY_RETURN)
      {
        di->done = 1;
        di->return_value = 0;
        return 0;
      }

      break;
    }
  }

  return key;
}

static boolean remove_files(char *directory_name, boolean remove_recursively)
{
  vdir *current_dir;
  char *current_dir_name;
  struct stat file_info;
  char *file_name;
  boolean success = true;

  current_dir = vdir_open(directory_name);
  if(!current_dir)
    return false;

  current_dir_name = cmalloc(MAX_PATH);
  file_name = cmalloc(MAX_PATH);

  vgetcwd(current_dir_name, MAX_PATH);
  vchdir(directory_name);

  while(1)
  {
    if(!vdir_read(current_dir, file_name, MAX_PATH, NULL))
      break;

    if(vstat(file_name, &file_info) < 0)
      continue;

    if(!S_ISDIR(file_info.st_mode))
    {
      // Only attempt to remove contents if remove_recursively is set...
      if(!remove_recursively || vunlink(file_name))
        success = false;
    }
    else

    if(strcmp(file_name, PATH_CURRENT_DIR) && strcmp(file_name, PATH_PARENT_DIR))
    {
      if(!remove_recursively ||
       !remove_files(file_name, true) || vrmdir(file_name))
        success = false;
    }
  }

  vchdir(current_dir_name);

  free(file_name);
  free(current_dir_name);

  vdir_close(current_dir);
  return success;
}

__editor_maybe_static int file_manager(struct world *mzx_world,
 const char *const *wildcards, const char *default_ext, char *ret,
 const char *title, enum allow_dirs allow_dirs, enum allow_new allow_new,
 struct element **dialog_ext, int num_ext, int ext_height)
{
  // FIXME no buffer size parameter for ret. this function assumes MAX_PATH.
  vdir *current_dir;
  char *file_name;
  struct stat file_info;
  char *current_dir_name;
  char current_dir_short[56];
  size_t current_dir_length;
  char *return_dir_name;
  char *base_dir_name;
  int total_filenames_allocated;
  int total_dirnames_allocated;
  char **file_list;
  char **dir_list;
  int num_files;
  int num_dirs;
  size_t file_name_length;
  ssize_t ext_pos = -1;
  int chosen_file, chosen_dir;
  int dialog_result = 1;
  int return_value = 1;
  struct dialog di;
  struct element *elements[FILESEL_MAX_ELEMENTS];
  int list_length = 17 - ext_height;
  int last_element = FILESEL_FILE_LIST;
  boolean return_dir_is_base_dir = true;
  int volumes_pos;
  int i;

  // Buffers for the return file's path and name.
  char ret_path[MAX_PATH];
  char ret_file[MAX_PATH];

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // These are stack heavy so put them on the heap
  // This function is not performance sensitive anyway.
  file_name = cmalloc(MAX_PATH);

  // The current directory the file manager is in.
  current_dir_name = cmalloc(MAX_PATH);

  // Where the file manager is allowed to search.
  base_dir_name = cmalloc(MAX_PATH);

  // Where the file manager needs to return on exit.
  return_dir_name = cmalloc(MAX_PATH);

  if(allow_new != NO_NEW_FILES)
    last_element = FILESEL_FILENAME;

  // TODO: some platforms don't return this in the format MZX needs it.
  vgetcwd(return_dir_name, MAX_PATH);
  path_clean(return_dir_name, MAX_PATH);

  snprintf(current_dir_name, MAX_PATH, "%s", return_dir_name);
  current_dir_name[MAX_PATH - 1] = '\0';

  // Check the input path for a directory. If there is a directory component
  // to the input, that should be used as the base directory.
  // TODO: this is a bad way to do this. The base dir should be a param instead.
  if(path_get_directory(ret_path, MAX_PATH, ret) > 0)
  {
    path_navigate(current_dir_name, MAX_PATH, ret_path);
    return_dir_is_base_dir = false;
  }

  snprintf(base_dir_name, MAX_PATH, "%s", current_dir_name);
  base_dir_name[MAX_PATH - 1] = '\0';

  while(return_value == 1)
  {
    total_filenames_allocated = 32;
    total_dirnames_allocated = 32;
    file_list = ccalloc(32, sizeof(char *));
    dir_list = ccalloc(32, sizeof(char *));

    num_files = 0;
    num_dirs = 0;
    chosen_file = 0;
    chosen_dir = 0;

    /**
     * Change to the current directory to make things like reading MZX files
     * to display world names a little easier.
     */
    vchdir(current_dir_name);

    current_dir = vdir_open(current_dir_name);
    if(!current_dir)
      goto skip_dir;

    // Hide .. if changing directories isn't allowed or if the selected file
    // should be in the current directory or a subdirectory of it. Also hide it
    // if this is a root directory.
    if(!(allow_dirs == NO_DIRS) &&
     !(allow_dirs == ALLOW_SUBDIRS && !strcmp(current_dir_name, base_dir_name)) &&
     !path_is_root(current_dir_name))
    {
      dir_list[num_dirs] = (char *)cmalloc(strlen(PATH_PARENT_DIR) + 1);
      strcpy(dir_list[num_dirs], PATH_PARENT_DIR);
      num_dirs++;
    }

    while(1)
    {
      enum vdir_type dir_type;
      if(!vdir_read(current_dir, file_name, MAX_PATH, &dir_type))
        break;

      file_name_length = strlen(file_name);

      // Exclude ./.. and hidden files
      if(file_name[0] == '.')
        continue;

      // The file type value from dirent isn't particularly reliable; might
      // need to use stat instead.
      if(dir_type == DIR_TYPE_UNKNOWN)
      {
        if(vstat(file_name, &file_info) < 0)
          continue;

        if(S_ISDIR(file_info.st_mode))
          dir_type = DIR_TYPE_DIR;

        else
        if(S_ISREG(file_info.st_mode))
          dir_type = DIR_TYPE_FILE;
      }

      if(dir_type == DIR_TYPE_DIR)
      {
        if(allow_dirs != NO_DIRS)
        {
          dir_list[num_dirs] = cmalloc(file_name_length + 1);
          memcpy(dir_list[num_dirs], file_name, file_name_length + 1);
          dir_list[num_dirs][file_name_length] = '\0';
          num_dirs++;
        }
      }
      else

      if(dir_type == DIR_TYPE_FILE)
      {
        // Find the extension.
        ext_pos = path_get_ext_offset(file_name);

        for(i = 0; wildcards[i] != NULL; i++)
        {
          if(ext_pos >= 0 && !strcasecmp(file_name + ext_pos, wildcards[i]))
          {
            struct file_list_entry *e = construct_file_list_entry(file_name);
            file_list[num_files] = (char *)e;
            num_files++;
            break;
          }
        }
      }

      if(num_files == total_filenames_allocated)
      {
        file_list = crealloc(file_list, sizeof(char *) *
         total_filenames_allocated * 2);
        memset(file_list + total_filenames_allocated, 0,
         sizeof(char *) * total_filenames_allocated);
        total_filenames_allocated *= 2;
      }

      if(num_dirs == total_dirnames_allocated)
      {
        dir_list = crealloc(dir_list, sizeof(char *) *
         total_dirnames_allocated * 2);
        memset(dir_list + total_dirnames_allocated, 0,
         sizeof(char *) * total_dirnames_allocated);
        total_dirnames_allocated *= 2;
      }
    }

    vdir_close(current_dir);
skip_dir:

    vchdir(return_dir_name);

    qsort(file_list, num_files, sizeof(char *), sort_function);
    qsort(dir_list, num_dirs, sizeof(char *), sort_function);

    volumes_pos = num_dirs;
    if(allow_dirs == ALLOW_ALL_DIRS)
    {
      vvolumelist *volumes = vvolumelist_open();
      if(volumes)
      {
        const char *dir;

        while((dir = vvolumelist_read(volumes)))
        {
          size_t sz = strlen(dir) + 1;

          /* Strip out / for now. */
          if(!strcmp(dir, DIR_SEPARATOR))
            continue;

          dir_list[num_dirs] = (char *)cmalloc(sz);
          if(!dir_list[num_dirs])
            break;

          memcpy(dir_list[num_dirs], dir, sz);
          num_dirs++;

          if(num_dirs == total_dirnames_allocated)
          {
            dir_list = crealloc(dir_list, sizeof(char *) *
             total_dirnames_allocated * 2);
            memset(dir_list + total_dirnames_allocated, 0,
             sizeof(char *) * total_dirnames_allocated);
            total_dirnames_allocated *= 2;
          }
        }
        vvolumelist_close(volumes);
      }
    }

    current_dir_length = strlen(current_dir_name);

    if(current_dir_length > 55)
    {
      memcpy(current_dir_short, "...", 3);
      memcpy(current_dir_short + 3,
       current_dir_name + current_dir_length - 52, 52);
      current_dir_short[55] = 0;
    }
    else
    {
      memcpy(current_dir_short, current_dir_name,
       current_dir_length + 1);
    }

    // Make ret relative for the display.
    path_to_filename(ret, MAX_PATH);
    ret[55] = '\0'; // Just in case

    elements[FILESEL_FILE_LIST] =
     construct_list_box(2, 2, (const char **)file_list, num_files,
     list_length, (MAX_FILE_LIST_DISPLAY - 1), 1, &chosen_file, NULL, true);
    elements[FILESEL_DIR_LIST] =
     construct_list_box(59, 2, (const char **)dir_list, num_dirs,
     list_length, 15, 2, &chosen_dir, NULL, true);
    elements[FILESEL_FILENAME] =
     construct_input_box(2, list_length + 3, "", 55, ret);
    elements[FILESEL_OKAY_BUTTON] =
     construct_button(60, list_length + 3, "OK", 0);
    elements[FILESEL_CANCEL_BUTTON] =
     construct_button(65, list_length + 3, "Cancel", -1);
    elements[FILESEL_FILES_LABEL] =
     construct_label(2, 1, current_dir_short);
    elements[FILESEL_DIRS_LABEL] =
     construct_label(59, 1, "Directories");

    if(num_ext)
    {
      memcpy(elements + FILESEL_BASE_ELEMENTS, dialog_ext,
       sizeof(struct element *) * num_ext);
    }

    construct_dialog_ext(&di, title, 2, 1, 76, 23,
     elements, FILESEL_BASE_ELEMENTS + num_ext, 0, 0,
     last_element, file_dialog_function);

    dialog_result = run_dialog(mzx_world, &di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    switch(dialog_result)
    {
      // Pressed Backspace
      case -2:
      {
        path_navigate(current_dir_name, MAX_PATH, PATH_PARENT_DIR);
        break;
      }

      // Pressed Escape
      case -1:
      {
        return_value = -1;
        break;
      }

      // Entered File/Pressed Okay
      case 0:
      case 1:
      {
        int stat_result;

        if(di.current_element == FILESEL_FILE_LIST)
        {
          /* If a file was selected from the file list, ignore the filename
           * entry box value, which may be wrong--join the full filename to
           * the (absolute) current directory instead. */
          struct file_list_entry *e =
           (struct file_list_entry *)file_list[chosen_file];

          if(path_join(ret, MAX_PATH, current_dir_name, e->filename) < 0)
          {
            ret[0] = '\0';
            break;
          }
        }
        else
        {
          /* Entered filenames are more messy--split them into components,
           * derive a new current directory, and make ret absolute. */

          /* This function will never set ret_file to an existing directory.
           * TODO: since ret is probably relative, the internal stat() needs
           * to be performed from the current directory to get reliable results.
           */
          vchdir(current_dir_name);
          path_get_directory_and_filename(ret_path, MAX_PATH, ret_file, MAX_PATH, ret);
          vchdir(return_dir_name);

          if(ret_path[0] != '\0')
          {
            if(path_navigate(current_dir_name, MAX_PATH, ret_path) < 0)
            {
              error("Directory does not exist or permission denied.",
               ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
              ret[0] = '\0';
              break;
            }
          }

          if(ret_file[0] == '\0' ||
           path_join(ret, MAX_PATH, current_dir_name, ret_file) < 0)
          {
            /* Directory entered, nothing else to do; or path is too long. */
            ret[0] = '\0';
            break;
          }
        }

        if(default_ext)
          path_force_ext(ret, MAX_PATH, default_ext);

        stat_result = vstat(ret, &file_info);

        if((stat_result >= 0) && S_ISDIR(file_info.st_mode))
        {
          /* Should be unreachable unless someone swapped out a file
           * for a directory while the file manager wasn't looking,
           * but this is harmless to check anyway.
           */
          if(path_navigate(current_dir_name, MAX_PATH, ret) < 0)
            error("Directory does not exist or permission denied.",
             ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
          ret[0] = '\0';
          break;
        }

        // Directory restrictions or DOS/Windows? Filename needs safety checks:
#if !defined(_WIN32) && !defined(CONFIG_DJGPP)
        if(allow_dirs != ALLOW_ALL_DIRS)
#endif
        {
          enum path_safe_mask check_result;
          size_t base_dir_offset = 0;

          if(allow_dirs != ALLOW_ALL_DIRS)
          {
            base_dir_offset = strlen(base_dir_name);

            /* This is a duplicate of a later check out of necessity... */
            if(strncmp(ret, base_dir_name, base_dir_offset))
            {
              error("File is outside of allowed base directory.",
               ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
              ret[0] = '\0';
              break;
            }
          }
          else
            base_dir_offset = strlen(current_dir_name);

          if((check_result = path_safety_check(ret + base_dir_offset,
           PATH_SAFE_UNIX_PARENT | PATH_SAFE_DOS_CHARACTER | PATH_SAFE_DOS_DEVICE)))
          {
            snprintf(file_name, MAX_PATH, "File name failed safety check: %s",
             path_safety_strerror(check_result));
            error(file_name, ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
            ret[0] = '\0';
            break;
          }
        }

        // We're creating a new file
        if(allow_new != NO_NEW_FILES)
        {
          // File Exists
          if((stat_result >= 0) && (allow_new == ALLOW_NEW_FILES))
          {
            char confirm_string[512];
            snprintf(confirm_string, 512, "%s already exists, overwrite?",
             ret_file);
            if(!ask_yes_no(mzx_world, confirm_string))
              return_value = 0;
          }
          else
          {
            if(ret[0])
              return_value = 0;
          }
        }
        else

        // It's a file, open it
        if(stat_result >= 0)
          return_value = 0;
        else
          ret[0] = 0;

        break;
      }

      // Selected a directory from the list
      case 2:
      {
        if(dir_list && dir_list[chosen_dir])
          if(path_navigate(current_dir_name, MAX_PATH, dir_list[chosen_dir]) < 0)
            error("Directory does not exist or permission denied.",
             ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);

        break;
      }

      // Create a new directory
      case 3:
      {
        char *new_name;
        char full_name[MAX_PATH];

        new_name = cmalloc(MAX_PATH);
        new_name[0] = 0;

        if(!confirm_input(mzx_world, "Create New Directory",
         "New directory name:", new_name))
        {
          path_join(full_name, MAX_PATH, current_dir_name, new_name);

          if(vstat(full_name, &file_info) < 0)
          {
            vmkdir(full_name, 0777);
          }
          else
          {
            char error_str[512];
            sprintf(error_str, "%s already exists.", new_name);
            error(error_str, ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
          }
        }

        free(new_name);
        break;
      }

      // Delete file
      case 4:
      {
        char *del_name = (char *)cmalloc(MAX_PATH);

        struct file_list_entry *e =
         (struct file_list_entry *)file_list[chosen_file];

        if(strcmp(e->filename, PATH_PARENT_DIR) &&
         strcmp(e->filename, PATH_CURRENT_DIR) &&
         path_join(del_name, MAX_PATH, current_dir_name, e->filename) >= 0 &&
         vstat(del_name, &file_info) >= 0)
        {
          char confirm_string[64];

          if(strlen(e->filename) <= 33)
          {
            snprintf(confirm_string, sizeof(confirm_string),
             "Delete %.33s - are you sure?", e->filename);
          }
          else
          {
            snprintf(confirm_string, sizeof(confirm_string),
             "Delete %.30s... - are you sure?", e->filename);
          }

          if(!confirm(mzx_world, confirm_string))
            if(vunlink(del_name))
              error("File could not be deleted.",
               ERROR_T_WARNING, ERROR_OPT_OK, 0x0000);
        }

        free(del_name);
        break;
      }

      // Rename file
      case 5:
      {
        char *old_path = cmalloc(MAX_PATH);
        char *new_path = cmalloc(MAX_PATH);
        char *new_name = cmalloc(MAX_PATH);

        struct file_list_entry *e =
         (struct file_list_entry *)file_list[chosen_file];

        snprintf(new_name, MAX_PATH, "%s", e->filename);

        if(!confirm_input(mzx_world, "Rename File", "New file name:", new_name))
        {
          path_join(old_path, MAX_PATH, current_dir_name, e->filename);
          path_join(new_path, MAX_PATH, current_dir_name, new_name);

          if(strcmp(old_path, new_path))
            if(vrename(old_path, new_path))
              error("File rename failed.",
               ERROR_T_WARNING, ERROR_OPT_OK, 0x0000);

        }

        free(old_path);
        free(new_path);
        free(new_name);
        break;
      }

      // Delete directory
      case 6:
      {
        if(strcmp(dir_list[chosen_dir], PATH_PARENT_DIR) &&
         strcmp(dir_list[chosen_dir], PATH_CURRENT_DIR) &&
         chosen_dir < volumes_pos)
        {
          char confirm_string[70];
          snprintf(confirm_string, 70, "Delete %s: are you sure?",
           dir_list[chosen_dir]);
          confirm_string[69] = 0;

          if(!confirm(mzx_world, confirm_string))
          {
            char file_name_ch[MAX_PATH];
            path_join(file_name_ch, MAX_PATH, current_dir_name,
             dir_list[chosen_dir]);

            if(!ask_yes_no(mzx_world,
             (char *)"Delete subdirectories recursively?"))
            {
              if(!remove_files(file_name_ch, true) || vrmdir(file_name_ch))
                error("Directory could not be deleted.",
                 ERROR_T_WARNING, ERROR_OPT_OK, 0x0000);
            }
            else
            {
              if(!remove_files(file_name_ch, false) || vrmdir(file_name_ch))
                error("Directory contains files or could not be deleted.",
                 ERROR_T_WARNING, ERROR_OPT_OK, 0x0000);
            }
          }
        }
        break;
      }

      // Rename directory
      case 7:
      {
        if(strcmp(dir_list[chosen_dir], PATH_PARENT_DIR) &&
         strcmp(dir_list[chosen_dir], PATH_CURRENT_DIR) &&
         chosen_dir < volumes_pos)
        {
          char *old_path = cmalloc(MAX_PATH);
          char *new_path = cmalloc(MAX_PATH);
          char *new_name = cmalloc(MAX_PATH);

          snprintf(new_name, MAX_PATH, "%s", dir_list[chosen_dir]);
          new_name[MAX_PATH - 1] = '\0';

          if(!confirm_input(mzx_world, "Rename Directory",
           "New directory name:", new_name))
          {
            path_join(old_path, MAX_PATH, current_dir_name, dir_list[chosen_dir]);
            path_join(new_path, MAX_PATH, current_dir_name, new_name);

            if(strcmp(old_path, new_path))
              if(vrename(old_path, new_path))
                error("Directory rename failed.",
                 ERROR_T_WARNING, ERROR_OPT_OK, 0x0000);

          }

          free(old_path);
          free(new_path);
          free(new_name);
        }
        break;
      }
    }

    // Filter out paths that violate the allowed directories mode.
    if(allow_dirs != ALLOW_ALL_DIRS)
    {
      size_t base_dir_len = strlen(base_dir_name);

      // If the base path isn't part of the return path
      if(strncmp(base_dir_name, current_dir_name,  base_dir_len) ||
       strstr(current_dir_name, "..") ||

      // or if there's an unallowed subdirectory
       (allow_dirs == NO_DIRS && path_has_directory(current_dir_name + base_dir_len)))
      {
        memcpy(current_dir_name, base_dir_name, base_dir_len + 1);
        return_value = 1;
        ret[0] = 0;
      }
      else

      // If the base dir and return dir are the same and the selected path is
      // prefixed with the base dir, make it a relative path. This is necessary
      // for files selected for use in a game from the editor.
      // TODO should maybe do this regardless of the return path. The only
      // thing that would be affected is probably GLSL shader selection.
      if(return_dir_is_base_dir)
      {
        path_remove_prefix(ret, MAX_PATH, base_dir_name, base_dir_len);
      }
    }

    // Hack - don't allow the added elements to be destroyed
    di.num_elements = FILESEL_BASE_ELEMENTS;
    last_element = di.current_element;

    for(i = 0; i < total_filenames_allocated; i++)
    {
      if(file_list[i])
        free(file_list[i]);
    }
    free(file_list);

    for(i = 0; i < total_dirnames_allocated; i++)
    {
      if(dir_list[i])
        free(dir_list[i]);
    }
    free(dir_list);

    destruct_dialog(&di);
  }

  // Now we can destroy the additional elements
  for(i = 0; i < num_ext; i++)
  {
    free(dialog_ext[i]);
  }

  if(return_value == -1)
  {
    ret[0] = 0;
  }

  free(base_dir_name);
  free(return_dir_name);
  free(current_dir_name);
  free(file_name);

  return return_value;
}

int choose_file_ch(struct world *mzx_world, const char *const *wildcards,
 char *ret, const char *title, enum allow_dirs allow_dirs)
{
  return file_manager(mzx_world, wildcards, NULL, ret, title, allow_dirs,
   NO_NEW_FILES, NULL, 0, 0);
}

int new_file(struct world *mzx_world, const char *const *wildcards,
 const char *default_ext, char *ret, const char *title, enum allow_dirs allow_dirs)
{
  return file_manager(mzx_world, wildcards, default_ext, ret, title, allow_dirs,
   ALLOW_NEW_FILES, NULL, 0, 0);
}

// Calculates the percent from progress and out_of as in (progress/out_of).
void meter(const char *title, unsigned int progress, unsigned int out_of)
{
  int titlex = 40 - ((int)strlen(title) >> 1);

  assert(titlex > 0);

  draw_window_box(5, 10, 74, 12, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  // Add title
  write_string(title, titlex, 10, DI_TITLE, 0);
  draw_char(' ', DI_TITLE, titlex - 1, 10);
  draw_char(' ', DI_TITLE, titlex + (unsigned int)strlen(title), 10);
  meter_interior(progress, out_of);
}

// Draws the meter but only the interior where the percent is.
void meter_interior(unsigned int progress, unsigned int out_of)
{
  // The actual meter has 66 spaces, or 132 half spaces, to use, so barsize is
  // the number of half spaces to display.
  unsigned int barsize = progress * 132ULL / out_of;
  unsigned int percent = progress * 100ULL / out_of;
  int revcolor = ((DI_METER & 15) << 4) + (DI_METER >> 4);
  char percentstr[5];

  assert(progress <= out_of);

  fill_line(66, 7, 11, 32, revcolor);
  // Draw half-space if appropriate
  if(barsize & 1)
    draw_char('\xDD', DI_METER, 7 + (barsize >> 1), 11);

  // Determine percentage
  snprintf(percentstr, 5, "%3u%%", percent);
  write_string(percentstr, 37, 11, DI_METER, 1);

  // Fill in meter
  if(barsize > 1)
    color_line(barsize >> 1, 7, 11, revcolor);
  if(barsize < 131)
    color_line((133 - barsize) >> 1, 7 + (barsize >> 1), 11, DI_METER);

  // Done! :)
}
