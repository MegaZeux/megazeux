/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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
#include "../../graphics.h"
#include "../../util.h"
#include "../../platform/platform.h"

#include <3ds.h>
#include <stdint.h>
#include "event_3ds.h"
#include "keyboard.h"

extern struct input_status input;
static enum bottom_screen_mode b_mode;
static enum focus_mode allow_focus_changes = FOCUS_ALLOW;
static boolean is_dragging = false;

boolean update_hid(void);

enum focus_mode get_allow_focus_changes(void)
{
  return allow_focus_changes;
}

enum bottom_screen_mode get_bottom_screen_mode(void)
{
  return b_mode;
}

boolean __update_event_status(void)
{
  boolean retval = false;
  retval |= aptMainLoop();
  retval |= update_hid();
  return retval;
}

boolean __peek_exit_input(void)
{
  /* FIXME stub */
  return false;
}

void __wait_event(void)
{
  while(!__update_event_status())
    gspWaitForVBlank();
}

void __warp_mouse(int x, int y)
{
  // Since the touchscreen stylus can't be warped, focus there instead.
  focus_pixel(x, y);
}

// Convert the 3DS axis ranges to the MZX axis ranges. The 3DS uses values in
// (roughly) the -150 to 150 range, but MZX uses the full range of a int16_t.
// The 3DS Y axis is also inverted from the typical axis direction, but that
// is fixed in check_circle below.
static inline int16_t axis_convert(s16 value)
{
  int new_value = ((int)value) * 32768 / 150;
  return (int16_t)CLAMP(new_value, -32768, 32767);
}

static inline boolean check_circle(struct buffered_status *status,
 circlePosition *current, circlePosition *prev, int axis_x, int axis_y)
{
  boolean rval = false;

  if(current->dx != prev->dx)
  {
    joystick_axis_update(status, 0, axis_x, axis_convert(current->dx));
    rval = true;
  }

  if(current->dy != prev->dy)
  {
    // 3DS has an inverted Y axis on the circle pad vs. most controllers.
    joystick_axis_update(status, 0, axis_y, axis_convert(-current->dy));
    rval = true;
  }

  return rval;
}

static inline boolean check_hat(struct buffered_status *status,
 uint32_t down, uint32_t up, uint32_t key, enum joystick_hat dir)
{
  if(down & key)
  {
    joystick_hat_update(status, 0, dir, true);
    return true;
  }
  else

  if(up & key)
  {
    joystick_hat_update(status, 0, dir, false);
    return true;
  }

  return false;
}

static inline boolean check_joy(struct buffered_status *status,
 uint32_t down, uint32_t up, uint32_t key, int button)
{
  if(down & key)
  {
    joystick_button_press(status, 0, button);
    return true;
  }
  else

  if(up & key)
  {
    joystick_button_release(status, 0, button);
    return true;
  }

  return false;
}

static inline boolean ctr_is_mouse_area(touchPosition *touch)
{
  int mx, my;

  if(b_mode == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    mx = touch->px * 4 - 320;
    my = touch->py * 4 - (13 * 4);

    if(mx < 0 || mx >= 640 || my < 0 || my >= 350)
      return false;

    else
      return true;
  }
  else
  {
    return true;
  }
}

static inline boolean ctr_update_touch(struct buffered_status *status,
 touchPosition *touch)
{
  int mx, my;

  if(b_mode == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    mx = touch->px * 4 - 320;
    my = touch->py * 4 - (13 * 4);

    if(mx < 0 || mx >= 640 || my < 0 || my >= 350)
      return false;
  }
  else
  {
    int s_height = ctr_get_subscreen_height();
    mx = touch->px * 2;
    my = ((int)(touch->py - ((240 - s_height) / 2)) * 350 / s_height);
    if(mx < 0) mx = 0;
    if(mx >= 640) mx = 639;
    if(my < 0) my = 0;
    if(my >= 350) my = 349;
  }

  if(mx != status->mouse_pixel_x || my != status->mouse_pixel_y)
  {
    status->mouse_pixel_x = mx;
    status->mouse_pixel_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;

    allow_focus_changes = FOCUS_PASS;
    focus_pixel(mx, my);
    allow_focus_changes = FOCUS_FORBID;

    return true;
  }
  else
  {
    return false;
  }
}

