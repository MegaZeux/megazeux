/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "legacy_world.h"
#include "legacy_board.h"
#include "legacy_robot.h"

#include "configure.h"
#include "const.h"
#include "counter.h"
#include "error.h"
#include "extmem.h"
#include "graphics.h"
#include "idput.h"
#include "robot.h"
#include "sfx.h"
#include "sprite.h"
#include "str.h"
#include "window.h"
#include "world.h"
#include "util.h"
#include "io/path.h"
#include "io/vio.h"

#define WORLD_GLOBAL_OFFSET_OFFSET 4230
#define WORLD_V1_BOARD_NUM_OFFSET 4009
#define WORLD_BLOCK_1_SIZE 4129
#define WORLD_BLOCK_2_SIZE 72
#define DECRYPT_BUFFER_SIZE 8192

#ifndef CONFIG_LOADSAVE_METER

static inline void meter_update_screen(int *curr, int target) {}
static inline void meter_restore_screen(void) {}
static inline void meter_initial_draw(int curr, int target, const char *title) {}

#endif //!CONFIG_LOADSAVE_METER

static inline boolean legacy_load_counter(struct world *mzx_world,
 vfile *vf, struct counter_list *counter_list, int index, int file_version)
{
  char name_buffer[ROBOT_MAX_TR];
  int name_length;
  int value;

  if(file_version >= V281)
  {
    value = vfgetd(vf);
    name_length = vfgetd(vf);

    name_buffer[0] = 0;
    if(name_length && !vfread(name_buffer, name_length, 1, vf))
      return false;

    if(file_version >= V284)
    {
      // Stupid legacy hacks
      if(!strncasecmp(name_buffer, "mzx_speed", name_length))
      {
        mzx_world->mzx_speed = value;
        return false;
      }

      if(!strncasecmp(name_buffer, "_____lock_speed", name_length))
      {
        mzx_world->lock_speed = value;
        return false;
      }
    }
  }
  else
  {
    name_length = vfread(name_buffer, 1, 15, vf);
    name_buffer[name_length] = '\0';

    if(file_version == V280)
    {
      // MegaZeux 2.80 used a hacked DOS counter list with dwords.
      value = vfgetd(vf);
    }
    else

    if(file_version >= V200)
    {
      // Counters are signed words in DOS 2.x.
      value = (signed short)vfgetw(vf);
    }
    else
    {
      // Counters are unsigned words in 1.x.
      value = vfgetw(vf);
    }
  }

  load_new_counter(counter_list, index, name_buffer, name_length, value);
  return true;
}

static void legacy_load_counter_list(struct world *mzx_world, vfile *vf,
 int file_version, int extra)
{
  struct counter_list *counter_list;
  int num_counters;
  int i;
  int j;

  num_counters = vfgetd(vf);
  counter_list = &(mzx_world->counter_list);
  counter_list->num_counters = num_counters;
  counter_list->num_counters_allocated = num_counters + extra;
  counter_list->counters = ccalloc(num_counters + extra, sizeof(struct counter *));

  for(i = 0, j = 0; i < num_counters; i++)
  {
    if(!legacy_load_counter(mzx_world, vf, counter_list, j, file_version))
    {
      // Remove 2.84 special counters and counters that failed to load.
      counter_list->num_counters--;
      continue;
    }
    j++;
  }
#ifndef CONFIG_COUNTER_HASH_TABLES
  // Versions without the hash table require these to be sorted at all times.
  sort_counter_list(counter_list);
#endif
}

static inline void legacy_load_string(vfile *vf,
 struct string_list *string_list, int index)
{
  char name_buffer[ROBOT_MAX_TR];
  int name_length = vfgetd(vf);
  int str_length = vfgetd(vf);

  struct string *src_string;

  name_buffer[0] = 0;
  if(name_length && !vfread(name_buffer, name_length, 1, vf))
    return;

  src_string = load_new_string(string_list, index,
   name_buffer, name_length, str_length);

  if(str_length && !vfread(src_string->value, str_length, 1, vf))
    return;
}

static void legacy_load_string_list(struct world *mzx_world, vfile *vf,
 int file_version)
{
  struct string_list *string_list;
  int num_strings;
  int i;

  num_strings = vfgetd(vf);
  string_list = &(mzx_world->string_list);
  string_list->num_strings = num_strings;
  string_list->num_strings_allocated = num_strings;
  string_list->strings = ccalloc(num_strings, sizeof(struct string *));

  for(i = 0; i < num_strings; i++)
    legacy_load_string(vf, string_list, i);

#ifndef CONFIG_COUNTER_HASH_TABLES
  // Versions without the hash table require these to be sorted at all times.
  sort_string_list(string_list);
#endif
}

static const char magic_code[16] =
 "\xE6\x52\xEB\xF2\x6D\x4D\x4A\xB7\x87\xB2\x92\x88\xDE\x91\x24";

#define MAX_PASSWORD_LENGTH 15

static int get_pw_xor_code(char *password, int pro_method)
{
  int work = 85; // Start with 85... (01010101)
  size_t i;
  // Clear pw after first null

  for(i = strlen(password); i < MAX_PASSWORD_LENGTH; i++)
  {
    password[i] = 0;
  }

  for(i = 0; i < MAX_PASSWORD_LENGTH; i++)
  {
    //For each byte, roll once to the left and xor in pw byte if it
    //is an odd character, or add in pw byte if it is an even character.
    work <<= 1;
    if(work > 255)
      work ^= 257; // Wraparound from roll

    if(i & 1)
    {
      work += (signed char)password[i]; // Add (even byte)
      if(work > 255)
        work ^= 257; // Wraparound from add
    }
    else
    {
      work ^= (signed char)password[i]; // XOR (odd byte);
    }
  }
  // To factor in protection method, add it in and roll one last time
  work += pro_method;
  if(work > 255)
    work ^= 257;

  work <<= 1;
  if(work > 255)
    work ^= 257;

  // Can't be 0-
  if(work == 0)
    work = 86; // (01010110)
  // Done!
  return work;
}

