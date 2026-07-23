/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
 * Copyright (C) 2019, 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../event.h"
#include "../../configure.h"
#include "../../graphics.h"
#include "../../util.h"
#include "../../platform/platform.h"
#include "../../render/sdl/render_sdl.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef CONFIG_WIIU
#include <whb/proc.h>
#endif

/* Several ports don't or did not send joystick added events:
 *
 * - SDL 1.2 doesn't have joystick added/removed events.
 * - The 3DS port didn't send them until 3.x
 * - The PS2 port didn't send them until 3.x
 * - The PSP port didn't send them until 2.32.0 and 3.x
 * - The Vita port didn't send them until 2.0.22 and 3.x
 * - The unofficial Switch port doesn't bother at all.
 */
#if !SDL_VERSION_ATLEAST(2,0,0) || \
 (!SDL_VERSION_ATLEAST(3,0,0) && defined(CONFIG_3DS)) || \
 (!SDL_VERSION_ATLEAST(3,0,0) && defined(CONFIG_PS2)) || \
 (!SDL_VERSION_ATLEAST(2,32,0) && defined(CONFIG_PSP)) || \
 (!SDL_VERSION_ATLEAST(2,0,22) && defined(CONFIG_PSVITA)) || \
 defined(CONFIG_SWITCH)
#define NO_JOYSTICK_ADDED_EVENTS
#endif

extern struct input_status input;

/* Enable converting keycodes to fake unicode presses when text input isn't
 * active. Enabling text input also enables an onscreen keyboard in some
 * ports, so it isn't always desired.
 *
 * SDL 1.2 builds will disable this automatically if unicode is detected.
 */
static boolean unicode_fallback;

static boolean numlock_status_initialized;
static unsigned last_text_window_id;
static int joystick_instance_ids[MAX_JOYSTICKS];
static SDL_Joystick *joysticks[MAX_JOYSTICKS];

static enum keycode convert_SDL_internal(SDL_Keycode key)
{
  switch(key)
  {
    case SDLK_BACKSPACE: return IKEY_BACKSPACE;
    case SDLK_TAB: return IKEY_TAB;
    case SDLK_RETURN: return IKEY_RETURN;
    case SDLK_ESCAPE: return IKEY_ESCAPE;
    case SDLK_SPACE: return IKEY_SPACE;
    case SDLK_APOSTROPHE: return IKEY_QUOTE;
    case SDLK_PLUS: return IKEY_EQUALS;
    case SDLK_COMMA: return IKEY_COMMA;
    case SDLK_MINUS: return IKEY_MINUS;
    case SDLK_PERIOD: return IKEY_PERIOD;
    case SDLK_SLASH: return IKEY_SLASH;
    case SDLK_0: return IKEY_0;
    case SDLK_1: return IKEY_1;
    case SDLK_2: return IKEY_2;
    case SDLK_3: return IKEY_3;
    case SDLK_4: return IKEY_4;
    case SDLK_5: return IKEY_5;
    case SDLK_6: return IKEY_6;
    case SDLK_7: return IKEY_7;
    case SDLK_8: return IKEY_8;
    case SDLK_9: return IKEY_9;
    case SDLK_SEMICOLON: return IKEY_SEMICOLON;
    case SDLK_EQUALS: return IKEY_EQUALS;
    case SDLK_LEFTBRACKET: return IKEY_LEFTBRACKET;
    case SDLK_BACKSLASH: return IKEY_BACKSLASH;
    case SDLK_RIGHTBRACKET: return IKEY_RIGHTBRACKET;
    case SDLK_GRAVE: return IKEY_BACKQUOTE;
    case SDLK_A: return IKEY_a;
    case SDLK_B: return IKEY_b;
    case SDLK_C: return IKEY_c;
    case SDLK_D: return IKEY_d;
    case SDLK_E: return IKEY_e;
    case SDLK_F: return IKEY_f;
    case SDLK_G: return IKEY_g;
    case SDLK_H: return IKEY_h;
    case SDLK_I: return IKEY_i;
    case SDLK_J: return IKEY_j;
    case SDLK_K: return IKEY_k;
    case SDLK_L: return IKEY_l;
    case SDLK_M: return IKEY_m;
    case SDLK_N: return IKEY_n;
    case SDLK_O: return IKEY_o;
    case SDLK_P: return IKEY_p;
    case SDLK_Q: return IKEY_q;
    case SDLK_R: return IKEY_r;
    case SDLK_S: return IKEY_s;
    case SDLK_T: return IKEY_t;
    case SDLK_U: return IKEY_u;
    case SDLK_V: return IKEY_v;
    case SDLK_W: return IKEY_w;
    case SDLK_X: return IKEY_x;
    case SDLK_Y: return IKEY_y;
    case SDLK_Z: return IKEY_z;
    case SDLK_DELETE: return IKEY_DELETE;
    case SDLK_KP_0: return IKEY_KP0;
    case SDLK_KP_1: return IKEY_KP1;
    case SDLK_KP_2: return IKEY_KP2;
    case SDLK_KP_3: return IKEY_KP3;
    case SDLK_KP_4: return IKEY_KP4;
    case SDLK_KP_5: return IKEY_KP5;
    case SDLK_KP_6: return IKEY_KP6;
    case SDLK_KP_7: return IKEY_KP7;
    case SDLK_KP_8: return IKEY_KP8;
    case SDLK_KP_9: return IKEY_KP9;
    case SDLK_KP_PERIOD: return IKEY_KP_PERIOD;
    case SDLK_KP_DIVIDE: return IKEY_KP_DIVIDE;
    case SDLK_KP_MULTIPLY: return IKEY_KP_MULTIPLY;
    case SDLK_KP_MINUS: return IKEY_KP_MINUS;
    case SDLK_KP_PLUS: return IKEY_KP_PLUS;
    case SDLK_KP_ENTER: return IKEY_KP_ENTER;
    case SDLK_UP: return IKEY_UP;
    case SDLK_DOWN: return IKEY_DOWN;
    case SDLK_RIGHT: return IKEY_RIGHT;
    case SDLK_LEFT: return IKEY_LEFT;
    case SDLK_INSERT: return IKEY_INSERT;
    case SDLK_HOME: return IKEY_HOME;
    case SDLK_END: return IKEY_END;
    case SDLK_PAGEUP: return IKEY_PAGEUP;
    case SDLK_PAGEDOWN: return IKEY_PAGEDOWN;
    case SDLK_F1: return IKEY_F1;
    case SDLK_F2: return IKEY_F2;
    case SDLK_F3: return IKEY_F3;
    case SDLK_F4: return IKEY_F4;
    case SDLK_F5: return IKEY_F5;
    case SDLK_F6: return IKEY_F6;
    case SDLK_F7: return IKEY_F7;
    case SDLK_F8: return IKEY_F8;
    case SDLK_F9: return IKEY_F9;
    case SDLK_F10: return IKEY_F10;
    case SDLK_F11: return IKEY_F11;
    case SDLK_F12: return IKEY_F12;
    case SDLK_NUMLOCKCLEAR: return IKEY_NUMLOCK;
    case SDLK_CAPSLOCK: return IKEY_CAPSLOCK;
    case SDLK_SCROLLLOCK: return IKEY_SCROLLOCK;
    case SDLK_RSHIFT: return IKEY_RSHIFT;
    case SDLK_LSHIFT: return IKEY_LSHIFT;
    case SDLK_RCTRL: return IKEY_RCTRL;
    case SDLK_LCTRL: return IKEY_LCTRL;
    case SDLK_RALT: return IKEY_RALT;
    case SDLK_LALT: return IKEY_LALT;
    case SDLK_MODE: return IKEY_ALTGR;
#if !SDL_VERSION_ATLEAST(2,0,0)
    // SDL 1.2 had two different versions of the same pair of keys.
    // Because of this, we can't just #define these to the new values.
    case SDLK_LMETA: return IKEY_LSUPER;
    case SDLK_RMETA: return IKEY_RSUPER;
    case SDLK_LSUPER: return IKEY_LSUPER;
    case SDLK_RSUPER: return IKEY_RSUPER;
#else
    case SDLK_LGUI: return IKEY_LSUPER;
    case SDLK_RGUI: return IKEY_RSUPER;
#endif
    case SDLK_SYSREQ: return IKEY_SYSREQ;
    case SDLK_PAUSE: return IKEY_BREAK;
    case SDLK_MENU: return IKEY_MENU;
#if SDL_VERSION_ATLEAST(2,0,0)
    // SDL 2.0 leaves the old keysyms around but doesn't use them?
    case SDLK_PRINTSCREEN: return IKEY_SYSREQ;
    case SDLK_APPLICATION: return IKEY_MENU;
#endif
#ifdef __WIN32__
#if SDL_VERSION_ATLEAST(2,0,6) && !SDL_VERSION_ATLEAST(2,0,10)
    // Dumb hack for a Windows virtual keycode bug. TODO remove.
    case SDLK_CLEAR: return IKEY_KP5;
#endif
#endif
    default: return IKEY_UNKNOWN;
  }
}

