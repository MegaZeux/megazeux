/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../Unit.hpp"
#include "../../src/audio/audio_struct.h"
#include "../../src/audio/audio_sfx.h"

/* The DOS timer and PCS interrupt relies on a lot of hardcoded values.
 * If these offsets are changed, fix interrupt.S! */
UNITTEST(audio_sfx_data)
{
  ASSERTEQ(AUDIO_SFX_QUEUE_SIZE, 4096, "queue array size wrong");
  ASSERTEQ(AUDIO_SFX_FREQ_REST, 1, "rest frequency wrong");

  ASSERTEQ(sizeof(struct audio_sfx_note), 4, "note size wrong");
  ASSERTEQ(offsetof(struct audio_sfx_note, duration), 0, "duration offset wrong");
  ASSERTEQ(offsetof(struct audio_sfx_note, freq), 2, "freq offset wrong");

  ASSERTEQ(sizeof(struct audio_sfx_data), 16396, "sfx data size wrong");
  ASSERTEQ(offsetof(struct audio_sfx_data, queue), 0, "queue offset wrong");
  ASSERTEQ(offsetof(struct audio_sfx_data, head_index), 16384, "head offset wrong");
  ASSERTEQ(offsetof(struct audio_sfx_data, tail_index), 16388, "tail offset wrong");
  ASSERTEQ(offsetof(struct audio_sfx_data, cancel_current_note), 16392, "cancel offset wrong");
}
