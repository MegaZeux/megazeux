/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "configure.h"
#include "core.h"
#include "intake.h"
#include "graphics.h"
#include "window.h"
#include "world_struct.h"

#include "event/event.h"

// Global status of insert
static boolean insert_on = true;

static char last_char = 0;

/**
 * Get the current intake insert state.
 */
boolean intake_get_insert(void)
{
  return insert_on;
}

/**
 * Set the current intake insert state.
 */
void intake_set_insert(boolean new_insert_state)
{
  insert_on = new_insert_state ? true : false;
}

static inline void intake_old_place_char(char *string, int *currx,
 int *curr_len, uint32_t chr)
{
  // Overwrite or insert?
  if(insert_on || (*currx == *curr_len))
  {
    // Insert- Move all ahead 1, increasing string length
    (*curr_len)++;
    memmove(string + *currx + 1, string + *currx, *curr_len - *currx);
  }
  // Add character and move forward one
  string[(*currx)++] = chr;
}

// (returns the key used to exit) String points to your memory for storing
// the new string. The current "value" is used- clear the string before
// calling intake if you need a blank string. Max_len is the maximum length
// of the string. X, y, and color are self-explanitory.

// The editing keys supported are as
// follows- Keys to enter characters, Enter/ESC to exit, Home/End to move to
// the front/back of the line, Insert to toggle insert/overwrite, Left/Right
// to move within the line, Bkspace/Delete to delete as usual, and Alt-Bksp
// to clear the entire line. No screen saving is performed. After this
// function, the cursor is automatically off.

// Mouse support- Clicking inside string sends cursor there. Clicking
// outside string returns a MOUSE_EVENT to caller without acknowledging
// the event.

// Returns a backspace if attempted at start of line. (exit_type==INTK_EXIT_ANY)
// Returns a delete if attempted at end of line. (exit_type==INTK_EXIT_ANY)

