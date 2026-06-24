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

#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_players.h"
#include "audio_struct.h"
#include "sampled_stream.h"

#include "../io/memfile.h"
#include "../io/vio.h"

// Safety check: DJGPP absolutely can not use vorbis/tremor here because they
// are not interrupt-safe. stb_vorbis can be used under the condition that
// alloca is patched out (probably not interrupt-safe) and stdio is disabled
// (not interrupt-safe). All allocations in the patched stb_vorbis are
// performed during setup. Additionally, a patch is required to add sample-
// accurate position tracking.
//
// alloca removal patch (not yet contributed upstream):
// https://github.com/sezero/stb/commit/7dde771bc8181796d516b67b5e5391594674b041

#if defined(CONFIG_DJGPP) && !defined(CONFIG_STB_VORBIS)
#error Vorbis/Tremor are not interrupt-safe, please fix your config!
#endif

#if defined(CONFIG_STB_VORBIS)
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include <stb/stb_vorbis.c>
#elif defined(CONFIG_TREMOR)
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#endif

typedef struct
{
  size_t stream_length;
  int rate;
  int channels;
} audio_vorbis_info;

#ifdef CONFIG_STB_VORBIS
typedef stb_vorbis *audio_vorbis_handle;

static boolean audio_vorbis_handle_init(audio_vorbis_handle *f, vfile *vf)
{
  struct memfile tmp;
  int64_t len;
  int err;

  // stb_vorbis is mainly for the DOS port and file IO isn't
  // interrupt safe, so force the entire file to RAM.
  if(!vfile_force_to_memory(vf))
    return false;

  len = vfilelength(vf, true);
  if(len < 0)
    return false;

  // TODO: stb_vorbis should really accept callbacks instead of this hack.
  // This may potentially be disastrous with builds that use VFS caching.
  if(!vfile_get_memfile_block(vf, len, &tmp))
    return false;

  *f = stb_vorbis_open_memory(tmp.start, len, &err, NULL);
  return *f != NULL;
}

static void audio_vorbis_handle_close(audio_vorbis_handle *f)
{
  stb_vorbis_close(*f);
  *f = NULL;
}

static boolean audio_vorbis_handle_info(audio_vorbis_info *dest,
 audio_vorbis_handle *f)
{
  stb_vorbis_info info = stb_vorbis_get_info(*f);
  dest->stream_length = stb_vorbis_stream_length_in_samples(*f);
  dest->channels = info.channels;
  dest->rate = info.sample_rate;
  return true;
}

static char **audio_vorbis_handle_comments(int *num, audio_vorbis_handle *f)
{
  stb_vorbis_comment comment = stb_vorbis_get_comment(*f);
  *num = comment.comment_list_length;
  return comment.comment_list;
}

static int audio_vorbis_handle_rewind(audio_vorbis_handle *f)
{
  return stb_vorbis_seek_start(*f);
}

static int audio_vorbis_handle_seek(audio_vorbis_handle *f, int64_t position)
{
  return stb_vorbis_seek(*f, (uint32_t)position);
}

static int64_t audio_vorbis_handle_tell(audio_vorbis_handle *f)
{
  // Requires https://github.com/nothings/stb/pull/1295
  return stb_vorbis_get_playback_sample_offset(*f);
}

static size_t audio_vorbis_handle_read(void * RESTRICT dest,
 size_t bytes_to_read, unsigned channels, audio_vorbis_handle *f)
{
  return stb_vorbis_get_samples_short_interleaved(*f, channels, dest,
   bytes_to_read >> 1) * channels * sizeof(int16_t);
}

#else /* libvorbis/tremor */

// ov_read() documentation states the 'bigendianp' argument..
//   "Specifies big or little endian byte packing.
//   0 for little endian, 1 for big endian. Typical value is 0."

// Tremor (as of r19494) doesn't use this, so suppress the warning...
#ifndef CONFIG_TREMOR
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
static const int ENDIAN_PACKING = 1;
#else
static const int ENDIAN_PACKING = 0;
#endif
#endif // !CONFIG_TREMOR

typedef OggVorbis_File audio_vorbis_handle;

static size_t vorbis_read_fn(void *ptr, size_t size, size_t count, void *stream)
{
  vfile *vf = (vfile *)stream;
  return vfread(ptr, size, count, vf);
}

static int vorbis_seek_fn(void *stream, ogg_int64_t offset, int whence)
{
  vfile *vf = (vfile *)stream;
  return vfseek(vf, offset, whence);
}

static int vorbis_close_fn(void *stream)
{
  // Do nothing; tremor lowmem will CRASH if this function isn't provided.
  return 0;
}

