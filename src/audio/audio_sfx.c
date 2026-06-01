/* MegaZeux
 *
 * Copyright (C) 1996  Alexis Janson
 * Copyright (C) 2004  Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018-2026  Alice Rowan <petrifiedrowan@gmail.com>
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
#include "audio_sfx.h"
#include "audio_struct.h"
#include "../util.h"

/**
 * In a parallel environment circular buffers are prone to very subtle bugs
 * without the use of atomics or locks. While normally that wouldn't cause
 * serious issues here, to make things worse both the main thread and audio
 * thread can cancel the entire SFX queue arbitrarily (and were previously
 * modifying the other thread's queue index where this occurred!).
 *
 * Just to be sure this isn't a problem going forward, the queue is now
 * protected with a mutex. The locked functions are very small so this
 * shouldn't cause much of a performance issue.
 */
#ifndef DEBUG

#define SFX_LOCK()   platform_mutex_lock(&audio.audio_sfx_mutex)
#define SFX_UNLOCK() platform_mutex_unlock(&audio.audio_sfx_mutex)

#else /* DEBUG */

#include "../platform/thread_debug.h"

static platform_mutex_debug mutex_debug;

#define SFX_LOCK()   platform_mutex_lock_debug(&audio.audio_sfx_mutex, \
                      &audio.audio_debug_mutex, &mutex_debug, __FILE__, __LINE__)
#define SFX_UNLOCK() platform_mutex_unlock_debug(&audio.audio_sfx_mutex, \
                      &audio.audio_debug_mutex, &mutex_debug, __FILE__, __LINE__)

#endif /* DEBUG */

static int sfx_queue_diff(void)
{
  return audio.sfx.tail_index - audio.sfx.head_index;
}

void audio_sfx_queue_sound(int freq, int duration)
{
  struct audio_sfx_note note = { CLAMP(duration, 0, UINT16_MAX), freq };
  int next_index;

  SFX_LOCK();

  next_index = (audio.sfx.tail_index + 1) & (AUDIO_SFX_QUEUE_SIZE - 1);
  if(next_index != audio.sfx.head_index)
  {
    audio.sfx.queue[next_index] = note;
    audio.sfx.tail_index = next_index;
  }

  SFX_UNLOCK();
}

/* Returns true if an active note (not a rest) is playing or false if
 * there is an active rest, no note in the queue, or if SFX are disabled.
 * dest is set to the next note/rest (or a duration 1 rest if there is
 * no note, or SFX is disabled).
 *
 * NOTE: DOS versions had a nosound() call for rests and an empty queue,
 * but not for when SFX is disabled. This function is vastly simplified
 * but should be equivalent to the original behavior otherwise.
 */
boolean audio_sfx_get_next_sound(int *freq, int *duration)
{
  struct audio_sfx_note note = { 1, AUDIO_SFX_FREQ_REST };
  int sfx_on = audio_get_pcs_on();

  SFX_LOCK();

  audio.sfx.cancel_current_note = false;
  if(sfx_queue_diff())
  {
    if(sfx_on)
      note = audio.sfx.queue[audio.sfx.head_index];

    audio.sfx.head_index = (audio.sfx.head_index + 1) & (AUDIO_SFX_QUEUE_SIZE - 1);
  }

  SFX_UNLOCK();

  *freq = note.freq;
  *duration = note.duration;
  return (sfx_on && note.freq != AUDIO_SFX_FREQ_REST);
}

void audio_sfx_clear_queue(void)
{
  SFX_LOCK();

  /**
   * In DOS versions, clearing the SFX queue would also stop the current
   * note. The best that can be done here is to alert the PC speaker stream
   * to cancel the current playing sound the next time pcs_mix_data runs.
   */
  audio.sfx.cancel_current_note = true;
  audio.sfx.head_index = 0;
  audio.sfx.tail_index = 0;

  SFX_UNLOCK();
}

int audio_sfx_get_num_queued_sounds(void)
{
  int diff;

  SFX_LOCK();

  diff = sfx_queue_diff();

  SFX_UNLOCK();

  if(diff < 0)
    diff += AUDIO_SFX_QUEUE_SIZE;

  return diff;
}

boolean audio_sfx_has_queued_sounds(void)
{
  int diff;

  SFX_LOCK();

  diff = sfx_queue_diff();

  SFX_UNLOCK();

  return diff != 0;
}

boolean audio_sfx_should_cancel_note(void)
{
  boolean ret;

  SFX_LOCK();

  ret = audio.sfx.cancel_current_note;

  SFX_UNLOCK();

  return ret;
}
