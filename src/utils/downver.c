/* MegaZeux
 *
 * This is a very trivial tool to downgrade a world or board file to the
 * previous binary version. It allows a world or board created in the current
 * version of MegaZeux to be loadable in the previous version (i.e. before the
 * world format change). Of course, the downgraded world may not work, but it
 * should be loadable by the editor.
 *
 * This tool is a good insurance policy for brand new releases, to prevent
 * the case where people start work in a newer version of MegaZeux, only
 * for there to be a critical bug discovered, and for the user to be "locked
 * in" to the buggy version.
 *
 * Primarily, this tool is intended for DoZs where a new version may be used,
 * but a serious bug cannot reasonably be corrected before the end of the
 * competition.
 *
 * Since the tool will change with every release, it is hard coded to downgrade
 * only to one previous version, from one current version. This is intentional,
 * to make the tool easy to use for newbies (simply drag and drop).
 *
 * Copyright (C) 2008-2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017-2023 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <string.h>

#ifdef __WIN32__
#include <strings.h>
#endif

#define CORE_LIBSPEC
#include "../compat.h"
#include "utils_alloc.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

#include "../const.h"
#include "../graphics.h"
#include "../util.h"
#include "../world.h"
#include "../world_format.h"
#include "../algorithm/string_compare.h"
#include "../io/memfile.h"
#include "../io/vio.h"
#include "../io/zip.h"

#define DOWNVER_VERSION "2.93"
#define DOWNVER_EXT ".292"

#define MZX_VERSION_HI ((MZX_VERSION >> 8) & 0xff)
#define MZX_VERSION_LO (MZX_VERSION & 0xff)

#define MZX_VERSION_PREV_HI ((MZX_VERSION_PREV >> 8) & 0xff)
#define MZX_VERSION_PREV_LO (MZX_VERSION_PREV & 0xff)

enum status
{
  SUCCESS = 0,
  ARCHIVE_ERROR,
  READ_ERROR,
  WRITE_ERROR,
  SEEK_ERROR,
};

#define error(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }

struct downver_state
{
  struct zip_archive *out;
  struct zip_archive *in;

  /* 2.93 conversion vars */
  int screen_mode;
};

static inline void save_prop_p(int ident, struct memfile *prop,
 struct memfile *mf)
{
  save_prop_a(ident, prop->start, (prop->end - prop->start), 1, mf);
}

/* <2.92d did not properly terminate this field on loading it, convert it back
 * to an ASCIIZ string. */
static inline void save_prop_s_to_asciiz(int ident, size_t len,
 struct memfile *prop, struct memfile *mf)
{
  char buf[BOARD_NAME_SIZE];
  len = MIN(len, BOARD_NAME_SIZE - 1);

  len = mfread(buf, 1, len, prop);
  buf[len] = '\0';
  save_prop_a(ident, buf, len + 1, 1, mf);
}

static enum zip_error zip_duplicate_file(struct downver_state *dv,
 void (*handler)(struct downver_state *, struct memfile *, struct memfile *))
{
  enum zip_error result;
  unsigned int method;
  char name[9];
  void *buffer;
  size_t actual_size;

  result = zip_get_next_name(dv->in, name, 9);
  if(result)
    return result;

  result = zip_get_next_method(dv->in, &method);
  if(result)
    return result;

  result = zip_get_next_uncompressed_size(dv->in, &actual_size);
  if(result)
    return result;

  buffer = malloc(actual_size);
  if(!buffer)
    return ZIP_ALLOC_ERROR;

  result = zip_read_file(dv->in, buffer, actual_size, &actual_size);
  if(result)
    goto err_free;

  if(handler)
  {
    struct memfile mf_in;
    struct memfile mf_out;

    void *buffer2 = malloc(actual_size * 2);
    if(buffer2)
    {
      mfopen(buffer, actual_size, &mf_in);
      mfopen_wr(buffer2, actual_size * 2, &mf_out);

      handler(dv, &mf_out, &mf_in);

      // Free the old buffer, then replace it with the new buffer.
      free(buffer);
      mfsync(&buffer, &actual_size, &mf_out);
    }
  }

  result = zip_write_file(dv->out, name, buffer, actual_size, (int)method);

err_free:
  free(buffer);
  return result;
}

