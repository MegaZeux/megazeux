/* MegaZeux
 *
 * Copyright (C) 2018-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdarg.h>
#include <string.h>

#include "caption.h"
#include "graphics.h"
#include "world_struct.h"
#include "platform/platform_attribute.h" /* ATTRIBUTE_PRINTF */

// TODO this might benefit from a dirty flag + graphics.c hook and/or
// integration into mzx_world to remove the statics.

#define MAX_CAPTION_SIZE 120
#define CAPTION_SPACER " :: "

static char caption_world[MAX_CAPTION_SIZE];
static char caption_board[MAX_CAPTION_SIZE];
static char caption_robot[MAX_CAPTION_SIZE];
static int caption_robot_x;
static int caption_robot_y;
static boolean caption_editing;
static boolean caption_modified;
#ifdef CONFIG_UPDATER
static boolean caption_updates;
#endif
#ifdef CONFIG_FPS
static double caption_fps;
#endif

/**
 * Strip a string for display in the caption.
 */

static boolean strip_caption_string(char *output, const char *input)
{
  int len = strlen(input);
  int i, j;
  output[0] = '\0';

  for(i = 0, j = 0; i < len; i++)
  {
    if(input[i] < 32 || input[i] > 126)
      continue;

    if(input[i] == '~' || input[i] == '@')
    {
      i++;
      if(input[i - 1] != input[i])
        continue;
    }

    output[j] = input[i];

    if(output[j] != ' ' || (j > 0 && output[j - 1] != ' '))
      j++;
  }

  if(j > 0 && output[j - 1] == ' ')
    j--;

  output[j] = '\0';
  return (j > 0);
}

/**
 * Append a string to the caption.
 */
ATTRIBUTE_PRINTF(2, 3)
static inline void caption_append(char caption[MAX_CAPTION_SIZE],
 const char *f, ...)
{
  size_t len = strlen(caption);

  if(len < MAX_CAPTION_SIZE - 1)
  {
    va_list args;
    va_start(args, f);

    vsnprintf(caption + len, MAX_CAPTION_SIZE - len, f, args);

    va_end(args);
  }
}

/**
 * Sets the caption to reflect current MegaZeux state information.
 */

static void update_caption(void)
{
  char caption[MAX_CAPTION_SIZE];
  caption[0] = '\0';

  if(caption_modified)
    strcpy(caption, "*");

  if(caption_robot[0])
    caption_append(caption, "%s (%i,%i)" CAPTION_SPACER,
     caption_robot, caption_robot_x, caption_robot_y);

  if(caption_board[0])
    caption_append(caption, "%s" CAPTION_SPACER, caption_board);

  if(caption_world[0])
    caption_append(caption, "%s" CAPTION_SPACER, caption_world);

  caption_append(caption, "%s", video_get_default_caption());

  if(caption_editing)
    caption_append(caption, " (editor)");

#ifdef CONFIG_UPDATER
  if(caption_updates)
    caption_append(caption, " *** UPDATES AVAILABLE ***");
#endif

#ifdef CONFIG_FPS
  if(caption_fps > 0.001)
    caption_append(caption, CAPTION_SPACER "FPS: %.2f", caption_fps);
#endif /* CONFIG_FPS */

  caption[MAX_CAPTION_SIZE - 1] = 0;
  video_set_window_caption(NULL, caption);
}

static void set_caption_world_string(struct world *mzx_world)
{
  if(mzx_world && mzx_world->active)
  {
    if(!strip_caption_string(caption_world, mzx_world->name))
      strcpy(caption_world, "Untitled world");
  }
  else
  {
    caption_world[0] = '\0';
  }
}

static void set_caption_board_string(struct board *board)
{
  if(board)
  {
    if(!strip_caption_string(caption_board, board->board_name))
      strcpy(caption_board, "Untitled board");
  }
  else
  {
    caption_board[0] = '\0';
  }
}

static void set_caption_robot_string(struct robot *robot)
{
  if(robot)
  {
    if(!strip_caption_string(caption_robot, robot->robot_name))
      strcpy(caption_robot, "Untitled robot");

    caption_robot_x = robot->xpos;
    caption_robot_y = robot->ypos;
  }
  else
  {
    caption_robot[0] = '\0';
  }
}

void caption_set_world(struct world *mzx_world)
{
  set_caption_world_string(mzx_world);
  set_caption_board_string(NULL);
  set_caption_robot_string(NULL);
  caption_modified = false;
  caption_editing = false;
  update_caption();
}

#ifdef CONFIG_EDITOR
void caption_set_board(struct world *mzx_world, struct board *board)
{
  set_caption_world_string(mzx_world);
  set_caption_board_string(board);
  set_caption_robot_string(NULL);
  caption_editing = true;
  update_caption();
}

void caption_set_robot(struct world *mzx_world, struct robot *robot)
{
  set_caption_world_string(mzx_world);
  set_caption_board_string(mzx_world->current_board);
  set_caption_robot_string(robot);
  caption_editing = true;
  update_caption();
}

void caption_set_modified(boolean modified)
{
  caption_modified = modified;
  caption_editing = true;
  update_caption();
}
#endif

#ifdef CONFIG_UPDATER
void caption_set_updates_available(boolean available)
{
  caption_updates = available;
  update_caption();
}
#endif

#ifdef CONFIG_FPS
void caption_set_fps(double fps)
{
  caption_fps = fps;
  update_caption();
}
#endif