struct decrypt_data
{
  vfile *source;
  vfile *dest;
  const char *file_name;
  size_t source_length;
  unsigned int xor_val;
  int xor_w;
  int xor_d;
  char password[MAX_PASSWORD_LENGTH + 1];
  char backup_name[MAX_PATH];
  char buffer[DECRYPT_BUFFER_SIZE];
  int meter_curr;
  int meter_target;
};

static long decrypt_backup(struct decrypt_data *data)
{
  vfile *source = NULL;
  vfile *dest = NULL;
  long ret = -1;
  long file_length;
  long left;
  int meter_target = 1;
  int meter_curr = 0;
  int len;

  meter_initial_draw(meter_curr, meter_target, "Writing backup world...");

  source = vfopen_unsafe(data->file_name, "rb");
  if(!source)
    goto err;

  dest = vfopen_unsafe(data->backup_name, "wb");
  if(!dest)
    goto err;

  file_length = vfilelength(source, false);
  meter_target = file_length;

  left = file_length;
  while(left > 0)
  {
    len = MIN(left, DECRYPT_BUFFER_SIZE);
    left -= len;

    if(!vfread(data->buffer, len, 1, source))
      goto err;

    if(!vfwrite(data->buffer, len, 1, dest))
      goto err;

    meter_curr += len - 1;
    meter_update_screen(&meter_curr, meter_target);
  }

  ret = file_length;

err:
  if(source)
    vfclose(source);
  if(dest)
    vfclose(dest);

  meter_restore_screen();
  return ret;
}

static boolean decrypt_block(struct decrypt_data *data, int block_len)
{
  int len;
  int i;

  while(block_len > 0)
  {
    len = MIN(block_len, DECRYPT_BUFFER_SIZE);
    block_len -= len;

    if(!vfread(data->buffer, len, 1, data->source))
      return false;

    for(i = 0; i < len; i++)
      data->buffer[i] ^= data->xor_val;

    if(!vfwrite(data->buffer, len, 1, data->dest))
      return false;

    data->meter_curr += len - 1;
    meter_update_screen(&data->meter_curr, data->meter_target);
  }
  return true;
}

static boolean decrypt_and_fix_offset(struct decrypt_data *data)
{
  long offset = vfgetd(data->source);
  if(offset == EOF)
    return false;

  offset ^= data->xor_d;

  // Adjust the offset to account for removing the password...
  offset -= MAX_PASSWORD_LENGTH;

  vfputd(offset, data->dest);

  data->meter_curr += 4 - 1;
  meter_update_screen(&data->meter_curr, data->meter_target);
  return true;
}

static boolean decrypt(struct decrypt_data *data)
{
  int pro_method;
  int i;
  int num_boards;
  char *src_ptr;
  char *magic_pos;

  data->meter_target = (data->source_length - MAX_PASSWORD_LENGTH);
  data->meter_curr = 0;
  meter_initial_draw(data->meter_curr, data->meter_target, "Decrypting...");

  if(!vfread(data->buffer, 44, 1, data->source))
    goto err_meter;

  src_ptr = data->buffer + LEGACY_BOARD_NAME_SIZE;
  pro_method = *src_ptr;
  src_ptr++;

  // Get password
  memcpy(data->password, src_ptr, MAX_PASSWORD_LENGTH);
  data->password[MAX_PASSWORD_LENGTH] = '\0';
  // First, normalize password...
  for(i = 0; i < MAX_PASSWORD_LENGTH; i++)
  {
    data->password[i] ^= magic_code[i];
    data->password[i] -= 0x12 + pro_method;
    data->password[i] ^= 0x8D;
  }

  // Xor code
  data->xor_val = get_pw_xor_code(data->password, pro_method);
  data->xor_w = data->xor_val | (data->xor_val << 8);
  data->xor_d = data->xor_val | (data->xor_val << 8) |
   (data->xor_val << 16) | (data->xor_val << 24);

  // Copy title
  if(!vfwrite(data->buffer, LEGACY_BOARD_NAME_SIZE, 1, data->dest))
    goto err_meter;

  // Protection method.
  vfputc(0, data->dest);

  // Copy magic.
  magic_pos = data->buffer + LEGACY_BOARD_NAME_SIZE + 1 + MAX_PASSWORD_LENGTH;
  if(!vfwrite(magic_pos, 3, 1, data->dest))
    goto err_meter;

  data->meter_curr += LEGACY_BOARD_NAME_SIZE + 1 + 3 - 1;
  meter_update_screen(&data->meter_curr, data->meter_target);

  // Decrypt world data.
  if(!decrypt_block(data, WORLD_BLOCK_1_SIZE + WORLD_BLOCK_2_SIZE))
    goto err_meter;

  // Decrypt and fix global robot offset.
  if(!decrypt_and_fix_offset(data))
    goto err_meter;

  // Decrypt SFX table (if present).
  num_boards = vfgetc(data->source) ^ data->xor_val;
  vfputc(num_boards, data->dest);
  data->meter_curr++;
  if(!num_boards)
  {
    int sfx_length = vfgetw(data->source) ^ data->xor_w;
    vfputw(sfx_length, data->dest);
    data->meter_curr += 2;

    if(!decrypt_block(data, sfx_length))
      goto err_meter;

    num_boards = vfgetc(data->source) ^ data->xor_val;
    vfputc(num_boards, data->dest);
    data->meter_curr++;
  }

  // Decrypt board titles.
  if(!decrypt_block(data, LEGACY_BOARD_NAME_SIZE * num_boards))
    goto err_meter;

  // Decrypt and fix board table.
  for(i = 0; i < num_boards; i++)
  {
    // Board length.
    int board_length = vfgetd(data->source) ^ data->xor_d;
    vfputd(board_length, data->dest);
    data->meter_curr += 4;

    // Board offset.
    if(!decrypt_and_fix_offset(data))
      goto err_meter;
  }

  // Decrypt the rest of the file.
  i = vftell(data->source);
  if(!decrypt_block(data, data->source_length - i))
    goto err_meter;

  meter_restore_screen();
  return true;

err_meter:
  meter_restore_screen();
  return false;
}

