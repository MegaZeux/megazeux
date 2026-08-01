/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

// Config file options, which can be given either through config.txt
// or at the command line.

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "configure.h"
#include "counter.h"
#include "rasm.h"
#include "util.h"
#include "io/fsafeopen.h"
#include "io/path.h"
#include "io/vio.h"
#include "event/event.h"
#include "platform/log.h"

#ifdef CONFIG_SDL
#include "platform/sdl/SDLmzx.h"
#endif

#define MAX_INCLUDE_DEPTH 16
#define MAX_CONFIG_REGISTERED 2

#if defined(_WIN32) || defined(CONFIG_DJGPP)
/* Do not hang attempting to include devices like COM1, etc. */
#define CONF_SAFETY_CHECK_FAILURE(p) path_safety_check((p), PATH_SAFE_DOS_DEVICE)
#else
#define CONF_SAFETY_CHECK_FAILURE(p) false
#endif

// Arch-specific config.
#ifdef _WIN32
#define DEFAULT_SDL_RENDER_DRIVER "direct3d"
#define UPDATE_AUTO_CHECK_DEFAULT UPDATE_AUTO_CHECK_SILENT
#endif

#ifdef __APPLE__
// Regular fullscreen behaves badly in conjunction with the maximized
// fullscreen behavior in Mac OS X Lion and later, and can lock the
// computer on a black screen in Monterey.
#define FULLSCREEN_WINDOWED_DEFAULT true
// TODO: this should be on by default, as Option is closer to AltGr than Alt.
//#define ALT_IS_ALTGR_DEFAULT true
#endif

#ifdef CONFIG_NDS
#define VIDEO_OUTPUT_DEFAULT "nds"
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_DREAMCAST
#define VIDEO_OUTPUT_DEFAULT "dreamcast"
#define AUDIO_BUFFER_SAMPLES 2048 // the value KOS seems to like best
#define AUDIO_SAMPLE_RATE 44100
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_GP2X
#define VIDEO_OUTPUT_DEFAULT "gp2x"
#define AUDIO_BUFFER_SAMPLES 128
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_PSP
#define FULLSCREEN_WIDTH_DEFAULT 640
#define FULLSCREEN_HEIGHT_DEFAULT 363
#define FORCE_BPP_DEFAULT 8
#define FULLSCREEN_DEFAULT 1
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_PSVITA
#define VIDEO_OUTPUT_DEFAULT "softscale"
#define FULLSCREEN_WIDTH_DEFAULT 960
#define FULLSCREEN_HEIGHT_DEFAULT 544
#define FULLSCREEN_DEFAULT 1
#define VFS_ENABLE_DEFAULT true
#define VFS_MAX_CACHE_SIZE_DEFAULT (1 << 27) /* 128 MiB */
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_WII
#define AUDIO_SAMPLE_RATE 48000
#define FULLSCREEN_DEFAULT 1
#define GL_VSYNC_DEFAULT 1
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
#define SAVE_SLOTS_DEFAULT true
#ifdef CONFIG_SDL
#define VIDEO_OUTPUT_DEFAULT "software"
#define FULLSCREEN_WIDTH_DEFAULT 640
#define FULLSCREEN_HEIGHT_DEFAULT 480
#define FORCE_BPP_DEFAULT 16
#endif /* CONFIG_SDL */
#endif

#ifdef CONFIG_3DS
#ifdef CONFIG_SDL
#define VIDEO_OUTPUT_DEFAULT "gp2x"
#define FULLSCREEN_WIDTH_DEFAULT 400
#define FULLSCREEN_HEIGHT_DEFAULT 240
#define FORCE_BPP_DEFAULT 16
#else
#define VIDEO_OUTPUT_DEFAULT "3ds"
#endif
#define VIDEO_RATIO_DEFAULT RATIO_CLASSIC_4_3
#define VFS_ENABLE_DEFAULT true
#define VFS_MAX_CACHE_SIZE_DEFAULT (1 << 25) /* 32 MiB */
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_WIIU
#define FULLSCREEN_DEFAULT 1
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef CONFIG_SWITCH
// Switch SDL needs this resolution or else weird things start happening...
#define FULLSCREEN_WIDTH_DEFAULT 1920
#define FULLSCREEN_HEIGHT_DEFAULT 1080
#define FULLSCREEN_DEFAULT 1
#define SAVE_SLOTS_DEFAULT true
#endif

#ifdef ANDROID
#define FULLSCREEN_DEFAULT 1
#endif

#ifdef __EMSCRIPTEN__
#define AUDIO_SAMPLE_RATE 48000
#endif

#ifdef CONFIG_DJGPP
#define RESAMPLE_MODE_DEFAULT RESAMPLE_MODE_NONE
#define MOD_RESAMPLE_MODE_DEFAULT RESAMPLE_MODE_LINEAR
#define FULLSCREEN_DEFAULT 1
// Uncomment if Ogg Vorbis files need to be forced into memory.
//#define VFS_ENABLE_DEFAULT true
//#define VFS_ENABLE_AUTO_CACHE_DEFAULT false
#endif

// End arch-specific config.

#ifndef FORCE_BPP_DEFAULT
#define FORCE_BPP_DEFAULT BPP_AUTO
#endif

#ifndef GL_VSYNC_DEFAULT
#define GL_VSYNC_DEFAULT 0
#endif

#ifndef VIDEO_OUTPUT_DEFAULT
#if defined(CONFIG_RENDER_GL_PROGRAM)
#define VIDEO_OUTPUT_DEFAULT "auto_glsl"
#elif defined(CONFIG_RENDER_SOFTSCALE)
#define VIDEO_OUTPUT_DEFAULT "softscale"
#else
#define VIDEO_OUTPUT_DEFAULT "software"
#endif
#endif

