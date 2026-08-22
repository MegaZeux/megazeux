/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <math.h>

#include <sys/stat.h>

#include "audio.h"
#include "audio_drivers.h"
#include "audio_players.h"
#include "audio_sfx.h"
#include "audio_struct.h"
#include "audio_wav.h"
#include "sampled_stream.h"

#include "../configure.h"
#include "../data.h"
#include "../util.h"
#include "../io/fsafeopen.h"
#include "../platform/platform.h"

// May be used by audio plugins
struct audio audio;

#ifndef DEBUG

#define LOCK()   platform_mutex_lock(&audio.audio_mutex)
#define UNLOCK() platform_mutex_unlock(&audio.audio_mutex)

#else /* DEBUG */

#include "../platform/thread_debug.h"

static platform_mutex_debug mutex_debug;

#define LOCK()   platform_mutex_lock_debug(&audio.audio_mutex, \
                  &audio.audio_debug_mutex, &mutex_debug, __FILE__, __LINE__)
#define UNLOCK() platform_mutex_unlock_debug(&audio.audio_mutex, \
                  &audio.audio_debug_mutex, &mutex_debug, __FILE__, __LINE__)

#endif // DEBUG

/* Special: DOS calls the audio callback from a hardware interrupt and
 * implements mutexes with enable()/disable(). Interrupts very much should
 * NOT be disabled in the audio callback, as it can run a long time and
 * interfere with timer/audio driver functions. In other words: do the exact
 * opposite of usual in the audio callback.
 */
#ifdef CONFIG_DJGPP
#define LOCK_AUDIO_THREAD()   enable();
#define UNLOCK_AUDIO_THREAD() disable();
#else
#define LOCK_AUDIO_THREAD()   LOCK()
#define UNLOCK_AUDIO_THREAD() UNLOCK()
#endif

static unsigned int volume_function(int input, int volume_setting)
{
  /* Adjust volume (0-255) exponentially according to a given setting (0-10).
   * 0 is no volume whatsoever and 10 is maximum volume. */

  double setting_f = (double)volume_setting / 10.0;
  double setting_exp = (exp(setting_f) - 1.0) / (M_E - 1.0);
  int output = (int)((double)input * setting_exp + 0.5);

  return CLAMP(output, 0, 255);
}

// Disable most of the standard audio implementation on NDS, where
// hardware mixing is utilized.
#ifndef CONFIG_NDS

static void audio_stream_insert_list(struct audio_stream **base,
 struct audio_stream **end, struct audio_stream *a_src)
{
  if(*base == NULL)
  {
    *base = a_src;
  }
  else
  {
    (*end)->next = a_src;
  }

  a_src->next = NULL;
  a_src->previous = *end;
  *end = a_src;
}

static void audio_stream_remove_from_lists(struct audio_stream *a_src)
{
  if(a_src == audio.primary_stream)
    audio.primary_stream = NULL;

  if(a_src == audio.stream_list_base)
    audio.stream_list_base = a_src->next;

  if(a_src == audio.stream_list_end)
    audio.stream_list_end = a_src->previous;

#ifdef AUDIO_GARBAGE_COLLECTOR
  if(a_src == audio.garbage_list_base)
    audio.garbage_list_base = a_src->next;

  if(a_src == audio.garbage_list_end)
    audio.garbage_list_end = a_src->previous;
#endif

  if(a_src->next)
    a_src->next->previous = a_src->previous;

  if(a_src->previous)
    a_src->previous->next = a_src->next;
}

static void audio_garbage_collect(void)
{
#ifdef AUDIO_GARBAGE_COLLECTOR
  struct audio_stream *a_src;
  struct audio_stream *next;

  for(a_src = audio.garbage_list_base; a_src; a_src = next)
  {
    next = a_src->next;
    a_src->player->destruct(a_src);
  }
  audio.garbage_list_base = audio.garbage_list_end = NULL;
#endif
}