static enum mouse_button convert_SDL_mouse_internal(uint32_t button)
{
  switch(button)
  {
    case SDL_BUTTON_LEFT:   return MOUSE_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE: return MOUSE_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT:  return MOUSE_BUTTON_RIGHT;

#if SDL_VERSION_ATLEAST(2,0,0)
    case SDL_BUTTON_X1:     return MOUSE_BUTTON_X1;
    case SDL_BUTTON_X2:     return MOUSE_BUTTON_X2;

    // Some SDL 2 versions have a bug where these values are carried through
    // from X11.
#ifdef CONFIG_X11
    case 8:                 return MOUSE_BUTTON_X1;
    case 9:                 return MOUSE_BUTTON_X2;
#endif /* CONFIG_X11 */

#else /* !SDL_VERSION_ATLEAST(2,0,0) */

    // SDL 1.2 didn't get the defines until 1.2.13.
    // Also, the wheel is handled here, and X11 does its own thing.
#ifndef CONFIG_X11
    case 4: return MOUSE_BUTTON_WHEELUP;
    case 5: return MOUSE_BUTTON_WHEELDOWN;
    case 6: return MOUSE_BUTTON_X1;
    case 7: return MOUSE_BUTTON_X2;
    case 8: return MOUSE_BUTTON_WHEELLEFT;
    case 9: return MOUSE_BUTTON_WHEELRIGHT;
#else /* CONFIG_X11 */
    case 4: return MOUSE_BUTTON_WHEELUP;
    case 5: return MOUSE_BUTTON_WHEELDOWN;
    case 6: return MOUSE_BUTTON_WHEELLEFT;
    case 7: return MOUSE_BUTTON_WHEELRIGHT;
    case 8: return MOUSE_BUTTON_X1;
    case 9: return MOUSE_BUTTON_X2;
#endif

#endif /* !SDL_VERSION_ATLEAST(2,0,0) */
  }
  return MOUSE_NO_BUTTON;
}

#ifdef CONFIG_PANDORA
static int get_pandora_joystick_button(SDL_Keycode key)
{
  // Pandora hack. The home, end, page up, page down, right shift,
  // and right control keys are actually joystick buttons.
  switch(key)
  {
    case SDLK_HOME:     return 0;
    case SDLK_END:      return 1;
    case SDLK_PAGEUP:   return 2;
    case SDLK_PAGEDOWN: return 3;
    case SDLK_RSHIFT:   return 4;
    case SDLK_RCTRL:    return 5;
  }
  return -1;
}
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
/**
 * The SDL gamepad API puts this in an interesting place. The best way
 * to populate MZX action mappings is to parse the SDL mapping string directly.
 * The API offers no other way to get detailed mapping information on half axes
 * and axis inversions (these were added after the API was designed; no new
 * functions were added to read this extended info), and using gamepad
 * events to simulate presses could result in situations where the joystick
 * events and the gamepad events create different simultaneous presses.
 *
 * However, the axis mappings can be so convoluted it's better to open the
 * controller with the API anyway just to populate the analog axis values. This
 * appears to not affect the joystick events received whatsoever.
 *
 * No equivalent of this API exists for SDL 1.x.
 */

static SDL_Gamepad *gamepads[MAX_JOYSTICKS];

