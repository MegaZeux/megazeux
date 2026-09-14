/* MegaZeux
*
* Copyright (C) 1996 Alexis Janson
* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
* Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "../../util.h"
#include "../../platform/platform.h"
#include "../../platform/djgpp/interrupt.h"

extern struct input_status input;

struct bios_key_event
{
  uint16_t code;
  uint8_t ascii;
  uint8_t alternate;
};

static struct bios_key_event bios_pending[64];
static uint8_t bios_pending_pos;
static uint8_t bios_pending_num;

static int read_kbd(void)
{
  int ret;
  if(kbd_read == kbd_write)
    return -1;
  ret = kbd_buffer[kbd_read++];
  trace("buffer: %d\n", ret);
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
  IKEY_RETURN, IKEY_LCTRL, IKEY_a, IKEY_s,
  // 2x
  IKEY_d, IKEY_f, IKEY_g, IKEY_h,
  IKEY_j, IKEY_k, IKEY_l, IKEY_SEMICOLON,
  IKEY_QUOTE, IKEY_BACKQUOTE, IKEY_LSHIFT, IKEY_BACKSLASH,
  IKEY_z, IKEY_x, IKEY_c, IKEY_v,
  // 3x
  IKEY_b, IKEY_n, IKEY_m, IKEY_COMMA,
  IKEY_PERIOD, IKEY_SLASH, IKEY_RSHIFT, IKEY_KP_MULTIPLY,
  IKEY_LALT, IKEY_SPACE, IKEY_CAPSLOCK, IKEY_F1,
  IKEY_F2, IKEY_F3, IKEY_F4, IKEY_F5,
  // 4x
  IKEY_F6, IKEY_F7, IKEY_F8, IKEY_F9,
  IKEY_F10, IKEY_NUMLOCK, IKEY_SCROLLLOCK, IKEY_KP7,
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
  IKEY_KP_ENTER, IKEY_RCTRL, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 2x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 3x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_KP_DIVIDE, IKEY_UNKNOWN, IKEY_SYSREQ,
  IKEY_RALT, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
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

static enum keycode convert_xt_internal(uint8_t key, boolean alternate)
{
  if(alternate)
    return extended_xt_to_internal[key];
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
    case 0x54: return XTKEY_F1;           // Shift-F1
    case 0x55: return XTKEY_F2;           // Shift-F2
    case 0x56: return XTKEY_F3;           // Shift-F3
    case 0x57: return XTKEY_F4;           // Shift-F4
    case 0x58: return XTKEY_F5;           // Shift-F5
    case 0x59: return XTKEY_F6;           // Shift-F6
    case 0x5A: return XTKEY_F7;           // Shift-F7
    case 0x5B: return XTKEY_F8;           // Shift-F8
    case 0x5C: return XTKEY_F9;           // Shift-F9
    case 0x5D: return XTKEY_F10;          // Shift-F10
    case 0x5E: return XTKEY_F1;           // Ctrl-F1
    case 0x5F: return XTKEY_F2;           // Ctrl-F2
    case 0x60: return XTKEY_F3;           // Ctrl-F3
    case 0x61: return XTKEY_F4;           // Ctrl-F4
    case 0x62: return XTKEY_F5;           // Ctrl-F5
    case 0x63: return XTKEY_F6;           // Ctrl-F6
    case 0x64: return XTKEY_F7;           // Ctrl-F7
    case 0x65: return XTKEY_F8;           // Ctrl-F8
    case 0x66: return XTKEY_F9;           // Ctrl-F9
    case 0x67: return XTKEY_F10;          // Ctrl-F10
    case 0x68: return XTKEY_F1;           // Alt-F1
    case 0x69: return XTKEY_F2;           // Alt-F2
    case 0x6A: return XTKEY_F3;           // Alt-F3
    case 0x6B: return XTKEY_F4;           // Alt-F4
    case 0x6C: return XTKEY_F5;           // Alt-F5
    case 0x6D: return XTKEY_F6;           // Alt-F6
    case 0x6E: return XTKEY_F7;           // Alt-F7
    case 0x6F: return XTKEY_F8;           // Alt-F8
    case 0x70: return XTKEY_F9;           // Alt-F9
    case 0x71: return XTKEY_F10;          // Alt-F10
    case 0x72: return XTKEY_KP_MULTIPLY;  // Ctrl-PrtSc
    case 0x73: return XTKEY_KP_4;         // Ctrl-KP-4 (Left)
    case 0x74: return XTKEY_KP_6;         // Ctrl-KP-6 (Right)
    case 0x75: return XTKEY_KP_1;         // Ctrl-KP-1 (End)
    case 0x76: return XTKEY_KP_3;         // Ctrl-KP-3 (PgDn)
    case 0x77: return XTKEY_KP_7;         // Ctrl-KP-7 (Home)
    case 0x78: return XTKEY_1;            // Alt-1
    case 0x79: return XTKEY_2;            // Alt-2
    case 0x7A: return XTKEY_3;            // Alt-3
    case 0x7B: return XTKEY_4;            // Alt-4
    case 0x7C: return XTKEY_5;            // Alt-5
    case 0x7D: return XTKEY_6;            // Alt-6
    case 0x7E: return XTKEY_7;            // Alt-7
    case 0x7F: return XTKEY_8;            // Alt-8
    case 0x80: return XTKEY_9;            // Alt-9
    case 0x81: return XTKEY_0;            // Alt-0
    case 0x82: return XTKEY_MINUS;        // Alt--
    case 0x83: return XTKEY_EQUALS;       // Alt-=
    case 0x84: return XTKEY_KP_9;         // Ctrl-PgUp
    case 0x85: return XTKEY_F11;          // F11
    case 0x86: return XTKEY_F12;          // F12
    case 0x87: return XTKEY_F11;          // Shift-F11
    case 0x88: return XTKEY_F12;          // Shift-F12
    case 0x89: return XTKEY_F11;          // Ctrl-F11
    case 0x8A: return XTKEY_F12;          // Ctrl-F12
    case 0x8B: return XTKEY_F11;          // Alt-F11
    case 0x8C: return XTKEY_F12;          // Alt-F12
    case 0x8D: return XTKEY_KP_8;         // Ctrl-KP-8 (Up)
    case 0x8E: return XTKEY_KP_MINUS;     // Ctrl-KP--
    case 0x8F: return XTKEY_KP_5;         // Ctrl-KP-5
    case 0x90: return XTKEY_KP_PLUS;      // Ctrl-KP-+
    case 0x91: return XTKEY_KP_2;         // Ctrl-KP-2 (Down)
    case 0x92: return XTKEY_KP_0;         // Ctrl-KP-0 (Insert)
    case 0x93: return XTKEY_KP_PERIOD;    // Ctrl-KP-. (Delete)
    case 0x94: return XTKEY_TAB;          // Ctrl-Tab
    case 0x95: return XTKEY_SLASH;        // Ctrl-KP-/
    case 0x96: return XTKEY_KP_MULTIPLY;  // Ctrl-KP-*
    case 0x97: return XTKEY_KP_7;         // Alt-KP-7 (Home)
    case 0x98: return XTKEY_KP_8;         // Alt-KP-8 (Up)
    case 0x99: return XTKEY_KP_9;         // Alt-KP-9 (PgUp)
    case 0x9B: return XTKEY_KP_4;         // Alt-KP-4 (Left)
    case 0x9D: return XTKEY_KP_6;         // Alt-KP-6 (Right)
    case 0x9F: return XTKEY_KP_1;         // Alt-KP-1 (End)
    case 0xA0: return XTKEY_KP_2;         // Alt-KP-2 (Down)
    case 0xA1: return XTKEY_KP_3;         // Alt-KP-3 (PgDn)
    case 0xA2: return XTKEY_KP_0;         // Alt-KP-0 (Insert)
    case 0xA3: return XTKEY_KP_PERIOD;    // Alt-KP-. (Delete)
    case 0xA4: return XTKEY_SLASH;        // Alt-KP-/
    case 0xA5: return XTKEY_TAB;          // Alt-Tab
    case 0xA6: return XTKEY_RETURN;       // Alt-Return
    default: return XTKEY_CODE(key);
  }
}

static void poll_keyboard_bios(void)
{
  unsigned short res;

  while(extbioskey(1))
  {
    struct bios_key_event ev;
    res = extbioskey(0);
    ev.code = convert_bios_xt(res >> 8);
    ev.ascii = res & 0xFF;
    ev.alternate = false;
    trace("bioskey: %d %d\n", ev.code, ev.ascii);
    if(ev.ascii == XTKEY_EXTENDED)
    {
      ev.ascii = 0;
      ev.alternate = true;
    }
    if(bios_pending_num < ARRAY_SIZE(bios_pending))
      bios_pending[bios_pending_num++] = ev;
  }
}

static int convert_xt_unicode(uint8_t key, boolean alternate)
{
  struct bios_key_event *ev = NULL;
  unsigned pos = bios_pending_pos;

  trace("cvt key: %d\n", key);

  poll_keyboard_bios();
  for(pos = bios_pending_pos; pos < bios_pending_num; pos++)
  {
    ev = &bios_pending[pos];
    if(ev->code == key && ev->alternate == alternate)
      break;
  }
  if(!ev || pos >= bios_pending_num) /* No corresponding BIOS press */
    return -1;

  trace(" [%2u] -> %d\n", pos, ev->ascii);
  bios_pending_pos = pos;
  return ev->ascii;
}

