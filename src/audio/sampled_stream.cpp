/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018, 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

// Common functions for sampled streams.

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_struct.h"
#include "sampled_stream.h"

#define FP_SHIFT      13
#define FP_AND        ((1 << FP_SHIFT) - 1)
#define FP_ONE        (1 << FP_SHIFT)

/**
 * This uses C++ templates to generate optimized mixer variants depending on
 * the number of channels, whether or not volume is required, and whether or
 * not resampling is required. This helps avoid redundant code.
 *
 * Likewise, fixed point is used to improve performance. The FP_SHIFT value is
 * the maximum amount an int16_t can be shifted into an int32_t while still
 * leaving enough space for the required math in the linear and cubic mixers.
 * The cubic mixer currently works with values shifted by 2x FP_SHIFT when
 * computing its polynomial.
 *
 * NOTE: FP_SHIFT values of 16 or 32 theoretically might help performance but
 *       some experimentation suggests it's not enough to be worth the effort.
 * TODO: FIR/Sinc-Lanczos resampler!!!!!111one
 * TODO: surround (runtime channel mapping only)
 */

enum mixer_resample
{
  FLAT        = 0,
  NEAREST     = 1,
  LINEAR      = 2,
  CUBIC       = 3
};

enum mixer_channels
{
  MONO        = 1,
  STEREO      = 2,
  SURROUND    = 3
};

enum mixer_volume
{
  FULL        = 0,
  DYNAMIC     = 1
};

/* Cubic resampling needs one prologue sample and three epilogue samples. */
#define PROLOGUE_LENGTH (1 * STEREO * sizeof(int16_t))
#define EPILOGUE_LENGTH (3 * STEREO * sizeof(int16_t))

template<mixer_volume VOLUME>
static int32_t volume_function(int32_t sample, int volume)
{
  /**
   * NOTE: previous versions of MZX did a /256 here. Just do the bitshift--the
   * rounding difference is more or less irrelevant and it performs better than
   * GCC's optimization for signed constant division.
   */
  if(VOLUME)
    return (sample * volume) >> 8;

  return sample;
}

template<mixer_channels DEST_CHANNELS, mixer_channels SRC_CHANNELS,
 mixer_volume VOLUME>
static void flat_mix_loop(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume)
{
  //size_t chn = DEST_CHANNELS > STEREO ? s_src->dest_channels : (size_t)DEST_CHANNELS;
  for(size_t i = 0; i < dest_frames; i++)
  {
    if(SRC_CHANNELS == DEST_CHANNELS && DEST_CHANNELS <= STEREO)
    {
      for(size_t j = 0; j < DEST_CHANNELS; j++)
        *(dest++) += volume_function<VOLUME>(*(src++), volume);
    }
    else

    if(DEST_CHANNELS == STEREO && SRC_CHANNELS == MONO)
    {
      int32_t smpl = volume_function<VOLUME>(*(src++), volume);
      *(dest++) += smpl;
      *(dest++) += smpl;
    }
    else

    if(DEST_CHANNELS == MONO && SRC_CHANNELS == STEREO)
    {
      int32_t mix = 0;
      mix += volume_function<VOLUME>(*(src++), volume);
      mix += volume_function<VOLUME>(*(src++), volume);
      *(dest++) += mix >> 1;
    }
    //else TODO: surround not implemented
  }
  s_src->sample_index = dest_frames << FP_SHIFT;
}

/**
 * C++98 requires that these have external linkage to be used as template
 * parameters. This is pointless but fortunately doesn't hurt C++11 builds.
 */
typedef int32_t (*mix_function)(const int16_t *src_offset, ssize_t frac_index);

template<mixer_channels SRC_CHANNELS>
int32_t nearest_mix(const int16_t *src_offset, ssize_t frac_index)
{
  return src_offset[0];
}

