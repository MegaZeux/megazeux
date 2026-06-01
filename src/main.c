/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A. (Koji) - Koji_Takeo@worldmailer.com
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "platform/platform.h"
#include "run_stubs.h"

#include "configure.h"
#include "core.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "game.h"
#include "graphics.h"
#include "helpsys.h"
#include "idput.h"
#include "sfx.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "io/path.h"
#include "io/vio.h"

#include "audio/audio.h"
#include "event/event.h"
#include "network/network.h"

#ifdef CONFIG_SDL
#if CONFIG_SDL == 3
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <SDL3/SDL_main.h>
#else
#include <SDL.h> /* SDL_main */
#endif
#endif

#ifndef VERSION
#error Must define VERSION for MegaZeux version string
#endif

#ifndef VERSION_DATE
#define VERSION_DATE
#endif

#define CAPTION "MegaZeux " VERSION VERSION_DATE

#ifdef __WIN32__
// Export symbols to indicate MegaZeux would be prefer to be run with
// the better video card on switchable graphics platforms.

__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001; // Nvidia
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // AMD
#endif

static char startup_dir[MAX_PATH];

#ifdef CONFIG_PLEDGE
/**
 * Experimental OpenBSD pledge(2) support.
 * This has to be done after init_video and possibly after init_audio.
 *
 * FIXME MZX will abort if pretty much anything happens that requires
 * destroying or creating a window (fullscreen, renderer switching, exiting).
 * Additionally, in OpenBSD 6.5, Mesa seems to use something that aborts
 * when drawing the current frame, so the GL renderers only work in 6.4 or
 * earlier. I don't think there's anything that can be done about this in
 * the near future.
 */
static void init_pledge(void)
{
  static const char * const promises = "stdio rpath wpath cpath audio drm";

  debug("Promises: '%s'\n", promises);
  if(pledge(promises, ""))
    perror("Pledge failed");
}
#endif

#if defined(CONFIG_UPDATER) && defined(__WIN32__)
#define CAN_RESTART
static char **rewrite_argv_for_execv(int argc, char **argv)
{
  char **new_argv = cmalloc((argc+1) * sizeof(char *));
  char *arg;
  int length;
  int pos;
  int i;
  int i2;

  // Due to a bug in execv, args with spaces present are treated as multiple
  // args in the new process. Each arg in argv must be wrapped in double quotes
  // to work properly. Because of this, " and \ must also be escaped.

  for(i = 0; i < argc; i++)
  {
    length = strlen(argv[i]);
    arg = cmalloc(length * 2 + 2);
    arg[0] = '"';

    for(i2 = 0, pos = 1; i2 < length; i2++, pos++)
    {
      switch(argv[i][i2])
      {
        case '"':
        case '\\':
          arg[pos] = '\\';
          pos++;
          break;
      }
      arg[pos] = argv[i][i2];
    }

    arg[pos] = '"';
    arg[pos + 1] = '\0';

    new_argv[i] = arg;
  }

  new_argv[argc] = NULL;

  return new_argv;
}
#endif /* CONFIG_UPDATER && __WIN32__ */

#ifdef __amigaos__
#define __libspec LIBSPEC
#else
#define __libspec
#endif

