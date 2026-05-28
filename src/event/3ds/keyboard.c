/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "../event.h"
#include "../../util.h"

#include <3ds.h>
#include <citro3d.h>

#include "event_3ds.h"
#include "keyboard.h"
#include "../../platform/3ds/platform_3ds.h"
#include "../../render/3ds/renderer_ctr.h"

//#define MAX_KEYS_DOWN 8

C3D_Tex *keyboard_tex;
static enum keycode keys_down[] =
{
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN
};
static u8 keys_down_count = 0;
static boolean force_zoom_out = false;

static touch_area_t touch_areas[] =
{
  { 2, 118, 22, 16, IKEY_ESCAPE, 0 },
  { 23, 118, 22, 16, IKEY_F1, 0 },
  { 44, 118, 22, 16, IKEY_F2, 0 },
  { 65, 118, 22, 16, IKEY_F3, 0 },
  { 86, 118, 22, 16, IKEY_F4, 0 },
  { 107, 118, 22, 16, IKEY_F5, 0 },
  { 128, 118, 22, 16, IKEY_F6, 0 },
  { 149, 118, 22, 16, IKEY_F7, 0 },
  { 170, 118, 22, 16, IKEY_F8, 0 },
  { 191, 118, 22, 16, IKEY_F9, 0 },
  { 212, 118, 22, 16, IKEY_F10, 0 },
  { 233, 118, 22, 16, IKEY_F11, 0 },
  { 254, 118, 22, 16, IKEY_F12, 0 },
  { 275, 118, 22, 16, IKEY_INSERT, 0 },
  { 296, 118, 22, 16, IKEY_DELETE, 0 },

  { 2, 133, 22, 22, IKEY_BACKQUOTE, 0 },
  { 23, 133, 22, 22, IKEY_1, 0 },
  { 44, 133, 22, 22, IKEY_2, 0 },
  { 65, 133, 22, 22, IKEY_3, 0 },
  { 86, 133, 22, 22, IKEY_4, 0 },
  { 107, 133, 22, 22, IKEY_5, 0 },
  { 128, 133, 22, 22, IKEY_6, 0 },
  { 149, 133, 22, 22, IKEY_7, 0 },
  { 170, 133, 22, 22, IKEY_8, 0 },
  { 191, 133, 22, 22, IKEY_9, 0 },
  { 212, 133, 22, 22, IKEY_0, 0 },
  { 233, 133, 22, 22, IKEY_MINUS, 0 },
  { 254, 133, 22, 22, IKEY_EQUALS, 0 },
  { 275, 133, 43, 22, IKEY_BACKSPACE, 0 },

  { 2, 154, 34, 22, IKEY_TAB, 0 },
  { 35, 154, 22, 22, IKEY_q, 0 },
  { 56, 154, 22, 22, IKEY_w, 0 },
  { 77, 154, 22, 22, IKEY_e, 0 },
  { 98, 154, 22, 22, IKEY_r, 0 },
  { 119, 154, 22, 22, IKEY_t, 0 },
  { 140, 154, 22, 22, IKEY_y, 0 },
  { 161, 154, 22, 22, IKEY_u, 0 },
  { 182, 154, 22, 22, IKEY_i, 0 },
  { 203, 154, 22, 22, IKEY_o, 0 },
  { 224, 154, 22, 22, IKEY_p, 0 },
  { 245, 154, 22, 22, IKEY_LEFTBRACKET, 0 },
  { 266, 154, 22, 22, IKEY_RIGHTBRACKET, 0 },
  { 287, 154, 31, 22, IKEY_BACKSLASH, 0 },

  { 2, 175, 41, 22, IKEY_LCTRL, 0 },
  { 42, 175, 22, 22, IKEY_a, 0 },
  { 63, 175, 22, 22, IKEY_s, 0 },
  { 84, 175, 22, 22, IKEY_d, 0 },
  { 105, 175, 22, 22, IKEY_f, 0 },
  { 126, 175, 22, 22, IKEY_g, 0 },
  { 147, 175, 22, 22, IKEY_h, 0 },
  { 168, 175, 22, 22, IKEY_j, 0 },
  { 189, 175, 22, 22, IKEY_k, 0 },
  { 210, 175, 22, 22, IKEY_l, 0 },
  { 231, 175, 22, 22, IKEY_SEMICOLON, 0 },
  { 252, 175, 22, 22, IKEY_QUOTE, 0 },
  { 273, 175, 45, 22, IKEY_RETURN, 0 },

  { 2, 196, 54, 22, IKEY_LSHIFT, 0 },
  { 55, 196, 22, 22, IKEY_z, 0 },
  { 76, 196, 22, 22, IKEY_x, 0 },
  { 97, 196, 22, 22, IKEY_c, 0 },
  { 118, 196, 22, 22, IKEY_v, 0 },
  { 139, 196, 22, 22, IKEY_b, 0 },
  { 160, 196, 22, 22, IKEY_n, 0 },
  { 181, 196, 22, 22, IKEY_m, 0 },
  { 202, 196, 22, 22, IKEY_COMMA, 0 },
  { 223, 196, 22, 22, IKEY_PERIOD, 0 },
  { 244, 196, 22, 22, IKEY_SLASH, 0 },
  { 265, 196, 53, 22, IKEY_RSHIFT, 0 },

  { 29, 217, 37, 22, IKEY_LALT, 0 },
  { 65, 217, 190, 22, IKEY_SPACE, 0 },
  { 254, 217, 37, 22, IKEY_RALT, 0 }
};
#define touch_areas_len (sizeof(touch_areas) / sizeof(touch_area_t))

