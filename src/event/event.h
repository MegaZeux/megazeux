/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __EVENT_H
#define __EVENT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "keysym.h"

#include <stdint.h>

#define UPDATE_DELAY 16

#define KEY_REPEAT_STACK_SIZE 32
#define KEY_UNICODE_MAX 1024

#define STATUS_NUM_KEYCODES 512

#if defined(CONFIG_NDS) || defined(CONFIG_3DS)
#define MAX_JOYSTICKS           1
#define MAX_JOYSTICK_AXES       4
#define MAX_JOYSTICK_BUTTONS    16
#else
#define MAX_JOYSTICKS           16
#define MAX_JOYSTICK_AXES       16
#define MAX_JOYSTICK_BUTTONS    256
#endif

#define MAX_JOYSTICK_PRESS      16
#define AXIS_DEFAULT_THRESHOLD  10000

// Capture F12 presses to save screenshots if true.
extern boolean enable_f12_hack;

// Store joystick presses in a list so their proper key will be released.
struct joystick_press
{
  uint8_t type;
  uint8_t num;
  int16_t key;
  uint8_t action;
};

struct buffered_status
{
  enum keycode key_pressed;
  enum keycode key;
  enum keycode key_repeat;
  enum keycode key_release;
  uint32_t *unicode;
  uint32_t unicode_repeat;
  int unicode_pos;
  int unicode_length;
  int unicode_alloc;
  uint32_t keypress_time;
  int mouse_x;
  int mouse_y;
  int mouse_last_x;
  int mouse_last_y;
  int mouse_pixel_x;
  int mouse_pixel_y;
  int mouse_last_pixel_x;
  int mouse_last_pixel_y;
  int warped_mouse_x;
  int warped_mouse_y;
  enum mouse_button mouse_button;
  enum mouse_button mouse_repeat;
  int mouse_repeat_state;
  int mouse_drag_state;
  uint32_t mouse_button_state;
  uint32_t mouse_time;
  boolean caps_status;
  boolean numlock_status;
  boolean mouse_moved;
  boolean exit_status;
  enum joystick_action joystick_action;
  enum joystick_action joystick_repeat;
  int joystick_repeat_state;
  int joystick_repeat_id;
  uint32_t joystick_time;