static enum joystick_special_axis sdl_axis_map[SDL_GAMEPAD_AXIS_COUNT] =
{
  [SDL_GAMEPAD_AXIS_LEFTX]         = JOY_AXIS_LEFT_X,
  [SDL_GAMEPAD_AXIS_LEFTY]         = JOY_AXIS_LEFT_Y,
  [SDL_GAMEPAD_AXIS_RIGHTX]        = JOY_AXIS_RIGHT_X,
  [SDL_GAMEPAD_AXIS_RIGHTY]        = JOY_AXIS_RIGHT_Y,
  [SDL_GAMEPAD_AXIS_LEFT_TRIGGER]  = JOY_AXIS_LEFT_TRIGGER,
  [SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = JOY_AXIS_RIGHT_TRIGGER
};

static int16_t sdl_axis_action_map[SDL_GAMEPAD_AXIS_COUNT][2] =
{
  [SDL_GAMEPAD_AXIS_LEFTX]         = { -JOY_L_LEFT,  -JOY_L_RIGHT },
  [SDL_GAMEPAD_AXIS_LEFTY]         = { -JOY_L_UP,    -JOY_L_DOWN },
  [SDL_GAMEPAD_AXIS_RIGHTX]        = { -JOY_R_LEFT,  -JOY_R_RIGHT },
  [SDL_GAMEPAD_AXIS_RIGHTY]        = { -JOY_R_UP,    -JOY_R_DOWN },
  [SDL_GAMEPAD_AXIS_LEFT_TRIGGER]  = { 0,            -JOY_LTRIGGER },
  [SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = { 0,            -JOY_RTRIGGER },
};

static int16_t sdl_action_map[SDL_GAMEPAD_BUTTON_COUNT] =
{
  [SDL_GAMEPAD_BUTTON_SOUTH]          = -JOY_A,
  [SDL_GAMEPAD_BUTTON_EAST]           = -JOY_B,
  [SDL_GAMEPAD_BUTTON_WEST]           = -JOY_X,
  [SDL_GAMEPAD_BUTTON_NORTH]          = -JOY_Y,
  [SDL_GAMEPAD_BUTTON_BACK]           = -JOY_SELECT,
//[SDL_GAMEPAD_BUTTON_GUIDE]          = -JOY_GUIDE,
  [SDL_GAMEPAD_BUTTON_START]          = -JOY_START,
  [SDL_GAMEPAD_BUTTON_LEFT_STICK]     = -JOY_LSTICK,
  [SDL_GAMEPAD_BUTTON_RIGHT_STICK]    = -JOY_RSTICK,
  [SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]  = -JOY_LSHOULDER,
  [SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = -JOY_RSHOULDER,
  [SDL_GAMEPAD_BUTTON_DPAD_UP]        = -JOY_UP,
  [SDL_GAMEPAD_BUTTON_DPAD_DOWN]      = -JOY_DOWN,
  [SDL_GAMEPAD_BUTTON_DPAD_LEFT]      = -JOY_LEFT,
  [SDL_GAMEPAD_BUTTON_DPAD_RIGHT]     = -JOY_RIGHT
};

enum
{
  GAMEPAD_NONE,
  GAMEPAD_BUTTON,
  GAMEPAD_AXIS,
  GAMEPAD_HAT,
};

struct gamepad_map
{
  char *dbg;
  uint8_t feature;
  uint8_t which;
  uint8_t pos;
};

struct gamepad_axis_map
{
  struct gamepad_map neg;
  struct gamepad_map pos;
};

static int sdl_hat_to_dir(int hat_mask)
{
  switch(hat_mask)
  {
    case SDL_HAT_UP:
      return JOYHAT_UP;

    case SDL_HAT_DOWN:
      return JOYHAT_DOWN;

    case SDL_HAT_LEFT:
      return JOYHAT_LEFT;

    case SDL_HAT_RIGHT:
      return JOYHAT_RIGHT;

    default:
      return -1;
  }
}

static void parse_gamepad_read_value(char *key, char *value,
 struct gamepad_map *single, struct gamepad_map *neg, struct gamepad_map *pos)
{
  // Joystick axes may have half-axis prefixes + or -.
  // Joystick axes may also have an inversion suffix ~.
  char half_axis = 0;

  if(*value == '+' || *value == '-')
    half_axis = *(value++);

  switch(*value)
  {
    case 'a':
    {
      // Axis- a# or a#~
      unsigned int axis;

      if(!isdigit((uint8_t)value[1]))
        break;

      axis = strtoul(value + 1, &value, 10);
      if(axis >= MAX_JOYSTICK_AXES)
        break;

      // Only one provided output and no half axis specified? Map + to it.
      if(half_axis == 0 && single)
      {
        neg = NULL;
        pos = single;
      }
      else

      if(half_axis == '+')
      {
        pos = single ? single : pos ? pos : neg;
      }
      else

      if(half_axis == '-')
      {
        neg = single ? single : neg ? neg : pos;
      }

      if(value && *value == '~')
      {
        // Invert
        single = neg;
        neg = pos;
        pos = single;
      }

      if(neg)
      {
        neg->feature = GAMEPAD_AXIS;
        neg->which = axis;
        neg->pos = 0;
        neg->dbg = key;
      }
      if(pos)
      {
        pos->feature = GAMEPAD_AXIS;
        pos->which = axis;
        pos->pos = 1;
        pos->dbg = key;
      }
      return;
    }

    case 'b':
    {
      // Button- b#
      unsigned int button;

      if(!isdigit((uint8_t)value[1]))
        break;

      button = strtoul(value + 1, NULL, 10);
      if(button >= MAX_JOYSTICK_BUTTONS)
        break;

      if(!single)
        single = pos ? pos : neg;

      if(single)
      {
        single->feature = GAMEPAD_BUTTON;
        single->which = button;
        single->dbg = key;
      }
      return;
    }

    case 'h':
    {
      // Hat- h#.#
      unsigned int hat;
      unsigned int hat_mask;
      int dir;

      if(!isdigit((uint8_t)value[1]))
        break;

      hat = strtoul(value + 1, &value, 10);
      if(hat != 0 || !value[0] || !isdigit((uint8_t)value[1]))
        break;

      hat_mask = strtoul(value + 1, NULL, 10);
      dir = sdl_hat_to_dir(hat_mask);
      if(dir < 0)
        break;

      if(!single)
        single = pos ? pos : neg;

      if(single)
      {
        single->feature = GAMEPAD_HAT;
        single->which = hat;
        single->pos = dir;
        single->dbg = key;
      }
      return;
    }
  }
  debug("--JOYSTICK-- ignoring '%s' -> '%s'\n", value, key);
  return;
}

static void parse_gamepad_read_entry(char *key, char *value,
 struct gamepad_axis_map *axes, struct gamepad_map *buttons)
{
  SDL_GamepadAxis a;
  SDL_GamepadButton b;
  struct gamepad_map *single = NULL;
  struct gamepad_map *neg = NULL;
  struct gamepad_map *pos = NULL;
  char half_axis = 0;

  // Gamepad axes may have half-axis prefixes + or -.
  if(*key == '+' || *key == '-')
    half_axis = *(key++);

  a = SDL_GetGamepadAxisFromString(key);
  b = SDL_GetGamepadButtonFromString(key);
  if(a != SDL_GAMEPAD_AXIS_INVALID)
  {
    if(half_axis == '+')
      single = &(axes[a].pos);

    if(half_axis == '-')
      single = &(axes[a].neg);

    if(half_axis == 0)
    {
      neg = &(axes[a].neg);
      pos = &(axes[a].pos);
    }
  }
  else

  if(b != SDL_GAMEPAD_BUTTON_INVALID)
  {
    // This button isn't really useful to MZX.
    if(b == SDL_GAMEPAD_BUTTON_GUIDE)
      return;

    single = &(buttons[b]);
  }
  else

  if(!strcasecmp(key, "platform") || !strcasecmp(key, "crc"))
  {
    // ignore- field used by SDL.
    return;
  }

  else
  {
    debug("--JOYSTICK-- Invalid control '%s'! Report this!\n", key);
    return;
  }

  parse_gamepad_read_value(key, value, single, neg, pos);
}

static void parse_gamepad_read_string(char *map,
 struct gamepad_axis_map *axes, struct gamepad_map *buttons)
{
  // Format: entry,entry,...
  // Entry:  value or key:value
  char *end = map + strlen(map);
  char *key;
  char *value;

  while(map < end)
  {
    key = map;
    value = NULL;

    while(*map != ',')
    {
      if(!*map) break;

      if(*map == ':' && !value)
      {
        *map = 0;
        value = map + 1;
      }

      map++;
    }
    *(map++) = 0;

    if(value)
      parse_gamepad_read_entry(key, value, axes, buttons);
  }
}

static void parse_gamepad_apply(int joy, int16_t mapping,
 struct gamepad_map *target, boolean *select_mapped, boolean *select_used)
{
  uint8_t which = target->which;
  uint8_t pos = target->pos;

  if(mapping == -JOY_SELECT || mapping == -JOY_START)
    *select_mapped = true;

  switch(target->feature)
  {
    case GAMEPAD_NONE:
      return;

    case GAMEPAD_BUTTON:
    {
      debug("--JOYSTICK--  b%u -> '%s' (%d)\n", which, target->dbg, mapping);
      if(!input.joystick_global_map.button_is_conf[joy][which])
        input.joystick_global_map.button[joy][which] = mapping;

      if(!input.joystick_game_map.button_is_conf[joy][which])
        input.joystick_game_map.button[joy][which] = mapping;
      break;
    }

    case GAMEPAD_AXIS:
    {
      debug("--JOYSTICK--  a%u%s -> '%s' (%d)\n", which, pos?"+":"-",
       target->dbg, mapping);

      if(!input.joystick_global_map.axis_is_conf[joy][which])
        input.joystick_global_map.axis[joy][which][pos] = mapping;

      if(!input.joystick_game_map.axis_is_conf[joy][which])
        input.joystick_game_map.axis[joy][which][pos] = mapping;
      break;
    }

    case GAMEPAD_HAT:
    {
      debug("--JOYSTICK--  hd%u -> '%s' (%d)\n", pos, target->dbg, mapping);
      if(!input.joystick_global_map.hat_is_conf[joy])
        input.joystick_global_map.hat[joy][pos] = mapping;

      if(!input.joystick_game_map.hat_is_conf[joy])
        input.joystick_game_map.hat[joy][pos] = mapping;
      break;
    }
  }
  if(mapping == -JOY_SELECT || mapping == -JOY_START)
    *select_used = true;
  return;
}

static void parse_gamepad_map(int joystick_index, char *map)
{
  struct gamepad_axis_map axes[SDL_GAMEPAD_AXIS_COUNT];
  struct gamepad_map buttons[SDL_GAMEPAD_BUTTON_COUNT];
  boolean select_mapped = false;
  boolean select_used = false;
  size_t i;

  memset(axes, 0, sizeof(axes));
  memset(buttons, 0, sizeof(buttons));

  parse_gamepad_read_string(map, axes, buttons);

  // Apply axes.
  for(i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
  {
    parse_gamepad_apply(joystick_index,
     sdl_axis_action_map[i][0], &(axes[i].neg), &select_mapped, &select_used);

    parse_gamepad_apply(joystick_index,
     sdl_axis_action_map[i][1], &(axes[i].pos), &select_mapped, &select_used);
  }

  // Apply buttons.
  for(i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
  {
    parse_gamepad_apply(joystick_index,
     sdl_action_map[i], &(buttons[i]), &select_mapped, &select_used);
  }

  if(select_mapped && !select_used)
  {
    // TODO originally this was going to try to place JOY_SELECT on another
    // button. That was kind of a bad idea, so just print a warning for now.
    info("--JOYSTICK-- %d doesn't have any gamepad button that binds "
     "'select' or 'start' (by default, these are the SDL gamecontoller buttons "
     "'back' and 'start', respectively). Since these buttons are used to open "
     "the joystick menu, you may want to override this controller mapping.\n",
      joystick_index + 1);
  }
}

/**
 * `sdl_joystick_id` is a device ID for SDL1/SDL2, but an instance ID for SDL3.
 */
static void init_gamepad(SDL_Joystick *joystick, int sdl_joystick_id,
 int joystick_index)
{
  SDL_GUID guid = SDL_GetJoystickGUID(joystick);
  char guid_string[33];

  SDL_GUIDToString(guid, guid_string, 33);
  gamepads[joystick_index] = NULL;

  if(SDL_IsGamepad(sdl_joystick_id))
  {
    SDL_Gamepad *gamepad = SDL_OpenGamepad(sdl_joystick_id);

    if(gamepad)
    {
      const struct config_info *conf = get_config();
      char *mapping = NULL;
      gamepads[joystick_index] = gamepad;

#if SDL_VERSION_ATLEAST(3,0,0)
      // This is the equivalent to [...]ForDeviceIndex() and is also currently
      // the only way to get the default generated mapping.
      mapping = (char *)SDL_GetGamepadMappingForID(sdl_joystick_id);
#elif SDL_VERSION_ATLEAST(2,0,9)
      // NOTE: the other functions for this will not return the default mapping
      // string; this is the only one that can return everything. Right now,
      // this only matters for the Emscripten port.
      mapping = (char *)SDL_GameControllerMappingForDeviceIndex(sdl_joystick_id);
#else
      mapping = (char *)SDL_GameControllerMapping(gamepad);
#endif

#ifdef __EMSCRIPTEN__
      if(!mapping)
      {
        // The only function that can return the default mapping was added
        // in SDL 2.0.9, but I'm not sure Emscripten actually has SDL 2.0.9.
        // This string is copied from SDL_gamecontrollerdb.h
        static const char default_mapping[] =
         "default,Standard Gamepad,a:b0,b:b1,back:b8,dpdown:b13,dpleft:b14,"
         "dpright:b15,dpup:b12,guide:b16,leftshoulder:b4,leftstick:b10,"
         "lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,"
         "righttrigger:b7,rightx:a2,righty:a3,start:b9,x:b2,y:b3,";

        mapping = SDL_malloc(sizeof(default_mapping));
        if(mapping)
          strcpy(mapping, default_mapping);
      }
#endif

      if(mapping)
      {
        info("--JOYSTICK-- joystick %d has an SDL mapping: %s\n",
         joystick_index + 1, mapping);

        if(strncmp(mapping, guid_string, strlen(guid_string)))
          info("--JOYSTICK-- GUID: %s\n", guid_string);

        if(conf->allow_gamepad)
          parse_gamepad_map(joystick_index, mapping);

        SDL_free(mapping);
        return;
      }
    }
  }
  info("--JOYSTICK-- joystick %d does not have an SDL mapping or could not be "
   "opened as a gamepad (GUID: %s).\n", joystick_index + 1, guid_string);
}

// Clean up auto-generated bindings so they don't cause problems for other
// controllers that might end up using this position.
static void gamepad_clean_map(int joy)
{
  int i;

  for(i = 0; i < MAX_JOYSTICK_AXES; i++)
  {
    if(!input.joystick_global_map.axis_is_conf[joy][i])
    {
      input.joystick_global_map.axis[joy][i][0] = 0;
      input.joystick_global_map.axis[joy][i][1] = 0;
    }

    if(!input.joystick_game_map.axis_is_conf[joy][i])
    {
      input.joystick_game_map.axis[joy][i][0] = 0;
      input.joystick_game_map.axis[joy][i][1] = 0;
    }
  }

  for(i = 0; i < MAX_JOYSTICK_BUTTONS; i++)
  {
    if(!input.joystick_global_map.button_is_conf[joy][i])
      input.joystick_global_map.button[joy][i] = 0;

    if(!input.joystick_game_map.button_is_conf[joy][i])
      input.joystick_game_map.button[joy][i] = 0;
  }

  if(!input.joystick_global_map.hat_is_conf[joy])
  {
    for(i = 0; i < NUM_JOYSTICK_HAT_DIRS; i++)
      input.joystick_global_map.hat[joy][i] = 0;
  }

  if(!input.joystick_game_map.hat_is_conf[joy])
  {
    for(i = 0; i < NUM_JOYSTICK_HAT_DIRS; i++)
      input.joystick_game_map.hat[joy][i] = 0;
  }
}

/**
 * Load gamecontrollerdb.txt if configured and if it isn't already loaded.
 * This adds more gamepad mappings so MZX can support more controllers.
 * The function this uses wasn't added until 2.0.2.
 */
static void load_gamecontrollerdb(void)
{
#if defined(CONFIG_GAMECONTROLLERDB) && SDL_VERSION_ATLEAST(2,0,2)
  static boolean gamecontrollerdb_loaded = false;

  if(!gamecontrollerdb_loaded)
  {
    const char *path = mzx_res_get_by_id(GAMECONTROLLERDB_TXT);

    if(path)
    {
      int result = SDL_AddGamepadMappingsFromFile(path);
      if(result >= 0)
        debug("--JOYSTICK-- Added %d mappings from '%s'.\n", result, path);
    }

    gamecontrollerdb_loaded = true;
  }
#endif
}

/**
 * Change one of the default SDL to MZX mapping values.
 */
void gamepad_map_sym(const char *sym, const char *value)
{
  SDL_GamepadAxis a;
  SDL_GamepadButton b;
  int16_t binding = 0;

  if(joystick_parse_map_value(value, &binding))
  {
    char dir = 0;
    if(*sym == '+' || *sym == '-')
      dir = *(sym++);

    // Digital axis (default to + if no dir specified).
    a = SDL_GetGamepadAxisFromString(sym);
    if(a != SDL_GAMEPAD_AXIS_INVALID)
    {
      int pos = (dir != '-') ? 1 : 0;
      sdl_axis_action_map[a][pos] = binding;
    }

    // Button
    b = SDL_GetGamepadButtonFromString(sym);
    if(b != SDL_GAMEPAD_BUTTON_INVALID)
      sdl_action_map[b] = binding;
  }

  // TODO analog axes
}

/**
 * Add a mapping string to SDL.
 */
void gamepad_add_mapping(const char *mapping)
{
  // Make sure this is loaded first so it doesn't override the user mapping.
  load_gamecontrollerdb();

  if(SDL_AddGamepadMapping(mapping) < 0)
    warn("Failed to add gamepad mapping: %s\n", SDL_GetError());
}

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

/**
 * SDL 2+ uses joystick instance IDs instead of the device ID (index) for all
 * purposes (except for initialization in SDL 2, but not SDL 3). These instance
 * IDs need to be mapped to MegaZeux joystick indices. For SDL 1.2, the SDL
 * device ID is used as a substitute for the instance ID.
 */
static int get_joystick_index(int sdl_instance_id)
{
  int i;
  for(i = 0; i < MAX_JOYSTICKS; i++)
    if((joystick_instance_ids[i] == sdl_instance_id) && joysticks[i])
      return i;

  return -1;
}

static int get_next_unused_joystick_index(void)
{
  int i;
  for(i = 0; i < MAX_JOYSTICKS; i++)
    if(!joysticks[i])
      return i;

  return -1;
}

/**
 * `sdl_joystick_id` is a device ID for SDL1/SDL2, but an instance ID for SDL3.
 */
static void init_joystick(int sdl_joystick_id)
{
  struct buffered_status *status = store_status();
  int joystick_index = get_next_unused_joystick_index();

  if(joystick_index >= 0)
  {
    SDL_Joystick *joystick = SDL_OpenJoystick(sdl_joystick_id);
    if(joystick)
    {
      joystick_instance_ids[joystick_index] = SDL_GetJoystickID(joystick);
      joysticks[joystick_index] = joystick;
      joystick_set_active(status, joystick_index, true);

      debug("--JOYSTICK-- Opened %d (SDL instance ID: %d)\n",
       joystick_index + 1, joystick_instance_ids[joystick_index]);

#if SDL_VERSION_ATLEAST(2,0,0)
      init_gamepad(joystick, sdl_joystick_id, joystick_index);
#endif
    }
  }
}

#if SDL_VERSION_ATLEAST(2,0,0)
// TODO: swappable joysticks in SDL <2
static void close_joystick(int joystick_index)
{
  if(joystick_index >= 0)
  {
    debug("--JOYSTICK-- Closing %d (SDL instance ID: %d)\n",
     joystick_index + 1, joystick_instance_ids[joystick_index]);

    // SDL_GameControllerClose also closes the joystick.
    if(gamepads[joystick_index])
    {
      SDL_CloseGamepad(gamepads[joystick_index]);
      gamepad_clean_map(joystick_index);
      gamepads[joystick_index] = NULL;
    }
    else
      SDL_CloseJoystick(joysticks[joystick_index]);

    joystick_instance_ids[joystick_index] = -1;
    joysticks[joystick_index] = NULL;
  }
}
#endif

static inline void read_key_event(int *key, int *mod, int *scancode,
 const SDL_KeyboardEvent *ev)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  *key = ev->key;
  *mod = ev->mod;
  *scancode = ev->scancode;
#else
  *key = ev->keysym.sym;
  *mod = ev->keysym.mod;
  *scancode = ev->keysym.scancode;
#endif
}

#if SDL_VERSION_ATLEAST(2,0,0)
static inline uint32_t utf8_next_char(uint8_t **_src)
{
  uint8_t *src = *_src;
  uint32_t unicode;

  if(!*src)
    return 0;

  unicode = *(src++);

  if(unicode & 0x80)
  {
    uint32_t extra = 1;
    uint32_t next;
    uint32_t i;

    if(!(unicode & 0x40))
      goto err_invalid;

    unicode = unicode & ~0xC0;
    if(unicode & 0x20)
    {
      unicode = unicode & ~0x20;
      extra++;
      if(unicode & 0x10)
      {
        unicode = unicode & ~0x10;
        extra++;
      }
    }

    for(i = 0; i < extra; i++)
    {
      if(!*src)
        goto err_invalid;

      next = *src++;
      if((next & 0xC0) != 0x80)
        goto err_invalid;

      unicode = (unicode << 6) | (next & 0x3F);
    }
  }
  *_src = src;
  return unicode;

err_invalid:
  *_src = src;
  return 0;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

static boolean process_event(SDL_Event *event)
{
  struct buffered_status *status = store_status();
  int key, mod, scancode;
  enum keycode ckey;

  /* SDL's numlock keyboard modifier handling seems to be broken on X11,
   * and it will only get numlock's status right on application init. We
   * can trust this value once, and then toggle based on user presses of
   * the numlock key.
   *
   * On Windows, KEYDOWN/KEYUP seem to be sent separately, to indicate
   * enabling or disabling of numlock. But on X11, both KEYDOWN/KEYUP are
   * sent for each toggle, so this must be handled differently.
   *
   * What a mess!
   */
  if(!numlock_status_initialized)
  {
    status->numlock_status = !!(SDL_GetModState() & SDL_KMOD_NUM);
    numlock_status_initialized = true;
  }

  switch(event->type)
  {
    case SDL_EVENT_QUIT:
    {
      trace("--EVENT_SDL-- SDL_EVENT_QUIT\n");
      // Set the exit status
      status->exit_status = true;
      break;
    }

#if SDL_VERSION_ATLEAST(3,0,0)
    case SDL_EVENT_WINDOW_RESIZED:
    {
      unsigned window_id = video_window_by_platform_id(event->window.windowID);

      trace("--EVENT_SDL-- SDL_EVENT_WINDOW_RESIZED: %u (%d,%d)\n",
       event->window.windowID, event->window.data1, event->window.data2);
      video_sync_window_size(window_id,
       event->window.data1, event->window.data2);
      break;
    }

    case SDL_EVENT_WINDOW_FOCUS_LOST:
    {
      trace("--EVENT_SDL-- SDL_EVENT_WINDOW_FOCUS_LOST\n");
      // Pause while minimized
      if(input.unfocus_pause)
      {
        while(1)
        {
          SDL_WaitEvent(event);
          if(event->type == SDL_EVENT_WINDOW_FOCUS_GAINED)
            break;
        }
        trace("--EVENT_SDL-- SDL_WINDOWEVENT_FOCUS_GAINED\n");
      }
      break;
    }
#elif SDL_VERSION_ATLEAST(2,0,0)
    case SDL_WINDOWEVENT:
    {
      Uint32 sdl_window_id = event->window.windowID;
      switch(event->window.event)
      {
        case SDL_WINDOWEVENT_RESIZED:
        {
          unsigned window_id = video_window_by_platform_id(sdl_window_id);
          trace(
            "--EVENT_SDL-- SDL_WINDOWEVENT_RESIZED: %u (%u,%u)\n",
            sdl_window_id, event->window.data1, event->window.data2
          );
          video_sync_window_size(window_id,
           event->window.data1, event->window.data2);
          break;
        }

        case SDL_WINDOWEVENT_FOCUS_LOST:
        {
          trace("--EVENT_SDL-- SDL_WINDOWEVENT_FOCUS_LOST: %u\n", sdl_window_id);
          // Pause while minimized
          if(input.unfocus_pause)
          {
            while(1)
            {
              SDL_WaitEvent(event);

              if(event->type == SDL_WINDOWEVENT &&
                 event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                break;
            }
            trace("--EVENT_SDL-- SDL_WINDOWEVENT_FOCUS_GAINED: %u\n", sdl_window_id);
          }
          break;
        }
      }
      break;
    }
#else // !SDL_VERSION_ATLEAST(2,0,0)
    case SDL_VIDEORESIZE:
    {
      trace(
        "--EVENT_SDL-- SDL_VIDEORESIZE: (%u,%u)\n",
        event->resize.w, event->resize.h
      );
      if(get_config()->allow_resize)
        video_resize_window(1, event->resize.w, event->resize.h);
      break;
    }

    case SDL_ACTIVEEVENT:
    {
      trace("--EVENT_SDL-- SDL_ACTIVEEVENT: %u\n", event->active.state);
      if(input.unfocus_pause)
      {
        // Pause while minimized
        if(event->active.state & (SDL_APPACTIVE | SDL_APPINPUTFOCUS))
        {
          // Wait for SDL_APPACTIVE with gain of 1
          do
          {
            SDL_WaitEvent(event);
          } while((event->type != SDL_ACTIVEEVENT) ||
           (event->active.state & ~(SDL_APPACTIVE | SDL_APPINPUTFOCUS)));
        }
      }
      break;
    }
#endif // !SDL_VERSION_ATLEAST(2,0,0)

    case SDL_EVENT_MOUSE_MOTION:
    {
      SDL_Window *window = sdl_get_current_window();
      int mx_real = event->motion.x;
      int my_real = event->motion.y;
      int mx, my, min_x, min_y, max_x, max_y;
      get_screen_coords(mx_real, my_real, &mx, &my, &min_x,
       &min_y, &max_x, &max_y);

      if(mx >= SCREEN_PIX_W)
      {
        SDL_WarpMouseInWindow(window, max_x, my_real);
        mx = SCREEN_PIX_W - 1;
      }

      if(mx < 0)
      {
        SDL_WarpMouseInWindow(window, min_x, my_real);
        mx = 0;
      }

      if(my >= SCREEN_PIX_H)
      {
        SDL_WarpMouseInWindow(window, mx_real, max_y);
        my = SCREEN_PIX_H - 1;
      }

      if(my < 0)
      {
        SDL_WarpMouseInWindow(window, mx_real, min_y);
        my = 0;
      }

      trace(
        "--EVENT_SDL-- SDL_EVENT_MOUSE_MOTION: (%d,%d) -> (%d,%d)\n",
        mx_real, my_real, mx, my
      );
      status->mouse_pixel_x = mx;
      status->mouse_pixel_y = my;
      status->mouse_x = mx / CHAR_W;
      status->mouse_y = my / CHAR_H;
      status->mouse_moved = true;
      break;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
      enum mouse_button button = convert_SDL_mouse_internal(event->button.button);
      trace("--EVENT_SDL-- SDL_EVENT_MOUSE_BUTTON_DOWN: %u\n", event->button.button);
      if(button == MOUSE_NO_BUTTON)
        break;
      status->mouse_button = button;
      status->mouse_repeat = button;
      status->mouse_button_state |= MOUSE_BUTTON(button);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      break;
    }

    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
      enum mouse_button button = convert_SDL_mouse_internal(event->button.button);
      trace("--EVENT_SDL-- SDL_EVENT_MOUSE_BUTTON_UP: %u\n", event->button.button);
      if(button == MOUSE_NO_BUTTON)
        break;
      status->mouse_button_state &= ~MOUSE_BUTTON(button);
      status->mouse_repeat = MOUSE_NO_BUTTON;
      status->mouse_drag_state = 0;
      status->mouse_repeat_state = 0;
      break;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    // emulate the X11-style "wheel is a button" that SDL 1.2 used
    // SDL3 uses floats, SDL2 uses ints.
    case SDL_EVENT_MOUSE_WHEEL:
    {
      uint32_t button;
      float wheel_x = (float)event->wheel.x;
      float wheel_y = (float)event->wheel.y;

      trace(
        "--EVENT_SDL-- SDL_EVENT_MOUSE_WHEEL: x=%.2f y=%.2f\n",
        wheel_x, wheel_y
      );

      if(fabsf(wheel_x) > fabsf(wheel_y))
      {
        if(wheel_x < 0.0f)
          button = MOUSE_BUTTON_WHEELLEFT;
        else
          button = MOUSE_BUTTON_WHEELRIGHT;
      }
      else
      {
        if(wheel_y < 0.0f)
          button = MOUSE_BUTTON_WHEELDOWN;
        else
          button = MOUSE_BUTTON_WHEELUP;
      }

      // Wheel "presses" are immediately "released", and don't affect the state
      // bitmask. Just set the current mouse button and clear everything else.
      status->mouse_button = button;
      status->mouse_repeat = MOUSE_NO_BUTTON;
      status->mouse_repeat_state = 0;
      status->mouse_drag_state = 0;
      status->mouse_time = get_ticks();
      break;
    }

    /* Finger events--mostly just debug for now. */
    case SDL_EVENT_FINGER_MOTION:
    {
      trace(
        "--EVENT_SDL-- SDL_EVENT_FINGER_MOTION: id:%ld w:%u "
        "x:%.2f y:%.2f dx:%.2f dy:%.2f p:%.2f\n",
        (long)TFINGER_TOUCH_ID(event->tfinger),
        TFINGER_WINDOW_ID(event->tfinger),
        event->tfinger.x, event->tfinger.y,
        event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure
      );
      break;
    }

    case SDL_EVENT_FINGER_DOWN:
    {
      trace(
        "--EVENT_SDL-- SDL_EVENT_FINGER_DOWN: id:%ld w:%u "
        "x:%.2f y:%.2f dx:%.2f dy:%.2f p:%.2f\n",
        (long)TFINGER_TOUCH_ID(event->tfinger),
        TFINGER_WINDOW_ID(event->tfinger),
        event->tfinger.x, event->tfinger.y,
        event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure
      );
#ifdef __ANDROID__
      {
        /* There may be no physical keyboard or gamepad present. While this
         * configuration is useless, this screen keyboard toggle gesture at
         * least allows opening the main menu and exiting from the title. */
        int count = 0;
        SDL_TouchID tid = TFINGER_TOUCH_ID(event->tfinger);
        SDL_Finger **fingers = SDL_GetTouchFingers(tid, &count);
        if(fingers)
        {
          trace("--EVENT_SDL-- queried %d active finger(s)\n", count);
          if(count == 3)
            screen_keyboard_toggle_state();
          SDL_free(fingers);
        }
        else
          debug("--EVENT_SDL-- couldn't query finger(s): %s\n", SDL_GetError());
      }
#endif
      break;
    }

    case SDL_EVENT_FINGER_UP:
    {
      trace(
        "--EVENT_SDL-- SDL_EVENT_FINGER_UP: id:%ld w:%u "
        "x:%.2f y:%.2f dx:%.2f dy:%.2f p:%.2f\n",
        (long)TFINGER_TOUCH_ID(event->tfinger),
        TFINGER_WINDOW_ID(event->tfinger),
        event->tfinger.x, event->tfinger.y,
        event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure
      );
      break;
    }
#endif // SDL_VERSION_ATLEAST(2,0,0)

    case SDL_EVENT_KEY_DOWN:
    {
      uint32_t unicode = 0;
      read_key_event(&key, &mod, &scancode, &event->key);

#if SDL_VERSION_ATLEAST(2,0,0)
      // SDL 2.0 uses proper key repeat, but derives its timing from the OS.
      // Our hand-rolled key repeat is more friendly to use than SDL 2.0's
      // (with Windows, at least) and is consistent across all platforms.

      // To enable SDL 2.0 key repeat, remove this check/break and see the
      // update_autorepeat() function in event.c

      if(event->key.repeat)
        break;
#endif

#ifdef CONFIG_PANDORA
      {
        // Pandora hack. Certain keys are actually joystick buttons.
        int button = get_pandora_joystick_button(key);
        if(button >= 0)
        {
          joystick_button_press(status, 0, button);
          break;
        }
      }
#endif

      ckey = convert_SDL_internal(key);
      trace(
        "--EVENT_SDL-- SDL_EVENT_KEY_DOWN: scancode:%d sym:%xh mod:%xh -> %d\n",
        scancode, key, mod, ckey
      );
      if(!ckey)
      {
#if !SDL_VERSION_ATLEAST(2,0,0)
        ckey = IKEY_UNICODE;
        if(!event->key.keysym.unicode)
#endif
          break;
      }

#if !SDL_VERSION_ATLEAST(2,0,0)
      unicode = event->key.keysym.unicode;
      if(unicode && unicode_fallback)
      {
        // Clear any unicode keys on the buffer generated from the fallback...
        status->unicode_length = 0;
        unicode_fallback = false;
      }
#endif

#ifdef CONFIG_PSVITA
      // Vita SDL keyboard will send an enter press immediately following
      // text input, which screws up several things. Suppress this:
      if(ckey == IKEY_RETURN && has_unicode_input())
        break;
#endif

      // Some platforms don't implement SDL_TEXTINPUT (SDL 2.0) or the unicode
      // field (SDL 1.2); until usage of those is detected, fake the text key
      // using the internal keycode.
      if(unicode_fallback && KEYCODE_IS_ASCII(ckey))
      {
        boolean caps_lock = !!(SDL_GetModState() & SDL_KMOD_CAPS);
        unicode = convert_internal_unicode(ckey, caps_lock);
      }
      if(unicode)
        trace("--EVENT_SDL--                     unicode:%d\n", (int)unicode);

      if((ckey == IKEY_RETURN) &&
       get_alt_status(keycode_internal) &&
       get_ctrl_status(keycode_internal))
      {
        video_toggle_fullscreen();
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        status->caps_status = true;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#if !SDL_VERSION_ATLEAST(2,0,0) && defined(__WIN32__)
        status->numlock_status = true;
#endif
        break;
      }

#ifdef CONFIG_ENABLE_SCREENSHOTS
      if(ckey == IKEY_F12 && enable_f12_hack)
      {
        dump_screen();
        break;
      }
#endif

      // Ignore alt + tab
      if((ckey == IKEY_TAB) && get_alt_status(keycode_internal))
      {
        break;
      }

      if(status->key_repeat &&
       (status->key_repeat != IKEY_LSHIFT) &&
       (status->key_repeat != IKEY_RSHIFT) &&
       (status->key_repeat != IKEY_LSUPER) &&
       (status->key_repeat != IKEY_RSUPER) &&
       (status->key_repeat != IKEY_LALT) &&
       (status->key_repeat != IKEY_RALT) &&
       (status->key_repeat != IKEY_ALTGR) &&
       (status->key_repeat != IKEY_LCTRL) &&
       (status->key_repeat != IKEY_RCTRL))
      {
        // Stack current repeat key if it isn't shift, super, alt, or ctrl
        if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
        {
          input.key_repeat_stack[input.repeat_stack_pointer] =
           status->key_repeat;
          input.unicode_repeat_stack[input.repeat_stack_pointer] =
           status->unicode_repeat;
          input.repeat_stack_pointer++;
        }
      }

      key_press(status, ckey);
      key_press_unicode(status, unicode, true);
      break;
    }

    case SDL_EVENT_KEY_UP:
    {
      read_key_event(&key, &mod, &scancode, &event->key);

#ifdef CONFIG_PANDORA
      {
        // Pandora hack. Certain keys are actually joystick buttons.
        int button = get_pandora_joystick_button(key);
        if(button >= 0)
        {
          joystick_button_release(status, 0, button);
          break;
        }
      }
#endif

      ckey = convert_SDL_internal(key);
      trace(
        "--EVENT_SDL-- SDL_EVENT_KEY_UP: scancode:%d sym:%xh mod:%xh -> %d\n",
        scancode, key, mod, ckey
      );
      if(!ckey)
      {
#if !SDL_VERSION_ATLEAST(2,0,0)
        ckey = IKEY_UNICODE;
        if(!status->keymap[IKEY_UNICODE])
#endif
          break;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#if !SDL_VERSION_ATLEAST(2,0,0) && defined(__WIN32__)
        status->numlock_status = false;
#else
        status->numlock_status = !status->numlock_status;
#endif
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        status->caps_status = false;
      }
      key_release(status, ckey);
      break;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    /**
     * SDL 2 sends repeat key press events. In the case of SDL_TEXTINPUT, these
     * can't be distinguished from regular key presses, so key_press_unicode
     * needs to be called without repeating enabled.
     */
    case SDL_EVENT_TEXT_INPUT:
    {
      uint8_t *text = (uint8_t *)event->text.text;

      trace("--EVENT_SDL-- SDL_EVENT_TEXT_INPUT: %s\n", text);

      if(unicode_fallback)
      {
        // Clear any unicode keys on the buffer generated from the fallback...
        status->unicode_length = 0;
        unicode_fallback = false;
      }

      // Decode the input UTF-8 string into UTF-32 for the event buffer.
      while(*text)
      {
        uint32_t unicode = utf8_next_char(&text);

        if(unicode)
          key_press_unicode(status, unicode, false);
      }
      break;
    }

    case SDL_EVENT_JOYSTICK_ADDED:
    {
      // Add a new joystick.
      // "which" for this event (but not for any other joystick event) is not
      // a joystick instance ID, but instead an index for SDL_JoystickOpen().

      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_ADDED\n"
      );

      init_joystick(event->jdevice.which);
      break;
    }

    case SDL_EVENT_JOYSTICK_REMOVED:
    {
      // Close a disconnected joystick.
      int which = event->jdevice.which;
      int joystick_index = get_joystick_index(which);

      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_REMOVED: j%d\n",
        joystick_index
      );

      close_joystick(joystick_index);

      // Joysticks can be trivially disconnected while holding a button and
      // the corresponding release event will never be sent for it. Release
      // all of this joystick's inputs.
      joystick_clear(status, joystick_index);
      break;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
      // Since gamepad axis mappings can be complicated, use
      // the gamepad events to update the named axis values.
      const struct config_info *conf = get_config();
#if SDL_VERSION_ATLEAST(3,0,0)
      int value = event->gaxis.value;
      int which = event->gaxis.which;
      int axis = event->gaxis.axis;
#else
      int value = event->caxis.value;
      int which = event->caxis.which;
      int axis = event->caxis.axis;
#endif
      enum joystick_special_axis special_axis = sdl_axis_map[axis];

      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0 || !special_axis || !conf->allow_gamepad)
        break;

#if 0
      /* May be sent almost nonstop, especially for Windows for some reason. */
      trace(
        "--EVENT_SDL-- SDL_EVENT_GAMEPAD_AXIS_MOTION: j%d sa%d = %d\n",
        joystick_index, special_axis, value
      );
#endif

      joystick_special_axis_update(status, joystick_index, special_axis, value);
      break;
    }
#endif

    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    {
      int axis_value = event->jaxis.value;
      int which = event->jaxis.which;
      int axis = event->jaxis.axis;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

#if 0
      /* May be sent almost nonstop, especially for Windows for some reason. */
      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_AXIS_MOTION: j%d a%d = %d\n",
        joystick_index, axis, axis_value
      );
#endif

      joystick_axis_update(status, joystick_index, axis, axis_value);
      break;
    }

    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_BUTTON_DOWN: j%d b%d\n",
        joystick_index, button
      );

#ifdef CONFIG_SWITCH
      // Ignore fake axis "buttons".
      if((button >= 16) && (button <= 23))
        break;
#endif

      joystick_button_press(status, joystick_index, button);
      break;
    }

    case SDL_EVENT_JOYSTICK_BUTTON_UP:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

#ifdef CONFIG_SWITCH
      // Ignore fake axis "buttons".
      if((button >= 16) && (button <= 23))
        break;
#endif

      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_BUTTON_UP: j%d b%d\n",
        joystick_index, button
      );

      joystick_button_release(status, joystick_index, button);
      break;
    }

