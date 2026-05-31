/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <kontakt@asie.pl>
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
#include "../audio_struct.h"
#include "../../util.h"

#include <kos.h>
#include <string.h>

static int16_t *sound_buffer;
static snd_stream_hnd_t stream;
static unsigned buffer_frames;
static unsigned buffer_size;
static boolean running;
static kthread_t *sound_thread;

// `smp_req` and `smp_recv` are actually BYTES, not samples.
static void *dc_audio_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
  size_t frames = MAX(0, smp_req) / (sizeof(int16_t) * 2);
  if(frames > buffer_frames)
    frames = buffer_frames;

  frames = audio_mixer_render_frames(sound_buffer, frames, 2, SAMPLE_S16);
  *smp_recv = frames * sizeof(int16_t) * 2;
  return sound_buffer;
}

static void *dc_audio_thread(void *dud)
{
  snd_stream_init();

  sound_buffer = cmalloc(buffer_size);

  stream = snd_stream_alloc(dc_audio_callback, buffer_size);

  snd_stream_start(stream, audio.output_frequency, 1);

  while(running)
  {
    snd_stream_poll(stream);
    thd_sleep(10);
  }

  snd_stream_destroy(stream);
  snd_stream_shutdown();

  free(sound_buffer);

  return NULL;
}

void init_audio_platform(struct config_info *conf)
{
  if(!audio_mixer_init(conf->audio_sample_rate, conf->audio_buffer_samples, 2))
    return;

  buffer_frames = audio.buffer_frames;
  buffer_size = buffer_frames * sizeof(int16_t) * 2 /* stereo */;

  sound_thread = thd_create(0, dc_audio_thread, NULL);
  if(sound_thread)
    running = true;
}

void quit_audio_platform(void)
{
  if(running)
  {
    running = false;
    thd_join(sound_thread, NULL);
  }
}
