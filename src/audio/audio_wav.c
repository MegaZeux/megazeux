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
#include "audio_struct.h"
#include "audio_wav.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../util.h"
#include "../io/path.h"
#include "../io/vio.h"

#ifdef CONFIG_SDL
#include "../platform/sdl/SDLmzx.h" // SDL WAV loader fallback
#endif

// If the WAV/SAM is larger than this, print a warning to the console.
// (Right now only do this for debug builds because a lot more games than
// anticipated use big WAVs and it could get annoying for end users.)
#define WARN_FILESIZE (1<<22)

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

static uint32_t read_little_endian32(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  uint32_t s = 0;
  int i;
  for(i = 3; i >= 0; i--)
    s = (s << 8) | b[i];
  return s;
}

static int read_little_endian16(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  return (b[1] << 8) | b[0];
}

static void *get_riff_chunk(vfile *vf, size_t filesize, char *id, uint32_t *size)
{
  long pos = vftell(vf);
  size_t maxsize = filesize - pos - 8;
  char size_buf[4];
  void *buf;

  if(pos < 0 || (size_t)(pos + 8) > filesize)
    return NULL;

  if(id)
  {
    if(vfread(id, 1, 4, vf) < 4)
      return NULL;
  }
  else
  {
    vfseek(vf, 4, SEEK_CUR);
  }

  if(vfread(size_buf, 1, 4, vf) < 4)
    return NULL;

  *size = read_little_endian32(size_buf);
  if(*size > maxsize)
    *size = maxsize;

  buf = malloc(*size);
  if(!buf)
    return NULL;

  if(vfread(buf, 1, *size, vf) < *size)
  {
    free(buf);
    return NULL;
  }

  // Realign if odd size unless padding byte isn't 0
  if(*size & 1)
  {
    int c = vfgetc(vf);
    if((c != 0) && (c != EOF))
      vungetc(c, vf);
  }

  return buf;
}

static boolean get_next_riff_chunk_id(vfile *vf, size_t filesize, char *id)
{
  long pos = vftell(vf);

  if(pos < 0 || (size_t)(pos + 8) > filesize)
    return false;

  if(vfread(id, 1, 4, vf) < 4)
    return false;

  vfseek(vf, -4, SEEK_CUR);
  return true;
}

static void skip_riff_chunk(vfile *vf, size_t filesize)
{
  long pos = vftell(vf);
  size_t maxsize = filesize - pos - 8;
  char size_buf[4];
  uint32_t s;

  if(pos > 0 && (size_t)(pos + 8) <= filesize)
  {
    vfseek(vf, 4, SEEK_CUR);
    if(vfread(size_buf, 1, 4, vf) < 4)
      return;

    s = read_little_endian32(size_buf);
    if(s > maxsize)
      s = maxsize;
    vfseek(vf, s, SEEK_CUR);

    // Realign if odd size unless padding byte isn't 0
    if(s & 1)
    {
      int c = vfgetc(vf);
      if((c != 0) && (c != EOF))
        vungetc(c, vf);
    }
  }
}

static void *get_riff_chunk_by_id(vfile *vf, size_t filesize,
 const char *id, uint32_t *size)
{
  boolean i;
  char id_buf[4];

  vfseek(vf, 12, SEEK_SET);

  while((i = get_next_riff_chunk_id(vf, filesize, id_buf)))
  {
    if(!memcmp(id_buf, id, 4))
      break;

    skip_riff_chunk(vf, filesize);
  }

  if(!i)
    return NULL;

  return get_riff_chunk(vf, filesize, NULL, size);
}

// Simple SAM loader.

static boolean load_sam_file(vfile *vf, const char *filename, struct wav_info *spec)
{
  size_t source_length;
  size_t read_length;
  void *buf;

  source_length = vfilelength(vf, false);
  if(source_length > WARN_FILESIZE)
  {
    trace("Size of SAM file '%s' is %zu; OGG should be used instead.\n",
     filename, source_length);
  }

  // Default to no loop
  spec->channels = 1;
  spec->freq = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
  spec->format = SAMPLE_S8;
  spec->loop_start = 0;
  spec->loop_end = 0;
  spec->enable_sam_frequency_hack = true;

  buf = malloc(source_length);
  if(!buf)
    return false;

  read_length = vfread(buf, 1, source_length, vf);
  if(read_length < source_length)
  {
    free(buf);
    return false;
  }
  spec->data_length = source_length;
  spec->wav_data = buf;
  return true;
}

#ifdef CONFIG_SDL
// SDL-based WAV loader, used as a fallback if the regular loader fails.
// It supports more formats than the regular loader.

