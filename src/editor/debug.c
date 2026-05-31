/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "debug.h"

#include "char_ed.h"
#include "pal_ed.h"
#include "robo_debug.h"
#include "stringsearch.h"
#include "window.h"

#include "../core.h"
#include "../counter.h"
#include "../extmem.h"
#include "../graphics.h"
#include "../intake.h"
#include "../memcasecmp.h"
#include "../scrdisp.h"
#include "../sprite.h"
#include "../str.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"
#include "../io/vio.h"

#include "../audio/audio.h"
#include "../event/event.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define TREE_LIST_X 62
#define TREE_LIST_Y 2
#define TREE_LIST_WIDTH 16
#define TREE_LIST_HEIGHT 15
#define VAR_LIST_X 2
#define VAR_LIST_Y 2
#define VAR_LIST_WIDTH 58
#define VAR_LIST_HEIGHT 21
#define BUTTONS_X 62
#define BUTTONS_Y 18
#define CVALUE_SIZE 11
#define LVALUE_SIZE 21
#define SVALUE_SIZE 40
#define CVALUE_COL_OFFSET (VAR_LIST_WIDTH - CVALUE_SIZE - 1)
#define LVALUE_COL_OFFSET (VAR_LIST_WIDTH - LVALUE_SIZE - 1)
#define SVALUE_COL_OFFSET (VAR_LIST_WIDTH - SVALUE_SIZE - 1)

#define VAR_SEARCH_DIALOG_X 4
#define VAR_SEARCH_DIALOG_Y 9
#define VAR_SEARCH_DIALOG_W 71
#define VAR_SEARCH_DIALOG_H 5
#define VAR_SEARCH_MAX 48
#define VAR_SEARCH_NAMES    1
#define VAR_SEARCH_VALUES   2
#define VAR_SEARCH_CASESENS 4
#define VAR_SEARCH_REVERSE  8
#define VAR_SEARCH_WRAP     16
#define VAR_SEARCH_LOCAL    32
#define VAR_SEARCH_EXACT    64

#define VAR_ADD_DIALOG_X 20
#define VAR_ADD_DIALOG_Y 8
#define VAR_ADD_DIALOG_W 40
#define VAR_ADD_DIALOG_H 6
#define VAR_ADD_MAX 30

static const char asc[17] = "0123456789ABCDEF";

// Escape \n. Use for most debug var name/text fields. This is intended for
// display only and doesn't escape \. For anything that needs to be edited
// or unescaped, use copy_substring_escaped instead. FIXME it'd be nice if
// list box elements could print these chars instead of making this necessary.
static void copy_name_escaped(char *dest, size_t dest_len, const char *src,
 size_t src_len)
{
  unsigned int i, j, left;

  for(i = 0, j = 0; j < src_len; i++, j++)
  {
    left = dest_len - i;
    if(src[j] == '\n')
    {
      if(left < 3)
        break;

      dest[i++] = '\\';
      dest[i] = 'n';
    }
    else
    {
      if(left < 2)
        break;

      dest[i] = src[j];
    }
  }

  dest[i] = 0;
}

// Escape all non-ASCII chars. Use for string values.
static void copy_substring_escaped(char *dest, size_t dest_len, const char *src,
 size_t src_len)
{
  unsigned int i, j, left;

  for(i = 0, j = 0; j < src_len; i++, j++)
  {
    left = dest_len - i;
    if(src[j] == '\\')
    {
      if(left < 3)
        break;

      dest[i++] = '\\';
      dest[i] = '\\';
    }
    else

    if(src[j] == '\n')
    {
      if(left < 3)
        break;

      dest[i++] = '\\';
      dest[i] = 'n';
    }
    else

    if(src[j] == '\r')
    {
      if(left < 3)
        break;

      dest[i++] = '\\';
      dest[i] = 'r';
    }
    else

    if(src[j] == '\t')
    {
      if(left < 3)
        break;

      dest[i++] = '\\';
      dest[i] = 't';
    }
    else

    if(src[j] < 32 || src[j] > 126)
    {
      if(left < 5)
        break;

      dest[i++] = '\\';
      dest[i++] = 'x';
      dest[i++] = asc[src[j] >> 4];
      dest[i] = asc[src[j] & 15];
    }
    else
    {
      if(left < 2)
        break;

      dest[i] = src[j];
    }
  }

  dest[i] = 0;
}

// Unescape a string escaped by copy_substring_escaped.
static void unescape_string(char *buf, int *len)
{
  size_t i = 0, j, old_len = strlen(buf);
  char tmp[3];

  for(j = 0; j < old_len; i++, j++)
  {
    if(buf[j] != '\\')
    {
      buf[i] = buf[j];
      continue;
    }

    j++;

    if(j == old_len)
      break;

    switch(buf[j])
    {
      case 'n':
        buf[i] = '\n';
        break;
      case 'r':
        buf[i] = '\r';
        break;
      case 't':
        buf[i] = '\t';
        break;
      case '\\':
        buf[i] = '\\';
        break;

      case 'x':
        if(j + 2 >= old_len)
        {
          j = old_len;
          break;
        }
        tmp[0] = buf[j + 1];
        tmp[1] = buf[j + 2];
        tmp[2] = '\0';
        buf[i] = strtol(tmp, NULL, 16);
        j += 2;
        break;

      default:
        buf[i] = buf[j];
        break;
    }
  }

  (*len) = i;
}

/***********************
 * Var reading/setting *
 ***********************/

enum virtual_var
{
  VIR_RAM_COUNTER_LIST,
  VIR_RAM_COUNTER_TABLE,
  VIR_RAM_COUNTERS,
  VIR_RAM_STRING_LIST,
  VIR_RAM_STRING_TABLE,
  VIR_RAM_STRINGS,
  VIR_RAM_SFX,
  VIR_RAM_SPRITES,
  VIR_RAM_VLAYER,
  VIR_RAM_BOARD_INFO,
  VIR_RAM_BOARD_DATA,
  VIR_RAM_BOARD_OVERLAY,
  VIR_RAM_ROBOT_INFO,
  VIR_RAM_ROBOT_STACK,
  VIR_RAM_ROBOT_SOURCE,
  VIR_RAM_ROBOT_PROGRAMS,
  VIR_RAM_ROBOT_PROGRAM_LABELS,
  VIR_RAM_SCROLLS_SENSORS,
  VIR_RAM_VIDEO_LAYERS,
  VIR_RAM_DEBUGGER_ROBOTS,
  VIR_RAM_DEBUGGER_VARIABLES,
  VIR_RAM_EXTRAM_DELTA,
  VIR_RAM_VIRTUAL_FILESYSTEM,
  VIR_RAM_VIRTUAL_FILESYSTEM_CACHED_ONLY,
};

static const char * const virtual_var_names[] =
{
  "Counter list*",
  "Counter table*",
  "Counters*",
  "String list*",
  "String table*",
  "Strings*",
  "Custom SFX*",
  "Sprites*",
  "Vlayer*",
  "Board info*",
  "Board data*",
  "Board overlay*",
  "Robot info*",
  "Robot stack*",
  "Robot program source*",
  "Robot program bytecode*",
  "Robot program labels*",
  "Scrolls and sensors*",
  "Video layers*",
  "Debug (robots)*",
  "Debug (variables)*",
  "ExtRAM compression delta*",
  "Virtual filesystem (total)*",
  "Virtual filesystem (cached only)*",
};

// We'll read off of these when we construct the tree
static const char *universal_var_list[] =
{
  "random_seed0",
  "random_seed1",
  "date_year*",
  "date_month*",
  "date_day*",
  "date_weekday*",
  "time_hours*",
  "time_minutes*",
  "time_seconds*",
  "time_millis*",
};

static const char *world_var_list[] =
{
  "bimesg", //no read
  "c_divisions",
  "commands",
  "commands_stop",
  "divider",
  "fread_delimiter", //no read
  "fwrite_delimiter", //no read
  "joy_simulate_keys", //no read
  "max_samples",
  "mod_frequency",
  "mod_length*",
  "mod_loopend",
  "mod_loopstart",
  "mod_name*",
  "mod_order",
  "mod_position",
  "multiplier",
  "mzx_speed",
  "smzx_message",
  "smzx_mode*",
  "spacelock", //no read
  "vlayer_size*",
  "vlayer_width*",
  "vlayer_height*",
};

static const enum virtual_var world_ram_var_list[] =
{
  VIR_RAM_COUNTER_LIST,
  VIR_RAM_COUNTER_TABLE,
  VIR_RAM_COUNTERS,
  VIR_RAM_STRING_LIST,
  VIR_RAM_STRING_TABLE,
  VIR_RAM_STRINGS,
  VIR_RAM_SFX,
  VIR_RAM_SPRITES,
  VIR_RAM_VLAYER,
  VIR_RAM_BOARD_INFO,
  VIR_RAM_BOARD_DATA,
  VIR_RAM_BOARD_OVERLAY,
  VIR_RAM_ROBOT_INFO,
  VIR_RAM_ROBOT_STACK,
#ifdef CONFIG_DEBYTECODE
  VIR_RAM_ROBOT_SOURCE,
#endif
  VIR_RAM_ROBOT_PROGRAMS,
  VIR_RAM_ROBOT_PROGRAM_LABELS,
  VIR_RAM_SCROLLS_SENSORS,
  VIR_RAM_VIDEO_LAYERS,
  VIR_RAM_DEBUGGER_ROBOTS,
  VIR_RAM_DEBUGGER_VARIABLES,
#ifdef CONFIG_EXTRAM
  VIR_RAM_EXTRAM_DELTA,
#endif
  VIR_RAM_VIRTUAL_FILESYSTEM,
  VIR_RAM_VIRTUAL_FILESYSTEM_CACHED_ONLY,
};

static const char *board_var_list[] =
{
  "board_name*",
  "board_mod*", //no read
  "board_w*",
  "board_h*",
  "input", // as a counter
  "input*", // as a string (no read function; handled in interpolation code)
  "inputsize",
  "key",
  "overlay_mode*",
  "playerfacedir",
  "playerlastdir",
  "playerx*",
  "playery*",
  "scrolledx*",
  "scrolledy*",
  "timereset",
  "volume", //no read or write
};

// Locals are added onto the end later.
static const char *robot_var_list[] =
{
  "robot_name*",
  "commands_total", //no read or write
  "commands_cycle*", //no read
  "bullettype",
  "lava_walk",
  "goop_walk",
  "lockself", //no read or write
  "loopcount",
  "thisx*",
  "thisy*",
  "this_char*",
  "this_color*",
  "playerdist*",
  "horizpld*",
  "vertpld*",
};

// Sprite parent list (note: clist# added to end)
static const char *sprite_parent_var_list[] =
{
  "Active sprites*", // no read/write
  "spr_num",
  "spr_yorder",
  "spr_collisions*",
};

// The following will all be added to the end of 'sprN_'
static const char *sprite_var_list[] =
{
  "x",
  "y",
  "z",
  "refx",
  "refy",
  "width",
  "height",
  "cx",
  "cy",
  "cwidth",
  "cheight",
  "ccheck", // no read
  "off",
  "offonexit",
  "offset",
  "overlay", // no read
  "static", // no read
  "tcol",
  "unbound",
  "vlayer", // no read
};

static const char *sensor_var_list[] =
{
  "Sensor name*", // no read/write
  "Robot to mesg*", // no read/write
};

static const char *scroll_text_var = "Scroll text*";