// DOS audio is handled during an interrupt and can never be allowed to
// manage memory. This means audio stream cleanup needs to be delayed until
// the main thread creates a new stream or explicitly destroys other streams.
static void audio_garbage_queue(struct audio_stream *a_src)
{
#ifdef AUDIO_GARBAGE_COLLECTOR
  audio_stream_remove_from_lists(a_src);
  audio_stream_insert_list(&audio.garbage_list_base,
   &audio.garbage_list_end, a_src);
#else
  a_src->player->destruct(a_src);
#endif
}

void destruct_audio_stream(struct audio_stream *a_src)
{
  audio_stream_remove_from_lists(a_src);
  free(a_src);
}

void initialize_audio_stream(struct audio_stream *a_src,
 unsigned int volume, boolean repeat)
{
  a_src->is_spot_sample = false;

  if(a_src->player->set_volume)
    a_src->player->set_volume(a_src, volume);

  if(a_src->player->set_repeat)
    a_src->player->set_repeat(a_src, repeat);

  a_src->next = NULL;

  LOCK();

  audio_garbage_collect();
  audio_stream_insert_list(&audio.stream_list_base,
   &audio.stream_list_end, a_src);

  UNLOCK();
}

static void clip_buffer_u8(uint8_t *dest, int32_t *src, size_t samples)
{
  int32_t cur_sample;
  size_t i;

  for(i = 0; i < samples; i++)
  {
    cur_sample = src[i];
    if(cur_sample > 32767)
      cur_sample = 32767;

    if(cur_sample < -32768)
      cur_sample = -32768;

    dest[i] = (uint8_t)(cur_sample >> 8) + 128;
  }
}

static void clip_buffer_s8(int8_t *dest, int32_t *src, size_t samples)
{
  int32_t cur_sample;
  size_t i;

  for(i = 0; i < samples; i++)
  {
    cur_sample = src[i];
    if(cur_sample > 32767)
      cur_sample = 32767;

    if(cur_sample < -32768)
      cur_sample = -32768;

    dest[i] = cur_sample >> 8;
  }
}

static void clip_buffer_s16(int16_t *dest, int32_t *src, size_t samples)
{
  int32_t cur_sample;
  size_t i;

  for(i = 0; i < samples; i++)
  {
    cur_sample = src[i];
    if(cur_sample > 32767)
      cur_sample = 32767;

    if(cur_sample < -32768)
      cur_sample = -32768;

    dest[i] = cur_sample;
  }
}

/**
 * Render `frames` number of audio frames with the software mixer. The
 * output buffer must be able to hold a number of bytes equal to the
 * requested frames times the requested channels times the size in bytes
 * of the requested sample type. Destroys any audio streams that end
 * while rendering the output.
 *
 * Returns the number of frames successfully renderered.
 */
size_t audio_mixer_render_frames(void *stream, unsigned frames,
 unsigned channels, unsigned format)
{
  struct audio_stream *current_astream;
  size_t frames_chn;
  boolean destroy_flag;

  LOCK_AUDIO_THREAD();

  current_astream = audio.stream_list_base;

  if(current_astream && audio.mix_buffer && frames && channels)
  {
    // Due to how resampling is implemented, the requested frames
    // should never exceed the buffer's number of frames. If it does,
    // this call can't complete the entire request.
    if(frames > audio.buffer_frames)
      frames = audio.buffer_frames;

    // Additionally, if the output channels exceed the configured
    // number of channels, the frames may need to be limited further.
    frames_chn = frames * channels;
    if(frames_chn * sizeof(int32_t) > audio.buffer_bytes)
    {
      frames = audio.buffer_bytes / (channels * sizeof(int32_t));
      frames_chn = frames * channels;
    }

    memset(audio.mix_buffer, 0, frames_chn * sizeof(int32_t));

    while(current_astream != NULL)
    {
      struct audio_stream *next_astream = current_astream->next;

      if(current_astream->player->mix_data)
      {
        destroy_flag = current_astream->player->mix_data(current_astream,
         audio.mix_buffer, frames, channels);

        if(destroy_flag)
          audio_garbage_queue(current_astream);
      }

      current_astream = next_astream;
    }

    switch(format)
    {
      case SAMPLE_U8:
        clip_buffer_u8((uint8_t *)stream, audio.mix_buffer, frames_chn);
        break;

      case SAMPLE_S8:
        clip_buffer_s8((int8_t *)stream, audio.mix_buffer, frames_chn);
        break;

      case SAMPLE_S16:
        clip_buffer_s16((int16_t *)stream, audio.mix_buffer, frames_chn);
        break;

      default:
        warn("clip_buffer unimplemented for sample format %d!\n", format);
    }
  }

  UNLOCK_AUDIO_THREAD();

  return frames;
}

