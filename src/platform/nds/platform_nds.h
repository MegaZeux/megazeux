/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __3DS_PLATFORM_H__
#define __3DS_PLATFORM_H__

#include "../../compat.h"

#include <nds.h>

// #define BUILD_PROFILING

#ifdef BUILD_PROFILING
void profile_start(const char *name);
void profile_end(void);
#else
#define profile_start(name) {}
#define profile_end() {}
#endif

// FIFO definitions.
#define FIFO_MZX FIFO_USER_01
#define CMD_MZX_PCS_TONE 0x01
#define CMD_MZX_SOUND_VOLUME 0x02
#define CMD_MZX_MM_GET_POSITION 0x03

#ifdef CONFIG_AUDIO
void nds_audio_vblank(void);
#endif

#endif /* __3DS_PLATFORM_H__ */