#ifndef DEFAULT_SDL_RENDER_DRIVER
#define DEFAULT_SDL_RENDER_DRIVER ""
#endif

#ifndef AUDIO_BUFFER_SAMPLES
#define AUDIO_BUFFER_SAMPLES 1024
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

#ifndef AUDIO_OUTPUT_CHANNELS
#define AUDIO_OUTPUT_CHANNELS 2
#endif

#ifndef RESAMPLE_MODE_DEFAULT
#define RESAMPLE_MODE_DEFAULT RESAMPLE_MODE_LINEAR
#endif

#ifndef MOD_RESAMPLE_MODE_DEFAULT
#define MOD_RESAMPLE_MODE_DEFAULT RESAMPLE_MODE_CUBIC
#endif

#ifndef FULLSCREEN_WIDTH_DEFAULT
#define FULLSCREEN_WIDTH_DEFAULT -1
#endif

#ifndef FULLSCREEN_HEIGHT_DEFAULT
#define FULLSCREEN_HEIGHT_DEFAULT -1
#endif

#ifndef FULLSCREEN_DEFAULT
#define FULLSCREEN_DEFAULT 0
#endif

#ifndef FULLSCREEN_WINDOWED_DEFAULT
#define FULLSCREEN_WINDOWED_DEFAULT false
#endif

#ifndef VIDEO_RATIO_DEFAULT
#define VIDEO_RATIO_DEFAULT RATIO_MODERN_64_35
#endif

#ifndef ALT_IS_ALTGR_DEFAULT
#define ALT_IS_ALTGR_DEFAULT false
#endif

#ifndef VFS_ENABLE_DEFAULT
#define VFS_ENABLE_DEFAULT false
#endif

#ifndef VFS_ENABLE_AUTO_CACHE_DEFAULT
#define VFS_ENABLE_AUTO_CACHE_DEFAULT true
#endif

#ifndef VFS_MAX_CACHE_SIZE_DEFAULT
#define VFS_MAX_CACHE_SIZE_DEFAULT (1 << 24)
#endif

#ifndef VFS_MAX_CACHE_FILE_SIZE_DEFAULT
#define VFS_MAX_CACHE_FILE_SIZE_DEFAULT (VFS_MAX_CACHE_SIZE_DEFAULT >> 2)
#endif

#ifndef AUTO_DECRYPT_WORLDS
#define AUTO_DECRYPT_WORLDS true
#endif

#ifndef SAVE_SLOTS_DEFAULT
#define SAVE_SLOTS_DEFAULT false
#endif

#ifdef CONFIG_UPDATER
#ifndef MAX_UPDATE_HOSTS
#define MAX_UPDATE_HOSTS 16
#endif

#ifndef UPDATE_HOST_COUNT
#define UPDATE_HOST_COUNT 4
#endif

static const char *default_update_hosts[] =
{
  "updates.digitalmzx.com",
  "updates.digitalmzx.net",
  "updates.megazeux.org",
  "updates.megazeux.net",
};

#ifndef UPDATE_BRANCH_PIN
#ifdef CONFIG_DEBYTECODE
#define UPDATE_BRANCH_PIN "Debytecode"
#else
#define UPDATE_BRANCH_PIN "Stable"
#endif
#endif

#ifndef UPDATE_AUTO_CHECK_DEFAULT
#define UPDATE_AUTO_CHECK_DEFAULT UPDATE_AUTO_CHECK_OFF
#endif

#endif /* CONFIG_UPDATER */

static enum config_type current_config_type;
static int current_include_depth = 0;

static struct config_info user_conf;

