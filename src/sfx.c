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

/* This is the game-facing side of MegaZeux's PC speaker sound effects.
 * See audio/audio_sfx.c for sound effect queue management and
 * audio/audio_pcs.c for the PC speaker rendering code. */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "data.h"
#include "sfx.h"
#include "world.h"
#include "world_format.h"
#include "world_struct.h"
#include "util.h"

#include "audio/audio.h"
#include "audio/audio_sfx.h"

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)

/**
 * The longest built-in SFX is the ring/potion sound, which is 68+1 chars long.
 *
 * This is also the legacy maximum SFX length. Custom SFX may potentially be
 * longer in the future, and/or SFX beyond the 50 built-in SFX may be possible.
 * Aside from potentially repurposing SFX 49, further built-ins are not likely.
 *
 * FIXME: make const
 * TODO: move the queue into the audio system and move the rest of this back to
 * the main source folder, as it is otherwise game behavior and file format.
 */
__editor_maybe_static char sfx_strs[NUM_BUILTIN_SFX][LEGACY_SFX_SIZE] =
{
  "5c-gec-gec", // Gem
  "5c-gec-gec", // Magic Gem
  "cge-zcge", // Health
  "-c+c+c-g-g-g", // Ammo
  "6a#a#zx", // Coin
  "-cegeg+c-g+cecegeg+c", // Life
  "-cc#dd#e", // Lo Bomb
  "cc#dd#e", // Hi Bomb
  "4gec-g+ec-ge+c-gec", // Key
  "-gc#-a#a", // Full Keys
  "ceg+c-eg+ce-g+ceg", // Unlock
  "s1a#z+a#-a#x", // Can't Unlock
  "-a-a+e-e+c-c", // Invis. Wall
  "zax", // Forest
  "0a#+a#-a#b+b-b", // Gate Locked
  "0a+a-a+a-a", // Opening Gate
  "-g+g-g+g+g-g+g-g-g+g-g-tg+g", // Invinco Start
  "sc-cqxsg-g+a#-a#xx+g-g+a#-a#", // Invinco Beat
  "sc-cqxsg-g+f-f+d#-d#+c-ca#-a#2c-c", // Invinco End
  "0g+g-gd#+d#-d#", // 19-Door locked
  "0g+gd#+d#", // Door opening
  "c-zgd#c-gd#c", // Hurt
  "a#e-a#e-a#e-a#e", // AUGH!
  "+c-gd#cd#c-gd#gd#c-g+c-gd#cd#c-gd#gd#c", // Death
  "ic1c+ca#+c1c+cx+d#1c+ca#+c1c+cx"
  "+c1c+ca#+c1c+cx+d#1c+c+fc1c+cx", // Game over
  "0c+c-c+c-c", //25 - Gate closing
  "1d#x", //26-Push
  "2c+c+c2d#+d#+d#2g+g+g3cd#g", // 27-Transport
  "z+c-c",// 28-shoot
  "1a+a+a", // 29-break
  "-af#-f#a", // 30-out of ammo
  "+f#", // 31-ricochet
  "s-d-d-d", // 32-out of bombs
  "c-aec-a", // 33-place bomb (lo)
  "+c-aec-a", // 34-place bomb (hi)
  "+g-ege-ege", // 35-switch bomb type
  "1c+c0d#+d#-g+g-c#", // 36-explosion
  "2cg+e+c2c#g#+f+c#2da+f#+d2d#a#+g+d#", // 37-Entrance
  "cge+c-g+ecge+c-g+ec", // 38-Pouch
  "zcd#gd#cd#gd#cd#gd#cd#gd#"
  "cfg#fcfg#fcfg#fcfg#f"
  "cga#gcga#gcga#gcga#g"
  "s+c",//39-ring/potion
  "z-a-a-aa-aa", // 40-Empty chest
  "1c+c-c#+c#-d+d-d#+d#-e+e-ec", // 41-Chest
  "c-gd#c-zd#gd#c", // 42-Out of time
  "zc-d#g-cd#", // 43-Fire ouch
  "cd#g+cd#g", // 44-Stolen gem
  "z1d#+d#+d#", // 45-Enemy HP down
  "z0ca#f+d#cf#", // 46-Dragon fire
  "cg+c-eda+d-f#eb+e-g#", // 47-Scroll/sign
  "1c+c+c+c+c", // 48-Goop
  "" // 49-Unused
};