/**
 * This function initializes audio.output_frequency, audio.mix_buffer,
 * and audio.buffer_frames for software mixing. This function may reconfigure
 * the provided values of any of these fields.
 *
 * This should be called when initializing a sound driver but before unpausing
 * audio, and any time the sample rate, number of buffer frames, or the number
 * of output channels changes (also while audio is paused). Sample format
 * changes are handled in the audio callback directly.
 */
boolean audio_mixer_init(unsigned rate, unsigned frames, unsigned channels)
{
  boolean ret = false;
  size_t sz;

  debug("--MIXER-- attempting init with rate=%u, frames=%u, channels=%u\n",
   rate, frames, channels);

  if(rate == 0)
    rate = 44100;
  if(rate < 256 || rate > 384000)
    goto err;

  if(frames == 0)
    frames = 1024;
  frames = CLAMP(frames, 8, 65536);
  frames = round_to_power_of_two(frames);

  if(channels == 0)
    channels = 2;
  if(channels > 2)
    goto err;

  sz = sizeof(int32_t) * frames * channels;

  LOCK();

  if(!audio.mix_buffer || audio.buffer_bytes != sz)
  {
    int32_t *buffer = (int32_t *)crealloc(audio.mix_buffer, sz);
    if(!buffer)
    {
      debug("--MIXER-- failed buffer alloc of size %zu\n", sz);
      goto err_unlock;
    }

    audio.mix_buffer = buffer;
    audio.buffer_bytes = sz;
  }

  audio.output_frequency = rate;
  audio.buffer_frames = frames;
  audio.buffer_channels = channels;
  ret = true;

  // TODO: if there are any open audio streams, they should sampled_set_buffer
  // since they rely on output_frequency and buffer_frames.

err_unlock:
  UNLOCK();

err:
  debug("--MIXER-- init %s with rate=%u, frames=%u, channels=%u\n",
   ret ? "OK" : "FAILURE", rate, frames, channels);
  return ret;
}

/**
 * Free the software mixer buffer. This does not need to be called
 * in driver exit functions; quit_audio will call it for them.
 */
void audio_mixer_free(void)
{
  free(audio.mix_buffer);
  audio.mix_buffer = NULL;
  audio.buffer_bytes = 0;
  audio.buffer_frames = 0;
  audio.buffer_channels = 0;
  audio.output_frequency = 44100;
}

void init_audio(struct config_info *conf)
{
  platform_mutex_init(&audio.audio_mutex);
  platform_mutex_init(&audio.audio_sfx_mutex);
#ifdef DEBUG
  platform_mutex_init(&audio.audio_debug_mutex);
#endif

  audio.output_frequency = 44100; // Dummy value.
  audio.global_resample_mode = conf->resample_mode;

  audio.max_simultaneous_samples = -1;
  audio.max_simultaneous_samples_config = conf->max_simultaneous_samples;
  audio.pc_speaker_use_hardware = conf->audio_pc_speaker_use_hardware;
  audio.opl_use_hardware = conf->audio_opl_use_hardware;
  audio.opl_port = conf->audio_opl_port;

  audio_set_music_volume(conf->music_volume);
  audio_set_sound_volume(conf->sam_volume);
  audio_set_music_on(conf->music_on);
  audio_set_pcs_on(conf->pc_speaker_on);

  audio.driver = audio_init_driver(conf, conf->audio_driver);

  if(audio.driver)
  {
    /* PC speaker providers are not kept in the regular player list and
     * are handled on a per-driver basis. */
    const struct audio_player *pcs_player = audio.pc_speaker_use_hardware ?
     audio.driver->pcs_hardware_player : audio.driver->pcs_software_player;

    if(!pcs_player)
      pcs_player = audio.driver->pcs_software_player;

    if(!pcs_player)
      pcs_player = audio.driver->pcs_hardware_player;

    if(pcs_player)
      audio.pcs_stream = pcs_player->construct(NULL, NULL, 0, 255, 0);
  }

  audio_set_pcs_volume(conf->pc_speaker_volume);
}

