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
#include "sampled_stream.h"

#include "../util.h"
#include "../io/path.h"
#include "../io/vio.h"

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>
#include <limits.h>
#include <stdlib.h>

struct openmpt_stream
{
  struct sampled_stream s;
  openmpt_module *module_data;
  uint32_t *row_tbl;
  uint32_t row_tbl_size;
  uint32_t total_rows;
};

// OpenMPT's resampling option takes interpolation taps instead of an enum.
static int omp_get_resample_mode(void)
{
  const struct config_info *conf = get_config();

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      return 1;

    case RESAMPLE_MODE_LINEAR:
    default:
      return 2;

    case RESAMPLE_MODE_CUBIC:
      return 4;

    case RESAMPLE_MODE_FIR:
      return 8;
  }
}

static boolean omp_mix_data(struct audio_stream *a_src, int32_t *buffer,
 size_t frames, unsigned int channels)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  struct sampled_stream *s = (struct sampled_stream *)a_src;

  int16_t *read_buffer;
  size_t read_wanted;
  boolean r_val = false;
  uint32_t read_len;

  read_buffer = (int16_t *)sampled_get_buffer(&omp_stream->s, &read_wanted);

  if(s->channels >= 2)
  {
    read_len = openmpt_module_read_interleaved_stereo(omp_stream->module_data,
     s->input_frequency, read_wanted / 4, read_buffer) * 4;
  }
  else
  {
    read_len = openmpt_module_read_mono(omp_stream->module_data,
     s->input_frequency, read_wanted / 2, read_buffer) * 2;
  }

  if(read_len < read_wanted && !a_src->repeat)
  {
    memset(read_buffer + read_len, 0, read_wanted - read_len);
    r_val = true;
  }

  sampled_mix_data(s, buffer, frames, channels);

  return r_val;
}

static void omp_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  a_src->volume = volume;

  if(volume == 0)
    volume = INT_MIN;

  else
    volume = (volume - 255) * 15;

  openmpt_module_set_render_param(omp_stream->module_data,
    OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL, volume);
}

static void omp_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  a_src->repeat = repeat;

  openmpt_module_set_repeat_count(omp_stream->module_data, repeat ? -1 : 0);
}

static void omp_set_order(struct audio_stream *a_src, uint32_t order)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  openmpt_module_set_position_order_row(omp_stream->module_data, order, 0);
}

static void omp_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  uint32_t ord;
  uint32_t row;

  for(ord = 1; ord < omp_stream->row_tbl_size; ord++)
  {
    if(position < omp_stream->row_tbl[ord])
    {
      row = position - omp_stream->row_tbl[ord - 1];

      openmpt_module_set_position_order_row(omp_stream->module_data, ord, row);
      return;
    }
  }

  // fallback - position out of range
  openmpt_module_set_position_order_row(omp_stream->module_data, 0, 0);
}

static void omp_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  sampled_set_buffer(s_src, frequency, 44100);
}

static uint32_t omp_get_order(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return openmpt_module_get_current_order(omp_stream->module_data);
}

static uint32_t omp_get_position(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  int ord = openmpt_module_get_current_order(omp_stream->module_data);

  return omp_stream->row_tbl[ord] +
   openmpt_module_get_current_row(omp_stream->module_data);
}

static uint32_t omp_get_length(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return omp_stream->row_tbl[omp_stream->row_tbl_size - 1];
}

static uint32_t omp_get_frequency(struct sampled_stream *s_src)
{
  return s_src->relative_frequency;
}

static void omp_destruct(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  openmpt_module_destroy(omp_stream->module_data);
  free(omp_stream->row_tbl);
  sampled_destruct(a_src);
}

static void omp_log(const char *message, void *data)
{
  if(message)
     fprintf(mzxerr, "%s\n", message);
}

static size_t omp_read_fn(void *stream, void *dest, size_t bytes)
{
  vfile *vf = (vfile *)stream;
  return vfread(dest, 1, bytes, vf);
}

