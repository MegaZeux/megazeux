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

#include "audio.h"
#include "audio_pcs.h"
#include "audio_sfx.h"
#include "audio_struct.h"

#include "../configure.h"

#include <string.h>

struct pc_speaker_stream
{
  struct audio_stream a;
  uint32_t volume;
  uint32_t playing;
  uint32_t frequency;
  uint32_t last_frequency;
  uint32_t note_duration;
  uint32_t last_duration;
  uint32_t last_playing;
  uint32_t sample_cutoff;
  uint32_t last_increment_buffer;
};

static boolean pcs_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t dest_frames, unsigned int dest_channels)
{
  struct pc_speaker_stream *pcs_stream = (struct pc_speaker_stream *)a_src;
  uint32_t offset = 0, i, j;
  uint32_t sample_duration = pcs_stream->last_duration;
  uint32_t end_duration;
  uint32_t increment_value, increment_buffer;
  uint32_t sfx_scale = (pcs_stream->volume ? pcs_stream->volume + 1 : 0) * 32;
  uint32_t sfx_scale_half = sfx_scale / 2;
  int32_t *mix_dest_ptr = buffer;
  int16_t cur_sample;

  /**
   * Cancel the current playing note if PC speaker effects were turned off or
   * if the sfx queue was cleared.
   */
  if(!audio_get_pcs_on() || audio_sfx_should_cancel_note())
  {
    pcs_stream->last_playing = 0;
    pcs_stream->last_duration = 0;
    sample_duration = 0;
  }

  if(sample_duration >= dest_frames)
  {
    end_duration = dest_frames;
    pcs_stream->last_duration = sample_duration - end_duration;
    sample_duration = end_duration;
  }

  if(pcs_stream->last_playing)
  {
    increment_value =
     (uint32_t)((double)pcs_stream->last_frequency /
     (audio.output_frequency) * 4294967296.0);

    increment_buffer = pcs_stream->last_increment_buffer;

    for(i = 0; i < sample_duration; i++)
    {
      cur_sample = (uint32_t)((increment_buffer & 0x80000000) >> 31) *
       sfx_scale - sfx_scale_half;
      for(j = 0; j < dest_channels; j++)
        *(mix_dest_ptr++) += cur_sample;
      increment_buffer += increment_value;
    }
    pcs_stream->last_increment_buffer = increment_buffer;
  }
  else
  {
    mix_dest_ptr += (sample_duration * dest_channels);
  }

  offset += sample_duration;

  if(offset < dest_frames)
    pcs_stream->last_playing = 0;

  while(offset < dest_frames)
  {
    int frequency, note_duration;
    pcs_stream->playing = audio_sfx_get_next_sound(&frequency, &note_duration);
    pcs_stream->frequency = frequency;
    pcs_stream->note_duration = note_duration;

    // Minimum note duration is 1 to prevent locking up the audio thread.
    if(pcs_stream->note_duration < 1)
      pcs_stream->note_duration = 1;

    sample_duration = (uint32_t)((double)audio.output_frequency / 500.0 *
     pcs_stream->note_duration);

    if(offset + sample_duration >= dest_frames)
    {
      end_duration = dest_frames - offset;
      pcs_stream->last_duration = sample_duration - end_duration;
      pcs_stream->last_playing = pcs_stream->playing;
      pcs_stream->last_frequency = pcs_stream->frequency;

      sample_duration = end_duration;
    }

    offset += sample_duration;

    if(pcs_stream->playing)
    {
      increment_value =
       (uint32_t)((double)pcs_stream->frequency /
       audio.output_frequency * 4294967296.0);
      increment_buffer = 0;

      for(i = 0; i < sample_duration; i++)
      {
        cur_sample = (uint32_t)((increment_buffer & 0x80000000) >> 31) *
         sfx_scale - sfx_scale_half;
        for(j = 0; j < dest_channels; j++)
          *(mix_dest_ptr++) += cur_sample;
        increment_buffer += increment_value;
      }
      pcs_stream->last_increment_buffer = increment_buffer;
    }
    else
    {
      mix_dest_ptr += (sample_duration * dest_channels);
    }
  }

  return 0;
}

static void pcs_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  ((struct pc_speaker_stream *)a_src)->volume = volume;
}

static void pcs_destruct(struct audio_stream *a_src)
{
  destruct_audio_stream(a_src);
}

static struct audio_stream *construct_pc_speaker_stream(void)
{
  struct pc_speaker_stream *pcs_stream;
  struct audio_stream_spec a_spec;

  pcs_stream = ccalloc(1, sizeof(struct pc_speaker_stream));

  memset(&a_spec, 0, sizeof(struct audio_stream_spec));
  a_spec.mix_data   = pcs_mix_data;
  a_spec.set_volume = pcs_set_volume;
  a_spec.destruct   = pcs_destruct;

  // The volume here will be corrected after initialization...
  initialize_audio_stream((struct audio_stream *)pcs_stream, &a_spec, 255, 0);

  return (struct audio_stream *)pcs_stream;
}

void init_pc_speaker(struct config_info *conf)
{
  audio.pcs_stream = construct_pc_speaker_stream();
}
