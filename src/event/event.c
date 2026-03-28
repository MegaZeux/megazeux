/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2019-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "event.h"
#include "../configure.h"
#include "../graphics.h"
#include "../util.h"
#include "../platform/platform.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KEY_REPEAT_START    250
#define KEY_REPEAT_RATE     33

#define MOUSE_REPEAT_START  200
#define MOUSE_REPEAT_RATE   10

#define JOYSTICK_REPEAT_START KEY_REPEAT_START
#define JOYSTICK_REPEAT_RATE  KEY_REPEAT_RATE

#if defined(__linux__) && !defined(__ANDROID__)
/* TODO: Steam Deck hack. SteamOS apparently broke this and the fix has
 * not been backported to SDL2. Additionally, SteamOS has its own shortcut
 * to open the screen keyboard, so it's not necessary to sacrifice a button.
 * Platforms that don't support this don't need to worry about it.
 * If Linux SDL ever gets another source of screen keyboard this will
 * need to be revised. */
#define DEFAULT_SCREEN_KEYBOARD_ACTION JOY_NO_ACTION
#else
#define DEFAULT_SCREEN_KEYBOARD_ACTION JOY_RSHOULDER
#endif

static uint32_t last_update_time;

struct input_status input;

static unsigned int num_buffered_events = 1;

boolean enable_f12_hack;

static boolean joystick_legacy_loop_hacks = false;
static boolean joystick_game_mode = false;
static boolean joystick_game_bindings = true;

static enum keycode joystick_action_map_default[NUM_JOYSTICK_ACTIONS] =
{
  [JOY_A] = IKEY_SPACE,
  [JOY_B] = IKEY_DELETE,
  [JOY_X] = IKEY_RETURN,
  [JOY_Y] = IKEY_s,
  [JOY_SELECT] = IKEY_ESCAPE,
  [JOY_START] = IKEY_RETURN,
  [JOY_LSTICK] = 0,
  [JOY_RSTICK] = 0,
  [JOY_LSHOULDER] = IKEY_INSERT,
  [JOY_RSHOULDER] = IKEY_p,
  [JOY_LTRIGGER] = IKEY_F3,
  [JOY_RTRIGGER] = IKEY_F4,
  [JOY_UP] = IKEY_UP,
  [JOY_DOWN] = IKEY_DOWN,
  [JOY_LEFT] = IKEY_LEFT,
  [JOY_RIGHT] = IKEY_RIGHT,
  [JOY_L_UP] = IKEY_UP,
  [JOY_L_DOWN] = IKEY_DOWN,
  [JOY_L_LEFT] = IKEY_LEFT,
  [JOY_L_RIGHT] = IKEY_RIGHT,
  [JOY_R_UP] = IKEY_UP,
  [JOY_R_DOWN] = IKEY_DOWN,
  [JOY_R_LEFT] = IKEY_LEFT,
  [JOY_R_RIGHT] = IKEY_RIGHT,
};

static enum keycode joystick_action_map_ui[NUM_JOYSTICK_ACTIONS] =
{
  [JOY_A] = IKEY_RETURN,
  [JOY_B] = IKEY_ESCAPE,
  [JOY_X] = 0,
  [JOY_Y] = 0,
  [JOY_SELECT] = IKEY_ESCAPE,
  [JOY_START] = IKEY_RETURN,
  [JOY_LSHOULDER] = IKEY_TAB,
  [JOY_RSHOULDER] = IKEY_F2,
  [JOY_LTRIGGER] = IKEY_PAGEUP,
  [JOY_RTRIGGER] = IKEY_PAGEDOWN,
  [JOY_LSTICK] = IKEY_HOME,
  [JOY_RSTICK] = IKEY_END,
  [JOY_UP] = IKEY_UP,
  [JOY_DOWN] = IKEY_DOWN,
  [JOY_LEFT] = IKEY_LEFT,
  [JOY_RIGHT] = IKEY_RIGHT,
  [JOY_L_UP] = IKEY_UP,
  [JOY_L_DOWN] = IKEY_DOWN,
  [JOY_L_LEFT] = IKEY_LEFT,
  [JOY_L_RIGHT] = IKEY_RIGHT,
  [JOY_R_UP] = IKEY_UP,
  [JOY_R_DOWN] = IKEY_DOWN,
  [JOY_R_LEFT] = IKEY_LEFT,
  [JOY_R_RIGHT] = IKEY_RIGHT,
};

struct buffered_status *store_status(void)
{
  return &input.buffer[input.store_offset];
}

#ifndef CONFIG_NDS
static
#endif
const struct buffered_status *load_status(void)
{
  return (const struct buffered_status *)&input.buffer[input.load_offset];
}

static void bump_status(void)
{
  unsigned int last_store_offset = input.store_offset;
  uint32_t *unicode;

  input.store_offset = (input.store_offset + 1) % num_buffered_events;
  input.load_offset = (input.store_offset + 1) % num_buffered_events;

  // No event buffering; nothing to do
  if(input.store_offset == input.load_offset)
    return;

  // Some events can "echo" from the previous buffer
  unicode = store_status()->unicode;
  memcpy(store_status(), &input.buffer[last_store_offset],
         sizeof(struct buffered_status));
  store_status()->unicode = unicode;
}

void init_event(struct config_info *conf)
{
  int i, i2;

  // Get defaults from config.
  num_buffered_events = MAX(1, conf->num_buffered_events);
  input.unfocus_pause = conf->pause_on_unfocus;
  input.left_alt_is_altgr = conf->key_left_alt_is_altgr;
  input.right_alt_is_altgr = conf->key_right_alt_is_altgr;

  input.buffer = ccalloc(num_buffered_events, sizeof(struct buffered_status));
  input.load_offset = num_buffered_events - 1;
  input.store_offset = 0;

  // Load default action keybindings for gameplay.
  // Config has already loaded, so there might be user mappings here already.
  for(i = 0; i < MAX_JOYSTICKS; i++)
    for(i2 = 0; i2 < NUM_JOYSTICK_ACTIONS; i2++)
      if(!input.joystick_global_map.action_is_conf[i][i2])
        input.joystick_global_map.action[i][i2] =
         joystick_action_map_default[i2];

  for(i = 0; i < MAX_JOYSTICKS; i++)
    if(!input.joystick_global_map.show_keyboard_is_conf[i])
      input.joystick_global_map.show_screen_keyboard_action[i] =
       DEFAULT_SCREEN_KEYBOARD_ACTION;

  if(!input.joystick_axis_threshold)
    input.joystick_axis_threshold = AXIS_DEFAULT_THRESHOLD;

  // Write the new global bindings over the game bindings.
  joystick_reset_game_map();

  // Platform-specific events system initialization.
  platform_init_event();
}

uint32_t convert_internal_unicode(enum keycode key, boolean caps_lock)
{
  if(KEYCODE_IS_ASCII(key))
  {
    boolean shift_status = get_shift_status(keycode_internal);

    if(caps_lock && (key >= IKEY_a) && (key <= IKEY_z))
    {
      if(!shift_status)
        return (key - 32);
      return key;
    }

    if(shift_status)
    {
      if((key >= IKEY_a) && (key <= IKEY_z))
        return (key - 32);

      // TODO based on a US keyboard right now since that's as good as any
      // default and possibly what most users are going to expect. It would
      // be nice to have more locales at some point if someone requests them.
      switch(key)
      {
        case IKEY_BACKQUOTE:    return '~';
        case IKEY_1:            return '!';
        case IKEY_2:            return '@';
        case IKEY_3:            return '#';
        case IKEY_4:            return '$';
        case IKEY_5:            return '%';
        case IKEY_6:            return '^';
        case IKEY_7:            return '&';
        case IKEY_8:            return '*';
        case IKEY_9:            return '(';
        case IKEY_0:            return ')';
        case IKEY_MINUS:        return '_';
        case IKEY_EQUALS:       return '+';
        case IKEY_LEFTBRACKET:  return '{';
        case IKEY_RIGHTBRACKET: return '}';
        case IKEY_BACKSLASH:    return '|';
        case IKEY_SEMICOLON:    return ':';
        case IKEY_QUOTE:        return '"';
        case IKEY_COMMA:        return '<';
        case IKEY_PERIOD:       return '>';
        case IKEY_SLASH:        return '?';
        default:                return IKEY_UNKNOWN;
      }
    }
    return key;
  }
  return IKEY_UNKNOWN;
}

