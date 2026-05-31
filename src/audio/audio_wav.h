/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_AUDIO_WAV_H
#define MEGAZEUX_AUDIO_WAV_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

// For use by audio_spot_sample.
struct audio_stream *construct_wav_stream_direct(struct wav_info *w_info,
 uint32_t frequency, unsigned int volume, boolean repeat);

void init_wav(struct config_info *conf);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_WAV_H */
