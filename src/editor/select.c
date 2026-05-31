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

/* Selection dialogs */
/* Prior to 2.91, this was block.c */

#include "edit.h"
#include "select.h"

#include "../core.h"
#include "../data.h"
#include "../error.h"
#include "../graphics.h"
#include "../idarray.h"
#include "../idput.h"
#include "../util.h"
#include "../window.h"
#include "../world_struct.h"

#include "../event/event.h"

#include <stdio.h>

int select_screen_mode(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[4];
  struct dialog di;

  int current_mode = get_screen_mode();
  int new_mode = current_mode;

  const char *radio_button_strings[] =
  {
    "Regular MZX      - 16 color palette",
    "Super MZX Mode 1 - 16 color palette, interpolated",
    "Super MZX Mode 2 - 256 color palette, fixed indexing",
    "Super MZX Mode 3 - 256 color palette, variable indexing",
  };

  const char *help_message =
   "~9  For additional information on the SMZX modes, press F1.";

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_SUPER_MEGAZEUX);

  do
  {
    elements[0] = construct_radio_button(2, 2, radio_button_strings,
     4, 55, &new_mode);
    elements[1] = construct_button(21, 9, "OK", 0);
    elements[2] = construct_button(35, 9, "Cancel", -1);
    elements[3] = construct_label(2, 7, help_message);

    construct_dialog(&di, "Select Screen Mode", 8, 5, 64, 12,
     elements, 4, 0);

    dialog_result = run_dialog(mzx_world, &di);

    if(dialog_result == 0 && current_mode == 3 && new_mode != 3 &&
     confirm(mzx_world, "This will delete all palette index data. Proceed?"))
    {
      dialog_result = 1;
    }

    destruct_dialog(&di);
  }
  while(dialog_result > 0);

  if(new_mode == current_mode || dialog_result == -1)
    new_mode = -1;

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  pop_context();

  return new_mode;
}

int choose_char_set(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[4];
  struct dialog di;
  int charset_type = 0;
  const char *radio_button_strings[] =
  {
    "MegaZeux default",
    "ASCII set",
    "SMZX Classic set",
    "SMZX Modern set",
    "Blank set"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CHOOSE_CHARSET);
  elements[0] = construct_radio_button(4, 6, radio_button_strings,
   ARRAY_SIZE(radio_button_strings), 16, &charset_type);
  elements[1] = construct_button(5, 12, "OK", 0);
  elements[2] = construct_button(15, 12, "Cancel", -1);
  elements[3] = construct_label(4, 2,
   "~9Note: this will clear\nany changes made to\nthe character set.");

  construct_dialog(&di, "Select Character Set", 26, 3, 28, 15,
   elements, ARRAY_SIZE(elements), 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return charset_type;
}

int export_type(struct world *mzx_world)
{
  int export_choice = 0;
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  const char *radio_strings[] =
  {
    "Board file (MZB)",
    "Character set (CHR)",
    "Palette (PAL)",
    "Sound effects (SFX)",
    "Downver. world (MZX)",
    "Board/vlayer image",
  };
  int num_choices = 6;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_IMPORTEXPORT_TYPE);

  elements[0] = construct_radio_button(2, 8 - num_choices, radio_strings,
   num_choices, 20, &export_choice);
  elements[1] = construct_button(5, 9, "OK", 0);
  elements[2] = construct_button(15, 9, "Cancel", -1);

  construct_dialog(&di, "Export as:", 26, 5, 28, 12,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return export_choice;
}

int import_type(struct world *mzx_world)
{
  int import_choice = 0;
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  const char *radio_strings[] =
  {
    "Board file (MZB)",
    "Character set (CHR)",
    "World file (MZX)",
    "Palette (PAL)",
    "Sound effects (SFX)",
    "ANSi/TXT (choose pos.)",
    "MZM (choose pos.)"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_IMPORTEXPORT_TYPE);

  elements[0] = construct_radio_button(2, 2, radio_strings,
   ARRAY_SIZE(radio_strings), 23, &import_choice);
  elements[1] = construct_button(6, 10, "OK", 0);
  elements[2] = construct_button(16, 10, "Cancel", -1);

  construct_dialog(&di, "Import:", 25, 4, 30, 13,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return import_choice;
}