static const int num_universal_vars = ARRAY_SIZE(universal_var_list);
static const int num_world_vars = ARRAY_SIZE(world_var_list);
static const int num_world_ram_vars = ARRAY_SIZE(world_ram_var_list);
static const int num_board_vars = ARRAY_SIZE(board_var_list);
static const int num_robot_vars = ARRAY_SIZE(robot_var_list);
static const int num_sprite_parent_vars = ARRAY_SIZE(sprite_parent_var_list);
static const int num_sprite_vars = ARRAY_SIZE(sprite_var_list);
static const int num_sensor_vars = ARRAY_SIZE(sensor_var_list);

enum debug_var_type
{
  V_COUNTER,
  V_STRING,
  V_VAR,
  V_VIRTUAL_VAR,
  V_SPRITE_VAR,
  V_SPRITE_CLIST,
  V_LOCAL_VAR,
  V_SCROLL_TEXT,
};

struct debug_var
{
  // This goes first so this struct can act like a char * for the dialog.
  char text[VAR_LIST_WIDTH + 1];

  // Internal data
  unsigned char type;
  unsigned char id;
  boolean is_empty;
  union
  {
    struct counter *counter;
    struct string *string;
    const char *var_name;
    enum virtual_var virtual_var;
    int clist_num;
    int local_num;
  }
  data;
};

struct debug_ram_data
{
  size_t counter_list_size;
  size_t counter_table_size;
  size_t counter_struct_size;
  size_t string_list_size;
  size_t string_table_size;
  size_t string_struct_size;
  size_t sfx_size;
  size_t sprites_size;
  size_t vlayer_size;
  size_t board_list_and_struct_size;
  size_t board_data_size;
  size_t board_overlay_size;
  size_t robot_list_and_struct_size;
  size_t robot_stack_size;
  size_t robot_source_size;
  size_t robot_program_size;
  size_t robot_program_labels_size;
  size_t scroll_sensor_total_size;
  size_t video_layer_size;
  size_t debug_robot_total_size;
  size_t debug_variables_total_size;
  size_t extram_uncompressed_size;
  size_t extram_compressed_size;
  size_t virtual_filesystem_size;
  size_t virtual_filesystem_cached_size;
};

static struct debug_ram_data ram_data;

static void robot_ram_usage(struct robot *robot, struct debug_ram_data *ram_data)
{
  if(robot->stack)
    ram_data->robot_stack_size += robot->stack_size * sizeof(robot->stack[0]);

#ifdef CONFIG_DEBYTECODE
  if(robot->program_source)
    ram_data->robot_source_size += robot->program_source_length;
#else
  // Count this in debug since it's only used for that...
  if(robot->program_source)
    ram_data->debug_robot_total_size += robot->program_source_length;
#endif /* !CONFIG_DEBYTECODE */

  if(robot->program_bytecode)
    ram_data->robot_program_size += robot->program_bytecode_length;

  if(robot->command_map)
  {
    ram_data->debug_robot_total_size +=
     robot->command_map_length * sizeof(struct command_mapping);
  }

  if(robot->label_list)
  {
    ram_data->robot_program_labels_size +=
     robot->num_labels * (sizeof(struct label *) + sizeof(struct label));
  }
}

static void update_ram_usage_data(struct world *mzx_world,
 size_t var_debug_usage)
{
  int i, j;
  size_t u;

  memset(&ram_data, 0, sizeof(struct debug_ram_data));

  counter_list_size(&mzx_world->counter_list, &ram_data.counter_list_size,
   &ram_data.counter_table_size, &ram_data.counter_struct_size);

  string_list_size(&mzx_world->string_list, &ram_data.string_list_size,
   &ram_data.string_table_size, &ram_data.string_struct_size);

  ram_data.sfx_size = sfx_ram_usage(&mzx_world->custom_sfx);

  ram_data.sprites_size =
   mzx_world->num_sprites_allocated * sizeof(struct sprite *) +
   mzx_world->num_sprites * sizeof(struct sprite);

  ram_data.vlayer_size = mzx_world->vlayer_size * 2;

  ram_data.board_list_and_struct_size =
   mzx_world->num_boards_allocated * sizeof(struct board *) +
   mzx_world->num_boards * sizeof(struct board);

  robot_ram_usage(&mzx_world->global_robot, &ram_data);

  for(i = 0; i < mzx_world->num_boards; i++)
  {
    struct board *b = mzx_world->board_list[i];
    // Board data.
    ram_data.board_data_size += b->board_width * b->board_height * 6;
    if(b->overlay_mode)
      ram_data.board_overlay_size += b->board_width * b->board_height * 2;

    // Buffers (split off the board struct).
    ram_data.board_list_and_struct_size +=
     b->input_allocated + b->charset_path_allocated + b->palette_path_allocated;

    // Robot list (+1) and robot name sorted list.
    ram_data.robot_list_and_struct_size +=
     ((b->num_robots_allocated + 1) + b->num_robots_active) * sizeof(struct robot *);

    // Scroll and sensor lists (both +1).
    ram_data.scroll_sensor_total_size +=
     (b->num_scrolls_allocated + 1) * sizeof(struct scroll *) +
     (b->num_sensors_allocated + 1) * sizeof(struct sensor *);

    for(j = 1; j <= b->num_robots; j++)
    {
      if(b->robot_list[j])
      {
        ram_data.robot_list_and_struct_size += sizeof(struct robot);
        robot_ram_usage(b->robot_list[j], &ram_data);
      }
    }

    for(j = 1; j <= b->num_scrolls; j++)
      if(b->scroll_list[j])
        ram_data.scroll_sensor_total_size +=
         sizeof(struct scroll) + b->scroll_list[j]->mesg_size;

    for(j = 1; j <= b->num_sensors; j++)
      if(b->sensor_list[j])
        ram_data.scroll_sensor_total_size += sizeof(struct sensor);

#ifdef CONFIG_EXTRAM
    {
      size_t c;
      size_t u;
      if(board_extram_usage(b, &c, &u))
      {
        ram_data.extram_compressed_size += c;
        ram_data.extram_uncompressed_size += u;
      }
    }
#endif
  }

  for(u = 0; u < graphics.layer_count; u++)
  {
    struct video_layer *layer = &(graphics.video_layers[u]);
    if(layer->data)
      ram_data.video_layer_size +=
       layer->w * layer->h * sizeof(struct char_element);
  }

  ram_data.debug_variables_total_size = var_debug_usage;

  ram_data.virtual_filesystem_size = vio_filesystem_total_memory_usage();
  ram_data.virtual_filesystem_cached_size = vio_filesystem_total_cached_usage();
}

#define match_counter(_name) (strlen(_name) == len && !strcasecmp(name, _name))

int get_counter_safe(struct world *mzx_world, const char *name, int id)
{
  // This is a safe wrapper for get_counter(). Some counters, when read,
  // could alter world data in a way we don't want to happen here.
  // So detect and block them.

  size_t len = strlen(name);

  if(match_counter("fread"))          return -1;
  if(match_counter("fread_counter"))  return -1;

  return get_counter(mzx_world, name, id);
}

static void get_var_name(struct debug_var *v, const char **name, int *len,
 char buffer[VAR_LIST_WIDTH + 1])
{
  switch((enum debug_var_type)v->type)
  {
    case V_COUNTER:
      if(name) *name = v->data.counter->name;
      if(len)  *len = v->data.counter->name_length;
      return;

    case V_STRING:
      if(name) *name = v->data.string->name;
      if(len)  *len = v->data.string->name_length;
      return;

    case V_VAR:
    case V_SCROLL_TEXT:
      if(name) *name = (char *)v->data.var_name;
      if(len)  *len = strlen(*name);
      return;

    case V_VIRTUAL_VAR:
      if(name) *name = (char *)virtual_var_names[v->data.virtual_var];
      if(len)  *len = strlen(*name);
      return;

    case V_SPRITE_VAR:
      snprintf(buffer, 32, "spr%d_%s", v->id, v->data.var_name);
      if(name) *name = buffer;
      if(len)  *len = strlen(buffer);
      return;

    case V_SPRITE_CLIST:
      snprintf(buffer, 32, "spr_clist%d*", v->data.clist_num);
      if(name) *name = buffer;
      if(len)  *len = strlen(buffer);
      return;

    case V_LOCAL_VAR:
      snprintf(buffer, 32, "local%d", v->data.local_num);
      if(name) *name = buffer;
      if(len)  *len = strlen(buffer);
      return;
  }
}

#define match_var(_name) (strlen(_name) == len && !memcmp(var, _name, len))

