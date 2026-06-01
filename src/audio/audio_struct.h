/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

/* Audio subsystem internal defines and types. */

#ifndef MEGAZEUX_AUDIO_STRUCT_H
#define MEGAZEUX_AUDIO_STRUCT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

#include "../configure.h"
#include "../platform/platform.h"

#define AUDIO_SFX_QUEUE_SIZE 4096

#ifdef CONFIG_DJGPP
#define AUDIO_GARBAGE_COLLECTOR
#endif

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
#define SAMPLE_S16 SAMPLE_S16BE
#else
#define SAMPLE_S16 SAMPLE_S16LE
#endif

enum wav_format
{
  SAMPLE_U8,
  SAMPLE_S8,
  SAMPLE_S16LE,
  SAMPLE_S16BE
};

struct wav_info
{
  /* Note: WAV fields are limited to 32-bits by the file format. */
  uint8_t *wav_data;
  uint32_t data_length;
  uint32_t channels;
  uint32_t freq;
  uint32_t loop_start;
  uint32_t loop_end;
  enum wav_format format;
  boolean enable_sam_frequency_hack;
};

struct audio_stream
{
  struct audio_stream *next;
  struct audio_stream *previous;
  unsigned int volume;
  boolean is_spot_sample;
  boolean repeat;
  boolean   (* mix_data)(struct audio_stream *a_src, int32_t * RESTRICT buffer,
                         size_t dest_frames, unsigned int dest_channels);
  void      (* set_volume)(struct audio_stream *a_src, unsigned int volume);
  void      (* set_repeat)(struct audio_stream *a_src, boolean repeat);
  void      (* set_order)(struct audio_stream *a_src, uint32_t order);
  void      (* set_position)(struct audio_stream *a_src, uint32_t pos);
  void      (* set_loop_start)(struct audio_stream *a_src, uint32_t pos);
  void      (* set_loop_end)(struct audio_stream *a_src, uint32_t pos);
  uint32_t  (* get_order)(struct audio_stream *a_src);
  uint32_t  (* get_position)(struct audio_stream *a_src);
  uint32_t  (* get_length)(struct audio_stream *a_src);
  uint32_t  (* get_loop_start)(struct audio_stream *a_src);
  uint32_t  (* get_loop_end)(struct audio_stream *a_src);
  boolean   (* get_sample)(struct audio_stream *a_src, unsigned int which,
             struct wav_info *dest);
  void      (* destruct)(struct audio_stream *a_src);
};

struct audio_stream_spec
{
  boolean   (* mix_data)(struct audio_stream *a_src, int32_t * RESTRICT buffer,
                         size_t dest_frames, unsigned int dest_channels);
  void      (* set_volume)(struct audio_stream *a_src, unsigned int volume);
  void      (* set_repeat)(struct audio_stream *a_src, boolean repeat);
  void      (* set_order)(struct audio_stream *a_src, uint32_t order);
  void      (* set_position)(struct audio_stream *a_src, uint32_t pos);
  void      (* set_loop_start)(struct audio_stream *a_src, uint32_t pos);
  void      (* set_loop_end)(struct audio_stream *a_src, uint32_t pos);
  uint32_t  (* get_order)(struct audio_stream *a_src);
  uint32_t  (* get_position)(struct audio_stream *a_src);
  uint32_t  (* get_length)(struct audio_stream *a_src);
  uint32_t  (* get_loop_start)(struct audio_stream *a_src);
  uint32_t  (* get_loop_end)(struct audio_stream *a_src);
  boolean   (* get_sample)(struct audio_stream *a_src, unsigned int which,
             struct wav_info *dest);
  void      (* destruct)(struct audio_stream *a_src);
};

/**
 * NOTE: these were signed 16-bit ints in DOS versions and signed 32-bit ints
 * up through 2.92e. It is highly unlikely anything ever relied on a duration
 * over 65535 (~131 seconds at 500 Hz).
 *
 * The frequency is guaranteed to never actually need anything higher than the
 * table below, so there's no reason for it to be 32-bit.
 */
struct audio_sfx_note
{
  uint16_t duration;
  uint16_t freq;
};

struct audio_sfx_data
{
  struct audio_sfx_note queue[AUDIO_SFX_QUEUE_SIZE];
  int head_index;
  int tail_index;

  /* In DOS versions, clearing the SFX queue would also stop the current
   * note. The best that can be done here is to alert the PC speaker stream
   * to cancel the current playing sound the next time pcs_mix_data runs. */
  boolean cancel_current_note;
};

struct audio
{
  int32_t *mix_buffer;
  size_t buffer_bytes;
  unsigned buffer_frames;
  unsigned buffer_channels;

  size_t output_frequency;
  unsigned int global_resample_mode;
  int max_simultaneous_samples;
  int max_simultaneous_samples_config;

  struct audio_stream *primary_stream;
  struct audio_stream *pcs_stream;
  struct audio_stream *stream_list_base;
  struct audio_stream *stream_list_end;
#ifdef AUDIO_GARBAGE_COLLECTOR
  struct audio_stream *garbage_list_base;
  struct audio_stream *garbage_list_end;
#endif

  platform_mutex audio_mutex;
  platform_mutex audio_sfx_mutex;
#ifdef DEBUG
  platform_mutex audio_debug_mutex;
#endif

  boolean music_on;
  boolean pcs_on;
  unsigned int music_volume;
  unsigned int sound_volume;
  unsigned int pcs_volume;

  struct audio_sfx_data sfx; /* Protected by audio_sfx_mutex. */
};

extern struct audio audio;

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_STRUCT_H */