#endif // CONFIG_AUDIO || CONFIG_EDITOR

#ifdef CONFIG_AUDIO

// Frequencies of 6C thru 6B
static const int note_freq[12] =
 { 2032, 2152, 2280, 2416, 2560, 2712, 2880, 3048, 3232, 3424, 3624, 3840 };

// Sample frequencies of 0C thru 0B
static const int sam_freq[12] =
 { 3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812 };

#ifdef CONFIG_DEBYTECODE
static const char open_char  = '(';
static const char close_char = ')';
#else
static const char open_char  = '&';
static const char close_char = '&';
#endif

static void play_note(int note, int octave, int delay)
{
  // Note is # 1-12
  // Octave #0-6
  audio_sfx_queue_sound(note_freq[note - 1] >> (6 - octave), delay);
}

void play_sfx(struct world *mzx_world, int sfxn)
{
  // TODO: pass the sfx_list here directly?
  struct sfx_list *sfx_list = &mzx_world->custom_sfx;
  if(sfx_list->list)
  {
    const struct custom_sfx *sfx = sfx_get(sfx_list, sfxn);
    if(sfx)
      play_string((char *)sfx->string, 1);
  }
  else

  if(sfxn >= 0 && sfxn < NUM_BUILTIN_SFX)
  {
    play_string(sfx_strs[sfxn], 1);
  }
}

// sfx_play = 1 to NOT play non-digi unless queue empty