// The buffer param is used for any vars that need to generate char values.
static void get_var_value(struct world *mzx_world, struct debug_var *v,
 const char **char_value, int *int_value, int64_t *long_value,
 char buffer[VAR_LIST_WIDTH + 1])
{
  struct board *cur_board = mzx_world->current_board;
  char real_var[32];

  *char_value = NULL;

  switch((enum debug_var_type)v->type)
  {
    case V_COUNTER:
    {
      v->is_empty = false;
      if(v->data.counter->value == 0)
        v->is_empty = true;

      *int_value = v->data.counter->value;
      break;
    }

    case V_STRING:
    {
      v->is_empty = false;
      if(!v->data.string->length)
        v->is_empty = true;

      *char_value = v->data.string->value;
      *int_value = v->data.string->length;
      break;
    }

    // It's a built-in var.  Since some of these don't have
    // read functions, we have to map several manually.
    case V_VAR:
    {
      const char *var = v->data.var_name;
      size_t len = strlen(var);
      int index = v->id;

      if(match_var("bimesg"))
      {
        *int_value = mzx_world->bi_mesg_status;
      }
      else

      if(match_var("spacelock"))
      {
        *int_value = mzx_world->bi_shoot_status;
      }
      else

      if(match_var("fread_delimiter"))
      {
        *int_value = mzx_world->fread_delimiter;
      }
      else

      if(match_var("fwrite_delimiter"))
      {
        *int_value = mzx_world->fwrite_delimiter;
      }
      else

      if(match_var("Active sprites*"))
      {
        *int_value = mzx_world->active_sprites;
      }
      else

      if(match_var("spr_yorder"))
      {
        *int_value = mzx_world->sprite_y_order;
      }
      else

      if(match_var("mod_name*"))
      {
        *char_value = mzx_world->real_mod_playing;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("board_name*"))
      {
        *char_value = cur_board->board_name;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("board_mod*"))
      {
        *char_value = cur_board->mod_playing;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("input*"))
      {
        // the starred version of input is the input string!
        *char_value = cur_board->input_string ? cur_board->input_string : "";
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("volume"))
      {
        *int_value = cur_board->volume;
      }
      else

      if(match_var("robot_name*"))
      {
        *char_value = cur_board->robot_list[index]->robot_name;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("commands_total"))
      {
        *int_value = cur_board->robot_list[index]->commands_total;
      }
      else

      if(match_var("commands_cycle*"))
      {
        *int_value = cur_board->robot_list[index]->commands_cycle;
      }
      else

      if(match_var("lockself"))
      {
        *int_value = cur_board->robot_list[index]->is_locked;
      }
      else

      if(match_var("Sensor name*"))
      {
        *char_value = cur_board->sensor_list[index]->sensor_name;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("Robot to mesg*"))
      {
        *char_value = cur_board->sensor_list[index]->robot_to_mesg;
        *int_value = strlen(*char_value);
      }
      else

      if(match_var("joy_simulate_keys"))
      {
        *int_value = mzx_world->joystick_simulate_keys;
      }

      else
      {
        // All other vars try to use a regular counter lookup.
        memcpy(real_var, var, len + 1);

        if(real_var[len - 1] == '*')
          real_var[len - 1] = 0;

        *int_value = get_counter_safe(mzx_world, real_var, index);
      }
      break;
    }

    // Subset of builtin vars that represent aggregate data or other info that
    // doesn't actually exist in the world.
    case V_VIRTUAL_VAR:
    {
      enum virtual_var vvar = v->data.virtual_var;
      int64_t value = 0;

      switch(vvar)
      {
        case VIR_RAM_COUNTER_LIST:
          value = ram_data.counter_list_size;
          break;
        case VIR_RAM_COUNTER_TABLE:
          value = ram_data.counter_table_size;
          break;
        case VIR_RAM_COUNTERS:
          value = ram_data.counter_struct_size;
          break;
        case VIR_RAM_STRING_LIST:
          value = ram_data.string_list_size;
          break;
        case VIR_RAM_STRING_TABLE:
          value = ram_data.string_table_size;
          break;
        case VIR_RAM_STRINGS:
          value = ram_data.string_struct_size;
          break;
        case VIR_RAM_VLAYER:
          value = ram_data.vlayer_size;
          break;
        case VIR_RAM_SFX:
          value = ram_data.sfx_size;
          break;
        case VIR_RAM_SPRITES:
          value = ram_data.sprites_size;
          break;
        case VIR_RAM_BOARD_INFO:
          value = ram_data.board_list_and_struct_size;
          break;
        case VIR_RAM_BOARD_DATA:
          value = ram_data.board_data_size;
          break;
        case VIR_RAM_BOARD_OVERLAY:
          value = ram_data.board_overlay_size;
          break;
        case VIR_RAM_ROBOT_INFO:
          value = ram_data.robot_list_and_struct_size;
          break;
        case VIR_RAM_ROBOT_STACK:
          value = ram_data.robot_stack_size;
          break;
        case VIR_RAM_ROBOT_SOURCE:
          value = ram_data.robot_source_size;
          break;
        case VIR_RAM_ROBOT_PROGRAMS:
          value = ram_data.robot_program_size;
          break;
        case VIR_RAM_ROBOT_PROGRAM_LABELS:
          value = ram_data.robot_program_labels_size;
          break;
        case VIR_RAM_SCROLLS_SENSORS:
          value = ram_data.scroll_sensor_total_size;
          break;
        case VIR_RAM_VIDEO_LAYERS:
          value = ram_data.video_layer_size;
          break;
        case VIR_RAM_DEBUGGER_ROBOTS:
          value = ram_data.debug_robot_total_size;
          break;
        case VIR_RAM_DEBUGGER_VARIABLES:
          value = ram_data.debug_variables_total_size;
          break;
        case VIR_RAM_EXTRAM_DELTA:
          value = (int64_t)ram_data.extram_compressed_size -
           (int64_t)ram_data.extram_uncompressed_size;
          break;
        case VIR_RAM_VIRTUAL_FILESYSTEM:
          value = ram_data.virtual_filesystem_size;
          break;
        case VIR_RAM_VIRTUAL_FILESYSTEM_CACHED_ONLY:
          value = ram_data.virtual_filesystem_cached_size;
          break;
      }
      *long_value = value;
      break;
    }

    case V_SPRITE_VAR:
    {
      const char *var = v->data.var_name;
      size_t len = strlen(var);
      int sprite_num = v->id;

      if(match_var("overlay"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_OVER_OVERLAY)
          *int_value = 1;
      }
      else

      if(match_var("static"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_STATIC)
          *int_value = 1;
      }
      else

      if(match_var("vlayer"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_VLAYER)
          *int_value = 1;
      }
      else

      if(match_var("ccheck"))
      {
        int flags = mzx_world->sprite_list[sprite_num]->flags;
        *int_value = 0;

        if(flags & SPRITE_CHAR_CHECK)
          *int_value |= 1;

        if(flags & SPRITE_CHAR_CHECK2)
          *int_value |= 2;
      }

      // Matched sprN but no var matched
      else
      {
        snprintf(real_var, 32, "spr%d_%s", sprite_num, var);

        if(real_var[len - 1] == '*')
          real_var[len - 1] = 0;

        *int_value = get_counter_safe(mzx_world, real_var, 0);
      }
      break;
    }

    case V_SPRITE_CLIST:
    {
      // These are generated separately from the vars list.
      int index = v->data.clist_num;
      *int_value = mzx_world->collision_list[index];
      break;
    }

    case V_LOCAL_VAR:
    {
      // These are a special case mostly because their names are generated.
      // NOTE: Annoyingly, local1 is at index 0, local2 is at index 1, etc...
      // NOTE: Awful C modulo-- add 31 instead of subtracting 1...
      int local_num = (v->data.local_num + 31) % 32;
      int index = v->id;

      *int_value = cur_board->robot_list[index]->local[local_num];
      break;
    }

    case V_SCROLL_TEXT:
    {
      struct scroll *src_scroll = cur_board->scroll_list[v->id];
      *char_value = src_scroll->mesg + 1;
      *int_value = src_scroll->mesg_size;
      break;
    }
  }
}

static void read_var(struct world *mzx_world, struct debug_var *v)
{
  char buffer[VAR_LIST_WIDTH + 1];
  char *char_dest = v->text + SVALUE_COL_OFFSET;
  const char *char_value = NULL;
  int64_t long_value = INT64_MIN;
  int int_value = 0;

  get_var_value(mzx_world, v, &char_value, &int_value, &long_value, buffer);

  if(v->type == V_STRING)
  {
    // Add special escaping to string values to match how they're edited.
    const char *src = v->data.string->value;
    size_t src_len = v->data.string->length;
    copy_substring_escaped(char_dest, SVALUE_SIZE + 1, src, src_len);
  }
  else

  if(char_value)
  {
    // Use minimal escaping to avoid display bugs.
    copy_name_escaped(char_dest, SVALUE_SIZE + 1, char_value, int_value);
  }
  else

  if(long_value != INT64_MIN)
  {
    snprintf(v->text + LVALUE_COL_OFFSET, LVALUE_SIZE + 1, "%"PRId64, long_value);
  }

  else
  {
    snprintf(v->text + CVALUE_COL_OFFSET, CVALUE_SIZE + 1, "%i", int_value);
  }
}

static void write_var(struct world *mzx_world, struct debug_var *v, int int_val,
 char *char_val)
{
  switch((enum debug_var_type)v->type)
  {
    case V_COUNTER:
    {
      // As tempting as it is to directly write, we need to respect gateways
      set_counter(mzx_world, v->data.counter->name, int_val, 0);
      break;
    }

    case V_STRING:
    {
      //set string -- int_val is the length here
      char buffer[ROBOT_MAX_TR];
      struct string temp;

      memset(&temp, '\0', sizeof(struct string));
      temp.length = int_val;
      temp.value = char_val;

      memcpy(buffer, v->data.string->name, v->data.string->name_length + 1);
      set_string(mzx_world, buffer, &temp, 0);
      break;
    }

    case V_VAR:
    {
      // It's a built-in var
      struct board *cur_board = mzx_world->current_board;
      const char *var = v->data.var_name;
      size_t len = strlen(var);
      int index = v->id;

      if(var[len - 1] != '*')
      {
        // Special cases: not an actual counter, but needs to be writable

        if(match_var("commands_total"))
        {
          cur_board->robot_list[index]->commands_total = int_val;
        }
        else

        if(match_var("volume"))
        {
          int_val = int_val & 255;

          cur_board->volume = int_val;
          cur_board->volume_target = int_val;

          audio_set_module_volume(int_val);
        }
        else

        if(match_var("lockself"))
        {
          cur_board->robot_list[index]->is_locked = (int_val != 0);
        }

        // Everything else
        else
        {
          set_counter(mzx_world, var, int_val, index);
        }
      }
      break;
    }

    case V_VIRTUAL_VAR:
    {
      // Virtual vars (read-only)
      return;
    };

    case V_SPRITE_VAR:
    {
      // Sprite variable
      char real_var[32];
      snprintf(real_var, 32, "spr%d_%s", v->id, v->data.var_name);

      set_counter(mzx_world, real_var, int_val, 0);
      break;
    }

    case V_SPRITE_CLIST:
    {
      // Sprite clist (read-only)
      return;
    }

    case V_LOCAL_VAR:
    {
      // Robot local variable
      char real_var[32];
      snprintf(real_var, 32, "local%d", v->data.local_num);

      set_counter(mzx_world, real_var, int_val, v->id);
      break;
    }

    case V_SCROLL_TEXT:
    {
      // Scroll text (read-only)
      return;
    }
  }

  // Now update debug_var to reflect the new value.
  read_var(mzx_world, v);
}

static void init_counter_var(struct debug_var *v, struct counter *src)
{
  char buf[CVALUE_COL_OFFSET];
  v->type = V_COUNTER;
  v->is_empty = (src->value ? false : true);
  v->data.counter = src;

  copy_name_escaped(buf, CVALUE_COL_OFFSET, src->name, src->name_length);
  snprintf(v->text, VAR_LIST_WIDTH, "%-*.*s %"PRId32,
   CVALUE_COL_OFFSET - 1, CVALUE_COL_OFFSET - 1, buf, src->value);
}

static void init_string_var(struct debug_var *v, struct string *src)
{
  char buf[SVALUE_COL_OFFSET];
  v->type = V_STRING;
  v->is_empty = (src->length ? false : true);
  v->data.string = src;

  copy_name_escaped(buf, SVALUE_COL_OFFSET, src->name, src->name_length);
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, buf, strlen(buf));
  copy_substring_escaped(v->text + SVALUE_COL_OFFSET, SVALUE_SIZE + 1,
   src->value, src->length);
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_builtin_var(struct debug_var *v, const char *name, int robot)
{
  v->type = V_VAR;
  v->is_empty = false;
  v->id = (char)robot;
  v->data.var_name = name;
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, name, strlen(name));
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_virtual_var(struct debug_var *v, enum virtual_var virtual_var)
{
  const char *name = virtual_var_names[virtual_var];
  v->type = V_VIRTUAL_VAR;
  v->is_empty = false;
  v->data.virtual_var = virtual_var;
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, name, strlen(name));
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_sprite_var(struct debug_var *v, const char *name, int spr)
{
  char buf[32];
  v->type = V_SPRITE_VAR;
  v->is_empty = false;
  v->id = (char)spr;
  v->data.var_name = name;
  sprintf(buf, "spr%d_%s", spr, name);
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, buf, strlen(buf));
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_sprite_clist_var(struct debug_var *v, int pos)
{
  char buf[32];
  v->type = V_SPRITE_CLIST;
  v->is_empty = false;
  v->data.clist_num = pos;
  sprintf(buf, "spr_clist%d*", pos);
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, buf, strlen(buf));
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_local_var(struct debug_var *v, int robot, int num)
{
  char buf[32];
  v->type = V_LOCAL_VAR;
  v->is_empty = false;
  v->id = (char)robot;
  v->data.local_num = num;
  sprintf(buf, "local%d", num);
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, buf, strlen(buf));
  v->text[VAR_LIST_WIDTH] = 0;
}

static void init_scroll_text(struct debug_var *v, int scroll)
{
  v->type = V_SCROLL_TEXT;
  v->is_empty = false;
  v->id = (char)scroll;
  v->data.var_name = scroll_text_var;
  memset(v->text, 32, VAR_LIST_WIDTH);
  memcpy(v->text, scroll_text_var, strlen(scroll_text_var));
  v->text[VAR_LIST_WIDTH] = 0;
}

/************************
 * Debug tree functions *
 ************************/

/* (root)-----16-S|---- for 58 now instead of 75.
 * - Counters
 *     a
 *     b
 *     c
 * - Strings
 *     $t
 * - Sprites
 *     spr0
 *     spr1
 *   World
 *   Board
 * + Robots
 *     0 Global
 *     1 (12,184)
 *     2 (139,18)
 */

