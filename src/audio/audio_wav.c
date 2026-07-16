/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_players.h"
#include "audio_struct.h"
#include "audio_wav.h"
#include "audio_wav_load.h"
#include "sampled_stream.h"

#include "../util.h"
#include "../io/path.h"
#include "../io/vio.h"

struct wav_stream
{
  struct sampled_stream s;
  uint8_t *wav_data;
  uint32_t data_offset;
  uint32_t data_length;
  uint32_t channels;
  uint32_t bytes_per_sample;
  uint32_t natural_frequency;
  uint32_t loop_start;
  uint32_t loop_end;
  enum wav_format format;
};

static uint32_t wav_read_data(struct wav_stream *w_stream,
 uint8_t * RESTRICT buffer, uint32_t len, boolean repeat)
{
  const uint8_t *src = (uint8_t *)w_stream->wav_data + w_stream->data_offset;
  uint32_t data_read = 0;
  uint32_t read_len = len;
  uint32_t new_offset = w_stream->data_offset;
  uint32_t i;

  switch(w_stream->format)
  {
    case SAMPLE_S8:
    case SAMPLE_U8:
    {
      int16_t *dest = (int16_t *)buffer;

      read_len /= 2;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end) &&
       (w_stream->data_offset + read_len >= w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

      data_read = read_len * 2;

      if(w_stream->format == SAMPLE_U8)
      {
        for(i = 0; i < read_len; i++)
          dest[i] = (int16_t)((src[i] - 128) << 8);
      }
      else
      {
        for(i = 0; i < read_len; i++)
          dest[i] = (int16_t)(src[i] << 8);
      }

      break;
    }

    case SAMPLE_S16LE:
    case SAMPLE_S16BE:
    {
      uint8_t *dest = (uint8_t *)buffer;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end)
       && (w_stream->data_offset + read_len >= w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

      if(w_stream->format != SAMPLE_S16)
      {
        // Swap bytes to match the current platform endianness...
        for(i = 0; i < read_len; i += 2)
        {
          dest[i] = src[i + 1];
          dest[i + 1] = src[i];
        }
      }
      else
        memcpy(dest, src, read_len);

      data_read = read_len;
      break;
    }
  }

  w_stream->data_offset = new_offset;

  return data_read;
}

static boolean wav_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t frames, unsigned int channels)
{
  uint32_t read_len = 0;
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  uint8_t *read_buffer;
  size_t read_wanted;

  read_buffer = (uint8_t *)sampled_get_buffer(&w_stream->s, &read_wanted);

  read_len = wav_read_data(w_stream, read_buffer, read_wanted, a_src->repeat);

  if(read_len < read_wanted)
  {
    read_buffer += read_len;
    read_wanted -= read_len;

    if(a_src->repeat)
    {
      read_len = wav_read_data(w_stream, read_buffer, read_wanted, true);
    }
    else
    {
      memset(read_buffer, 0, read_wanted);
      read_len = 0;
    }
  }

  sampled_mix_data((struct sampled_stream *)w_stream, buffer, frames, channels);

  if(read_len == 0)
    return true;

  return false;
}

static void wav_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  a_src->volume = volume * 256 / 255;
}

static void wav_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  a_src->repeat = repeat;
}

static void wav_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  if(position < (w_stream->data_length / w_stream->bytes_per_sample))
    w_stream->data_offset = position * w_stream->bytes_per_sample;
}

static void wav_set_loop_start(struct audio_stream *a_src, uint32_t position)
{
  ((struct wav_stream *)a_src)->loop_start = position;
}

static void wav_set_loop_end(struct audio_stream *a_src, uint32_t position)
{
  ((struct wav_stream *)a_src)->loop_end = position;
}

static void wav_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  struct wav_stream *w_stream = (struct wav_stream *)s_src;

  sampled_set_buffer(s_src, frequency, w_stream->natural_frequency);
}

static uint32_t wav_get_position(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  return w_stream->data_offset / w_stream->bytes_per_sample;
}

static uint32_t wav_get_length(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  return w_stream->data_length / w_stream->bytes_per_sample;
}

static uint32_t wav_get_loop_start(struct audio_stream *a_src)
{
  return ((struct wav_stream *)a_src)->loop_start;
}