void play_string(char *str, int sfx_play)
{
  int t1, oct = 3, note = 1, dur = 18, t2, last_note = -1,
   digi_st = -1, digi_end = -1, digi_played = 0;
  char chr;
  // Note trans. table from 1-7 (a-z) to 1-12
  char nn[7] = { 10, 12, 1, 3, 5, 6, 8 };

  /**
   * There is a bug in the SFX code where every duration is unconditionally
   * increased by 1 dating back to 2.51 or earlier. For compatibility, this
   * is emulated, but at some point in the future it might be nice to have a
   * world option to use the correct durations.
   */
  static const int extra_duration = 1;

  if(!audio_get_pcs_on())
  {
    sfx_play = 1; // SFX off
  }
  else
  {
    if(!audio_sfx_has_queued_sounds())
      sfx_play = 0;
  }

  // Now, if sfx_play, only play digi
  for(t1 = 0; (unsigned int)t1 < strlen((char *)str); t1++)
  {
    chr = str[t1];
    if((chr >= 'a') && (chr <= 'z'))
      chr -= 32;

    if(chr == '-')
    {
      // Octave down
      if(oct > 0) oct--;
      continue;
    }
    if(chr == '+')
    {
      // Octave up
      if(oct < 6) oct++;
      continue;
    }
    if(chr == 'X')
    {
      // Rest
      if(!sfx_play && !(digi_st > 0))
        audio_sfx_queue_sound(AUDIO_SFX_FREQ_REST, dur + extra_duration);

      continue;
    }
    if((chr > 47) && (chr < 55))
      oct = chr - 48; // Set octave

    if(chr == '.')
      dur += (dur >> 1); // Dot (duration 150%)

    if(chr == '!')
      dur /= 3; // Triplet (cut duration to 33%)

    if((chr > 64) && (chr < 72))
    {
      // Note
      note = nn[chr - 65]; // Convert to 1-12
      t2 = oct; // Save old octave in case # or $ changes it
      if(str[t1 + 1] == '#')
      {
        // Increase if sharp
        t1++;
        if(++note == 13)
        {
          // "B#" := "+C-"
          note = 1;
          if(++oct == 7) oct = 6;
        }
      }
      else

      if(str[t1 + 1] == '$')
      {
        // Decrease if flat
        t1++;
        if(--note == 0)
        {
          // "C$" := "-B+"
          note = 12;
          if(--oct == -1) oct = 0;
        }
      }
      // Digi
      if(digi_st > 0)
      {
        if(audio_get_music_on())
        {
          str[digi_end] = 0;

          audio_play_sample(str + digi_st, true, sam_freq[note - 1] >> oct);

          str[digi_end] = close_char;
          digi_played = 1;
          continue;
        }
      }
      else

      if(sfx_play)
      {
        continue;
      }
      else

      if(last_note == note)
      {
        if(dur <= 9)
        {
          audio_sfx_queue_sound(AUDIO_SFX_FREQ_REST, 1 + extra_duration);
          play_note(note, oct, dur - 1 + extra_duration);
        }
        else
        {
          audio_sfx_queue_sound(AUDIO_SFX_FREQ_REST, 2 + extra_duration);
          play_note(note, oct, dur - 2 + extra_duration);
        }
      }
      else
      {
        play_note(note, oct, dur + extra_duration);
        last_note = note;
      }
      oct = t2; // Restore old octave
    }

    switch(chr)
    {
      case 'Z':
        dur = 9;
        break;

      case 'T':
        dur = 18;
        break;

      case 'S':
        dur = 36;
        break;

      case 'I':
        dur = 72;
        break;

      case 'Q':
        dur = 144;
        break;

      case 'H':
        dur = 288;
        break;

      case 'W':
        dur = 576;
        break;
    }

    if(chr == open_char)
    {
      // Digitized
      t1++;
      digi_st = t1;

      if(str[t1] == 0)
        break;

      do
      {
        t1++;
        if(str[t1] == 0)
          break;

        if(str[t1] == close_char)
          break;

      } while(1);

      digi_end = t1;
      digi_played = 0;
    }

    if(chr == '_')
    {
      if(audio_get_music_on())
        break;

      digi_st = -1;
    }
  }

  // Pending digital music?
  if((digi_st > 0) && (digi_played == 0))
  {
    str[digi_end] = 0;

    audio_play_sample(str + digi_st, true, 0);

    str[digi_end] = close_char;
  }
}

void sfx_clear_queue(void)
{
  audio_sfx_clear_queue();
}

boolean sfx_is_playing(void)
{
  return audio_sfx_has_queued_sounds();
}

int sfx_length_left(void)
{
  return audio_sfx_get_num_queued_sounds();
}

#endif // CONFIG_AUDIO

#ifdef CONFIG_DEBYTECODE

static void convert_sfx_strs(char *sfx_buf)
{
  char *start, *end = sfx_buf - 1, str_buf_len = strlen(sfx_buf);

  while(true)
  {
    // no starting & was found
    start = strchr(end + 1, '&');
    if(!start || start - sfx_buf + 1 > str_buf_len)
      break;

    // no ending & was found
    end = strchr(start + 1, '&');
    if(!end || end - sfx_buf + 1 > str_buf_len)
      break;

    // Wipe out the &s to get a token
    *start = '(';
    *end = ')';
  }
}

static void legacy_convert_sfx(struct sfx_list *sfx_list)
{
  int i;
  for(i = 0; i < sfx_list->num_alloc; i++)
  {
    const struct custom_sfx *sfx = sfx_get(sfx_list, i);
    if(sfx)
      convert_sfx_strs((char *)sfx->string);
  }
}

#endif /* CONFIG_DEBYTECODE */

static size_t sfx_alloc_size(size_t src_len)
{
  return MAX(sizeof(struct custom_sfx),
   offsetof(struct custom_sfx, string) + src_len + 1);
}