static boolean load_wav_file_sdl(const char *filename, struct wav_info *spec)
{
  SDL_AudioSpec sdlspec;
  void *copy_buf;

  if(!SDL_LoadWAV(filename, &sdlspec, &(spec->wav_data), &(spec->data_length)))
    return false;

  copy_buf = malloc(spec->data_length);
  if(!copy_buf)
  {
    SDL_FreeWAV(spec->wav_data);
    return false;
  }

  memcpy(copy_buf, spec->wav_data, spec->data_length);
  SDL_FreeWAV(spec->wav_data);

  spec->wav_data = copy_buf;
  spec->channels = sdlspec.channels;
  spec->freq = sdlspec.freq;
  switch(sdlspec.format)
  {
    case SDL_AUDIO_U8:
      spec->format = SAMPLE_U8;
      break;
    case SDL_AUDIO_S8:
      spec->format = SAMPLE_S8;
      break;
    case SDL_AUDIO_S16LE:
      spec->format = SAMPLE_S16LE;
      break;
    // May be returned by SDL on big endian machines.
    case SDL_AUDIO_S16BE:
      spec->format = SAMPLE_S16BE;
      break;
    /**
     * TODO: SDL 2.0 can technically return AUDIO_S32LSB or AUDIO_F32LSB.
     * Support for these would be trivial to add but might encourage worse
     * abuse of .WAV support (as those formats are twice the size of S16).
     */
    default:
      warn("Unsupported WAV SDL_AudioFormat 0x%x! Report this!\n", sdlspec.format);
      free(copy_buf);
      return false;
  }

  return true;
}
#endif

// More lenient than SDL's WAV loader, but only supports
// uncompressed PCM files (for now.)

static boolean load_wav_file(vfile *vf, const char *filename, struct wav_info *spec)
{
  int channels, srate, sbytes, numloops;
  uint32_t riffsize, data_size, fmt_size, smpl_size;
  size_t loop_start, loop_end;
  char *fmt_chunk, *smpl_chunk, tmp_buf[4];
  size_t file_size;

  file_size = vfilelength(vf, false);
  if(file_size > WARN_FILESIZE)
  {
    trace("This WAV is too big sempai OwO;;;\n");
    trace("Size of WAV file '%s' is %zu; OGG should be used instead.\n",
     filename, file_size);
  }

  // If it doesn't start with "RIFF", it's not a WAV file.
  if(vfread(tmp_buf, 1, 4, vf) < 4)
    return false;

  if(memcmp(tmp_buf, "RIFF", 4))
    return false;

  // Read reported file size (if the file turns out to be larger, this will be
  // used instead of the real file size.)
  if(vfread(tmp_buf, 1, 4, vf) < 4)
    return false;

  riffsize = read_little_endian32(tmp_buf) + 8;

  // If the RIFF type isn't "WAVE", it's not a WAV file.
  if(vfread(tmp_buf, 1, 4, vf) < 4)
    return false;

  if(memcmp(tmp_buf, "WAVE", 4))
    return false;

  vrewind(vf);
  if(file_size > riffsize)
    file_size = riffsize;

  fmt_chunk = get_riff_chunk_by_id(vf, file_size, "fmt ", &fmt_size);

  // If there's no "fmt " chunk, or it's less than 16 bytes, it's not a valid
  // WAV file.
  if(!fmt_chunk || (fmt_size < 16))
  {
    free(fmt_chunk);
    return false;
  }

  // Default to no loop
  spec->loop_start = 0;
  spec->loop_end = 0;

  // Not a SAM, so don't enable this hack.
  spec->enable_sam_frequency_hack = false;

  // If the WAV file isn't uncompressed PCM (format 1), let SDL handle it.
  if(read_little_endian16(fmt_chunk) != 1)
  {
    free(fmt_chunk);

#ifdef CONFIG_SDL
    if(load_wav_file_sdl(filename, spec))
      return true;
#endif

    return false;
  }

  // Get the data we need from the "fmt " chunk.
  channels = read_little_endian16(fmt_chunk + 2);
  srate = read_little_endian32(fmt_chunk + 4);

  // Average bytes per second go here (4 bytes)
  // Block align goes here (2 bytes)
  // Round up when dividing by 8
  sbytes = (read_little_endian16(fmt_chunk + 14) + 7) / 8;
  free(fmt_chunk);

  // If I remember correctly, we can't do anything past stereo, and 0 channels
  // isn't valid.  We can't do anything past 16-bit either, and 0-bit isn't
  // valid. 0Hz sample rate is invalid too.
  if(!channels || (channels > 2) || !sbytes || (sbytes > 2) || !srate)
    return false;

  // Everything seems to check out, so let's load the "data" chunk.
  spec->wav_data = get_riff_chunk_by_id(vf, file_size, "data", &data_size);
  spec->data_length = data_size;

  // No "data" chunk?! FAIL!
  if(!spec->wav_data)
    return false;

  // Empty "data" chunk?! ALSO FAIL!
  if(!data_size)
  {
    free(spec->wav_data);
    return false;
  }

  spec->freq = srate;
  if(sbytes == 1)
    spec->format = SAMPLE_U8;
  else
    spec->format = SAMPLE_S16LE;
  spec->channels = channels;

  // Check for "smpl" chunk for looping info
  smpl_chunk = get_riff_chunk_by_id(vf, file_size, "smpl", &smpl_size);

  // If there's no "smpl" chunk or it's less than 60 bytes, there's no valid
  // loop data
  if(!smpl_chunk || (smpl_size < 60))
  {
    free(smpl_chunk);
    return true;
  }

  numloops = read_little_endian32(smpl_chunk + 28);
  // First loop is at 36
  loop_start = read_little_endian32(smpl_chunk + 44) * channels * sbytes;
  loop_end = read_little_endian32(smpl_chunk + 48) * channels * sbytes;
  free(smpl_chunk);

  // If the number of loops is less than 1, the loop data's invalid
  if(numloops < 1)
    return true;

  // Boundary check loop points
  if((loop_start >= spec->data_length) || (loop_end > spec->data_length)
   || (loop_start >= loop_end))
    return true;

  spec->loop_start = loop_start;
  spec->loop_end = loop_end;
  return true;
}

