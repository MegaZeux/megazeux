/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#define BOOL _BOOL
#include <asndlib.h>
#include <gctypes.h>
#include <ogc/lwp.h>
#include <ogc/cache.h>
#include <ogc/system.h>
#undef BOOL

#define STACKSIZE 8192

static lwpq_t audio_queue;
static lwp_t audio_thread;
static u8 audio_stack[STACKSIZE];
static int16_t *audio_buffer[3];
static volatile int current = 0;
static volatile int audio_stop = 0;
static unsigned buffer_frames;
static unsigned buffer_size;
static boolean active = false;

static void *wii_audio_thread(void *dud)
{
  while(!audio_stop)
  {
    LWP_ThreadSleep(audio_queue);
    audio_mixer_render_frames(audio_buffer[current], buffer_frames, 2, SAMPLE_S16);
    DCFlushRange(audio_buffer[current], buffer_size);
    current = (current + 1) % 3;
  }

  return 0;
}

static void voice_callback(s32 voice)
{
  ASND_AddVoice(voice, audio_buffer[(current + 2) % 3], buffer_size);
  LWP_ThreadSignal(audio_queue);
}

void init_audio_platform(struct config_info *conf)
{
  // buffer size must be multiple of 32 bytes, so samples must be multiple of 8
  unsigned frames = (conf->audio_buffer_samples + 7) & ~7;
  int i;

  if(!audio_mixer_init(conf->audio_sample_rate, frames, 2))
    return;

  buffer_frames = audio.buffer_frames;
  buffer_size = buffer_frames * sizeof(int16_t) * 2 /* stereo */;

  for(i = 0; i < 3; i++)
  {
    audio_buffer[i] = memalign(32, buffer_size);
    memset(audio_buffer[i], 0, buffer_size);
    DCFlushRange(audio_buffer[i], buffer_size);
  }

  ASND_Init();
  LWP_InitQueue(&audio_queue);
  if(LWP_CreateThread(&audio_thread, wii_audio_thread, NULL, audio_stack,
   STACKSIZE, 80) >= 0)
  {
    ASND_SetVoice(0, VOICE_STEREO_16BIT, audio.output_frequency, 0,
     audio_buffer[0], buffer_size, 255, 255, voice_callback);
    ASND_Pause(0);
    active = true;
  }
}

void quit_audio_platform(void)
{
  void *dud;

  if(!active)
    return;

  active = false;
  audio_stop = 1;
  LWP_JoinThread(audio_thread, &dud);
  ASND_Pause(1);
  ASND_End();
  LWP_CloseQueue(audio_queue);
  // Don't free hardware audio buffers
  // Memory allocated with memalign() can't neccessarily be free()'d
}