static boolean sfx_alloc_at_pos(struct sfx_list *sfx_list, int num,
 size_t src_len)
{
  struct custom_sfx **tmp;
  struct custom_sfx *tmp2;
  size_t sz;

  if(!sfx_list->list || num >= sfx_list->num_alloc)
  {
    int num_alloc = 32;
    int i;

    for(i = 0; i < 16 && num >= num_alloc; i++)
      num_alloc <<= 1;

    num_alloc = MAX(num_alloc, NUM_BUILTIN_SFX);
    tmp = (struct custom_sfx **)
     crealloc(sfx_list->list, num_alloc * sizeof(struct custom_sfx *));
    if(!tmp)
      return false;

    memset(tmp + sfx_list->num_alloc, 0,
     sizeof(struct custom_sfx *) * (num_alloc - sfx_list->num_alloc));

    sfx_list->list = (struct custom_sfx **)tmp;
    sfx_list->num_alloc = num_alloc;
  }

  if(sfx_list->list[num] && src_len == 0)
    return true;

  sz = sfx_alloc_size(src_len);
  tmp2 = (struct custom_sfx *)crealloc(sfx_list->list[num], sz);
  if(!tmp2)
    return false;

  if(!sfx_list->list[num])
  {
    tmp2->string[0] = '\0';
    tmp2->label[0] = '\0';
  }

  sfx_list->list[num] = tmp2;
  return true;
}

const struct custom_sfx *sfx_get(const struct sfx_list *sfx_list, int num)
{
  if(num < 0 || num >= sfx_list->num_alloc)
    return NULL;

  return sfx_list->list[num];
}

boolean sfx_set_string(struct sfx_list *sfx_list, int num,
 const char *src, size_t src_len)
{
  struct custom_sfx *sfx;

  if(num < 0 || num >= MAX_NUM_SFX)
    return false;

  sfx = (struct custom_sfx *)sfx_get(sfx_list, num);
  if(src_len == 0 && (!sfx || !sfx->label[0]))
  {
    sfx_unset(sfx_list, num);
    return true;
  }

  if(!sfx_alloc_at_pos(sfx_list, num, src_len))
    return false;

  sfx = sfx_list->list[num];

  src_len = MIN(src_len, MAX_SFX_SIZE - 1);
  memcpy(sfx->string, src, src_len);
  sfx->string[src_len] = '\0';
  return true;
}

boolean sfx_set_label(struct sfx_list *sfx_list, int num,
 const char *src, size_t src_len)
{
  struct custom_sfx *sfx;

  if(num < 0 || num >= MAX_NUM_SFX)
    return false;
  if(!sfx_alloc_at_pos(sfx_list, num, 0))
    return false;

  sfx = sfx_list->list[num];
  src_len = MIN(src_len, sizeof(sfx->label) - 1);
  memcpy(sfx->label, src, src_len);
  sfx->label[src_len] = '\0';
  return true;
}

void sfx_unset(struct sfx_list *custom_sfx, int num)
{
  if(num < 0 || num >= custom_sfx->num_alloc)
    return;

  free(custom_sfx->list[num]);
  custom_sfx->list[num] = NULL;
  return;
}

void sfx_free(struct sfx_list *custom_sfx)
{
  if(custom_sfx->list)
  {
    int i;
    for(i = 0; i < custom_sfx->num_alloc; i++)
      free(custom_sfx->list[i]);

    free(custom_sfx->list);
    custom_sfx->list = NULL;
  }
  custom_sfx->num_alloc = 0;
}

/**
 * Block-SFX style that was used for <=2.92X .SFX files and for worlds
 * saved between 2.90X and 2.92X.
 */
