/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

/* Audio subsystem internal defines and types. */

#ifndef MEGAZEUX_AUDIO_STRUCT_H
#define MEGAZEUX_AUDIO_STRUCT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

#include "../configure.h"
#include "../io/vfile.h"
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

struct audio_player;
struct audio_stream;
struct sampled_stream;

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

struct audio_driver
{
  const char *name;
  const char *ident;
  /* If true, this driver can be initialized automatically on startup.
   * If false, only initialize if explicitly requested by the user.
   * This should be false only for devices that can have bad side effects
   * where assumed to exist when they actually do not, e.g. LPT DACs. */
  boolean allow_auto_init;

  /* Driver-specific audio players. Put general players in audio_players.c. */
  const struct audio_player * const * const driver_players;
  const struct audio_player * const pcs_software_player;
  const struct audio_player * const pcs_hardware_player;

  boolean (*init_audio_driver)(struct config_info *conf);
  void    (*quit_audio_driver)(void);
};

struct audio_player
{
  const char *name;

  boolean   (*test)(vfile *vf, const char *filename, boolean is_primary);

  /* Construct an audio stream. Only call this function from the main thread,
   * and do NOT call it under lock; it will lock when needed.
   */
  struct audio_stream *(*construct)(vfile *vf, const char *filename,
                                    uint32_t frequency, unsigned int volume,
                                    boolean repeat);

  /* Destruct an audio stream. Do not call this function under lock; it should
   * have been moved to a local list (also removed from audio.primary_stream
   * etc.) prior to destruction. In the DOS port, only call from the main thread
   * (see AUDIO_GARBAGE_COLLECTOR).
   */
  void      (*destruct)(struct audio_stream *a_src);

  /* Render data to PCM. Non-PCM players do not need to implement this.
   * A return value of true signals to the software mixer that this stream
   * has finished playing and may be destroyed.
   */
  boolean   (* mix_data)(struct audio_stream *a_src, int32_t * RESTRICT buffer,
                         size_t dest_frames, unsigned int dest_channels);

  /* Volume--this should be implemented for everything possible. An example
   * of a rare exception where it is not possible: hardware PIT PC speaker.
   */
  void      (* set_volume)(struct audio_stream *a_src, unsigned int volume);

  /* Repeat--if set to 0, the stream should terminate when it has reached
   * the end. Should be implemented for everything except PC speaker.
   */
  void      (* set_repeat)(struct audio_stream *a_src, boolean repeat);

  /* Module position in sequence ("order")--this is intended for modules only.
   * It currently has no meaning for other formats and should not be supported
   * by them (TODO: maybe cue points in Ogg/WAV?).
   */
  void      (* set_order)(struct audio_stream *a_src, uint32_t order);
  uint32_t  (* get_order)(struct audio_stream *a_src);

  /* Position and length--for module formats, this is measured in rows from
   * the start of the module (originally derived from libmodplug). For PCM,
   * this is measured in sample frames from the start of the stream. Every
   * player needs to implement this where possible, but some module rendering
   * libraries make it more difficult than others.
   */
  void      (* set_position)(struct audio_stream *a_src, uint32_t pos);
  uint32_t  (* get_position)(struct audio_stream *a_src);
  uint32_t  (* get_length)(struct audio_stream *a_src);

  /* Loop start and loop end--the position (in sample frames, measured from
   * the start of the stream) of the current start and end of the loop. This
   * is only relevant for PCM, and module players should not implement it.
   * The loop end position must be EXCLUSIVE for all players.
   */
  void      (* set_loop_start)(struct audio_stream *a_src, uint32_t pos);
  uint32_t  (* get_loop_start)(struct audio_stream *a_src);
  void      (* set_loop_end)(struct audio_stream *a_src, uint32_t pos);
  uint32_t  (* get_loop_end)(struct audio_stream *a_src);

  /* Resampling frequency--this is a feature of MegaZeux's resampler,
   * and it should not be implemented by anything except sampled streams.
   * Do not attempt to approximate this by e.g. adjusting module timing.
   */
  void      (* set_frequency)(struct sampled_stream *s_src, uint32_t frequency);
  uint32_t  (* get_frequency)(struct sampled_stream *s_src);

  /* Get a copy of a sample from this audio stream as a raw PCM spec for use
   * by audio_spot_sample. Currently, only the libxmp player implements this.
   * audio_spot_sample assumes this function allocates a copy of the sample.
   */
  boolean   (* get_sample)(struct audio_stream *a_src, unsigned int which,
             struct wav_info *dest);
};

struct audio_stream
{
  struct audio_stream *next;
  struct audio_stream *previous;
  const struct audio_player *player;
  unsigned int volume;
  boolean is_spot_sample;
  boolean repeat;
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

struct audio_stream_list
{
  struct audio_stream *base;
  struct audio_stream *end;
};

struct audio
{
  const struct audio_driver *driver;
  int32_t *mix_buffer;
  size_t buffer_bytes;
  unsigned buffer_frames;
  unsigned buffer_channels;

  size_t output_frequency;
  unsigned int global_resample_mode;
  int max_simultaneous_samples;
  int max_simultaneous_samples_config;
  boolean pc_speaker_use_hardware;
  boolean opl_use_hardware;
  int opl_port;

  struct audio_stream *primary_stream;
  struct audio_stream *pcs_stream;
  struct audio_stream_list stream_list;
#ifdef AUDIO_GARBAGE_COLLECTOR
  struct audio_stream_list garbage_list;
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
