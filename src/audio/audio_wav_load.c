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

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_struct.h"
#include "audio_wav_load.h"

#include "../util.h"
#include "../io/vio.h"

#ifdef CONFIG_SDL
#include "../platform/sdl/SDLmzx.h" // SDL WAV loader fallback
#endif

// If the WAV/SAM is larger than this, print a warning to the console.
// (Right now only do this for debug builds because a lot more games than
// anticipated use big WAVs and it could get annoying for end users.)
#define WARN_FILESIZE (1<<22)

#define WAV_FOURCC(a,b,c,d)   (((uint32_t)(d) << 24) | ((c) << 16) | ((b) << 8) | (a))
#define WAV_PAD(x)            (((x) + 1) & ~1)

#define WAV_FORMAT_PCM        0x01
//#define WAV_FORMAT_MS_ADPCM   0x02
#define WAV_FORMAT_FLOAT      0x03
#define WAV_FORMAT_ALAW       0x06
#define WAV_FORMAT_MULAW      0x07
//#define WAV_FORMAT_IMA_ADPCM  0x11
//#define WAV_FORMAT_EXTENSIBLE 0xfffe

struct wave_fmt
{
  uint16_t fmt;
  uint16_t channels;
  uint32_t rate;
  uint32_t bytes_per_second;
  uint16_t bytes_per_frame;
  uint16_t sample_bits;
};

struct wave_smpl_loop
{
  uint32_t cue_point_id;
  uint32_t type;             /* 0 = forward; 1 = ping-pong; 2 = reverse(?) */
  uint32_t loop_start;       /* frame */
  uint32_t loop_end;         /* frame, INCLUSIVE! */
  uint32_t fraction;         /* 0 = none */
  uint32_t play_count;       /* 0 = infinite loop */
};

struct wave_smpl
{
  uint32_t manufacturer;
  uint32_t product;
  uint32_t sample_period_ns;
  uint32_t midi_unity_note;
  uint32_t midi_pitch_fraction;
  uint32_t smpte_format;
  uint32_t smpte_offset;
  uint32_t num_loops;
  uint32_t sampler_data_length;

  struct wave_smpl_loop loop; /* only care about the first one. */
};

static uint16_t mem_u16le(const uint8_t *buf)
{
  return (buf[1] << 8) | buf[0];
}