static long vorbis_tell_fn(void *stream)
{
  vfile *vf = (vfile *)stream;
  return vftell(vf);
}

static const ov_callbacks vorbis_callbacks =
{
  vorbis_read_fn,
  vorbis_seek_fn,
  vorbis_close_fn,
  vorbis_tell_fn,
};

static boolean audio_vorbis_handle_init(audio_vorbis_handle *f, vfile *vf)
{
  if(ov_open_callbacks(vf, f, NULL, 0, vorbis_callbacks) != 0)
    return false;

  return true;
}

static void audio_vorbis_handle_close(audio_vorbis_handle *f)
{
  ov_clear(f);
}

static boolean audio_vorbis_handle_info(audio_vorbis_info *dest,
 audio_vorbis_handle *f)
{
  vorbis_info *vorbis_file_info = ov_info(f, -1);
  if(!vorbis_file_info)
    return false;

  dest->stream_length = ov_pcm_total(f, -1);
  dest->channels = vorbis_file_info->channels;
  dest->rate = vorbis_file_info->rate;
  return true;
}

static char **audio_vorbis_handle_comments(int *num, audio_vorbis_handle *f)
{
  vorbis_comment *comment = ov_comment(f, -1);
  if(!comment)
    return NULL;

  *num = comment->comments;
  return comment->user_comments;
}

static int audio_vorbis_handle_rewind(audio_vorbis_handle *f)
{
  return ov_raw_seek(f, 0);
}

static int audio_vorbis_handle_seek(audio_vorbis_handle *f, int64_t pos)
{
  return ov_pcm_seek(f, (ogg_int64_t)pos);
}

static int64_t audio_vorbis_handle_tell(audio_vorbis_handle *f)
{
  return ov_pcm_tell(f);
}

static size_t audio_vorbis_handle_read(void * RESTRICT dest,
 size_t bytes_to_read, unsigned channels, audio_vorbis_handle *f)
{
  int current_bitstream;
#ifdef CONFIG_TREMOR
  return ov_read(f, dest, bytes_to_read, &current_bitstream);
#else
  return ov_read(f, dest, bytes_to_read,
   ENDIAN_PACKING, sizeof(int16_t), 1, &current_bitstream);
#endif
}
#endif /* libvorbis/tremor */

struct vorbis_stream
{
  struct sampled_stream s;
  audio_vorbis_handle handle;
  audio_vorbis_info info;
  vfile *input_file;
  uint32_t loop_start;
  uint32_t loop_end;
};

static boolean vorbis_mix_data(struct audio_stream *a_src,
 int32_t * RESTRICT buffer, size_t frames, unsigned int channels)
{
  uint32_t read_len = 0;
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  char *read_buffer;
  size_t read_wanted;
  size_t read_channels = v_stream->s.channels;
  uint32_t pos = 0;

  read_buffer = (char *)sampled_get_buffer(&v_stream->s, &read_wanted);

  do
  {
    read_wanted -= read_len;

    if(a_src->repeat && v_stream->loop_end)
      pos = (uint32_t)audio_vorbis_handle_tell(&v_stream->handle);

    read_len = audio_vorbis_handle_read(read_buffer, read_wanted,
     read_channels, &(v_stream->handle));

    if(a_src->repeat && (pos < v_stream->loop_end) &&
     (pos + read_len / read_channels / sizeof(int16_t) >= v_stream->loop_end))
    {
      read_len = (v_stream->loop_end - pos) * read_channels * sizeof(int16_t);
      audio_vorbis_handle_seek(&(v_stream->handle), v_stream->loop_start);
    }

    // If it hit the end go back to the beginning if repeat is on

    if(read_len == 0)
    {
      if(a_src->repeat)
      {
        audio_vorbis_handle_rewind(&(v_stream->handle));

        if(read_wanted)
        {
          read_len = audio_vorbis_handle_read(read_buffer, read_wanted,
           read_channels, &(v_stream->handle));
        }
      }
      else
      {
        memset(read_buffer, 0, read_wanted);
      }
    }

    read_buffer += read_len;
  } while((read_len < read_wanted) && read_len);

  sampled_mix_data((struct sampled_stream *)v_stream, buffer, frames, channels);

  if(read_len == 0)
    return true;

  return false;
}

static void vorbis_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  a_src->volume = volume * 256 / 255;
}

static void vorbis_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  a_src->repeat = repeat;
}

static void vorbis_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct vorbis_stream *v = (struct vorbis_stream *)a_src;
  audio_vorbis_handle_seek(&v->handle, (int64_t)position);
}

