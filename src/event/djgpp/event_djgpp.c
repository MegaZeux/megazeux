/* MegaZeux
*
* Copyright (C) 1996 Alexis Janson
* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#define delay delay_dos
#include <bios.h>
#include <dpmi.h>
#include <sys/segments.h>
#undef delay
#include "../event.h"
#include "../../graphics.h"
#include "../../platform/platform.h"
#include "../../platform/djgpp/interrupt.h"

extern struct input_status input;

static enum
{
  KBD_RELEASED,
  KBD_PRESSING,
  KBD_PRESSED
} kbd_statmap[0x80] = {0};
static uint16_t kbd_unicode[0x80] = {0};

static int read_kbd(void)
{
  int ret;
  if(kbd_read == kbd_write)
    return -1;
  ret = kbd_buffer[kbd_read++];
  return ret;
}

static const enum keycode xt_to_internal[0x80] =
{
  // 0x
  IKEY_UNKNOWN, IKEY_ESCAPE, IKEY_1, IKEY_2,
  IKEY_3, IKEY_4, IKEY_5, IKEY_6,
  IKEY_7, IKEY_8, IKEY_9, IKEY_0,
  IKEY_MINUS, IKEY_EQUALS, IKEY_BACKSPACE, IKEY_TAB,
  // 1x
  IKEY_q, IKEY_w, IKEY_e, IKEY_r,
  IKEY_t, IKEY_y, IKEY_u, IKEY_i,
  IKEY_o, IKEY_p, IKEY_LEFTBRACKET, IKEY_RIGHTBRACKET,
  IKEY_RETURN, IKEY_RCTRL, IKEY_a, IKEY_s,
  // 2x
  IKEY_d, IKEY_f, IKEY_g, IKEY_h,
  IKEY_j, IKEY_k, IKEY_l, IKEY_SEMICOLON,
  IKEY_QUOTE, IKEY_BACKQUOTE, IKEY_LSHIFT, IKEY_BACKSLASH,
  IKEY_z, IKEY_x, IKEY_c, IKEY_v,
  // 3x
  IKEY_b, IKEY_n, IKEY_m, IKEY_COMMA,
  IKEY_PERIOD, IKEY_SLASH, IKEY_RSHIFT, IKEY_KP_MULTIPLY,
  IKEY_RALT, IKEY_SPACE, IKEY_CAPSLOCK, IKEY_F1,
  IKEY_F2, IKEY_F3, IKEY_F4, IKEY_F5,
  // 4x
  IKEY_F6, IKEY_F7, IKEY_F8, IKEY_F9,
  IKEY_F10, IKEY_NUMLOCK, IKEY_SCROLLOCK, IKEY_KP7,
  IKEY_KP8, IKEY_KP9, IKEY_KP_MINUS, IKEY_KP4,
  IKEY_KP5, IKEY_KP6, IKEY_KP_PLUS, IKEY_KP1,
  // 5x
  IKEY_KP2, IKEY_KP3, IKEY_KP0, IKEY_KP_PERIOD,
  IKEY_SYSREQ, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_F11,
  IKEY_F12, IKEY_UNKNOWN
};

static const enum keycode extended_xt_to_internal[0x80] =
{
  // 0x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 1x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_KP_ENTER, IKEY_LCTRL, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 2x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 3x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_KP_DIVIDE, IKEY_UNKNOWN, IKEY_SYSREQ,
  IKEY_LALT, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 4x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_BREAK, IKEY_HOME,
  IKEY_UP, IKEY_PAGEUP, IKEY_UNKNOWN, IKEY_LEFT,
  IKEY_UNKNOWN, IKEY_RIGHT, IKEY_UNKNOWN, IKEY_END,
  // 5x
  IKEY_DOWN, IKEY_PAGEDOWN, IKEY_INSERT, IKEY_DELETE,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_LSUPER,
  IKEY_RSUPER, IKEY_MENU, IKEY_UNKNOWN
};

static enum keycode convert_ext_internal(uint8_t key)
{
  if(key & 0x80)
    return extended_xt_to_internal[key & 0x7F];
  else
    return xt_to_internal[key];
}

static int extbioskey(int cmd)
{
  __dpmi_regs reg;
  if(cmd < 0 || cmd >= 3)
    return -1;
  reg.h.ah = cmd | 0x10;
  __dpmi_int(0x16, &reg);
  switch(cmd)
  {
    default:
    case 0:
      return reg.x.ax;
    case 1:
      if(reg.x.flags & 0x40)
        return 0;
      else
        return reg.x.ax;
    case 2:
      return reg.h.al;
  }
}

static void update_lock_status(struct buffered_status *status)
{
  unsigned short res;
  res = extbioskey(2);
  status->numlock_status = !!(res & 0x20);
  status->caps_status = !!(res & 0x40);
}

static uint8_t convert_bios_xt(uint8_t key)
{
  switch(key)
  {
    case 0x54: return 0x3B; // Shift-F1
    case 0x55: return 0x3C; // Shift-F2
    case 0x56: return 0x3D; // Shift-F3
    case 0x57: return 0x3E; // Shift-F4
    case 0x58: return 0x3F; // Shift-F5
    case 0x59: return 0x40; // Shift-F6
    case 0x5A: return 0x41; // Shift-F7
    case 0x5B: return 0x42; // Shift-F8
    case 0x5C: return 0x43; // Shift-F9
    case 0x5D: return 0x44; // Shift-F10
    case 0x5E: return 0x3B; // Ctrl-F1
    case 0x5F: return 0x3C; // Ctrl-F2
    case 0x60: return 0x3D; // Ctrl-F3
    case 0x61: return 0x3E; // Ctrl-F4
    case 0x62: return 0x3F; // Ctrl-F5
    case 0x63: return 0x40; // Ctrl-F6
    case 0x64: return 0x41; // Ctrl-F7
    case 0x65: return 0x42; // Ctrl-F8
    case 0x66: return 0x43; // Ctrl-F9
    case 0x67: return 0x44; // Ctrl-F10
    case 0x68: return 0x3B; // Alt-F1
    case 0x69: return 0x3C; // Alt-F2
    case 0x6A: return 0x3D; // Alt-F3
    case 0x6B: return 0x3E; // Alt-F4
    case 0x6C: return 0x3F; // Alt-F5
    case 0x6D: return 0x40; // Alt-F6
    case 0x6E: return 0x41; // Alt-F7
    case 0x6F: return 0x42; // Alt-F8
    case 0x70: return 0x43; // Alt-F9
    case 0x71: return 0x44; // Alt-F10
    case 0x72: return 0x37; // Ctrl-PrtSc
    case 0x73: return 0x4B; // Ctrl-Left
    case 0x74: return 0x4D; // Ctrl-Right
    case 0x75: return 0x4F; // Ctrl-End
    case 0x76: return 0x51; // Ctrl-PgDn
    case 0x77: return 0x47; // Ctrl-Home
    case 0x78: return 0x02; // Alt-1
    case 0x79: return 0x03; // Alt-2
    case 0x7A: return 0x04; // Alt-3
    case 0x7B: return 0x05; // Alt-4
    case 0x7C: return 0x06; // Alt-5
    case 0x7D: return 0x07; // Alt-6
    case 0x7E: return 0x08; // Alt-7
    case 0x7F: return 0x09; // Alt-8
    case 0x80: return 0x0A; // Alt-9
    case 0x81: return 0x0B; // Alt-0
    case 0x82: return 0x0C; // Alt--
    case 0x83: return 0x0D; // Alt-=
    case 0x84: return 0x49; // Ctrl-PgUp
    // Extended keycodes
    case 0x85: return 0x57; // F11
    case 0x86: return 0x58; // F12
    case 0x87: return 0x57; // Shift-F11
    case 0x88: return 0x58; // Shift-F12
    case 0x89: return 0x57; // Ctrl-F11
    case 0x8A: return 0x58; // Ctrl-F12
    case 0x8B: return 0x57; // Alt-F11
    case 0x8C: return 0x58; // Alt-F12
    case 0x8D: return 0x48; // Ctrl-KP-8 (Up)
    case 0x8E: return 0x4A; // Ctrl-KP--
    case 0x8F: return 0x4C; // Ctrl-KP-5
    case 0x90: return 0x4E; // Ctrl-KP-+
    case 0x91: return 0x50; // Ctrl-KP-2 (Down)
    case 0x92: return 0x52; // Ctrl-KP-0 (Insert)
    case 0x93: return 0x53; // Ctrl-KP-. (Delete)
    case 0x94: return 0x0F; // Ctrl-Tab
    case 0x95: return 0x35; // Ctrl-KP-/
    case 0x96: return 0x37; // Ctrl-KP-*
    case 0x97: return 0x47; // Alt-Home
    case 0x98: return 0x48; // Alt-Up
    case 0x99: return 0x49; // Alt-PgUp
    case 0x9B: return 0x4B; // Alt-Left
    case 0x9D: return 0x4D; // Alt-Right
    case 0x9F: return 0x4F; // Alt-End
    case 0xA0: return 0x50; // Alt-Down
    case 0xA1: return 0x51; // Alt-PgDn
    case 0xA2: return 0x52; // Alt-Insert
    case 0xA3: return 0x53; // Alt-Delete
    case 0xA4: return 0x35; // Alt-KP-/
    case 0xA5: return 0x0F; // Alt-Tab
    case 0xA6: return 0x1C; // Alt-KP-Enter
    default: return key & 0x7F;
  }
}

static void poll_keyboard_bios(void)
{
  unsigned short res;
  uint8_t scancode;
  uint16_t unicode;

  while(extbioskey(1))
  {
    res = extbioskey(0);
    scancode = convert_bios_xt(res >> 8);
    unicode = res & 0xFF;
    kbd_unicode[scancode] = unicode;
    if(kbd_statmap[scancode] == KBD_RELEASED)
      kbd_statmap[scancode] = KBD_PRESSING;
  }
}

static uint16_t convert_ext_unicode(uint8_t key)
{
  poll_keyboard_bios();
  return kbd_unicode[key & 0x7F];
}

static int get_keystat(int key)
{
  return kbd_statmap[key & 0x7F];
}

static void set_keystat(int key, int stat)
{
  kbd_statmap[key & 0x7F] = stat;
}

static boolean non_bios_key(uint8_t key)
{
  switch(key)
  {
    case 0x1D: // Right Ctrl
    case 0x9D: // Left Ctrl
    case 0x38: // Right Alt
    case 0xB8: // Left Alt
    case 0x2A: // Left Shift
    case 0x36: // Right Shift
    case 0xDB: // Left Super (Windows)
    case 0xDC: // Right Super (Windows)
    case 0xDD: // Menu
      return true;
    // BIOS can't return keycodes >= 0x54 due to Alt/Ctrl keycode modification
    // shenanigans, except for F11/F12 which are given alternate keycodes
    case 0x57:
    case 0x58:
      return false;
    default:
      if((key & 0x7F) >= 0x54)
        return true;
      else
        return false;
  }
}

static boolean process_keypress(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_ext_internal(key);
  uint16_t unicode = convert_ext_unicode(key);

  if((get_keystat(key) != KBD_PRESSING) && !non_bios_key(key))
    return false;
  set_keystat(key, KBD_PRESSED);

  if(!ikey)
  {
    if(unicode)
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  if(status->keymap[ikey])
    return false;

  if((ikey == IKEY_CAPSLOCK) || (ikey == IKEY_NUMLOCK))
    update_lock_status(status);

  if((ikey == IKEY_RETURN) &&
   get_alt_status(keycode_internal) &&
   get_ctrl_status(keycode_internal))
  {
    video_toggle_fullscreen();
    return true;
  }

#ifdef CONFIG_ENABLE_SCREENSHOTS
  if(ikey == IKEY_F12)
  {
    dump_screen();
    return true;
  }
#endif

  if(status->key_repeat &&
   (status->key_repeat != IKEY_LSHIFT) &&
   (status->key_repeat != IKEY_RSHIFT) &&
   (status->key_repeat != IKEY_LALT) &&
   (status->key_repeat != IKEY_RALT) &&
   (status->key_repeat != IKEY_LCTRL) &&
   (status->key_repeat != IKEY_RCTRL))
  {
    // Stack current repeat key if it isn't shift, alt, or ctrl
    if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
    {
      input.key_repeat_stack[input.repeat_stack_pointer] =
       status->key_repeat;
      input.unicode_repeat_stack[input.repeat_stack_pointer] =
       status->unicode_repeat;
      input.repeat_stack_pointer++;
    }
  }

  key_press(status, ikey);
  key_press_unicode(status, unicode, true);
  return true;
}

static boolean process_keyrelease(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_ext_internal(key);

  if((get_keystat(key) != KBD_PRESSED) && !non_bios_key(key))
    return false;
  set_keystat(key, KBD_RELEASED);

  if(!ikey)
  {
    if(status->keymap[IKEY_UNICODE])
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  status->keymap[ikey] = 0;
  if(status->key_repeat == ikey)
  {
    status->key_repeat = IKEY_UNKNOWN;
    status->unicode_repeat = 0;
  }
  status->key_release = ikey;
  return true;
}

static boolean process_key(int key)
{
  static int extended = 0;
  boolean ret;

  if(key == 0xE0)
  {
    extended = 0x80;
    return false;
  }

  if(key & 0x80)
    ret = process_keyrelease((key & 0x7F) | extended);
  else
    ret = process_keypress(key | extended);
  extended = 0;

  return ret;
}


static struct mouse_event mouse_last = { 0, 0, 0, 0 };

static boolean read_mouse(struct mouse_event *mev)
{
  if(mouse_read == mouse_write)
    return false;
  *mev = mouse_buffer[mouse_read++];
  return true;
}

static boolean process_mouse(struct mouse_event *mev)
{
  struct buffered_status *status = store_status();
  boolean rval = false;

  if(mev->cond & 0x01)
  {
    int mx = status->mouse_pixel_x + mev->dx - mouse_last.dx;
    int my = status->mouse_pixel_y + mev->dy - mouse_last.dy;

    if(mx < 0)
      mx = 0;
    if(my < 0)
      my = 0;
    if(mx >= 640)
      mx = 639;
    if(my >= 350)
      my = 349;

    status->mouse_pixel_x = mx;
    status->mouse_pixel_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;
    rval = true;

    mouse_last.dx = mev->dx;
    mouse_last.dy = mev->dy;
  }

  if(mev->cond & 0x7E)
  {
    static const uint32_t buttons[] =
    {
      MOUSE_BUTTON_LEFT,
      MOUSE_BUTTON_RIGHT,
      MOUSE_BUTTON_MIDDLE
    };
    int changed = mev->button ^ mouse_last.button;
    int i, j;
    for(i = 0, j = 1; i < 3; i++, j <<= 1)
    {
      if(changed & j)
      {
        if(mev->button & j)
        {
          status->mouse_button = buttons[i];
          status->mouse_repeat = buttons[i];
          status->mouse_button_state |= MOUSE_BUTTON(buttons[i]);
          status->mouse_repeat_state = 1;
          status->mouse_drag_state = -1;
          status->mouse_time = get_ticks();
        }
        else
        {
          status->mouse_button_state &= ~MOUSE_BUTTON(buttons[i]);
          status->mouse_repeat = 0;
          status->mouse_drag_state = 0;
          status->mouse_repeat_state = 0;
        }
        rval = true;
      }
    }
    mouse_last.button = mev->button;
  }
  return rval;
}

boolean __update_event_status(void)
{
  struct mouse_event mev;
  boolean rval = false;
  int key;

  while((key = read_kbd()) != -1)
    rval |= process_key(key);
  while(read_mouse(&mev))
    rval |= process_mouse(&mev);

  return rval;
}

void __wait_event(void)
{
  struct mouse_event mev;
  boolean ret;
  int key;

  while((key = read_kbd()) == -1 && !(ret = read_mouse(&mev)));
  if(key != -1)
    process_key(key);
  if(ret)
    process_mouse(&mev);
}

void __warp_mouse(int x, int y)
{
  // TODO?
}

boolean __peek_exit_input(void)
{
  // TODO stub
  return false;
}

boolean platform_has_screen_keyboard(void)
{
  return false;
}

boolean platform_show_screen_keyboard(void)
{
  return false;
}

boolean platform_hide_screen_keyboard(void)
{
  return false;
}

boolean platform_is_screen_keyboard_active(void)
{
  return false;
}

void platform_init_event(void)
{
  // nop
}