void quit_audio(void)
{
  struct audio_stream *a_src;

  // Signal the audio thread to stop and wait for it to release the lock.
  if(audio.driver)
  {
    audio.driver->quit_audio_driver();
    audio.driver = NULL;
  }

  LOCK();

  audio_sfx_clear_queue();

  a_src = audio.stream_list_base;
  while(a_src)
  {
    struct audio_stream *next = a_src->next;
    a_src->player->destruct(a_src);
    a_src = next;
  }
  audio.stream_list_base = audio.stream_list_end = NULL;
  audio.pcs_stream = NULL;

  audio_garbage_collect();
  audio_mixer_free();

  UNLOCK();

#ifdef DEBUG
  platform_mutex_destroy(&audio.audio_debug_mutex);
#endif
  platform_mutex_destroy(&audio.audio_sfx_mutex);
  platform_mutex_destroy(&audio.audio_mutex);
}

/* If the mod was successfully changed, return 1.  This value is used
*  to determine whether to change real_mod_playing.
*/
int audio_play_module(char *filename, boolean safely, int volume)
{
  char translated_filename[MAX_PATH];
  struct audio_stream *a_src;
  int real_volume;

  if(!filename || !filename[0])
  {
    debug("audio_play_module received empty filename!\n");
    return 0;
  }

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS &&
     audio_legacy_translate(filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS)
    {
      debug("Module filename '%s' failed safety checks\n", filename);
      return 0;
    }

    filename = translated_filename;
  }

  audio_end_module();

  real_volume = volume_function(volume, audio.music_volume);
  a_src = audio_construct_stream(filename, 0, real_volume, true, true);

  LOCK();

  audio.primary_stream = a_src;

  UNLOCK();
  return 1;
}

void audio_end_module(void)
{
  if(audio.primary_stream)
  {
    struct audio_stream *current_astream;

    LOCK();

    // Ensure that this didn't change while waiting for the lock.
    if(audio.primary_stream)
    {
      audio.primary_stream->player->destruct(audio.primary_stream);
      audio.primary_stream = NULL;
    }

    // Also end any sound effects attached to the mod.
    current_astream = audio.stream_list_base;
    while(current_astream)
    {
      struct audio_stream *next_astream = current_astream->next;

      if(current_astream->is_spot_sample)
        current_astream->player->destruct(current_astream);

      current_astream = next_astream;
    }
    audio_garbage_collect();

    UNLOCK();
  }
}

void audio_set_max_samples(int max_samples)
{
  // -1 is unlimited
  int max_samples_config = audio.max_simultaneous_samples_config;

  if(max_samples_config >= 0)
  {
    if((max_samples_config < max_samples) || (max_samples < 0))
      max_samples = max_samples_config;
  }

  audio.max_simultaneous_samples = max_samples;
}

int audio_get_max_samples(void)
{
  return audio.max_simultaneous_samples;
}

static void limit_samples(int max)
{
  struct audio_stream *current_astream;
  struct audio_stream *prev_astream;
  int samples_playing = 0;

  // Don't limit samples if the max samples setting is -1.
  if(max < 0)
    return;

  LOCK();

  /* The most recent audio streams are at the end of the list.
   * For efficiency, walk the list in reverse. */
  current_astream = audio.stream_list_end;
  while(current_astream)
  {
    prev_astream = current_astream->previous;

    if(current_astream != audio.primary_stream && current_astream != audio.pcs_stream)
    {
      if(samples_playing >= max)
        current_astream->player->destruct(current_astream);

      samples_playing++;
    }

    current_astream = prev_astream;
  }

  UNLOCK();
}

