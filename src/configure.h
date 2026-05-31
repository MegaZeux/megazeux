/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_CONFIGURE_H
#define MEGAZEUX_CONFIGURE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

enum config_type
{
  SYSTEM_CNF,
  GAME_CNF,
  GAME_EDITOR_CNF,
  NUM_CONFIG_TYPES
};

enum force_bpp_special
{
  BPP_AUTO = 0
};

enum ratio_type
{
  RATIO_CLASSIC_4_3,
  RATIO_MODERN_64_35,
  RATIO_STRETCH,
  NUM_RATIO_TYPES
};

enum resample_mode
{
  RESAMPLE_MODE_NONE,
  RESAMPLE_MODE_LINEAR,
  RESAMPLE_MODE_CUBIC,
  RESAMPLE_MODE_FIR,
  NUM_RESAMPLE_MODES
};

enum gl_filter_type
{
  CONFIG_GL_FILTER_NEAREST,
  CONFIG_GL_FILTER_LINEAR,
  NUM_GL_FILTER_TYPES
};

enum system_mouse_type
{
  SYSTEM_MOUSE_OFF,
  SYSTEM_MOUSE_ON,
  SYSTEM_MOUSE_HIDE_SOFTWARE_MOUSE,
  NUM_SYSTEM_MOUSE_TYPES
};

enum cursor_mode_types
{
  CURSOR_MODE_UNDERLINE,  // Underline for text entry (insert).
  CURSOR_MODE_SOLID,      // Solid for text entry (overwrite) and editing.
  CURSOR_MODE_HINT,       // Hidden cursor for active UI element hints.
  CURSOR_MODE_INVISIBLE,  // Cursor disabled.
  NUM_CURSOR_MODE_TYPES
};

enum screensaver_disable_mode
{
  SCREENSAVER_ENABLE,
  SCREENSAVER_DISABLE,
  SCREENSAVER_DISABLE_IN_GAME,
  NUM_SCREENSAVER_MODES
};

enum allow_cheats_type
{
  ALLOW_CHEATS_NEVER,
  ALLOW_CHEATS_MZXRUN,
  ALLOW_CHEATS_ALWAYS,
  NUM_ALLOW_CHEATS_TYPES
};

enum host_family
{
  /** Prefer IPv4 address resolution for hostnames */
  HOST_FAMILY_IPV4,
  /** Prefer IPv6 address resolution for hostnames */
  HOST_FAMILY_IPV6,
  /** Allow either IPv4 or IPv6 addresses. */
  HOST_FAMILY_ANY,
  NUM_HOST_FAMILIES
};

enum update_auto_check_mode
{
  UPDATE_AUTO_CHECK_OFF = 0,
  UPDATE_AUTO_CHECK_ON,
  UPDATE_AUTO_CHECK_SILENT
};

struct config_info
{
  // Video options
  boolean fullscreen;
  boolean fullscreen_windowed;
  int resolution_width;
  int resolution_height;
  int window_width;
  int window_height;
  boolean allow_resize;
  char video_output[16];
  int force_bpp;
  enum ratio_type video_ratio;
  enum gl_filter_type gl_filter_method;
  int gl_vsync;
  char gl_scaling_shader[32];
  char sdl_render_driver[16];
  enum cursor_mode_types cursor_hint_mode;
  enum screensaver_disable_mode disable_screensaver;
  boolean allow_screenshots;

  // Audio options
  int audio_sample_rate;
  int audio_buffer_samples;
  int audio_output_channels;
  boolean oversampling_on;
  enum resample_mode resample_mode;
  enum resample_mode module_resample_mode;
  int max_simultaneous_samples;
  int music_volume;
  int sam_volume;
  int pc_speaker_volume;
  boolean music_on;
  boolean pc_speaker_on;

  // Event options
  boolean allow_gamepad;
  boolean pause_on_unfocus;
  boolean key_left_alt_is_altgr;
  boolean key_right_alt_is_altgr;
  int num_buffered_events;

  // Virtual filesystem options
  boolean vfs_enable;
  boolean vfs_enable_auto_cache;
  int64_t vfs_max_cache_size;
  int64_t vfs_max_cache_file_size;

  // Game options
  char startup_path[256];
  char startup_file[256];
  char default_save_name[256];
  int mzx_speed;
  enum allow_cheats_type allow_cheats;
  boolean auto_decrypt_worlds;
  boolean startup_editor;
  boolean standalone_mode;
  boolean no_titlescreen;
  enum system_mouse_type system_mouse;
  boolean grab_mouse;
  boolean save_slots;
  char save_slots_name[256];
  char save_slots_ext[256];

  // Editor options
  boolean test_mode;
  unsigned char test_mode_start_board;
  // TODO: two places outside of the editor currently require access to this.
  boolean mask_midchars;

  // Network layer options
#ifdef CONFIG_NETWORK
  boolean network_enabled;
  enum host_family network_address_family;
  char socks_host[256];
  char socks_username[256];
  char socks_password[256];
  int socks_port;
#endif

#ifdef CONFIG_UPDATER
  boolean updater_enabled;
  int update_host_count;
  const char **update_hosts;
  char update_branch_pin[256];
  int update_auto_check;
#endif
};

/**
 * Used to create arrays containing all allowed values for certain options.
 */
struct config_enum
{
  const char * const key;
  const int value;
};

CORE_LIBSPEC struct config_info *get_config(void);
CORE_LIBSPEC void default_config(void);
CORE_LIBSPEC void set_config_from_file(enum config_type type,
 const char *conf_file_name);
CORE_LIBSPEC void set_config_from_command_line(int *argc, char *argv[]);
CORE_LIBSPEC void set_config_startup_path_and_file(const char *path);
CORE_LIBSPEC void free_config(void);

typedef boolean (*find_change_option)(void *conf, const char *name,
 const char *value, const char *extended_data);

#ifdef CONFIG_EDITOR

CORE_LIBSPEC void register_config(enum config_type type, void *conf,
 find_change_option handler);

CORE_LIBSPEC boolean config_int(int *dest, const char *value, int min, int max);
CORE_LIBSPEC boolean _config_enum(int *dest, const char *value,
 const struct config_enum *allowed, size_t num);
CORE_LIBSPEC boolean config_boolean(boolean *dest, const char *value);
CORE_LIBSPEC boolean _config_string(char *dest, size_t dest_len, const char *value);

#define config_enum(d, v, a) _config_enum(d, v, a, ARRAY_SIZE(a))
#define config_string(d, v) _config_string(d, ARRAY_SIZE(d), v)

#endif // CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_CONFIGURE_H */
