/* MegaZeux
 *
 * Copyright (C) 2018-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_players.h"
#include "audio_struct.h"

#include "../util.h"
#include "../io/vio.h"

static const struct audio_player * const available_players[] =
{
  &audio_player_sam,
  &audio_player_wav,
#ifdef CONFIG_VORBIS
  &audio_player_vorbis,
#endif
#ifdef CONFIG_REALITY
#ifdef CONFIG_DJGPP
  &audio_player_reality_hwopl,
#endif
  &audio_player_reality,
#endif

  // Generic module players may misidentify files due to the ambiguity of
  // formats like Soundtracker MOD, so register them last.
#ifdef CONFIG_XMP
  &audio_player_xmp,
#endif
#ifdef CONFIG_MODPLUG
  &audio_player_modplug,
#endif
#ifdef CONFIG_MIKMOD
  &audio_player_mikmod,
#endif
#ifdef CONFIG_OPENMPT
  &audio_player_openmpt,
#endif
  NULL
};

static struct audio_stream *try_construct(const struct audio_player *player,
 vfile *vf, const char *filename, uint32_t frequency, unsigned int volume,
 boolean is_primary, boolean repeat)
{
  struct audio_stream *stream;

  if(player->test)
  {
    boolean result = player->test(vf, filename, is_primary);
    vrewind(vf);
    if(!result)
      return NULL;
  }

  stream = player->construct(vf, filename, frequency, volume, repeat);
  if(!stream)
    vrewind(vf);

  return stream;
}

struct audio_stream *audio_construct_stream(const char *filename,
 uint32_t frequency, unsigned int volume, boolean is_primary, boolean repeat)
{
  const struct audio_player * const *driver_players;
  struct audio_stream *stream = NULL;
  vfile *vf;
  unsigned i;

  if(!audio.music_on)
    return NULL;

  // Using a buffer vastly improves module load times on some architectures.
  vf = vfopen_unsafe_ext(filename, "rb", V_LARGE_BUFFER);
  if(!vf)
    return NULL;

  driver_players = audio.driver ? audio.driver->driver_players : NULL;

  if((!driver_players || !driver_players[0]) && !available_players[0])
    warn("no available audio players.\n");

  if(driver_players)
  {
    for(i = 0; driver_players[i]; i++)
    {
      stream = try_construct(driver_players[i], vf, filename,
       frequency, volume, is_primary, repeat);

      if(stream)
        return stream;
    }
  }

  for(i = 0; i < ARRAY_SIZE(available_players) && available_players[i]; i++)
  {
    stream = try_construct(available_players[i], vf, filename,
     frequency, volume, is_primary, repeat);

    if(stream)
      return stream;
  }

  // The constructor function is responsible for closing or
  // retaining the file handle on successful loads.
  vfclose(vf);
  return NULL;
}