void audio_play_sample(char *filename, boolean safely, int period)
{
  unsigned int vol = volume_function(255, audio.sound_volume);
  char translated_filename[MAX_PATH];

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS &&
     audio_legacy_translate(filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS)
    {
      debug("Sample filename '%s' failed safety checks\n", filename);
      return;
    }

    filename = translated_filename;
  }

  /**
   * NOTE: the period is doubled here to compensate for a SAM to WAV
   * conversion bug introduced in MZX 2.80. Unfortunately this has
   * permanently affected the way the frequency field has been used for
   * WAV, Ogg, and any modules used here and needs to be carried forward.
   *
   * Note that WAVs generated from the buggy SAM to WAV conversion routine
   * are treated as stereo and must also have this buggy doubling. In other
   * words, just double the frequency in the SAM loader.
   */
  audio_construct_stream(filename,
   audio_get_real_frequency(period * 2), vol, false, false);

  limit_samples(audio.max_simultaneous_samples);
}

void audio_spot_sample(int period, int which)
{
  // Play a sample from the current playing mod.
  // Currently only works with libxmp (and maybe only ever will).

  unsigned int vol = volume_function(255, audio.sound_volume);
  struct wav_info wav;
  boolean ret = false;

  memset(&wav, 0, sizeof(struct wav_info));

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_sample)
    ret = audio.primary_stream->player->get_sample(audio.primary_stream, which, &wav);

  UNLOCK();

  if(ret)
  {
    /**
     * NOTE: see above for why the period is being multiplied by 2 here.
     * The player implementation of get_sample should enable the sam frequency
     * hack to compensate for this.
     */
    struct audio_stream *a_src = construct_wav_stream_direct(&wav,
     audio_get_real_frequency(period * 2), vol, !!(wav.loop_end));
    a_src->is_spot_sample = true;

    limit_samples(audio.max_simultaneous_samples);
  }
}

void audio_end_sample(void)
{
  struct audio_stream *current_astream;
  struct audio_stream *next_astream;

  LOCK();

  current_astream = audio.stream_list_base;
  while(current_astream)
  {
    next_astream = current_astream->next;

    if(current_astream != audio.primary_stream && current_astream != audio.pcs_stream)
      current_astream->player->destruct(current_astream);

    current_astream = next_astream;
  }
  audio_garbage_collect();

  UNLOCK();
}

/**
 * Functions to modify the current primary stream.
 * Keep these functions in the same order as in struct audio_stream.
 */

void audio_set_module_volume(int volume)
{
  int real_volume = volume_function(volume, audio.music_volume);

  LOCK();

  if(audio.primary_stream)
    audio.primary_stream->player->set_volume(audio.primary_stream, real_volume);

  UNLOCK();
}

void audio_set_module_order(int order)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->set_order)
    audio.primary_stream->player->set_order(audio.primary_stream, order);

  UNLOCK();
}

int audio_get_module_order(void)
{
  int order = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_order)
    order = audio.primary_stream->player->get_order(audio.primary_stream);

  UNLOCK();

  return order;
}

void audio_set_module_position(int pos)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->set_position)
    audio.primary_stream->player->set_position(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_position(void)
{
  int pos = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_position)
    pos = audio.primary_stream->player->get_position(audio.primary_stream);

  UNLOCK();

  return pos;
}

int audio_get_module_length(void)
{
  int length = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_length)
    length = audio.primary_stream->player->get_length(audio.primary_stream);

  UNLOCK();

  return length;
}

void audio_set_module_loop_start(int pos)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->set_loop_start)
    audio.primary_stream->player->set_loop_start(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_loop_start(void)
{
  int loop_start = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_loop_start)
    loop_start = audio.primary_stream->player->get_loop_start(audio.primary_stream);

  UNLOCK();

  return loop_start;
}