static uint32_t convert_internal_xt(enum keycode key)
{
  switch(key)
  {
    case IKEY_ESCAPE: return 0x01;
    case IKEY_F1: return 0x3B;
    case IKEY_F2: return 0x3C;
    case IKEY_F3: return 0x3D;
    case IKEY_F4: return 0x3E;
    case IKEY_F5: return 0x3F;
    case IKEY_F6: return 0x40;
    case IKEY_F7: return 0x41;
    case IKEY_F8: return 0x42;
    case IKEY_F9: return 0x43;
    case IKEY_F10: return 0x44;
    case IKEY_F11: return 0x57;
    case IKEY_F12: return 0x58;
    case IKEY_BACKQUOTE: return 0x29;
    case IKEY_1: return 0x02;
    case IKEY_2: return 0x03;
    case IKEY_3: return 0x04;
    case IKEY_4: return 0x05;
    case IKEY_5: return 0x06;
    case IKEY_6: return 0x07;
    case IKEY_7: return 0x08;
    case IKEY_8: return 0x09;
    case IKEY_9: return 0x0A;
    case IKEY_0: return 0x0B;
    case IKEY_MINUS: return 0x0C;
    case IKEY_EQUALS: return 0x0D;
    case IKEY_BACKSLASH: return 0x2B;
    case IKEY_BACKSPACE: return 0x0E;
    case IKEY_TAB: return 0x0F;
    case IKEY_q: return 0x10;
    case IKEY_w: return 0x11;
    case IKEY_e: return 0x12;
    case IKEY_r: return 0x13;
    case IKEY_t: return 0x14;
    case IKEY_y: return 0x15;
    case IKEY_u: return 0x16;
    case IKEY_i: return 0x17;
    case IKEY_o: return 0x18;
    case IKEY_p: return 0x19;
    case IKEY_LEFTBRACKET: return 0x1A;
    case IKEY_RIGHTBRACKET: return 0x1B;
    case IKEY_CAPSLOCK: return 0x3A;
    case IKEY_a: return 0x1E;
    case IKEY_s: return 0x1F;
    case IKEY_d: return 0x20;
    case IKEY_f: return 0x21;
    case IKEY_g: return 0x22;
    case IKEY_h: return 0x23;
    case IKEY_j: return 0x24;
    case IKEY_k: return 0x25;
    case IKEY_l: return 0x26;
    case IKEY_SEMICOLON: return 0x27;
    case IKEY_QUOTE: return 0x28;
    case IKEY_RETURN: return 0x1C;
    case IKEY_LSHIFT: return 0x2A;
    case IKEY_z: return 0x2C;
    case IKEY_x: return 0x2D;
    case IKEY_c: return 0x2E;
    case IKEY_v: return 0x2F;
    case IKEY_b: return 0x30;
    case IKEY_n: return 0x31;
    case IKEY_m: return 0x32;
    case IKEY_COMMA: return 0x33;
    case IKEY_PERIOD: return 0x34;
    case IKEY_SLASH: return 0x35;
    case IKEY_RSHIFT: return 0x36;
    case IKEY_LCTRL: return 0x1D;
    case IKEY_LSUPER: return 0x5B;
    case IKEY_LALT: return 0x38;
    case IKEY_SPACE: return 0x39;
    case IKEY_RALT: return 0x38;
    case IKEY_ALTGR: return 0x38;
    case IKEY_RSUPER: return 0x5C;
    case IKEY_MENU: return 0x5D;
    case IKEY_RCTRL: return 0x1D;
    case IKEY_SYSREQ: return 0x37;
    case IKEY_SCROLLOCK: return 0x46;
    case IKEY_BREAK: return 0xC5;
    case IKEY_INSERT: return 0x52;
    case IKEY_HOME: return 0x47;
    case IKEY_PAGEUP: return 0x49;
    case IKEY_DELETE: return 0x53;
    case IKEY_END: return 0x4F;
    case IKEY_PAGEDOWN: return 0x51;
    case IKEY_NUMLOCK: return 0x45;
    case IKEY_KP_DIVIDE: return 0x35;
    case IKEY_KP_MULTIPLY: return 0x37;
    case IKEY_KP_MINUS: return 0x4A;
    case IKEY_KP7: return 0x47;
    case IKEY_KP8: return 0x48;
    case IKEY_KP9: return 0x49;
    case IKEY_KP4: return 0x4B;
    case IKEY_KP5: return 0x4C;
    case IKEY_KP6: return 0x4D;
    case IKEY_KP_PLUS: return 0x4E;
    case IKEY_KP1: return 0x4F;
    case IKEY_KP2: return 0x50;
    case IKEY_KP3: return 0x51;
    case IKEY_KP_ENTER: return 0x1C;
    case IKEY_KP0: return 0x52;
    case IKEY_KP_PERIOD: return 0x53;
    case IKEY_UP: return 0x48;
    case IKEY_LEFT: return 0x4B;
    case IKEY_DOWN: return 0x50;
    case IKEY_RIGHT: return 0x4D;

    // All others are currently undefined; returns 0
    default: return 0;
  }
}

static enum keycode convert_xt_internal(uint32_t key, enum keycode *second,
 enum keycode *third)
{
  *second = IKEY_UNKNOWN;
  *third = IKEY_UNKNOWN;
  switch(key)
  {
    case 0x01: return IKEY_ESCAPE;
    case 0x3B: return IKEY_F1;
    case 0x3C: return IKEY_F2;
    case 0x3D: return IKEY_F3;
    case 0x3E: return IKEY_F4;
    case 0x3F: return IKEY_F5;
    case 0x40: return IKEY_F6;
    case 0x41: return IKEY_F7;
    case 0x42: return IKEY_F8;
    case 0x43: return IKEY_F9;
    case 0x44: return IKEY_F10;
    case 0x57: return IKEY_F11;
    case 0x58: return IKEY_F12;
    case 0x29: return IKEY_BACKQUOTE;
    case 0x02: return IKEY_1;
    case 0x03: return IKEY_2;
    case 0x04: return IKEY_3;
    case 0x05: return IKEY_4;
    case 0x06: return IKEY_5;
    case 0x07: return IKEY_6;
    case 0x08: return IKEY_7;
    case 0x09: return IKEY_8;
    case 0x0A: return IKEY_9;
    case 0x0B: return IKEY_0;
    case 0x0C: return IKEY_MINUS;
    case 0x0D: return IKEY_EQUALS;
    case 0x2B: return IKEY_BACKSLASH;
    case 0x0E: return IKEY_BACKSPACE;
    case 0x0F: return IKEY_TAB;
    case 0x10: return IKEY_q;
    case 0x11: return IKEY_w;
    case 0x12: return IKEY_e;
    case 0x13: return IKEY_r;
    case 0x14: return IKEY_t;
    case 0x15: return IKEY_y;
    case 0x16: return IKEY_u;
    case 0x17: return IKEY_i;
    case 0x18: return IKEY_o;
    case 0x19: return IKEY_p;
    case 0x1A: return IKEY_LEFTBRACKET;
    case 0x1B: return IKEY_RIGHTBRACKET;
    case 0x3A: return IKEY_CAPSLOCK;
    case 0x1E: return IKEY_a;
    case 0x1F: return IKEY_s;
    case 0x20: return IKEY_d;
    case 0x21: return IKEY_f;
    case 0x22: return IKEY_g;
    case 0x23: return IKEY_h;
    case 0x24: return IKEY_j;
    case 0x25: return IKEY_k;
    case 0x26: return IKEY_l;
    case 0x27: return IKEY_SEMICOLON;
    case 0x28: return IKEY_QUOTE;
    case 0x1C:
      *second = IKEY_KP_ENTER;
      return IKEY_RETURN;
    case 0x2A: return IKEY_LSHIFT;
    case 0x2C: return IKEY_z;
    case 0x2D: return IKEY_x;
    case 0x2E: return IKEY_c;
    case 0x2F: return IKEY_v;
    case 0x30: return IKEY_b;
    case 0x31: return IKEY_n;
    case 0x32: return IKEY_m;
    case 0x33: return IKEY_COMMA;
    case 0x34: return IKEY_PERIOD;
    case 0x35:
      *second = IKEY_KP_DIVIDE;
      return IKEY_SLASH;
    case 0x36: return IKEY_RSHIFT;
    case 0x1D:
      *second = IKEY_RCTRL;
      return IKEY_LCTRL;
    case 0x5B: return IKEY_LSUPER;
    case 0x38:
      *second = IKEY_RALT;
      *third = IKEY_ALTGR;
      return IKEY_LALT;
    case 0x39: return IKEY_SPACE;
    case 0x5C: return IKEY_RSUPER;
    case 0x5D: return IKEY_MENU;
    case 0x37:
      *second = IKEY_KP_MULTIPLY;
      return IKEY_SYSREQ;
    case 0x46: return IKEY_SCROLLOCK;
    case 0xC5: return IKEY_BREAK;
    case 0x52:
      *second = IKEY_KP0;
      return IKEY_INSERT;
    case 0x47:
      *second = IKEY_KP7;
      return IKEY_HOME;
    case 0x49:
      *second = IKEY_KP9;
      return IKEY_PAGEUP;
    case 0x53:
      *second = IKEY_KP_PERIOD;
      return IKEY_DELETE;
    case 0x4F:
      *second = IKEY_KP1;
      return IKEY_END;
    case 0x51:
      *second = IKEY_KP3;
      return IKEY_PAGEDOWN;
    case 0x45: return IKEY_NUMLOCK;
    case 0x4A: return IKEY_KP_MINUS;
    case 0x48:
      *second = IKEY_UP;
      return IKEY_KP8;
    case 0x4B:
      *second = IKEY_LEFT;
       return IKEY_KP4;
    case 0x4C: return IKEY_KP5;
    case 0x4D:
      *second = IKEY_RIGHT;
      return IKEY_KP6;
    case 0x4E: return IKEY_KP_PLUS;
    case 0x50:
      *second = IKEY_DOWN;
      return IKEY_KP2;
    default: return IKEY_UNKNOWN;
  }
}

