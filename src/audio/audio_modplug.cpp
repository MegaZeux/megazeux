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

#include <stdint.h>
#include <modplug.h>
#include <stdafx.h>
#include <sndfile.h>

#include "audio.h"
#include "audio_players.h"
#include "audio_struct.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../configure.h"
#include "../util.h"
#include "../io/fsafeopen.h"
#include "../io/vio.h"

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

struct modplug_stream
{
  struct sampled_stream s;
  ModPlugFile *module_data;
};

static void init_modplug_settings(void)
{
  const struct config_info *conf = get_config();
  ModPlug_Settings mod_settings;

  memset(&mod_settings, 0, sizeof(ModPlug_Settings));

  if(conf->oversampling_on)
    mod_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;

  mod_settings.mFrequency = audio.output_frequency;
  mod_settings.mChannels = audio.buffer_channels < 2 ? 1 : 2;
  mod_settings.mBits = 16;

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_NEAREST;
      break;

    case RESAMPLE_MODE_LINEAR:
    default:
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
      break;

    case RESAMPLE_MODE_CUBIC:
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
      break;

    case RESAMPLE_MODE_FIR:
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
      break;
  }

  mod_settings.mLoopCount = -1;

  ModPlug_SetSettings(&mod_settings);
}

static boolean mp_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t frames, unsigned int channels)
{
  struct modplug_stream *mp_stream = (struct modplug_stream *)a_src;
  void *read_buffer;
  size_t read_wanted;
  size_t read_len;
  boolean r_val = false;

  read_buffer = sampled_get_buffer(&mp_stream->s, &read_wanted);

  read_len = ModPlug_Read(mp_stream->module_data, read_buffer, read_wanted);

  if(!a_src->volume)
  {
    // Modplug doesn't support a volume of zero; the lowest volume usable
    // via SetMasterVolume is 1. If the volume is 0, zero the buffer we
    // just read. We read the buffer to ensure the mod still progresses
    // in the background, maintaining the old semantics.
    memset(read_buffer, 0, read_len);
  }

  if(read_len < read_wanted)
  {
    // Modplug still fails to repeat on some mods, even after already
    // restarting it still ends the thing (even fades it). This won't
    // eliminate the fade but at least it'll stop the mod from
    // ending..

    if(a_src->repeat)
    {
      a_src->player->set_position(a_src, a_src->player->get_position(a_src));
    }
    else
    {
      // FIXME: I think this memset should always be done?
      memset((uint8_t *)read_buffer + read_len, 0, read_wanted - read_len);
      r_val = true;
    }

    read_len = 0;
  }

  sampled_mix_data((struct sampled_stream *)mp_stream, buffer, frames, channels);

  return r_val;
}

static void mp_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->volume = volume;
  mp_file->mSoundFile.SetMasterVolume(volume);
}

static void mp_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mp_file->mSoundFile.SetRepeatCount(-1);
  else
    mp_file->mSoundFile.SetRepeatCount(0);
}

static void mp_set_order(struct audio_stream *a_src, uint32_t order)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentOrder(order);
}

static void mp_set_position(struct audio_stream *a_src, uint32_t position)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentPos(position);
}

static void mp_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  sampled_set_buffer(s_src, frequency, 44100);
}

static uint32_t mp_get_order(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentOrder();
}

static uint32_t mp_get_position(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentPos();
}

static uint32_t mp_get_length(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetMaxPosition();
}

static uint32_t mp_get_frequency(struct sampled_stream *s_src)
{
  return s_src->relative_frequency;
}

static void mp_destruct(struct audio_stream *a_src)
{
  struct modplug_stream *mp_stream = (struct modplug_stream *)a_src;
  ModPlug_Unload(mp_stream->module_data);
  sampled_destruct(a_src);
}

static struct audio_stream *mp_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct modplug_stream *mp_stream;
  void *input_buffer;

  size_t file_size = vfilelength(vf, false);
  ModPlugFile *open_file;

  init_modplug_settings();

  input_buffer = malloc(file_size);
  if(!input_buffer)
    return NULL;

  if(!vfread(input_buffer, file_size, 1, vf))
  {
    free(input_buffer);
    return NULL;
  }

  open_file = ModPlug_Load(input_buffer, file_size);
  free(input_buffer);

  if(!open_file)
    return NULL;

  mp_stream = (struct modplug_stream *)malloc(sizeof(struct modplug_stream));
  if(!mp_stream)
  {
    ModPlug_Unload(open_file);
    return NULL;
  }
  mp_stream->s.a.player = &audio_player_modplug;

  mp_stream->module_data = open_file;

  initialize_sampled_stream((struct sampled_stream *)mp_stream,
   audio.output_frequency, frequency, 2, false);

  initialize_audio_stream((struct audio_stream *)mp_stream, volume, repeat);

  vfclose(vf);
  return (struct audio_stream *)mp_stream;
}

const struct audio_player audio_player_modplug =
{
  "libmodplug",
  NULL,

  mp_construct,
  mp_destruct,
  mp_mix_data,
  mp_set_volume,
  mp_set_repeat,
  mp_set_order,
  mp_get_order,
  mp_set_position,
  mp_get_position,
  mp_get_length,
  NULL,
  NULL,
  NULL,
  NULL,
  mp_set_frequency,
  mp_get_frequency,
  NULL,
};