static const struct config_info user_conf_default =
{
  // Video options
  FULLSCREEN_DEFAULT,           // fullscreen
  FULLSCREEN_WINDOWED_DEFAULT,  // fullscreen_windowed
  FULLSCREEN_WIDTH_DEFAULT,     // resolution_width
  FULLSCREEN_HEIGHT_DEFAULT,    // resolution_height
  640,                          // window_width
  350,                          // window_height
  1,                            // allow_resize
  VIDEO_OUTPUT_DEFAULT,         // video_output
  FORCE_BPP_DEFAULT,            // force_bpp
  VIDEO_RATIO_DEFAULT,          // video_ratio
  CONFIG_GL_FILTER_LINEAR,      // opengl filter method
  GL_VSYNC_DEFAULT,             // opengl vsync mode
  "",                           // opengl default scaling shader
  DEFAULT_SDL_RENDER_DRIVER,    // sdl_render_driver
  CURSOR_MODE_HINT,             // cursor_hint_mode
  SCREENSAVER_ENABLE,           // disable_screensaver
  true,                         // allow screenshots

  // Audio options
  "",                           // audio_driver
  AUDIO_SAMPLE_RATE,            // audio_sample_rate
  AUDIO_BUFFER_SAMPLES,         // audio_buffer_samples
  AUDIO_OUTPUT_CHANNELS,        // audio_output_channels
  0,                            // oversampling_on
  RESAMPLE_MODE_DEFAULT,        // resample_mode
  MOD_RESAMPLE_MODE_DEFAULT,    // module_resample_mode
  -1,                           // max_simultaneous_samples
  8,                            // music_volume
  8,                            // sam_volume
  8,                            // pc_speaker_volume
  true,                         // music_on
  true,                         // pc_speaker_on
  false,                        // audio_pc_speaker_use_hardware
  false,                        // audio_opl_use_hardware
  0x388,                        // audio_opl_port

  // Event options
  true,                         // allow_gamepad
  false,                        // pause_on_unfocus
  ALT_IS_ALTGR_DEFAULT,         // key_left_alt_is_altgr
  ALT_IS_ALTGR_DEFAULT,         // key_right_alt_is_altgr
  1,                            // num_buffered_events

  // Virtual filesystem options
  VFS_ENABLE_DEFAULT,           // vfs_enable
  VFS_ENABLE_AUTO_CACHE_DEFAULT,// vfs_enable_auto_cache
  VFS_MAX_CACHE_SIZE_DEFAULT,   // vfs_max_cache_size
  VFS_MAX_CACHE_FILE_SIZE_DEFAULT, // vfs_max_cache_file_size

  // Game options
  "",                           // startup_path
  "caverns.mzx",                // startup_file
  "saved.sav",                  // default_save_name
  4,                            // mzx_speed
  ALLOW_CHEATS_NEVER,           // allow_cheats
  AUTO_DECRYPT_WORLDS,          // auto_decrypt_worlds
  false,                        // startup_editor
  false,                        // standalone_mode
  false,                        // no_titlescreen
  SYSTEM_MOUSE_OFF,             // system_mouse
  false,                        // grab_mouse
  SAVE_SLOTS_DEFAULT,           // save_slots
  "%w.",                        // save_slots_name
  ".sav",                       // save_slots_ext

  // Editor options
  false,                        // test_mode
  NO_BOARD,                     // test_mode_start_board
  true,                         // mask_midchars

#ifdef CONFIG_NETWORK
  true,                         // network_enabled
  HOST_FAMILY_ANY,              // network_address_family
  "",                           // socks_host
  "",                           // socks_username
  "",                           // socks_password
  1080,                         // socks_port
#endif
#if defined(CONFIG_UPDATER)
  true,                         // updater_enabled
  UPDATE_HOST_COUNT,            // update_host_count
  default_update_hosts,         // update_hosts
  UPDATE_BRANCH_PIN,            // update_branch_pin
  UPDATE_AUTO_CHECK_DEFAULT,    // update_auto_check
#endif /* CONFIG_UPDATER */
};

typedef void (*config_function)(struct config_info *conf, const char *name,
 const char *value, const char *extended_data);

struct config_entry
{
  const char *option_name;
  config_function change_option;
  boolean allow_in_game_config;
};

static const struct config_enum boolean_values[] =
{
  { "0", false },
  { "1", true }
};

static const struct config_enum allow_cheats_values[] =
{
  { "0", ALLOW_CHEATS_NEVER },
  { "1", ALLOW_CHEATS_ALWAYS },
  { "never", ALLOW_CHEATS_NEVER },
  { "always", ALLOW_CHEATS_ALWAYS },
  { "mzxrun", ALLOW_CHEATS_MZXRUN }
};

static const struct config_enum force_bpp_values[] =
{
  { "0", BPP_AUTO },
  { "8", 8 },
  { "16", 16 },
  { "32", 32 },
  { "auto", BPP_AUTO },
};

static const struct config_enum gl_filter_method_values[] =
{
  { "nearest", CONFIG_GL_FILTER_NEAREST },
  { "linear", CONFIG_GL_FILTER_LINEAR }
};

static const struct config_enum gl_vsync_values[] =
{
  { "-1", -1 },
  { "0", 0 },
  { "1", 1 },
  { "default", -1 }
};

static const struct config_enum module_resample_mode_values[] =
{
  { "none", RESAMPLE_MODE_NONE },
  { "linear", RESAMPLE_MODE_LINEAR },
  { "cubic", RESAMPLE_MODE_CUBIC },
  { "fir", RESAMPLE_MODE_FIR }
};

static const struct config_enum resample_mode_values[] =
{
  { "none", RESAMPLE_MODE_NONE },
  { "linear", RESAMPLE_MODE_LINEAR },
  { "cubic", RESAMPLE_MODE_CUBIC }
};

static const struct config_enum cursor_hint_type_values[] =
{
  { "0", CURSOR_MODE_INVISIBLE },
  { "1", CURSOR_MODE_HINT },
  { "off", CURSOR_MODE_INVISIBLE },
  { "hidden", CURSOR_MODE_HINT },
  { "underline", CURSOR_MODE_UNDERLINE },
  { "solid", CURSOR_MODE_SOLID },
};

static const struct config_enum system_mouse_values[] =
{
  { "0", SYSTEM_MOUSE_OFF },
  { "1", SYSTEM_MOUSE_ON },
  { "off", SYSTEM_MOUSE_OFF },
  { "on", SYSTEM_MOUSE_ON },
  { "only", SYSTEM_MOUSE_HIDE_SOFTWARE_MOUSE },
};

static const struct config_enum screensaver_disable_values[] =
{
  { "0", SCREENSAVER_ENABLE },
  { "1", SCREENSAVER_DISABLE },
  //{ "ingame", SCREENSAVER_DISABLE_IN_GAME }
};

#ifdef CONFIG_NETWORK
static const struct config_enum network_address_family_values[] =
{
  { "0", HOST_FAMILY_ANY },
  { "any", HOST_FAMILY_ANY },
  { "ipv4", HOST_FAMILY_IPV4 },
  { "ipv6", HOST_FAMILY_IPV6 }
};
#endif

#ifdef CONFIG_UPDATER
static const struct config_enum update_auto_check_values[] =
{
  { "0", UPDATE_AUTO_CHECK_OFF },
  { "1", UPDATE_AUTO_CHECK_ON },
  { "off", UPDATE_AUTO_CHECK_OFF },
  { "on", UPDATE_AUTO_CHECK_ON },
  { "silent", UPDATE_AUTO_CHECK_SILENT }
};
#endif