static vfile *legacy_decrypt_world_to_temp_file(vfile *source)
{
  struct decrypt_data *data;
  vfile *dest;
  size_t source_length = vfilelength(source, true);

  debug("Attempting decrypt: to temporary file.\n");
  dest = vtempfile(source_length - MAX_PASSWORD_LENGTH);
  if(!dest)
  {
    debug("Attempting decrypt: to _DECRYPT.TMP\n");
    dest = vfopen_unsafe("_DECRYPT.TMP", "wb+");
    if(!dest)
      return NULL;
  }

  data = ccalloc(1, sizeof(struct decrypt_data));

  data->source_length = source_length;
  data->source = source;
  data->dest = dest;

  if(!decrypt(data))
  {
    vfclose(data->dest);
    free(data);
    return NULL;
  }
  free(data);
  return dest;
}

static vfile *legacy_decrypt_world(const char *file_name)
{
  struct decrypt_data *data;
  vfile *dest;
  int file_length;

  data = ccalloc(1, sizeof(struct decrypt_data));

  data->file_name = file_name;
  snprintf(data->backup_name, MAX_PATH, "%.*s.locked", MAX_PATH - 8, file_name);
  debug("Attempting decrypt: to '%s'.\n", data->backup_name);

  file_length = decrypt_backup(data);
  if(file_length < 0)
  {
    // Try a shorter name...
    ptrdiff_t pos = strrchr(file_name, '.') - file_name;
    if(pos >= 0 && pos < MAX_PATH)
    {
      data->backup_name[pos] = '\0';

      if(path_force_ext(data->backup_name, MAX_PATH, ".LCK"))
      {
        debug("Attempting decrypt: to '%s'.\n", data->backup_name);
        file_length = decrypt_backup(data);
      }
    }

    if(file_length < 0)
    {
      free(data);
      return NULL;
    }
  }

  data->source_length = file_length;
  data->source = vfopen_unsafe(data->backup_name, "rb");
  if(!data->source)
    goto err_free;

  data->dest = vfopen_unsafe_ext(file_name, "wb", V_SMALL_BUFFER);
  if(!data->dest)
  {
    error_message(E_WORLD_DECRYPT_WRITE_PROTECTED, 0, NULL);
    goto err_free;
  }

  if(!decrypt(data))
    goto err_free;

  dest = data->dest;
  vfclose(data->source);
  free(data);
  return dest;

err_free:
  if(data->source)
    vfclose(data->source);
  if(data->dest)
    vfclose(data->dest);

  free(data);
  return NULL;
}


/* Validate that this file is a world or save file within reasonable doubt.
 * This needs to be done before any data is ever loaded so that Megazeux can
 * cleanly abort if there is an issue.
 *
 * There are a few redundant checks here with try_load_world, but that's ok.
 */
static enum val_result __validate_legacy_world_file(vfile *vf, boolean savegame)
{
  char magic[15];
  int num_boards;
  int board_name_offset;
  int v, i;

  /* TEST 2:  Is it a save file? */
  if(savegame)
  {
    int screen_mode, num_counters, num_strings, len;

    if(vfread(magic, 5, 1, vf) != 1)
      goto err_invalid;

    v = save_magic(magic);
    if(!v)
    {
      goto err_invalid;
    }
    else

    if(v > MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_TOO_RECENT, v, NULL);
      return VAL_VERSION;
    }
    else