int intake(struct world *mzx_world, char *string, int max_len, int display_len,
 int x, int y, char color, enum intake_exit_type exit_type, boolean mask_colors,
 int *return_x_pos)
{
  int use_mask = get_config()->mask_midchars && mask_colors;
  int offset = mask_colors ? PRO_CH : 0;
  int c_offset = mask_colors ? 16 : 0;
  int currx, curr_len;
  int scrolledx, tmpx;
  int done = 0, place = 0;
  char tmp_char;
  boolean select_char = false;
  int mouse_press;
  int action;
  int key;

  // Put cursor at the end of the string...
  currx = curr_len = (int)strlen(string);

  // ...unless return_x_pos says not to.
  if((return_x_pos) && (*return_x_pos < currx))
    currx = *return_x_pos;

  do
  {
    int flags = use_mask ? WR_MASK : 0;

    if(max_len > display_len && currx > display_len)
    {
      scrolledx = currx - display_len;
      tmpx = currx;
    }
    else
    {
      scrolledx = 0;
      tmpx = display_len;
    }

    tmp_char = string[tmpx];
    string[tmpx] = '\0';
    write_string_ext(string + scrolledx, x, y, color, flags, 0, c_offset);
    string[tmpx] = tmp_char;

    if(curr_len < display_len)
    {
      fill_line_ext(display_len + 1 - curr_len, x + curr_len, y, 32, color,
       offset, c_offset);
    }

    if(insert_on)
      cursor_underline(x + currx - scrolledx, y);
    else
      cursor_solid(x + currx - scrolledx, y);

    update_screen();
    update_event_status_delay();
    key = get_key(keycode_internal_wrt_numlock);
    place = 0;

    if(!key && has_unicode_input())
      key = IKEY_UNICODE;

    action = get_joystick_ui_action();
    if(action)
    {
      switch(action)
      {
        case JOY_X:
          // Select character
          select_char = true;
          key = IKEY_c;
          break;

        case JOY_Y:
          key = IKEY_BACKSPACE;
          break;

        default:
          key = get_joystick_ui_key();;
          break;
      }
    }

    // Exit event mimics escape
    if(get_exit_status() && exit_type != INTK_EXIT_ENTER)
    {
      key = 0;
      done = 1;
    }

    mouse_press = get_mouse_press_ext();

    if(mouse_press)
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);
      if((mouse_y == y) && (mouse_x >= x) &&
       (mouse_x <= (x + display_len)) && (mouse_press <= MOUSE_BUTTON_RIGHT))
      {
        // Yep, reposition cursor.
        currx = mouse_x - x;
        if(currx > curr_len)
          currx = curr_len;
      }
      else
      {
        key = -1;
        done = 1;
      }
    }

    // Handle key cases
    switch(key)
    {
      case IKEY_ESCAPE:
      {
        // ESC
        if(exit_type != INTK_EXIT_ENTER)
        {
          done = 1;
        }
        break;
      }

      case IKEY_RETURN:
      {
        // Enter
        done = 1;
        break;
      }

      case IKEY_HOME:
      {
        // Home
        currx = 0;
        break;
      }

      case IKEY_END:
      {
        // End
        currx = curr_len;
        break;
      }

      case IKEY_LEFT:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the left
          if(currx)
          {
            char *next_position = string + currx - 1;

            while(currx && !isalnum((int)*next_position))
            {
              next_position--;
              currx--;
            }

            while(currx && isalnum((int)*next_position))
            {
              next_position--;
              currx--;
            }
          }
        }
        else
        {
          // Left
          if(currx > 0)
            currx--;
        }

        break;
      }

      case IKEY_RIGHT:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the right
          if(currx < curr_len)
          {
            char *current_position = string + currx;
            char current_char = *current_position;
            if(!isalnum((int)current_char))
            {
              do
              {
                current_position++;
                currx++;
                current_char = *current_position;
              } while(current_char && !isalnum((int)current_char));
            }

            while(current_char && isalnum((int)current_char))
            {
              current_position++;
              currx++;
              current_char = *current_position;
            }
          }
        }
        else
        {
          // Right
          if(currx < curr_len)
            currx++;
        }

        break;
      }

      case IKEY_F1:
      case IKEY_F2:
      case IKEY_F3:
      case IKEY_F4:
      case IKEY_F5:
      case IKEY_F6:
      case IKEY_F7:
      case IKEY_F8:
      case IKEY_F9:
      case IKEY_F10:
      case IKEY_F11:
      case IKEY_F12:
      case IKEY_UP:
      case IKEY_DOWN:
      case IKEY_TAB:
      case IKEY_PAGEUP:
      case IKEY_PAGEDOWN:
      {
        done = 1;
        break;
      }

      case IKEY_INSERT:
      {
        // Insert
        insert_on ^= 1;
        break;
      }

      case IKEY_BACKSPACE:
      {
        // Backspace, at 0 it might exit
        if(get_alt_status(keycode_internal))
        {
          // Alt-backspace, erase input
          curr_len = currx = 0;
          string[0] = 0;
        }
        else

        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the left
          if(currx)
          {
            int old_position = currx;

            while(currx && !isalnum((int)string[currx]))
              currx--;

            while(currx && isalnum((int)string[currx]))
              currx--;

            curr_len -= old_position - currx;

            memmove(string + currx, string + old_position,
             strlen(string + old_position) + 1);
          }
        }
        else

        if(currx == 0)
        {
          if(exit_type == INTK_EXIT_ANY)
          {
            done = 1;
          }
        }
        else
        {
          // Move all back 1, decreasing string length
          memmove(string + currx - 1, string + currx, curr_len - currx + 1);
          curr_len--;
          // Cursor back one
          currx--;
        }
        break;
      }

      case IKEY_DELETE:
      {
        // Delete, at the end might exit
        if(currx == curr_len)
        {
          if(exit_type == INTK_EXIT_ANY)
            done = 1;
        }
        else
        {
          if(curr_len)
          {
            // Move all back 1, decreasing string length
            memmove(string + currx, string + currx + 1, curr_len - currx);
            curr_len--;
          }
        }
        break;
      }

      case IKEY_c:
      {
        if(select_char || (get_alt_status(keycode_internal) &&
         !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal)))
        {
          // If alt - C is pressed, choose character
          int new_char = char_selection(last_char);
          select_char = false;

          if(new_char >= 32)
          {
            intake_old_place_char(string, &currx, &curr_len, new_char);
            last_char = new_char;
          }
        }
        else
        {
          place = 1;
        }

        break;
      }

      case IKEY_t:
      {
        // Hack for SFX editor dialog
        if(get_alt_status(keycode_internal) &&
         !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_v:
      {
        // Hack for scroll editor
        if(get_alt_status(keycode_internal) &&
         !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_LSHIFT:
      case IKEY_RSHIFT:
      case 0:
      {
        place = 0;
        break;
      }

      default:
      {
        // Place the char
        place = 1;
        break;
      }

      case -1:
      {
        break;
      }
    }

    if(place)
    {
      int num_placed = 0;

      while((curr_len < max_len) && (!done) && num_placed < KEY_UNICODE_MAX)
      {
        uint32_t cur_char = get_key(keycode_text_ascii);
        if(cur_char)
        {
          intake_old_place_char(string, &currx, &curr_len, cur_char);
          num_placed++;
        }
        else
          break;
      }
    }

    // Loop
  } while(!done);

  cursor_off();
  if(return_x_pos)
    *return_x_pos = currx;

  return key;
}

struct intake_subcontext
{
  subcontext ctx;
  boolean (*event_cb)(void *priv, subcontext *sub, enum intake_event_type type,
   int old_pos, int new_pos, int value, const char *data);
  void *event_priv;
  char *dest;
  int current_length;
  int max_length;
  int pos;
  int *pos_external;
  int *length_external;
  boolean select_char;
};

/**
 * Set the current cursor position within the editing string.
 * This should be used mainly by intake_sync and intake_apply_event_fixed.
 */
static void intake_set_pos(struct intake_subcontext *intk, int new_pos)
{
  if(new_pos < 0)
    new_pos = 0;
  if(new_pos > intk->current_length)
    new_pos = intk->current_length;

  intk->pos = new_pos;
  if(intk->pos_external)
    *(intk->pos_external) = new_pos;
}

/**
 * Set the editing string length.
 * This should be used mainly by intake_sync and intake_apply_event_fixed.
 */
static void intake_set_length(struct intake_subcontext *intk, int new_length)
{
  if(new_length < 0)
    new_length = 0;
  if(new_length > intk->max_length)
    new_length = intk->max_length;

  if(intk->dest)
    intk->dest[new_length] = '\0';

  intk->current_length = new_length;
  if(intk->length_external)
    *(intk->length_external) = new_length;
}

/**
 * Sync the current pos/length values with their external duplicates.
 */
void intake_sync(subcontext *sub)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  if(!intk)
    return;

  // If using a fixed buffer, calculate from it instead of the external length.
  if(intk->dest)
  {
    intake_set_length(intk, strlen(intk->dest));
  }
  else

  if(intk->length_external)
    intake_set_length(intk, *(intk->length_external));

  if(intk->pos_external)
    intake_set_pos(intk, *(intk->pos_external));
}

