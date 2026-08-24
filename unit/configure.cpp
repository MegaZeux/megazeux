/* MegaZeux
 *
 * Copyright (C) 2020-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <limits.h>
#include <stdio.h>
#include <limits>

#include "Unit.hpp"

#include "../src/configure.h"
#include "../src/const.h"
#include "../src/util.h"
#include "../src/io/vio.h"

#include "../src/event/event.h"
#include "../src/event/keysym.h"

#ifdef CONFIG_SDL
#include "../src/platform/sdl/SDLmzx.h"
#undef IGNORE /* Windows defines this for some reason... */
#endif

#ifdef CONFIG_EDITOR
#include "../src/editor/configure.h"
#include "../src/editor/robo_ed.h"
#endif

static const int IGNORE = INT_MAX;
static boolean game_allowed;

struct config_test_single
{
  const char * const value;
  int expected;
};

struct config_test_pair
{
  const char * const value;
  int expected_a;
  int expected_b;
};

struct config_test_string
{
  const char * const value;
  const char * const expected;
};

#ifdef CONFIG_EDITOR
struct config_test_board_size_viewport
{
  const char * const setting;
  const char * const value;
  int width;
  int height;
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
};

struct config_test_saved_position
{
  int which;
  const char * const value;
  struct saved_position expected;
};
#endif /* CONFIG_EDITOR */

/**
 * For integer types, use min (if signed) or max (otherwise).
 * Enum types need to be specialized since numeric_limits doesn't work on them.
 */
template<class T>
static constexpr T INVALID()
{
  static_assert(std::numeric_limits<T>::max() > 0, "INVALID<> needs specialization.");
  return std::numeric_limits<T>::is_signed ?
   std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}

template<>
constexpr ratio_type INVALID<ratio_type>()
{
  return NUM_RATIO_TYPES;
}

template<>
constexpr gl_filter_type INVALID<gl_filter_type>()
{
  return NUM_GL_FILTER_TYPES;
}

template<>
constexpr system_mouse_type INVALID<system_mouse_type>()
{
  return NUM_SYSTEM_MOUSE_TYPES;
}

template<>
constexpr cursor_mode_types INVALID<cursor_mode_types>()
{
  return NUM_CURSOR_MODE_TYPES;
}

template<>
constexpr resample_mode INVALID<resample_mode>()
{
  return NUM_RESAMPLE_MODES;
}

template<>
constexpr screensaver_disable_mode INVALID<screensaver_disable_mode>()
{
  return NUM_SCREENSAVER_MODES;
}

template<>
constexpr allow_cheats_type INVALID<allow_cheats_type>()
{
  return NUM_ALLOW_CHEATS_TYPES;
}

#ifdef CONFIG_NETWORK
template<>
constexpr host_family INVALID<host_family>()
{
  return NUM_HOST_FAMILIES;
}
#endif

static void load_arg(char *arg)
{
  char *tmp_args[2];
  int argc = 0;

  tmp_args[argc++] = nullptr;
  tmp_args[argc++] = arg;

  set_config_from_command_line(&argc, tmp_args);
}

static boolean write_config(const char *path, const char *config)
{
  // This vastly improves the performance in builds that support it.
  vio_virtual_file(path);

  vfile *vf = vfopen_unsafe(path, "wb");
  ASSERT(vf, "failed to open '%s'", path);
  if(vf)
  {
    size_t ret = vfwrite(config, strlen(config), 1, vf);
    ASSERT(ret == 1, "write error for '%s'", path);
    ret = vfclose(vf);
    return true;
  }
  return false;
}

static void load_arg_file(const char *arg, boolean is_game_config)
{
  static const char CONFIG_FILENAME[] = UNIT_TMP_DIR "/_config_tmp";

  if(write_config(CONFIG_FILENAME, arg))
  {
    config_type type = is_game_config ? GAME_EDITOR_CNF : SYSTEM_CNF;
    set_config_from_file(type, CONFIG_FILENAME);
  }
}

template<int SIZE>
static void load_args(const char * const (&args)[SIZE])
{
  char *tmp_args[SIZE + 1];
  int argc = 0;
  int i;

  tmp_args[argc++] = nullptr;

  for(i = 0; i < SIZE; i++)
  {
    if(!args[i])
      break;

    tmp_args[argc++] = (char *)args[i];
  }

  set_config_from_command_line(&argc, tmp_args);
}

template<class T>
void TEST_INT(const char *setting_name, T &setting, ssize_t min, ssize_t max)
{
  constexpr T default_value = INVALID<T>();
  char arg[512];
  int lower_bound = (min >= -SSIZE_MAX / 2) ? -16 : 0;
  int upper_bound = (max < SSIZE_MAX / 2) ? 32 : 16;

  for(int i = lower_bound; i < upper_bound; i++)
  {
    ssize_t tmp;
    if(max >= SSIZE_MAX / 16)
      tmp = ((int64_t)max - min) / 16 * i + min;
    else
      tmp = ((int64_t)max - min) * i / 16 + min;

    ssize_t expected = (tmp >= min && tmp <= max) ? tmp : static_cast<ssize_t>(default_value);

    if(tmp < (ssize_t)std::numeric_limits<T>::min() ||
     tmp > (ssize_t)std::numeric_limits<T>::max())
      continue;

    snprintf(arg, arraysize(arg), "%s=%zd", setting_name, tmp);

    setting = default_value;
    load_arg(arg);
    ASSERTEQ((ssize_t)setting, expected, "%s", arg);

    setting = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQ((ssize_t)setting, expected, "%s", arg);

    if(!game_allowed)
    {
      setting = default_value;
      load_arg_file(arg, true);
      ASSERTEQ(setting, default_value, "%s", arg);
    }
  }
}

template<class T, int NUM_TESTS>
void TEST_ENUM(const char *setting_name, T &setting,
 const config_test_single (&data)[NUM_TESTS])
{
  constexpr T default_value = INVALID<T>();
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    snprintf(arg, arraysize(arg), "%s=%s", setting_name, data[i].value);

    setting = default_value;
    load_arg(arg);
    ASSERTEQ((int)setting, data[i].expected, "%s", arg);

    setting = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQ((int)setting, data[i].expected, "%s", arg);

    if(!game_allowed)
    {
      setting = default_value;
      load_arg_file(arg, true);
      ASSERTEQ(setting, default_value, "%s", arg);
    }
  }
}

template<class T, int NUM_TESTS>
void TEST_PAIR(const char *setting_name, T &setting_a, T &setting_b,
 const config_test_pair (&data)[NUM_TESTS])
{
  constexpr T default_value = INVALID<T>();
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    snprintf(arg, 512, "%s=%s", setting_name, data[i].value);

    setting_a = default_value;
    setting_b = default_value;
    load_arg(arg);
    ASSERTEQ((int)setting_a, data[i].expected_a, "%s", arg);
    ASSERTEQ((int)setting_b, data[i].expected_b, "%s", arg);

    setting_a = default_value;
    setting_b = default_value;
    load_arg_file(arg, game_allowed);
    ASSERTEQ((int)setting_a, data[i].expected_a, "%s", arg);
    ASSERTEQ((int)setting_b, data[i].expected_b, "%s", arg);

    if(!game_allowed)
    {
      setting_a = default_value;
      setting_b = default_value;
      load_arg_file(arg, true);
      ASSERTEQ(setting_a, default_value, "%s", arg);
      ASSERTEQ(setting_b, default_value, "%s", arg);
    }
  }
}

template<size_t S, int NUM_TESTS>
void TEST_STRING(const char *setting_name, char (&setting)[S],
 const config_test_string (&data)[NUM_TESTS])
{
  char arg[512];

  for(int i = 0; i < NUM_TESTS; i++)
  {
    size_t len = Unit::min(strlen(data[i].expected), S - 1);
    snprintf(arg, arraysize(arg), "%s=%s", setting_name, data[i].value);

    setting[0] = '\0';
    load_arg(arg);
    ASSERTNCMP(setting, data[i].expected, len, "%s", arg);
    ASSERTEQ(setting[len], '\0', "%s", arg);

    setting[0] = '\0';
    load_arg_file(arg, game_allowed);
    ASSERTNCMP(setting, data[i].expected, len, "%s", arg);
    ASSERTEQ(setting[len], '\0', "%s", arg);

    if(!game_allowed)
    {
      setting[0] = '\0';
      load_arg_file(arg, true);
      ASSERTEQ(setting[0], '\0', "%s", arg);
    }
  }
}

