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

#ifndef MEGAZEUX_KEYSYM_H
#define MEGAZEUX_KEYSYM_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

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
  IKEY_SCROLLLOCK   = 302,
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

#define XTKEY_CODE(x) ((x) & 0x7f)

enum pcxt_keycode /* and PS/2 */
{
  XTKEY_UNKNOWN       = 0x00,
  XTKEY_ESCAPE        = 0x01,
  XTKEY_1             = 0x02,
  XTKEY_2             = 0x03,
  XTKEY_3             = 0x04,
  XTKEY_4             = 0x05,
  XTKEY_5             = 0x06,
  XTKEY_6             = 0x07,
  XTKEY_7             = 0x08,
  XTKEY_8             = 0x09,
  XTKEY_9             = 0x0a,
  XTKEY_0             = 0x0b,
  XTKEY_MINUS         = 0x0c,
  XTKEY_EQUALS        = 0x0d,
  XTKEY_BACKSPACE     = 0x0e,
  XTKEY_TAB           = 0x0f,
  XTKEY_Q             = 0x10,
  XTKEY_W             = 0x11,
  XTKEY_E             = 0x12,
  XTKEY_R             = 0x13,
  XTKEY_T             = 0x14,
  XTKEY_Y             = 0x15,
  XTKEY_U             = 0x16,
  XTKEY_I             = 0x17,
  XTKEY_O             = 0x18,
  XTKEY_P             = 0x19,
  XTKEY_LEFTBRACKET   = 0x1a,
  XTKEY_RIGHTBRACKET  = 0x1b,
  XTKEY_RETURN        = 0x1c, /* Alternate: KP return */
  XTKEY_LCTRL         = 0x1d, /* Alternate: right control */
  XTKEY_A             = 0x1e,
  XTKEY_S             = 0x1f,
  XTKEY_D             = 0x20,
  XTKEY_F             = 0x21,
  XTKEY_G             = 0x22,
  XTKEY_H             = 0x23,
  XTKEY_J             = 0x24,
  XTKEY_K             = 0x25,
  XTKEY_L             = 0x26,
  XTKEY_SEMICOLON     = 0x27,
  XTKEY_QUOTE         = 0x28,
  XTKEY_BACKQUOTE     = 0x29,
  XTKEY_LSHIFT        = 0x2a,
  XTKEY_BACKSLASH     = 0x2b,
  XTKEY_Z             = 0x2c,
  XTKEY_X             = 0x2d,
  XTKEY_C             = 0x2e,
  XTKEY_V             = 0x2f,
  XTKEY_B             = 0x30,
  XTKEY_N             = 0x31,
  XTKEY_M             = 0x32,
  XTKEY_COMMA         = 0x33,
  XTKEY_PERIOD        = 0x34,
  XTKEY_SLASH         = 0x35, /* Alternate: KP divide */
  XTKEY_RSHIFT        = 0x36,
  XTKEY_KP_MULTIPLY   = 0x37, /* Alternate: print screen */
  XTKEY_LALT          = 0x38, /* Alternate: right alt or AltGr */
  XTKEY_SPACE         = 0x39,
  XTKEY_CAPSLOCK      = 0x3a,
  XTKEY_F1            = 0x3b,
  XTKEY_F2            = 0x3c,
  XTKEY_F3            = 0x3d,
  XTKEY_F4            = 0x3e,
  XTKEY_F5            = 0x3f,
  XTKEY_F6            = 0x40,
  XTKEY_F7            = 0x41,
  XTKEY_F8            = 0x42,
  XTKEY_F9            = 0x43,
  XTKEY_F10           = 0x44,
  XTKEY_NUMLOCK       = 0x45,
  XTKEY_SCROLLLOCK    = 0x46,
  XTKEY_KP_7          = 0x47, /* Alternate: home */
  XTKEY_KP_8          = 0x48, /* Alternate: up */
  XTKEY_KP_9          = 0x49, /* Alternate: page up */
  XTKEY_KP_MINUS      = 0x4a,
  XTKEY_KP_4          = 0x4b, /* Alternate left */
  XTKEY_KP_5          = 0x4c,
  XTKEY_KP_6          = 0x4d, /* Alternate: right */
  XTKEY_KP_PLUS       = 0x4e,
  XTKEY_KP_1          = 0x4f, /* Alternate: end */
  XTKEY_KP_2          = 0x50, /* Alternate: down */
  XTKEY_KP_3          = 0x51, /* Alternate: page down */
  XTKEY_KP_0          = 0x52, /* Alternate: insert */
  XTKEY_KP_PERIOD     = 0x53, /* Alternate: delete */
  XTKEY_F11           = 0x57,
  XTKEY_F12           = 0x58,
  XTKEY_LSUPER        = 0x5b, /* Alternate only */
  XTKEY_RSUPER        = 0x5c, /* Alternate only */
  XTKEY_MENU          = 0x5d, /* Alternate only */
  XTKEY_NUM_CODES     = 0x80,

  XTKEY_BREAK         = 0xc5, /* Not a real PC/XT keycode, but derived from the
                               * PS/2 break sequence E1 1D 45 E1 9D C5
                               * (press LCtrl + Numlock, release LCtrl + Numlock).
                               * This specifically is the release Numlock code. */

  XTKEY_RELEASE       = 0x80, /* PS/2 flag to indicate release. */
  XTKEY_EXTENDED      = 0xe0  /* PS/2 marker indicates alternate key for next code. */
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

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_KEYSYM_H */