/**
 * Find the start of the word preceding the cursor and place the cursor there.
 */
static void intake_skip_back(struct intake_subcontext *intk)
{
  char *next_position;
  int pos = intk->pos;

  if(pos)
  {
    next_position = intk->dest + pos - 1;

    while(pos && !isalnum((int)*next_position))
    {
      next_position--;
      pos--;
    }

    while(pos && isalnum((int)*next_position))
    {
      next_position--;
      pos--;
    }

    intake_set_pos(intk, pos);
  }
}

/**
 * Find the end of the word following the cursor and place the cursor there.
 */
static void intake_skip_forward(struct intake_subcontext *intk)
{
  char *current_position;
  int pos = intk->pos;

  if(pos < intk->current_length)
  {
    current_position = intk->dest + pos;

    if(!isalnum((int)*current_position))
    {
      do
      {
        current_position++;
        pos++;
      } while(*current_position && !isalnum((int)*current_position));
    }

    while(*current_position && isalnum((int)*current_position))
    {
      current_position++;
      pos++;
    }

    intake_set_pos(intk, pos);
  }
}

/**
 * Apply intake events to the fixed-size buffer supplied with the original
 * intake2() call. If no event callback is provided, this function will always
 * be used as the event callback. If an event callback is provided, the event
 * callback must call this function manually if it is required.
 */
