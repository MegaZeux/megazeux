/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Simon Parzer <simon.parzer@gmail.com>
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
#include "audio_players.h"
#include "audio_struct.h"
#include "audio_wav.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"
#include "../io/vio.h"

//#ifdef __WIN32__
//#define BUILDING_STATIC
//#endif

#include <xmp.h>

struct xmp_stream
{
  struct sampled_stream s;
  xmp_context ctx;
  uint32_t row_tbl[XMP_MAX_MOD_LENGTH];
  size_t total_rows;
  boolean repeat;
};

static int get_xmp_resample_mode(void)
{
  const struct config_info *conf = get_config();

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      return XMP_INTERP_NEAREST;

    case RESAMPLE_MODE_LINEAR:
    default:
      return XMP_INTERP_LINEAR;

    case RESAMPLE_MODE_CUBIC:
    case RESAMPLE_MODE_FIR:
      return XMP_INTERP_SPLINE;
  }
}

static boolean audio_xmp_mix_data(struct audio_stream *a_src,
 int32_t * RESTRICT buffer, size_t frames, unsigned int channels)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  void *read_buffer;
  size_t read_wanted;
  boolean r_val = false;
  int xmp_r_val;

  read_buffer = sampled_get_buffer(&stream->s, &read_wanted);

  xmp_r_val =
   xmp_play_buffer(stream->ctx, read_buffer, read_wanted, a_src->repeat ? 0 : 1);

  if(xmp_r_val == -XMP_END && !a_src->repeat)
  {
    r_val = true;
  }

  sampled_mix_data((struct sampled_stream *)stream, buffer, frames, channels);

  return r_val;
}

static void audio_xmp_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  a_src->volume = volume;

  // xmp uses SMIX volume for all extra channels, but some files can generate
  // these channels through normal behavior. Since we don't use SMIX, we can
  // safely set both volumes to the same value.
  xmp_set_player(stream->ctx, XMP_PLAYER_VOLUME, volume * 100 / 255);
  xmp_set_player(stream->ctx, XMP_PLAYER_SMIX_VOLUME, volume * 100 / 255);
}

static void audio_xmp_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  a_src->repeat = repeat;
}

static void audio_xmp_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  struct xmp_frame_info info;
  int i;

  xmp_get_frame_info(stream->ctx, &info);

  for(i = 1; i < XMP_MAX_MOD_LENGTH; i++)
  {
    if(stream->row_tbl[i] > position)
    {
      int position_row = position - stream->row_tbl[i - 1];
      xmp_set_position(stream->ctx, i - 1);
      xmp_set_row(stream->ctx, position_row);
      return;
    }
  }
  xmp_set_position(stream->ctx, 0);
}

static void audio_xmp_set_order(struct audio_stream *a_src, uint32_t order)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  // XMP won't jump to the start of the specified order it's already playing
  // most of the time, so set the row too.
  xmp_set_position(stream->ctx, order);
  xmp_set_row(stream->ctx, 0);
}

static void audio_xmp_set_frequency(struct sampled_stream *s_src,
 uint32_t frequency)
{
  sampled_set_buffer(s_src, frequency, 44100);
}

static uint32_t audio_xmp_get_order(struct audio_stream *a_src)
{
  struct xmp_frame_info info;
  xmp_get_frame_info(((struct xmp_stream *)a_src)->ctx, &info);
  return info.pos;
}

static uint32_t audio_xmp_get_position(struct audio_stream *a_src)
{
  struct xmp_frame_info info;
  xmp_get_frame_info(((struct xmp_stream *)a_src)->ctx, &info);
  return ((struct xmp_stream *)a_src)->row_tbl[info.pos] + info.row;
}

static uint32_t audio_xmp_get_length(struct audio_stream *a_src)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  return stream->total_rows;
}

static uint32_t audio_xmp_get_frequency(struct sampled_stream *s_src)
{
  return s_src->relative_frequency;
}

