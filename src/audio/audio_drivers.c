/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "audio_drivers.h"

#include "../util.h"

#ifdef CONFIG_SDL
#include "../platform/sdl/SDLmzx.h"
#endif

static const struct audio_driver * const available_drivers[] =
{
#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(3,0,0)
  &audio_driver_sdl3,
#else
  &audio_driver_sdl,
#endif
#endif

#ifdef CONFIG_DJGPP
  &audio_driver_sb,
  &audio_driver_lpt_mono,
  &audio_driver_lpt_stereo1,
  &audio_driver_lpt_stereo2,
  &audio_driver_lpt_dss,
  &audio_driver_no_pcm,
#endif
#ifdef CONFIG_NDS
  &audio_driver_nds,
#endif
#if defined(CONFIG_3DS) && !defined(CONFIG_SDL)
  &audio_driver_3ds,
#endif
#if defined(CONFIG_WII) && !defined(CONFIG_SDL)
  &audio_driver_wii,
#endif
#ifdef CONFIG_DREAMCAST
  &audio_driver_dreamcast,
#endif
  NULL
};

const struct audio_driver *audio_init_driver(struct config_info *conf,
 const char *name)
{
  const struct audio_driver *name_match = NULL;
  unsigned i;

  if(!available_drivers[0])
  {
    warn("no available audio drivers.\n");
    return NULL;
  }

  /* If specified, search for the named driver first. */
  if(name && name[0])
  {
    /* Special: if "none", do not initialize any driver. */
    if(!strcasecmp(name, "none"))
      return NULL;

    for(i = 0; i < ARRAY_SIZE(available_drivers); i++)
    {
      const struct audio_driver *driver = available_drivers[i];
      if(!driver)
        break;
      if(strcasecmp(driver->ident, name))
        continue;

      name_match = driver;
      break;
    }
    if(name_match && name_match->init_audio_driver(conf))
      return name_match;
  }

  /* Attempt every other driver in the list. */
  for(i = 0; i < ARRAY_SIZE(available_drivers); i++)
  {
    const struct audio_driver *driver = available_drivers[i];
    if(!driver)
      break;
    if(driver == name_match || !driver->allow_auto_init)
      continue;

    if(driver->init_audio_driver(conf))
      return driver;
  }

  return NULL;
}