__libspec int main(int argc, char *argv[])
{
  char *_backup_argv[] = { (char*) SHAREDIR "/megazeux" };
  int err = 1;

  core_context *core_data;
  struct config_info *conf;
#ifdef CAN_RESTART
  boolean need_restart = false;
#endif

  // Keep this 7.2k structure off the stack..
  static struct world mzx_world;

  if(!platform_init())
    goto err_out;

  check_alloc_init();

  // We need to store the current working directory so it's
  // always possible to get back to it..
  vgetcwd(startup_dir, MAX_PATH);
  snprintf(current_dir, MAX_PATH, "%s", startup_dir);

#ifdef CONFIG_STDIO_REDIRECT
  // Do this after platform_init() since, even though platform_init() might
  // log stuff, it actually initializes the filesystem on some platforms.
  if(!redirect_stdio_init(startup_dir, true))
    if(!redirect_stdio_init(SHAREDIR, false))
      redirect_stdio_init(getenv("HOME"), false);
#endif

#ifdef __APPLE__
  if(!strcmp(current_dir, "/") || !strncmp(current_dir, "/App", 4))
  {
    // Mac .APPs start at / and we don't like that.
    char *user = getlogin();
    snprintf(current_dir, MAX_PATH, "/Users/%s", user);
  }
#endif // __APPLE__

#ifdef ANDROID
  // Accept argv[1] passed in from the Java side as the "intended" argv[0].
  if(argc >= 2)
  {
    argv++;
    argc--;
  }
#endif

#ifdef STARTUPDIR
  // Some ports (Android and Vita) require a custom startup directory.
  path_navigate(current_dir, MAX_PATH, STARTUPDIR);
#endif

  // argc may be 0 on e.g. some Wii homebrew loaders.
#if !defined(CONFIG_WIIU) && !defined(CONFIG_PSVITA)
  if(argc == 0)
#endif
  {
    argv = _backup_argv;
    argc = 1;
  }

  if(mzx_res_init(argv[0], is_editor()))
    goto err_free_res;

  // Figure out where all configuration files should be loaded
  // form. For game.cnf, et al this should eventually be wrt
  // the game directory, not the config.txt's path.
  path_get_directory(config_dir, MAX_PATH, mzx_res_get_by_id(CONFIG_TXT));

  // Move into the configuration file's directory so that any
  // "include" statements are done wrt this path. Move back
  // into the "current" directory after loading.
  vchdir(config_dir);

  default_config();
  default_editor_config();
  set_config_from_file(SYSTEM_CNF, mzx_res_get_by_id(CONFIG_TXT));
  set_config_from_command_line(&argc, argv);
  conf = get_config();

  store_editor_config_backup();

  init_macros();

  // Startup path might be relative, so change back before checking it.
  vchdir(current_dir);

  // At this point argv should have all the config options
  // of the form var=value removed, leaving only unparsed
  // parameters. Interpret the first unparsed parameter
  // as a file to load (overriding startup_file etc.)
  if(argc > 1 && argv != _backup_argv)
    set_config_startup_path_and_file(argv[1]);

  if(strlen(conf->startup_path))
  {
    debug("Config: Using startup path '%s'\n", conf->startup_path);
    snprintf(current_dir, MAX_PATH, "%s", conf->startup_path);
  }

  vchdir(current_dir);

  // Initialize the VFS in the final startup directory.
  if(conf->vfs_enable)
  {
    if(!vio_filesystem_init(conf->vfs_max_cache_size,
     conf->vfs_max_cache_file_size, conf->vfs_enable_auto_cache))
      warn("failed to initialize virtual filesystem!\n");
  }

  counter_fsg();

  rng_seed_init();

  mouse_size(8, 14);

  init_event(conf);

  if(!init_video(conf, CAPTION))
    goto err_free_config;
  init_audio(conf);

  /* Loads charsets -> should be performed after video init. */
  editor_init();

#ifdef CONFIG_PLEDGE
  init_pledge();
#endif

  core_data = core_init(&mzx_world);

  if(network_layer_init(conf))
  {
#ifdef CONFIG_UPDATER
    if(is_updater())
    {
      if(updater_init())
      {
        // No auto update checks on repo builds.
        if((strstr(VERSION, "GIT") || strstr(VERSION, "PRE")) &&
         !strcmp(conf->update_branch_pin, "Stable"))
          conf->update_auto_check = UPDATE_AUTO_CHECK_OFF;

        if(conf->update_auto_check)
          if(check_for_updates((context *)core_data, true))
            goto update_restart_mzx;
      }
      else
        info("Updater disabled.\n");
    }
#endif
  }
  else
    info("Network layer disabled.\n");

  warp_mouse(39, 12);
  cursor_off();
  default_scroll_values(&mzx_world);

#ifdef CONFIG_HELPSYS
  help_open(&mzx_world, mzx_res_get_by_id(MZX_HELP_FIL));
#endif

  snprintf(curr_file, MAX_PATH, "%s", conf->startup_file);
  snprintf(curr_sav, MAX_PATH, "%s", conf->default_save_name);
  mzx_world.mzx_speed = conf->mzx_speed;

  // Run main game (mouse is hidden and palette is faded)
  title_screen((context *)core_data);
  core_run(core_data);

  vquick_fadeout();

  if(mzx_world.active)
  {
    clear_world(&mzx_world);
    clear_global_data(&mzx_world);
  }

#ifdef CONFIG_HELPSYS
  help_close(&mzx_world);
#endif

#ifdef CONFIG_UPDATER
update_restart_mzx:
#endif
#ifdef CAN_RESTART
  need_restart = core_restart_requested(core_data);
#endif
  core_free(core_data);
  network_layer_exit(conf);
  quit_audio();

#ifdef CAN_RESTART
  // TODO: eventually any platform with execv will need to be able to allow
  // this for config/standalone-invoked restarts. Locking it to the updater
  // for now because that's the only thing that currently uses it.
  if(need_restart)
  {
    char **new_argv = rewrite_argv_for_execv(argc, argv);

    info("Attempting to restart MegaZeux...\n");
    if(!vchdir(startup_dir))
    {
      execv(argv[0], (const void *)new_argv);
      perror("execv");
    }

    error_message(E_INVOKE_SELF_FAILED, 0, NULL);
  }
#endif

  quit_video();

  err = 0;
err_free_config:
  // FIXME maybe shouldn't be here...?
  if(mzx_world.update_done)
    free(mzx_world.update_done);
  vio_filesystem_exit();
  free_config();
  free_editor_config();
err_free_res:
#ifdef CONFIG_STDIO_REDIRECT
  redirect_stdio_exit();
#endif
  mzx_res_free();
  platform_quit();
err_out:
#ifdef CONFIG_DJGPP
  chdir(startup_dir);
#endif
  return err;
}
