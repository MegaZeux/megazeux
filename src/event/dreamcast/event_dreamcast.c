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
#include "../../util.h"
#include "../../platform/platform.h"

#include <kos.h>

extern struct input_status input;

void platform_init_event(void)
{
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

void __warp_mouse(int x, int y)
{
}

boolean dc_update_input(void);

boolean __update_event_status(void)
{
  boolean retval = false;
  retval |= dc_update_input();
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
    thd_pass();
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
  uint32_t down, uint32_t up, uint32_t key, int code)
{
  if(down & key)
  {
    joystick_button_press(status, 0, code);
    return true;
  }
  else

  if(up & key)
  {
    joystick_button_release(status, 0, code);
    return true;
  }

  else
  {
    return false;
  }
}

static uint32_t last_buttons;
static uint32_t last_ltrig, last_rtrig;

boolean dc_update_input(void)
{
  struct buffered_status *status = store_status();
  maple_device_t *maple_pad;
  //maple_device_t *maple_kbd;
  cont_state_t *pad;
  uint32_t down;
  //uint32_t held;
  uint32_t up;
  boolean retval = false;

  maple_pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
  if(maple_pad && (pad = (cont_state_t *) maple_dev_status(maple_pad)))
  {
    down = (pad->buttons ^ last_buttons) & pad->buttons;
    //held = (pad->buttons & last_buttons);
    up = (pad->buttons ^ last_buttons) & last_buttons;

    retval |= check_hat(status, down, up, CONT_DPAD_UP, JOYHAT_UP);
    retval |= check_hat(status, down, up, CONT_DPAD_DOWN, JOYHAT_DOWN);
    retval |= check_hat(status, down, up, CONT_DPAD_LEFT, JOYHAT_LEFT);
    retval |= check_hat(status, down, up, CONT_DPAD_RIGHT, JOYHAT_RIGHT);
    retval |= check_joy(status, down, up, CONT_A, 0);
    retval |= check_joy(status, down, up, CONT_B, 1);
    retval |= check_joy(status, down, up, CONT_X, 2);
    retval |= check_joy(status, down, up, CONT_Y, 3);
    retval |= check_joy(status, down, up, CONT_START, 7);
    retval |= ((last_ltrig ^ pad->ltrig) >= 0x80) || ((last_rtrig ^ pad->rtrig) >= 0x80);

    if(last_ltrig >= 0x80 && pad->ltrig < 0x80)
        joystick_button_release(status, 0, 4);
    if(last_ltrig < 0x80 && pad->ltrig >= 0x80)
        joystick_button_press(status, 0, 4);
    if(last_rtrig >= 0x80 && pad->rtrig < 0x80)
        joystick_button_release(status, 0, 5);
    if(last_rtrig < 0x80 && pad->rtrig >= 0x80)
        joystick_button_press(status, 0, 5);

    last_buttons = pad->buttons;
    last_ltrig = pad->ltrig;
    last_rtrig = pad->rtrig;
  }
  else
  {
    last_buttons = last_ltrig = last_rtrig = 0;
  }

  return retval;
}