struct audio_stream *construct_wav_stream_direct(struct wav_info *w_info,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct wav_stream *w_stream;
  struct sampled_stream_spec s_spec;
  struct audio_stream_spec a_spec;

  w_stream = (struct wav_stream *)malloc(sizeof(struct wav_stream));
  if(!w_stream)
  {
    free(w_info->wav_data);
    return NULL;
  }

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

  memset(&a_spec, 0, sizeof(struct audio_stream_spec));
  a_spec.mix_data       = wav_mix_data;
  a_spec.set_volume     = wav_set_volume;
  a_spec.set_repeat     = wav_set_repeat;
  a_spec.set_position   = wav_set_position;
  a_spec.set_loop_start = wav_set_loop_start;
  a_spec.set_loop_end   = wav_set_loop_end;
  a_spec.get_position   = wav_get_position;
  a_spec.get_length     = wav_get_length;
  a_spec.get_loop_start = wav_get_loop_start;
  a_spec.get_loop_end   = wav_get_loop_end;
  a_spec.destruct       = wav_destruct;

  memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
  s_spec.set_frequency = wav_set_frequency;
  s_spec.get_frequency = wav_get_frequency;

  initialize_sampled_stream((struct sampled_stream *)w_stream, &s_spec,
   w_info->freq, frequency, w_info->channels, true);

  initialize_audio_stream((struct audio_stream *)w_stream, &a_spec,
   volume, repeat);

  return (struct audio_stream *)w_stream;
}

static struct audio_stream *construct_wav_stream(vfile *vf,
 const char *filename, uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_src;
  struct wav_info w_info;
  memset(&w_info, 0, sizeof(struct wav_info));

  if(!load_wav_file(vf, filename, &w_info))
    return NULL;

  // Surround WAVs not supported yet..
  if(w_info.channels > 2)
    return NULL;

  a_src = construct_wav_stream_direct(&w_info, frequency, volume, repeat);
  if(a_src)
    vfclose(vf);

  return a_src;
}

static struct audio_stream *construct_sam_stream(vfile *vf,
 const char *filename, uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_src;
  struct wav_info w_info;
  memset(&w_info, 0, sizeof(struct wav_info));

  if(!load_sam_file(vf, filename, &w_info))
    return NULL;

  a_src = construct_wav_stream_direct(&w_info, frequency, volume, repeat);
  if(a_src)
    vfclose(vf);

  return a_src;
}

static boolean test_wav_stream(vfile *vf, const char *filename)
{
  char buf[12];
  if(!vfread(buf, 12, 1, vf))
    return false;

  if(memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
    return false;

  return true;
}

static boolean test_sam_stream(vfile *vf, const char *filename)
{
  // .SAM files are raw 8363Hz signed 8-bit samples and can only be identified
  // by their .SAM extension.
  ssize_t ext_pos = path_get_ext_offset(filename);
  if(ext_pos < 0)
    return false;

  return strcasecmp(filename + ext_pos, ".sam") == 0;
}

void init_wav(struct config_info *conf)
{
  audio_ext_register(test_sam_stream, construct_sam_stream);
  audio_ext_register(test_wav_stream, construct_wav_stream);
}