// TODO: linear and cubic need external s_chn for surround.
template<mixer_channels SRC_CHANNELS>
int32_t linear_mix(const int16_t *src_offset, ssize_t frac_index)
{
  int chn = static_cast<int>(SRC_CHANNELS);
  int32_t left = src_offset[0];
  int32_t right = src_offset[chn];
  return left + ((right - left) * frac_index >> FP_SHIFT);
}

template<mixer_channels SRC_CHANNELS>
int32_t cubic_mix(const int16_t *src_offset, ssize_t frac_index)
{
  int chn = static_cast<int>(SRC_CHANNELS);
  /**
   * NOTE: copied mostly verbatim from the old mixer code, with cleanup.
   * This uses ssize_t instead of int32_t since it seems to be faster for
   * 64-bit machines in this particular resampler.
   *
   * This uses a Hermite spline. This is somewhat faster to compute and
   * generally considered better quality than a Lagrange cubic.
   */
  ssize_t s0 = (ssize_t)src_offset[-chn]    << FP_SHIFT;
  ssize_t s1 = (ssize_t)src_offset[0]       << FP_SHIFT;
  ssize_t s2 = (ssize_t)src_offset[chn]     << FP_SHIFT;
  ssize_t s3 = (ssize_t)src_offset[chn * 2] << FP_SHIFT;

  int64_t a = (((3 * (s1 - s2)) - s0 + s3) / 2);
  int64_t b = ((2 * s2) + s0 - (((5 * s1) + s3) / 2));
  int64_t c = ((s2 - s0) / 2);

  a = ((a * frac_index) >> FP_SHIFT) + b;
  a = ((a * frac_index) >> FP_SHIFT) + c;
  a = ((a * frac_index) >> FP_SHIFT) + s1;
  return (a >> FP_SHIFT);
}

template<mixer_channels DEST_CHANNELS, mixer_channels SRC_CHANNELS,
 mixer_volume VOLUME, mix_function MIX>
static void resample_mix_loop(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume)
{
  //size_t d_chn = DEST_CHANNELS > STEREO ? s_src->dest_channels : (size_t)DEST_CHANNELS;
  size_t s_chn = SRC_CHANNELS > STEREO ? s_src->channels : (size_t)SRC_CHANNELS;
  int64_t sample_index = s_src->sample_index;
  int64_t delta = s_src->frequency_delta;

  for(size_t i = 0; i < dest_frames; i++, sample_index += delta)
  {
    ssize_t int_index = (sample_index >> FP_SHIFT) * s_chn;
    ssize_t frac_index = sample_index & FP_AND;

    if(SRC_CHANNELS == DEST_CHANNELS && DEST_CHANNELS <= STEREO)
    {
      for(size_t j = 0; j < DEST_CHANNELS; j++)
      {
        int32_t mix = MIX(src + int_index + j, frac_index);
        *(dest++) += volume_function<VOLUME>(mix, volume);
      }
    }
    else

    if(DEST_CHANNELS == STEREO && SRC_CHANNELS == MONO)
    {
      int32_t mix = MIX(src + int_index, frac_index);
      int32_t smpl = volume_function<VOLUME>(mix, volume);
      *(dest++) += smpl;
      *(dest++) += smpl;
    }
    else

    if(DEST_CHANNELS == MONO && SRC_CHANNELS == STEREO)
    {
      int32_t mix = 0;
      for(size_t chn = 0; chn < SRC_CHANNELS; chn++)
      {
        int32_t smpl = MIX(src + int_index + chn, frac_index);
        mix += volume_function<VOLUME>(smpl, volume);
      }
      *(dest++) += mix >> 1;
    }
    //else TODO: surround not implemented
  }
  s_src->sample_index = sample_index;
}

