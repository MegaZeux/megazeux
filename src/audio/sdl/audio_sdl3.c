/* MegaZeux
 *
 * Copyright (C) 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../../util.h"
#include "../../platform/sdl/SDLmzx.h"

#include "../audio.h"
#include "../audio_struct.h"

static SDL_AudioSpec audio_settings;
static SDL_AudioStream *audio_stream;
static void *audio_buffer;
static unsigned audio_format;

static void sdl_audio_callback(void *userdata, SDL_AudioStream *stream,
 int required_bytes, int requested_bytes)
{
  size_t framesize = SDL_AUDIO_FRAMESIZE(audio_settings);
  size_t frames = MAX(0, required_bytes) / framesize;

  //fprintf(mzxerr, "  #:%d fr:%zu frsz:%zu\n", required_bytes, frames, framesize);
  while(frames > 0)
  {
    size_t out = audio_mixer_render_frames(userdata, frames,
     audio_settings.channels, audio_format);

    assert(out <= frames);
    SDL_PutAudioStreamData(audio_stream, userdata, out * framesize);
    frames -= out;
  }
}

void init_audio_platform(struct config_info *conf)
{
  SDL_AudioDeviceID audio_device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  int frames = 0;
  char hint[16];
  void *tmp;
  // TODO: 8-bit?

  debug("--AUDIO_DRIVER_SDL3-- init_audio_platform\n");

  audio_format = SAMPLE_S16;

  memset(&audio_settings, 0, sizeof(audio_settings));
  if(!SDL_GetAudioDeviceFormat(audio_device, &audio_settings, &frames))
  {
    debug("--AUDIO_DRIVER_SDL3-- failed to query default device format: %s\n",
     SDL_GetError());
    // Can't query, try to continue anyway...
    audio_settings.freq = 48000;
    frames = 1024;
  }
  audio_settings.format = SDL_AUDIO_S16;
  audio_settings.channels = 2;

  if(conf->audio_sample_rate != 0)
  {
    // Reject very low sample rates to avoid PulseAudio hangs.
    audio_settings.freq = MAX(conf->audio_sample_rate, 2048);
  }

  if(conf->audio_buffer_samples != 0)
    frames = conf->audio_buffer_samples;

  if(conf->audio_output_channels != 0)
    audio_settings.channels = MIN(conf->audio_output_channels, 2);

  if(!audio_mixer_init(audio_settings.freq, frames, audio_settings.channels))
    return;

  audio_settings.freq = audio.output_frequency;

  tmp = crealloc(audio_buffer,
   audio.buffer_frames * audio.buffer_channels * sizeof(int16_t));
  if(!tmp)
  {
    warn("--AUDIO_DRIVER_SDL3-- failed to allocate buffer\n");
    return;
  }

  // The buffer frames need to be configured with this hack.
  // This value may be ignored or modified by some platforms.
  snprintf(hint, sizeof(hint), "%u", audio.buffer_frames);
  SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, hint);

  debug("--AUDIO_DRIVER_SDL3-- initializing default audio device %" PRIu32 ": "
   "S16 %dch %dHz %dfr\n", audio_device, audio_settings.channels,
   audio_settings.freq, audio.buffer_frames);

  audio_buffer = tmp;
  audio_stream = SDL_OpenAudioDeviceStream(audio_device, &audio_settings,
   sdl_audio_callback, tmp);
  if(!audio_stream)
  {
    warn("--AUDIO_DRIVER_SDL3-- failed to initialize default audio device: %s\n",
     SDL_GetError());
    goto err;
  }

  audio_device = SDL_GetAudioStreamDevice(audio_stream);
  if(audio_device == 0)
  {
    debug("--AUDIO_DRIVER_SDL3-- failed to get device ID to resume: %s\n",
     SDL_GetError());
    audio_device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  }

  if(SDL_ResumeAudioDevice(audio_device))
  {
    debug("--AUDIO_DRIVER_SDL3-- audio device %" PRIu32
     " initialized successfully\n", audio_device);
  }
  else
  {
    warn("--AUDIO_DRIVER_SDL3-- failed to resume audio device %" PRIu32 ": %s\n",
     audio_device, SDL_GetError());
  }
  return;

err:
  free(tmp);
  audio_buffer = NULL;
  return;
}

void quit_audio_platform(void)
{
  if(audio_stream)
  {
    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
  }
  free(audio_buffer);
  audio_buffer = NULL;
}