static uint32_t wav_get_loop_end(struct audio_stream *a_src)
{
  return ((struct wav_stream *)a_src)->loop_end;
}

static uint32_t wav_get_frequency(struct sampled_stream *s_src)
{
  return s_src->relative_frequency;
}

static void wav_destruct(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  free(w_stream->wav_data);
  sampled_destruct(a_src);
}

struct audio_stream *construct_wav_stream_direct(struct wav_info *w_info,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct wav_stream *w_stream;

  w_stream = (struct wav_stream *)malloc(sizeof(struct wav_stream));
  if(!w_stream)
  {
    audio_free_wav(w_info);
    return NULL;
  }
  w_stream->s.a.player = &audio_player_wav;

  w_stream->wav_data = w_info->wav_data;
  w_stream->data_length = w_info->data_length;
  w_stream->channels = w_info->channels;
  w_stream->data_offset = 0;
  w_stream->format = w_info->format;
  w_stream->natural_frequency = w_info->freq;
  w_stream->bytes_per_sample = w_info->channels;
  w_stream->loop_start = w_info->loop_start;
  w_stream->loop_end = w_info->loop_end;

  /**
   * Due to a bug in the old SAM to WAV conversion code, the frequency provided
   * has been halved with respect to what it should have been in DOS versions.
   * If this wav spec was loaded directly from a SAM or via audio_spot_sample,
   * reverse this "fix" that is now a permanent compatibility concern.
   */
  if(w_info->enable_sam_frequency_hack)
    frequency *= 2;

  if((w_info->format != SAMPLE_U8) && (w_info->format != SAMPLE_S8))
    w_stream->bytes_per_sample *= 2;

  initialize_sampled_stream((struct sampled_stream *)w_stream,
   w_info->freq, frequency, w_info->channels, true);

  initialize_audio_stream((struct audio_stream *)w_stream, volume, repeat);

  return (struct audio_stream *)w_stream;
}

static struct audio_stream *wav_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_src;
  struct wav_info w_info;
  memset(&w_info, 0, sizeof(struct wav_info));

  if(!audio_load_wav(&w_info, vf, filename))
    return NULL;

  // Surround WAVs not supported yet..
  if(w_info.channels > 2)
    return NULL;

  a_src = construct_wav_stream_direct(&w_info, frequency, volume, repeat);
  if(a_src)
    vfclose(vf);

  return a_src;
}

static struct audio_stream *sam_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_src;
  struct wav_info w_info;
  memset(&w_info, 0, sizeof(struct wav_info));

  if(!audio_load_sam(&w_info, vf, filename))
    return NULL;

  a_src = construct_wav_stream_direct(&w_info, frequency, volume, repeat);
  if(a_src)
    vfclose(vf);

  return a_src;
}

static boolean wav_test(vfile *vf, const char *filename, boolean is_primary)
{
  char buf[12];
  if(!vfread(buf, 12, 1, vf))
    return false;

  if(memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
    return false;

  return true;
}

static boolean sam_test(vfile *vf, const char *filename, boolean is_primary)
{
  // .SAM files are raw 8363Hz signed 8-bit samples and can only be identified
  // by their .SAM extension.
  ssize_t ext_pos = path_get_ext_offset(filename);
  if(ext_pos < 0)
    return false;

  return strcasecmp(filename + ext_pos, ".sam") == 0;
}

const struct audio_player audio_player_wav =
{
  "WAV",
  wav_test,

  wav_construct,
  wav_destruct,
  wav_mix_data,
  wav_set_volume,
  wav_set_repeat,
  NULL,
  NULL,
  wav_set_position,
  wav_get_position,
  wav_get_length,
  wav_set_loop_start,
  wav_get_loop_start,
  wav_set_loop_end,
  wav_get_loop_end,
  wav_set_frequency,
  wav_get_frequency,
  NULL,
};

const struct audio_player audio_player_sam =
{
  "SAM",
  sam_test,

  sam_construct,
  wav_destruct,
  wav_mix_data,
  wav_set_volume,
  wav_set_repeat,
  NULL,
  NULL,
  wav_set_position,
  wav_get_position,
  wav_get_length,
  wav_set_loop_start,
  wav_get_loop_start,
  wav_set_loop_end,
  wav_get_loop_end,
  wav_set_frequency,
  wav_get_frequency,
  NULL,
};