static boolean update_autorepeat(void)
{
  // The repeat key may not be a "valid" keycode due to the unbounded nature
  // of joypad support.  All invalid keys use the last position because that's
  // better than crashing.
  struct buffered_status *status = store_status();
  enum keycode status_key =
   MIN((unsigned int) status->key_repeat, STATUS_NUM_KEYCODES - 1);
  boolean rval = false;

  // Repeat code
  int last_key_state = status->keymap[status_key];
  int last_mouse_state = status->mouse_repeat_state;
  int last_joystick_state = status->joystick_repeat_state;

  // If you enable SDL 2.0 key repeat, uncomment these lines:
//#ifdef CONFIG_SDL
//#if SDL_VERSION_ATLEAST(2,0,0)
  //last_key_state = 0;
  //input.repeat_stack_pointer = 0;
//#endif
//#endif

  if(last_key_state)
  {
    uint32_t new_time = get_ticks();
    uint32_t ms_difference = new_time - status->keypress_time;

    if(last_key_state == 1)
    {
      if(ms_difference > KEY_REPEAT_START)
      {
        status->keypress_time = new_time;
        status->keymap[status_key] = 2;
        status->key = status->key_repeat;
        key_press_unicode(status, status->unicode_repeat, false);
        rval = true;
      }
    }
    else
    {
      if(ms_difference > KEY_REPEAT_RATE)
      {
        status->keypress_time = new_time;
        status->key = status->key_repeat;
        key_press_unicode(status, status->unicode_repeat, false);
        rval = true;
      }
    }
  }
  else

  if(input.repeat_stack_pointer)
  {
    // Take repeat off the stack.
    input.repeat_stack_pointer--;
    status->key_repeat =
     input.key_repeat_stack[input.repeat_stack_pointer];
    status->unicode_repeat =
     input.unicode_repeat_stack[input.repeat_stack_pointer];

    status->keypress_time = get_ticks();
  }

  if(last_mouse_state)
  {
    uint32_t new_time = get_ticks();
    uint32_t ms_difference = new_time - status->mouse_time;

    if(status->mouse_drag_state == -1)
      status->mouse_drag_state = 0;
    else
      status->mouse_drag_state = 1;

    if(last_mouse_state == 1)
    {
      if(ms_difference > MOUSE_REPEAT_START)
      {
        status->mouse_time = new_time;
        status->mouse_repeat_state = 2;
        status->mouse_button = status->mouse_repeat;
        rval = true;
      }
    }
    else
    {
      if(ms_difference > MOUSE_REPEAT_RATE)
      {
        status->mouse_time = new_time;
        status->mouse_button = status->mouse_repeat;
        rval = true;
      }
    }
  }

  if(last_joystick_state)
  {
    uint32_t new_time = get_ticks();
    uint32_t ms_difference = new_time - status->joystick_time;

    if(last_joystick_state == 1)
    {
      if(ms_difference > JOYSTICK_REPEAT_START)
      {
        status->joystick_time = new_time;
        status->joystick_repeat_state = 2;
        status->joystick_action = status->joystick_repeat;
        rval = true;
      }
    }
    else
    {
      if(ms_difference > JOYSTICK_REPEAT_RATE)
      {
        status->joystick_time = new_time;
        status->joystick_action = status->joystick_repeat;
        rval = true;
      }
    }
  }

  bump_status();
  return rval;
}

static void start_frame_event_status(void)
{
  struct buffered_status *status = store_status();

  status->key = IKEY_UNKNOWN;
  status->unicode_pos = 0;
  status->unicode_length = 0;
  status->mouse_moved = 0;
  status->mouse_button = 0;
  status->joystick_action = 0;
  status->exit_status = false;

  status->mouse_last_x = status->mouse_x;
  status->mouse_last_y = status->mouse_y;
  status->mouse_last_pixel_x = status->mouse_pixel_x;
  status->mouse_last_pixel_y = status->mouse_pixel_y;
  status->warped_mouse_x = -1;
  status->warped_mouse_y = -1;
}

boolean update_event_status(void)
{
  boolean rval;

  start_frame_event_status();

  rval  = __update_event_status();
  rval |= update_autorepeat();

  return rval;
}

boolean peek_exit_input(void)
{
  return __peek_exit_input();
}

void wait_event(int timeout)
{
  start_frame_event_status();

  if(timeout > 0)
  {
    int ticks_start = get_ticks();
    int ticks_end;

    while(timeout > 0)
    {
      delay(1);

      if(__update_event_status())
        break;

      ticks_end = get_ticks();
      timeout -= ticks_end - ticks_start;
      ticks_start = ticks_end;
    }
  }
  else
    __wait_event();

  update_autorepeat();
}

boolean update_event_status_delay(void)
{
  int delay_ticks;

  if(!last_update_time)
    last_update_time = get_ticks();

  delay_ticks = UPDATE_DELAY - (get_ticks() - last_update_time);

  if(delay_ticks < 0)
    delay_ticks = 0;

  delay(delay_ticks);
  last_update_time = get_ticks();

  return update_event_status();
}

void update_event_status_intake(void)
{
  int delay_ticks;

  if(!last_update_time)
    last_update_time = get_ticks();

  delay_ticks = UPDATE_DELAY - (get_ticks() - last_update_time);

  // wait_event will wait indefinitely unless the timeout is greater than 0.
  if(delay_ticks < 1)
    delay_ticks = 1;

  wait_event(delay_ticks);
  last_update_time = get_ticks();
}

static enum keycode emit_keysym_wrt_numlock(enum keycode key)
{
  const struct buffered_status *status = load_status();

  if(status->numlock_status)
  {
    switch(key)
    {
      case IKEY_KP0:         return IKEY_0;
      case IKEY_KP1:         return IKEY_1;
      case IKEY_KP2:         return IKEY_2;
      case IKEY_KP3:         return IKEY_3;
      case IKEY_KP4:         return IKEY_4;
      case IKEY_KP5:         return IKEY_5;
      case IKEY_KP6:         return IKEY_6;
      case IKEY_KP7:         return IKEY_7;
      case IKEY_KP8:         return IKEY_8;
      case IKEY_KP9:         return IKEY_9;
      case IKEY_KP_PERIOD:   return IKEY_PERIOD;
      case IKEY_KP_ENTER:    return IKEY_RETURN;
      default: break;
    }
  }
  else
  {
    switch(key)
    {
      case IKEY_KP0:         return IKEY_INSERT;
      case IKEY_KP1:         return IKEY_END;
      case IKEY_KP2:         return IKEY_DOWN;
      case IKEY_KP3:         return IKEY_PAGEDOWN;
      case IKEY_KP4:         return IKEY_LEFT;
      case IKEY_KP5:         return IKEY_SPACE; // kinda arbitrary
      case IKEY_KP6:         return IKEY_RIGHT;
      case IKEY_KP7:         return IKEY_HOME;
      case IKEY_KP8:         return IKEY_UP;
      case IKEY_KP9:         return IKEY_PAGEUP;
      case IKEY_KP_PERIOD:   return IKEY_DELETE;
      case IKEY_KP_ENTER:    return IKEY_RETURN;
      default: break;
    }
  }
  return key;
}