static uint32_t mem_u32le(const uint8_t *buf)
{
  return ((uint32_t)buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

static void *load_pcm_direct(uint32_t size, vfile *vf)
{
  void *dest = malloc(size);
  if(!dest)
    return NULL;

  if(vfread(dest, 1, size, vf) < size)
  {
    free(dest);
    return NULL;
  }
  return dest;
}

static boolean wav_load_riff_header(uint32_t *size, vfile *vf)
{
  uint8_t buf[12];

  if(vfread(buf, 1, sizeof(buf), vf) < sizeof(buf))
    return false;

  if(mem_u32le(buf + 0) != WAV_FOURCC('R','I','F','F'))
    return false;
  if(mem_u32le(buf + 8) != WAV_FOURCC('W','A','V','E'))
    return false;

  /* At least 24 bytes required for a valid fmt chunk; limit to sane sizes. */
  *size = mem_u32le(buf + 4);
  if(*size < 24 || *size > 0x7fffffffu)
    return false;

  return true;
}

static boolean wav_load_riff_tag(uint32_t *tag, uint32_t *size, vfile *vf)
{
  uint8_t buf[8];

  if(vfread(buf, 1, sizeof(buf), vf) < sizeof(buf))
    return false;

  *tag = mem_u32le(buf + 0);
  *size = mem_u32le(buf + 4);
  return true;
}

static boolean wav_skip(uint32_t size, vfile *vf)
{
  if(size && vfseek(vf, size, SEEK_CUR) < 0)
    return false;

  return true;
}

static boolean wav_load_fmt_chunk(struct wave_fmt *dest, uint32_t size, vfile *vf)
{
  uint8_t buf[16];

  if(size < 16 || vfread(buf, 1, 16, vf) < 16)
    return false;
  size -= 16;

  dest->fmt               = mem_u16le(buf + 0);
  dest->channels          = mem_u16le(buf + 2);
  dest->rate              = mem_u32le(buf + 4);
  dest->bytes_per_second  = mem_u32le(buf + 8);
  dest->bytes_per_frame   = mem_u16le(buf + 12);
  dest->sample_bits       = mem_u16le(buf + 14);

  /* Technically there's no problem loading >2 channels, but the player doesn't
   * support it. 0 channels and 0 rate is meaningless. */
  if(dest->channels == 0 || dest->channels > 2)
    return false;
  if(dest->rate == 0 || dest->rate > (uint32_t)INT_MAX)
    return false;

  /* Per-format handling (currently only handling PCM internally). */
  switch(dest->fmt)
  {
    case WAV_FORMAT_PCM:
      /* Do not allow 24-bit/32-bit formats to discourage huge files.
       * (Also, the player currently doesn't support them anyway.) */
      if(dest->sample_bits != 8 && dest->sample_bits != 16)
        return false;
  }

  /* Also safety check the bytes per second and bytes per frame fields.
   * This doesn't work for the ADPCM formats, which use these fields
   * for other purposes.
   */
  if(dest->fmt == WAV_FORMAT_PCM || dest->fmt == WAV_FORMAT_FLOAT ||
     dest->fmt == WAV_FORMAT_ALAW || dest->fmt == WAV_FORMAT_MULAW)
  {
    if(((dest->sample_bits + 7) >> 3) * dest->channels != dest->bytes_per_frame)
      return false;
    if(dest->bytes_per_second / dest->rate != dest->bytes_per_frame)
      return false;
  }
  return wav_skip(WAV_PAD(size), vf);
}

static boolean wav_load_smpl_chunk(struct wave_smpl *dest, uint32_t size, vfile *vf)
{
  uint8_t buf[36];

  dest->loop.loop_start = 0;
  dest->loop.loop_end = 0;

  if(size < 36 || vfread(buf, 1, 36, vf) < 36)
    return false;
  size -= 36;

  //dest->manufacturer        = mem_u32le(buf + 0);
  //dest->product             = mem_u32le(buf + 4);
  //dest->sample_period_ns    = mem_u32le(buf + 8);
  //dest->midi_unity_note     = mem_u32le(buf + 12);
  //dest->midi_pitch_fraction = mem_u32le(buf + 16);
  //dest->smpte_format        = mem_u32le(buf + 20);
  //dest->smpte_offset        = mem_u32le(buf + 24);
  dest->num_loops             = mem_u32le(buf + 28);
  //dest->sampler_data_length = mem_u32le(buf + 32);

  if(dest->num_loops >= 1 && size >= 24)
  {
    if(vfread(buf, 1, 24, vf) < 24)
      return false;
    size -= 24;

    //dest->loop.cue_point_id = mem_u32le(buf + 0);
    //dest->loop.type         = mem_u32le(buf + 4);
    dest->loop.loop_start     = mem_u32le(buf + 8);
    dest->loop.loop_end       = mem_u32le(buf + 12);
    //dest->loop.fraction     = mem_u32le(buf + 16);
    //dest->loop.play_count   = mem_u32le(buf + 20);
  }

  return wav_skip(WAV_PAD(size), vf);
}

static boolean wav_load_data_chunk(void **dest, uint32_t size, vfile *vf)
{
  if(size == 0)
    return false;

  *dest = load_pcm_direct(size, vf);
  if(!*dest)
    return false;

  if((size & 1) && vfseek(vf, 1, SEEK_CUR) < 0)
    return false;

  return true;
}

#ifdef CONFIG_SDL
// SDL-based WAV loader, used as a fallback if the regular loader fails.
// It supports more formats than the regular loader.
static boolean load_wav_file_sdl(const char *filename, struct wav_info *spec)
{
  SDL_AudioSpec sdlspec;
  Uint8 *sdl_buf;
  Uint32 sdl_length;
  void *copy_buf;

  if(!SDL_LoadWAV(filename, &sdlspec, &sdl_buf, &sdl_length))
    return false;

  copy_buf = malloc(sdl_length);
  if(!copy_buf)
  {
    SDL_FreeWAV(sdl_buf);
    return false;
  }

  memcpy(copy_buf, sdl_buf, sdl_length);
  SDL_FreeWAV(sdl_buf);

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

  spec->wav_data = (uint8_t *)copy_buf;
  spec->data_length = sdl_length;
  spec->channels = sdlspec.channels;
  spec->freq = sdlspec.freq;
  return true;
}
#endif

boolean audio_load_wav(struct wav_info *dest, vfile *vf, const char *filename)
{
  struct wave_fmt fmt;
  struct wave_smpl smpl;
  uint32_t riff_size;
  uint32_t chunk_tag;
  uint32_t chunk_size;
  boolean has_fmt = false;
  boolean has_data = false;
  boolean has_smpl = false;

  int64_t file_size = vfilelength(vf, false);
  if(file_size > WARN_FILESIZE)
  {
    trace("This WAV is too big sempai OwO;;;\n");
    trace("Size of WAV file '%s' is %" PRId64 "; OGG should be used instead.\n",
     filename, file_size);
  }

  if(!wav_load_riff_header(&riff_size, vf))
    return false;

  dest->wav_data = NULL;
  // Default to no loop
  dest->loop_start = 0;
  dest->loop_end = 0;
  // Not a SAM, so don't enable this hack.
  dest->enable_sam_frequency_hack = false;

  memset(&fmt, 0, sizeof(fmt));
  while(wav_load_riff_tag(&chunk_tag, &chunk_size, vf))
  {
    switch(chunk_tag)
    {
      case WAV_FOURCC('f','m','t',' '):
        if(has_fmt)
          goto skip;
        has_fmt = true;

        if(!wav_load_fmt_chunk(&fmt, chunk_size, vf))
          goto err;

        dest->channels = fmt.channels;
        dest->freq     = fmt.rate;
        dest->format   = (fmt.sample_bits == 8) ? SAMPLE_U8 : SAMPLE_S16LE;
        break;

      case WAV_FOURCC('s','m','p','l'):
        if(!has_fmt || has_smpl)
          goto skip;
        has_smpl = true;

        if(!wav_load_smpl_chunk(&smpl, chunk_size, vf))
          goto err;
        break;

      case WAV_FOURCC('d','a','t','a'):
        if(!has_fmt || has_data)
          goto skip;
        has_data = true;

        if(fmt.fmt == WAV_FORMAT_PCM)
        {
          void *data;
          if(!wav_load_data_chunk(&data, chunk_size, vf))
            return false;

          dest->wav_data = (uint8_t *)data;
          dest->data_length = chunk_size;
        }
        else
        {
#ifdef CONFIG_SDL
          if(!load_wav_file_sdl(filename, dest))
#endif
            goto err;

          /* Correct this value since it's going to be wrong for mu-law/etc. */
          fmt.bytes_per_frame = fmt.channels;
          if(dest->format == SAMPLE_S16LE || dest->format == SAMPLE_S16BE)
            fmt.bytes_per_frame *= 2;
          goto skip;
        }
        break;

      default: /* Unsupported, just skip. */
      skip:
        if(!wav_skip(chunk_size, vf))
          goto done;
        break;
    }
  }
done:

  if(!has_fmt || !has_data)
    goto err;

  if(has_smpl)
  {
    uint64_t loop_start = (uint64_t)smpl.loop.loop_start * fmt.bytes_per_frame;
    uint64_t loop_end = ((uint64_t)smpl.loop.loop_end + 1) * fmt.bytes_per_frame;

    // Silently correct off-by-one loop ends...
    if(loop_end == (uint64_t)dest->data_length + fmt.bytes_per_frame)
      loop_end -= fmt.bytes_per_frame;

    // Boundary check loop points. Since the data length can never be over
    // UINT32_MAX, this implicitly filters out overflowing start/end points.
    if(loop_start >= dest->data_length || loop_end > dest->data_length ||
       loop_start >= loop_end)
      return true;

    dest->loop_start = (uint32_t)loop_start;
    dest->loop_end = (uint32_t)loop_end;
  }
  return true;

err:
  audio_free_wav(dest);
  return false;
}

boolean audio_load_sam(struct wav_info *dest, vfile *vf, const char *filename)
{
  size_t source_length;
  void *buf;

  source_length = vfilelength(vf, false);
  if(source_length > WARN_FILESIZE)
  {
    trace("Size of SAM file '%s' is %zu; OGG should be used instead.\n",
     filename, source_length);
  }

  buf = load_pcm_direct(source_length, vf);
  if(!buf)
    return false;

  dest->wav_data = (uint8_t *)buf;
  dest->data_length = source_length;
  dest->channels = 1;
  dest->freq = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
  dest->format = SAMPLE_S8;
  dest->loop_start = 0;
  dest->loop_end = 0;
  dest->enable_sam_frequency_hack = true;

  return true;
}

void audio_free_wav(struct wav_info *src)
{
  free(src->wav_data);
  src->wav_data = NULL;
}
