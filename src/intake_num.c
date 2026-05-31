/* MegaZeux
 *
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

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "core.h"
#include "graphics.h"
#include "intake_num.h"
#include "util.h"
#include "window.h"

#include "event/event.h"

struct intake_num_context
{
  context ctx;
  int x;
  int y;
  int w;
  int color;
  int value;
  int min_val;
  int max_val;
  boolean leading_minus;
  boolean empty;
  boolean save;
  context *callback_ctx;
  void (*callback_fn)(context *, int);
};

/**
 * Change the input number and bound it.
 */

static void change_value(struct intake_num_context *intk, int value)
{
  if(intk->leading_minus && value)
    value *= -1;

  intk->value = CLAMP(value, intk->min_val, intk->max_val);
  intk->empty = false;
  intk->leading_minus = false;
}

/**
 * Expand draw area to encompass maximum/minimum value if necessary.
 * The minus is drawn on the left border char, so abs() is used to ignore it.
 */

static void fix_draw_area(struct intake_num_context *intk)
{
  char buffer[12];
  int buf_len;

  snprintf(buffer, 12, "%d", abs(intk->max_val));
  buf_len = strlen(buffer);
  if(buf_len > intk->w)
    intk->w = buf_len;

  snprintf(buffer, 12, "%d", abs(intk->min_val));
  buf_len = strlen(buffer);
  if(buf_len > intk->w)
    intk->w = buf_len;
}

/**
 * Draw the number input to the screen.
 */

static boolean intake_num_draw(context *ctx)
{
  struct intake_num_context *intk = (struct intake_num_context *)ctx;
  int write_pos = intk->x + intk->w + 1;
  char buffer[12] = { 0 };
  int i;

  snprintf(buffer, 12, "%d", intk->value);
  write_pos -= strlen(buffer);

  for(i = 0; i < intk->w + 2; i++)
    draw_char(0, intk->color, intk->x + i, intk->y);

  if(intk->leading_minus)
    draw_char('-', intk->color, write_pos, intk->y);

  if(!intk->empty)
    write_string(buffer, write_pos, intk->y, intk->color, false);

  return true;
}

/**
 * Keyhandler for number input.
 */

static boolean intake_num_key(context *ctx, int *key)
{
  struct intake_num_context *intk = (struct intake_num_context *)ctx;
  int increment_value = 0;

  if(get_exit_status())
  {
    *key = IKEY_ESCAPE;
  }
  else

  if(has_unicode_input())
  {
    boolean modified = false;
    int num_read = 0;

    while(num_read < KEY_UNICODE_MAX)
    {
      uint32_t unicode = get_key(keycode_text_ascii);
      num_read++;
      if(unicode < '0' || unicode > '9')
        continue;

      modified = true;

      // At exactly maximum/minimum, typing a number should wrap
      if((intk->value == intk->min_val && intk->min_val < 0)
       || (intk->value == intk->max_val && intk->max_val > 0))
      {
        change_value(intk, 0);
        break;
      }

      else
      {
        int add_value = (unicode - '0');

        if(intk->value < 0)
          add_value *= -1;

        change_value(intk, intk->value * 10 + add_value);
      }
    }
    if(modified)
      return true;
  }

  switch(*key)
  {
    case IKEY_ESCAPE:
    {
      // Exit
      intk->save = false;
      destroy_context(ctx);
      return true;
    }

    case IKEY_RETURN:
    case IKEY_TAB:
    {
      // Set component and exit
      destroy_context(ctx);
      return true;
    }

    case IKEY_DELETE:
    case IKEY_PERIOD:
    {
      // Set to zero and exit
      change_value(intk, 0);
      destroy_context(ctx);
      return true;
    }

    case IKEY_BACKSPACE:
    {
      int new_value = intk->value / 10;

      if(intk->leading_minus)
      {
        intk->leading_minus = false;
      }
      else

      if(new_value != 0)
      {
        change_value(intk, new_value);
      }

      else
      {
        if(intk->value < 0)
          intk->leading_minus = true;

        intk->value = 0;
        intk->empty = true;
      }

      return true;
    }

    case IKEY_MINUS:
    {
      if(intk->min_val < 0)
      {
        // Add/remove leading '-'
        if(intk->empty || intk->value == 0)
        {
          intk->empty = 1;
          intk->leading_minus ^= 1;
        }
        else

        // Negate the existing number
        if(-intk->value >= intk->min_val && -intk->value <= intk->max_val)
        {
          change_value(intk, intk->value * -1);
        }
      }
      return true;
    }

    case IKEY_HOME:
    {
      intk->value = intk->min_val;
      return true;
    }

    case IKEY_END:
    {
      intk->value = intk->max_val;
      return true;
    }

    case IKEY_RIGHT:
    case IKEY_UP:
    {
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
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
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
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
  }

  if(increment_value)
  {
    change_value(intk, intk->value + increment_value);
    return true;
  }

  return false;
}

/**
 * New mouse clicks exit and save the new value.
 */

static boolean intake_num_click(context *ctx, int *key, int button,
 int x, int y)
{
  if(button && !get_mouse_drag())
  {
    destroy_context(ctx);
    return true;
  }
  return false;
}

/**
 * On exit, get the final value and execute the callback if necessary.
 */

static void intake_num_destroy(context *ctx)
{
  struct intake_num_context *intk = (struct intake_num_context *)ctx;

  if(intk->save)
  {
    int value = CLAMP(intk->value, intk->min_val, intk->max_val);

    if(intk->callback_fn)
      intk->callback_fn(intk->callback_ctx, value);
  }
}

/**
 * Create a context for number entry. On exit if the number was changed, the
 * callback will be executed using the parent context and the final value as
 * inputs.
 */

context *intake_num(context *parent, int value, int min_val, int max_val,
 int x, int y, int min_width, int color, void (*callback)(context *, int))
{
  struct intake_num_context *intk = cmalloc(sizeof(struct intake_num_context));
  struct context_spec spec;

  intk->x = x;
  intk->y = y;
  intk->w = min_width;
  intk->color = color;

  intk->value = value;
  intk->min_val = min_val;
  intk->max_val = max_val;
  intk->leading_minus = false;
  intk->empty = false;
  intk->save = true;

  intk->callback_ctx = parent;
  intk->callback_fn = callback;

  fix_draw_area(intk);

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = intake_num_draw;
  spec.key      = intake_num_key;
  spec.click    = intake_num_click;
  spec.destroy  = intake_num_destroy;

  create_context((context *)intk, parent, &spec, CTX_INTAKE_NUM);
  return (context *)intk;
}