static const struct config_enum video_ratio_values[] =
{
  { "classic", RATIO_CLASSIC_4_3 },
  { "modern", RATIO_MODERN_64_35 },
  { "stretch", RATIO_STRETCH }
};

struct config_registry_data
{
  void *conf;
  find_change_option handler;
};

struct config_registry_entry
{
  int num_registered;
  struct config_registry_data registered[MAX_CONFIG_REGISTERED];
};

static struct config_registry_entry config_registry[NUM_CONFIG_TYPES];

__editor_maybe_static
void register_config(enum config_type type, void *conf, find_change_option handler)
{
  if(type < NUM_CONFIG_TYPES)
  {
    struct config_registry_entry *e = &config_registry[type];
    if(e->num_registered < MAX_CONFIG_REGISTERED)
    {
      e->registered[e->num_registered].conf = conf;
      e->registered[e->num_registered].handler = handler;
      e->num_registered++;
    }
  }
}

__editor_maybe_static
boolean config_int(int *dest, const char *value, int min, int max)
{
  int result;
  int n;

  if(sscanf(value, "%d%n", &result, &n) != 1 || value[n] != 0)
    return false;

  if(result < min || result > max)
    return false;

  *dest = result;
  return true;
}

static boolean config_long_long(long long *dest, const char *value,
 long long min, long long max)
{
  long long result;
  int n;

  if(sscanf(value, "%lld%n", &result, &n) != 1 || value[n] != 0)
    return false;

  if(result < min || result > max)
    return false;

  *dest = result;
  return true;
}

#define config_enum(d, v, a) _config_enum(d, v, a, ARRAY_SIZE(a))

__editor_maybe_static
boolean _config_enum(int *dest, const char *value,
 const struct config_enum *allowed, size_t num)
{
  size_t i;
  for(i = 0; i < num; i++)
  {
    if(!strcasecmp(value, allowed[i].key))
    {
      *dest = allowed[i].value;
      return true;
    }
  }
  return false;
}

__editor_maybe_static
boolean config_boolean(boolean *dest, const char *value)
{
  int result;
  if(config_enum(&result, value, boolean_values))
  {
    *dest = (boolean)result;
    return true;
  }
  return false;
}

#define config_string(d, v) _config_string(d, ARRAY_SIZE(d), v)

__editor_maybe_static
boolean _config_string(char *dest, size_t dest_len, const char *value)
{
  snprintf(dest, dest_len, "%s", value);
  dest[dest_len - 1] = '\0';
  return true;
}

#ifdef CONFIG_NETWORK

static void config_set_network_enabled(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->network_enabled, value);
}

static void config_set_network_address_family(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, network_address_family_values))
    conf->network_address_family = (enum host_family)result;
}

static void config_set_socks_host(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->socks_host, value);
}

static void config_set_socks_port(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 65535))
    conf->socks_port = result;
}

static void config_set_socks_username(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->socks_username, value);
}

static void config_set_socks_password(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->socks_password, value);
}

#endif // CONFIG_NETWORK

#ifdef CONFIG_UPDATER

static void config_update_host(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  if(!conf->update_hosts || conf->update_hosts == default_update_hosts)
  {
    conf->update_hosts = (const char **)ccalloc(MAX_UPDATE_HOSTS, sizeof(char *));
    conf->update_host_count = 0;
  }

  if(conf->update_host_count < MAX_UPDATE_HOSTS)
  {
    size_t size = strlen(value) + 1;
    char *host = (char *)cmalloc(size);
    memcpy(host, value, size);
    conf->update_hosts[conf->update_host_count] = host;
    conf->update_host_count++;
  }
}

static void config_update_branch_pin(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->update_branch_pin, value);
}

static void config_update_auto_check(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, update_auto_check_values))
    conf->update_auto_check = result;
}

static void config_set_updater_enabled(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->updater_enabled, value);
}

#endif // CONFIG_UPDATER

static void config_set_audio_buffer(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, INT_MAX))
    conf->audio_buffer_samples = result;
}

static void config_set_audio_channels(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 2))
    conf->audio_output_channels = result;
}

static void config_set_resolution(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int width;
  int height;
  int n;

  if(sscanf(value, "%d, %d%n", &width, &height, &n) != 2 || value[n] ||
   width < -1 || height < -1)
    return;

  conf->resolution_width = width;
  conf->resolution_height = height;
}

static void config_set_dialog_cursor_hints(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, cursor_hint_type_values))
    conf->cursor_hint_mode = (enum cursor_mode_types)result;
}

static void config_set_fullscreen(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fullscreen, value);
}

static void config_set_fullscreen_windowed(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fullscreen_windowed, value);
}

static void config_set_audio_driver(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->audio_driver, value);
}

static void config_set_music(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->music_on, value);
}

static void config_set_mod_volume(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 10))
    conf->music_volume = result;
}

static void config_set_mzx_speed(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 16))
    conf->mzx_speed = result;
}

static void config_set_pc_speaker(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->pc_speaker_on, value);
}

static void config_set_sam_volume(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 10))
    conf->sam_volume = result;
}

static void config_set_audio_pc_speaker_use_hardware(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->audio_pc_speaker_use_hardware, value);
}

static void config_set_audio_opl_use_hardware(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->audio_opl_use_hardware, value);
}

static void config_set_audio_opl_port(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 65535))
    conf->audio_opl_port = result;
}

static void config_save_file(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  if(CONF_SAFETY_CHECK_FAILURE(value))
    return;

  config_string(conf->default_save_name, value);
}

