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

#ifndef MEGAZEUX_AUDIO_SFX_H
#define MEGAZEUX_AUDIO_SFX_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

#define AUDIO_SFX_FREQ_REST 1

void audio_sfx_queue_sound(int freq, int duration);
boolean audio_sfx_get_next_sound(int *freq, int *duration);
void audio_sfx_clear_queue(void);
int audio_sfx_get_num_queued_sounds(void);
boolean audio_sfx_has_queued_sounds(void);
boolean audio_sfx_should_cancel_note(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_SFX_H */