    case SDL_EVENT_JOYSTICK_HAT_MOTION:
    {
      int which = event->jhat.which;
      int dir = event->jhat.value;
      boolean hat_u = (dir & SDL_HAT_UP) ? true : false;
      boolean hat_d = (dir & SDL_HAT_DOWN) ? true : false;
      boolean hat_l = (dir & SDL_HAT_LEFT) ? true : false;
      boolean hat_r = (dir & SDL_HAT_RIGHT) ? true : false;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

      trace(
        "--EVENT_SDL-- SDL_EVENT_JOYSTICK_HAT_MOTION: "
        "j%d up:%u down:%u left:%u right:%u\n",
        joystick_index, hat_u, hat_d, hat_l, hat_r
      );

      joystick_hat_update(status, joystick_index, JOYHAT_UP, hat_u);
      joystick_hat_update(status, joystick_index, JOYHAT_DOWN, hat_d);
      joystick_hat_update(status, joystick_index, JOYHAT_LEFT, hat_l);
      joystick_hat_update(status, joystick_index, JOYHAT_RIGHT, hat_r);
      break;
    }

    default:
      trace("--EVENT_SDL-- unhandled event %u\n", event->type);
      return false;
  }

  return true;
}

boolean __update_event_status(void)
{
  boolean rval = false;
  SDL_Event event;

#ifdef CONFIG_WIIU
  WHBProcIsRunning();
#endif

  while(SDL_PollEvent(&event))
    rval |= process_event(&event);

#if 0
  // This one is a little annoying even for trace logging...
  trace("--EVENT_SDL-- __update_event_status -> %u\n", rval);
#endif

#if !SDL_VERSION_ATLEAST(2,0,0)
  {
    // ALT+F4 - will not trigger an exit event, so set the variable manually.
    struct buffered_status *status = store_status();

    if(status->key_repeat == IKEY_F4 && get_alt_status(keycode_internal))
    {
      status->key = IKEY_UNKNOWN;
      status->key_repeat = IKEY_UNKNOWN;
      status->unicode_repeat = 0;
      status->unicode_length = 0;
      status->exit_status = true;
      return true;
    }
  }
#endif /*SDL_VERSION_ATLEAST*/

  return rval;
}