boolean intake_apply_event_fixed(subcontext *sub, enum intake_event_type type,
 int new_pos, int value, const char *data)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;

  if(!intk || !intk->dest || intk->pos < 0 || intk->pos > intk->current_length)
    return false;

  switch(type)
  {
    case INTK_NO_EVENT:
      return false;

    case INTK_MOVE:
      break;

    case INTK_MOVE_WORDS:
    {
      while(value < 0)
        intake_skip_back(intk), value++;
      while(value > 0)
        intake_skip_forward(intk), value--;
      new_pos = intk->pos;
      break;
    }

    case INTK_INSERT:
    {
      if(intk->current_length >= intk->max_length)
        return false;
      intake_set_length(intk, intk->current_length + 1);

      if(intk->pos < intk->current_length)
        memmove(intk->dest + intk->pos + 1, intk->dest + intk->pos,
         intk->current_length - intk->pos);

      if(intk->pos <= intk->current_length)
        intk->dest[intk->pos] = value;
      break;
    }

    case INTK_OVERWRITE:
    {
      if(intk->pos == intk->current_length)
      {
        if(intk->current_length >= intk->max_length)
          return false;

        intake_set_length(intk, intk->current_length + 1);
      }

      if(intk->pos <= intk->current_length)
        intk->dest[intk->pos] = value;
      break;
    }

    case INTK_DELETE:
    {
      if(intk->pos < intk->current_length)
      {
        memmove(intk->dest + intk->pos, intk->dest + intk->pos + 1,
         intk->current_length - intk->pos);
        intake_set_length(intk, intk->current_length - 1);
      }
      break;
    }

    case INTK_BACKSPACE:
    {
      if(intk->pos > 0)
      {
        memmove(intk->dest + intk->pos - 1, intk->dest + intk->pos,
         intk->current_length - intk->pos + 1);
        intake_set_length(intk, intk->current_length - 1);
      }
      break;
    }

    case INTK_BACKSPACE_WORDS:
    {
      if(intk->pos > 0)
      {
        int old_pos = intk->pos;
        while(value > 0 && intk->pos > 0)
        {
          intake_skip_back(intk);
          value--;
        }
        new_pos = intk->pos;
        if(intk->pos < old_pos)
        {
          memmove(intk->dest + intk->pos, intk->dest + old_pos,
           intk->current_length - old_pos + 1);
          intake_set_length(intk, intk->current_length - (old_pos - intk->pos));
        }
      }
      break;
    }

    case INTK_CLEAR:
    {
      intk->dest[0] = 0;
      intake_set_length(intk, 0);
      break;
    }

    case INTK_INSERT_BLOCK:
    {
      if(!data)
        return false;

      if(intk->current_length + value > intk->max_length)
      {
        value = intk->max_length - intk->current_length;
        new_pos = intk->pos + value;
        if(!value)
          return false;
      }

      if(intk->pos < intk->current_length)
        memmove(intk->dest + intk->pos + value, intk->dest + intk->pos,
         intk->current_length - intk->pos + 1);

      memcpy(intk->dest + intk->pos, data, value);
      intake_set_length(intk, intk->current_length + value);
      break;
    }

    case INTK_OVERWRITE_BLOCK:
    {
      if(!data)
        return false;

      if(intk->pos + value > intk->max_length)
      {
        value = intk->max_length - intk->pos;
        new_pos = intk->pos + value;
        if(!value)
          return false;
      }

      memcpy(intk->dest + intk->pos, data, value);

      if(intk->pos + value > intk->current_length)
        intake_set_length(intk, intk->pos + value);
      break;
    }
  }
  intake_set_pos(intk, new_pos);
  return true;
}

/**
 * Send an intake event to the parent context, if applicable.
 */