static inline boolean ctr_update_cstick(struct buffered_status *status)
{
  circlePosition pos;
  int dmx, dmy, nmx, nmy;

  hidCstickRead(&pos);
  dmx = pos.dx / 3;
  dmy = pos.dy / 3;

  if(dmx != 0 || dmy != 0)
  {
    nmx = status->mouse_pixel_x + dmx;
    nmy = status->mouse_pixel_y + dmy;
    if(nmx < 0) nmx = 0;
    if(nmx >= 640) nmx = 639;
    if(nmy < 0) nmy = 0;
    if(nmy >= 350) nmy = 349;

    if(nmx != status->mouse_pixel_x || nmy != status->mouse_pixel_y)
    {
      status->mouse_pixel_x = nmx;
      status->mouse_pixel_y = nmy;
      status->mouse_x = nmx / 8;
      status->mouse_y = nmy / 14;
      status->mouse_moved = true;

      focus_pixel(nmx, nmy);

      return true;
    }
  }

  return false;
}

boolean update_hid(void)
{
  struct buffered_status *status = store_status();
  uint32_t down, held, up;
  boolean retval = false;
  touchPosition touch;
  circlePosition cpad;
  static circlePosition last_cpad;

  hidScanInput();
  hidCircleRead(&cpad);
  down = hidKeysDown();
  held = hidKeysHeld();
  up = hidKeysUp();

  retval |= check_circle(status, &cpad, &last_cpad, 0, 1);
  retval |= check_hat(status, down, up, KEY_DUP, JOYHAT_UP);
  retval |= check_hat(status, down, up, KEY_DDOWN, JOYHAT_DOWN);
  retval |= check_hat(status, down, up, KEY_DLEFT, JOYHAT_LEFT);
  retval |= check_hat(status, down, up, KEY_DRIGHT, JOYHAT_RIGHT);
  retval |= check_joy(status, down, up, KEY_A, 0);
  retval |= check_joy(status, down, up, KEY_B, 1);
  retval |= check_joy(status, down, up, KEY_X, 2);
  retval |= check_joy(status, down, up, KEY_Y, 3);
  retval |= check_joy(status, down, up, KEY_L, 4);
  retval |= check_joy(status, down, up, KEY_R, 5);
  retval |= check_joy(status, down, up, KEY_SELECT, 6);
  retval |= check_joy(status, down, up, KEY_START, 7);
  retval |= check_joy(status, down, up, KEY_ZL, 8);
  retval |= check_joy(status, down, up, KEY_ZR, 9);

  last_cpad = cpad;

  if((down | held | up) & KEY_TOUCH)
  {
    hidTouchRead(&touch);

    if((down & KEY_TOUCH) && ctr_is_mouse_area(&touch))
    {
      status->mouse_button = MOUSE_BUTTON_LEFT;
      status->mouse_repeat = MOUSE_BUTTON_LEFT;
      status->mouse_button_state |= MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      is_dragging = true;
      allow_focus_changes = FOCUS_FORBID;
      retval = true;
    }

    if(is_dragging)
    {
      if(up & KEY_TOUCH)
      {
        status->mouse_button_state &= ~MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
        status->mouse_repeat = 0;
        status->mouse_repeat_state = 0;
        status->mouse_drag_state = -0;
        allow_focus_changes = FOCUS_ALLOW;
        is_dragging = false;
        retval = true;
      }
      else
      {
        retval |= ctr_update_touch(status, &touch);
      }
    }

    retval |= ctr_keyboard_update(status);
  }

//  retval |= ctr_update_cstick(status);

  return retval;
}

int ctr_get_subscreen_height(void)
{
  switch(get_config()->video_ratio)
  {
    case RATIO_CLASSIC_4_3:
    case RATIO_STRETCH:
      return 240;
    default:
      return 175;
  }
}

boolean platform_has_screen_keyboard(void)
{
  return true;
}

boolean platform_show_screen_keyboard(void)
{
  input.showing_screen_keyboard = true;
  b_mode = BOTTOM_SCREEN_MODE_KEYBOARD;
  return true;
}

boolean platform_hide_screen_keyboard(void)
{
  input.showing_screen_keyboard = false;
  b_mode = BOTTOM_SCREEN_MODE_PREVIEW;
  return true;
}

boolean platform_is_screen_keyboard_active(void)
{
  return input.showing_screen_keyboard;
}

void platform_init_event(void)
{
  struct buffered_status *status = store_status();
  joystick_set_active(status, 0, true);
  joystick_map_fallback_keyboard_button(0, 5);
}