static void convert_293_to_292_status_counters(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  char counters[NUM_STATUS_COUNTERS * COUNTER_NAME_SIZE];
  struct memfile prop;
  int ident;
  int len;
  int num = 0;

  memset(counters, 0, sizeof(counters));

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case STATCTRPROP_SET_ID:
        num = load_prop_int(&prop);
        break;

      case STATCTRPROP_NAME:
        if(num < NUM_STATUS_COUNTERS)
        {
          char *pos = counters + num * COUNTER_NAME_SIZE;
          len = MIN(len, COUNTER_NAME_SIZE - 1);
          len = mfread(pos, 1, len, &prop);
          pos[len] = '\0';
        }
        break;
    }
  }
  save_prop_a(WPROP_STATUS_COUNTERS, counters,
   COUNTER_NAME_SIZE, NUM_STATUS_COUNTERS, dest);
}

static void convert_293_to_292_world_info(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case WPROP_WORLD_VERSION:
      case WPROP_FILE_VERSION:
        // Replace the version number
        save_prop_w(ident, MZX_VERSION_PREV, dest);
        break;

      case WPROP_WORLD_NAME:
        /* NO loading termination pre-2.92d */
        save_prop_s_to_asciiz(ident, BOARD_NAME_SIZE, &prop, dest);
        break;

      case WPROP_STATUS_COUNTERS:
        convert_293_to_292_status_counters(dv, dest, &prop);
        break;

      case WPROP_SMZX_MODE:
        /* Load for palette conversion. */
        dv->screen_mode = load_prop_int(&prop);
        save_prop_c(ident, dv->screen_mode, dest);
        break;

      case WPROP_OUTPUT_MODE:
        /* Added in 2.93 */
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static void convert_293_to_292_board_info(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case BPROP_FILE_VERSION:
        // Replace the version number
        save_prop_w(ident, MZX_VERSION_PREV, dest);
        break;

      case BPROP_BOARD_NAME:
        /* loading termination expects BOARD_NAME_SIZE input pre-2.93 */
        save_prop_s_to_asciiz(ident, BOARD_NAME_SIZE, &prop, dest);
        break;

      case BPROP_RESET_ON_ENTRY_SAME_BOARD:
      case BPROP_DRAGONS_CAN_RANDOMLY_MOVE:
      case BPROP_BLIND_DUR:
      case BPROP_FIREWALKER_DUR:
      case BPROP_FREEZE_TIME_DUR:
      case BPROP_SLOW_TIME_DUR:
      case BPROP_WIND_DUR:
        /* Added in 2.93 */
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static void convert_293_to_292_robot_info(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case RPROP_ROBOT_NAME:
        /* NO loading termination pre-2.92d */
        save_prop_s_to_asciiz(ident, ROBOT_NAME_SIZE, &prop, dest);
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static void convert_293_to_292_sensor_info(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case SENPROP_SENSOR_NAME:
      case SENPROP_ROBOT_TO_MESG:
        /* NO loading termination pre-2.92d */
        save_prop_s_to_asciiz(ident, ROBOT_NAME_SIZE, &prop, dest);
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static void convert_293_to_292_sfx(struct downver_state *dv,
 struct memfile *dest, struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;
  int num = 0;
  char buf[8];

  mfresize(NUM_BUILTIN_SFX * LEGACY_SFX_SIZE, dest);
  memset(dest->start, 0, NUM_BUILTIN_SFX * LEGACY_SFX_SIZE);

  if(mfread(buf, 8, 1, src) && !memcmp(buf, "MZFX\x1a", 6))
  {
    while(next_prop(&prop, &ident, &len, src))
    {
      switch(ident)
      {
        case SFXPROP_SET_ID:
          num = load_prop_int(&prop);
          break;

        case SFXPROP_STRING:
          if(num >= 0 && num < NUM_BUILTIN_SFX)
          {
            len = MIN(len, LEGACY_SFX_SIZE - 1);
            mfseek(dest, num * LEGACY_SFX_SIZE, SEEK_SET);
            mfread(dest->current, len, 1, &prop);
          }
          break;
      }
    }
  }
  else
  {
    mfseek(src, 0, SEEK_SET);
    mfread(dest->start, LEGACY_SFX_SIZE, NUM_BUILTIN_SFX, src);
    dest->end[-1] = '\0';
  }
}

static enum status convert_293_to_292(struct downver_state *dv)
{
  enum zip_error err = ZIP_SUCCESS;
  unsigned int file_id;
  unsigned int board_id;
  unsigned int robot_id;

  if(!dv->in || !dv->out)
  {
    zip_close(dv->in, NULL);
    zip_close(dv->out, NULL);
    return ARCHIVE_ERROR;
  }

  world_assign_file_ids(dv->in, true);

  while(ZIP_SUCCESS == zip_get_next_mzx_file_id(dv->in, &file_id, &board_id, &robot_id))
  {
    switch(file_id)
    {
      case FILE_ID_WORLD_INFO:
        err = zip_duplicate_file(dv, convert_293_to_292_world_info);
        break;

      case FILE_ID_BOARD_INFO:
        err = zip_duplicate_file(dv, convert_293_to_292_board_info);
        break;

      case FILE_ID_WORLD_GLOBAL_ROBOT:
      case FILE_ID_ROBOT:
        err = zip_duplicate_file(dv, convert_293_to_292_robot_info);
        break;

      case FILE_ID_SENSOR:
        err = zip_duplicate_file(dv, convert_293_to_292_sensor_info);
        break;

      case FILE_ID_WORLD_SFX:
        err = zip_duplicate_file(dv, convert_293_to_292_sfx);
        break;

      case FILE_ID_WORLD_PAL:
        if(!dv->screen_mode)
          err = zip_duplicate_file(dv, NULL);
        else
          zip_skip_file(dv->in);
        break;

      case FILE_ID_WORLD_PAL_SMZX:
        if(dv->screen_mode)
        {
          char buf[SMZX_PAL_SIZE * 3];
          memset(buf, 0, sizeof(buf));
          err = zip_read_file(dv->in, buf, SMZX_PAL_SIZE * 3, NULL);
          err = zip_write_file(dv->out, "pal", buf, SMZX_PAL_SIZE * 3, ZIP_M_DEFLATE);
        }
        else
          zip_skip_file(dv->in);
        break;

      case FILE_ID_WORLD_PAL_INTENSITY:
        if(!dv->screen_mode)
        {
          char buf[PAL_SIZE * 4];
          int i;

          memset(buf, 0, sizeof(buf));
          err = zip_read_file(dv->in, buf, PAL_SIZE * 4, NULL);
          if(err == ZIP_SUCCESS)
          {
            // Convert to the old format.
            for(i = 0; i < PAL_SIZE; i++)
              buf[i] = buf[i * 4];

            zip_write_file(dv->out, "palint", buf, PAL_SIZE, ZIP_M_NONE);
          }
        }
        else
          zip_skip_file(dv->in);
        break;

      case FILE_ID_WORLD_PAL_INTENSITY_SMZX:
        if(dv->screen_mode)
        {
          char buf[SMZX_PAL_SIZE * 4];
          int i;

          memset(buf, 0, sizeof(buf));
          err = zip_read_file(dv->in, buf, SMZX_PAL_SIZE * 4, NULL);
          if(err == ZIP_SUCCESS)
          {
            // Convert to the old format.
            for(i = 0; i < SMZX_PAL_SIZE; i++)
              buf[i] = buf[i * 4];

            zip_write_file(dv->out, "palint", buf, SMZX_PAL_SIZE, ZIP_M_NONE);
          }
        }
        else
          zip_skip_file(dv->in);
        break;

      default:
        err = zip_duplicate_file(dv, NULL);
        break;
    }

    if(err != ZIP_SUCCESS)
    {
      // Skip files that don't work
      zip_skip_file(dv->in);
      err = ZIP_SUCCESS;
    }
  }

  zip_close(dv->in, NULL);
  zip_close(dv->out, NULL);
  return SUCCESS;
}

/* Validate the magic string. */
static boolean check_magic(const char *magic, boolean save)
{
  if(save)
  {
    if(magic[0] != 'M' || magic[1] != 'Z' || magic[2] != 'S')
    {
      error("Save file is corrupt or unsupported.\n");
      return false;
    }
    magic += 3;
  }
  else
  {
    if(magic[0] != 'M')
    {
      error("World or board file is corrupt or unsupported.\n");
      return false;
    }
    magic++;
  }

  if(magic[0] != MZX_VERSION_HI || magic[1] != MZX_VERSION_LO)
  {
    error("This tool only supports worlds, saves, or boards from %d.%d.\n",
     MZX_VERSION_HI, MZX_VERSION_LO);
    return false;
  }
  return true;
}

/* Replace the new magic string with the old magic string. */
static void fix_magic(char *magic, boolean save)
{
  if(save)
  {
    magic[3] = MZX_VERSION_PREV_HI;
    magic[4] = MZX_VERSION_PREV_LO;

    /* Also lower the original world version value (little endian). */
    if(magic[6] > MZX_VERSION_HI ||
     (magic[6] == MZX_VERSION_HI && magic[5] > MZX_VERSION_LO))
    {
      magic[5] = MZX_VERSION_PREV_LO;
      magic[6] = MZX_VERSION_PREV_HI;
    }
  }
  else
  {
    magic[1] = MZX_VERSION_PREV_HI;
    magic[2] = MZX_VERSION_PREV_LO;
  }
}

int main(int argc, char *argv[])
{
  char fname[MAX_PATH + 4];
  char magic[8];
  struct downver_state dv;
  enum status ret;
  long ext_pos;
  int byte;
  boolean world = false;
  boolean save = false;
  vfile *in;
  vfile *out;

  if(strcmp(VERSION, DOWNVER_VERSION) < 0 && 0)
  {
    error("[ERROR] Update downver for " VERSION "!\n");
    return 1;
  }

  if(argc <= 1)
  {
    error("Usage: %s [mzx/mzb file]\n", argv[0]);
    goto exit_out;
  }

  ext_pos = strlen(argv[1]) - 4;
  if(ext_pos < 0)
  {
    error("File '%s' of an unknown type.\n", argv[1]);
    goto exit_out;
  }

  if(!strcasecmp(argv[1] + ext_pos, ".mzb"))
  {
    snprintf(fname, sizeof(fname), "%.*s" DOWNVER_EXT ".mzb",
     (int)ext_pos, argv[1]);
    fname[sizeof(fname) - 1] = '\0';
  }
  else

  if(!strcasecmp(argv[1] + ext_pos, ".mzx"))
  {
    snprintf(fname, sizeof(fname), "%.*s" DOWNVER_EXT ".mzx",
     (int)ext_pos, argv[1]);
    fname[sizeof(fname) - 1] = '\0';
    world = true;
  }
  else

  if(!strcasecmp(argv[1] + ext_pos, ".sav"))
  {
    snprintf(fname, sizeof(fname), "%.*s" DOWNVER_EXT ".sav",
     (int)ext_pos, argv[1]);
    fname[sizeof(fname) - 1] = '\0';
    save = true;
  }
  else
  {
    error("Unknown extension '%s'.\n", argv[1] + ext_pos);
    goto exit_out;
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if(unveil(argv[1], "r") || unveil(fname, "cw") || unveil(NULL, NULL))
  {
    error("[ERROR] Failed unveil!\n");
    return 1;
  }
#endif

  if(pledge(PROMISES, ""))
  {
    error("[ERROR] Failed pledge!\n");
    return 1;
  }
#endif

  in = vfopen_unsafe(argv[1], "rb");
  if(!in)
  {
    error("Could not open '%s' for read.\n", argv[1]);
    goto exit_out;
  }

  out = vfopen_unsafe(fname, "wb");
  if(!out)
  {
    error("Could not open '%s' for write.\n", fname);
    vfclose(in);
    goto exit_out;
  }

  /* World files need some crap at the beginning skipped. Also, world files
   * can theoretically be encrypted, though practically this will not happen
   * with modern worlds. Just in case, check for it and abort.
   *
   * Board files just need the 0xFF byte at the start skipped.
   */
  if(world)
  {
    char name[BOARD_NAME_SIZE];
    size_t len;

    // Duplicate the world name
    len = vfread(name, BOARD_NAME_SIZE, 1, in);
    if(len != 1)
      goto err_read;

    len = vfwrite(name, BOARD_NAME_SIZE, 1, out);
    if(len != 1)
      goto err_write;

    // Check protection isn't enabled
    byte = vfgetc(in);
    if(byte < 0)
    {
      goto err_read;
    }
    else

    if(byte != 0)
    {
      error("Protected worlds are not supported.\n");
      goto exit_close;
    }
    vfputc(0, out);

    if(vfread(magic, 1, 3, in) < 3)
      goto err_read;

    if(!check_magic(magic, false))
      goto exit_close;

    fix_magic(magic, false);
    vfwrite(magic, 1, 3, out);
  }
  else

  if(save)
  {
    if(vfread(magic, 1, 8, in) < 8)
      goto err_read;

    if(!check_magic(magic, true))
      goto exit_close;

    fix_magic(magic, true);
    vfwrite(magic, 1, 8, out);
  }
  else
  {
    byte = vfgetc(in);
    if(byte < 0)
    {
      goto err_read;
    }
    else

    if(byte != 0xFF)
    {
      error("Board file is corrupt or unsupported.\n");
      goto exit_close;
    }
    vfputc(0xFF, out);

    if(vfread(magic, 1, 3, in) < 3)
      goto exit_close;

    if(!check_magic(magic, false))
      goto exit_close;

    fix_magic(magic, false);
    vfwrite(magic, 1, 3, out);
  }

  // Worlds, saves, and boards are the same from here out.
  // Conversion closes the file pointers, so NULL them.

  memset(&dv, 0, sizeof(dv));
  dv.in  = zip_open_vf_read(in);
  dv.out = zip_open_vf_write(out);

  // Zip64 support was added in 2.93.
  if(MZX_VERSION_PREV < V293)
    zip_set_zip64_enabled(dv.out, false);

  ret = convert_293_to_292(&dv);
  out = NULL;
  in = NULL;

  switch(ret)
  {
    case ARCHIVE_ERROR: goto err_zip;
    case SEEK_ERROR:    goto err_seek;
    case READ_ERROR:    goto err_read;
    case WRITE_ERROR:   goto err_write;
    case SUCCESS:       break;
  }

  fprintf(stdout,
   "File '%s' successfully downgraded from %d.%d to %d.%d.\n"
   "Saved to '%s'.\n",
   argv[1], MZX_VERSION_HI, MZX_VERSION_LO,
   MZX_VERSION_PREV_HI, MZX_VERSION_PREV_LO, fname);

exit_close:
  if(out)
    vfclose(out);
  if(in)
    vfclose(in);

exit_out:
  return 0;

err_zip:
  error("Error opening world archive, aborting.\n")
  goto exit_close;

err_seek:
  error("Seek error, aborting.\n");
  goto exit_close;

err_read:
  error("Read error, aborting.\n");
  goto exit_close;

err_write:
  error("Write error, aborting.\n");
  goto exit_close;
}