#define DEBUG_NODE_NAME_SIZE 32

struct debug_node
{
   char name[DEBUG_NODE_NAME_SIZE];
   boolean opened;
   boolean refresh_on_focus;
   boolean show_child_contents;
   boolean delete_child_nodes;
   int num_nodes;
   int num_vars;
   struct debug_node *parent;
   struct debug_node *nodes;
   struct debug_var *vars;
};

// Build the tree display on the left side of the screen
static void build_tree_list(struct debug_node *node,
 char ***tree_list, int *tree_size, int level)
{
  int i;
  int level_offset = 1;
  char *name;

  // Skip empty nodes entirely unless they're root-level.
  if(level > 1 && node->num_nodes == 0 && node->num_vars == 0)
    return;

  if(level > 0)
  {
    // Display name and a real name so the menu can find it later.
    int buffer_len = TREE_LIST_WIDTH + strlen(node->name) + 1;
    int pad_len = level * level_offset;
    name = cmalloc(buffer_len);

    snprintf(name, buffer_len, "%*.*s%-*.*s %s",
      pad_len, pad_len, "",
      TREE_LIST_WIDTH - pad_len - 1, TREE_LIST_WIDTH - pad_len - 1, node->name,
      node->name
    );
    name[TREE_LIST_WIDTH - 1] = '\0';
    name[buffer_len - 1] = '\0';

    if(node->num_nodes)
    {
      if(node->opened)
        name[pad_len - 1] = '-';
      else
        name[pad_len - 1] = '+';
    }

    (*tree_list) = crealloc(*tree_list, sizeof(char *) * (*tree_size + 1));

    (*tree_list)[*tree_size] = name;
    (*tree_size)++;
  }

  if(node->num_nodes && node->opened)
    for(i = 0; i < node->num_nodes; i++)
      build_tree_list(&(node->nodes[i]), tree_list, tree_size, level+1);
}

// Free the tree list and all of its lines.
static void free_tree_list(char **tree_list, int tree_size)
{
  int i;

  for(i = 0; i < tree_size; i++)
    free(tree_list[i]);

  free(tree_list);
}

// Free and build in one package
static void rebuild_tree_list(struct debug_node *node,
 char ***tree_list, int *tree_size)
{
  if(*tree_list)
  {
    free_tree_list(*tree_list, *tree_size);
    *tree_list = NULL;
    *tree_size = 0;
  }
  build_tree_list(node, tree_list, tree_size, 0);
}

/**
 * From a node, populate the list of all counters that should appear.
 */

static void init_var_list(struct debug_node *node,
 struct debug_var **var_list, int num_vars, int *_index, boolean hide_empty)
{
  struct debug_var *current = node->vars;
  int index = *_index;
  int i;

  if(node->num_vars)
  {
    for(i = 0; i < node->num_vars; i++)
    {
      if(!hide_empty || !current->is_empty)
        var_list[index++] = &(node->vars[i]);

      current++;
    }
  }

  *_index = index;

  if(node->num_nodes && node->show_child_contents)
  {
    for(i = 0; i < node->num_nodes; i++)
      init_var_list(&(node->nodes[i]), var_list, num_vars, _index, hide_empty);
  }
}

/**
 * Calculate the number of vars contained inside of this node and its
 * children.
 */

static void get_var_count(struct debug_node *node, int *num, boolean hide_empty)
{
  int my_num = node->num_vars;
  int i;

  if(hide_empty)
    for(i = 0; i < node->num_vars; i++)
      if(node->vars[i].is_empty)
        my_num--;

  *num += my_num;

  if(node->num_nodes && node->show_child_contents)
  {
    int i;
    for(i = 0; i < node->num_nodes; i++)
      get_var_count(&(node->nodes[i]), num, hide_empty);
  }
}

/**
 * Reallocate an existing var list or create a new var list.
 */

static void rebuild_var_list(struct debug_node *node,
 struct debug_var ***var_list, int *num_vars, boolean hide_empty)
{
  int index = 0;

  free(*var_list);
  *num_vars = 0;
  get_var_count(node, num_vars, hide_empty);
  *var_list = cmalloc(*num_vars * sizeof(struct debug_var *));

  init_var_list(node, *var_list, *num_vars, &index, hide_empty);
}

/**
 * Delete all vars in the tree. If delete_all is true, all child nodes will
 * also be deleted; otherwise, only delete children of nodes explicitly marked.
 */
static void clear_debug_tree(struct debug_node *node, boolean delete_all)
{
  int i;
  if(node->num_vars)
  {
    free(node->vars);
    node->vars = NULL;
    node->num_vars = 0;
  }

  if(node->num_nodes)
  {
    // If this node deletes its children, its children also need to delete
    // their children recursively.
    delete_all |= node->delete_child_nodes;

    for(i = 0; i < node->num_nodes; i++)
      clear_debug_tree(&(node->nodes[i]), delete_all);

    if(delete_all)
    {
      free(node->nodes);
      node->nodes = NULL;
      node->num_nodes = 0;
    }
  }
}

/**
 * Get the total size of a debug tree's node and var structs.
 */
static size_t get_debug_tree_ram_size(struct debug_node *node)
{
  size_t sz = node->num_vars * sizeof(struct debug_var) +
   node->num_nodes * sizeof(struct debug_node);
  int i;

  for(i = 0; i < node->num_nodes; i++)
    sz += get_debug_tree_ram_size(&node->nodes[i]);

  return sz;
}

/****************************/
/* Tree Searching functions */
/****************************/

// Use this when somebody selects something from the tree list
static struct debug_node *find_node(struct debug_node *node, char *name)
{
  if(!strcmp(node->name, name))
    return node;

  if(node->nodes)
  {
    int i;
    struct debug_node *result;
    for(i = 0; i < node->num_nodes; i++)
    {
      result = find_node(&(node->nodes[i]), name);
      if(result)
        return result;
    }
  }

  return NULL;
}

static void get_node_name(struct debug_node *node, char *label, int max_length)
{
  if(node->parent && node->parent->parent)
  {
    get_node_name(node->parent, label, max_length);
    snprintf(label + strlen(label), max_length - strlen(label), " :: ");
  }

  snprintf(label + strlen(label), max_length - strlen(label), "%s", node->name);
}

static boolean match_debug_var_name(struct debug_var *v, char *name,
 size_t name_length)
{
  switch((enum debug_var_type)v->type)
  {
    case V_COUNTER:
    {
      if((size_t)v->data.counter->name_length == name_length &&
       !memcmp(v->data.counter->name, name, name_length))
        return true;
      return false;
    }

    case V_STRING:
    {
      if((size_t)v->data.string->name_length == name_length &&
       !memcmp(v->data.string->name, name, name_length))
        return true;
      return false;
    }

    default:
    {
      // Just compare against the display area.
      if(!strcmp(v->text, name))
        return true;
      return false;
    }
  }
}

static boolean select_debug_var(struct debug_node *node, char *var_name,
 size_t var_len, struct debug_node **new_focus, int *new_var_pos)
{
  struct debug_var *current = node->vars;
  int i;

  if(node->num_vars)
  {
    for(i = 0; i < node->num_vars; i++)
    {
      if(match_debug_var_name(current, var_name, var_len))
      {
        *new_focus = node;
        *new_var_pos = i;
        return true;
      }
      current++;
    }
  }

  if(node->num_nodes)
    for(i = 0; i < node->num_nodes; i++)
      if(select_debug_var(&(node->nodes[i]), var_name, var_len, new_focus,
       new_var_pos))
        return true;

  return false;
}

/******************/
/* Counter search */
/******************/

static boolean search_match(const char *var_text, const size_t var_text_length,
 const char *match_text, const struct string_search_data *match_text_index,
 const size_t match_length, int search_flags)
{
  boolean ignore_case = (search_flags & VAR_SEARCH_CASESENS) == 0;

  if(search_flags & VAR_SEARCH_EXACT)
  {
    if(var_text_length == match_length)
    {
      if(ignore_case)
      {
        if(!memcasecmp(var_text, match_text, match_length))
          return true;
      }
      else

      if(!memcmp(var_text, match_text, match_length))
        return true;
    }
  }
  else
  {
    if(string_search(var_text, var_text_length, match_text, match_length,
     match_text_index, ignore_case))
      return true;
  }

  return false;
}

static boolean search_vars(struct world *mzx_world, struct debug_node *node,
 struct debug_var **res_var, struct debug_node **res_node, int *res_pos,
 const char *match_text, const struct string_search_data *match_text_index,
 const size_t match_length, int search_flags, struct debug_var **stop_var)
{
  boolean matched = false;
  struct debug_var *current;
  int stop = node->num_vars;
  int start = 0;
  int inc = 1;
  int i;

  char var_text_buffer[VAR_LIST_WIDTH + 1];
  int var_text_length;
  const char *var_text;

  if(search_flags & VAR_SEARCH_REVERSE)
  {
    start = node->num_vars - 1;
    stop = -1;
    inc = -1;
  }

  // If there's an initial var, see if it's in this node.
  if(*res_var)
  {
    if((*res_var >= node->vars) && (*res_var < node->vars + node->num_vars))
    {
      // It is, so skip directly to it.
      start = *res_var - node->vars;
    }
    else
    {
      // It isn't, so we can just skip this node altogether.
      return false;
    }
  }

  current = node->vars + start;

  for(i = start; i != stop; i += inc, current += inc)
  {
    // Skip entries until we find the initial var that was provided.
    if(*res_var)
    {
      // This should be the very first var we try.
      if(*res_var == current)
        *res_var = NULL;

      continue;
    }

    if(search_flags & VAR_SEARCH_NAMES)
    {
      get_var_name(current, &var_text, &var_text_length, var_text_buffer);

      matched = search_match(var_text, var_text_length, match_text,
       match_text_index, match_length, search_flags);
    }

    if((search_flags & VAR_SEARCH_VALUES) && !matched)
    {
      int64_t long_value = INT64_MIN;
      get_var_value(mzx_world, current, &var_text, &var_text_length, &long_value,
       var_text_buffer);
      if(!var_text)
      {
        // Use the number already printed onto the var text area.
        if(long_value != INT64_MIN)
          var_text = current->text + LVALUE_COL_OFFSET;
        else
          var_text = current->text + CVALUE_COL_OFFSET;

        var_text_length = strlen(var_text);
      }

      matched = search_match(var_text, var_text_length, match_text,
       match_text_index, match_length, search_flags);
    }

    if(matched)
    {
      *res_var = current;
      *res_node = node;
      *res_pos = i;
      return true;
    }

    if(current == *stop_var)
    {
      *res_var = NULL;
      *res_node = NULL;
      *res_pos = -1;
      return true;
    }

    if(!*stop_var)
      *stop_var = current;
  }
  return false;
}

static boolean search_node(struct world *mzx_world, struct debug_node *node,
 struct debug_var **res_var, struct debug_node **res_node, int *res_pos,
 const char *match_text, const struct string_search_data *match_text_index,
 const size_t match_length, int search_flags, struct debug_var **stop_var)
{
  boolean r = false;
  int i;

  if(search_flags & VAR_SEARCH_REVERSE)
  {
    for(i = node->num_nodes - 1; i >= 0; i--)
    {
      r = search_node(mzx_world, &(node->nodes[i]), res_var, res_node, res_pos,
        match_text, match_text_index, match_length, search_flags, stop_var);
      if(r)
        return r;
    }
  }

  if(*res_node)
  {
    // We may have started in an empty child node of the provided parent.
    // In this case, we want to find the starting position before we begin
    // actually searching.
    if(node == *res_node)
    {
      *res_var = NULL;
      *res_node = NULL;
    }
  }

  if(node->num_vars && !*res_node)
  {
    r = search_vars(mzx_world, node, res_var, res_node, res_pos,
     match_text, match_text_index, match_length, search_flags, stop_var);
    if(r)
      return r;
  }

  if(!(search_flags & VAR_SEARCH_REVERSE))
  {
    for(i = 0; i < node->num_nodes; i++)
    {
      r = search_node(mzx_world, &(node->nodes[i]), res_var, res_node, res_pos,
        match_text, match_text_index, match_length, search_flags, stop_var);
      if(r)
        return r;
    }
  }
  return false;
}