static inline boolean ctr_is_modifier(enum keycode keycode)
{
  return keycode >= IKEY_RSHIFT && keycode <= IKEY_RSUPER;
}

static inline boolean ctr_key_touched(touchPosition *pos, touch_area_t *area)
{
  return (pos->px >= area->x) && (pos->py >= area->y) &&
   (pos->px < (area->x + area->w)) && (pos->py < (area->y + area->h));
}

boolean ctr_keyboard_force_zoom_out(void)
{
  return force_zoom_out;
}

void ctr_keyboard_init(struct ctr_render_data *render_data)
{
  keyboard_tex = ctr_load_png("romfs:/kbd_display.png");
}

void ctr_keyboard_draw(struct ctr_render_data *render_data)
{
  size_t i, j;

  ctr_draw_2d_texture(render_data, keyboard_tex, 0, 0, 320, 240, 0, 0, 320, 240,
   4.0f, 0xffffffff, false);

  if(ctr_is_2d())
  {
    ctr_draw_2d_texture(render_data, keyboard_tex, force_zoom_out ? 16 : 0, 240,
     16, 16, 302, 2, 16, 16, 3.0f, 0xffffffff, false);
  }

  if(ctr_supports_wide())
  {
    ctr_draw_2d_texture(render_data, keyboard_tex, gfxIsWide() ? 48 : 32, 240,
     16, 16, 284, 2, 16, 16, 3.0f, 0xffffffff, false);
  }

  if(keys_down_count > 0)
  {
    for(i = 0; i < touch_areas_len; i++)
    {
      touch_area_t *area = &touch_areas[i];
      for(j = 0; j < keys_down_count; j++)
      {
        if(keys_down[j] == area->keycode)
        {
          ctr_draw_2d_texture(render_data, keyboard_tex, area->x,
           240 - area->y - area->h, area->w, area->h,
           area->x, area->y + 1, area->w, area->h - 1,
           3.0f, 0x808080ff, false);
          break;
        }
      }
    }
  }
}

boolean ctr_keyboard_update(struct buffered_status *status)
{
  touchPosition pos;
  u32 down, up, i;
  boolean retval = false;
  u32 unicode;

  if(get_bottom_screen_mode() != BOTTOM_SCREEN_MODE_KEYBOARD)
    return retval;

  down = hidKeysDown();
  up = hidKeysUp();
  hidTouchRead(&pos);

  if(down & KEY_TOUCH)
  {
    if(ctr_is_2d() && pos.px >= 302 && pos.py >= 2 && pos.px < 318 &&
     pos.py < 18)
    {
      force_zoom_out = !force_zoom_out;
    }
    else

    if(ctr_supports_wide() && pos.px >= 284 && pos.py >= 2 &&
     pos.px < 300 && pos.py < 18)
    {
      ctr_request_set_wide(!gfxIsWide());
    }

    for(i = 0; i < touch_areas_len; i++)
    {
      touch_area_t *area = &touch_areas[i];
      if(ctr_key_touched(&pos, area))
      {
        unicode = convert_internal_unicode(area->keycode, false);

        key_press(status, area->keycode);
        key_press_unicode(status, unicode, true);

        keys_down[keys_down_count++] = area->keycode;
        retval = true;
        break;
      }
    }
  }

  if(up & KEY_TOUCH && keys_down_count > 0 &&
   !ctr_is_modifier(keys_down[keys_down_count - 1]))
  {
    for(; keys_down_count > 0; keys_down_count--)
    {
      key_release(status, keys_down[keys_down_count - 1]);
      keys_down[keys_down_count - 1] = IKEY_UNKNOWN;
    }
    retval = true;
  }

  return retval;
}