// Return a duplicate of a sample from the current playing mod.
static boolean audio_xmp_get_sample(struct audio_stream *a_src, unsigned int which,
 struct wav_info *dest)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;

  struct xmp_module_info info;
  xmp_get_module_info(stream->ctx, &info);

  if(info.mod && info.mod->xxs && (info.mod->smp > (int)which))
  {
    struct xmp_sample *sam = &(info.mod->xxs[which]);

    // I don't know if it's even possible for synth samples to get this far,
    // but check for them so they can be ignored anyway.
    if(sam->len && !(sam->flg & XMP_SAMPLE_SYNTH))
    {
      dest->channels = 1;
      dest->freq = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
      dest->data_length = sam->len;

      // The period provided to audio_spot_sample was doubled for consistency
      // with the incorrect behavior added to audio_play_sample, so enable SAM
      // hacks to fix the frequency.
      dest->enable_sam_frequency_hack = true;

      // If the sample loops, the sample this returns needs to loop too.
      if(sam->flg & XMP_SAMPLE_LOOP)
      {
        dest->loop_start = sam->lps;
        dest->loop_end = sam->lpe;
      }
      else
        dest->loop_start = dest->loop_end = 0;

      // XMP samples are signed, and if 16bit, use the system byte order.
      // MZX supports all of these, so just copy the sample directly.
      if(sam->flg & XMP_SAMPLE_16BIT)
      {
        dest->format = SAMPLE_S16;
        dest->data_length *= 2;
      }
      else
        dest->format = SAMPLE_S8;

      dest->wav_data = (uint8_t *)cmalloc(dest->data_length);
      memcpy(dest->wav_data, sam->data, dest->data_length);
      return true;
    }
  }
  return false;
}

static void audio_xmp_destruct(struct audio_stream *a_src)
{
  xmp_end_player(((struct xmp_stream *)a_src)->ctx);
  xmp_release_module(((struct xmp_stream *)a_src)->ctx);
  xmp_free_context(((struct xmp_stream *)a_src)->ctx);
  sampled_destruct(a_src);
}

static unsigned long read_func(void *dest, unsigned long len, unsigned long nmemb, void *priv)
{
  vfile *vf = (vfile *)priv;
  return vfread(dest, len, nmemb, vf);
}

static int seek_func(void *priv, long offset, int whence)
{
  vfile *vf = (vfile *)priv;
  return vfseek(vf, offset, whence);
}

static long tell_func(void *priv)
{
  vfile *vf = (vfile *)priv;
  return vftell(vf);
}

static struct xmp_callbacks xmp_callbacks =
{
  read_func,
  seek_func,
  tell_func,
  NULL
};

static struct audio_stream *audio_xmp_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct xmp_stream *xmp_stream;
  struct xmp_module_info info;
  xmp_context ctx;
  unsigned char ord;
  size_t row_pos;
  int num_orders;
  int i;
  int chn = audio.buffer_channels < 2 ? 1 : 2;
  int flags = chn == 1 ? XMP_FORMAT_MONO : 0;

  ctx = xmp_create_context();
  if(!ctx)
    return NULL;

  xmp_set_player(ctx, XMP_PLAYER_DEFPAN, 50);

  /* This function does not close the provided file pointer. */
  if(xmp_load_module_from_callbacks(ctx, vf, xmp_callbacks) < 0)
  {
    xmp_free_context(ctx);
    return NULL;
  }

  xmp_stream = (struct xmp_stream *)ccalloc(1, sizeof(struct xmp_stream));
  if(!xmp_stream)
  {
    xmp_free_context(ctx);
    return NULL;
  }
  xmp_stream->s.a.player = &audio_player_xmp;

  xmp_stream->ctx = ctx;
  xmp_start_player(ctx, audio.output_frequency, flags);
  xmp_set_player(ctx, XMP_PLAYER_INTERP, get_xmp_resample_mode());

  xmp_get_module_info(ctx, &info);
  num_orders = info.mod->len;

  for(i = 0, row_pos = 0; i < num_orders; i++)
  {
    xmp_stream->row_tbl[i] = row_pos;
    ord = info.mod->xxo[i];
    if(ord < info.mod->pat)
      row_pos += info.mod->xxp[ord]->rows;
  }
  xmp_stream->total_rows = row_pos;

  // xmp uses sequences to make secondary loops in modules playable, but this
  // breaks looping from the end of the module to the start. Since MZX doesn't
  // use sequences and games rely on normal looping, zero all entry points.
  for(i = 0; i < info.num_sequences; i++)
  {
    info.seq_data[i].entry_point = 0;
  }

  initialize_sampled_stream((struct sampled_stream *)xmp_stream,
   audio.output_frequency, frequency, chn, false);

  initialize_audio_stream((struct audio_stream *)xmp_stream, volume, repeat);

  vfclose(vf);
  return (struct audio_stream *)xmp_stream;
}

const struct audio_player audio_player_xmp =
{
  "libxmp",
  NULL,

  audio_xmp_construct,
  audio_xmp_destruct,
  audio_xmp_mix_data,
  audio_xmp_set_volume,
  audio_xmp_set_repeat,
  audio_xmp_set_order,
  audio_xmp_get_order,
  audio_xmp_set_position,
  audio_xmp_get_position,
  audio_xmp_get_length,
  NULL,
  NULL,
  NULL,
  NULL,
  audio_xmp_set_frequency,
  audio_xmp_get_frequency,
  audio_xmp_get_sample,
};