static void config_startup_file(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  if(CONF_SAFETY_CHECK_FAILURE(value))
    return;

  // If no startup_path has been set, set both startup_path and startup_file
  // from this path. Otherwise, set startup_file and discard the directory
  // portion of the path.
  if(!conf->startup_path[0])
  {
    struct stat stat_info;

    path_get_directory_and_filename(
      conf->startup_path, sizeof(conf->startup_path),
      conf->startup_file, sizeof(conf->startup_file),
      value
    );

    // Make sure the startup path actually exists.
    if(conf->startup_path[0] &&
     (vstat(conf->startup_path, &stat_info) < 0 || !S_ISDIR(stat_info.st_mode)))
      conf->startup_path[0] = '\0';
  }
  else
    path_get_filename(conf->startup_file, sizeof(conf->startup_file), value);
}

static void config_startup_path(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  struct stat stat_info;
  if(vstat(value, &stat_info) || !S_ISDIR(stat_info.st_mode))
    return;

  snprintf(conf->startup_path, 256, "%s", value);
  conf->startup_path[255] = '\0';
}

static void config_system_mouse(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, system_mouse_values))
    conf->system_mouse = (enum system_mouse_type)result;
}

static void config_grab_mouse(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->grab_mouse, value);
}

static void config_disable_screensaver(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, screensaver_disable_values))
    conf->disable_screensaver = (enum screensaver_disable_mode)result;
}

static void config_save_slots(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->save_slots, value);
}

static void config_save_slots_name(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->save_slots_name, value);
}

static void config_save_slots_ext(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->save_slots_ext, value);
}

static void config_enable_oversampling(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  boolean result;
  if(config_boolean(&result, value))
    conf->oversampling_on = result;
}

static void config_resample_mode(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, resample_mode_values))
    conf->resample_mode = (enum resample_mode)result;
}

static void config_mod_resample_mode(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, module_resample_mode_values))
    conf->module_resample_mode = (enum resample_mode)result;
}

#define JOY_ENUM "%15[0-9A-Za-z_]"

static boolean joy_num(const char **name, unsigned int *first, unsigned int *last)
{
  int next = 0;
  if(!strncasecmp(*name, "joy", 3))
  {
    *name += 3;

    if(sscanf(*name, "[%u,%u]%n", first, last, &next) == 2)
    {
      *name += next;
      return true;
    }
    else

    if(sscanf(*name, "%u%n", first, &next) == 1)
    {
      *last = *first;
      *name += next;
      return true;
    }
  }
  return false;
}

static boolean joy_button_name(const char *rest, unsigned int *button_num)
{
  int next = 0;
  if(sscanf(rest, "button%u%n", button_num, &next) != 1 || (rest[next] != 0))
    return false;
  return true;
}

static boolean joy_axis_name(const char *rest, unsigned int *axis_num)
{
  int next = 0;
  if(sscanf(rest, "axis%u%n", axis_num, &next) != 1 || (rest[next] != 0))
    return false;
  return true;
}

static boolean joy_axis_value(const char *value, char min[16], char max[16])
{
  int next = 0;
  if(sscanf(value, JOY_ENUM ", " JOY_ENUM "%n", min, max, &next) != 2
   || (value[next] != 0))
    return false;
  return true;
}

static boolean joy_hat_value(const char *value, char up[16], char down[16],
 char left[16], char right[16])
{
  int next = 0;
  if(sscanf(value, JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM ", " JOY_ENUM "%n",
   up, down, left, right, &next) != 4 || (value[next] != 0))
    return false;
  return true;
}

static void joy_axis_set(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int first, last, axis;
  char min[16], max[16];

  if(joy_num(&name, &first, &last) && joy_axis_name(name, &axis) &&
   joy_axis_value(value, min, max))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    boolean is_startup = (current_config_type == SYSTEM_CNF);
    joystick_map_axis(first - 1, last - 1, axis - 1, min, max, is_startup);
  }
}

static void joy_button_set(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int first, last, button;

  if(joy_num(&name, &first, &last) && joy_button_name(name, &button))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    boolean is_startup = (current_config_type == SYSTEM_CNF);
    joystick_map_button(first - 1, last - 1, button - 1, value, is_startup);
  }
}

static void joy_hat_set(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int first, last;
  char up[16], down[16], left[16], right[16];

  if(joy_num(&name, &first, &last) && !strcasecmp(name, "hat") &&
   joy_hat_value(value, up, down, left, right))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    boolean is_startup = (current_config_type == SYSTEM_CNF);
    joystick_map_hat(first - 1, last - 1, up, down, left, right, is_startup);
  }
}

static void joy_action_set(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int first, last;

  if(joy_num(&name, &first, &last) && (*name == '.'))
  {
    // Right now do a global binding at startup and a game binding otherwise.
    boolean is_startup = (current_config_type == SYSTEM_CNF);
    joystick_map_action(first - 1, last - 1, name + 1, value, is_startup);
  }
}

static void config_set_joy_axis_threshold(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int threshold;
  int read = 0;

  if(sscanf(value, "%u%n", &threshold, &read) != 1 || (value[read] != 0) ||
   threshold > 32767)
    return;

  joystick_set_axis_threshold(threshold);
}

#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
#define GAMEPAD_ENUM "%15[-+0-9A-Za-z_]"

static void config_gamepad_set(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  char gamepad_sym[16];
  char key[16];
  int read = 0;

  // Option may be either "gamepad.[enum]" or "gamecontroller.[enum]".
  if(!strncmp(name, "gamepad", 7))
    name += 7;
  else
  if(!strncmp(name, "gamecontroller", 14))
    name += 14;

  if(sscanf(name, "." GAMEPAD_ENUM "%n", gamepad_sym, &read) != 1
   || (name[read] != 0))
    return;

  read = 0;
  if(sscanf(value, JOY_ENUM "%n", key, &read) != 1 || (value[read] != 0))
    return;

  gamepad_map_sym(gamepad_sym, key);
}

