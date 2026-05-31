/* MegaZeux
 *
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __KEYSYM_H
#define __KEYSYM_H

#include "../compat.h"

__M_BEGIN_DECLS

enum keycode
{
  IKEY_UNKNOWN      = 0,
  IKEY_FIRST        = 0,
  IKEY_UNICODE      = 1,
  IKEY_BACKSPACE    = 8,
  IKEY_TAB          = 9,
  IKEY_RETURN       = 13,
  IKEY_ESCAPE       = 27,
  IKEY_SPACE        = 32,
  IKEY_QUOTE        = 39,
//IKEY_PLUS         = 43,
  IKEY_COMMA        = 44,
  IKEY_MINUS        = 45,
  IKEY_PERIOD       = 46,
  IKEY_SLASH        = 47,
  IKEY_0            = 48,
  IKEY_1            = 49,
  IKEY_2            = 50,
  IKEY_3            = 51,
  IKEY_4            = 52,
  IKEY_5            = 53,
  IKEY_6            = 54,
  IKEY_7            = 55,
  IKEY_8            = 56,
  IKEY_9            = 57,
  IKEY_SEMICOLON    = 59,
  IKEY_EQUALS       = 61,
  IKEY_LEFTBRACKET  = 91,
  IKEY_BACKSLASH    = 92,
  IKEY_RIGHTBRACKET = 93,
  IKEY_BACKQUOTE    = 96,
  IKEY_a            = 97,
  IKEY_b            = 98,
  IKEY_c            = 99,
  IKEY_d            = 100,
  IKEY_e            = 101,
  IKEY_f            = 102,
  IKEY_g            = 103,
  IKEY_h            = 104,
  IKEY_i            = 105,
  IKEY_j            = 106,
  IKEY_k            = 107,
  IKEY_l            = 108,
  IKEY_m            = 109,
  IKEY_n            = 110,
  IKEY_o            = 111,
  IKEY_p            = 112,
  IKEY_q            = 113,
  IKEY_r            = 114,
  IKEY_s            = 115,
  IKEY_t            = 116,
  IKEY_u            = 117,
  IKEY_v            = 118,
  IKEY_w            = 119,
  IKEY_x            = 120,
  IKEY_y            = 121,
  IKEY_z            = 122,
  IKEY_DELETE       = 127,
  IKEY_KP0          = 256,
  IKEY_KP1          = 257,
  IKEY_KP2          = 258,
  IKEY_KP3          = 259,
  IKEY_KP4          = 260,
  IKEY_KP5          = 261,
  IKEY_KP6          = 262,
  IKEY_KP7          = 263,
  IKEY_KP8          = 264,
  IKEY_KP9          = 265,
  IKEY_KP_PERIOD    = 266,
  IKEY_KP_DIVIDE    = 267,
  IKEY_KP_MULTIPLY  = 268,
  IKEY_KP_MINUS     = 269,
  IKEY_KP_PLUS      = 270,
  IKEY_KP_ENTER     = 271,
  IKEY_UP           = 273,
  IKEY_DOWN         = 274,
  IKEY_RIGHT        = 275,
  IKEY_LEFT         = 276,
  IKEY_INSERT       = 277,
  IKEY_HOME         = 278,
  IKEY_END          = 279,
  IKEY_PAGEUP       = 280,
  IKEY_PAGEDOWN     = 281,
  IKEY_F1           = 282,
  IKEY_F2           = 283,
  IKEY_F3           = 284,
  IKEY_F4           = 285,
  IKEY_F5           = 286,
  IKEY_F6           = 287,
  IKEY_F7           = 288,
  IKEY_F8           = 289,
  IKEY_F9           = 290,
  IKEY_F10          = 291,
  IKEY_F11          = 292,
  IKEY_F12          = 293,
  IKEY_NUMLOCK      = 300,
  IKEY_CAPSLOCK     = 301,
  IKEY_SCROLLOCK    = 302,
  IKEY_RSHIFT       = 303,
  IKEY_LSHIFT       = 304,
  IKEY_RCTRL        = 305,
  IKEY_LCTRL        = 306,
  IKEY_RALT         = 307,
  IKEY_LALT         = 308,
  IKEY_LSUPER       = 311,
  IKEY_RSUPER       = 312,
  IKEY_ALTGR        = 313,
  IKEY_SYSREQ       = 317,
  IKEY_BREAK        = 318,
  IKEY_MENU         = 319,
  IKEY_LAST
};

#define MOUSE_BUTTON(x) (1 << ((x) - 1))

enum mouse_button
{
  MOUSE_NO_BUTTON,
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_MIDDLE,
  MOUSE_BUTTON_RIGHT,
  MOUSE_BUTTON_X1,
  MOUSE_BUTTON_X2,
  MOUSE_BUTTON_WHEELUP,
  MOUSE_BUTTON_WHEELDOWN,
  MOUSE_BUTTON_WHEELLEFT,
  MOUSE_BUTTON_WHEELRIGHT,
  NUM_MOUSE_BUTTONS
};

enum joystick_action
{
  JOY_NO_ACTION,
  JOY_A,
  JOY_B,
  JOY_X,
  JOY_Y,
  JOY_SELECT,
  //JOY_GUIDE,
  JOY_START,
  JOY_LSTICK,
  JOY_RSTICK,
  JOY_LSHOULDER,
  JOY_RSHOULDER,
  JOY_LTRIGGER,
  JOY_RTRIGGER,
  JOY_UP,
  JOY_DOWN,
  JOY_LEFT,
  JOY_RIGHT,
  JOY_L_UP,
  JOY_L_DOWN,
  JOY_L_LEFT,
  JOY_L_RIGHT,
  JOY_R_UP,
  JOY_R_DOWN,
  JOY_R_LEFT,
  JOY_R_RIGHT,
  NUM_JOYSTICK_ACTIONS
};

enum joystick_special_axis
{
  JOY_NO_AXIS,
  JOY_AXIS_LEFT_X,
  JOY_AXIS_LEFT_Y,
  JOY_AXIS_RIGHT_X,
  JOY_AXIS_RIGHT_Y,
  JOY_AXIS_LEFT_TRIGGER,
  JOY_AXIS_RIGHT_TRIGGER,
  NUM_JOYSTICK_SPECIAL_AXES
};

enum joystick_hat
{
  JOYHAT_UP,
  JOYHAT_DOWN,
  JOYHAT_LEFT,
  JOYHAT_RIGHT,
  NUM_JOYSTICK_HAT_DIRS
};

__M_END_DECLS

#endif // __KEYSYM_H
