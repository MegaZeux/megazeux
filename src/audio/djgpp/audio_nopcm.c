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

#include "../audio_drivers.h"
#include "../audio_players.h"

static boolean init_audio_driver_no_pcm(struct config_info *conf)
{
  return true;
}

static void quit_audio_driver_no_pcm(void)
{
  // nop
}

const struct audio_driver audio_driver_no_pcm =
{
  "PC Speaker SFX only",
  "nopcm",
  true,

  NULL,
  NULL,
  &audio_player_pcs_hw,

  init_audio_driver_no_pcm,
  quit_audio_driver_no_pcm,
};