static enum keycode reverse_keysym_numlock(enum keycode key)
{
  const struct buffered_status *status = load_status();

  if(status->numlock_status)
  {
    switch(key)
    {
      case IKEY_0:         return IKEY_KP0;
      case IKEY_1:         return IKEY_KP1;
      case IKEY_2:         return IKEY_KP2;
      case IKEY_3:         return IKEY_KP3;
      case IKEY_4:         return IKEY_KP4;
      case IKEY_5:         return IKEY_KP5;
      case IKEY_6:         return IKEY_KP6;
      case IKEY_7:         return IKEY_KP7;
      case IKEY_8:         return IKEY_KP8;
      case IKEY_9:         return IKEY_KP9;
      case IKEY_PERIOD:    return IKEY_KP_PERIOD;
      case IKEY_RETURN:    return IKEY_KP_ENTER;
      default: break;
    }
  }
  else
  {
    switch(key)
    {
      case IKEY_INSERT:    return IKEY_KP0;
      case IKEY_END:       return IKEY_KP1;
      case IKEY_DOWN:      return IKEY_KP2;
      case IKEY_PAGEDOWN:  return IKEY_KP3;
      case IKEY_LEFT:      return IKEY_KP4;
      case IKEY_SPACE:     return IKEY_KP5; // kinda arbitrary
      case IKEY_RIGHT:     return IKEY_KP6;
      case IKEY_HOME:      return IKEY_KP7;
      case IKEY_UP:        return IKEY_KP8;
      case IKEY_PAGEUP:    return IKEY_KP9;
      case IKEY_DELETE:    return IKEY_KP_PERIOD;
      case IKEY_RETURN:    return IKEY_KP_ENTER;
      default: break;
    }
  }
  return key;
}

uint32_t get_key(enum keycode_type type)
{
  struct buffered_status *status = (struct buffered_status *)load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key);

    case keycode_internal:
      return status->key;

    case keycode_internal_wrt_numlock:
      return emit_keysym_wrt_numlock(status->key);

    case keycode_text_ascii:
    {
      while(status->unicode_pos < status->unicode_length)
      {
        uint32_t unicode = status->unicode[status->unicode_pos++];
        if(KEYCODE_IS_ASCII(unicode))
          return unicode;
      }
      break;
    }

    case keycode_text_unicode:
    {
      if(status->unicode_pos < status->unicode_length)
        return status->unicode[status->unicode_pos++];

      break;
    }
  }
  return 0;
}

uint32_t get_key_status(enum keycode_type type, uint32_t index)
{
  const struct buffered_status *status = load_status();
  index = MIN((int)index, STATUS_NUM_KEYCODES - 1);

  switch(type)
  {
    case keycode_pc_xt:
    {
      enum keycode first, second, third;
      first = convert_xt_internal(index, &second, &third);
      return status->keymap[first] || status->keymap[second] || status->keymap[third];
    }

    case keycode_internal:
      return status->keymap[index];

    case keycode_internal_wrt_numlock:
      return status->keymap[index] ||
        status->keymap[reverse_keysym_numlock(index)];

    default:
      return 0;
  }
}

uint32_t get_last_key(enum keycode_type type)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key_pressed);

    case keycode_internal:
      return status->key_pressed;

    case keycode_internal_wrt_numlock:
      return emit_keysym_wrt_numlock(status->key_pressed);

    default:
      return 0;
  }
}

uint32_t get_last_key_released(enum keycode_type type)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key_release);

    case keycode_internal:
      return status->key_release;

    case keycode_internal_wrt_numlock:
      return emit_keysym_wrt_numlock(status->key_release);

    default:
      return 0;
  }
}

void get_mouse_position(int *char_x, int *char_y)
{
  const struct buffered_status *status = load_status();
  *char_x = status->mouse_x;
  *char_y = status->mouse_y;
}

void get_mouse_pixel_position(int *pixel_x, int *pixel_y)
{
  const struct buffered_status *status = load_status();
  *pixel_x = status->mouse_pixel_x;
  *pixel_y = status->mouse_pixel_y;
}

int get_mouse_x(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_x;
}

int get_mouse_y(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_y;
}

int get_mouse_pixel_x(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_pixel_x;
}

int get_mouse_pixel_y(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_pixel_y;
}

void get_mouse_movement(int *delta_x, int *delta_y)
{
  const struct buffered_status *status = load_status();
  *delta_x = status->mouse_x - status->mouse_last_x;
  *delta_y = status->mouse_y - status->mouse_last_y;
}

void get_mouse_pixel_movement(int *delta_x, int *delta_y)
{
  const struct buffered_status *status = load_status();
  *delta_x = status->mouse_pixel_x - status->mouse_last_pixel_x;
  *delta_y = status->mouse_pixel_y - status->mouse_last_pixel_y;
}

boolean get_mouse_drag(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_drag_state > 0;
}

enum mouse_button get_mouse_press(void)
{
  const struct buffered_status *status = load_status();

  if(status->mouse_button <= MOUSE_BUTTON_RIGHT)
    return status->mouse_button;

  return MOUSE_NO_BUTTON;
}

enum mouse_button get_mouse_press_ext(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_button;
}

boolean get_mouse_held(int button)
{
  const struct buffered_status *status = load_status();

  if(button >= 1 && button <= 32)
    if(status->mouse_button_state & MOUSE_BUTTON(button))
      return true;

  return false;
}

uint32_t get_mouse_status(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_button_state;
}

void warp_mouse(int char_x, int char_y)
{
  int real_x, real_y;
  int pixel_x = (char_x * CHAR_W) + CHAR_W / 2;
  int pixel_y = (char_y * CHAR_H) + CHAR_H / 2;
  struct buffered_status *status = store_status();

  status->mouse_x = char_x;
  status->mouse_y = char_y;
  status->mouse_pixel_x = pixel_x;
  status->mouse_pixel_y = pixel_y;

  set_screen_coords(pixel_x, pixel_y, &real_x, &real_y);
  status->warped_mouse_x = real_x;
  status->warped_mouse_y = real_y;

  __warp_mouse(real_x, real_y);
}

void warp_mouse_x(int char_x)
{
  warp_mouse_pixel_x(char_x * CHAR_W + CHAR_W / 2);
}

void warp_mouse_y(int char_y)
{
  warp_mouse_pixel_y(char_y * CHAR_H + CHAR_H / 2);
}

void warp_mouse_pixel_x(int pixel_x)
{
  struct buffered_status *status = store_status();
  int real_x, real_y;
  int char_x = pixel_x / CHAR_W;

  status->mouse_x = char_x;
  status->mouse_pixel_x = pixel_x;

  set_screen_coords(pixel_x, status->mouse_pixel_y, &real_x, &real_y);
  status->warped_mouse_x = real_x;

  __warp_mouse(real_x, status->warped_mouse_y);
}

void warp_mouse_pixel_y(int pixel_y)
{
  struct buffered_status *status = store_status();
  int real_x, real_y;
  int char_y = pixel_y / CHAR_H;

  status->mouse_y = char_y;
  status->mouse_pixel_y = pixel_y;

  set_screen_coords(status->mouse_pixel_x, pixel_y, &real_x, &real_y);
  status->warped_mouse_y = real_y;

  __warp_mouse(status->warped_mouse_x, real_y);
}

void force_last_key(enum keycode_type type, int val)
{
  struct buffered_status *status = store_status();

  switch(type)
  {
    case keycode_pc_xt:
    {
      enum keycode second, third;
      status->key_pressed = convert_xt_internal(val, &second, &third);
      break;
    }

    case keycode_internal:
    case keycode_internal_wrt_numlock:
    {
      status->key_pressed = (enum keycode)val;
      break;
    }

    default:
      break;
  }
}

void force_release_all_keys(void)
{
  struct buffered_status *status = store_status();

  force_last_key(keycode_internal, 0);
  memset(status->keymap, 0, sizeof(status->keymap));

  status->key = 0;
  status->mouse_button = 0;
  status->mouse_repeat = 0;
  status->mouse_button_state = 0;
  status->mouse_repeat_state = 0;
  status->mouse_drag_state = 0;
  status->joystick_action = 0;
  status->joystick_repeat = 0;
  status->joystick_repeat_state = 0;
}