static void intake_event_ext(struct intake_subcontext *intk,
 enum intake_event_type type, int old_pos, int new_pos, int value, const char *data)
{
  if(intk->event_cb)
  {
    if(intk->event_cb(intk->event_priv, (subcontext *)intk, type, old_pos,
     new_pos, value, data))
    {
      intake_sync((subcontext *)intk);
    }
  }
  else
    intake_apply_event_fixed((subcontext *)intk, type, new_pos, value, data);
}

static void intake_event(struct intake_subcontext *intk,
 enum intake_event_type type, int old_pos, int new_pos)
{
  intake_event_ext(intk, type, old_pos, new_pos, 0, NULL);
}

/**
 * Place a new char inside of the string.
 */
static boolean intake_place_char(struct intake_subcontext *intk, char chr)
{
  if(chr && (intk->current_length != intk->max_length))
  {
    enum intake_event_type type = insert_on ? INTK_INSERT : INTK_OVERWRITE;
    intake_event_ext(intk, type, intk->pos, intk->pos + 1, chr, NULL);
    return true;
  }
  return false;
}

/**
 * Make sure the intake values are synchronized before doing anything.
 */
static boolean intake_idle(subcontext *sub)
{
  intake_sync(sub);
  return false;
}

/**
 * Joystick input. Can't do much here; pass most input through to the parent.
 */
static boolean intake_joystick(subcontext *sub, int *key, int action)
{
  switch(action)
  {
    case JOY_LEFT:
      *key = IKEY_LEFT;
      return true;

    case JOY_RIGHT:
      *key = IKEY_RIGHT;
      return true;

    case JOY_X:
      // Select character
      ((struct intake_subcontext *)sub)->select_char = true;
      *key = IKEY_c;
      return true;

    case JOY_Y:
      *key = IKEY_BACKSPACE;
      return true;
  }
  return false;
}

/**
 * Input/delete text, move the cursor, or pass input on to the parent context.
 */
static boolean intake_key(subcontext *sub, int *key)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  boolean alt_status = get_alt_status(keycode_internal);
  boolean ctrl_status = get_ctrl_status(keycode_internal);
  boolean shift_status = get_shift_status(keycode_internal);
  boolean any_mod = (alt_status || ctrl_status || shift_status);
  boolean place = false;
  int num_placed = 0;

  // Exit-- let the parent context handle.
  if(get_exit_status())
    return false;

  switch(*key)
  {
    case IKEY_HOME:
    {
      if(!any_mod)
      {
        // Home
        intake_event(intk, INTK_MOVE, intk->pos, 0);
        return true;
      }
      break;
    }

    case IKEY_END:
    {
      if(!any_mod)
      {
        // End
        intake_event(intk, INTK_MOVE, intk->pos, intk->current_length);
        return true;
      }
      break;
    }

    case IKEY_LEFT:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Move one word backward.
        intake_event_ext(intk, INTK_MOVE_WORDS, intk->pos, intk->pos, -1, NULL);
        return true;
      }
      else

      if(!any_mod)
      {
        // Left
        if(intk->pos > 0)
          intake_event(intk, INTK_MOVE, intk->pos, intk->pos - 1);

        return true;
      }
      break;
    }

    case IKEY_RIGHT:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Move one word forward.
        intake_event_ext(intk, INTK_MOVE_WORDS, intk->pos, intk->pos, 1, NULL);
        return true;
      }
      else

      if(!any_mod)
      {
        // Right
        if(intk->pos < intk->current_length)
          intake_event(intk, INTK_MOVE, intk->pos, intk->pos + 1);

        return true;
      }
      break;
    }

    case IKEY_INSERT:
    {
      if(!any_mod)
      {
        // Insert
        insert_on = !insert_on;
        return true;
      }
      break;
    }

    case IKEY_BACKSPACE:
    {
      if(get_alt_status(keycode_internal))
      {
        // Alt-backspace, erase input
        intake_event(intk, INTK_CLEAR, intk->pos, 0);
        return true;
      }
      else

      if(get_ctrl_status(keycode_internal))
      {
        // Delete word
        if(intk->pos)
          intake_event_ext(intk, INTK_BACKSPACE_WORDS, intk->pos, intk->pos, 1, NULL);
        return true;
      }
      else

      if(intk->pos > 0)
      {
        // Backspace previous char.
        intake_event(intk, INTK_BACKSPACE, intk->pos, intk->pos - 1);
        return true;
      }
      break;
    }

    case IKEY_DELETE:
    {
      // Delete current char.
      // If at the end of the string, let the parent handle this instead.
      if(intk->current_length && intk->pos < intk->current_length)
      {
        intake_event(intk, INTK_DELETE, intk->pos, intk->pos);
        return true;
      }
      break;
    }

    case IKEY_c:
    {
      if(intk->select_char || (get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal)))
      {
        // If alt - C is pressed, choose character
        int new_char = char_selection(last_char);
        intk->select_char = false;

        if(new_char >= 32)
        {
          last_char = new_char;
          intake_place_char(intk, new_char);
        }
        return true;
      }
      else
      {
        place = true;
      }
      break;
    }

    default:
    {
      place = true;
      break;
    }
  }

  // Attempt to add char(s) to the string.
  while(place && num_placed < KEY_UNICODE_MAX)
  {
    char cur_char = get_key(keycode_text_ascii);
    if(cur_char)
    {
      intake_place_char(intk, cur_char);
      num_placed++;
    }
    else
      break;
  }
  return !!(num_placed);
}

