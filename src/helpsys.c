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

// internal help system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "data.h"
#include "graphics.h"
#include "helpsys.h"
#include "scrdisp.h"
#include "window.h"
#include "world.h"
#include "io/vio.h"

static char *help;

void help_open(struct world *mzx_world, const char *file_name)
{
  if(file_name)
  {
    mzx_world->help_file = vfopen_unsafe(file_name, "rb");
    if(!mzx_world->help_file)
      return;

    help = cmalloc(1024 * 64);
  }
}

void help_close(struct world *mzx_world)
{
  if(mzx_world->help_file)
  {
    vfclose(mzx_world->help_file);
    mzx_world->help_file = NULL;
  }

  free(help);
  help = NULL;
}

/**
 * This really ought to only take the context, but this function is still used
 * outside of core.c in the following places:
 *
 * src/editor/char_ed.c
 * src/editor/robo_ed.c
 * src/window.c (dialogs)
 */
void help_system(context *ctx, struct world *mzx_world)
{
  char file[13], file2[13], label[13];
  int where, offs, size, t1, t2;
  vfile *vf;

  vf = mzx_world->help_file;
  if(!vf)
    return;

  vrewind(vf);
  t1 = vfgetw(vf);
  vfseek(vf, t1 * 21 + 4 + get_context(ctx) * 12, SEEK_SET);

  // At proper context info
  where = vfgetd(vf);    // Where file to load is
  size = vfgetd(vf);     // Size of file to load
  offs = vfgetd(vf);     // Offset within file of link

  // Jump to file
  vfseek(vf, where, SEEK_SET);
  // Read it in
  size = vfread(help, 1, size, vf);
  // Display it
  cursor_off();

labelled:
  help_display(mzx_world, help, offs, file, label);

  // File?
  if(file[0])
  {
    // Yep. Search for file.
    vfseek(vf, 2, SEEK_SET);
    for(t2 = 0; t2 < t1; t2++)
    {
      if(!vfread(file2, 13, 1, vf))
        return;
      if(!strcmp(file, file2))
        break;
      vfseek(vf, 8, SEEK_CUR);
    }

    if(t2 < t1)
    {
      // Found file.
      where = vfgetd(vf);
      size = vfgetd(vf);
      vfseek(vf, where, SEEK_SET);
      size = vfread(help, 1, size, vf);

      // Search for label
      for(t2 = 0; t2 < size; t2++)
      {
        if(help[t2] != 255)
          continue;
        if(help[t2 + 1] != ':')
          continue;
        if(!strcmp(help + t2 + 3, label))
          break; // Found label!
      }

      if(t2 < size)
      {
        // Found label. t2 is offset.
        offs = t2;
        goto labelled;
      }
    }
  }
}