template<mixer_channels DEST_CHANNELS, mixer_channels SRC_CHANNELS, mixer_volume VOLUME>
static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume,
 int resample_mode)
{
  switch((mixer_resample)resample_mode)
  {
    case FLAT:
      flat_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME>(
       s_src, dest, dest_frames, src, volume);
      break;

    case NEAREST:
      resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, nearest_mix<SRC_CHANNELS> >(
       s_src, dest, dest_frames, src, volume);
      break;

    case LINEAR:
      resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, linear_mix<SRC_CHANNELS> >(
       s_src, dest, dest_frames, src, volume);
      break;

    case CUBIC:
      resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, cubic_mix<SRC_CHANNELS> >(
       s_src, dest, dest_frames, src, volume);
      break;
  }
}

template<mixer_channels DEST_CHANNELS, mixer_channels SRC_CHANNELS>
static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume,
 int resample_mode, mixer_volume volume_mode)
{
  switch(volume_mode)
  {
    case FULL:
      mixer_function<DEST_CHANNELS, SRC_CHANNELS, FULL>(
       s_src, dest, dest_frames, src, volume, resample_mode);
      break;

    case DYNAMIC:
      mixer_function<DEST_CHANNELS, SRC_CHANNELS, DYNAMIC>(
       s_src, dest, dest_frames, src, volume, resample_mode);
      break;
  }
}

template<mixer_channels DEST_CHANNELS>
static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume,
 int resample_mode, mixer_channels src_channels, mixer_volume volume_mode)
{
  switch(src_channels)
  {
    case MONO:
      mixer_function<DEST_CHANNELS, MONO>(
       s_src, dest, dest_frames, src, volume, resample_mode, volume_mode);
      break;

    case STEREO:
      mixer_function<DEST_CHANNELS, STEREO>(
       s_src, dest, dest_frames, src, volume, resample_mode, volume_mode);
      break;

    case SURROUND:
      mixer_function<DEST_CHANNELS, SURROUND>(
       s_src, dest, dest_frames, src, volume, resample_mode, volume_mode);
      break;
  }
}

static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t dest_frames, const int16_t *src, int volume,
 int resample_mode, mixer_channels dest_channels, mixer_channels src_channels,
 mixer_volume volume_mode)
{
  switch(dest_channels)
  {
    case MONO:
      mixer_function<MONO>(
       s_src, dest, dest_frames, src, volume, resample_mode, src_channels, volume_mode);
      break;

    case STEREO:
      mixer_function<STEREO>(
       s_src, dest, dest_frames, src, volume, resample_mode, src_channels, volume_mode);
      break;

    case SURROUND:
      mixer_function<SURROUND>(
       s_src, dest, dest_frames, src, volume, resample_mode, src_channels, volume_mode);
      break;
  }
}

void *sampled_get_buffer(struct sampled_stream *s_src, size_t *needed)
{
  size_t total = s_src->data_window_length + PROLOGUE_LENGTH + EPILOGUE_LENGTH;
  *needed = (total > s_src->stream_offset) ? total - s_src->stream_offset : 0;
  return ((uint8_t *)s_src->output_data) + s_src->stream_offset;
}

void sampled_set_buffer(struct sampled_stream *s_src, uint32_t rel_frequency,
 uint32_t rel_frequency_base)
{
  size_t data_window_length;
  size_t allocated_data_length;
  int64_t frequency;
  int64_t frequency_base;
  int64_t frequency_delta;

  frequency = (int64_t)s_src->input_frequency;
  frequency_base = (int64_t)audio.output_frequency;

  /* The relative frequency base is whatever value should be reported for "1:1"
   * rate to the game engine, usually 44100 for modules and native for PCM.
   * This value should never be zero, but check it just in case.
   *
   * Relative frequency of 0 acts the same as 1:1.
   */
  if(rel_frequency != 0 && rel_frequency_base != 0)
  {
    frequency *= (int64_t)rel_frequency;
    frequency_base *= (int64_t)rel_frequency_base;
  }
  else
    rel_frequency = rel_frequency_base;

  frequency_delta = (frequency << FP_SHIFT) / frequency_base;

  data_window_length =
   (size_t)(ceil((double)audio.buffer_frames *
   frequency / frequency_base) * s_src->bytes_per_frame);

  allocated_data_length = data_window_length + PROLOGUE_LENGTH + EPILOGUE_LENGTH;

  if(allocated_data_length < s_src->bytes_per_frame)
    allocated_data_length = s_src->bytes_per_frame;

  /* If the buffer still contains data past the desired allocation, leave it.
   * Otherwise, the audio will audibly skip. */
  if(s_src->stream_offset > allocated_data_length)
    allocated_data_length = s_src->stream_offset;

  s_src->relative_frequency = rel_frequency;
  s_src->frequency_delta = frequency_delta;
  s_src->data_window_length = data_window_length;
  s_src->allocated_data_length = allocated_data_length;

  s_src->output_data =
   (int16_t *)crealloc(s_src->output_data, allocated_data_length);
}