boolean get_alt_status(enum keycode_type type)
{
  /* Some Mac users have indicated that they prefer to use the command key.
   * Amiga has the Amiga keys on alt scancodes and alt on the meta scancodes.
   */
  return
#if defined(__APPLE__) || defined(CONFIG_AMIGA)
   get_key_status(type, IKEY_LSUPER) ||
   get_key_status(type, IKEY_RSUPER) ||
#endif
   (!input.left_alt_is_altgr && get_key_status(type, IKEY_LALT)) ||
   (!input.right_alt_is_altgr && get_key_status(type, IKEY_RALT));
}

boolean get_altgr_status(enum keycode_type type)
{
  return
   (input.left_alt_is_altgr && get_key_status(type, IKEY_LALT)) ||
   (input.right_alt_is_altgr && get_key_status(type, IKEY_RALT)) ||
   get_key_status(type, IKEY_ALTGR);
}

boolean get_shift_status(enum keycode_type type)
{
  return
   get_key_status(type, IKEY_LSHIFT) ||
   get_key_status(type, IKEY_RSHIFT);
}

boolean get_ctrl_status(enum keycode_type type)
{
  return
   get_key_status(type, IKEY_LCTRL) ||
   get_key_status(type, IKEY_RCTRL);
}

// Nothing uses this right now, but it needs to be settable for networking.
void set_unfocus_pause(boolean value)
{
  input.unfocus_pause = value;
}

// Nothing uses this right now, but it needs to be settable for networking.
// TODO: when set this ought to actually reallocate the buffered events.
void set_num_buffered_events(unsigned int value)
{
  num_buffered_events = MAX(1, value);
}

void key_press(struct buffered_status *status, enum keycode key)
{
  // Prevent invalid keycodes from writing out-of-bounds.
  enum keycode map_key = MIN((int)key, STATUS_NUM_KEYCODES - 1);
  status->keymap[map_key] = 1;
  status->key_pressed = key;
  status->key = key;
  status->key_repeat = key;
  status->keypress_time = get_ticks();
  status->key_release = IKEY_UNKNOWN;
}

void key_press_unicode(struct buffered_status *status, uint32_t unicode,
 boolean repeating)
{
  if(unicode)
  {
    if(status->unicode_length < KEY_UNICODE_MAX)
    {
      if(status->unicode_length >= status->unicode_alloc)
      {
        int alloc = MAX(4, status->unicode_alloc << 1);
        uint32_t *unicode =
         (uint32_t *)crealloc(status->unicode, alloc * sizeof(uint32_t));
        if(unicode)
        {
          status->unicode = unicode;
          status->unicode_alloc = alloc;
        }
      }
      if(status->unicode_length < status->unicode_alloc)
        status->unicode[status->unicode_length++] = unicode;
    }
    else
      status->unicode[KEY_UNICODE_MAX - 1] = unicode;
  }
  if(repeating)
    status->unicode_repeat = unicode;
}

void key_release(struct buffered_status *status, enum keycode key)
{
  // Prevent invalid keycodes from writing out-of-bounds.
  enum keycode map_key = MIN((int)key, STATUS_NUM_KEYCODES - 1);
  status->keymap[map_key] = 0;
  status->key_release = key;

  if(status->key_repeat == key)
  {
    status->key_repeat = IKEY_UNKNOWN;
    status->unicode_repeat = 0;
  }
}

boolean has_unicode_input(void)
{
  const struct buffered_status *status = load_status();
  return status->unicode_length > status->unicode_pos;
}

boolean get_exit_status(void)
{
  const struct buffered_status *status = load_status();
  return status->exit_status;
}

boolean set_exit_status(boolean value)
{
  struct buffered_status *status = store_status();
  boolean exit_status = status->exit_status;
  status->exit_status = value;
  return exit_status;
}

/**
 * @return true if the screen keyboard is active, otherwise false.
 */
boolean screen_keyboard_is_active(void)
{
  return platform_is_screen_keyboard_active();
}

/**
 * Toggle the state of the screen keyboard and report whether or
 * not the event was handled.
 *
 * Note platforms may not actually change the keyboard state
 * (e.g. during an animation), but will still report the change
 * as handled. A return value of false should be interpreted as
 * lack of a functioning screen keyboard.
 *
 * @return true if the event is supported and handled, otherwise false.
 */
boolean screen_keyboard_toggle_state(void)
{
  if(platform_has_screen_keyboard())
  {
    if(!platform_is_screen_keyboard_active())
    {
      if(platform_show_screen_keyboard())
        return true;
    }
    else
    {
      if(platform_hide_screen_keyboard())
        return true;
    }
  }
  return false;
}

struct keycode_name
{
  const char * const name;
  const enum keycode value;
};

struct joystick_action_name
{
  const char * const name;
  const int value;
};

static const struct keycode_name keycode_names[] =
{
  { "0",            IKEY_0 },
  { "1",            IKEY_1 },
  { "2",            IKEY_2 },
  { "3",            IKEY_3 },
  { "4",            IKEY_4 },
  { "5",            IKEY_5 },
  { "6",            IKEY_6 },
  { "7",            IKEY_7 },
  { "8",            IKEY_8 },
  { "9",            IKEY_9 },
  { "a",            IKEY_a },
  { "b",            IKEY_b },
  { "backquote",    IKEY_BACKQUOTE },
  { "backslash",    IKEY_BACKSLASH },
  { "backspace",    IKEY_BACKSPACE },
  { "break",        IKEY_BREAK },
  { "c",            IKEY_c },
  { "capslock",     IKEY_CAPSLOCK },
  { "comma",        IKEY_COMMA },
  { "d",            IKEY_d },
  { "delete",       IKEY_DELETE },
  { "down",         IKEY_DOWN },
  { "e",            IKEY_e },
  { "end",          IKEY_END },
  { "equals",       IKEY_EQUALS },
  { "escape",       IKEY_ESCAPE },
  { "f",            IKEY_f },
  { "f1",           IKEY_F1 },
  { "f10",          IKEY_F10 },
  { "f11",          IKEY_F11 },
  { "f12",          IKEY_F12 },
  { "f2",           IKEY_F2 },
  { "f3",           IKEY_F3 },
  { "f4",           IKEY_F4 },
  { "f5",           IKEY_F5 },
  { "f6",           IKEY_F6 },
  { "f7",           IKEY_F7 },
  { "f8",           IKEY_F8 },
  { "f9",           IKEY_F9 },
  { "g",            IKEY_g },
  { "h",            IKEY_h },
  { "home",         IKEY_HOME },
  { "i",            IKEY_i },
  { "insert",       IKEY_INSERT },
  { "j",            IKEY_j },
  { "k",            IKEY_k },
  { "kp0",          IKEY_KP0 },
  { "kp1",          IKEY_KP1 },
  { "kp2",          IKEY_KP2 },
  { "kp3",          IKEY_KP3 },
  { "kp4",          IKEY_KP4 },
  { "kp5",          IKEY_KP5 },
  { "kp6",          IKEY_KP6 },
  { "kp7",          IKEY_KP7 },
  { "kp8",          IKEY_KP8 },
  { "kp9",          IKEY_KP9 },
  { "kp_divide",    IKEY_KP_DIVIDE },
  { "kp_enter",     IKEY_KP_ENTER },
  { "kp_minus",     IKEY_KP_MINUS },
  { "kp_multiply",  IKEY_KP_MULTIPLY },
  { "kp_period",    IKEY_KP_PERIOD },
  { "kp_plus",      IKEY_KP_PLUS },
  { "l",            IKEY_l },
  { "lalt",         IKEY_LALT },
  { "lctrl",        IKEY_LCTRL },
  { "left",         IKEY_LEFT },
  { "leftbracket",  IKEY_LEFTBRACKET },
  { "lshift",       IKEY_LSHIFT },
  { "lsuper",       IKEY_LSUPER },
  { "m",            IKEY_m },
  { "menu",         IKEY_MENU },
  { "minus",        IKEY_MINUS },
  { "n",            IKEY_n },
  { "numlock",      IKEY_NUMLOCK },
  { "o",            IKEY_o },
  { "p",            IKEY_p },
  { "pagedown",     IKEY_PAGEDOWN },
  { "pageup",       IKEY_PAGEUP },
  { "period",       IKEY_PERIOD },
  { "q",            IKEY_q },
  { "quote",        IKEY_QUOTE },
  { "r",            IKEY_r },
  { "ralt",         IKEY_RALT },
  { "rctrl",        IKEY_RCTRL },
  { "return",       IKEY_RETURN },
  { "right",        IKEY_RIGHT },
  { "rightbracket", IKEY_RIGHTBRACKET },
  { "rshift",       IKEY_RSHIFT },
  { "rsuper",       IKEY_RSUPER },
  { "s",            IKEY_s },
  { "scrolllock",   IKEY_SCROLLOCK },
  { "semicolon",    IKEY_SEMICOLON },
  { "slash",        IKEY_SLASH },
  { "space",        IKEY_SPACE },
  { "sysreq",       IKEY_SYSREQ },
  { "t",            IKEY_t },
  { "tab",          IKEY_TAB },
  { "u",            IKEY_u },
  { "up",           IKEY_UP },
  { "v",            IKEY_v },
  { "w",            IKEY_w },
  { "x",            IKEY_x },
  { "y",            IKEY_y },
  { "z",            IKEY_z },
};

