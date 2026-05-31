/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef MEGAZEUX_AUDIO_EXT_H
#define MEGAZEUX_AUDIO_EXT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "audio.h"
#include "../io/vfile.h"

typedef boolean (*filter_stream_fn)(vfile *vf, const char *filename);
typedef struct audio_stream *(*construct_stream_fn)(vfile *vf, const char *,
 uint32_t frequency, unsigned int volume, boolean repeat);

void audio_ext_register(filter_stream_fn test, construct_stream_fn constructor);
void audio_ext_free_registry(void);

struct audio_stream *audio_ext_construct_stream(const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_EXT_H */