  // NOTE: These arrays track raw joystick state but this is only used to
  // determine if an action status or a simulated press should be altered.
  // So, these arrays probably don't need to be processed in a network context.
  boolean joystick_hat[MAX_JOYSTICKS][4];
  boolean joystick_button[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  int16_t joystick_axis[MAX_JOYSTICKS][MAX_JOYSTICK_AXES];

  // NOTE: These arrays track abstracted joystick state and are the ones that
  // simulated keypresses, games, and event buffering need to care about.
  boolean joystick_active[MAX_JOYSTICKS];
  boolean joystick_action_status[MAX_JOYSTICKS][NUM_JOYSTICK_ACTIONS];
  int16_t joystick_special_axis_status[MAX_JOYSTICKS][NUM_JOYSTICK_SPECIAL_AXES];
  struct joystick_press joystick_press[MAX_JOYSTICKS][MAX_JOYSTICK_PRESS];
  uint8_t joystick_press_count[MAX_JOYSTICKS];

  uint8_t keymap[512];
};

struct joystick_map
{
  int16_t button[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  int16_t axis[MAX_JOYSTICKS][MAX_JOYSTICK_AXES][2];
  int16_t hat[MAX_JOYSTICKS][4];
  int16_t action[MAX_JOYSTICKS][NUM_JOYSTICK_ACTIONS];
  uint8_t special_axis[MAX_JOYSTICKS][MAX_JOYSTICK_AXES];
  int16_t show_screen_keyboard_action[MAX_JOYSTICKS];

  boolean button_is_conf[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  boolean axis_is_conf[MAX_JOYSTICKS][MAX_JOYSTICK_AXES];
  boolean hat_is_conf[MAX_JOYSTICKS];
  boolean action_is_conf[MAX_JOYSTICKS][NUM_JOYSTICK_ACTIONS];
  boolean show_keyboard_is_conf[MAX_JOYSTICKS];
};

struct input_status
{
  struct buffered_status *buffer;
  unsigned int load_offset;
  unsigned int store_offset;

  enum keycode key_repeat_stack[KEY_REPEAT_STACK_SIZE];
  uint32_t unicode_repeat_stack[KEY_REPEAT_STACK_SIZE];
  unsigned int repeat_stack_pointer;

  struct joystick_map joystick_global_map;
  struct joystick_map joystick_game_map;
  uint16_t joystick_axis_threshold;

  boolean unfocus_pause;
  boolean left_alt_is_altgr;
  boolean right_alt_is_altgr;
  boolean showing_screen_keyboard; /* optional, used by platform drivers */
};

// regular keycode_internal treats numpad keys as unique keys
// wrt_numlock translates them to numeric/navigation based on numlock status
// keycode_text_ascii masks unicode chars from 32 to 126; keycode_text_unicode
// will return any unicode char encountered >= 32.
enum keycode_type
{
  keycode_pc_xt,
  keycode_internal,
  keycode_internal_wrt_numlock,
  keycode_text_ascii,
  keycode_text_unicode
};

#define KEYCODE_IS_ASCII(key) ((key) >= 32 && (key) < 127)

struct config_info;

CORE_LIBSPEC void init_event(struct config_info *conf);

struct buffered_status *store_status(void);

CORE_LIBSPEC boolean update_event_status(void);
CORE_LIBSPEC boolean update_event_status_delay(void);
CORE_LIBSPEC void update_event_status_intake(void);
CORE_LIBSPEC void force_release_all_keys(void);
CORE_LIBSPEC uint32_t get_key(enum keycode_type type);
CORE_LIBSPEC uint32_t get_last_key(enum keycode_type type);
CORE_LIBSPEC uint32_t get_key_status(enum keycode_type type, uint32_t index);
CORE_LIBSPEC void get_mouse_position(int *char_x, int *char_y);
CORE_LIBSPEC void get_mouse_pixel_position(int *pixel_x, int *pixel_y);
CORE_LIBSPEC void get_mouse_movement(int *delta_x, int *delta_y);
CORE_LIBSPEC void get_mouse_pixel_movement(int *delta_x, int *delta_y);
CORE_LIBSPEC enum mouse_button get_mouse_press(void);
CORE_LIBSPEC enum mouse_button get_mouse_press_ext(void);
CORE_LIBSPEC boolean get_mouse_held(int button);
CORE_LIBSPEC uint32_t get_mouse_status(void);
CORE_LIBSPEC void warp_mouse(int char_x, int char_y);
CORE_LIBSPEC void warp_mouse_pixel_x(int pixel_x);
CORE_LIBSPEC void warp_mouse_pixel_y(int pixel_y);
CORE_LIBSPEC boolean get_mouse_drag(void);
CORE_LIBSPEC boolean get_alt_status(enum keycode_type type);
CORE_LIBSPEC boolean get_altgr_status(enum keycode_type type);
CORE_LIBSPEC boolean get_shift_status(enum keycode_type type);
CORE_LIBSPEC boolean get_ctrl_status(enum keycode_type type);
CORE_LIBSPEC boolean get_exit_status(void);
CORE_LIBSPEC boolean set_exit_status(boolean value);
CORE_LIBSPEC boolean peek_exit_input(void);
CORE_LIBSPEC struct joystick_map *get_joystick_map(boolean is_global);
CORE_LIBSPEC enum joystick_action get_joystick_ui_action(void);
CORE_LIBSPEC enum keycode get_joystick_ui_key(void);

boolean has_unicode_input(void);
uint32_t convert_internal_unicode(enum keycode key, boolean caps_lock);

// Implemented by "drivers" (SDL, Wii, NDS, 3DS, etc.)
void platform_init_event(void);
boolean platform_has_screen_keyboard(void);
boolean platform_show_screen_keyboard(void);
boolean platform_hide_screen_keyboard(void);
boolean platform_is_screen_keyboard_active(void);
boolean __update_event_status(void);
boolean __peek_exit_input(void);
void __wait_event(void);
void __warp_mouse(int x, int y);

// "Driver" functions currently only supported by SDL.
void sdl_init_window_text_events(unsigned sdl_window_id);
void gamepad_map_sym(const char *sym, const char *value);
void gamepad_add_mapping(const char *mapping);

#ifdef CONFIG_NDS
const struct buffered_status *load_status(void);
#endif

void wait_event(int timeout);
void force_last_key(enum keycode_type type, int val);
void warp_mouse_x(int char_x);
void warp_mouse_y(int char_y);
int get_mouse_x(void);
int get_mouse_y(void);
int get_mouse_pixel_x(void);
int get_mouse_pixel_y(void);
uint32_t get_last_key_released(enum keycode_type type);
void set_unfocus_pause(boolean value);
void set_num_buffered_events(unsigned int value);

void key_press(struct buffered_status *status, enum keycode key);
void key_press_unicode(struct buffered_status *status,
 uint32_t unicode, boolean repeating);
void key_release(struct buffered_status *status, enum keycode key);
boolean screen_keyboard_is_active(void);
boolean screen_keyboard_toggle_state(void);

boolean joystick_parse_map_value(const char *value, int16_t *binding);
void joystick_map_button(int first, int last, int button, const char *value,
 boolean is_global);
void joystick_map_axis(int first, int last, int axis, const char *neg,
 const char *pos, boolean is_global);
void joystick_map_hat(int first, int last, const char *up, const char *down,
 const char *left, const char *right, boolean is_global);
void joystick_map_action(int first, int last, const char *action,
 const char *value, boolean is_global);
void joystick_map_fallback_keyboard_button(int joystick, int button);
void joystick_reset_game_map(void);
void joystick_set_game_mode(boolean enable);
void joystick_set_game_bindings(boolean enable);
void joystick_set_legacy_loop_hacks(boolean enable);
void joystick_set_axis_threshold(uint16_t threshold);
void joystick_button_press(struct buffered_status *status,
 int joystick, int button);
void joystick_button_release(struct buffered_status *status,
 int joystick, int button);
void joystick_hat_update(struct buffered_status *status,
 int joystick, enum joystick_hat dir, boolean dir_active);
void joystick_axis_update(struct buffered_status *status,
 int joystick, int axis, int16_t value);
void joystick_special_axis_update(struct buffered_status *status,
 int joystick, enum joystick_special_axis axis, int16_t value);
void joystick_set_active(struct buffered_status *status, int joystick,
 boolean active);
void joystick_clear(struct buffered_status *status, int joystick);
boolean joystick_is_active(int joystick, boolean *is_active);
boolean joystick_get_status(int joystick, char *name, int16_t *value);

__M_END_DECLS

#endif // __EVENT_H