static void save_sfx_array(const struct sfx_list *sfx_list,
 char custom_sfx[NUM_BUILTIN_SFX * LEGACY_SFX_SIZE])
{
  const struct custom_sfx *sfx;
  char *dest;
  size_t i;

  memset(custom_sfx, 0, NUM_BUILTIN_SFX * LEGACY_SFX_SIZE);

  // Ignore everything past the null terminator.
  dest = custom_sfx;
  for(i = 0; i < NUM_BUILTIN_SFX; i++, dest += LEGACY_SFX_SIZE)
  {
    sfx = sfx_get(sfx_list, i);
    if(sfx)
    {
      size_t len = strlen(sfx->string);
      len = MIN(len, LEGACY_SFX_SIZE - 1);
      memcpy(dest, sfx->string, len);
    }
  }
}

static boolean load_sfx_array(struct sfx_list *sfx_list,
 const char custom_sfx[NUM_BUILTIN_SFX * LEGACY_SFX_SIZE])
{
  uint8_t lens[NUM_BUILTIN_SFX];
  const char *src;
  size_t i;
  size_t j;

  src = custom_sfx;
  for(i = 0; i < NUM_BUILTIN_SFX; i++)
  {
    // Must contain a null terminator within the read block.
    // It's possible for a malicious file to contain no terminator.
    for(j = 0; j < LEGACY_SFX_SIZE; j++)
      if(src[j] == '\0')
        break;

    if(j >= LEGACY_SFX_SIZE)
      return false;

    lens[i] = j;
    src += LEGACY_SFX_SIZE;
  }

  src = custom_sfx;
  for(i = 0; i < NUM_BUILTIN_SFX; i++)
  {
    sfx_set_string(sfx_list, i, src, lens[i]);
    src += LEGACY_SFX_SIZE;
  }
  return true;
}

static size_t save_sfx_properties(const struct sfx_list *sfx_list,
 int file_version, char *dest, size_t dest_len)
{
  struct memfile mf;
  const struct custom_sfx *sfx;
  size_t label_len;
  size_t str_len;
  int i;

  // Bounding was already verified in `save_sfx_memory`.
  mfopen_wr(dest, dest_len, &mf);

  for(i = 0; i < sfx_list->num_alloc; i++)
  {
    sfx = sfx_list->list[i];
    if(sfx && (sfx->label[0] || sfx->string[0]))
    {
      label_len = strlen(sfx->label);
      str_len = strlen(sfx->string);
      save_prop_c(SFXPROP_SET_ID, i, &mf);
      if(str_len)
        save_prop_a(SFXPROP_STRING, sfx->string, str_len, 1, &mf);
      if(label_len)
        save_prop_a(SFXPROP_LABEL, sfx->label, label_len, 1, &mf);
    }
  }
  save_prop_eof(&mf);
  return mftell(&mf);
}

static boolean load_sfx_properties(struct sfx_list *sfx_list,
 int file_version, const char *src, size_t src_len)
{
  struct memfile mf;
  struct memfile prop;
  int ident;
  int len;
  unsigned num = 0;

  sfx_free(sfx_list);

  mfopen(src, src_len, &mf);
  if(!check_properties_file(&mf, SFXPROP_LABEL))
    return false;

  while(next_prop(&prop, &ident, &len, &mf))
  {
    switch(ident)
    {
      case SFXPROP_SET_ID:
        num = load_prop_int(&prop);
        break;

      case SFXPROP_STRING:
        sfx_set_string(sfx_list, num, (char *)prop.start, len);
        break;

      case SFXPROP_LABEL:
        sfx_set_label(sfx_list, num, (char *)prop.start, len);
        break;
    }
  }
  return true;
}

/**
 * Save the world custom SFX to a memory buffer for file export.
 *
 * @param mzx_world     world data.
 * @param file_version  MegaZeux version for compatibility.
 *                      `V293`+ writes a properties file with header that can
 *                      be loaded by the specified MZX version and up; lower
 *                      writes the legacy fixed size array format.
 * @param dest          buffer to write SFX to.
 * @param dest_len      size of `dest`.
 * @param required      if non-`NULL`, the minimum buffer size is returned here.
 * @return              number of bytes written to `dest`, or 0 on failure.
 */