static const struct joystick_action_name joystick_action_names[] =
{
  { "a",        JOY_A },
  { "b",        JOY_B },
  { "down",     JOY_DOWN },
  { "l_down",   JOY_L_DOWN },
  { "l_left",   JOY_L_LEFT },
  { "l_right",  JOY_L_RIGHT },
  { "l_up",     JOY_L_UP },
  { "left",     JOY_LEFT },
  { "lshoulder",JOY_LSHOULDER },
  { "lstick",   JOY_LSTICK },
  { "ltrigger", JOY_LTRIGGER },
  { "r_down",   JOY_R_DOWN },
  { "r_left",   JOY_R_LEFT },
  { "r_right",  JOY_R_RIGHT },
  { "r_up",     JOY_R_UP },
  { "right",    JOY_RIGHT },
  { "rshoulder",JOY_RSHOULDER },
  { "rstick",   JOY_RSTICK },
  { "rtrigger", JOY_RTRIGGER },
  { "select",   JOY_SELECT },
  { "start",    JOY_START },
  { "up",       JOY_UP },
  { "x",        JOY_X },
  { "y",        JOY_Y }
};

static const struct joystick_action_name joystick_axis_names[] =
{
  { "ltrigger", JOY_AXIS_LEFT_TRIGGER },
  { "lx",       JOY_AXIS_LEFT_X },
  { "ly",       JOY_AXIS_LEFT_Y },
  { "rtrigger", JOY_AXIS_RIGHT_TRIGGER },
  { "rx",       JOY_AXIS_RIGHT_X },
  { "ry",       JOY_AXIS_RIGHT_Y },
};

/* Get an internal keycode by config name. This does NOT include the key_
 * prefix used by the config file. */
static enum keycode keycode_by_name(const char *name)
{
  int top = ARRAY_SIZE(keycode_names) - 1;
  int bottom = 0;
  int middle;
  int cmpval;

  while(bottom <= top)
  {
    middle = (bottom + top) / 2;
    cmpval = strcasecmp(name, keycode_names[middle].name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;

    else
      return keycode_names[middle].value;
  }
  return IKEY_UNKNOWN;
}

/* Get a joystick action by name. This does NOT include the joy#. or act_
 * prefix used by Robotic and the config file, respectively. */
static enum joystick_action joystick_action_by_name(const char *name)
{
  int top = ARRAY_SIZE(joystick_action_names) - 1;
  int bottom = 0;
  int middle;
  int cmpval;
  if(!name)
    return JOY_NO_ACTION;

  while(bottom <= top)
  {
    middle = (bottom + top) / 2;
    cmpval = strcasecmp(name, joystick_action_names[middle].name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;

    else
      return joystick_action_names[middle].value;
  }
  return JOY_NO_ACTION;
}

/* Get a joystick axis by name. This does NOT include the joy#.axis_ or axis_
 * prefix used by Robotic and the config file, respectively. */
static enum joystick_special_axis joystick_axis_by_name(const char *name)
{
  size_t i;
  for(i = 0; i < ARRAY_SIZE(joystick_axis_names); i++)
    if(!strcasecmp(name, joystick_axis_names[i].name))
      return (enum joystick_special_axis)joystick_axis_names[i].value;

