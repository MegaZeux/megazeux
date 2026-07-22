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

#include "../audio.h"
#include "../audio_drivers.h"
#include "../audio_players.h"
#include "../audio_struct.h"

#include "../../platform/log.h"
#include "../../platform/sdl/SDLmzx.h"

#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static SDL_AudioSpec audio_settings;
#if SDL_VERSION_ATLEAST(2,0,0)
static SDL_AudioDeviceID audio_device;
#endif

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
  // TODO: 8-bit?
  size_t frames;
  switch(audio_settings.channels)
  {
    case 0:
    case 1:
      frames = (unsigned)len / sizeof(int16_t);
      break;
    case 2:
      frames = (unsigned)len / (2u * sizeof(int16_t));
      break;
    default:
      frames = (unsigned)len / (audio_settings.channels * sizeof(int16_t));
      break;
  }
  audio_mixer_render_frames(stream, frames, audio_settings.channels, SAMPLE_S16);
}

static boolean init_audio_driver_sdl(struct config_info *conf)
{
  SDL_AudioSpec desired_spec =
  {
    conf->audio_sample_rate,
    AUDIO_S16SYS,
    conf->audio_output_channels ? conf->audio_output_channels : 2,
    0,
    conf->audio_buffer_samples,
    0,
    0,
    sdl_audio_callback,
    NULL
  };

  // Reject very low sample rates to avoid PulseAudio hangs.
  if(conf->audio_sample_rate > 0 && conf->audio_sample_rate < 2048)
    desired_spec.freq = 2048;

#if SDL_VERSION_ATLEAST(2,0,0)
  audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &audio_settings, 0);
  if(!audio_device)
  {
    warn("failed to init SDL audio device: %s\n", SDL_GetError());
    return false;
  }
#else
  SDL_OpenAudio(&desired_spec, &audio_settings);
#endif

  if(!audio_mixer_init(audio_settings.freq, audio_settings.samples,
   audio_settings.channels))
    return false;
  // If the mixer doesn't like SDL's selected config, exit to avoid bugs...
  if(audio.output_frequency != (size_t)audio_settings.freq ||
   audio.buffer_channels != audio_settings.channels)
    return false;

  // now set the audio going
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_PauseAudioDevice(audio_device, 0);
#else
  SDL_PauseAudio(0);
#endif

#ifdef __EMSCRIPTEN__
  // Safari doesn't unlock the audio context unless a sound is played
  // immediately after an input event has been fired. This takes an audio
  // context that we set up earlier in JavaScript and sets up SDL2 to use that
  // instead of the one that it creates (since SDL's context will never be
  // unlocked).
  //
  // This swap should only happen if it's required by the web browser.
  EM_ASM({ window.replaceSdlAudioContext(Module["SDL2"]); });
#endif
  return true;
}

static void quit_audio_driver_sdl(void)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  if(audio_device)
  {
    SDL_PauseAudioDevice(audio_device, 1);
    SDL_CloseAudioDevice(audio_device);
    audio_device = 0;
  }
#else
  SDL_PauseAudio(1);
#endif
}

const struct audio_driver audio_driver_sdl =
{
  "SDL Audio",
  "sdl",

  NULL,
  &audio_player_pcs,
  NULL,

  init_audio_driver_sdl,
  quit_audio_driver_sdl,
};