static boolean non_bios_key(uint8_t key)
{
  switch(key)
  {
    case XTKEY_LCTRL:  /* Alternate: right ctrl */
    case XTKEY_LALT:   /* Alternate: right alt */
    case XTKEY_LSHIFT:
    case XTKEY_RSHIFT:
    case XTKEY_LSUPER: /* Always alternate */
    case XTKEY_RSUPER: /* Always alternate */
    case XTKEY_MENU:   /* Always alternate */
      return true;
    /* BIOS may not send KP return or slash without numlock.
     * BIOS may not send KP return, slash, 0-9, or period at all with Alt,
     * including the alternate versions of these keys. */
    case XTKEY_RETURN:
    case XTKEY_SLASH:
    case XTKEY_KP_7:
    case XTKEY_KP_8:
    case XTKEY_KP_9:
    case XTKEY_KP_4:
    case XTKEY_KP_5:
    case XTKEY_KP_6:
    case XTKEY_KP_1:
    case XTKEY_KP_2:
    case XTKEY_KP_3:
    case XTKEY_KP_0:
    case XTKEY_KP_PERIOD:
      return true;
    // BIOS can't return keycodes >= 0x54 due to Alt/Ctrl keycode modification
    // shenanigans, except for F11/F12 which are given alternate keycodes
    case XTKEY_F11:
    case XTKEY_F12:
      return false;
    default:
      if(key >= 0x54)
        return true;
      else
        return false;
  }
}