UNITTEST(Settings)
{
  struct config_info *conf = get_config();

  vio_filesystem_init(0, 0, false);
  struct vfsclose
  {
    ~vfsclose() { vio_filesystem_exit(); }
  } a;

  static const config_test_single boolean_data[] =
  {
    { "0",    false },
    { "1",    true },
    { "2",    INVALID<boolean>() },
    { "23",   INVALID<boolean>() },
    { "-12",  INVALID<boolean>() },
    { "-1",   INVALID<boolean>() },
    { "yes",  INVALID<boolean>() },
    { "ABCD", INVALID<boolean>() },
    { "0\\s", INVALID<boolean>() },
  };

  static const config_test_pair pair_data[] =
  {
    { "-1,-1", -1, -1 },
    { "-1,0", -1, 0 },
    { "0,-1", 0, -1 },
    { "640,350", 640, 350 },
    { "640,480", 640, 480 },
    { "1280,700", 1280, 700 },
    { "1920,1080", 1920, 1080 },
    { "2560,1440", 2560, 1440 },
    { "3840,2160", 3840, 2160 },
    { "640, 350", 640, 350 },
    { "1600,  1200", 1600, 1200 },
    { "640,a",        INVALID<int>(), INVALID<int>() },
    { "a,480",        INVALID<int>(), INVALID<int>() },
    { "a,b",          INVALID<int>(), INVALID<int>() },
    { "640px,350px",  INVALID<int>(), INVALID<int>() },
    { "-2,4154",      INVALID<int>(), INVALID<int>() },
    { "1233,-13513",  INVALID<int>(), INVALID<int>() },
  };

  static const char LONGSTRING[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  static const config_test_string string_data[] =
  {
    { "", "" },
    { "software", "software" },
    { "glsl", "glsl" },
    { "crt", "crt" },
    { "greyscale", "greyscale" },
    { "really_long_setting_string_wtf_don't_do_this",
      "really_long_setting_string_wtf_don't_do_this" },
    { LONGSTRING, LONGSTRING },
    // NOTE: Both the command line and the config file translate \s to a single
    // space. Whitespace can't really be tested the same between the two as it
    // isn't consumed from the command line but is unconditionally consumed
    // from the config file. This leads to situations where "test thing = a"
    // tries to set the option "testthing" from the file but "test thing " from
    // the command line. Both of these are wrong but not really worth fixing.
    { "space\\sspace", "space space" },
    { "copy\\soverlay\\sblock\\sat\\s0\\s0\\sfor\\s1\\sby\\s1\\sto\\s10\\s10",
      "copy overlay block at 0 0 for 1 by 1 to 10 10" },
    { "triple\\s\\s\\sspace", "triple   space" },
  };

  game_allowed = false;
  default_config();

  // Video options.

  SECTION(fullscreen)
  {
    TEST_ENUM("fullscreen", conf->fullscreen, boolean_data);
  }

  SECTION(fullscreen_windowed)
  {
    TEST_ENUM("fullscreen_windowed", conf->fullscreen_windowed, boolean_data);
  }

  SECTION(fullscreen_resolution)
  {
    TEST_PAIR("fullscreen_resolution", conf->resolution_width,
     conf->resolution_height, pair_data);
  }

  SECTION(window_resolution)
  {
    TEST_PAIR("window_resolution", conf->window_width,
     conf->window_height, pair_data);
  }

  SECTION(allow_resize)
  {
    TEST_ENUM("enable_resizing", conf->allow_resize, boolean_data);
  }

  SECTION(video_output)
  {
    TEST_STRING("video_output", conf->video_output, string_data);
  }

  SECTION(force_bpp)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "0", BPP_AUTO },
      { "8", 8 },
      { "16", 16 },
      { "32", 32 },
      { "auto", BPP_AUTO },
      { "-1", DEFAULT },
      { "231", DEFAULT },
      { "asdfsdf", DEFAULT },
      { "autou", DEFAULT },
    };
    TEST_ENUM("force_bpp", conf->force_bpp, data);
  }

  SECTION(video_ratio)
  {
    constexpr ratio_type DEFAULT = INVALID<ratio_type>();
    static const config_test_single data[] =
    {
      { "classic", RATIO_CLASSIC_4_3 },
      { "modern", RATIO_MODERN_64_35 },
      { "stretch", RATIO_STRETCH },
      { "-1", DEFAULT },
      { "12312342", DEFAULT },
      { "classiccc", DEFAULT },
    };
    TEST_ENUM("video_ratio", conf->video_ratio, data);
  }

  SECTION(gl_filter_method)
  {
    constexpr gl_filter_type DEFAULT = INVALID<gl_filter_type>();
    static const config_test_single data[] =
    {
      { "nearest", CONFIG_GL_FILTER_NEAREST },
      { "linear", CONFIG_GL_FILTER_LINEAR },
      { "213123", DEFAULT },
      { "nearets", DEFAULT },
    };
    TEST_ENUM("gl_filter_method", conf->gl_filter_method, data);
  }

  SECTION(gl_vsync)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "-1", -1 },
      { "0", 0 },
      { "1", 1 },
      { "default", -1 },
      { "-2", DEFAULT },
      { "-12312", DEFAULT },
      { "2", DEFAULT },
      { "6456", DEFAULT },
      { "1a", DEFAULT },
    };
    TEST_ENUM("gl_vsync", conf->gl_vsync, data);
  }

  SECTION(gl_scaling_shader)
  {
    game_allowed = true;
    TEST_STRING("gl_scaling_shader", conf->gl_scaling_shader, string_data);
  }

  SECTION(sdl_render_driver)
  {
    TEST_STRING("sdl_render_driver", conf->sdl_render_driver, string_data);
  }

  SECTION(cursor_hint_mode)
  {
    constexpr cursor_mode_types DEFAULT = INVALID<cursor_mode_types>();
    static const config_test_single data[] =
    {
      { "0", CURSOR_MODE_INVISIBLE },
      { "1", CURSOR_MODE_HINT },
      { "off", CURSOR_MODE_INVISIBLE },
      { "hidden", CURSOR_MODE_HINT },
      { "underline", CURSOR_MODE_UNDERLINE },
      { "solid",  CURSOR_MODE_SOLID },
      { "-1", DEFAULT },
      { "-24124124", DEFAULT },
      { "2", DEFAULT },
      { "938019831", DEFAULT },
      { "1a", DEFAULT },
      { "umderl1ne", DEFAULT },
    };
    TEST_ENUM("dialog_cursor_hints", conf->cursor_hint_mode, data);
  }

  SECTION(allow_screenshots)
  {
    TEST_ENUM("allow_screenshots", conf->allow_screenshots, boolean_data);
  }

  // Audio options.

  SECTION(audio_driver)
  {
    TEST_STRING("audio_driver", conf->audio_driver, string_data);
  }

  SECTION(audio_sample_rate)
  {
    TEST_INT("audio_sample_rate", conf->audio_sample_rate, 1, INT_MAX);
  }

  SECTION(audio_buffer_samples)
  {
    TEST_INT("audio_buffer_samples", conf->audio_buffer_samples, 1, INT_MAX);
  }

  SECTION(audio_output_channels)
  {
    TEST_INT("audio_output_channels", conf->audio_output_channels, 1, 2);
  }

  SECTION(enable_oversampling)
  {
    TEST_ENUM("enable_oversampling", conf->oversampling_on, boolean_data);
  }

  SECTION(resample_mode)
  {
    constexpr resample_mode DEFAULT = INVALID<resample_mode>();
    static const config_test_single data[] =
    {
      { "none", RESAMPLE_MODE_NONE },
      { "linear", RESAMPLE_MODE_LINEAR },
      { "cubic", RESAMPLE_MODE_CUBIC },
      { "fir", DEFAULT },
      { "sdfsedfsd", DEFAULT },
      { "0", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("resample_mode", conf->resample_mode, data);
  }

  SECTION(module_resample_mode)
  {
    constexpr resample_mode DEFAULT = INVALID<resample_mode>();
    static const config_test_single data[] =
    {
      { "none", RESAMPLE_MODE_NONE },
      { "linear", RESAMPLE_MODE_LINEAR },
      { "cubic", RESAMPLE_MODE_CUBIC },
      { "fir", RESAMPLE_MODE_FIR },
      { "sdfsedfsd", DEFAULT },
      { "0", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("module_resample_mode", conf->module_resample_mode, data);
  }

  SECTION(max_simultaneous_samples)
  {
    TEST_INT("max_simultaneous_samples", conf->max_simultaneous_samples, -1, INT_MAX);
  }

  SECTION(music_volume)
  {
    TEST_INT("music_volume", conf->music_volume, 0, 10);
  }

  SECTION(sam_volume)
  {
    TEST_INT("sample_volume", conf->sam_volume, 0, 10);
  }

  SECTION(pc_speaker_volume)
  {
    TEST_INT("pc_speaker_volume", conf->pc_speaker_volume, 0, 10);
  }

  SECTION(music_on)
  {
    TEST_ENUM("music_on", conf->music_on, boolean_data);
  }

  SECTION(pc_speaker_on)
  {
    TEST_ENUM("pc_speaker_on", conf->pc_speaker_on, boolean_data);
  }

  SECTION(audio_pc_speaker_use_hardware)
  {
    TEST_ENUM("audio_pc_speaker_use_hardware", conf->audio_pc_speaker_use_hardware, boolean_data);
  }

  SECTION(audio_opl_use_hardware)
  {
    TEST_ENUM("audio_opl_use_hardware", conf->audio_opl_use_hardware, boolean_data);
  }

  SECTION(audio_opl_port)
  {
    TEST_INT("audio_opl_port", conf->audio_opl_port, 0, 65535);
  }

  // Audio options (DOS-only)
  SECTION(audio_driver_config)
  {
    TEST_STRING("audio_config_lpt", conf->audio_config_lpt, string_data);
  }

  SECTION(audio_driver_config)
  {
    TEST_STRING("audio_config_sb", conf->audio_config_sb, string_data);
  }

  // Event options.

#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
  SECTION(gamecontroller_enable)
  {
    TEST_ENUM("gamecontroller_enable", conf->allow_gamepad, boolean_data);
  }

  SECTION(gamepad_enable)
  {
    TEST_ENUM("gamepad_enable", conf->allow_gamepad, boolean_data);
  }
#endif
#endif

  SECTION(pause_on_unfocus)
  {
    TEST_ENUM("pause_on_unfocus", conf->pause_on_unfocus, boolean_data);
  }

  SECTION(key_left_alt_is_altgr)
  {
    TEST_ENUM("key_left_alt_is_altgr", conf->key_left_alt_is_altgr, boolean_data);
  }

  SECTION(key_right_alt_is_altgr)
  {
    TEST_ENUM("key_right_alt_is_altgr", conf->key_right_alt_is_altgr, boolean_data);
  }

  SECTION(num_buffered_events)
  {
    TEST_INT("num_buffered_events", conf->num_buffered_events, 1, 256);
  }

  // Virtual filesystem options.
  SECTION(vfs_enable)
  {
    TEST_ENUM("vfs_enable", conf->vfs_enable, boolean_data);
  }

  SECTION(vfs_enable_auto_cache)
  {
    TEST_ENUM("vfs_enable_auto_cache", conf->vfs_enable_auto_cache, boolean_data);
  }

  SECTION(vfs_max_cache_size)
  {
    TEST_INT("vfs_max_cache_size", conf->vfs_max_cache_size, 0, SSIZE_MAX);
  }

  SECTION(vfs_max_cache_file_size)
  {
    TEST_INT("vfs_max_cache_file_size", conf->vfs_max_cache_file_size, 0, SSIZE_MAX);
  }

  // Game options.

  // TODO startup_path relies on stat.
  // TODO startup_file relies on stat.

  SECTION(save_file)
  {
    TEST_STRING("save_file", conf->default_save_name, string_data);
  }

  SECTION(mzx_speed)
  {
    game_allowed = true;
    TEST_INT("mzx_speed", conf->mzx_speed, 1, 16);
  }

  SECTION(allow_cheats)
  {
    constexpr allow_cheats_type DEFAULT = INVALID<allow_cheats_type>();
    static const config_test_single data[] =
    {
      { "0", ALLOW_CHEATS_NEVER },
      { "1", ALLOW_CHEATS_ALWAYS },
      { "never", ALLOW_CHEATS_NEVER },
      { "always", ALLOW_CHEATS_ALWAYS },
      { "mzxrun", ALLOW_CHEATS_MZXRUN },
      { "asdbfnsd", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("allow_cheats", conf->allow_cheats, data);
  }

  SECTION(auto_decrypt_worlds)
  {
    TEST_ENUM("auto_decrypt_worlds", conf->auto_decrypt_worlds, boolean_data);
  }

  SECTION(startup_editor)
  {
    TEST_ENUM("startup_editor", conf->startup_editor, boolean_data);
  }

  SECTION(standalone_mode)
  {
    TEST_ENUM("standalone_mode", conf->standalone_mode, boolean_data);
  }

  SECTION(no_titlescreen)
  {
    TEST_ENUM("no_titlescreen", conf->no_titlescreen, boolean_data);
  }

  SECTION(system_mouse)
  {
    constexpr system_mouse_type DEFAULT = INVALID<system_mouse_type>();
    static const config_test_single data[] =
    {
      { "0", SYSTEM_MOUSE_OFF },
      { "1", SYSTEM_MOUSE_ON },
      { "off", SYSTEM_MOUSE_OFF },
      { "on", SYSTEM_MOUSE_ON },
      { "only", SYSTEM_MOUSE_HIDE_SOFTWARE_MOUSE },
      { "mzxrun", DEFAULT },
      { "ksdjfksdf", DEFAULT },
      { "2", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("system_mouse", conf->system_mouse, data);
  }

  SECTION(grab_mouse)
  {
    TEST_ENUM("grab_mouse", conf->grab_mouse, boolean_data);
  }

  SECTION(disable_screensaver)
  {
    constexpr screensaver_disable_mode DEFAULT = INVALID<screensaver_disable_mode>();
    static const config_test_single data[] =
    {
      { "0", SCREENSAVER_ENABLE },
      { "1", SCREENSAVER_DISABLE },
      { "ingame", DEFAULT },
      { "fgdjiogjf", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("disable_screensaver", conf->disable_screensaver, data);
  }

  SECTION(save_slots)
  {
    TEST_ENUM("save_slots", conf->save_slots, boolean_data);
  }

  SECTION(save_slots_name)
  {
    TEST_STRING("save_slots_name", conf->save_slots_name, string_data);
  }

  SECTION(save_slots_ext)
  {
    TEST_STRING("save_slots_ext", conf->save_slots_ext, string_data);
  }

  // Editor options used by core.

  SECTION(test_mode)
  {
    TEST_ENUM("test_mode", conf->test_mode, boolean_data);
  }

  SECTION(test_mode_start_board)
  {
    TEST_INT("test_mode_start_board", conf->test_mode_start_board, 0, MAX_BOARDS - 1);
  }

  SECTION(mask_midchars)
  {
    TEST_ENUM("mask_midchars", conf->mask_midchars, boolean_data);
  }

#ifdef CONFIG_NETWORK

  // Network options.

  SECTION(network_enabled)
  {
    TEST_ENUM("network_enabled", conf->network_enabled, boolean_data);
  }

  SECTION(network_address_family)
  {
    constexpr host_family DEFAULT = INVALID<host_family>();
    static const config_test_single data[] =
    {
      { "0", HOST_FAMILY_ANY },
      { "any", HOST_FAMILY_ANY },
      { "ipv4", HOST_FAMILY_IPV4 },
      { "ipv6", HOST_FAMILY_IPV6 },
      { "", DEFAULT },
      { "1", DEFAULT },
      { "sdfasdf", DEFAULT },
      { "-1213", DEFAULT }
    };
    TEST_ENUM("network_address_family", conf->network_address_family, data);
  }

  SECTION(socks_host)
  {
    TEST_STRING("socks_host", conf->socks_host, string_data);
  }

  SECTION(socks_port)
  {
    TEST_INT("socks_port", conf->socks_port, 0, 65535);
  }

  SECTION(socks_username)
  {
    TEST_STRING("socks_username", conf->socks_username, string_data);
  }

  SECTION(socks_password)
  {
    TEST_STRING("socks_password", conf->socks_password, string_data);
  }

#endif /* CONFIG_NETWORK */

#ifdef CONFIG_UPDATER

  // Updater options.

  SECTION(update_host)
  {
    static const char * const option = "update_host";
    static const char * const hosts[] =
    {
      "updates.doot.rip",
      "updates.digitalmzx.com",
      "updates.megazuex.org",
      "looooo,.l",
      "a website",
      "sdjfjklsdfsdsdfsd",
      "",
      "PADDING"
    };
    char buffer[512];

    for(int i = 0; i < arraysize(hosts); i++)
    {
      snprintf(buffer, sizeof(buffer), "%s=%s", option, hosts[i]);
      load_arg(buffer);

      ASSERTEQ(conf->update_host_count, i+1, "");
      for(int j = 0; j <= i; j++)
        ASSERTCMP(conf->update_hosts[j], hosts[j], "");
    }
    free_config();
  }

  SECTION(update_branch_pin)
  {
    TEST_STRING("update_branch_pin", conf->update_branch_pin, string_data);
  }

  SECTION(update_auto_check)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "0", UPDATE_AUTO_CHECK_OFF },
      { "1", UPDATE_AUTO_CHECK_ON },
      { "off", UPDATE_AUTO_CHECK_OFF },
      { "on", UPDATE_AUTO_CHECK_ON },
      { "silent", UPDATE_AUTO_CHECK_SILENT },
      { "2", DEFAULT },
      { "asdfasdfasdf", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("update_auto_check", conf->update_auto_check, data);
  }

  SECTION(updater_enabled)
  {
    TEST_ENUM("updater_enabled", conf->updater_enabled, boolean_data);
  }

#endif /* CONFIG_UPDATER */

#ifdef CONFIG_EDITOR

  struct editor_config_info *econf = get_editor_config();
  default_editor_config();
  game_allowed = true;

  // Board editor options.

  SECTION(board_editor_hide_help)
  {
    TEST_ENUM("board_editor_hide_help", econf->board_editor_hide_help, boolean_data);
  }

  SECTION(editor_space_toggles)
  {
    TEST_ENUM("editor_space_toggles", econf->editor_space_toggles, boolean_data);
  }

  SECTION(editor_tab_focuses_view)
  {
    TEST_ENUM("editor_tab_focuses_view", econf->editor_tab_focuses_view, boolean_data);
  }

  SECTION(editor_load_board_assets)
  {
    TEST_ENUM("editor_load_board_assets", econf->editor_load_board_assets, boolean_data);
  }

  SECTION(editor_thing_menu_places)
  {
    TEST_ENUM("editor_thing_menu_places", econf->editor_thing_menu_places, boolean_data);
  }

  SECTION(editor_show_thing_toggles)
  {
    TEST_ENUM("editor_show_thing_toggles", econf->editor_show_thing_toggles, boolean_data);
  }

  SECTION(editor_show_thing_blink_speed)
  {
    TEST_INT("editor_show_thing_blink_speed", econf->editor_show_thing_blink_speed, 0, INT_MAX);
  }

  // Board defaults.

  SECTION(viewport_and_size)
  {
    static const config_test_board_size_viewport data[] =
    {
      // Setting      Value     w       h       vx      vy      vw      vh
      { "width",      "100",    100,    IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "100",    100,    100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_w", "74",     100,    100,    IGNORE, IGNORE, 74,     IGNORE },
      { "viewport_h", "21",     100,    100,    IGNORE, IGNORE, 74,     21 },
      { "viewport_x", "3",      100,    100,    3,      IGNORE, 74,     21 },
      { "viewport_y", "2",      100,    100,    3,      2,      74,     21 },

      // Ignore invalid values...
      { "width",      "0",      100,    100,    3,      2,      74,     21 },
      { "width",      "32768",  100,    100,    3,      2,      74,     21 },
      { "height",     "0",      100,    100,    3,      2,      74,     21 },
      { "height",     "32768",  100,    100,    3,      2,      74,     21 },
      { "viewport_x", "-1",     100,    100,    3,      2,      74,     21 },
      { "viewport_x", "80",     100,    100,    3,      2,      74,     21 },
      { "viewport_y", "-1",     100,    100,    3,      2,      74,     21 },
      { "viewport_y", "25",     100,    100,    3,      2,      74,     21 },
      { "viewport_w", "0",      100,    100,    3,      2,      74,     21 },
      { "viewport_w", "81",     100,    100,    3,      2,      74,     21 },
      { "viewport_h", "0",      100,    100,    3,      2,      74,     21 },
      { "viewport_h", "26",     100,    100,    3,      2,      74,     21 },

      // Width * height is capped to 16M.
      { "width",      "32767",  32767,  100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "32767",  512,    32767,  IGNORE, IGNORE, IGNORE, IGNORE },
      { "width",      "4097",   4097,   4095,   IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "4097",   4095,   4097,   IGNORE, IGNORE, IGNORE, IGNORE },

      // Width and height cap the viewport dimensions.
      { "width",      "40",     40,     IGNORE, IGNORE, IGNORE, 40,     IGNORE },
      { "height",     "15",     IGNORE, 15,     IGNORE, IGNORE, IGNORE, 15 },

      // Viewport size values over the board size and under the cap work (capped).
      { "width",      "60",     60,     IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_w", "75",     60,     IGNORE, IGNORE, IGNORE, 60,     IGNORE },
      { "height",     "20",     IGNORE, 20,     IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_h", "25",     IGNORE, 20,     IGNORE, IGNORE, IGNORE, 20 },

      // Viewport size overrides a previously set viewport position.
      { "width",      "100",    100,    IGNORE, IGNORE, IGNORE, IGNORE, IGNORE },
      { "height",     "100",    100,    100,    IGNORE, IGNORE, IGNORE, IGNORE },
      { "viewport_x", "10",     IGNORE, IGNORE, 10,     IGNORE, IGNORE, IGNORE },
      { "viewport_y", "5",      IGNORE, IGNORE, 10,     5,      IGNORE, IGNORE },
      { "viewport_w", "80",     IGNORE, IGNORE, 0,      IGNORE, 80,     IGNORE },
      { "viewport_h", "25",     IGNORE, IGNORE, IGNORE, 0,      IGNORE, 25 },

      // Viewport position respects previously set viewport size.
      { "viewport_w", "70",     IGNORE, IGNORE, IGNORE, IGNORE, 70,     IGNORE },
      { "viewport_h", "20",     IGNORE, IGNORE, IGNORE, IGNORE, 70,     20 },
      { "viewport_x", "5",      IGNORE, IGNORE, 5,      IGNORE, 70,     IGNORE },
      { "viewport_x", "15",     IGNORE, IGNORE, 10,     IGNORE, 70,     IGNORE },
      { "viewport_y", "2",      IGNORE, IGNORE, IGNORE, 2,      IGNORE, 20 },
      { "viewport_y", "10",     IGNORE, IGNORE, IGNORE, 5,      IGNORE, 20 },
    };
    char buffer[512];

    for(int i = 0; i < arraysize(data); i++)
    {
      const config_test_board_size_viewport &current = data[i];
      snprintf(buffer, sizeof(buffer), "board_default_%s=%s",
       current.setting, current.value);

      load_arg(buffer);

      if(current.width != IGNORE)
        ASSERTEQ(econf->board_width, current.width, "%s", buffer);
      if(current.height != IGNORE)
        ASSERTEQ(econf->board_height, current.height, "%s", buffer);
      if(current.viewport_x != IGNORE)
        ASSERTEQ(econf->viewport_x, current.viewport_x, "%s", buffer);
      if(current.viewport_y != IGNORE)
        ASSERTEQ(econf->viewport_y, current.viewport_y, "%s", buffer);
      if(current.viewport_w != IGNORE)
        ASSERTEQ(econf->viewport_w, current.viewport_w, "%s", buffer);
      if(current.viewport_h != IGNORE)
        ASSERTEQ(econf->viewport_h, current.viewport_h, "%s", buffer);
    }
  }

  SECTION(can_shoot)
  {
    TEST_ENUM("board_default_can_shoot", econf->can_shoot, boolean_data);
  }

  SECTION(can_bomb)
  {
    TEST_ENUM("board_default_can_bomb", econf->can_bomb, boolean_data);
  }

  SECTION(fire_burns_spaces)
  {
    TEST_ENUM("board_default_fire_burns_spaces", econf->fire_burns_spaces, boolean_data);
  }

  SECTION(fire_burns_fakes)
  {
    TEST_ENUM("board_default_fire_burns_fakes", econf->fire_burns_fakes, boolean_data);
  }

  SECTION(fire_burns_trees)
  {
    TEST_ENUM("board_default_fire_burns_trees", econf->fire_burns_trees, boolean_data);
  }

  SECTION(fire_burns_brown)
  {
    TEST_ENUM("board_default_fire_burns_brown", econf->fire_burns_brown, boolean_data);
  }

  SECTION(fire_burns_forever)
  {
    TEST_ENUM("board_default_fire_burns_forever", econf->fire_burns_forever, boolean_data);
  }

  SECTION(forest_to_floor)
  {
    TEST_ENUM("board_default_forest_to_floor", econf->forest_to_floor, boolean_data);
  }

  SECTION(collect_bombs)
  {
    TEST_ENUM("board_default_collect_bombs", econf->collect_bombs, boolean_data);
  }

  SECTION(dragons_can_randomly_move)
  {
    TEST_ENUM("board_default_dragons_can_randomly_move",
     econf->dragons_can_randomly_move, boolean_data);
  }

  SECTION(restart_if_hurt)
  {
    TEST_ENUM("board_default_restart_if_hurt", econf->restart_if_hurt, boolean_data);
  }

  SECTION(reset_on_entry)
  {
    TEST_ENUM("board_default_reset_on_entry", econf->reset_on_entry, boolean_data);
  }

  SECTION(reset_on_entry_same_board)
  {
    TEST_ENUM("board_default_reset_on_entry_same_board",
     econf->reset_on_entry_same_board, boolean_data);
  }

  SECTION(player_locked_ns)
  {
    TEST_ENUM("board_default_player_locked_ns", econf->player_locked_ns, boolean_data);
  }

  SECTION(player_locked_ew)
  {
    TEST_ENUM("board_default_player_locked_ew", econf->player_locked_ew, boolean_data);
  }

  SECTION(player_locked_att)
  {
    TEST_ENUM("board_default_player_locked_att", econf->player_locked_att, boolean_data);
  }

  SECTION(time_limit)
  {
    TEST_INT("board_default_time_limit", econf->time_limit, 0, 32767);
  }

  SECTION(explosions_leave)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "space", EXPL_LEAVE_SPACE },
      { "ash", EXPL_LEAVE_ASH },
      { "fire", EXPL_LEAVE_FIRE },
      { "0", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "1a", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_explosions_leave", econf->explosions_leave, data);
  }

  SECTION(saving_enabled)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "disabled", CANT_SAVE },
      { "enabled", CAN_SAVE },
      { "sensoronly", CAN_SAVE_ON_SENSOR },
      { "0", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "sdfbv", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_saving", econf->saving_enabled, data);
  }

  SECTION(overlay_enabled)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "disabled", OVERLAY_OFF },
      { "enabled", OVERLAY_ON },
      { "static", OVERLAY_STATIC },
      { "transparent", OVERLAY_TRANSPARENT },
      { "-1", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "disabledb", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("board_default_overlay", econf->overlay_enabled, data);
  }

  SECTION(charset_path)
  {
    TEST_STRING("board_default_charset_path", econf->charset_path, string_data);
  }

  SECTION(palette_path)
  {
    TEST_STRING("board_default_palette_path", econf->palette_path, string_data);
  }

  // Palette editor options.

  SECTION(palette_editor_hide_help)
  {
    TEST_ENUM("palette_editor_hide_help", econf->palette_editor_hide_help, boolean_data);
  }

  // Robot editor options.

  SECTION(editor_enter_splits)
  {
    TEST_ENUM("editor_enter_splits", econf->editor_enter_splits, boolean_data);
  }

  SECTION(color_codes)
  {
    static const struct
    {
      const char * const opt;
      unsigned int idx;
    } color_code_opts[] =
    {
      { "ccode_chars", ROBO_ED_CC_CHARACTERS },
      { "ccode_colors", ROBO_ED_CC_COLORS },
      { "ccode_commands", ROBO_ED_CC_COMMANDS },
      { "ccode_conditions", ROBO_ED_CC_CONDITIONS },
      { "ccode_current_line", ROBO_ED_CC_CURRENT_LINE },
      { "ccode_directions", ROBO_ED_CC_DIRECTIONS },
      { "ccode_equalities", ROBO_ED_CC_EQUALITIES },
      { "ccode_extras", ROBO_ED_CC_EXTRAS },
      { "ccode_immediates", ROBO_ED_CC_IMMEDIATES },
      { "ccode_items", ROBO_ED_CC_ITEMS },
      { "ccode_params", ROBO_ED_CC_PARAMS },
      { "ccode_strings", ROBO_ED_CC_STRINGS },
      { "ccode_things", ROBO_ED_CC_THINGS },
    };
    static const config_test_single color_data[] =
    {
      { "255", 255 },
    };

    for(int i = 0; i < arraysize(color_code_opts); i++)
    {
      auto opt = color_code_opts[i];
      TEST_INT(opt.opt, econf->color_codes[opt.idx], 0, 15);
    }

    // Test the colors special case...
    TEST_ENUM("ccode_colors", econf->color_codes[ROBO_ED_CC_COLORS], color_data);
  }

  SECTION(color_coding_on)
  {
    TEST_ENUM("color_coding_on", econf->color_coding_on, boolean_data);
  }

#ifndef CONFIG_DEBYTECODE
  SECTION(default_invalid_status)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "ignore", invalid_uncertain },
      { "delete", invalid_discard },
      { "comment", invalid_comment },
      { "-1", DEFAULT },
      { "1", DEFAULT },
      { "2", DEFAULT },
      { "disabledb", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("default_invalid_status", econf->default_invalid_status, data);
  }
#endif

  SECTION(disassemble_extras)
  {
    TEST_ENUM("disassemble_extras", econf->disassemble_extras, boolean_data);
  }

  SECTION(disassemble_base)
  {
    constexpr int DEFAULT = INVALID<int>();
    static const config_test_single data[] =
    {
      { "10", 10 },
      { "16", 16 },
      { "2", DEFAULT },
      { "4", DEFAULT },
      { "8", DEFAULT },
      { "10bfd", DEFAULT },
      { "11", DEFAULT },
      { "", DEFAULT },
    };
    TEST_ENUM("disassemble_base", econf->disassemble_base, data);
  }

  SECTION(robot_editor_hide_help)
  {
    TEST_ENUM("robot_editor_hide_help", econf->robot_editor_hide_help, boolean_data);
  }

  // Backup options.

  SECTION(backup_count)
  {
    TEST_INT("backup_count", econf->backup_count, 0, INT_MAX);
  }

  SECTION(backup_interval)
  {
    TEST_INT("backup_interval", econf->backup_interval, 0, INT_MAX);
  }

  SECTION(backup_name)
  {
    TEST_STRING("backup_name", econf->backup_name, string_data);
  }

  SECTION(backup_ext)
  {
    TEST_STRING("backup_ext", econf->backup_ext, string_data);
  }

  // Macro options.

  // TODO extended macros should be a separate test.

  SECTION(default_macros)
  {
    char buffer[16];
    for(int i = 0; i < arraysize(econf->default_macros); i++)
    {
      snprintf(buffer, sizeof(buffer), "macro_%d", i+1);
      TEST_STRING(buffer, econf->default_macros[i], string_data);
    }
  }

  // Saved positions.

  SECTION(saved_position)
  {
    static const config_test_saved_position data[] =
    {
      { 0, "1,2,3,4,5,0",       { 1, 2, 3, 4, 5, 0 } },
      { 0, "",                  { 1, 2, 3, 4, 5, 0 } },
      { 1, "9,8,7,6,5,60",      { 9, 8, 7, 6, 5, 60 } },
      { 2, "249,32767,32767,32767,32767,60", { 249, 32767, 32767, 32767, 32767, 60 } },
      { 3, "3,3,3,3,3,0",       { 3, 3, 3, 3, 3, 0 } },
      { 4, "4,4,4,4,4,0",       { 4, 4, 4, 4, 4, 0 } },
      { 5, "5,5,5,5,5,0",       { 5, 5, 5, 5, 5, 0 } },
      { 6, "6,6,6,6,6,0",       { 6, 6, 6, 6, 6, 0 } },
      { 7, "7,7,7,7,7,0",       { 7, 7, 7, 7, 7, 0 } },
      { 8, "8,8,8,8,8,0",       { 8, 8, 8, 8, 8, 0 } },
      { 9, "9,9,9,9,9,0",       { 9, 9, 9, 9, 9, 0 } },
      { 8, "",                  { 8, 8, 8, 8, 8, 0 } },
      { 7, "",                  { 7, 7, 7, 7, 7, 0 } },
      { 6, "",                  { 6, 6, 6, 6, 6, 0 } },
      { 5, "",                  { 5, 5, 5, 5, 5, 0 } },
      { 4, "",                  { 4, 4, 4, 4, 4, 0 } },
      { 3, "",                  { 3, 3, 3, 3, 3, 0 } },
      // Make sure this at least has rudimentary bounding for invalid things...
      { 0, "0,0,0,0,0,0",       { 0, 0, 0, 0, 0, 0 } },
      { 0, "250,0,0,0,0,0",     { 0, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "-1,0,0,0,0,0",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,32768,0,0,0,0,0", { 0, 0,      IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,-1,0,0,0,0",      { 0, 0,      IGNORE, IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,32768,0,0,0",   { 0, IGNORE, 0,      IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,-1,0,0,0",      { 0, IGNORE, 0,      IGNORE, IGNORE, IGNORE } },
      { 0, "9,0,0,32768,0,0",   { 0, IGNORE, IGNORE, 0,      IGNORE, IGNORE } },
      { 0, "9,0,0,-1,0,0",      { 0, IGNORE, IGNORE, 0,      IGNORE, IGNORE } },
      { 0, "9,0,0,0,32768,0",   { 0, IGNORE, IGNORE, IGNORE, 0,      IGNORE } },
      { 0, "9,0,0,0,-1,0",      { 0, IGNORE, IGNORE, IGNORE, 0,      IGNORE } },
      { 0, "9,0,0,0,0,80",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, 0 } },
      { 0, "9,0,0,0,0,-1",      { 0, IGNORE, IGNORE, IGNORE, IGNORE, 0 } },
    };
    const struct saved_position *dest;
    const struct saved_position *expected;
    char buffer[256];

    for(int i = 0; i < arraysize(data); i++)
    {
      snprintf(buffer, sizeof(buffer), "saved_position%d=%s",
       data[i].which, data[i].value);

      load_arg(buffer);

      dest = &(econf->saved_positions[data[i].which]);
      expected = &(data[i].expected);

      if(expected->board_id != IGNORE)
        ASSERTEQ(dest->board_id, expected->board_id, "%s", buffer);
      if(expected->cursor_x != IGNORE)
        ASSERTEQ(dest->cursor_x, expected->cursor_x, "%s", buffer);
      if(expected->cursor_y != IGNORE)
        ASSERTEQ(dest->cursor_y, expected->cursor_y, "%s", buffer);
      if(expected->scroll_x != IGNORE)
        ASSERTEQ(dest->scroll_x, expected->scroll_x, "%s", buffer);
      if(expected->scroll_y != IGNORE)
        ASSERTEQ(dest->scroll_y, expected->scroll_y, "%s", buffer);
      if(expected->debug_x != IGNORE)
        ASSERTEQ(dest->debug_x, expected->debug_x, "%s", buffer);
    }
  }

#endif /* CONFIG_EDITOR */
}

#define SP_AXIS     (1 << 13)
#define SP_KEYBOARD (1 << 14)
#define AXIS(n)     (SP_AXIS | (n))
#define AXIS_NONE   (SP_AXIS | 255)

struct config_test_joystick
{
  const char * const setting;
  const char * const value;
  int first;
  int last;
  int which;
  int expected[4];
};

// TODO only bothering to test these as strings right now, but they all should
// work from game config files too.

template<int NUM_TESTS>
void TEST_JOY_ALIAS(const config_test_single (&data)[NUM_TESTS])
{
  struct joystick_map *joy_global_map = get_joystick_map(true);
  constexpr int16_t DEFAULT = INVALID<int16_t>();
  char arg[512];

  ASSERT(joy_global_map, "");
  for(int i = 0; i < NUM_TESTS; i++)
  {
    joy_global_map->button[0][0] = DEFAULT;
    snprintf(arg, arraysize(arg), "joy1button1=%s", data[i].value);
    load_arg(arg);

    ASSERTEQ(joy_global_map->button[0][0], data[i].expected, "%s", arg);
  }
}

template<int NUM_TESTS>
void TEST_JOY_BUTTON(const config_test_joystick (&data)[NUM_TESTS])
{
  struct joystick_map *joy_global_map = get_joystick_map(true);
  constexpr int16_t DEFAULT = INVALID<int16_t>();
  char arg[512];

  ASSERT(joy_global_map, "");
  for(int i = 0; i < NUM_TESTS; i++)
  {
    const config_test_joystick &t = data[i];
    if(t.which > MAX_JOYSTICK_BUTTONS)
      continue;

    for(int j = 0; j < MAX_JOYSTICKS; j++)
      joy_global_map->button[j][t.which - 1] = DEFAULT;

    snprintf(arg, arraysize(arg), "%s=%s", t.setting, t.value);
    load_arg(arg);

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      if(j >= t.first - 1 && j <= t.last - 1)
        ASSERTEQ(joy_global_map->button[j][t.which - 1], t.expected[0], "%s", arg);
      else
        ASSERTEQ(joy_global_map->button[j][t.which - 1], DEFAULT, "%s", arg);
    }
  }
}

template<int NUM_TESTS>
void TEST_JOY_AXIS(const config_test_joystick (&data)[NUM_TESTS])
{
  struct joystick_map *joy_global_map = get_joystick_map(true);
  constexpr int16_t DEFAULT = INVALID<int16_t>();
  char arg[512];

  ASSERT(joy_global_map, "");
  for(int i = 0; i < NUM_TESTS; i++)
  {
    const config_test_joystick &t = data[i];
    if(t.which > MAX_JOYSTICK_AXES)
      continue;

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      joy_global_map->axis[j][t.which - 1][0] = DEFAULT;
      joy_global_map->axis[j][t.which - 1][1] = DEFAULT;
    }

    snprintf(arg, arraysize(arg), "%s=%s", t.setting, t.value);
    load_arg(arg);

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      if(j >= t.first - 1 && j <= t.last - 1)
      {
        ASSERTEQ(joy_global_map->axis[j][t.which - 1][0], t.expected[0], "%s", arg);
        ASSERTEQ(joy_global_map->axis[j][t.which - 1][1], t.expected[1], "%s", arg);
      }
      else
      {
        ASSERTEQ(joy_global_map->axis[j][t.which - 1][0], DEFAULT, "%s", arg);
        ASSERTEQ(joy_global_map->axis[j][t.which - 1][1], DEFAULT, "%s", arg);
      }
    }
  }
}

template<int NUM_TESTS>
void TEST_JOY_HAT(const config_test_joystick (&data)[NUM_TESTS])
{
  struct joystick_map *joy_global_map = get_joystick_map(true);
  constexpr int16_t DEFAULT = INVALID<int16_t>();
  char arg[512];

  ASSERT(joy_global_map, "");
  for(int i = 0; i < NUM_TESTS; i++)
  {
    const config_test_joystick &t = data[i];

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      joy_global_map->hat[j][JOYHAT_UP] = DEFAULT;
      joy_global_map->hat[j][JOYHAT_DOWN] = DEFAULT;
      joy_global_map->hat[j][JOYHAT_LEFT] = DEFAULT;
      joy_global_map->hat[j][JOYHAT_RIGHT] = DEFAULT;
    }

    snprintf(arg, arraysize(arg), "%s=%s", t.setting, t.value);
    load_arg(arg);

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      if(j >= t.first - 1 && j <= t.last - 1)
      {
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_UP], t.expected[JOYHAT_UP], "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_DOWN], t.expected[JOYHAT_DOWN], "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_LEFT], t.expected[JOYHAT_LEFT], "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_RIGHT], t.expected[JOYHAT_RIGHT], "%s", arg);
      }
      else
      {
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_UP], DEFAULT, "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_DOWN], DEFAULT, "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_LEFT], DEFAULT, "%s", arg);
        ASSERTEQ(joy_global_map->hat[j][JOYHAT_RIGHT], DEFAULT, "%s", arg);
      }
    }
  }
}