static void config_gamepad_add(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  gamepad_add_mapping(value);
}

static void config_gamepad_enable(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->allow_gamepad, value);
}

#endif // SDL_VERSION_ATLEAST(2,0,0)
#endif // CONFIG_SDL

static void pause_on_unfocus(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->pause_on_unfocus, value);
}

static void config_set_left_alt_is_altgr(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->key_left_alt_is_altgr, value);
}

static void config_set_right_alt_is_altgr(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->key_right_alt_is_altgr, value);
}

static void include_config(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  if(current_include_depth < MAX_INCLUDE_DEPTH)
  {
    current_include_depth++;

    // The format "include FILENAME.EXT" is condensed into the name.
    // The format "include=FILENAME.EXT" uses the value instead.
    if(name[7])
    {
      set_config_from_file(current_config_type, name + 7);
    }
    else
      set_config_from_file(current_config_type, value);

    current_include_depth--;
  }
  else
    warn("Failed to include '%s' (maximum recursion depth exceeded)\n", name + 7);
}

static void config_set_pcs_volume(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 10))
    conf->pc_speaker_volume = result;
}

static void config_mask_midchars(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  // TODO move to editor config when non-editor code stops relying on it
  config_boolean(&conf->mask_midchars, value);
}

static void config_set_audio_freq(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, INT_MAX))
    conf->audio_sample_rate = result;
}

static void config_force_bpp(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, force_bpp_values))
    conf->force_bpp = result;
}

static void config_window_resolution(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int width;
  int height;
  int n;

  if(sscanf(value, "%d, %d%n", &width, &height, &n) != 2 || value[n] ||
   width < -1 || height < -1)
    return;

  conf->window_width = width;
  conf->window_height = height;
}

static void config_set_video_output(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->video_output, value);
}

static void config_enable_resizing(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->allow_resize, value);
}

static void config_set_gl_filter_method(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, gl_filter_method_values))
    conf->gl_filter_method = (enum gl_filter_type)result;
}

static void config_set_gl_scaling_shader(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->gl_scaling_shader, value);
}

static void config_gl_vsync(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, gl_vsync_values))
    conf->gl_vsync = result;
}

static void config_sdl_render_driver(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->sdl_render_driver, value);
}

static void config_set_allow_screenshots(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->allow_screenshots, value);
}

static void config_startup_editor(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->startup_editor, value);
}

static void config_standalone_mode(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->standalone_mode, value);
}

static void config_no_titlescreen(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->no_titlescreen, value);
}

static void config_set_allow_cheats(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, allow_cheats_values))
    conf->allow_cheats = (enum allow_cheats_type)result;
}

static void config_set_auto_decrypt_worlds(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->auto_decrypt_worlds, value);
}

static void config_set_video_ratio(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, video_ratio_values))
    conf->video_ratio = (enum ratio_type)result;
}

static void config_set_num_buffered_events(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 256))
    conf->num_buffered_events = result;
}

static void config_max_simultaneous_samples(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, -1, INT_MAX))
    conf->max_simultaneous_samples = result;
}

static void config_test_mode(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->test_mode, value);
}

static void config_test_mode_start_board(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, MAX_BOARDS - 1))
    conf->test_mode_start_board = result;
}

static void config_set_vfs_enable(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->vfs_enable, value);
}

static void config_set_vfs_enable_auto_cache(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->vfs_enable_auto_cache, value);
}

static void config_set_vfs_max_cache_size(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  long long result;
  if(config_long_long(&result, value, 0, LLONG_MAX))
    conf->vfs_max_cache_size = result;
}

static void config_set_vfs_max_cache_file_size(struct config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  long long result;
  if(config_long_long(&result, value, 0, LLONG_MAX))
    conf->vfs_max_cache_file_size = result;
}

/* NOTE: This is searched as a binary tree, the nodes must be
 *       sorted alphabetically, or they risk being ignored.
 */