void sampled_mix_data(struct sampled_stream *s_src,
 int32_t * RESTRICT dest_buffer, size_t dest_frames, unsigned int dest_channels)
{
  uint8_t *output_data = (uint8_t *)s_src->output_data;
  int16_t *src_buffer = (int16_t *)(output_data + PROLOGUE_LENGTH);
  int volume = ((struct audio_stream *)s_src)->volume;
  int resample_mode = audio.global_resample_mode + 1;
  enum mixer_volume   volume_mode  = DYNAMIC;
  enum mixer_channels src_channels = STEREO;
  enum mixer_channels out_channels = STEREO;
  size_t new_index;
  size_t new_index_bytes;
  size_t leftover_bytes = 0;

  // MZX currently only supports mono and stereo output.
  assert(dest_channels == 1 || dest_channels == 2);

  if(s_src->frequency_delta == FP_ONE)
    resample_mode = FLAT;

  if(!s_src->use_volume || volume == 256)
    volume_mode = FULL;

  if(s_src->channels < 2)
    src_channels = MONO;
  if(dest_channels < 2)
    out_channels = MONO;

  s_src->dest_channels = dest_channels;
  mixer_function(s_src, dest_buffer, dest_frames, src_buffer, volume,
   resample_mode, out_channels, src_channels, volume_mode);

  /* Relocate the leftovers--plus the extra prologue and epilogue samples for
   * the resamplers--back to the start of the buffer. */
  new_index = s_src->sample_index >> FP_SHIFT;
  new_index_bytes = new_index * s_src->bytes_per_frame;
  if(new_index_bytes > s_src->data_window_length)
    new_index_bytes = s_src->data_window_length;

  leftover_bytes = s_src->data_window_length - new_index_bytes;

  /* Note new_index_bytes doesn't count the prologue, so both the source and
   * the dest include it implicitly. */
  memmove(output_data,
   output_data + new_index_bytes,
   leftover_bytes + PROLOGUE_LENGTH + EPILOGUE_LENGTH);

  s_src->stream_offset = leftover_bytes + PROLOGUE_LENGTH + EPILOGUE_LENGTH;
  s_src->sample_index &= FP_AND;
}

void sampled_destruct(struct audio_stream *a_src)
{
  struct sampled_stream *s_stream = (struct sampled_stream *)a_src;
  free(s_stream->output_data);
  destruct_audio_stream(a_src);
}

void initialize_sampled_stream(struct sampled_stream *s_src,
 uint32_t input_frequency, uint32_t relative_frequency,
 uint32_t channels, boolean use_volume)
{
  s_src->channels = channels;
  s_src->output_data = NULL;
  s_src->use_volume = use_volume;
  s_src->bytes_per_frame = channels * sizeof(int16_t);
  s_src->stream_offset = PROLOGUE_LENGTH;
  s_src->sample_index = 0;

  s_src->input_frequency = input_frequency;
  s_src->a.player->set_frequency(s_src, relative_frequency);

  memset(s_src->output_data, 0, PROLOGUE_LENGTH);
}
