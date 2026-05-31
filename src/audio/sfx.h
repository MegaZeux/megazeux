/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_SFX_H
#define MEGAZEUX_SFX_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

// Number of unique sound effects and length of sound effects
// SFX sizes include the \0 terminator.
#define MAX_SFX_SIZE    256
#define LEGACY_SFX_SIZE 69

enum sfx_id
{
  SFX_GEM               = 0,
  SFX_MAGIC_GEM         = 1,
  SFX_HEALTH            = 2,
  SFX_AMMO              = 3,
  SFX_COIN              = 4,
  SFX_LIFE              = 5,
  SFX_LO_BOMB           = 6,
  SFX_HI_BOMB           = 7,
  SFX_KEY               = 8,
  SFX_FULL_KEYS         = 9,
  SFX_UNLOCK            = 10,
  SFX_CANT_UNLOCK       = 11,
  SFX_INVIS_WALL        = 12,
  SFX_FOREST            = 13,
  SFX_GATE_LOCKED       = 14,
  SFX_OPEN_GATE         = 15,
  SFX_INVINCO_START     = 16,
  SFX_INVINCO_BEAT      = 17,
  SFX_INVINCO_END       = 18,
  SFX_DOOR_LOCKED       = 19,
  SFX_OPEN_DOOR         = 20,
  SFX_HURT              = 21,
  SFX_LAVA              = 22,
  SFX_DEATH             = 23,
  SFX_GAME_OVER         = 24,
  SFX_GATE_CLOSE        = 25,
  SFX_PUSH              = 26,
  SFX_TRANSPORT         = 27,
  SFX_SHOOT             = 28,
  SFX_BREAK             = 29,
  SFX_OUT_OF_AMMO       = 30,
  SFX_RICOCHET          = 31,
  SFX_OUT_OF_BOMBS      = 32,
  SFX_PLACE_LO_BOMB     = 33,
  SFX_PLACE_HI_BOMB     = 34,
  SFX_SWITCH_BOMB_TYPE  = 35,
  SFX_EXPLOSION         = 36,
  SFX_ENTRANCE          = 37,
  SFX_POUCH             = 38,
  SFX_RING_POTION       = 39,
  SFX_EMPTY_CHEST       = 40,
  SFX_CHEST             = 41,
  SFX_OUT_OF_TIME       = 42,
  SFX_FIRE_HURT         = 43,
  SFX_STOLEN_GEM        = 44,
  SFX_ENEMY_HP_DOWN     = 45,
  SFX_DRAGON_FIRE       = 46,
  SFX_SCROLL_SIGN       = 47,
  SFX_GOOP              = 48,
  SFX_UNUSED_49         = 49,
  NUM_BUILTIN_SFX       = 50,
  MAX_NUM_SFX           = 256
};

struct custom_sfx
{
  char label[12];
  char string[1]; // Flexible array member
};

struct sfx_list
{
  struct custom_sfx **list;
  int num_alloc;
};

// Requires struct custom_sfx, so don't include.
struct world;

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern char sfx_strs[NUM_BUILTIN_SFX][LEGACY_SFX_SIZE];

CORE_LIBSPEC size_t sfx_ram_usage(const struct sfx_list *sfx_list);
#endif // CONFIG_EDITOR

#ifdef CONFIG_AUDIO

// Called by audio_pcs.c under lock.
void sfx_next_note(int *is_playing, int *freq, int *duration);
boolean sfx_should_cancel_note(void);

void play_sfx(struct world *mzx_world, int sfx);
void play_string(char *str, int sfx_play);
void sfx_clear_queue(void);
boolean sfx_is_playing(void);
int sfx_length_left(void);

#else // !CONFIG_AUDIO

static inline void play_sfx(struct world *mzx_world, int sfxn) {}
static inline void play_string(char *str, int sfx_play) {}
static inline void sfx_clear_queue(void) {}
static inline boolean sfx_is_playing(void) { return false; }
static inline int sfx_length_left(void) { return 0; }

#endif // CONFIG_AUDIO

CORE_LIBSPEC const struct custom_sfx *sfx_get(
 const struct sfx_list *sfx_list, int num);
CORE_LIBSPEC boolean sfx_set_string(struct sfx_list *sfx_list, int num,
 const char *src, size_t src_len);
CORE_LIBSPEC boolean sfx_set_label(struct sfx_list *sfx_list, int num,
 const char *src, size_t src_len);
CORE_LIBSPEC void sfx_unset(struct sfx_list *sfx_list, int num);
CORE_LIBSPEC void sfx_free(struct sfx_list *sfx_list);
CORE_LIBSPEC size_t sfx_save_to_memory(const struct sfx_list *sfx_list,
 int file_version, char *dest, size_t dest_len, size_t *required);
CORE_LIBSPEC boolean sfx_load_from_memory(struct sfx_list *sfx_list,
 int file_version, const char *src, size_t src_len);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_SFX_H */