size_t sfx_save_to_memory(const struct sfx_list *sfx_list,
 int file_version, char *dest, size_t dest_len, size_t *required)
{
  if(!sfx_list->list)
  {
    if(required)
      *required = 0;
    return 0;
  }

  if(file_version >= V293)
  {
    const struct custom_sfx *sfx;
    size_t size = 0;
    int i;

    for(i = 0; i < sfx_list->num_alloc; i++)
    {
      sfx = sfx_list->list[i];
      if(sfx && (sfx->label[0] || sfx->string[0]))
      {
        size += PROP_HEADER_SIZE + 1;
        if(sfx->label[0])
          size += PROP_HEADER_SIZE + strlen(sfx->label);
        if(sfx->string[0])
          size += PROP_HEADER_SIZE + strlen(sfx->string);
      }
    }
    size += 2 + 8; // eof + header

    if(required)
      *required = size;
    if(dest_len < size || !dest)
      return 0;

    memcpy(dest, "MZFX\x1a", 6);
    dest[6] = file_version >> 8;
    dest[7] = file_version & 0xff;

    return save_sfx_properties(sfx_list, file_version, dest + 8, dest_len - 8) + 8;
  }
  else
  {
    if(required)
      *required = NUM_BUILTIN_SFX * LEGACY_SFX_SIZE;
    if(dest_len < NUM_BUILTIN_SFX * LEGACY_SFX_SIZE || !dest)
      return 0;

    save_sfx_array(sfx_list, dest);
    return NUM_BUILTIN_SFX * LEGACY_SFX_SIZE;
  }
}

/**
 * Import the world custom SFX from a memory buffer.
 *
 * @param mzx_world     world data.
 * @param file_version  MegaZeux version for compatibility.
 *                      `V293`+ allows loading either a legacy file or a
 *                      properties file, in which case the embedded header
 *                      will override this version; lower reads a legacy file.
 * @param src           buffer to read SFX data from.
 * @param src_len       length of `src` in bytes.
 * @return              `true` on success, otherwise `false`. The world data
 *                      will not be modified if `false` is returned.
 */
boolean sfx_load_from_memory(struct sfx_list *sfx_list,
 int file_version, const char *src, size_t src_len)
{
  if(file_version >= V293 && src_len >= 8 && !memcmp(src, "MZFX\x1a", 6))
  {
    // Currently make an attempt at loading any plausible future version file.
    // Due to the nature of this format, a breaking change is unlikely.
    file_version = ((uint8_t)src[6] << 8) | (uint8_t)src[7];
    if(file_version < V293)
      return false;

    if(!load_sfx_properties(sfx_list, file_version, src + 8, src_len - 8))
      return false;
  }
  else
  {
    if(src_len != NUM_BUILTIN_SFX * LEGACY_SFX_SIZE)
      return false;

    if(!load_sfx_array(sfx_list, src))
      return false;
  }

#ifdef CONFIG_DEBYTECODE
  if(file_version < VERSION_SOURCE)
    legacy_convert_sfx(sfx_list);
#endif

  return true;
}

#ifdef CONFIG_EDITOR

size_t sfx_ram_usage(const struct sfx_list *sfx_list)
{
  const struct custom_sfx *sfx;
  size_t total = 0;
  int i;

  if(sfx_list->list)
  {
    total = sfx_list->num_alloc * sizeof(struct custom_sfx *);

    for(i = 0; i < sfx_list->num_alloc; i++)
    {
      sfx = sfx_list->list[i];
      if(sfx)
        total += sfx_alloc_size(strlen(sfx->string));
    }
  }
  return total;
}

#endif /* CONFIG_EDITOR */