static void reset_mapping(struct joystick_map *map, int j, int which)
{
  if(which & SP_KEYBOARD)
  {
    map->show_screen_keyboard_action[j] = INVALID<int16_t>();
  }
  else

  if(which & SP_AXIS)
  {
    memset(map->special_axis[j], 255, sizeof(map->special_axis[j]));
  }
  else
    map->action[j][which] = INVALID<int16_t>();
}

static void check_mapping(const struct joystick_map *map, int j, int which,
 int expected, const char *arg)
{
  if(which & SP_KEYBOARD)
  {
    ASSERTEQ(map->show_screen_keyboard_action[j], expected, "%s", arg);
  }
  else

  if(which & SP_AXIS)
  {
    which = (which & ~SP_AXIS) - 1;
    for(int i = 0; i < arraysize(map->special_axis[j]); i++)
    {
      if(i == which && expected != INVALID<int16_t>())
        ASSERTEQ(map->special_axis[j][i], expected, "%s", arg);
      else
        ASSERTEQ(map->special_axis[j][i], 255, "%s", arg);
    }
  }
  else
    ASSERTEQ(map->action[j][which], expected, "%s", arg);
}

template<int NUM_TESTS>
void TEST_JOY_ACTION(const config_test_joystick (&data)[NUM_TESTS])
{
  struct joystick_map *joy_global_map = get_joystick_map(true);
  constexpr int16_t DEFAULT = INVALID<int16_t>();
  char arg[512];

  ASSERT(joy_global_map, "");
  for(int i = 0; i < NUM_TESTS; i++)
  {
    const config_test_joystick &t = data[i];

    // NOTE: unlike the other tests, don't subtract 1 from t.which here since
    // it's an action number.
    for(int j = 0; j < MAX_JOYSTICKS; j++)
      reset_mapping(joy_global_map, j, t.which);

    snprintf(arg, arraysize(arg), "%s=%s", t.setting, t.value);
    load_arg(arg);

    for(int j = 0; j < MAX_JOYSTICKS; j++)
    {
      if(j >= t.first - 1 && j <= t.last - 1)
        check_mapping(joy_global_map, j, t.which, t.expected[0], arg);
      else
        check_mapping(joy_global_map, j, t.which, DEFAULT, arg);
    }
  }
}