static struct debug_var *find_first_var(struct debug_node *node)
{
  struct debug_var *res = NULL;

  if(node->num_vars)
    return node->vars;

  if(node->num_nodes)
  {
    int i;
    for(i = 0; i < node->num_nodes && !res; i++)
      res = find_first_var(&(node->nodes[i]));
  }
  return res;
}

static struct debug_var *find_last_var(struct debug_node *node)
{
  struct debug_var *res = NULL;

  if(node->num_nodes)
  {
    int i;
    for(i = node->num_nodes - 1; i >= 0 && !res; i--)
      res = find_last_var(&(node->nodes[i]));
  }

  if(node->num_vars && !res)
    return &(node->vars[node->num_vars - 1]);

  return res;
}

/**
 * If a var is currently selected, *res_var should be the pointer to the
 * current selected var. Otherwise, *res_node should be the pointer to the
 * current selected node. On a match, *res_var, *res_node, and *res_pos will
 * be pointers to the new selected var, node, and position in the node's var
 * list, respectively.
 */

static boolean search_debug(struct world *mzx_world, struct debug_node *node,
 struct debug_var **res_var, struct debug_node **res_node, int *res_pos,
 char *match_text, size_t match_length, int search_flags)
{
  struct debug_var *stop_var = NULL;
  boolean matched = false;

  struct string_search_data match_text_index;
  boolean ignore_case = (search_flags & VAR_SEARCH_CASESENS) == 0;

  // Generate an index to help speed up substring searches.
  if(!(search_flags & VAR_SEARCH_EXACT))
    string_search_index(match_text, match_length, &match_text_index, ignore_case);

  if(search_flags & VAR_SEARCH_WRAP)
  {
    // If we're wrapping, stop at the current selected var. May be NULL, in
    // which case the search will pick a stopping variable automatically.
    stop_var = *res_var;
  }
  else

  if(search_flags & VAR_SEARCH_REVERSE)
  {
    stop_var = find_first_var(node);
  }
  else
  {
    stop_var = find_last_var(node);
  }

  matched = search_node(mzx_world, node, res_var, res_node, res_pos,
   match_text, &match_text_index, match_length, search_flags, &stop_var);

  // Wrap? Try again from the start
  if(!matched && (search_flags & VAR_SEARCH_WRAP))
    matched = search_node(mzx_world, node, res_var, res_node, res_pos,
     match_text, &match_text_index, match_length, search_flags, &stop_var);

  // If we stopped on a non-match, return false.
  if(*res_var == NULL)
    return false;

  return matched;
}

/**********************************/
/******* MAKE THE TREE CODE *******/
/**********************************/

#define NUM_ROLODEX 27

static const char rolodex_letters[NUM_ROLODEX + 1] =
 "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int get_rolodex_position(struct world *mzx_world, int first)
{
  first = tolower(first);

  if(first < 'a')
    return 0;

  if(first > 'z')
    return 0;

  return (first - 'a') + 1;
}

static struct debug_node *create_rolodex_nodes(struct debug_node *parent,
 size_t counts[NUM_ROLODEX], const char *name_prefix)
{
  struct debug_node *nodes = ccalloc(NUM_ROLODEX, sizeof(struct debug_node));
  int i;

  parent->num_nodes = NUM_ROLODEX;
  parent->nodes = nodes;

  for(i = 0; i < NUM_ROLODEX; i++)
  {
    if(counts[i])
    {
      snprintf(nodes[i].name, DEBUG_NODE_NAME_SIZE, "%s%c", name_prefix,
       rolodex_letters[i]);
      nodes[i].name[DEBUG_NODE_NAME_SIZE - 1] = '\0';
      nodes[i].parent = parent;
      nodes[i].num_nodes = 0;
      nodes[i].num_vars = 0;
      nodes[i].nodes = NULL;
      nodes[i].vars = cmalloc(counts[i] * sizeof(struct debug_var));
    }
  }
  return nodes;
}

static void init_counters_node(struct world *mzx_world, struct debug_node *dest)
{
  struct counter_list *counter_list = &(mzx_world->counter_list);
  struct debug_node *nodes;
  struct debug_node *current_node = NULL;
  struct debug_var *current_var;
  size_t var_counts[NUM_ROLODEX] = { 0 };
  size_t num_counters = counter_list->num_counters;
  int first_prev = -1;
  int first;
  int n = 0;
  size_t i;

  // The counter list may not be in the order we want to display it in.
  sort_counter_list(counter_list);

  // Get the number of vars for each node so they can be allocated in one pass.
  for(i = 0; i < num_counters; i++)
  {
    first = counter_list->counters[i]->name[0];
    if(first != first_prev)
    {
      n = get_rolodex_position(mzx_world, first);
      first_prev = first;
    }
    var_counts[n]++;
  }

  // Allocate and initialize subtree nodes.
  nodes = create_rolodex_nodes(dest, var_counts, "");

  // Insert vars into the newly allocated nodes.
  first_prev = -1;
  for(i = 0; i < num_counters; i++)
  {
    first = counter_list->counters[i]->name[0];
    if(first != first_prev)
    {
      n = get_rolodex_position(mzx_world, first);
      first_prev = first;
      current_node = &(nodes[n]);
    }

    current_var = &(current_node->vars[current_node->num_vars++]);

    init_counter_var(current_var, counter_list->counters[i]);
  }
}

static void init_strings_node(struct world *mzx_world, struct debug_node *dest)
{
  struct string_list *string_list = &(mzx_world->string_list);
  struct debug_node *nodes;
  struct debug_node *current_node = NULL;
  struct debug_var *current_var;
  size_t var_counts[NUM_ROLODEX] = { 0 };
  size_t num_strings = string_list->num_strings;
  int first_prev = -1;
  int first;
  int n = 0;
  size_t i;

  // Don't create any child nodes if there are no strings.
  if(!num_strings)
    return;

  // The string list may not be in the order we want to display it in.
  sort_string_list(string_list);

  // Get the number of vars for each node so they can be allocated in one pass.
  for(i = 0; i < num_strings; i++)
  {
    first = string_list->strings[i]->name[1];
    if(first != first_prev)
    {
      n = get_rolodex_position(mzx_world, first);
      first_prev = first;
    }
    var_counts[n]++;
  }

  // Allocate and initialize subtree nodes.
  nodes = create_rolodex_nodes(dest, var_counts, "$");

  // Insert vars into the newly allocated nodes.
  first_prev = -1;
  for(i = 0; i < num_strings; i++)
  {
    first = string_list->strings[i]->name[1];
    if(first != first_prev)
    {
      n = get_rolodex_position(mzx_world, first);
      first_prev = first;
      current_node = &(nodes[n]);
    }
    current_var = &(current_node->vars[current_node->num_vars++]);

    init_string_var(current_var, string_list->strings[i]);
  }
}

static void init_sprite_vars_node(struct world *mzx_world,
 struct debug_node *parent, struct debug_node *dest, int sprite_num)
{
  struct debug_var *vars =
   cmalloc(num_sprite_vars * sizeof(struct debug_var));
  struct debug_var *current_var;
  int i;

  for(i = 0; i < num_sprite_vars; i++)
  {
    current_var = &(vars[i]);
    init_sprite_var(current_var, sprite_var_list[i], sprite_num);
    read_var(mzx_world, current_var);
  }

  snprintf(dest->name, DEBUG_NODE_NAME_SIZE, "spr%d", sprite_num);
  dest->name[DEBUG_NODE_NAME_SIZE - 1] = '\0';
  dest->parent = parent;
  dest->num_nodes = 0;
  dest->nodes = NULL;
  dest->num_vars = num_sprite_vars;
  dest->vars = vars;
}

static void init_sprites_node(struct world *mzx_world, struct debug_node *dest)
{
  struct sprite **sprite_list = mzx_world->sprite_list;
  int num_sprites_active = 0;

  struct debug_node *nodes;
  struct debug_var *parent_vars;
  struct debug_var *current_var;
  int num_parent_vars = mzx_world->collision_count + num_sprite_parent_vars;
  int spr;
  int i;

  parent_vars = cmalloc(num_parent_vars * sizeof(struct debug_var));

  dest->num_vars = num_parent_vars;
  dest->vars = parent_vars;

  // Default parent vars
  for(i = 0; i < num_sprite_parent_vars; i++)
  {
    current_var = &(parent_vars[i]);
    init_builtin_var(current_var, sprite_parent_var_list[i], 0);
    read_var(mzx_world, current_var);
  }

  // Add the clist vars
  for(i = 0; i < mzx_world->collision_count; i++)
  {
    current_var = &(parent_vars[num_sprite_parent_vars + i]);
    init_sprite_clist_var(current_var, i);
    read_var(mzx_world, current_var);
  }

  // Get number of active sprites
  for(i = 0; i < MAX_SPRITES; i++)
    if(sprite_list[i]->width != 0 || sprite_list[i]->height != 0)
      num_sprites_active++;

  dest->num_nodes = num_sprites_active;
  if(num_sprites_active)
  {
    nodes = ccalloc(num_sprites_active, sizeof(struct debug_node));
    dest->nodes = nodes;

    // Populate the sprite nodes.
    for(spr = 0, i = 0; i < MAX_SPRITES; i++)
    {
      if(sprite_list[i]->width == 0 && sprite_list[i]->height == 0)
        continue;

      init_sprite_vars_node(mzx_world, dest, &(nodes[spr]), i);
      spr++;
    }
  }
  else
    dest->nodes = NULL;
}

static void init_builtin_node(struct world *mzx_world,
 struct debug_node *dest, const char **var_list, int num_vars, int robot_id)
{
  struct debug_var *vars = cmalloc(num_vars * sizeof(struct debug_var));
  struct debug_var *current_var;
  int i;

  for(i = 0; i < num_vars; i++)
  {
    current_var = &(vars[i]);
    init_builtin_var(current_var, var_list[i], robot_id);
    read_var(mzx_world, current_var);
  }

  dest->num_vars = num_vars;
  dest->vars = vars;
}

static void init_virtual_node(struct world *mzx_world,
 struct debug_node *dest, const enum virtual_var *var_list, int num_vars)
{
  struct debug_var *vars = cmalloc(num_vars * sizeof(struct debug_var));
  struct debug_var *current_var;
  int i;

  for(i = 0; i < num_vars; i++)
  {
    current_var = &(vars[i]);
    init_virtual_var(current_var, var_list[i]);
    read_var(mzx_world, current_var);
  }

  dest->num_vars = num_vars;
  dest->vars = vars;
}

static void init_universal_node(struct world *mzx_world, struct debug_node *dest)
{
  init_builtin_node(mzx_world, dest, universal_var_list,
   num_universal_vars, 0);
}

static void init_world_node(struct world *mzx_world, struct debug_node *dest)
{
  init_builtin_node(mzx_world, dest, world_var_list, num_world_vars, 0);
}

static void init_world_ram_node(struct world *mzx_world, struct debug_node *dest)
{
  init_virtual_node(mzx_world, dest, world_ram_var_list, num_world_ram_vars);
}

