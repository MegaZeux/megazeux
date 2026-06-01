/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2023-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

// Code to edit (via edit dialog) and test custom sound effects

#include <assert.h>
#include <string.h>

#include "../core.h"
#include "../data.h"
#include "../error.h"
#include "../sfx.h"
#include "../window.h"
#include "../world.h"
#include "../util.h"
#include "../io/vio.h"
#include "../event/event.h"

#include "sfx_edit.h"
#include "window.h"

// 8 char names per sfx

static const char sfx_names[NUM_BUILTIN_SFX][10] =
{
  "Gem      ",
  "MagicGem ",
  "Health   ",
  "Ammo     ",
  "Coin     ",
  "Life     ",
  "Lo Bomb  ",
  "Hi Bomb  ",
  "Key      ",
  "FullKeys ",
  "Unlock   ",
  "Need Key ",
  "InvsWall ",
  "Forest   ",
  "GateLock ",
  "OpenGate ",
  "Invinco1 ",
  "Invinco2 ",
  "Invinco3 ",
  "DoorLock ",
  "OpenDoor ",
  "Ouch     ",
  "Lava     ",
  "Death    ",
  "GameOver ",
  "GateClse ",
  "Push     ",
  "Trnsport ",
  "Shoot    ",
  "Break    ",
  "NeedAmmo ",
  "Ricochet ",
  "NeedBomb ",
  "Place Lo ",
  "Place Hi ",
  "BombType ",
  "Explsion ",
  "Entrance ",
  "Pouch    ",
  "Potion   ",
  "EmtyChst ",
  "Chest    ",
  "Time Out ",
  "FireOuch ",
  "StolenGm ",
  "EnemyHit ",
  "DrgnFire ",
  "Scroll   ",
  "Goop     ",
  "Unused   "
};

static const char *const sfx_ext[] = { ".SFX", NULL };

//--------------------------
//
// ( ) Default internal SFX
// ( ) Custom SFX
//
//    _OK_      _Cancel_
//
//--------------------------


//(full screen)
//
//|--
//|
//|Explsion_________//_____|
//|MagicGem_________//_____|
//|etc.    _________//_____|
//|
//|
//|
//| Next Prev Done
//|
//|--

// 17 on pg 1/2, 16 on pg 3

static const int offsets[] = { 0, 17, 34, 50, 67, 84 };
static const int counts[] = { 17, 17, 16, 17, 17, 16 };

// FIXME
static int page = 0;
static char labels[17][12];

static void update_label(const struct custom_sfx *sfx, int pos, int i)
{
  labels[i][0] = '\0';
  if(sfx && sfx->label[0])
    snprintf(labels[i], sizeof(labels[i]), "%-9.9s", sfx->label);

  if(labels[i][0] == '\0')
  {
    if(pos < NUM_BUILTIN_SFX)
      snprintf(labels[i], sizeof(labels[i]), "%-9.9s", sfx_names[pos]);
    else
      snprintf(labels[i], sizeof(labels[i]), "%-9d", pos);
  }
}

static int sfx_edit_idle(struct world *mzx_world, struct dialog *di, int key)
{
  struct sfx_list *sfx_list = &mzx_world->custom_sfx;
  const struct custom_sfx *sfx;
  char title[32];
  char tmp[12];
  int pos;

  if(di->current_element >= counts[page])
    return key;

  switch(key)
  {
    case IKEY_F3:
      pos = offsets[page] + di->current_element;
      sfx = sfx_get(sfx_list, pos);
      if(sfx)
        memcpy(tmp, sfx->label, sizeof(sfx->label));
      else
        tmp[0] = '\0';

      snprintf(title, sizeof(title), "Name for SFX %d:", pos);
      if(input_window(mzx_world, title, tmp, 9))
        return 0;

      sfx_set_label(sfx_list, pos, tmp, strlen(tmp));
      // SFX may have been (re)allocated, fetch again.
      update_label(sfx_get(sfx_list, pos), pos, di->current_element);
      return 0;
  }
  return key;
}

