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

#ifndef MEGAZEUX_AUDIO_DRIVERS_H
#define MEGAZEUX_AUDIO_DRIVERS_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "audio_struct.h"

extern const struct audio_driver audio_driver_sdl;
extern const struct audio_driver audio_driver_sdl3;

extern const struct audio_driver audio_driver_sb;
extern const struct audio_driver audio_driver_lpt_mono;
extern const struct audio_driver audio_driver_lpt_stereo1;
extern const struct audio_driver audio_driver_lpt_stereo2;
extern const struct audio_driver audio_driver_lpt_dss;

extern const struct audio_driver audio_driver_nds;
extern const struct audio_driver audio_driver_3ds;
extern const struct audio_driver audio_driver_wii;
extern const struct audio_driver audio_driver_dreamcast;

const struct audio_driver *audio_init_driver(struct config_info *conf,
 const char *name);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_DRIVERS_H */
