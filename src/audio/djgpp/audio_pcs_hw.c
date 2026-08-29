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

#include "../audio.h"
#include "../audio_players.h"
#include "../../platform/djgpp/interrupt.h"

#include <dos.h>

static void pcs_hw_destruct(struct audio_stream *a_src)
{
  disable();

  sfx_ptr = NULL;
  sfx_on_ptr = NULL;

  enable();

  destruct_audio_stream(a_src);
}

static struct audio_stream *pcs_hw_construct(vfile *vf, const char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct audio_stream *a_src =
   (struct audio_stream *)ccalloc(1, sizeof(struct audio_stream));
  if(!a_src)
    return NULL;

  a_src->player = &audio_player_pcs_hw;

  disable();

  sfx_ptr = &audio.sfx;
  sfx_on_ptr = &audio.pcs_on;
  sfx_timer = 0;
  sfx_playing = 0;

  enable();

  initialize_audio_stream(a_src, volume, repeat);
  return a_src;
}

const struct audio_player audio_player_pcs_hw =
{
  "PCS",
  NULL,

  pcs_hw_construct,
  pcs_hw_destruct,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};