void sfx_edit(struct world *mzx_world)
{
  // First, ask whether sfx should be INTERNAL or CUSTOM. If change is made
  // from CUSTOM to INTERNAL, verify deletion of CUSTOM sfx. If changed to
  // CUSTOM, copy over INTERNAL to CUSTOM. If changed to or left at CUSTOM,
  // go to sfx editing.
  struct dialog a_di, b_di;
  struct sfx_list *custom_sfx = &mzx_world->custom_sfx;
  int i;
  int pos;
  int new_sfx_mode = !!custom_sfx->list;
  int dialog_result;
  int num_sfx;
  int num_elements;
  const char *radio_strings[] =
  {
    "Default internal SFX", "Custom SFX"
  };
  struct element *a_elements[3] =
  {
    construct_radio_button(2, 2, radio_strings, 2, 20,
     &new_sfx_mode),
    construct_button(5, 5, "OK", 0),
    construct_button(15, 5, "Cancel", -1)
  };

  struct element *b_elements[21];

  char buffers[17][LEGACY_SFX_SIZE];

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_SFX_EDITOR);

  construct_dialog(&a_di, "Choose SFX mode", 26, 7, 28, 8,
   a_elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &a_di);

  if(!dialog_result)
  {
    // Custom to internal
    if(custom_sfx->list && !new_sfx_mode)
    {
      if(!confirm(mzx_world, "Delete current custom SFX?"))
        sfx_free(custom_sfx);
    }
    else

    // Internal to custom
    if(!custom_sfx->list && new_sfx_mode)
    {
      for(i = 0; i < NUM_BUILTIN_SFX; i++)
        sfx_set_string(custom_sfx, i, sfx_strs[i], strlen(sfx_strs[i]));
    }

    // Edit custom
    if(custom_sfx->list)
    {
      do
      {
        // Setup page
        num_elements = counts[page];
        num_sfx = num_elements;

        memset(buffers, 0, sizeof(buffers));
        for(i = 0, pos = offsets[page]; i < num_sfx; i++, pos++)
        {
          const struct custom_sfx *sfx = sfx_get(custom_sfx, pos);

          update_label(sfx, pos, i);
          if(sfx)
            snprintf(buffers[i], sizeof(buffers[i]), "%s", sfx->string);

          b_elements[i] = construct_input_box(1, i + 2,
           labels[i], LEGACY_SFX_SIZE - 1, buffers[i]);
        }

        b_elements[i] = construct_label(13, 20,
         "Press Alt+T to test a sound effect. Press F3 to rename.");
        b_elements[i + 1] = construct_button(16, 22, "Next", 1);
        b_elements[i + 2] = construct_button(36, 22, "Previous", 2);
        b_elements[i + 3] = construct_button(60, 22, "Done", 0);

        num_elements += 4;

        construct_dialog_ext(&b_di, "Edit custom SFX", 0, 0, 80, 25,
         b_elements, num_elements, 1, 0, 0, sfx_edit_idle);
        // Run dialog
        dialog_result = run_dialog(mzx_world, &b_di);
        destruct_dialog(&b_di);

        // Commit changes...
        for(i = 0, pos = offsets[page]; i < num_sfx; i++, pos++)
          sfx_set_string(custom_sfx, pos, buffers[i], strlen(buffers[i]));

        // Next/prev page?
        switch(dialog_result)
        {
          case 1:
          {
            page++;
            if(page > 5)
              page = 0;
            break;
          }

          case 2:
          {
            page--;
            if(page < 0)
              page = 5;
            break;
          }
        }
      } while(dialog_result > 0);
    }
  }

  destruct_dialog(&a_di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  // Done!
  pop_context();
}

/**
 * Import world SFX strings.
 */
void import_sfx(context *parent, boolean *modified)
{
  struct world *mzx_world = parent->world;
  struct sfx_list *custom_sfx = &mzx_world->custom_sfx;
  char import_name[MAX_PATH];
  import_name[0] = '\0';

  if(!choose_file(mzx_world, sfx_ext, import_name,
   "Choose SFX file to import", ALLOW_ALL_DIRS))
  {
    char *tmp;
    vfile *sfx_file;
    int64_t len;
    size_t n;

    sfx_file = vfopen_unsafe(import_name, "rb");
    if(!sfx_file)
      goto err;

    len = vfilelength(sfx_file, true);
    if(len < 0 || len > 1024 * 1024)
    {
      vfclose(sfx_file);
      goto err;
    }

    tmp = (char *)cmalloc(len);
    if(!tmp)
    {
      vfclose(sfx_file);
      goto err;
    }

    n = vfread(tmp, 1, len, sfx_file);
    vfclose(sfx_file);

    if(!sfx_load_from_memory(custom_sfx, MZX_VERSION, tmp, n))
      goto err;

    *modified = true;
    return;
err:
    error_message(E_SFX_IMPORT, 0, NULL);
  }
}

/**
 * Export world SFX strings.
 */
void export_sfx(context *parent)
{
  struct world *mzx_world = parent->world;
  const struct sfx_list *custom_sfx = &mzx_world->custom_sfx;
  struct element *elements[1];
  const char *opt_names[1] = { "Legacy export (for versions <2.93)" };
  int results[1];
  char export_name[MAX_PATH];
  export_name[0] = '\0';

  results[0] = false;
  elements[0] = construct_check_box(2, 21, opt_names, 1, 40, results);

  if(!file_manager(mzx_world, sfx_ext, ".sfx", export_name,
   "Export SFX file", ALLOW_ALL_DIRS, ALLOW_NEW_FILES,
   elements, ARRAY_SIZE(elements), 1))
  {
    vfile *sfx_file;
    const char *out;
    char *tmp = NULL;
    size_t size;
    size_t size_written;
    int ver = results[0] ? V251 : MZX_VERSION;

    if(custom_sfx->list)
    {
      size_t required = 0;
      size = sfx_save_to_memory(custom_sfx, ver, NULL, 0, &required);

      if(!required)
        goto err;

      tmp = (char *)cmalloc(required);
      if(!tmp)
        goto err;

      out = tmp;
      size = sfx_save_to_memory(custom_sfx, ver, tmp, required, NULL);
      if(size == 0)
        goto err;
    }
    else
    {
      // TODO: hack relies on sfx_strs matching the legacy format.
      out = (const char *)sfx_strs;
      size = sizeof(sfx_strs);
      assert(size == NUM_BUILTIN_SFX * LEGACY_SFX_SIZE);
    }

    sfx_file = vfopen_unsafe(export_name, "wb");
    if(!sfx_file)
      goto err;

    size_written = vfwrite(out, 1, size, sfx_file);
    vfclose(sfx_file);

    if(size_written < size)
      error_message(E_IO_WRITE, 0, NULL);

    free(tmp);
    return;
err:
    error_message(E_SFX_EXPORT, 0, NULL);
    free(tmp);
    return;
  }
}