// This returns whether the input buffer _may_ contain a request to quit.
// Proper polling should be performed if the answer is yes.
boolean __peek_exit_input(void)
{
  SDL_Event events[256];
  int num_events;
  int key, mod, scancode;
  int i;

  SDL_PumpEvents();

#if SDL_VERSION_ATLEAST(2,0,0)
  num_events =
   SDL_PeepEvents(events, 256, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
#else /* !SDL_VERSION_ATLEAST(2,0,0) */
  num_events = SDL_PeepEvents(events, 256, SDL_PEEKEVENT, SDL_ALLEVENTS);
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

  for(i = 0; i < num_events; i++)
  {
    if(events[i].type == SDL_EVENT_QUIT)
      return true;

    if(events[i].type == SDL_EVENT_KEY_DOWN)
    {
      read_key_event(&key, &mod, &scancode, &(events[i].key));

      if(key == SDLK_ESCAPE)
        return true;

      if(key == SDLK_C && (mod & SDL_KMOD_CTRL))
        return true;

      if(key == SDLK_F4 && (mod & SDL_KMOD_ALT))
        return true;
    }
  }
  return false;
}

void __wait_event(void)
{
  /**
   * This function used to take a timeout param, but we really don't want to
   * implement that on a per-platform basis. SDL_WaitEventTimeout also does a
   * very cool SDL_Delay(10) which we can probably live without.
   */
  SDL_Event event;
  boolean any_event;

  // FIXME: WaitEvent with MSVC hangs the render cycle, so this is, hopefully,
  //        a short-term fix.
#ifdef MSVC_H
  any_event = SDL_PollEvent(&event);
#else
  any_event = SDL_WaitEvent(&event);
#endif

  if(any_event)
    process_event(&event);
}

void __warp_mouse(int x, int y)
{
  SDL_Window *window = sdl_get_current_window();

  if((x < 0) || (y < 0))
  {
#if SDL_VERSION_ATLEAST(3,0,0)
    float current_x, current_y;
#else
    int current_x, current_y;
#endif
    SDL_GetMouseState(&current_x, &current_y);

    if(x < 0)
      x = current_x;

    if(y < 0)
      y = current_y;
  }

  SDL_WarpMouseInWindow(window, x, y);
}

/**
 * Enable text events for the provided window.
 * Prior to SDL3, this was global, so the window ID isn't used.
 */
void sdl_init_window_text_events(unsigned sdl_window_id)
{
#if SDL_VERSION_ATLEAST(2,28,0)
  /* This SDL version supports hiding the screen keyboard, so it's safe to
   * enable text events and defer to MegaZeux's screen keyboard toggle. */
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
  if(window)
    last_text_window_id = sdl_window_id;

  SDL_StopTextInput(window);
  SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD,
   input.showing_screen_keyboard ? "1" : "0");
  SDL_StartTextInput(window);

#elif SDL_VERSION_ATLEAST(2,0,0)
  /* Hack: since this version of SDL doesn't support forcibly hiding the
   * screen keyboard, if a screen keyboard is supported, do not enable text
   * events and use the fallback instead. This is required on Android. */
  if(!SDL_HasScreenKeyboardSupport())
  {
    SDL_StartTextInput();
  }
  else
  {
    SDL_StopTextInput();
    unicode_fallback = true;
  }
#else
  SDL_EnableUNICODE(1);
#endif
}