/**
 * Create a text entry subcontext on top of the current context. Will pass all
 * inputs it can't/won't handle through to its parent. An optional external
 * cursor position pointer can be provided to allow intake to return the cursor
 * position and receive outside updates to the cursor position.
 *
 * Unlike the original intake(), this intake implementation takes no screen
 * positioning information, doesn't draw the string being edited, and doesn't
 * handle mouse clicks. Doing these is the responsibility of the parent context.
 */
subcontext *intake2(context *parent, char *dest, int max_length,
 int *pos_external, int *length_external)
{
  struct intake_subcontext *intk =
   (struct intake_subcontext *)ccalloc(1, sizeof(struct intake_subcontext));
  struct context_spec spec;

  intk->dest = dest;
  intk->max_length = max_length;
  intk->pos_external = pos_external;
  intk->length_external = length_external;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.idle     = intake_idle;
  spec.key      = intake_key;
  spec.joystick = intake_joystick;

  intake_sync((subcontext *)intk);
  if(!pos_external)
    intake_set_pos(intk, intk->current_length);

  create_subcontext((subcontext *)intk, parent, &spec);
  return (subcontext *)intk;
}

/**
 * Insert a string of text into the intake buffer. A linebreak char can be
 * provided which will cause this loop to terminate and return the next char
 * position. Otherwise, return NULL when the input string has been exhausted.
 */
const char *intake_input_string(subcontext *sub, const char *src,
 int linebreak_char)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  const char *pos = src;
  int cur_char;
  int length = 0;
  enum intake_event_type type = insert_on ? INTK_INSERT_BLOCK : INTK_OVERWRITE_BLOCK;

  intake_sync(sub);

  while(*pos)
  {
    cur_char = *pos;

    // Linebreak char? Skip and request a line break.
    if(cur_char == linebreak_char)
    {
      if(length > 0)
        intake_event_ext(intk, type, intk->pos, intk->pos + length, length, src);

      return (pos + 1);
    }

    // Otherwise, attempt to place until no more space is left.
    length++;
    if(intk->current_length + length >= intk->max_length)
      break;

    pos++;
  }

  if(length > 0)
    intake_event_ext(intk, type, intk->pos, intk->pos + length, length, src);

  return NULL;
}

/**
 * Set the intake event callback function. This feature is used to report
 * individual intake events immediately to the parent context as they occur.
 * The callback return value should indicate success (`true`) or failure
 * (`false`). If the callback returns `true`, `intake_sync` will be called.
 */
void intake_event_callback(subcontext *sub, void *priv,
 boolean (*event_cb)(void *priv, subcontext *sub, enum intake_event_type type,
 int old_pos, int new_pos, int value, const char *data))
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  if(intk)
  {
    intk->event_cb = event_cb;
    intk->event_priv = priv;
  }
}