    // This enables 2.84 save loading.
    // This can be expanded to even older save files in the future:
    // 2.00-2.83 saves require various types of surgery to the load functions.
    // TODO: 1.xx saves are implemented but this function needs an overhaul.
    if(v < MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_OLD, v, NULL);
      return VAL_VERSION;
    }

    /* TEST 3:  Check for truncation, savegame style. */
    if(vfseek(vf, 8 + WORLD_BLOCK_1_SIZE + 71, SEEK_SET))
    {
      debug("save block 1\n");
      goto err_invalid;
    }
    len = vfgetw(vf);
    if(len < 0 || vfseek(vf, len + WORLD_BLOCK_2_SIZE + 24, SEEK_CUR))
    {
      debug("save block 2\n");
      goto err_invalid;
    }

    //do counters - vvvvnnnn(name)
    num_counters = vfgetd(vf);
    if(num_counters < 0)
    {
      debug("counter num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_counters; i++)
    {
      vfgetd(vf);
      len = vfgetd(vf);

      if(len < 0 || len >= ROBOT_MAX_TR)
      {
        debug("counter %d: name length=%d\n", i, len);
        goto err_invalid;
      }
      if(vfseek(vf, len, SEEK_CUR))
      {
        debug("counter %d name\n", i);
        goto err_invalid;
      }
    }

    //do strings-   nnnnllll(name)(value)
    num_strings = vfgetd(vf);
    if(num_strings < 0)
    {
      debug("string num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_strings; i++)
    {
      int name_length = vfgetd(vf);
      int value_length = vfgetd(vf);
      if(value_length < 0 || name_length < 0 || name_length >= ROBOT_MAX_TR)
      {
        debug("string %d: value length=%d, name length=%d\n",
         i, value_length, name_length);
        goto err_invalid;
      }
      if(vfseek(vf, name_length + value_length, SEEK_CUR))
      {
        debug("string %d name+value\n", i);
        goto err_invalid;
      }
    }

    if(
     vfseek(vf, 4612, SEEK_CUR) || //sprites
     vfseek(vf, 12, SEEK_CUR) || //misc
     vfseek(vf, vfgetw(vf), SEEK_CUR) || //fread_open
     vfseek(vf, 4, SEEK_CUR) || //fread_pos
     vfseek(vf, vfgetw(vf), SEEK_CUR) || //fwrite_open
     vfseek(vf, 4, SEEK_CUR)) //fwrite_pos
    {
      debug("post strings\n");
      goto err_invalid;
    }

    screen_mode = vfgetw(vf);
    if((screen_mode < 0) || (screen_mode > 3) || (screen_mode > 1 &&
     vfseek(vf, 768, SEEK_CUR))) //smzx palette
    {
      debug("smzx palette\n");
      goto err_invalid;
    }

    if(
     vfseek(vf, 4, SEEK_CUR) || //commands
     ((len = vfgetd(vf)) < 0) || //vlayer size
     vfseek(vf, 4, SEEK_CUR) || // width & height
     vfseek(vf, len, SEEK_CUR) || //chars
     vfseek(vf, len, SEEK_CUR)) //colors
    {
      debug("vlayer\n");
      goto err_invalid;
    }

    /* Global robot pointer */
    if(vfgetd(vf) < 0)
      goto err_invalid;
  }

  else /* !savegame */
  {
    int protection_method;

    if(vfseek(vf, LEGACY_BOARD_NAME_SIZE, SEEK_SET))
    {
      debug("world name\n");
      goto err_invalid;
    }

    /* TEST 4:  If it's locked, try to decrypt it. */
    protection_method = vfgetc(vf);
    if(protection_method != 0)
    {
      if(protection_method < 0 || protection_method > 3)
        goto err_invalid;

      /**
       * Check the version to make sure this file is a non-corrupted MZX file.
       * The MZX 2.x format stores a 15 byte password followed by the version.
       * The MZX 1.x format stores a 1 byte password length, a fixed 15 byte
       * password buffer, and then the version. In either case, the version is
       * not encrypted.
       */
      vfseek(vf, LEGACY_BOARD_NAME_SIZE + 1 + MAX_PASSWORD_LENGTH, SEEK_SET);
      if(!vfread(magic, 4, 1, vf))
        goto err_invalid;

      v = world_magic(magic);
      if(v < V200)
      {
        v = world_magic(magic + 1);
        if(v != V100)
          goto err_invalid;

        vfseek(vf, -3, SEEK_CUR);
      }
      /* 2.x encrypts locked worlds. */
      else
        return VAL_PROTECTED;
    }

    /* TEST 5:  Test the magic */
    if(!vfread(magic, 3, 1, vf))
      goto err_invalid;

    v = world_magic(magic);
    if(v == 0)
    {
      goto err_invalid;
    }
    else

    if(v < V100)
    {
      error_message(E_WORLD_FILE_VERSION_OLD, v, NULL);
      return VAL_VERSION;
    }
    else

    if(v > MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_WORLD_FILE_VERSION_TOO_RECENT, v, NULL);
      return VAL_VERSION;
    }

    if(v >= V200)
    {
      /* TEST 6:  Attempt to eliminate invalid files by
       * testing the palette for impossible values.
       */
      vfseek(vf, WORLD_GLOBAL_OFFSET_OFFSET - 48, SEEK_SET);
      for(i = 0; i < 48; i++)
      {
        int val = vfgetc(vf);
        if(val < 0 || val > 63)
          goto err_invalid;
      }

      /* Global robot pointer */
      if(vfgetd(vf) < 0)
        goto err_invalid;
    }
    else
    {
      /* 1.x doesn't have a palette or global robot pointer to check. */
      long offset = WORLD_V1_BOARD_NUM_OFFSET;
      if(protection_method)
        offset += MAX_PASSWORD_LENGTH + 1;

      if(vfseek(vf, offset, SEEK_SET))
        goto err_invalid;
    }
  }

  /* TEST 7:  All branches should be at the board count now.
   * Test for valid SFX structure, if applicable, and board information.
   */
  num_boards = vfgetc(vf);
  if(num_boards == 0 && v >= V200)
  {
    int sfx_size = vfgetw(vf);
    int sfx_read = 0;
    int cur_sfx_size;

    for(i = 0; i < NUM_BUILTIN_SFX; i++)
    {
      cur_sfx_size = vfgetc(vf);
      if(cur_sfx_size < 0 || cur_sfx_size > LEGACY_SFX_SIZE)
        goto err_invalid;

      if(vfseek(vf, cur_sfx_size, SEEK_CUR))
        break;

      sfx_read += cur_sfx_size + 1;
    }

    if(i != NUM_BUILTIN_SFX || sfx_read != sfx_size)
      goto err_invalid;

    num_boards = vfgetc(vf);
  }
  if(num_boards <= 0)
    goto err_invalid;

  // Read the last board size and pointer to make sure the board table exists.
  board_name_offset = num_boards * (LEGACY_BOARD_NAME_SIZE + 8) - 8;

  if(vfseek(vf, board_name_offset, SEEK_CUR))
    goto err_invalid;

  if(vfgetd(vf) == EOF || vfgetd(vf) == EOF)
    goto err_invalid;

  return VAL_SUCCESS;

err_invalid:
  if(savegame)
    error_message(E_SAVE_FILE_INVALID, 0, NULL);
  else
    error_message(E_WORLD_FILE_INVALID, 0, NULL);
  return VAL_INVALID;
}

vfile *validate_legacy_world_file(struct world *mzx_world,
 const char *file, boolean savegame)
{
  struct stat stat_result;
  enum val_result res;
  vfile *vf;

  /* TEST 1: make sure it's even a file. */
  if(vstat(file, &stat_result) || !S_ISREG(stat_result.st_mode))
  {
    error_message(E_FILE_DOES_NOT_EXIST, 0, NULL);
    return NULL;
  }

  vf = vfopen_unsafe_ext(file, "rb", V_LARGE_BUFFER);
  if(!vf)
  {
    error_message(E_IO_READ, 0, NULL);
    return NULL;
  }

  res = __validate_legacy_world_file(vf, savegame);
  if(res == VAL_SUCCESS)
    return vf;

  if(res == VAL_PROTECTED)
  {
    if(get_config()->auto_decrypt_worlds || !has_video_initialized())
    {
      // Attempt to decrypt to a temporary file.
      vfile *tmp = legacy_decrypt_world_to_temp_file(vf);
      if(tmp)
      {
        vfclose(vf);
        return tmp;
      }
      error_message(E_IO_WRITE, 0, NULL);
      return NULL;
    }

    if(!confirm(mzx_world, "This world may be password protected. Decrypt it?"))
    {
      vfclose(vf);

      // Decrypt and try again
      vf = legacy_decrypt_world(file);
      if(!vf)
      {
        error_message(E_IO_READ, 0, NULL);
        return NULL;
      }

      res = __validate_legacy_world_file(vf, savegame);
      if(res == VAL_SUCCESS)
        return vf;
    }
    if(res == VAL_PROTECTED)
      error_message(E_WORLD_LOCKED, 0, NULL);
  }
  vfclose(vf);
  return NULL;
}

/**
 * Common v1 and v2 board list loader.
 */
static void legacy_load_world_boards(struct world *mzx_world, vfile *vf,
 boolean savegame, int file_version, int num_boards)
{
  unsigned int *board_offsets;
  unsigned int *board_sizes;
  long board_names_pos;
  int i;

  int meter_target = 2 + num_boards;
  int meter_curr = 0;

  meter_update_screen(&meter_curr, meter_target);

  mzx_world->num_boards = num_boards;
  mzx_world->num_boards_allocated = num_boards;
  mzx_world->board_list = cmalloc(sizeof(struct board *) * num_boards);

  // Skip the names for now
  board_names_pos = vftell(vf);
  vfseek(vf, num_boards * LEGACY_BOARD_NAME_SIZE, SEEK_CUR);

  // Read the board offsets/sizes preemptively to reduce the amount of seeking.
  board_offsets = cmalloc(sizeof(int) * num_boards);
  board_sizes = cmalloc(sizeof(int) * num_boards);
  for(i = 0; i < num_boards; i++)
  {
    board_sizes[i] = vfgetd(vf);
    board_offsets[i] = vfgetd(vf);
  }

  for(i = 0; i < num_boards; i++)
  {
    mzx_world->board_list[i] =
     legacy_load_board_allocate(mzx_world, vf, board_offsets[i], board_sizes[i],
      savegame, file_version);

    if(mzx_world->board_list[i])
    {
      // Also patch a pointer to the global robot
      if(mzx_world->board_list[i]->robot_list)
        (mzx_world->board_list[i])->robot_list[0] = &mzx_world->global_robot;

      // Also optimize out null objects
      optimize_null_objects(mzx_world->board_list[i]);

      store_board_to_extram(mzx_world->board_list[i]);
    }

    meter_update_screen(&meter_curr, meter_target);
  }

  free(board_offsets);
  free(board_sizes);

  // Go back to where the names are
  vfseek(vf, board_names_pos, SEEK_SET);
  for(i = 0; i < num_boards; i++)
  {
    char ignore[LEGACY_BOARD_NAME_SIZE];
    char *board_name = ignore;

    if(mzx_world->board_list[i])
      board_name = mzx_world->board_list[i]->board_name;

    if(!vfread(board_name, LEGACY_BOARD_NAME_SIZE, 1, vf))
      board_name[0] = 0;

    board_name[LEGACY_BOARD_NAME_SIZE - 1] = '\0';
  }
}

/**
 * Load a world in the extremely legacy 1.x world format.
 */
static void legacy_load_world_v1(struct world *mzx_world, vfile *vf, const char *file,
 boolean savegame, int file_version, char *name, boolean *faded)
{
  static const uint8_t v1_id_dmg[ID_DMG_SIZE] =
  {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 0x0? */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,100,  0,  0,  0,  0,  0, /* 0x1? */
    0,  0,  0,  0,  0,  0,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 0x2? */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10,  0, 10,  5,  5, /* 0x3? */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10, 10,  0, 10, 10, /* 0x4? */
    10, 0,  0, 10, 10, 10, 10, 10, 10, 10, 10, 10,  0,  0, 10, 10, /* 0x5? */
  };

  unsigned char *charset_mem;
  int protection_method;
  int num_boards;
  long offset;
  int score = 0;
  int i;
  char buf[4];

  int meter_target = 2, meter_curr = 0;

  mzx_world->version = file_version;
  mzx_world->current_board_id = 0;

  if(savegame)
  {
    vfseek(vf, 5, SEEK_SET);
    mzx_world->version = file_version;
    mzx_world->current_board_id = vfgetc(vf);
  }
  else
  {
    strcpy(mzx_world->name, name);

    vfseek(vf, BOARD_NAME_SIZE, SEEK_SET);
    protection_method = vfgetc(vf);

    offset = BOARD_NAME_SIZE + 1 + 3;
    if(protection_method)
      offset += MAX_PASSWORD_LENGTH + 1;
    vfseek(vf, offset, SEEK_SET);
  }

  meter_initial_draw(meter_curr, meter_target, "Loading...");

  charset_mem = cmalloc(CHAR_SIZE * CHARSET_SIZE);
  if(!vfread(charset_mem, CHAR_SIZE * CHARSET_SIZE, 1, vf))
  {
    free(charset_mem);
    goto err_close;
  }
  ec_clear_set();
  ec_mem_load_set(charset_mem, CHAR_SIZE * CHARSET_SIZE);
  free(charset_mem);

  // id_chars.
  memset(id_chars, 0, ID_CHARS_SIZE);
  memcpy(id_dmg, v1_id_dmg, ID_DMG_SIZE);
  memset(bullet_color, 0, ID_BULLET_COLOR_SIZE);

  if(!vfread(id_chars, 306, 1, vf))
    goto err_close;
  // Goop didn't exist in 1.x.
  id_chars[GOOP] = 176;

  if(savegame)
  {
    if(!vfread(mzx_world->keys, NUM_KEYS, 1, vf))
      goto err_close;
  }

  // Status counters.
  if(vfread((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE, 4, vf) != 4)
    goto err_close;

  for(i = 0; i < 4; i++)
    mzx_world->status_counters_shown[i][COUNTER_NAME_SIZE - 1] = '\0';
  for(; i < NUM_STATUS_COUNTERS; i++)
    mzx_world->status_counters_shown[i][0] = '\0';

  // Junk that was later moved to id_chars.
  if(!vfread(buf, 4, 1, vf))
    goto err_close;
  memcpy(id_chars + bullet_char, buf, 4);
  memcpy(id_chars + bullet_char + 4, buf, 4);
  memcpy(id_chars + bullet_char + 8, buf, 4);
  buf[0] = vfgetc(vf);
  memset(id_chars + player_char, buf[0], 4);

  // Saved position.
  memset(mzx_world->pl_saved_x, 0, sizeof(mzx_world->pl_saved_x));
  memset(mzx_world->pl_saved_y, 0, sizeof(mzx_world->pl_saved_y));
  memset(mzx_world->pl_saved_board, 0, sizeof(mzx_world->pl_saved_board));
  mzx_world->pl_saved_x[0] = vfgetc(vf);
  mzx_world->pl_saved_y[0] = vfgetc(vf);
  mzx_world->pl_saved_board[0] = vfgetc(vf);

  // Global info.
  mzx_world->edge_color = vfgetc(vf);

  // More junk that was later moved to id_chars.
  id_chars[player_color] = vfgetc(vf);
  i = vfgetc(vf);
  memset(bullet_color, i, sizeof(bullet_color));
  missile_color = vfgetc(vf);

  if(savegame)
  {
    mzx_world->scroll_base_color = vfgetc(vf);
    mzx_world->scroll_corner_color = vfgetc(vf);
    mzx_world->scroll_pointer_color = vfgetc(vf);
    mzx_world->scroll_title_color = vfgetc(vf);
    mzx_world->scroll_arrow_color = vfgetc(vf);
    score = vfgetd(vf);
  }

  // Global info.
  mzx_world->first_board = vfgetc(vf);
  mzx_world->endgame_board = vfgetc(vf);
  mzx_world->death_board = vfgetc(vf);
  mzx_world->endgame_x = vfgetc(vf);
  mzx_world->endgame_y = vfgetc(vf);
  mzx_world->death_x = vfgetc(vf);
  mzx_world->death_y = vfgetc(vf);
  mzx_world->starting_lives = vfgetw(vf);
  mzx_world->lives_limit = vfgetw(vf);
  mzx_world->starting_health = vfgetw(vf);
  mzx_world->health_limit = vfgetw(vf);
  mzx_world->enemy_hurt_enemy = vfgetc(vf);
  mzx_world->clear_on_exit = 0;
  mzx_world->only_from_swap = 0;
  mzx_world->game_over_sfx = vfgetc(vf);
  vfgetc(vf); // unused

  // Fix special values for these.
  if(mzx_world->endgame_board >= 128)
    mzx_world->endgame_board = NO_ENDGAME_BOARD;

  if(mzx_world->death_board >= 129)
    mzx_world->death_board = DEATH_SAME_POS;
  else
  if(mzx_world->death_board == 128)
    mzx_world->death_board = NO_DEATH_BOARD;

  if(savegame)
  {
    // Counter list.
    // Allocate 1 extra for the score.
    legacy_load_counter_list(mzx_world, vf, file_version, 1);

    // Patch in the score.
    set_counter(mzx_world, "SCORE", score, 0);
  }

  // No custom palette or global robot in 1.x.
  *faded = 0;
  default_palette();
  create_blank_robot(&mzx_world->global_robot);
  create_blank_robot_program(&mzx_world->global_robot);

  // Board list is the same as 2.x, though the board structs are different.
  num_boards = vfgetc(vf);
  legacy_load_world_boards(mzx_world, vf, savegame, file_version, num_boards);

  meter_restore_screen();
  vfclose(vf);
  return;

err_close:
  // Note that this file had already been successfully validated for length
  // and opened with no issue before this error occurred, and that the only
  // way to reach this error is a failed fread before any board/robot data
  // was loaded. Something seriously went wrong somewhere.

  error_message(E_IO_READ, 0, NULL);
  meter_restore_screen();
  vfclose(vf);
}

/**
 * Load a world or save file from the legacy world format.
 * This will close the provided vfile handle.
 */
void legacy_load_world(struct world *mzx_world, vfile *vf, const char *file,
 boolean savegame, int file_version, char *name, boolean *faded)
{
  int i;
  int num_boards;
  int global_robot_pos;
  unsigned char *charset_mem;
  unsigned char r, g, b;

  int meter_target = 2, meter_curr = 0;

  if(file_version < V200)
  {
    /* Too many differences to properly version here :( */
    legacy_load_world_v1(mzx_world, vf, file, savegame, file_version, name, faded);
    return;
  }

  if(savegame)
  {
    vfseek(vf, 5, SEEK_SET);
    mzx_world->version = vfgetw(vf);
    mzx_world->current_board_id = vfgetc(vf);
  }
  else
  {
    vfseek(vf, 29, SEEK_SET);
    strcpy(mzx_world->name, name);
    mzx_world->version = file_version;
    mzx_world->current_board_id = 0;
  }

  meter_initial_draw(meter_curr, meter_target, "Loading...");

  charset_mem = cmalloc(CHAR_SIZE * CHARSET_SIZE);
  if(!vfread(charset_mem, CHAR_SIZE * CHARSET_SIZE, 1, vf))
  {
    free(charset_mem);
    goto err_close;
  }
  ec_clear_set();
  ec_mem_load_set(charset_mem, CHAR_SIZE * CHARSET_SIZE);
  free(charset_mem);

  // Idchars array...
  memset(id_chars, 0, ID_CHARS_SIZE);
  memset(id_dmg, 0, ID_DMG_SIZE);
  memset(bullet_color, 0, ID_BULLET_COLOR_SIZE);

  if(!vfread(id_chars, LEGACY_ID_CHARS_SIZE, 1, vf))
    goto err_close;
  missile_color = vfgetc(vf);
  if(!vfread(bullet_color, LEGACY_ID_BULLET_COLOR_SIZE, 1, vf))
    goto err_close;
  if(!vfread(id_dmg, LEGACY_ID_DMG_SIZE, 1, vf))
    goto err_close;

  // Status counters...
  if(vfread((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE,
   NUM_STATUS_COUNTERS, vf) != NUM_STATUS_COUNTERS)
    goto err_close;

  // Don't trust legacy null termination...
  for(i = 0; i < NUM_STATUS_COUNTERS; i++)
    mzx_world->status_counters_shown[i][COUNTER_NAME_SIZE - 1] = '\0';

  if(savegame)
  {
    if(!vfread(mzx_world->keys, NUM_KEYS, 1, vf))
      goto err_close;
    mzx_world->blind_dur = vfgetc(vf);
    mzx_world->firewalker_dur = vfgetc(vf);
    mzx_world->freeze_time_dur = vfgetc(vf);
    mzx_world->slow_time_dur = vfgetc(vf);
    mzx_world->wind_dur = vfgetc(vf);

    for(i = 0; i < 8; i++)
    {
      mzx_world->pl_saved_x[i] = vfgetw(vf);
    }

    for(i = 0; i < 8; i++)
    {
      mzx_world->pl_saved_y[i] = vfgetw(vf);
    }

    if(file_version >= V280)
    {
      // Saved positions 3-8 were broken due to a bug and 1-2 were
      // saved as endian-dependent dwords.
      mzx_world->pl_saved_board[0] = vfgetd(vf) & 0xff;
      mzx_world->pl_saved_board[1] = vfgetd(vf) & 0xff;

      for(i = 2; i < 8; i++)
        mzx_world->pl_saved_board[i] = 0;
    }
    else
    {
      for(i = 0; i < 8; i++)
        mzx_world->pl_saved_board[i] = vfgetc(vf);
    }

    mzx_world->saved_pl_color = vfgetc(vf);
    mzx_world->under_player_id = vfgetc(vf);
    mzx_world->under_player_color = vfgetc(vf);
    mzx_world->under_player_param = vfgetc(vf);
    mzx_world->mesg_edges = vfgetc(vf);
    mzx_world->scroll_base_color = vfgetc(vf);
    mzx_world->scroll_corner_color = vfgetc(vf);
    mzx_world->scroll_pointer_color = vfgetc(vf);
    mzx_world->scroll_title_color = vfgetc(vf);
    mzx_world->scroll_arrow_color = vfgetc(vf);

    {
      size_t len = vfgetw(vf);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !vfread(mzx_world->real_mod_playing, len, 1, vf))
        goto err_close;
      mzx_world->real_mod_playing[len] = 0;
    }
  }

  mzx_world->edge_color = vfgetc(vf);
  mzx_world->first_board = vfgetc(vf);
  mzx_world->endgame_board = vfgetc(vf);
  mzx_world->death_board = vfgetc(vf);
  mzx_world->endgame_x = vfgetw(vf);
  mzx_world->endgame_y = vfgetw(vf);
  mzx_world->game_over_sfx = vfgetc(vf);
  mzx_world->death_x = vfgetw(vf);
  mzx_world->death_y = vfgetw(vf);
  mzx_world->starting_lives = vfgetw(vf);
  mzx_world->lives_limit = vfgetw(vf);
  mzx_world->starting_health = vfgetw(vf);
  mzx_world->health_limit = vfgetw(vf);
  mzx_world->enemy_hurt_enemy = vfgetc(vf);
  mzx_world->clear_on_exit = vfgetc(vf);
  mzx_world->only_from_swap = vfgetc(vf);

  // Palette...
  // Note: always loaded to the MZX mode palette as SMZX hasn't been set yet.
  for(i = 0; i < 16; i++)
  {
    r = vfgetc(vf);
    g = vfgetc(vf);
    b = vfgetc(vf);

    set_rgb(i, r, g, b);
  }

  if(savegame)
  {
    unsigned int vlayer_size;
    int screen_mode;

    for(i = 0; i < 16; i++)
    {
      set_color_intensity(i, vfgetc(vf));
    }

    *faded = vfgetc(vf);

    mzx_world->player_restart_x = vfgetw(vf);
    mzx_world->player_restart_y = vfgetw(vf);
    mzx_world->under_player_id = vfgetc(vf);
    mzx_world->under_player_color = vfgetc(vf);
    mzx_world->under_player_param = vfgetc(vf);

    // Read counters
    legacy_load_counter_list(mzx_world, vf, file_version, 0);

    // Read strings (2.80 and up; DOS handled this differently)
    if(file_version >= V280)
      legacy_load_string_list(mzx_world, vf, file_version);

    // Sprite data
    for(i = 0; i < MAX_SPRITES; i++)
    {
      (mzx_world->sprite_list[i])->x = vfgetw(vf);
      (mzx_world->sprite_list[i])->y = vfgetw(vf);
      (mzx_world->sprite_list[i])->ref_x = vfgetw(vf);
      (mzx_world->sprite_list[i])->ref_y = vfgetw(vf);
      (mzx_world->sprite_list[i])->color = vfgetc(vf);
      (mzx_world->sprite_list[i])->flags = vfgetc(vf);
      (mzx_world->sprite_list[i])->width = vfgetc(vf);
      (mzx_world->sprite_list[i])->height = vfgetc(vf);
      (mzx_world->sprite_list[i])->col_x = vfgetc(vf);
      (mzx_world->sprite_list[i])->col_y = vfgetc(vf);
      (mzx_world->sprite_list[i])->col_width = vfgetc(vf);
      (mzx_world->sprite_list[i])->col_height = vfgetc(vf);
    }

    // total sprites
    mzx_world->active_sprites = vfgetc(vf);
    // y order flag
    mzx_world->sprite_y_order = vfgetc(vf);
    // collision info
    mzx_world->collision_count = vfgetw(vf);

    for(i = 0; i < MAX_SPRITES; i++)
    {
      mzx_world->collision_list[i] = vfgetw(vf);
    }

    // Multiplier
    mzx_world->multiplier = vfgetw(vf);
    // Divider
    mzx_world->divider = vfgetw(vf);
    // Circle divisions
    mzx_world->c_divisions = vfgetw(vf);
    // String FREAD and FWRITE Delimiters
    mzx_world->fread_delimiter = vfgetw(vf);
    mzx_world->fwrite_delimiter = vfgetw(vf);
    // Builtin shooting/message status
    mzx_world->bi_shoot_status = vfgetc(vf);
    mzx_world->bi_mesg_status = vfgetc(vf);

    // Load input file name, open later
    {
      size_t len = vfgetw(vf);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !vfread(mzx_world->input_file_name, len, 1, vf))
        goto err_close;
      mzx_world->input_file_name[len] = 0;
    }
    mzx_world->temp_input_pos = vfgetd(vf);

    // Load output file name, open later
    {
      size_t len = vfgetw(vf);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !vfread(mzx_world->output_file_name, len, 1, vf))
        goto err_close;
      mzx_world->output_file_name[len] = 0;
    }
    mzx_world->temp_output_pos = vfgetd(vf);

    screen_mode = vfgetw(vf);

    // If it's at SMZX mode 2, set default palette as loaded
    // so the .sav one doesn't get overwritten
    if(screen_mode == 2)
    {
      smzx_palette_loaded(true);
    }
    set_screen_mode(screen_mode);

    // Also get the palette
    if(screen_mode > 1)
    {
      for(i = 0; i < 256; i++)
      {
        r = vfgetc(vf);
        g = vfgetc(vf);
        b = vfgetc(vf);

        set_rgb(i, r, g, b);
      }
    }

    mzx_world->commands = vfgetd(vf);

    vlayer_size = vfgetd(vf);
    vlayer_size = CLAMP(vlayer_size, 1, MAX_BOARD_SIZE);
    mzx_world->vlayer_width = vfgetw(vf);
    mzx_world->vlayer_height = vfgetw(vf);
    mzx_world->vlayer_size = vlayer_size;

    // This might have been allocated already...
    mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, vlayer_size);
    mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, vlayer_size);

    if(vfread(mzx_world->vlayer_chars, 1, vlayer_size, vf) != vlayer_size ||
     vfread(mzx_world->vlayer_colors, 1, vlayer_size, vf) != vlayer_size)
      goto err_close;
  }

  // Get position of global robot...
  global_robot_pos = vfgetd(vf);
  // Get number of boards
  num_boards = vfgetc(vf);

  if(num_boards == 0)
  {
    struct sfx_list *custom_sfx = &mzx_world->custom_sfx;
    char sfx_buf[LEGACY_SFX_SIZE];
    size_t sfx_size;
    // Sfx
    vfgetw(vf); // Skip word size

    //Read sfx
    for(i = 0; i < NUM_BUILTIN_SFX; i++)
    {
      // Already bound checked...
      sfx_size = vfgetc(vf);
      if(vfread(sfx_buf, 1, sfx_size, vf) < sfx_size)
        goto err_close;

      sfx_set_string(custom_sfx, i, sfx_buf, sfx_size);
    }
    num_boards = vfgetc(vf);
  }
  else
  {
    sfx_free(&mzx_world->custom_sfx);
  }

  legacy_load_world_boards(mzx_world, vf, savegame, file_version, num_boards);

  // Read global robot
  vfseek(vf, global_robot_pos, SEEK_SET); //don't worry if this fails
  legacy_load_robot(mzx_world, &mzx_world->global_robot, vf, savegame,
   file_version);

  // Some old worlds have the global_robot marked unused. Always mark it used.
  mzx_world->global_robot.used = 1;

  meter_restore_screen();
  vfclose(vf);
  return;

err_close:
  // Note that this file had already been successfully validated for length
  // and opened with no issue before this error occurred, and that the only
  // way to reach this error is a failed fread before any board/robot data
  // was loaded. Something seriously went wrong somewhere.

  error_message(E_IO_READ, 0, NULL);
  meter_restore_screen();
  vfclose(vf);
}
