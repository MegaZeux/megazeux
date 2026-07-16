/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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
#include "../../platform/3ds/platform_3ds.h"

#include <3ds.h>
#include <string.h>

static u8 *audio_buffer;
static ndspWaveBuf ndsp_buffer[2];
static unsigned buffer_frames;
static unsigned buffer_size;
static bool soundFillBlock = false;

static void ndsp_callback(void *dud)
{
  if(ndsp_buffer[soundFillBlock].status == NDSP_WBUF_DONE)
  {
    audio_mixer_render_frames(ndsp_buffer[soundFillBlock].data_pcm16,
     buffer_frames, 2, SAMPLE_S16);

    DSP_FlushDataCache(ndsp_buffer[soundFillBlock].data_pcm8, buffer_size);
    ndspChnWaveBufAdd(0, &ndsp_buffer[soundFillBlock]);
    soundFillBlock = !soundFillBlock;
  }
}

static boolean init_audio_driver_3ds(struct config_info *conf)
{
  // buffer size must be multiple of 32 bytes(?), so samples must be multiple of 8
  unsigned frames = (conf->audio_buffer_samples + 7) & ~7;
  float mix[12];

  audio_buffer = NULL;
  if(!audio_mixer_init(conf->audio_sample_rate, frames, 2))
    return false;

  buffer_frames = audio.buffer_frames;
  buffer_size = buffer_frames * sizeof(int16_t) * 2 /* stereo */;

  if(ndspInit() != 0)
    return false;

  memset(mix, 0, sizeof(mix));
  mix[0] = mix[1] = 1.0f;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  ndspSetOutputCount(1);
  ndspSetMasterVol(1.0f);

  audio_buffer = clinearAlloc(buffer_size * 2, 0x80);
  memset(audio_buffer, 0, buffer_size * 2);
  ndspSetCallback(ndsp_callback, audio_buffer);

  memset(&ndsp_buffer[0], 0, sizeof(ndspWaveBuf));
  memset(&ndsp_buffer[1], 0, sizeof(ndspWaveBuf));
  ndspChnReset(0);
  ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
  ndspChnSetRate(0, audio.output_frequency);
  ndspChnSetFormat(0, NDSP_CHANNELS(2) | NDSP_ENCODING(NDSP_ENCODING_PCM16));
  ndspChnSetMix(0, mix);

  ndsp_buffer[0].data_vaddr = &audio_buffer[0];
  ndsp_buffer[0].nsamples = buffer_frames;
  ndsp_buffer[1].data_vaddr = &audio_buffer[buffer_size];
  ndsp_buffer[1].nsamples = buffer_frames;

  ndspChnWaveBufAdd(0, &ndsp_buffer[0]);
  ndspChnWaveBufAdd(0, &ndsp_buffer[1]);
  return true;
}

static void quit_audio_driver_3ds(void)
{
  if(audio_buffer)
  {
    linearFree(audio_buffer);
    audio_buffer = NULL;

    ndspExit();
  }
}

const struct audio_driver audio_driver_3ds =
{
  "3DS Audio",
  "3ds",

  NULL,
  &audio_player_pcs,
  NULL,

  init_audio_driver_3ds,
  quit_audio_driver_3ds,
};