static const struct config_entry config_options[] =
{
  { "allow_cheats", config_set_allow_cheats, false },
  { "allow_screenshots", config_set_allow_screenshots, false },
  { "audio_buffer", config_set_audio_buffer, false },
  { "audio_buffer_samples", config_set_audio_buffer, false },
  { "audio_driver", config_set_audio_driver, false },
  { "audio_opl_port", config_set_audio_opl_port, false },
  { "audio_opl_use_hardware", config_set_audio_opl_use_hardware, false },
  { "audio_output_channels", config_set_audio_channels, false },
  { "audio_pc_speaker_use_hardware", config_set_audio_pc_speaker_use_hardware, false },
  { "audio_sample_rate", config_set_audio_freq, false },
  { "auto_decrypt_worlds", config_set_auto_decrypt_worlds, false },
  { "dialog_cursor_hints", config_set_dialog_cursor_hints, false },
  { "disable_screensaver", config_disable_screensaver, false },
  { "enable_oversampling", config_enable_oversampling, false },
  { "enable_resizing", config_enable_resizing, false },
  { "force_bpp", config_force_bpp, false },
  { "fullscreen", config_set_fullscreen, false },
  { "fullscreen_resolution", config_set_resolution, false },
  { "fullscreen_windowed", config_set_fullscreen_windowed, false },
#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
  { "gamecontroller.*", config_gamepad_set, false },
  { "gamecontroller_add", config_gamepad_add, false },
  { "gamecontroller_enable", config_gamepad_enable, false },
  { "gamepad.*", config_gamepad_set, false },
  { "gamepad_add", config_gamepad_add, false },
  { "gamepad_enable", config_gamepad_enable, false },
#endif
#endif
  { "gl_filter_method", config_set_gl_filter_method, false },
  { "gl_scaling_shader", config_set_gl_scaling_shader, true },
  { "gl_vsync", config_gl_vsync, false },
  { "grab_mouse", config_grab_mouse, false },
  { "include*", include_config, true },
  { "joy!.*", joy_action_set, true },
  { "joy!axis!", joy_axis_set, true },
  { "joy!button!", joy_button_set, true },
  { "joy!hat", joy_hat_set, true },
  { "joy[!,!].*", joy_action_set, true },
  { "joy[!,!]axis!", joy_axis_set, true },
  { "joy[!,!]button!", joy_button_set, true },
  { "joy[!,!]hat", joy_hat_set, true },
  { "joy_axis_threshold", config_set_joy_axis_threshold, false },
  { "key_left_alt_is_altgr", config_set_left_alt_is_altgr, false },
  { "key_right_alt_is_altgr", config_set_right_alt_is_altgr, false },
  { "mask_midchars", config_mask_midchars, false },
  { "max_simultaneous_samples", config_max_simultaneous_samples, false },
  { "modplug_resample_mode", config_mod_resample_mode, false },
  { "module_resample_mode", config_mod_resample_mode, false },
  { "music_on", config_set_music, false },
  { "music_volume", config_set_mod_volume, false },
  { "mzx_speed", config_set_mzx_speed, true },
#ifdef CONFIG_NETWORK
  { "network_address_family", config_set_network_address_family, false },
  { "network_enabled", config_set_network_enabled, false },
#endif
  { "no_titlescreen", config_no_titlescreen, false },
  { "num_buffered_events", config_set_num_buffered_events, false },
  { "pause_on_unfocus", pause_on_unfocus, false },
  { "pc_speaker_on", config_set_pc_speaker, false },
  { "pc_speaker_volume", config_set_pcs_volume, false },
  { "resample_mode", config_resample_mode, false },
  { "sample_volume", config_set_sam_volume, false },
  { "save_file", config_save_file, false },
  { "save_slots", config_save_slots, false },
  { "save_slots_ext", config_save_slots_ext, false },
  { "save_slots_name", config_save_slots_name, false },
  { "sdl_render_driver", config_sdl_render_driver, false },
#ifdef CONFIG_NETWORK
  { "socks_host", config_set_socks_host, false },
  { "socks_password", config_set_socks_password, false },
  { "socks_port", config_set_socks_port, false },
  { "socks_username", config_set_socks_username, false },
#endif
  { "standalone_mode", config_standalone_mode, false },
  { "startup_editor", config_startup_editor, false },
  { "startup_file", config_startup_file, false },
  { "startup_path", config_startup_path, false },
  { "system_mouse", config_system_mouse, false },
  { "test_mode", config_test_mode, false },
  { "test_mode_start_board", config_test_mode_start_board, false },
#ifdef CONFIG_UPDATER
  { "updater_enabled", config_set_updater_enabled, false },
  { "update_auto_check", config_update_auto_check, false },
  { "update_branch_pin", config_update_branch_pin, false },
  { "update_host", config_update_host, false },
#endif
  { "vfs_enable", config_set_vfs_enable, false },
  { "vfs_enable_auto_cache", config_set_vfs_enable_auto_cache, false },
  { "vfs_max_cache_file_size", config_set_vfs_max_cache_file_size, false },
  { "vfs_max_cache_size", config_set_vfs_max_cache_size, false },
  { "video_output", config_set_video_output, false },
  { "video_ratio", config_set_video_ratio, false },
  { "window_resolution", config_window_resolution, false }
};

