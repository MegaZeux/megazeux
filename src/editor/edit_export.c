/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

/* Misc. export functionality not better suited to other places. */

#include "edit_export.h"

#include "../core_task.h"
#include "../error.h"
#include "../graphics.h"
#include "../idput.h"

#include <string.h>

static boolean export_image_status_callback(void *priv, size_t progress,
 size_t progress_max)
{
  return core_task_tick((context *)priv, progress, progress_max, NULL);
}

struct export_image_task_data
{
  char *filename;
  unsigned width_ch;
  unsigned height_ch;
  struct char_element *layer;
};

static boolean export_image_task_run(context *ctx, void *priv)
{
  struct export_image_task_data *data = (struct export_image_task_data *)priv;

  return dump_layer_to_image(data->filename,
   data->width_ch, data->height_ch, data->layer,
   export_image_status_callback, ctx);
}

static void export_image_task_complete(void *priv, boolean ret)
{
  struct export_image_task_data *data = (struct export_image_task_data *)priv;
  free(data->filename);
  free(data->layer);
  free(data);

  if(!ret)
    error_message(E_IMAGE_EXPORT_CANCELED, 0, NULL);
}

static void export_image_task(context *parent, const char *filename,
 unsigned width_ch, unsigned height_ch, struct char_element *layer)
{
  struct export_image_task_data *d;
  char *name;
  size_t len = strlen(filename);
  char title[80];

  d = (struct export_image_task_data *)cmalloc(sizeof(struct export_image_task_data));
  name = (char *)cmalloc(len + 1);
  if(!d || !name)
  {
    free(d);
    free(name);
    error_message(E_IMAGE_EXPORT, 0, NULL);
    return;
  }

  d->filename = name;
  d->width_ch = width_ch;
  d->height_ch = height_ch;
  d->layer = layer;

  memcpy(d->filename, filename, len + 1);
  if(len > 55)
  {
    filename += len - 52;
    snprintf(title, sizeof(title), "Saving ...%.52s", filename);
  }
  else
    snprintf(title, sizeof(title), "Saving %.55s", filename);

  core_task_context(parent, title, export_image_task_run,
   export_image_task_complete, d);
}

/**
 * Export a board to an image file.
 */
void export_board_image(context *parent, struct board *src_board,
 const char *file)
{
  struct char_element *image;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int overlay_mode = src_board->overlay_mode & OVERLAY_MODE_MASK;
  size_t offset;
  size_t sz;
  int ch, co;
  int x, y;

  sz = (size_t)board_width * board_height;
  image = (struct char_element *)cmalloc(sz * sizeof(struct char_element));
  if(!image)
    return;

  offset = 0;
  for(y = 0; y < board_height; y++)
  {
    for(x = 0; x < board_width; x++, offset++)
    {
      /* simplified version of id_put */
      if(overlay_mode == OVERLAY_ON || overlay_mode == OVERLAY_STATIC)
      {
        ch = src_board->overlay[offset];
        co = src_board->overlay_color[offset];
        if(ch != 32)
        {
          image[offset].char_value = ch;
          image[offset].fg_color = co & 0xf;

          if(co & 0xf0)
            image[offset].bg_color = co >> 4;
          else
            image[offset].bg_color = get_id_color(src_board, offset) >> 4;

          continue;
        }
      }

      ch = get_id_char(src_board, offset);
      co = get_id_color(src_board, offset);
      image[offset].char_value = ch;
      image[offset].fg_color = co & 0xf;
      image[offset].bg_color = co >> 4;
    }
  }

  export_image_task(parent, file, board_width, board_height, image);
}

/**
 * Export the vlayer to an image file.
 */
void export_vlayer_image(context *parent, struct world *mzx_world,
 const char *file)
{
  struct char_element *image;
  int board_width = mzx_world->vlayer_width;
  int board_height = mzx_world->vlayer_height;
  size_t offset;
  size_t sz;
  int x, y;

  sz = (size_t)board_width * board_height;
  image = (struct char_element *)cmalloc(sz * sizeof(struct char_element));
  if(!image)
    return;

  offset = 0;
  for(y = 0; y < board_height; y++)
  {
    for(x = 0; x < board_width; x++, offset++)
    {
      int color = mzx_world->vlayer_colors[offset];
      image[offset].char_value = mzx_world->vlayer_chars[offset];
      image[offset].fg_color = color & 0x0f;
      image[offset].bg_color = color >> 4;
    }
  }

  export_image_task(parent, file, board_width, board_height, image);
}
