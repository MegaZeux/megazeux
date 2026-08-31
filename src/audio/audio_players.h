/* MegaZeux
 *
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

#ifndef MEGAZEUX_AUDIO_PLAYERS_H
#define MEGAZEUX_AUDIO_PLAYERS_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "audio_struct.h"

extern const struct audio_player audio_player_pcs;
extern const struct audio_player audio_player_sam;
extern const struct audio_player audio_player_wav;
extern const struct audio_player audio_player_vorbis;
extern const struct audio_player audio_player_reality;

extern const struct audio_player audio_player_xmp;
extern const struct audio_player audio_player_modplug;
extern const struct audio_player audio_player_mikmod;
extern const struct audio_player audio_player_openmpt;

extern const struct audio_player audio_player_pcs_hw; /* DOS */

struct audio_stream *audio_construct_stream(const char *filename,
 uint32_t frequency, unsigned int volume, boolean is_primary, boolean repeat);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_PLAYERS_H */