static void init_board_node(struct world *mzx_world, struct debug_node *dest)
{
  init_builtin_node(mzx_world, dest, board_var_list, num_board_vars, 0);
}

static void init_robot_vars_node(struct world *mzx_world,
 struct debug_node *parent, struct debug_node *dest, struct robot *src_robot,
 int robot_id)
{
  int num_vars = num_robot_vars + 32;
  struct debug_var *vars = cmalloc(num_vars * sizeof(struct debug_var));
  struct debug_var *current_var;
  char temp[DEBUG_NODE_NAME_SIZE];
  int i;

  // Init the default vars first
  for(i = 0; i < num_robot_vars; i++)
  {
    current_var = &(vars[i]);
    init_builtin_var(current_var, robot_var_list[i], robot_id);
    read_var(mzx_world, current_var);
  }

  // Add the locals
  for(i = 0; i < 32; i++)
  {
    current_var = &(vars[num_robot_vars + i]);
    init_local_var(current_var, robot_id, i);
    read_var(mzx_world, current_var);
  }

  snprintf(temp, DEBUG_NODE_NAME_SIZE, "%d:%s", robot_id, src_robot->robot_name);
  temp[DEBUG_NODE_NAME_SIZE - 1] = '\0';
  copy_name_escaped(dest->name, DEBUG_NODE_NAME_SIZE, temp, strlen(temp));
  dest->parent = parent;
  dest->num_nodes = 0;
  dest->nodes = NULL;
  dest->num_vars = num_vars;
  dest->vars = vars;
}

static void init_robot_node(struct world *mzx_world, struct debug_node *dest)
{
  struct debug_node *nodes;
  struct robot **robot_list = mzx_world->current_board->robot_list;
  int max_robots = mzx_world->current_board->num_robots + 1;
  int num_robots = 0;
  int cur;
  int i;

  // Get the number of robots to display
  for(i = 0; i < max_robots; i++)
    if(robot_list[i] && robot_list[i]->used)
      num_robots++;

  nodes = ccalloc(num_robots, sizeof(struct debug_node));
  dest->num_nodes = num_robots;
  dest->nodes = nodes;

  // Populate the robot nodes.
  for(cur = 0, i = 0; i < max_robots; i++)
    if(robot_list[i] && robot_list[i]->used)
      init_robot_vars_node(mzx_world, dest, &(nodes[cur++]), robot_list[i], i);
}

static void init_scroll_var_node(struct world *mzx_world,
 struct debug_node *parent, struct debug_node *dest, int scroll_id)
{
  struct debug_var *var_list = ccalloc(1, sizeof(struct debug_var));

  init_scroll_text(&(var_list[0]), scroll_id);
  read_var(mzx_world, &(var_list[0]));

  snprintf(dest->name, DEBUG_NODE_NAME_SIZE, "Scroll %d", scroll_id);
  dest->name[DEBUG_NODE_NAME_SIZE - 1] = '\0';
  dest->parent = parent;
  dest->num_nodes = 0;
  dest->nodes = NULL;
  dest->num_vars = 1;
  dest->vars = var_list;
}

static void init_scroll_node(struct world *mzx_world, struct debug_node *dest)
{
  struct debug_node *nodes;
  struct scroll **scroll_list = mzx_world->current_board->scroll_list;
  int max_scrolls = mzx_world->current_board->num_scrolls;
  int num_scrolls = 0;
  int cur;
  int i;

  for(i = 1; i <= max_scrolls; i++)
    if(scroll_list[i] && scroll_list[i]->used)
      num_scrolls++;

  // Don't create any child nodes if there are no scrolls...
  if(!num_scrolls)
    return;

  nodes = ccalloc(num_scrolls, sizeof(struct debug_node));
  dest->num_nodes = num_scrolls;
  dest->nodes = nodes;

  // Populate scroll nodes.
  for(cur = 0, i = 1; i <= max_scrolls; i++)
    if(scroll_list[i] && scroll_list[i]->used)
      init_scroll_var_node(mzx_world, dest, &(nodes[cur++]), i);
}

static void init_sensor_var_node(struct world *mzx_world,
 struct debug_node *parent, struct debug_node *dest, int sensor_id)
{
  init_builtin_node(mzx_world, dest, sensor_var_list, num_sensor_vars,
   sensor_id);

  snprintf(dest->name, DEBUG_NODE_NAME_SIZE, "Sensor %d", sensor_id);
  dest->name[DEBUG_NODE_NAME_SIZE - 1] = '\0';
  dest->parent = parent;
  dest->num_nodes = 0;
  dest->nodes = NULL;
}

static void init_sensor_node(struct world *mzx_world, struct debug_node *dest)
{
  struct debug_node *nodes;
  struct sensor **sensor_list = mzx_world->current_board->sensor_list;
  int max_sensors = mzx_world->current_board->num_sensors;
  int num_sensors = 0;
  int cur;
  int i;

  for(i = 1; i <= max_sensors; i++)
    if(sensor_list[i] && sensor_list[i]->used)
      num_sensors++;

  // Don't create any child nodes if there are no sensors...
  if(!num_sensors)
    return;

  nodes = ccalloc(num_sensors, sizeof(struct debug_node));
  dest->num_nodes = num_sensors;
  dest->nodes = nodes;

  // Populate the sensor nodes.
  for(cur = 0, i = 1; i <= max_sensors; i++)
    if(sensor_list[i] && sensor_list[i]->used)
      init_sensor_var_node(mzx_world, dest, &(nodes[cur++]), i);
}

enum root_node_ids
{
  NODE_COUNTERS,
  NODE_STRINGS,
  NODE_SPRITES,
  NODE_UNIVERSAL,
  NODE_WORLD,
  NODE_BOARD,
  NODE_ROBOTS,
  NUM_ROOT_NODES
};

enum world_node_ids
{
  NODE_RAM,
  NUM_WORLD_NODES
};

enum board_node_ids
{
  NODE_SCROLLS,
  NODE_SENSORS,
  NUM_BOARD_NODES
};

/**
 * Some variable values may be cached, such as the clock time.
 * This resets those values so they will update when the var list refreshes.
 */
static void clear_cached_data(struct world *mzx_world)
{
  mzx_world->command_cache = 0;
}

// Create new counter lists.
// (Re)make the child nodes
static void repopulate_tree(struct world *mzx_world, struct debug_node *root)
{
  struct debug_node *world = &(root->nodes[NODE_WORLD]);
  struct debug_node *board = &(root->nodes[NODE_BOARD]);
  size_t sz;

  // Clear the debug tree recursively (but preserve the base structure).
  clear_debug_tree(root, false);

  clear_cached_data(mzx_world);

  // Initialize the tree.
  init_counters_node(mzx_world,   &(root->nodes[NODE_COUNTERS]));
  init_strings_node(mzx_world,    &(root->nodes[NODE_STRINGS]));
  init_sprites_node(mzx_world,    &(root->nodes[NODE_SPRITES]));
  init_universal_node(mzx_world,  &(root->nodes[NODE_UNIVERSAL]));
  init_world_node(mzx_world,      &(root->nodes[NODE_WORLD]));
  init_board_node(mzx_world,      &(root->nodes[NODE_BOARD]));
  init_robot_node(mzx_world,      &(root->nodes[NODE_ROBOTS]));

  if(board->nodes)
  {
    init_scroll_node(mzx_world,   &(board->nodes[NODE_SCROLLS]));
    init_sensor_node(mzx_world,   &(board->nodes[NODE_SENSORS]));
  }

  if(world->nodes)
  {
    /**
     * The RAM node should be done last so the rest of the tree is finished
     * for the debug variables RAM calculation.
     *
     * TODO this doesn't include the size of the var/node list allocations.
     */
    sz = get_debug_tree_ram_size(root);
    sz += num_world_ram_vars * sizeof(struct debug_var);
    update_ram_usage_data(mzx_world, sz);

    init_world_ram_node(mzx_world,   &(world->nodes[NODE_RAM]));
  }
}