static const struct config_entry *find_option(const char *name,
 const struct config_entry options[], int num_options)
{
  int cmpval, top = num_options - 1, middle, bottom = 0;
  const struct config_entry *base = options;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = match_function_counter(name, (base[middle]).option_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

static boolean config_change_option(void *_conf, const char *name,
 const char *value, const char *extended_data)
{
  const struct config_entry *current_option = find_option(name,
   config_options, ARRAY_SIZE(config_options));

  if(current_option)
  {
    if(current_option->allow_in_game_config || (current_config_type == SYSTEM_CNF))
    {
      struct config_info *conf = (struct config_info *)_conf;
      current_option->change_option(conf, name, value, extended_data);
      return true;
    }
  }
  return false;
}

#define LINE_BUFFER_SIZE 512

void set_config_from_file(enum config_type type, const char *conf_file_name)
{
  char current_char, *input_position, *output_position, *use_extended_buffer;
  int line_size, extended_size, extended_allocate_size = 512;
  char line_buffer_alternate[LINE_BUFFER_SIZE], line_buffer[LINE_BUFFER_SIZE];
  int extended_buffer_offset, peek_char;
  char *extended_buffer;
  char *equals_position, *value;
  char *output_end_position = line_buffer + LINE_BUFFER_SIZE;
  vfile *conf_file;
  int i;

  if(type >= NUM_CONFIG_TYPES)
    return;

  if(CONF_SAFETY_CHECK_FAILURE(conf_file_name))
  {
    warn("Refused to load config file '%s'\n", conf_file_name);
    return;
  }

  conf_file = vfopen_unsafe(conf_file_name, "rb");
  if(!conf_file)
    return;

  extended_buffer = (char *)cmalloc(extended_allocate_size);

  while(vfsafegets(line_buffer_alternate, LINE_BUFFER_SIZE, conf_file))
  {
    if(line_buffer_alternate[0] != '#')
    {
      input_position = line_buffer_alternate;
      output_position = line_buffer;
      equals_position = NULL;

      do
      {
        current_char = *input_position;

        if(!isspace((int)current_char))
        {
          if((current_char == '\\') &&
            (input_position[1] == 's'))
          {
            input_position++;
            current_char = ' ';
          }

          if(output_position < output_end_position)
          {
            if((current_char == '=') && (equals_position == NULL))
              equals_position = output_position;

            *output_position = current_char;
            output_position++;
          }
        }
        input_position++;
      } while(current_char);

      line_buffer[LINE_BUFFER_SIZE - 1] = 0;

      if(equals_position)
      {
        *equals_position = 0;
        value = equals_position + 1;
      }
      else
      {
        value = (char *)"1";
      }

      if(line_buffer[0])
      {
        // There might be extended information too - get it.
        peek_char = vfgetc(conf_file);
        extended_size = 1;
        extended_buffer_offset = 0;
        use_extended_buffer = NULL;
        extended_buffer[0] = '\0';

        while((peek_char == ' ') || (peek_char == '\t'))
        {
          // Extended data line
          use_extended_buffer = extended_buffer;
          if(vfsafegets(line_buffer_alternate, 254, conf_file))
          {
            // Skip any extra whitespace at the start of the line...
            char *line_buffer_pos = line_buffer_alternate;
            while(*line_buffer_pos && isspace((int)*line_buffer_pos))
              line_buffer_pos++;

            line_size = (int)strlen(line_buffer_pos);
            line_buffer_pos[line_size++] = '\n';
            line_buffer_pos[line_size] = '\0';

            extended_size += line_size;
            if(extended_size >= extended_allocate_size)
            {
              extended_allocate_size *= 2;
              extended_buffer = (char *)crealloc(extended_buffer, extended_allocate_size);
              use_extended_buffer = extended_buffer;
            }

            memcpy(extended_buffer + extended_buffer_offset,
             line_buffer_pos, line_size + 1);
            extended_buffer_offset += line_size;
          }

          peek_char = vfgetc(conf_file);
        }
        vungetc(peek_char, conf_file);

        for(i = 0; i < config_registry[type].num_registered; i++)
        {
          struct config_registry_data *d = &config_registry[type].registered[i];
          current_config_type = type;
          if(d->handler(d->conf, line_buffer, value, use_extended_buffer))
            break;
        }
      }
    }
  }

  free(extended_buffer);
  vfclose(conf_file);
}

void set_config_from_command_line(int *argc, char *argv[])
{
  char current_char, *input_position, *output_position;
  char *equals_position, line_buffer[LINE_BUFFER_SIZE], *value;
  char *output_end_position = line_buffer + LINE_BUFFER_SIZE;
  int i = 1;
  int j;
  int k;

  while(i < *argc)
  {
    input_position = argv[i];
    output_position = line_buffer;
    equals_position = NULL;

    do
    {
      current_char = *input_position;

      if((current_char == '\\') &&
       (input_position[1] == 's'))
      {
        input_position++;
        current_char = ' ';
      }

      if(output_position < output_end_position)
      {
        if((current_char == '=') && (equals_position == NULL))
          equals_position = output_position;

        *output_position = current_char;
        output_position++;
      }
      input_position++;
    } while(current_char);

    line_buffer[LINE_BUFFER_SIZE - 1] = 0;

    if(equals_position && line_buffer[0])
    {
      *equals_position = 0;
      value = equals_position + 1;

      for(k = 0; k < config_registry[SYSTEM_CNF].num_registered; k++)
      {
        struct config_registry_data *d = &config_registry[SYSTEM_CNF].registered[k];
        current_config_type = SYSTEM_CNF;
        if(d->handler(d->conf, line_buffer, value, NULL))
        {
          // Found the option; remove it from argv and make sure i stays the same
          for(j = i; j < *argc - 1; j++)
            argv[j] = argv[j + 1];
          (*argc)--;
          i--;
          break;
        }
      }
    }

    i++;
  }
}

/* Set both the startup path and startup file from a string (usually argv[1]).
 * This should be called from the startup directory, as the input path may
 * be relative.
 */
void set_config_startup_path_and_file(const char *path)
{
  char path_backup[sizeof(user_conf.startup_path)];
  memcpy(path_backup, user_conf.startup_path, sizeof(path_backup));

  /* Use the startup_file path splitting logic instead of duplicating it. */
  user_conf.startup_path[0] = '\0';
  config_startup_file(&user_conf, "startup_file", path, NULL);

  /* Restore old path if that didn't add one. */
  if(user_conf.startup_path[0] == '\0')
    memcpy(user_conf.startup_path, path_backup, sizeof(path_backup));
}

struct config_info *get_config(void)
{
  return &user_conf;
}

void default_config(void)
{
  static boolean registered = false;
  memcpy(&user_conf, &user_conf_default, sizeof(struct config_info));

  if(!registered)
  {
    register_config(SYSTEM_CNF, &user_conf, config_change_option);
    register_config(GAME_CNF, &user_conf, config_change_option);
    register_config(GAME_EDITOR_CNF, &user_conf, config_change_option);
    registered = true;
  }
}

void free_config(void)
{
#ifdef CONFIG_UPDATER
  // Custom updater hosts might have been allocated
  if(user_conf.update_hosts != default_update_hosts)
  {
    int i;

    for(i = 0; i < user_conf.update_host_count; i++)
      free((char *)user_conf.update_hosts[i]);

    free(user_conf.update_hosts);
    user_conf.update_hosts = default_update_hosts;
  }
#endif
}