static boolean process_keypress(uint8_t key, boolean alternate)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_xt_internal(key, alternate);
  int unicode = convert_xt_unicode(key, alternate);

  if(unicode < 0)
  {
    trace("key %d (ikey %d) has no corresponding BIOS press\n", key, ikey);
    unicode = 0;

    /* This needs to be checked after checking BIOS, as the codes for return
     * and slash (and for several keys when alt is held) may or may not be
     * accompanied by a BIOS press, and they should always consume the BIOS
     * press if present. */
    if(!non_bios_key(key))
      return false;
  }

  if(!ikey)
  {
    if(unicode)
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  if(status->keymap[ikey]) /* suppress key repeat */
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

static boolean process_keyrelease(int key, boolean alternate)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_xt_internal(key, alternate);

  if(!ikey)
  {
    if(status->keymap[IKEY_UNICODE])
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  key_release(status, ikey);
  return true;
}

static boolean process_key(int key)
{
  static boolean alternate = false;
  boolean ret;

  if(key == XTKEY_EXTENDED)
  {
    alternate = true;
    return false;
  }

  if(key & XTKEY_RELEASE)
    ret = process_keyrelease(XTKEY_CODE(key), alternate);
  else
    ret = process_keypress(XTKEY_CODE(key), alternate);
  alternate = false;

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

  /* Always unconditionally poll BIOS at least once */
  bios_pending_pos = 0;
  bios_pending_num = 0;
  poll_keyboard_bios();

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
  __update_event_status();
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