boolean platform_has_screen_keyboard(void)
{
  return SDL_HasScreenKeyboardSupport();
}

boolean platform_show_screen_keyboard(void)
{
  trace("--EVENT_SDL-- requesting screen keyboard\n");
  input.showing_screen_keyboard = true;
  sdl_init_window_text_events(last_text_window_id);
  return true;
}

boolean platform_hide_screen_keyboard(void)
{
  trace("--EVENT_SDL-- hiding screen keyboard\n");
  input.showing_screen_keyboard = false;
  sdl_init_window_text_events(last_text_window_id);
  return true;
}

boolean platform_is_screen_keyboard_active(void)
{
  SDL_Window *window = SDL_GetWindowFromID(last_text_window_id);
  (void)window; /* SDL2 */
  return SDL_ScreenKeyboardShown(window);
}

void platform_init_event(void)
{
#ifdef NO_JOYSTICK_ADDED_EVENTS
  int i, count;

#if SDL_VERSION_ATLEAST(3,0,0)
  SDL_JoystickID *instance_ids = SDL_GetJoysticks(&count);
  if(instance_ids)
  {
    if(count > MAX_JOYSTICKS)
      count = MAX_JOYSTICKS;

    for(i = 0; i < count; i++)
      init_joystick(instance_ids[i]);

    SDL_free(instance_ids);
  }
#else
  count = SDL_NumJoysticks();

  if(count > MAX_JOYSTICKS)
    count = MAX_JOYSTICKS;

  for(i = 0; i < count; i++)
    init_joystick(i);
#endif
#endif /* NO_JOYSTICK_ADDED_EVENTS */

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_SetGamepadEventsEnabled(true);
  load_gamecontrollerdb();
#endif

  SDL_SetJoystickEventsEnabled(true);

#if SDL_VERSION_ATLEAST(3,0,0)
  /* Disable a couple of events that are unused by MZX. */
  SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE, false);
  SDL_SetEventEnabled(SDL_EVENT_GAMEPAD_UPDATE_COMPLETE, false);
#endif

  /* It's not clear which ports do and don't implement SDL text events, so
   * enable the unicode fallback at startup until proven otherwise.
   * SDL 1.2 might also need this (Pandora? doesn't generate unicode presses).
   * If it isn't required, real unicode events will turn this off. */
  unicode_fallback = true;
}