static void vorbis_set_loop_start(struct audio_stream *a_src, uint32_t position)
{
  ((struct vorbis_stream *)a_src)->loop_start = position;
}

static void vorbis_set_loop_end(struct audio_stream *a_src, uint32_t position)
{
  ((struct vorbis_stream *)a_src)->loop_end = position;
}

static void vorbis_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  struct vorbis_stream *v_stream = (struct vorbis_stream *)s_src;

  sampled_set_buffer(s_src, frequency, v_stream->info.rate);
}

static uint32_t vorbis_get_position(struct audio_stream *a_src)
{
  struct vorbis_stream *v = (struct vorbis_stream *)a_src;
  return (uint32_t)audio_vorbis_handle_tell(&v->handle);
}

static uint32_t vorbis_get_length(struct audio_stream *a_src)
{
  return ((struct vorbis_stream *)a_src)->info.stream_length;
}

static uint32_t vorbis_get_loop_start(struct audio_stream *a_src)
{
  return ((struct vorbis_stream *)a_src)->loop_start;
}

static uint32_t vorbis_get_loop_end(struct audio_stream *a_src)
{
  return ((struct vorbis_stream *)a_src)->loop_end;
}

static uint32_t vorbis_get_frequency(struct sampled_stream *s_src)
{
  return s_src->relative_frequency;
}

static void vorbis_destruct(struct audio_stream *a_src)
{
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  audio_vorbis_handle_close(&(v_stream->handle));
  vfclose(v_stream->input_file);
  sampled_destruct(a_src);
}

static void get_loopstart_loopend(struct vorbis_stream *v_stream)
{
  char **comments;
  int loopstart = -1;
  int looplength = -1;
  int loopend = -1;
  int num;
  int i;

  comments = audio_vorbis_handle_comments(&num, &(v_stream->handle));
  if(!comments)
    return;

  for(i = 0; i < num; i++)
  {
    if(!strncasecmp("loopstart=", comments[i], 10))
      loopstart = atoi(comments[i] + 10);
    else

    if(!strncasecmp("loopend=", comments[i], 8))
      loopend = atoi(comments[i] + 8);
    else

    if(!strncasecmp("looplength=", comments[i], 11))
      looplength = atoi(comments[i] + 11);
  }

  if(loopstart >= 0 && (looplength > 0 || loopend > loopstart))
  {
    v_stream->loop_start = loopstart;

    // looplength takes priority since it's older and more "standard"
    if(looplength > 0)
      v_stream->loop_end = loopstart + looplength;
    else
      v_stream->loop_end = loopend;
  }
}

static struct audio_stream *vorbis_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct vorbis_stream *v_stream;
  audio_vorbis_handle handle;
  audio_vorbis_info info;

  if(!audio_vorbis_handle_init(&handle, vf))
    return NULL;

  // Surround OGGs not supported yet..
  if(!audio_vorbis_handle_info(&info, &handle) || info.channels > 2)
  {
    audio_vorbis_handle_close(&handle);
    return NULL;
  }

  v_stream = (struct vorbis_stream *)cmalloc(sizeof(struct vorbis_stream));
  if(!v_stream)
  {
    audio_vorbis_handle_close(&handle);
    return NULL;
  }
  v_stream->s.a.player = &audio_player_vorbis;

  v_stream->handle = handle;
  v_stream->info = info;
  v_stream->input_file = vf;

  v_stream->loop_start = 0;
  v_stream->loop_end = 0;
  get_loopstart_loopend(v_stream);

  initialize_sampled_stream((struct sampled_stream *)v_stream,
   info.rate, frequency, info.channels, true);

  initialize_audio_stream((struct audio_stream *)v_stream, volume, repeat);

  return (struct audio_stream *)v_stream;
}

static boolean vorbis_test(vfile *vf, const char *filename)
{
  char buf[4];
  if(!vfread(buf, 4, 1, vf))
    return false;

  return memcmp(buf, "OggS", 4) == 0;
}

const struct audio_player audio_player_vorbis =
{
  "Ogg Vorbis",
  vorbis_test,

  vorbis_construct,
  vorbis_destruct,
  vorbis_mix_data,
  vorbis_set_volume,
  vorbis_set_repeat,
  NULL,
  NULL,
  vorbis_set_position,
  vorbis_get_position,
  vorbis_get_length,
  vorbis_set_loop_start,
  vorbis_get_loop_start,
  vorbis_set_loop_end,
  vorbis_get_loop_end,
  vorbis_set_frequency,
  vorbis_get_frequency,
  NULL,
};