static int omp_seek_fn(void *stream, int64_t offset, int whence)
{
  vfile *vf = (vfile *)stream;
  return vfseek(vf, offset, whence);
}

static int64_t omp_tell_fn(void *stream)
{
  vfile *vf = (vfile *)stream;
  return vftell(vf);
}

static const struct openmpt_stream_callbacks omp_callbacks =
{
  omp_read_fn,
  omp_seek_fn,
  omp_tell_fn
};

static struct audio_stream *omp_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct openmpt_stream *omp_stream;
  uint32_t *row_tbl;
  uint32_t row_pos;
  uint32_t ord;
  uint32_t i;
  int row_tbl_size;
  int chn = audio.buffer_channels < 2 ? 1 : 2;

  openmpt_module *open_file = openmpt_module_create2(omp_callbacks, vf,
   &omp_log, NULL, NULL, NULL, NULL, NULL, NULL);
  if(!open_file)
    return NULL;

  row_tbl_size = openmpt_module_get_num_orders(open_file) + 1;
  if(row_tbl_size < 1)
    row_tbl_size = 1;

  omp_stream = (struct openmpt_stream *)malloc(sizeof(struct openmpt_stream));
  row_tbl = (uint32_t *)malloc(row_tbl_size * sizeof(uint32_t));
  if(!omp_stream || !row_tbl)
  {
    openmpt_module_destroy(open_file);
    free(omp_stream);
    free(row_tbl);
    return NULL;
  }
  omp_stream->s.a.player = &audio_player_openmpt;

  omp_stream->module_data = open_file;
  omp_stream->row_tbl = row_tbl;
  omp_stream->row_tbl_size = row_tbl_size;

  for(i = 0, row_pos = 0; i < (uint32_t)row_tbl_size; i++)
  {
    row_tbl[i] = row_pos;
    ord = openmpt_module_get_order_pattern(open_file, i);
    if(i < (uint32_t)row_tbl_size - 1)
      row_pos += openmpt_module_get_pattern_num_rows(open_file, ord);
  }

  openmpt_module_set_render_param(omp_stream->module_data,
   OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
   omp_get_resample_mode());

  openmpt_module_set_repeat_count(omp_stream->module_data, -1);

  initialize_sampled_stream((struct sampled_stream *)omp_stream,
   audio.output_frequency, frequency, chn, false);

  initialize_audio_stream((struct audio_stream *)omp_stream, volume, repeat);

  vfclose(vf);
  return (struct audio_stream *)omp_stream;
}

static boolean omp_test(vfile *vf, const char *filename)
{
  /* TODO: OpenMPT doesn't have a way to whitelist individual formats yet,
   * and having to add Symphonie/etc. support to libxmp because a package
   * maintainer liked OpenMPT better doesn't sound fun, so filter by extension.
   * This is not foolproof.
   */
  static const char * const ext_list[] =
  {
    "669", "amf", "dsm", "far", "gdm", "it", "med", "mod", "mtm",
    "nst", "oct", "okt", "s3m", "stm", "ult", "wow", "xm"
  };
  ssize_t ext_pos = path_get_ext_offset(filename);
  size_t i;
  if(ext_pos < 0)
    return false;

  for(i = 0; i < ARRAY_SIZE(ext_list); i++)
  {
    if(!strcasecmp(filename + ext_pos + 1, ext_list[i]))
      return true;
  }
  return false;
}

const struct audio_player audio_player_openmpt =
{
  "libopenmpt",
  omp_test,

  omp_construct,
  omp_destruct,
  omp_mix_data,
  omp_set_volume,
  omp_set_repeat,
  omp_set_order,
  omp_get_order,
  omp_set_position,
  omp_get_position,
  omp_get_length,
  NULL,
  NULL,
  NULL,
  NULL,
  omp_set_frequency,
  omp_get_frequency,
  NULL,
};
