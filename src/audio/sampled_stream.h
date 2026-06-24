/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_AUDIO_SAMPLED_STREAM_H
#define MEGAZEUX_AUDIO_SAMPLED_STREAM_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "audio_struct.h"

struct sampled_stream
{
  struct audio_stream a;
  uint32_t relative_frequency;
  uint32_t input_frequency;
  int16_t *output_data;
  size_t data_window_length;
  size_t allocated_data_length;
  size_t stream_offset;
  size_t channels;
  size_t dest_channels;
  unsigned bytes_per_frame;
  boolean use_volume;
  int64_t frequency_delta;
  int64_t sample_index;
};

void *sampled_get_buffer(struct sampled_stream *s_src, size_t *needed);
void sampled_set_buffer(struct sampled_stream *s_src, uint32_t rel_frequency,
 uint32_t rel_frequency_base);
void sampled_mix_data(struct sampled_stream *s_src,
 int32_t * RESTRICT dest_buffer, size_t dest_frames, unsigned int dest_channels);
void sampled_destruct(struct audio_stream *a_src);

/**
 *                      input frequency * relative frequency
 * frequency ratio = ------------------------------------------
 *                   output frequency * relative frequency base
 *
 * This function will call the provided set_frequency function with the
 * provided relative frequency to recalculate the frequency ratio and allocate
 * a mixing buffer.
 *
 * @param s_src               audio_stream/sampled_stream struct.
 * @param input_frequency     the native rate of the input stream. For module
 *                            players, this is set to the output frequency
 *                            (to allow them to use internal resamplers); for
 *                            RAD, this is 49716; for PCM files, this is the
 *                            sample rate of the file.
 * @param relative_frequency  the logical playback rate. For module players,
 *                            this is relative to 44100; for PCM files, this
 *                            is relative to the sample rate of the file.
 * @param channels            channels in stream (1 or 2).
 * @param use_volume          if true, compute volume, otherwise always max.
 */
void initialize_sampled_stream(struct sampled_stream *s_src,
 uint32_t input_frequency, uint32_t relative_frequency,
 uint32_t channels, boolean use_volume);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_SAMPLED_STREAM_H */