  return JOY_NO_AXIS;
}

/**
 * Provide direct access to the joystick map structure. This is currently only
 * used for config file unit tests but it might be useful later for the
 * settings menu.
 */
struct joystick_map *get_joystick_map(boolean is_global)
{
  if(is_global)
    return &(input.joystick_global_map);
  else
    return &(input.joystick_game_map);
}

/**
 * A joystick can be mapped to either an int from 0 to 32767 (a key binding),
 * a key enum string (also a key binding), or to a joystick action enum string
 * (action binding). Read either from an input value string.
 */
boolean joystick_parse_map_value(const char *value, int16_t *binding)
{
  char *next;
  enum joystick_action action_value;
  unsigned int key_value;

  key_value = strtoul(value, &next, 10);
  if((key_value <= INT16_MAX) && (!next[0]))
  {
    *binding = key_value;
    return true;
  }

  if(!strncmp(value, "act_", 4))
  {
    action_value = joystick_action_by_name(value + 4);
    if(action_value)
    {
      *binding = -((int16_t)action_value);
      return true;
    }
  }
  else

  if(!strncmp(value, "key_", 4))
  {
    key_value = keycode_by_name(value + 4);
    if(key_value)
    {
      *binding = key_value;
      return true;
    }
  }
  return false;
}

/**
 * Map a joystick button to a key or action value. The input value should be
 * null-terminated.
 */
void joystick_map_button(int first, int last, int button, const char *value,
 boolean is_global)
{
  if((first <= last) && (first >= 0) && (first < MAX_JOYSTICKS) &&
   (button >= 0) && (button < MAX_JOYSTICK_BUTTONS))
  {
    int16_t binding;
    int i;

    if(joystick_parse_map_value(value, &binding))
    {
      last = MIN(last, MAX_JOYSTICKS - 1);
      for(i = first; i <= last; i++)
      {
        if(is_global)
        {
          input.joystick_global_map.button[i][button] = binding;
          input.joystick_global_map.button_is_conf[i][button] = true;
        }
        else
        {
          input.joystick_game_map.button[i][button] = binding;
          input.joystick_game_map.button_is_conf[i][button] = true;
        }
      }
    }
  }
}

/**
 * Map a joystick axis to keys or action values. The input values should be
 * null-terminated.
 */
void joystick_map_axis(int first, int last, int axis, const char *neg,
 const char *pos, boolean is_global)
{
  if((first <= last) && (first >= 0) && (first < MAX_JOYSTICKS) &&
   (axis >= 0) && (axis < MAX_JOYSTICK_AXES))
  {
    int16_t binding_neg, binding_pos;
    int i;

    if(joystick_parse_map_value(neg, &binding_neg) &&
     joystick_parse_map_value(pos, &binding_pos))
    {
      last = MIN(last, MAX_JOYSTICKS - 1);
      for(i = first; i <= last; i++)
      {
        if(is_global)
        {
          input.joystick_global_map.axis[i][axis][0] = binding_neg;
          input.joystick_global_map.axis[i][axis][1] = binding_pos;
          input.joystick_global_map.axis_is_conf[i][axis] = true;
        }
        else
        {
          input.joystick_game_map.axis[i][axis][0] = binding_neg;
          input.joystick_game_map.axis[i][axis][1] = binding_pos;
          input.joystick_game_map.axis_is_conf[i][axis] = true;
        }
      }
    }
  }
}

/**
 * Map a joystick hat to keys or action values. The input values should be
 * null-terminated.
 */
void joystick_map_hat(int first, int last, const char *up, const char *down,
 const char *left, const char *right, boolean is_global)
{
  if((first <= last) && (first >= 0) && (first < MAX_JOYSTICKS))
  {
    int16_t binding_up, binding_down, binding_left, binding_right;
    int i;

    if(joystick_parse_map_value(up, &binding_up) &&
     joystick_parse_map_value(down, &binding_down) &&
     joystick_parse_map_value(left, &binding_left) &&
     joystick_parse_map_value(right, &binding_right))
    {
      last = MIN(last, MAX_JOYSTICKS - 1);
      for(i = first; i <= last; i++)
      {
        if(is_global)
        {
          input.joystick_global_map.hat[i][JOYHAT_UP] = binding_up;
          input.joystick_global_map.hat[i][JOYHAT_DOWN] = binding_down;
          input.joystick_global_map.hat[i][JOYHAT_LEFT] = binding_left;
          input.joystick_global_map.hat[i][JOYHAT_RIGHT] = binding_right;
          input.joystick_global_map.hat_is_conf[i] = true;
        }
        else
        {
          input.joystick_game_map.hat[i][JOYHAT_UP] = binding_up;
          input.joystick_game_map.hat[i][JOYHAT_DOWN] = binding_down;
          input.joystick_game_map.hat[i][JOYHAT_LEFT] = binding_left;
          input.joystick_game_map.hat[i][JOYHAT_RIGHT] = binding_right;
          input.joystick_game_map.hat_is_conf[i] = true;
        }
      }
    }
  }
}

/**
 * Map a generic action to a key. The input action should be null-terminated.
 * Only allow key bindings; if value resolves to an action, it is ignored.
 *
 * Alternatively, the action provided may be an axis, in which case the named
 * axis is being assigned to a real axis number.
 *
 * This function also handles the per-joystick setting show_screen_keyboard.
 * In this case, the value must resolve to an action or to 0.
 */
void joystick_map_action(int first, int last, const char *action,
 const char *value, boolean is_global)
{
  if((first <= last) && (first >= 0) && (first < MAX_JOYSTICKS))
  {
    enum joystick_action action_value;
    int16_t binding;
    int i;

    if(!joystick_parse_map_value(value, &binding))
      return;

    last = MIN(last, MAX_JOYSTICKS - 1);

    if(!strcasecmp(action, "show_screen_keyboard"))
    {
      // Must be an action or 0. Valid for the global map only.
      if(binding > 0 || !is_global)
        return;

      for(i = first; i <= last; i++)
      {
        input.joystick_global_map.show_screen_keyboard_action[i] = -binding;
        input.joystick_global_map.show_keyboard_is_conf[i] = true;
      }
      return;
    }

    action_value = joystick_action_by_name(action);
    if(action_value != JOY_NO_ACTION && binding > 0)
    {
      for(i = first; i <= last; i++)
      {
        if(is_global)
        {
          input.joystick_global_map.action[i][action_value] = binding;
          input.joystick_global_map.action_is_conf[i][action_value] = true;
        }
        else
          input.joystick_game_map.action[i][action_value] = binding;
      }
    }
    else

    if(!strncasecmp(action, "axis_", 5) && (binding > 0) &&
     (binding <= MAX_JOYSTICK_AXES))
    {
      enum joystick_special_axis axis_value = joystick_axis_by_name(action + 5);

      if(axis_value == JOY_NO_AXIS)
        return;

      for(i = first; i <= last; i++)
      {
        if(is_global)
          input.joystick_global_map.special_axis[i][binding - 1] = axis_value;
      }
    }
  }
}

/**
 * Set a fallback screen keyboard button for a joystick (console ports)
 * if pad.config failed to load. Call during/after init_event.
 */
void joystick_map_fallback_keyboard_button(int joystick, int button)
{
  assert(joystick >= 0 && joystick < MAX_JOYSTICKS &&
   button >= 0 && button < MAX_JOYSTICK_BUTTONS);

  if(!input.joystick_global_map.button_is_conf[joystick][button] &&
   input.joystick_global_map.button[joystick][button] == 0)
  {
    /* Positive show_screen_keyboard action -> negative button */
    input.joystick_global_map.button[joystick][button] =
     -input.joystick_global_map.show_screen_keyboard_action[joystick];
  }
}

/**
 * Reset the gameplay joystick maps to the global maps. Use this before
 * loading game-specific joystick bindings.
 */
void joystick_reset_game_map(void)
{
  joystick_game_bindings = true;
  memcpy(&(input.joystick_game_map), &(input.joystick_global_map),
   sizeof(struct joystick_map));
}

/**
 * Switch between global and gameplay joystick binding modes. Use 'true' to
 * enable gameplay mode joystick handling.
 */
void joystick_set_game_mode(boolean enable)
{
  joystick_game_mode = enable;
}

/**
 * Enable or disable gameplay bindings in gameplay mode.
 */
void joystick_set_game_bindings(boolean enable)
{
  joystick_game_bindings = enable;
}

/**
 * Enable or disable joystick binding hacks for legacy event loops. Enabling
 * this fixes bindings for non-context event loops, but this should be disabled
 * when processing events for the main loop.
 */
void joystick_set_legacy_loop_hacks(boolean enable)
{
  joystick_legacy_loop_hacks = enable;
}

/**
 * Set the threshold for joystick mapped axis presses. Higher values require
 * more movement to trigger a press.
 */
void joystick_set_axis_threshold(uint16_t threshold)
{
  input.joystick_axis_threshold = threshold;
}

/**
 * Determine which key (if any) a joystick press should bind to
 * within the current context.
 */
static void joystick_resolve_bindings(int joystick, int16_t global_binding,
 int16_t game_binding, enum keycode *key, enum joystick_action *action)
{
  // Global key bindings
  // HACK: places where the no context hacks are enabled are never gameplay,
  // but may be reached while the game mode flag is still enabled. We always
  // want to use global bindings outside of contexts.
  if(!joystick_game_mode || joystick_legacy_loop_hacks)
  {
    if(global_binding > 0)
      *key = (enum keycode)global_binding;
  }

  // Gameplay bindings
  else
  {
    if(game_binding < 0 && (-game_binding < NUM_JOYSTICK_ACTIONS))
    {
      *action = (enum joystick_action)(-game_binding);

      if(joystick_game_bindings &&
       input.joystick_game_map.action[joystick][-game_binding] > 0)
        *key = (enum keycode)input.joystick_game_map.action[joystick][*action];
    }
    else

    if(game_binding > 0 && joystick_game_bindings)
      *key = (enum keycode)game_binding;
  }
}

enum joy_press_type
{
  JOY_BUTTON,
  JOY_AXIS,
  JOY_HAT
};

/**
 * "Press" a joystick button/axis/hat/etc; fake a key press and/or activate
 * the global action used by UI contexts where appropriate.
 */
static void joystick_press(struct buffered_status *status, int joystick,
 enum joy_press_type type, int num, int16_t global_binding, int16_t game_binding)
{
  int pos = status->joystick_press_count[joystick];

  /* Special: hijack the show screen keyboard button. */
  int16_t kbd = input.joystick_global_map.show_screen_keyboard_action[joystick];
  if(global_binding < 0 && -global_binding == kbd)
    if(screen_keyboard_toggle_state())
      return;

  if(pos < MAX_JOYSTICK_PRESS)
  {
    enum joystick_action press_action = JOY_NO_ACTION;
    enum keycode press_key = IKEY_UNKNOWN;

    joystick_resolve_bindings(joystick, global_binding, game_binding,
     &press_key, &press_action);

    if(press_key || press_action)
    {
      status->joystick_press[joystick][pos].type = type;
      status->joystick_press[joystick][pos].num = num;
      status->joystick_press[joystick][pos].key = press_key;
      status->joystick_press[joystick][pos].action = press_action;
      status->joystick_press_count[joystick]++;

      if(press_action)
        status->joystick_action_status[joystick][press_action] = true;
      if(press_key)
      {
        uint32_t unicode = KEYCODE_IS_ASCII(press_key) ? press_key : 0;
        key_press(status, press_key);
        key_press_unicode(status, unicode, true);
      }
    }
  }

  // Global action press.
  if(global_binding < 0 && (-global_binding < NUM_JOYSTICK_ACTIONS))
  {
    status->joystick_action = -global_binding;
    status->joystick_repeat = -global_binding;
    status->joystick_repeat_id = joystick;
    status->joystick_repeat_state = 1;
    status->joystick_time = get_ticks();
  }
}

/**
 * "Release" a joystick button/axis/hat/etc; fake a key release and/or disable
 * the global action used by UI contexts where appropriate.
 */
static void joystick_release(struct buffered_status *status, int joystick,
 enum joy_press_type type, int num, int16_t global_binding, int16_t game_binding)
{
  int count = status->joystick_press_count[joystick];
  if(count > 0)
  {
    struct joystick_press *p;
    int i;

    for(i = 0; i < count; i++)
    {
      p = &(status->joystick_press[joystick][i]);

      if(p->type == type && p->num == num)
      {
        if(p->action)
          status->joystick_action_status[joystick][p->action] = false;
        if(p->key)
          key_release(status, p->key);

        count--;
        status->joystick_press_count[joystick] = count;
        status->joystick_press[joystick][i] =
         status->joystick_press[joystick][count];
        i--;
      }
    }
  }

