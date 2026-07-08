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

#ifndef MEGAZEUX_AUDIO_WAV_LOAD_H
#define MEGAZEUX_AUDIO_WAV_LOAD_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "audio_struct.h"

boolean audio_load_wav(struct wav_info *dest, vfile *vf, const char *filename);
boolean audio_load_sam(struct wav_info *dest, vfile *vf, const char *filename);
void audio_free_wav(struct wav_info *src);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_WAV_LOAD_H */