void audio_set_module_loop_end(int pos)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->set_loop_end)
    audio.primary_stream->player->set_loop_end(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_loop_end(void)
{
  int loop_end = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_loop_end)
    loop_end = audio.primary_stream->player->get_loop_end(audio.primary_stream);

  UNLOCK();

  return loop_end;
}

void audio_set_module_frequency(int freq)
{
  if(freq < 16 || freq > (2 << 20))
    return;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->set_frequency)
  {
    struct sampled_stream *s = (struct sampled_stream *)audio.primary_stream;
    audio.primary_stream->player->set_frequency(s, freq);
  }

  UNLOCK();
}

int audio_get_module_frequency(void)
{
  int freq = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->player->get_frequency)
  {
    struct sampled_stream *s = (struct sampled_stream *)audio.primary_stream;
    freq = audio.primary_stream->player->get_frequency(s);
  }

  UNLOCK();

  return freq;
}

#endif

/**
 * Functions to modify the global audio settings.
 *
 * The volumes are only used by the main thread and don't need read locks.
 */

void audio_set_music_on(int val)
{
  LOCK();
  audio.music_on = val;
  UNLOCK();
}

void audio_set_pcs_on(int val)
{
  LOCK();
  audio.pcs_on = val;
  UNLOCK();
}

int audio_get_music_on(void)
{
  /* TODO: locking would be ideal here */
  return audio.music_on;
}

int audio_get_pcs_on(void)
{
  /* TODO: locking would be ideal here */
  return audio.pcs_on;
}

int audio_get_music_volume(void)
{
  return audio.music_volume;
}

int audio_get_sound_volume(void)
{
  return audio.sound_volume;
}

int audio_get_pcs_volume(void)
{
  return audio.pcs_volume;
}

void audio_set_music_volume(int volume)
{
  LOCK();
  audio.music_volume = volume;
  UNLOCK();
}

void audio_set_sound_volume(int volume)
{
  struct audio_stream *current_astream;
  int real_volume;

  LOCK();

  audio.sound_volume = volume;
  real_volume = volume_function(255, audio.sound_volume);

  current_astream = audio.stream_list_base;
  while(current_astream)
  {
    if(current_astream != audio.primary_stream && current_astream != audio.pcs_stream)
      current_astream->player->set_volume(current_astream, real_volume);

    current_astream = current_astream->next;
  }

  UNLOCK();
}

void audio_set_pcs_volume(int volume)
{
  int real_volume;

  LOCK();

  audio.pcs_volume = volume;
  real_volume = volume_function(255, audio.pcs_volume);

  if(audio.pcs_stream)
    audio.pcs_stream->player->set_volume(audio.pcs_stream, real_volume);

  UNLOCK();
}

/**
 * Wrapper for fsafetranslate. Prior to 2.83, the audio code would apply
 * special translations to certain filenames BEFORE the fsafetranslate step
 * that is now in audio_play_module and audio_play_sample; due to this change,
 * certain quirks of the old engine relied on by some games stopped working:
 *
 * + Requests to play files with the .SAM extension would first try to play a
 *   .WAV with the same name even if the .SAM didn't exist at all.
 * + Requests to play files with the .GDM extension would first try to play an
 *   .S3M with the same name even if the .GDM didn't exist at all.
 *
 * This function provides a compatibility layer for this old behavior; use
 * after the initial fsafetranslate fails.
 */
int audio_legacy_translate(const char *path, char *newpath, size_t buffer_len)
{
  char temp[MAX_PATH];
  ssize_t ext_pos = strlen(path) - 4;

  if(ext_pos >= 0 && (size_t)(ext_pos + 4) < buffer_len)
  {
    if(!strcasecmp(path + ext_pos, ".SAM"))
    {
      strcpy(temp, path);
      strcpy(temp + ext_pos, ".WAV");
      return fsafetranslate(temp, newpath, buffer_len);
    }
    else

    if(!strcasecmp(path + ext_pos, ".GDM"))
    {
      strcpy(temp, path);
      strcpy(temp + ext_pos, ".S3M");
      return fsafetranslate(temp, newpath, buffer_len);
    }
  }
  return -FSAFE_MATCH_FAILED;
}