UNITTEST(Joystick)
{
  constexpr int16_t DEFAULT = INVALID<int16_t>();

  SECTION(joyN_button)
  {
    static const config_test_joystick data[] =
    {
      // Setting              Value           [#, #]  which   expected
      { "joy1button1",        "key_space",    1,  1,  1,      { IKEY_SPACE }},
      { "joy[1,1]button2",    "key_insert",   1,  1,  2,      { IKEY_INSERT }},
      { "joy16button256",     "key_escape",   16, 16, 256,    { IKEY_ESCAPE, }},
      { "joy[1,16]button16",  "act_lstick",   1,  16, 16,     { -JOY_LSTICK }},
      { "joy[1,16]button256", "act_a",        1,  16, 256,    { -JOY_A }},
      { "joy[4,8]button10",   "act_start",    4,  8,  10,     { -JOY_START }},
      { "joy[9,9]button123",  "27",           9,  9,  123,    { IKEY_ESCAPE }},
      { "joy[10,11]button71", "13",           10, 11, 71,     { IKEY_RETURN }},
    };
    TEST_JOY_BUTTON(data);
  }

  SECTION(joyN_axis)
  {
    static const config_test_joystick data[] =
    {
      // Setting              Value                   [#, #]  which   expected
      { "joy1axis1",          "key_left, key_right",  1,  1,  1,      { IKEY_LEFT, IKEY_RIGHT }},
      { "joy[1,1]axis2",      "key_a,key_d",          1,  1,  2,      { IKEY_a, IKEY_d }},
      { "joy16axis16",        "key_w,key_s",          16, 16, 16,     { IKEY_w, IKEY_s }},
      { "joy[11,16]axis16",   "key_up, key_down",     11, 16, 16,     { IKEY_UP, IKEY_DOWN }},
      { "joy[2,9]axis7",      "key_1,key_delete",     2,  9,  7,      { IKEY_1, IKEY_DELETE }},
      { "joy[1,14]axis3",     "act_r_up,act_r_down",  1,  14, 3,      { -JOY_R_UP, -JOY_R_DOWN }},
      { "joy[7,7]axis15",     "act_x,act_y",          7,  7,  15,     { -JOY_X, -JOY_Y }},
    };
    TEST_JOY_AXIS(data);
  }

  SECTION(joyN_hat)
  {
    static const config_test_joystick data[] =
    {
      // Setting          Value                                 [#, #]  which
      { "joy1hat",        "key_up,key_down,key_left,key_right", 1,  1,  0,
        { IKEY_UP, IKEY_DOWN, IKEY_LEFT, IKEY_RIGHT }},
      { "joy[10,14]hat",  "key_w, key_s, key_a, key_d",         10, 14, 0,
        { IKEY_w, IKEY_s, IKEY_a, IKEY_d }},
      { "joy[1,16]hat",   "act_up,act_down,act_left,act_right", 1,  16, 0,
        { -JOY_UP, -JOY_DOWN, -JOY_LEFT, -JOY_RIGHT }},
      { "joy[7,12]hat",   "273,274,276,275",                    7,  12, 0,
        { IKEY_UP, IKEY_DOWN, IKEY_LEFT, IKEY_RIGHT }},
    };
    TEST_JOY_HAT(data);
  }

  SECTION(joyN_action)
  {
    static const config_test_joystick data[] =
    {
      // Make sure every action is assignable.
      // Setting              Value           [#, #]  which           expected
      { "joy1.a",             "key_space",    1,  1,  JOY_A,          { IKEY_SPACE }},
      { "joy1.b",             "key_space",    1,  1,  JOY_B,          { IKEY_SPACE }},
      { "joy1.x",             "key_space",    1,  1,  JOY_X,          { IKEY_SPACE }},
      { "joy1.y",             "key_space",    1,  1,  JOY_Y,          { IKEY_SPACE }},
      { "joy1.select",        "key_space",    1,  1,  JOY_SELECT,     { IKEY_SPACE }},
      { "joy1.start",         "key_space",    1,  1,  JOY_START,      { IKEY_SPACE }},
      { "joy1.lstick",        "key_space",    1,  1,  JOY_LSTICK,     { IKEY_SPACE }},
      { "joy1.rstick",        "key_space",    1,  1,  JOY_RSTICK,     { IKEY_SPACE }},
      { "joy1.lshoulder",     "key_space",    1,  1,  JOY_LSHOULDER,  { IKEY_SPACE }},
      { "joy1.rshoulder",     "key_space",    1,  1,  JOY_RSHOULDER,  { IKEY_SPACE }},
      { "joy1.ltrigger",      "key_space",    1,  1,  JOY_LTRIGGER,   { IKEY_SPACE }},
      { "joy1.rtrigger",      "key_space",    1,  1,  JOY_RTRIGGER,   { IKEY_SPACE }},
      { "joy1.up",            "key_space",    1,  1,  JOY_UP,         { IKEY_SPACE }},
      { "joy1.down",          "key_space",    1,  1,  JOY_DOWN,       { IKEY_SPACE }},
      { "joy1.left",          "key_space",    1,  1,  JOY_LEFT,       { IKEY_SPACE }},
      { "joy1.right",         "key_space",    1,  1,  JOY_RIGHT,      { IKEY_SPACE }},
      { "joy1.l_up",          "key_space",    1,  1,  JOY_L_UP,       { IKEY_SPACE }},
      { "joy1.l_down",        "key_space",    1,  1,  JOY_L_DOWN,     { IKEY_SPACE }},
      { "joy1.l_left",        "key_space",    1,  1,  JOY_L_LEFT,     { IKEY_SPACE }},
      { "joy1.l_right",       "key_space",    1,  1,  JOY_L_RIGHT,    { IKEY_SPACE }},
      { "joy1.r_up",          "key_space",    1,  1,  JOY_R_UP,       { IKEY_SPACE }},
      { "joy1.r_down",        "key_space",    1,  1,  JOY_R_DOWN,     { IKEY_SPACE }},
      { "joy1.r_left",        "key_space",    1,  1,  JOY_R_LEFT,     { IKEY_SPACE }},
      { "joy1.r_right",       "key_space",    1,  1,  JOY_R_RIGHT,    { IKEY_SPACE }},

      // Reject actions.
      { "joy1.ltrigger",      "0",            1,  1,  JOY_LTRIGGER,   { DEFAULT }},
      { "joy1.select",        "act_up",       1,  1,  JOY_SELECT,     { DEFAULT }},

      // Ranges.
      // Setting              Value           [#, #]  which           expected
      { "joy[1,16].select",   "key_escape",   1,  16, JOY_SELECT,     { IKEY_ESCAPE }},
      { "joy[7,9].up",        "key_w",        7,  9,  JOY_UP,         { IKEY_w }},
      { "joy[14,15].l_right", "key_7",        14, 15, JOY_L_RIGHT,    { IKEY_7 }},
      { "joy[1,4].y",         "49",           1,  4,  JOY_Y,          { IKEY_1 }},

      // Special axes.
      { "joy1.axis_lx",       "3",            1,  1,  AXIS(3),        { JOY_AXIS_LEFT_X }},
      { "joy1.axis_ly",       "1",            1,  1,  AXIS(1),        { JOY_AXIS_LEFT_Y }},
      { "joy1.axis_rx",       "5",            1,  1,  AXIS(5),        { JOY_AXIS_RIGHT_X }},
      { "joy1.axis_ry",       "10",           1,  1,  AXIS(10),       { JOY_AXIS_RIGHT_Y }},
      { "joy1.axis_ltrigger", "16",           1,  1,  AXIS(16),       { JOY_AXIS_LEFT_TRIGGER }},
      { "joy1.axis_rtrigger", "8",            1,  1,  AXIS(8),        { JOY_AXIS_RIGHT_TRIGGER }},
      { "joy1.axis_lx",       "0",            1,  1,  AXIS_NONE,      { JOY_AXIS_LEFT_X }},
      { "joy1.axis_lx",       "-1",           1,  1,  AXIS_NONE,      { JOY_AXIS_LEFT_X }},
      { "joy1.axis_lx",       "17",           1,  1,  AXIS_NONE,      { JOY_AXIS_LEFT_X }},
      { "joy1.axis_lx",       "act_x",        1,  1,  AXIS_NONE,      { JOY_AXIS_LEFT_X }},
      { "joy[4,12].axis_lx",  "7",            4, 12,  AXIS(7),        { JOY_AXIS_LEFT_X }},

      // Special settings.
      { "joy1.show_screen_keyboard", "act_l_up",   1, 1, SP_KEYBOARD, { JOY_L_UP }},
      { "joy1.show_screen_keyboard", "0",          1, 1, SP_KEYBOARD, { JOY_NO_ACTION }},
      { "joy1.show_screen_keyboard", "key_space",  1, 1, SP_KEYBOARD, { DEFAULT }},
      { "joy[2,9].show_screen_keyboard", "act_x",  2, 9, SP_KEYBOARD, { JOY_X }},
    };
    TEST_JOY_ACTION(data);
  }

  SECTION(joystick_aliases)
  {
    /**
     * That general case stuff worked?
     * Great, now make sure every alias works! :(
     *
     * Uses joy1button1 internally. Action aliases translate to negative values.
     */
    static const config_test_single data[] =
    {
      // Keys (keep in the same order as the list in event.c).
      { "key_0",            IKEY_0 },
      { "key_1",            IKEY_1 },
      { "key_2",            IKEY_2 },
      { "key_3",            IKEY_3 },
      { "key_4",            IKEY_4 },
      { "key_5",            IKEY_5 },
      { "key_6",            IKEY_6 },
      { "key_7",            IKEY_7 },
      { "key_8",            IKEY_8 },
      { "key_9",            IKEY_9 },
      { "key_a",            IKEY_a },
      { "key_b",            IKEY_b },
      { "key_backquote",    IKEY_BACKQUOTE },
      { "key_backslash",    IKEY_BACKSLASH },
      { "key_backspace",    IKEY_BACKSPACE },
      { "key_break",        IKEY_BREAK },
      { "key_c",            IKEY_c },
      { "key_capslock",     IKEY_CAPSLOCK },
      { "key_comma",        IKEY_COMMA },
      { "key_d",            IKEY_d },
      { "key_delete",       IKEY_DELETE },
      { "key_down",         IKEY_DOWN },
      { "key_e",            IKEY_e },
      { "key_end",          IKEY_END },
      { "key_equals",       IKEY_EQUALS },
      { "key_escape",       IKEY_ESCAPE },
      { "key_f",            IKEY_f },
      { "key_f1",           IKEY_F1 },
      { "key_f10",          IKEY_F10 },
      { "key_f11",          IKEY_F11 },
      { "key_f12",          IKEY_F12 },
      { "key_f2",           IKEY_F2 },
      { "key_f3",           IKEY_F3 },
      { "key_f4",           IKEY_F4 },
      { "key_f5",           IKEY_F5 },
      { "key_f6",           IKEY_F6 },
      { "key_f7",           IKEY_F7 },
      { "key_f8",           IKEY_F8 },
      { "key_f9",           IKEY_F9 },
      { "key_g",            IKEY_g },
      { "key_h",            IKEY_h },
      { "key_home",         IKEY_HOME },
      { "key_i",            IKEY_i },
      { "key_insert",       IKEY_INSERT },
      { "key_j",            IKEY_j },
      { "key_k",            IKEY_k },
      { "key_kp0",          IKEY_KP0 },
      { "key_kp1",          IKEY_KP1 },
      { "key_kp2",          IKEY_KP2 },
      { "key_kp3",          IKEY_KP3 },
      { "key_kp4",          IKEY_KP4 },
      { "key_kp5",          IKEY_KP5 },
      { "key_kp6",          IKEY_KP6 },
      { "key_kp7",          IKEY_KP7 },
      { "key_kp8",          IKEY_KP8 },
      { "key_kp9",          IKEY_KP9 },
      { "key_kp_divide",    IKEY_KP_DIVIDE },
      { "key_kp_enter",     IKEY_KP_ENTER },
      { "key_kp_minus",     IKEY_KP_MINUS },
      { "key_kp_multiply",  IKEY_KP_MULTIPLY },
      { "key_kp_period",    IKEY_KP_PERIOD },
      { "key_kp_plus",      IKEY_KP_PLUS },
      { "key_l",            IKEY_l },
      { "key_lalt",         IKEY_LALT },
      { "key_lctrl",        IKEY_LCTRL },
      { "key_left",         IKEY_LEFT },
      { "key_leftbracket",  IKEY_LEFTBRACKET },
      { "key_lshift",       IKEY_LSHIFT },
      { "key_lsuper",       IKEY_LSUPER },
      { "key_m",            IKEY_m },
      { "key_menu",         IKEY_MENU },
      { "key_minus",        IKEY_MINUS },
      { "key_n",            IKEY_n },
      { "key_numlock",      IKEY_NUMLOCK },
      { "key_o",            IKEY_o },
      { "key_p",            IKEY_p },
      { "key_pagedown",     IKEY_PAGEDOWN },
      { "key_pageup",       IKEY_PAGEUP },
      { "key_period",       IKEY_PERIOD },
      { "key_q",            IKEY_q },
      { "key_quote",        IKEY_QUOTE },
      { "key_r",            IKEY_r },
      { "key_ralt",         IKEY_RALT },
      { "key_rctrl",        IKEY_RCTRL },
      { "key_return",       IKEY_RETURN },
      { "key_right",        IKEY_RIGHT },
      { "key_rightbracket", IKEY_RIGHTBRACKET },
      { "key_rshift",       IKEY_RSHIFT },
      { "key_rsuper",       IKEY_RSUPER },
      { "key_s",            IKEY_s },
      { "key_scrolllock",   IKEY_SCROLLOCK },
      { "key_semicolon",    IKEY_SEMICOLON },
      { "key_slash",        IKEY_SLASH },
      { "key_space",        IKEY_SPACE },
      { "key_sysreq",       IKEY_SYSREQ },
      { "key_t",            IKEY_t },
      { "key_tab",          IKEY_TAB },
      { "key_u",            IKEY_u },
      { "key_up",           IKEY_UP },
      { "key_v",            IKEY_v },
      { "key_w",            IKEY_w },
      { "key_x",            IKEY_x },
      { "key_y",            IKEY_y },
      { "key_z",            IKEY_z },

      // Actions (keep in the same order as the list in event.c).
      { "act_a",            -JOY_A },
      { "act_b",            -JOY_B },
      { "act_down",         -JOY_DOWN },
      { "act_l_down",       -JOY_L_DOWN },
      { "act_l_left",       -JOY_L_LEFT },
      { "act_l_right",      -JOY_L_RIGHT },
      { "act_l_up",         -JOY_L_UP },
      { "act_left",         -JOY_LEFT },
      { "act_lshoulder",    -JOY_LSHOULDER },
      { "act_lstick",       -JOY_LSTICK },
      { "act_ltrigger",     -JOY_LTRIGGER },
      { "act_r_down",       -JOY_R_DOWN },
      { "act_r_left",       -JOY_R_LEFT },
      { "act_r_right",      -JOY_R_RIGHT },
      { "act_r_up",         -JOY_R_UP },
      { "act_right",        -JOY_RIGHT },
      { "act_rshoulder",    -JOY_RSHOULDER },
      { "act_rstick",       -JOY_RSTICK },
      { "act_rtrigger",     -JOY_RTRIGGER },
      { "act_select",       -JOY_SELECT },
      { "act_start",        -JOY_START },
      { "act_up",           -JOY_UP },
      { "act_x",            -JOY_X },
      { "act_y",            -JOY_Y }
    };
    TEST_JOY_ALIAS(data);
  }

  // TODO SDL2 gamecontroller options are static in event_sdl.c (annoying to test...)
}