// Create the base tree structure, except for sprites and robots
static void build_debug_tree(struct world *mzx_world, struct debug_node *root)
{
  struct debug_node *nodes = ccalloc(NUM_ROOT_NODES, sizeof(struct debug_node));

  struct debug_node root_node =
  {
    "",
    true,  //opened
    false, //refresh_on_focus
    false, //show_child_contents
    false, //delete_child_nodes
    NUM_ROOT_NODES,
    0,
    NULL,  //parent
    nodes,
    NULL
  };

  struct debug_node counters =
  {
    "Counters",
    false,
    false,
    true,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node strings =
  {
    "Strings",
    false,
    false,
    true,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node sprites =
  {
    "Sprites",
    false,
    true,
    false,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node universal =
  {
    "Universal",
    false,
    true,
    false,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node world =
  {
    "World",
    false,
    true,
    false,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node world_ram =
  {
    "RAM",
    false,
    true,
    false,
    false,
    0,
    0,
    &(nodes[NODE_WORLD]),
    NULL,
    NULL
  };

  struct debug_node board =
  {
    "Board",
    false,
    true,
    false,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node robots =
  {
    "Robots",
    false,
    false,
    false,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node scrolls =
  {
    "Scrolls/Signs",
    false,
    false,
    false,
    true,
    0,
    0,
    &(nodes[NODE_BOARD]),
    NULL,
    NULL
  };

  struct debug_node sensors =
  {
    "Sensors",
    false,
    false,
    false,
    true,
    0,
    0,
    &(nodes[NODE_BOARD]),
    NULL,
    NULL
  };

  struct debug_node *world_n = ccalloc(NUM_WORLD_NODES, sizeof(struct debug_node));
  world.num_nodes = NUM_WORLD_NODES;
  world.nodes = world_n;
  world_n[NODE_RAM] = world_ram;

  if(mzx_world->current_board->num_scrolls ||
   mzx_world->current_board->num_sensors)
  {
    // Only create these if there are actually scrolls/sensors on the board.
    struct debug_node *n = ccalloc(NUM_BOARD_NODES, sizeof(struct debug_node));
    board.num_nodes = NUM_BOARD_NODES;
    board.nodes = n;

    n[NODE_SCROLLS] = scrolls;
    n[NODE_SENSORS] = sensors;
  }

  *root = root_node;
  nodes[NODE_COUNTERS] = counters;
  nodes[NODE_STRINGS] = strings;
  nodes[NODE_SPRITES] = sprites;
  nodes[NODE_UNIVERSAL] = universal;
  nodes[NODE_WORLD] = world;
  nodes[NODE_BOARD] = board;
  nodes[NODE_ROBOTS] = robots;

  repopulate_tree(mzx_world, root);
}

/*****************************/
/* Set Counter/String Dialog */
/*****************************/

static void input_counter_value(struct world *mzx_world, struct debug_var *v)
{
  char new_value[71];
  char dialog_name[71];

  new_value[0] = 0;
  dialog_name[0] = 0;

  switch((enum debug_var_type)v->type)
  {
    case V_COUNTER:
    {
      const struct counter *src = v->data.counter;
      const char *mesg = "Edit: counter ";
      char *dest = dialog_name + strlen(mesg);
      size_t dest_len = sizeof(dialog_name) - strlen(mesg);

      strcpy(dialog_name, mesg);
      copy_name_escaped(dest, dest_len, src->name, src->name_length);
      sprintf(new_value, "%"PRId32, src->value);
      break;
    }

    case V_STRING:
    {
      const struct string *src = v->data.string;
      const char *mesg = "Edit: string ";
      char *dest = dialog_name + strlen(mesg);
      size_t dest_len = sizeof(dialog_name) - strlen(mesg);

      strcpy(dialog_name, mesg);
      copy_name_escaped(dest, dest_len, src->name, src->name_length);
      copy_substring_escaped(new_value, 71, src->value, src->length);
      break;
    }

    case V_VAR:
    {
      const char *src_var = v->data.var_name;
      size_t len = strlen(src_var);

      // Ignore write-only variable names.
      if(src_var[len - 1] == '*')
        return;

      snprintf(dialog_name, 70, "Edit: variable %s", src_var);

      // Just strcpy it off of the debug_var for simplicity
      strcpy(new_value, v->text + CVALUE_COL_OFFSET);
      break;
    }

    case V_VIRTUAL_VAR:
    {
      // Virtual var (read-only)
      return;
    }

    case V_SPRITE_VAR:
    {
      snprintf(dialog_name, 70, "Edit: variable spr%d_%s",
       v->id, v->data.var_name);

      // Just strcpy it off of the debug_var for simplicity
      strcpy(new_value, v->text + CVALUE_COL_OFFSET);
      break;
    }

    case V_SPRITE_CLIST:
    {
      // Sprite clist (read-only)
      return;
    }

    case V_LOCAL_VAR:
    {
      snprintf(dialog_name, 70, "Edit: variable local%d", v->data.local_num);

      // Just strcpy it off of the debug_var for simplicity
      strcpy(new_value, v->text + CVALUE_COL_OFFSET);
      break;
    }

    case V_SCROLL_TEXT:
    {
      // Too much of a pain to figure out if it's a sign or scroll so
      scroll_edit(mzx_world, mzx_world->current_board->scroll_list[v->id], 0);
      return;
    }
  }

  // Prompt user to edit value
  if(!input_window(mzx_world, dialog_name, new_value, 70))
  {
    if(v->type == V_STRING)
    {
      int len;
      unescape_string(new_value, &len);
      write_var(mzx_world, v, len, new_value);
    }
    else
    {
      int counter_value = strtol(new_value, NULL, 10);
      write_var(mzx_world, v, counter_value, NULL);
    }
  }
}

/*****************/
/* Search Dialog */
/*****************/

static int counter_search_dialog_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  if((key == IKEY_RETURN) && (di->current_element == 0))
  {
    di->return_value = 0;
    di->done = 1;
    return 0;
  }

  return key;
}

static int counter_search_dialog(struct world *mzx_world, char *string,
 int *search_flags)
{
  int result = 0;
  struct dialog di;

  static const char *title = "Search variables (repeat: Ctrl+R)";
  static const char *name_opt[] = { "Search names" };
  static const char *value_opt[] = { "Search values" };
  static const char *case_opt[] = { "Case sensitive" };
  static const char *exact_opt[] = { "Exact" };
  static const char *reverse_opt[] = { "Reverse" };
  static const char *wrap_opt[] = { "Wrap" };
  static const char *local_opt[] = { "Current list" };

  int names =    (*search_flags & VAR_SEARCH_NAMES) > 0;
  int values =   (*search_flags & VAR_SEARCH_VALUES) > 0;
  int casesens = (*search_flags & VAR_SEARCH_CASESENS) > 0;
  int exact =    (*search_flags & VAR_SEARCH_EXACT) > 0;
  int reverse =  (*search_flags & VAR_SEARCH_REVERSE) > 0;
  int wrap =     (*search_flags & VAR_SEARCH_WRAP) > 0;
  int local =    (*search_flags & VAR_SEARCH_LOCAL) > 0;

  struct element *elements[] =
  {
    construct_input_box( 2, 1, "Search: ", VAR_SEARCH_MAX, string),
    construct_button(61, 1, "Search", 0),
    construct_check_box( 3, 2, name_opt,    1, strlen(*name_opt),    &names),
    construct_check_box(21, 2, value_opt,   1, strlen(*value_opt),   &values),
    construct_check_box(40, 2, case_opt,    1, strlen(*case_opt),    &casesens),
    construct_check_box( 5, 3, exact_opt,   1, strlen(*exact_opt),   &exact),
    construct_check_box(16, 3, reverse_opt, 1, strlen(*reverse_opt), &reverse),
    construct_check_box(29, 3, wrap_opt,    1, strlen(*wrap_opt),    &wrap),
    construct_check_box(39, 3, local_opt,   1, strlen(*local_opt),   &local),
    construct_button(61, 3, "Cancel", -1),
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  construct_dialog_ext(&di, title, VAR_SEARCH_DIALOG_X, VAR_SEARCH_DIALOG_Y,
   VAR_SEARCH_DIALOG_W, VAR_SEARCH_DIALOG_H, elements, ARRAY_SIZE(elements),
   0, 0, 0, counter_search_dialog_idle_function);

  result = run_dialog(mzx_world, &di);

  *search_flags = (names * VAR_SEARCH_NAMES) + (values * VAR_SEARCH_VALUES) +
   (casesens * VAR_SEARCH_CASESENS) + (reverse * VAR_SEARCH_REVERSE) +
   (wrap * VAR_SEARCH_WRAP) + (exact * VAR_SEARCH_EXACT) +
   (local * VAR_SEARCH_LOCAL);

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return result;
}

/**********************/
/* New Counter Dialog */
/**********************/

// Returns pointer to name if successful or NULL if cancelled or already existed
static int new_counter_dialog(struct world *mzx_world, char *name)
{
  int result;
  struct element *elements[3];
  struct dialog di;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  elements[0] = construct_input_box(2, 2, "Name:", VAR_ADD_MAX, name);
  elements[1] = construct_button(9, 4, "Confirm", 0);
  elements[2] = construct_button(23, 4, "Cancel", -1);

  construct_dialog_ext(&di, "New Counter/String",
   VAR_ADD_DIALOG_X, VAR_ADD_DIALOG_Y, VAR_ADD_DIALOG_W, VAR_ADD_DIALOG_H,
   elements, ARRAY_SIZE(elements), 0, 0, 0, NULL);

  result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(!result && (strlen(name) > 0))
  {
    // String
    if(name[0] == '$')
    {
      struct string temp;
      memset(&temp, '\0', sizeof(struct string));

      if(strchr(name, '.') || strchr(name, '+') || strchr(name, '#'))
      {
        confirm(mzx_world, "Characters .+# not allowed in string name.");
        return 0;
      }

      if(!get_string(mzx_world, name, &temp, 0))
      {
        //Doesn't exist -- set string
        temp.value = (char *)"";
        temp.length = 0;

        set_string(mzx_world, name, &temp, 0);
      }

      return 1;
    }
    // Counter
    else
    {
      if(!get_counter_safe(mzx_world, name, 0))
        set_counter(mzx_world, name, 0, 0);

      return 1;
    }
  }

  return 0;
}

/********************/
/* Counter Debugger */
/********************/

/*-3 - Repeat search
 *-2 - Update selected tree
 *-1 - Close
 * 0 - Selected var in var list
 * 1 - Selected node in tree list
 * 2 - Search
 * 3 - Add new counter
 * 4 - Hide empties
 * 5 - Export
 */
static int last_node_selected = 0;
static context *ctx_for_pal_char_editors = NULL; // FIXME hack

static int counter_debugger_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  struct list_box *tree_list = (struct list_box *)di->elements[1];

  if((di->current_element == 1) && (*(tree_list->result) != last_node_selected))
  {
    di->return_value = -2;
    di->done = 1;
  }
  // List element, so filter unhandled modifiers.
  if(get_shift_status(keycode_internal))
    return key;

  switch(key)
  {
    // Char editor
    case IKEY_c:
    {
      if(get_alt_status(keycode_internal) && !get_ctrl_status(keycode_internal))
      {
        char_editor(mzx_world);
        return 0;
      }
      break;
    }
    // Palette editor
    case IKEY_e:
    {
      if(get_alt_status(keycode_internal) && !get_ctrl_status(keycode_internal))
      {
        // FIXME hack
        if(ctx_for_pal_char_editors)
        {
          palette_editor(ctx_for_pal_char_editors);
          core_run(ctx_for_pal_char_editors->root);
        }
        return 0;
      }
      break;
    }
    // Search dialog
    case IKEY_f:
    {
      if(get_ctrl_status(keycode_internal) && !get_alt_status(keycode_internal))
      {
        di->return_value = 2;
        di->done = 1;
        return 0;
      }
      break;
    }
    // Repeat search
    case IKEY_r:
    {
      if(get_ctrl_status(keycode_internal) && !get_alt_status(keycode_internal))
      {
        di->return_value = -3;
        di->done = 1;
        return 0;
      }
      break;
    }
    // Hide empties
    case IKEY_h:
    {
      if(get_alt_status(keycode_internal) && !get_ctrl_status(keycode_internal))
      {
        di->return_value = 4;
        di->done = 1;
        return 0;
      }
      break;
    }
    // New counter/string
    case IKEY_n:
    {
      if(get_alt_status(keycode_internal) && !get_ctrl_status(keycode_internal))
      {
        di->return_value = 3;
        di->done = 1;
        return 0;
      }
      break;
    }
  }

  return key;
}

static struct debug_node root;
static char previous_var[VAR_SEARCH_MAX + 1];
static char previous_node_name[DEBUG_NODE_NAME_SIZE];

void __debug_counters(context *ctx)
{
  struct world *mzx_world = ctx->world;
  int i;

  int num_vars = 0;
  int tree_size = 0;
  struct debug_var **var_list = NULL;
  char **tree_list = NULL;

  int window_focus = 0;
  int var_selected = 0;
  int node_selected = 0;
  int node_scroll_offset = 0;
  int dialog_result;
  struct element *elements[8];
  struct dialog di;

  char label[80] = "Counters";
  struct debug_node *focus;
  boolean hide_empty_vars = false;

  char search_text[VAR_SEARCH_MAX + 1] = { 0 };
  int search_flags = VAR_SEARCH_NAMES + VAR_SEARCH_VALUES + VAR_SEARCH_WRAP;

  boolean reopened = false;
  boolean refocus_tree_list = false;

  // FIXME hack
  ctx_for_pal_char_editors = ctx;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_COUNTER_DEBUG);

  build_debug_tree(mzx_world, &root);

  focus = &(root.nodes[node_selected]);

  // If the debugger was opened before, try to pick up where we left off.
  if(previous_var[0] && previous_node_name[0])
  {
    struct debug_node *previous_node = find_node(&root, previous_node_name);

    if(previous_node)
    {
      focus = previous_node;
      snprintf(search_text, VAR_SEARCH_MAX, "%s", previous_var);
      search_flags =
       VAR_SEARCH_NAMES | VAR_SEARCH_LOCAL | VAR_SEARCH_WRAP | VAR_SEARCH_EXACT;

      reopened = true;
      refocus_tree_list = true;
    }
  }

  rebuild_tree_list(&root, &tree_list, &tree_size);
  rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);

  do
  {
    last_node_selected = node_selected;

    if(!reopened)
    {
      int bx = BUTTONS_X;
      int by = BUTTONS_Y;

      elements[0] = construct_list_box(
         VAR_LIST_X, VAR_LIST_Y, (const char **)var_list, num_vars,
         VAR_LIST_HEIGHT, VAR_LIST_WIDTH, 0, &var_selected, NULL, false);
      elements[1] = construct_list_box(
         TREE_LIST_X, TREE_LIST_Y, (const char **)tree_list, tree_size,
         TREE_LIST_HEIGHT, TREE_LIST_WIDTH, 1, &node_selected,
         &node_scroll_offset, false);
      elements[2] = construct_button(bx + 0, by + 0, "Search", 2);
      elements[3] = construct_button(bx +11, by + 0, "New", 3);
      elements[4] = construct_button(bx + 0, by + 2, "Toggle Empties", 4);
      elements[5] = construct_button(bx + 0, by + 4, "Export", 5);
      elements[6] = construct_button(bx +10, by + 4, "Done", -1);
      elements[7] = construct_label(VAR_LIST_X, VAR_LIST_Y - 1, label);

      construct_dialog_ext(&di, "Debug Variables", 0, 0,
       80, 25, elements, ARRAY_SIZE(elements), 0, 0, window_focus,
       counter_debugger_idle_function);

      dialog_result = run_dialog(mzx_world, &di);
    }

    else
    {
      // Reopened? Force a search
      dialog_result = -3;
    }

    if(node_selected != last_node_selected)
      focus = find_node(&root, tree_list[node_selected] + TREE_LIST_WIDTH);

    switch(dialog_result)
    {
      // Var list
      case 0:
      {
        window_focus = 0;
        if(var_selected < num_vars)
          input_counter_value(mzx_world, var_list[var_selected]);

        break;
      }

      // Switch to a new view (called by idle function)
      case -2:
      {
        // Tree list
        window_focus = 1;

        if(last_node_selected != node_selected)
        // && (focus->num_counters || focus->show_child_contents))
        {
          rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);
          var_selected = 0;

          label[0] = '\0';
          get_node_name(focus, label, 80);
        }
        break;
      }

      // Tree node
      case 1:
      {
        window_focus = 1;

        // Collapse/Expand selected view if applicable
        if(focus->num_nodes)
        {
          focus->opened = !(focus->opened);

          rebuild_tree_list(&root, &tree_list, &tree_size);
        }
        break;
      }

      // Search (Ctrl+F)
      case 2:
      {
        if(counter_search_dialog(mzx_world, search_text, &search_flags))
          break;
      }

      /* fallthrough */

      // Repeat search (Ctrl+R)
      case -3:
      {
        boolean matched = false;
        struct debug_var *search_var = NULL;
        struct debug_node *search_node = NULL;
        struct debug_node *search_targ = &root;
        char search_text_unescaped[VAR_SEARCH_MAX + 1];
        int search_text_length = 0;
        int search_pos = 0;

        // There is a var currently selected.
        if(var_selected < num_vars)
          search_var = var_list[var_selected];
        // The current view is empty.
        else
          search_node = focus;

        memcpy(search_text_unescaped, search_text, VAR_SEARCH_MAX);
        search_text_unescaped[VAR_SEARCH_MAX] = '\0';
        unescape_string(search_text_unescaped, &search_text_length);

        if(!search_text_length)
          break;

        // Local search?
        if(search_flags & VAR_SEARCH_LOCAL)
          search_targ = focus;

        matched = search_debug(mzx_world, search_targ,
         &search_var, &search_node, &search_pos,
         search_text_unescaped, search_text_length, search_flags);

        if(matched)
        {
          // First, is it in the current list? If so, we don't want to switch
          // focus nodes (and possibly spend time rebuilding the list).
          // We want to start from the current selected var and check in the
          // direction of the search for best results.
          if(num_vars)
          {
            int inc = !(search_flags & VAR_SEARCH_REVERSE) ? 1 : -1;
            boolean found = false;

            i = var_selected;
            do
            {
              i = (num_vars + i + inc) % num_vars;
              if(var_list[i] == search_var)
              {
                window_focus = 0; // Var list
                var_selected = i;
                found = true;
                break;
              }
            }
            while(i != var_selected);

            if(found)
              break;
          }

          // Nothing in the local context in a local search?  Abandon ship!
          if((search_flags & VAR_SEARCH_LOCAL) && (search_node != focus))
            break;

          // The var we matched might be hidden, so disable var hiding.
          hide_empty_vars = false;

          // Open search_node in var list
          rebuild_var_list(search_node, &var_list, &num_vars, hide_empty_vars);
          var_selected = search_pos;

          window_focus = 0; // Var list
          focus = search_node;
          refocus_tree_list = true;
        }
        break;
      }

      // Add counter/string
      case 3:
      {
        char add_name[VAR_ADD_MAX + 1] = { 0 };
        window_focus = 3;

        if(new_counter_dialog(mzx_world, add_name))
        {
          size_t add_len = strlen(add_name);

          // Hope it was worth it!
          repopulate_tree(mzx_world, &root);

          // The new counter is empty, so
          hide_empty_vars = false;

          // Find the counter/string we just made
          select_debug_var(&root, add_name, add_len, &focus, &var_selected);

          rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);

          window_focus = 0;
          refocus_tree_list = true;
        }
        break;
      }

      // Toggle Empties
      case 4:
      {
        struct debug_var *current = NULL;
        hide_empty_vars = !hide_empty_vars;

        if(num_vars)
          current = var_list[var_selected];

        rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);
        var_selected = 0;

        for(i = 0; i < num_vars; i++)
        {
          if(var_list[i] == current)
          {
            var_selected = i;
            break;
          }
        }

        label[0] = '\0';
        get_node_name(focus, label, 80);

        window_focus = 4;
        break;
      }

      // Export
      case 5:
      {
        char export_name[MAX_PATH];
        const char *txt_ext[] = { ".TXT", NULL };

        export_name[0] = 0;

        if(!new_file(mzx_world, txt_ext, ".txt", export_name,
         "Export counters/strings text file...", ALLOW_ALL_DIRS))
        {
          struct counter_list *counter_list = &(mzx_world->counter_list);
          struct string_list *string_list = &(mzx_world->string_list);
          vfile *vf;
          size_t i;

          vf = vfopen_unsafe(export_name, "wb");

          for(i = 0; i < counter_list->num_counters; i++)
          {
            struct counter *ctr = counter_list->counters[i];
            vf_printf(vf, "set \"%s\" to %" PRId32 "\n", ctr->name, ctr->value);
          }

          for(i = 0; i < string_list->num_strings; i++)
          {
            struct string *str = string_list->strings[i];
            vf_printf(vf, "set \"%s\" to \"", str->name);
            vfwrite(str->value, str->length, 1, vf);
            vfputs("\"\n", vf);
          }

          vfclose(vf);
        }

        window_focus = 5;
        break;
      }
    }
    if(focus->refresh_on_focus)
    {
      clear_cached_data(mzx_world);
      for(i = 0; i < focus->num_vars; i++)
        read_var(mzx_world, &(focus->vars[i]));
    }

    // If the current position in the tree was changed by a search, bring it
    // to focus in the tree list. This should only be used after a search.
    if(refocus_tree_list)
    {
      // Open all parents to ensure that this node will exist in the tree list.
      struct debug_node *node = focus;
      while(node && node->parent)
      {
        node = node->parent;
        node->opened = true;
      }

      // Clear and rebuild tree list.
      rebuild_tree_list(&root, &tree_list, &tree_size);

      // Find our node in the new tree list.
      for(i = 0; i < tree_size; i++)
      {
        if(!strcmp(tree_list[i] + TREE_LIST_WIDTH, focus->name))
        {
          node_selected = i;
          node_scroll_offset = node_selected - (TREE_LIST_HEIGHT/2);
          break;
        }
      }

      // Fix the current node name label.
      label[0] = '\0';
      get_node_name(focus, label, 80);

      refocus_tree_list = false;
    }

    // If the debug menu was just reopened, reset searching
    if(reopened)
    {
      search_text[0] = 0;
      search_flags = VAR_SEARCH_NAMES + VAR_SEARCH_VALUES + VAR_SEARCH_WRAP;
      reopened = false;
      // Also don't destruct the (uninitialized) dialog
      continue;
    }

    destruct_dialog(&di);

  } while(dialog_result != -1);

  pop_context();

  // Make sure the robot debugger watchpoints have updated values
  update_watchpoint_last_values(mzx_world);

  // Copy the last selected var to the previous field.
  if(var_selected < num_vars)
  {
    char buffer[VAR_LIST_WIDTH + 1];
    const char *var_name;
    int len;

    get_var_name(var_list[var_selected], &var_name, &len, buffer);
    if(len > VAR_SEARCH_MAX)
      len = VAR_SEARCH_MAX;

    memcpy(previous_node_name, focus->name, DEBUG_NODE_NAME_SIZE);
    previous_node_name[DEBUG_NODE_NAME_SIZE - 1] = '\0';
    memcpy(previous_var, var_name, len + 1);
    previous_var[len] = '\0';
  }

  // Clear the big dumb tree first
  clear_debug_tree(&root, true);

  // Get rid of the tree view
  free_tree_list(tree_list, tree_size);

  // We don't need to free anything this list points
  // to because clear_debug_tree already did it.
  free(var_list);

  // Prevent UI keys from carrying through.
  force_release_all_keys();
}

/************************************************/
/* Debug box, which shows actually useful stuff */
/************************************************/

void __draw_debug_box(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys)
{
  struct board *src_board = mzx_world->current_board;
  char version_string[16];
  int version_string_len;
  char key_string[4];
  int key;
  int robot_mem = 0;
  int i;

  draw_window_box(x, y, x + 19, y + 5, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
   DI_DEBUG_BOX_CORNER, 0, 1);

  write_string
  (
    "X/Y:        /     \n"
    "Board:            \n"
    "Robot mem:      kb\n",
    x + 1, y + 1, DI_DEBUG_LABEL, WR_NEWLINE
  );

  version_string_len = get_version_string(version_string, mzx_world->version);
  write_string(version_string, x + 19 - version_string_len, y, DI_DEBUG_BOX, WR_NONE);

  if(show_keys)
  {
    // key_code
    key = get_last_key(keycode_pc_xt);
    if(key && show_keys)
    {
      sprintf(key_string, "%d", key);
      write_string(key_string, x + 15 - strlen(key_string), y + 5,
       DI_DEBUG_BOX_DARK + 0x0A, WR_NONE);
    }

    // key_pressed

    // In 2.82X through 2.84X, this erroneously applied
    // numlock translations to the keycode.
    if(mzx_world->version >= V282 && mzx_world->version <= V284)
      key = get_last_key(keycode_internal_wrt_numlock);

    else
      key = get_last_key(keycode_internal);

    if(key && show_keys)
    {
      sprintf(key_string, "%d", key);
      write_string(key_string, x + 19 - strlen(key_string), y + 5,
       DI_DEBUG_BOX_DARK + 0x0D, WR_NONE);
    }
  }

  write_number(d_x, DI_DEBUG_NUMBER, x + 8, y + 1, 5, 0, 10);
  write_number(d_y, DI_DEBUG_NUMBER, x + 14, y + 1, 5, 0, 10);
  write_number(mzx_world->current_board_id, DI_DEBUG_NUMBER,
   x + 18, y + 2, 0, 1, 10);

  for(i = 0; i < src_board->num_robots_active; i++)
  {
    robot_mem +=
#ifdef CONFIG_DEBYTECODE
     (src_board->robot_list_name_sorted[i])->program_source_length;
#else
     (src_board->robot_list_name_sorted[i])->program_bytecode_length;
#endif
  }

  write_number((robot_mem + 512) / 1024, DI_DEBUG_NUMBER,
   x + 12, y + 3, 5, 0, 10);

  if(*(src_board->mod_playing) != 0)
  {
    if(strlen(src_board->mod_playing) > 18)
    {
      char tempc = src_board->mod_playing[18];
      src_board->mod_playing[18] = 0;
      write_string(src_board->mod_playing, x + 1, y + 4,
       DI_DEBUG_NUMBER, WR_NONE);
      src_board->mod_playing[18] = tempc;
    }
    else
    {
      write_string(src_board->mod_playing, x + 1, y + 4,
       DI_DEBUG_NUMBER, WR_NONE);
    }
  }
  else
  {
    write_string("(no module)", x + 2, y + 4, DI_DEBUG_NUMBER, WR_NONE);
  }
}