  // Global action release.
  if(global_binding < 0 && (-global_binding < NUM_JOYSTICK_ACTIONS))
  {
    if(status->joystick_repeat_id == joystick &&
     (int)status->joystick_repeat == -global_binding)
    {
      status->joystick_repeat = JOY_NO_ACTION;
      status->joystick_repeat_state = 0;
    }
  }
}

/**
 * Press a joystick button. Event handlers should use this function.
 */
void joystick_button_press(struct buffered_status *status,
 int joystick, int button)
{
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS) &&
   (button >= 0) && (button < MAX_JOYSTICK_BUTTONS) &&
   !status->joystick_button[joystick][button])
  {
    int16_t global_binding = input.joystick_global_map.button[joystick][button];
    int16_t game_binding = input.joystick_game_map.button[joystick][button];

    status->joystick_button[joystick][button] = true;
    joystick_press(status, joystick, JOY_BUTTON, button,
     global_binding, game_binding);
  }
}

/**
 * Release a joystick button. Event handlers should use this function.
 */
void joystick_button_release(struct buffered_status *status,
 int joystick, int button)
{
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS) &&
   (button >= 0) && (button < MAX_JOYSTICK_BUTTONS))
  {
    int16_t global_binding = input.joystick_global_map.button[joystick][button];
    int16_t game_binding = input.joystick_game_map.button[joystick][button];

    status->joystick_button[joystick][button] = false;
    joystick_release(status, joystick, JOY_BUTTON, button,
     global_binding, game_binding);
  }
}

/**
 * Update a joystick hat for a given direction. Event handlers should use this
 * function.
 */
void joystick_hat_update(struct buffered_status *status,
 int joystick, enum joystick_hat dir, boolean dir_active)
{
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS) &&
   (dir < NUM_JOYSTICK_HAT_DIRS))
  {
    int16_t global_binding = input.joystick_global_map.hat[joystick][dir];
    int16_t game_binding = input.joystick_game_map.hat[joystick][dir];

    if(dir_active)
    {
      if(!status->joystick_hat[joystick][dir])
        joystick_press(status, joystick, JOY_HAT, dir,
         global_binding, game_binding);
    }
    else
    {
      if(status->joystick_hat[joystick][dir])
        joystick_release(status, joystick, JOY_HAT, dir,
         global_binding, game_binding);
    }

    status->joystick_hat[joystick][dir] = !!dir_active;
  }
}

/**
 * We store analog axis values, but the axis maps are digital. Return
 * the axis map position for an analog axis value or -1 for the dead zone.
 */
static int joystick_axis_to_digital(int16_t value)
{
  if(value > input.joystick_axis_threshold)
    return 1;
  else

  if(value < -input.joystick_axis_threshold)
    return 0;

  return -1;
}

static void joystick_axis_press(struct buffered_status *status,
 int joystick, int axis, int dir)
{
  int16_t global_binding = input.joystick_global_map.axis[joystick][axis][dir];
  int16_t game_binding = input.joystick_game_map.axis[joystick][axis][dir];

  joystick_press(status, joystick, JOY_AXIS, (axis * 2) + dir,
   global_binding, game_binding);
}

static void joystick_axis_release(struct buffered_status *status,
 int joystick, int axis, int dir)
{
  int16_t global_binding = input.joystick_global_map.axis[joystick][axis][dir];
  int16_t game_binding = input.joystick_game_map.axis[joystick][axis][dir];

  joystick_release(status, joystick, JOY_AXIS, (axis * 2) + dir,
   global_binding, game_binding);
}

/**
 * Update a joystick axis. Event handlers should use this function.
 */
void joystick_axis_update(struct buffered_status *status,
 int joystick, int axis, int16_t value)
{
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS) &&
   (axis >= 0) && (axis < MAX_JOYSTICK_AXES))
  {
    int16_t last_value = status->joystick_axis[joystick][axis];
    int last_digital_value = joystick_axis_to_digital(last_value);
    int digital_value = joystick_axis_to_digital(value);

    status->joystick_axis[joystick][axis] = value;

    // Might be a special axis.
    if(input.joystick_global_map.special_axis[joystick][axis])
      joystick_special_axis_update(status, joystick,
       input.joystick_global_map.special_axis[joystick][axis], value);

    if(digital_value != -1)
    {
      if(last_digital_value != digital_value)
        joystick_axis_press(status, joystick, axis, digital_value);

      if(last_digital_value == (digital_value ^ 1))
        joystick_axis_release(status, joystick, axis, last_digital_value);
    }
    else

    if(last_digital_value != -1)
    {
      joystick_axis_release(status, joystick, axis, last_digital_value);
    }
  }
}

/**
 * Update the value of a named axis. This usually corresponds to a regular axis.
 */
void joystick_special_axis_update(struct buffered_status *status,
 int joystick, enum joystick_special_axis axis, int16_t value)
{
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS) &&
   (axis > JOY_NO_AXIS) && (axis < NUM_JOYSTICK_SPECIAL_AXES))
  {
    status->joystick_special_axis_status[joystick][axis] = value;
  }
}

/**
 * Set the current active status for a joystick.
 */
void joystick_set_active(struct buffered_status *status, int joystick,
 boolean active)
{
  if(joystick >= 0 && joystick < MAX_JOYSTICKS)
    status->joystick_active[joystick] = active;
}

/**
 * Release any active inputs for a given joystick and clear other info.
 * Use this function if a joystick is removed.
 */
void joystick_clear(struct buffered_status *status, int joystick)
{
  int count = status->joystick_press_count[joystick];
  int i;

  for(i = 0; i < count; i++)
    key_release(status, status->joystick_press[joystick][i].key);

  status->joystick_active[joystick] = false;
  status->joystick_press_count[joystick] = 0;

  memset(status->joystick_button[joystick], 0,
   sizeof(status->joystick_button[joystick]));

  memset(status->joystick_axis[joystick], 0,
   sizeof(status->joystick_axis[joystick]));

  memset(status->joystick_hat[joystick], 0,
   sizeof(status->joystick_hat[joystick]));

  memset(status->joystick_action_status[joystick], 0,
   sizeof(status->joystick_action_status[joystick]));

  memset(status->joystick_special_axis_status[joystick], 0,
   sizeof(status->joystick_special_axis_status));

  if(status->joystick_repeat_id == joystick)
  {
    status->joystick_action = JOY_NO_ACTION;
    status->joystick_repeat = JOY_NO_ACTION;
    status->joystick_repeat_state = 0;
  }
}

/**
 * Get the current UI joystick action.
 */
enum joystick_action get_joystick_ui_action(void)
{
  const struct buffered_status *status = load_status();
  return status->joystick_action;
}

/**
 * Get the standard UI key for the current joystick action.
 * Most places that use joystick actions want similar bindings or only have
 * a few exceptions.
 */
enum keycode get_joystick_ui_key(void)
{
  const struct buffered_status *status = load_status();
  return joystick_action_map_ui[status->joystick_action];
}

/**
 * Get the active status of a joystick. Returns false if the joystick number
 * is invalid. If the joystick is valid, is_active will be set to the active
 * status and the function will return true.
 */
boolean joystick_is_active(int joystick, boolean *is_active)
{
  const struct buffered_status *status = load_status();
  if((joystick >= 0) && (joystick < MAX_JOYSTICKS))
  {
    *is_active = status->joystick_active[joystick];
    return true;
  }
  return false;
}

/**
 * Get the status of a joystick control by name. Returns false if the joystick
 * number is invalid or if the control is invalid. If the joystick and control
 * are both valid, value will be set to the status value of the control and the
 * function will return true.
 */
boolean joystick_get_status(int joystick, char *name, int16_t *value)
{
  const struct buffered_status *status = load_status();

  if((joystick >= 0) && (joystick < MAX_JOYSTICKS))
  {
    if(!strncasecmp(name, "axis_", 5))
    {
      enum joystick_special_axis axis = joystick_axis_by_name(name + 5);

      if(axis != JOY_NO_AXIS)
      {
        *value = status->joystick_special_axis_status[joystick][axis];
        return true;
      }
    }
    else
    {
      enum joystick_action action = joystick_action_by_name(name);

      if(action != JOY_NO_ACTION)
      {
        *value = status->joystick_action_status[joystick][action];
        return true;
      }
    }
  }
  return false;
}