UNITTEST(Include)
{
  struct config_info *conf = get_config();
  default_config();

#ifdef CONFIG_EDITOR
  struct editor_config_info *econf = get_editor_config();
  default_editor_config();
#endif

  SECTION(Include)
  {
    // This version only works from a config file.
    boolean ret = write_config(UNIT_TMP_DIR "/a.cnf", "include " UNIT_TMP_DIR "/b.cnf");
    ret &= write_config(UNIT_TMP_DIR "/b.cnf", "mzx_speed = 4");
    ASSERTEQ(ret, true, "");

    conf->mzx_speed = 2;

    set_config_from_file(SYSTEM_CNF, UNIT_TMP_DIR "/a.cnf");
    ASSERTEQ(conf->mzx_speed, 4, "");
  }

  SECTION(IncludeEquals)
  {
    // This version works from both the config file and the command line.
    char include_conf[] = "include=" UNIT_TMP_DIR "/c.cnf";
    boolean ret = write_config(UNIT_TMP_DIR "/c.cnf", "mzx_speed = 6");
    ASSERTEQ(ret, true, "");

    conf->mzx_speed = 1;

    load_arg(include_conf);
    ASSERTEQ(conf->mzx_speed, 6, "%s", include_conf);

    conf->mzx_speed = 2;

    load_arg_file(include_conf, false);
    ASSERTEQ(conf->mzx_speed, 6, "%s", include_conf);
  }

  SECTION(Recursion)
  {
    // Make sure a sane amount of recursive includes work.
    // Also make sure this works for both the main and editor configuration.
    boolean ret = write_config(UNIT_TMP_DIR "/a.cnf", "include " UNIT_TMP_DIR "/b.cnf");
    ret &= write_config(UNIT_TMP_DIR "/b.cnf", "include " UNIT_TMP_DIR "/c.cnf");
    ret &= write_config(UNIT_TMP_DIR "/c.cnf", "mzx_speed=5\nboard_editor_hide_help=1");
    ASSERTEQ(ret, true, "");

    conf->mzx_speed = 1;
#ifdef CONFIG_EDITOR
    econf->board_editor_hide_help = false;
#endif

    load_arg((char *)"include=" UNIT_TMP_DIR "/a.cnf");
    ASSERTEQ(conf->mzx_speed, 5, "");
#ifdef CONFIG_EDITOR
    ASSERTEQ(econf->board_editor_hide_help, true, "");
#endif
  }

  SECTION(RecursionLimit)
  {
    // Make sure recursive include fails gracefully. Even before the hard limit
    // was added this would happen due to MZX hitting the maximum allowed number
    // of file descriptors prior to running out of stack. This is pretty much
    // a freebie, it just needs to not crash or run out of memory.
    boolean ret = write_config(UNIT_TMP_DIR "/a.cnf", "include " UNIT_TMP_DIR "/a.cnf");
    ASSERTEQ(ret, true, "");

    set_config_from_file(SYSTEM_CNF, UNIT_TMP_DIR "/a.cnf");
  }
}

#ifdef CONFIG_EDITOR

/*
UNITTEST(ExtendedMacros)
{
  // FIXME
  UNIMPLEMENTED();

  free_editor_config();
}
*/

#endif /* CONFIG_EDITOR */
