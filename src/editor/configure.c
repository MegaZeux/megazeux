/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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
#include "configure.h"
#include "robo_ed.h"

#include "../counter.h"
#include "../configure.h"
#include "../const.h"
#include "../util.h"
#include "../io/vio.h"
#include "../platform/log.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static struct editor_config_info editor_conf;
static struct editor_config_info editor_conf_backup;

static const struct editor_config_info editor_conf_default =
{
  // Board editor options
  false,                        // board_editor_hide_help
  false,                        // editor_space_toggles
  false,                        // editor_tab_focuses_view
  false,                        // editor_load_board_assets
  true,                         // editor_thing_menu_places
  true,                         // editor_show_thing_toggles
  4,                            // editor_show_thing_blink_speed
  100,                          // Undo history size

  // Defaults for new boards
  0,                            // viewport_x
  0,                            // viewport_y
  80,                           // viewport_w
  25,                           // viewport_h
  100,                          // board_width
  100,                          // board_height
  1,                            // can_shoot
  1,                            // can_bomb
  0,                            // fire_burns_spaces
  1,                            // fire_burns_fakes
  1,                            // fire_burns_trees
  0,                            // fire_burns_brown
  0,                            // fire_burns_forever
  1,                            // forest_to_floor
  1,                            // collect_bombs
  true,                         // dragons_can_randomly_move
  0,                            // restart_if_hurt
  0,                            // reset_on_entry
  true,                         // reset_on_entry_same_board
  0,                            // player_locked_ns
  0,                            // player_locked_ew
  0,                            // player_locked_att
  0,                            // time_limit
  EXPL_LEAVE_ASH,               // explosions_leave (default = ash)
  CAN_SAVE,                     // saving_enabled (default = enabled)
  OVERLAY_ON,                   // overlay_enabled (default = enabled)
  "",                           // charset_path
  "",                           // palette_path

  // Palette editor options
  false,                        // palette_editor_hide_help

  // Robot editor options
  true,                         // editor_enter_splits
  { 11, 10, 10, 14, 255, 3, 11, 2, 14, 0, 15, 11, 7, 15, 1, 2, 3 },
  true,                         // color_coding_on
  1,                            // default_invalid_status
  true,                         // disassemble_extras
  10,                           // disassemble_base
  false,                        // robot_editor_hide_help

  // Backup options
  3,                            // backup_count
  60,                           // backup_interval
  "backup",                     // backup_name
  ".mzx",                       // backup_ext

  // Macro options
  { "char ", "color ", "goto ", "send ", ": playershot^" },
  0,                            // num_extended_macros
  0,
  NULL,

  // saved_positions
  { { 0, 0, 0, 0, 0, 0 } },
  // vlayer_positions
  { { 0, 0, 0, 0, 0, 0 } },
};

typedef void (* editor_config_function)(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data);

struct editor_config_entry
{
  const char *option_name;
  editor_config_function change_option;
};

static const struct config_enum default_invalid_status_values[] =
{
#ifndef CONFIG_DEBYTECODE
  { "ignore", invalid_uncertain },
  { "delete", invalid_discard },
  { "comment", invalid_comment }
#else
  { "", 0 }
#endif
};

static const struct config_enum disassemble_base_values[] =
{
  { "10", 10 },
  { "16", 16 }
};

static const struct config_enum explosions_leave_values[] =
{
  { "space", EXPL_LEAVE_SPACE },
  { "ash", EXPL_LEAVE_ASH },
  { "fire", EXPL_LEAVE_FIRE }
};

static const struct config_enum overlay_enabled_values[] =
{
  { "disabled", OVERLAY_OFF },
  { "enabled", OVERLAY_ON },
  { "static", OVERLAY_STATIC },
  { "transparent", OVERLAY_TRANSPARENT },
};

static const struct config_enum saving_enabled_values[] =
{
  { "disabled", CANT_SAVE },
  { "enabled", CAN_SAVE },
  { "sensoronly", CAN_SAVE_ON_SENSOR }
};

// Default colors for color coding:
// 0 current line - 11
// 1 immediate  - 10
// 2 immediates - 10 (unused?)
// 3 characters - 14
// 4 colors - color box (255) or 2
// 5 directions - 3
// 6 things - 11
// 7 params - 2
// 8 strings - 14
// 9 equalities - 0
// 10 conditions - 15
// 11 items - 11
// 12 extras - 7
// 13 commands and command fragments - 15
// 14 invalid line (ignore)
// 15 invalid line (delete)
// 16 invalid line (comment)

static void config_ccode_chars(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_CHARACTERS] = result;
}

static void config_ccode_colors(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
  {
    conf->color_codes[ROBO_ED_CC_COLORS] = result;
  }
  else

  // Special case- 255 instructs the robot editor to use a color box.
  if(!strcmp(value, "255"))
    conf->color_codes[ROBO_ED_CC_COLORS] = 255;
}

static void config_ccode_commands(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_COMMANDS] = result;
}

static void config_ccode_conditions(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_CONDITIONS] = result;
}

static void config_ccode_current_line(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_CURRENT_LINE] = result;
}

static void config_ccode_directions(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_DIRECTIONS] = result;
}

static void config_ccode_equalities(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_EQUALITIES] = result;
}

static void config_ccode_extras(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_EXTRAS] = result;
}

static void config_ccode_on(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->color_coding_on, value);
}

static void config_ccode_immediates(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
  {
    conf->color_codes[ROBO_ED_CC_IMMEDIATES] = result;
    conf->color_codes[ROBO_ED_CC_IMMEDIATES_2] = result;
  }
}

static void config_ccode_items(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_ITEMS] = result;
}

static void config_ccode_params(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_PARAMS] = result;
}

static void config_ccode_strings(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_STRINGS] = result;
}

static void config_ccode_things(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 15))
    conf->color_codes[ROBO_ED_CC_THINGS] = result;
}

static void config_default_invalid(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, default_invalid_status_values))
    conf->default_invalid_status = result;
}

static void config_disassemble_extras(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->disassemble_extras, value);
}

static void config_disassemble_base(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, disassemble_base_values))
    conf->disassemble_base = result;
}

static void config_macro(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  const char *macro_name = name + 6;

  if(isdigit((int)macro_name[0]) && !macro_name[1] && !extended_data)
  {
    size_t macro_num = macro_name[0] - '1';
    if(macro_num < ARRAY_SIZE(conf->default_macros))
      config_string(conf->default_macros[macro_num], value);
  }
  else
  {
    if(extended_data)
      add_ext_macro(conf, macro_name, extended_data, value);
  }
}

static void board_editor_hide_help(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->board_editor_hide_help, value);
}

static void robot_editor_hide_help(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->robot_editor_hide_help, value);
}

static void palette_editor_hide_help(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->palette_editor_hide_help, value);
}

static void backup_count(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, INT_MAX))
    conf->backup_count = result;
}

static void backup_interval(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, INT_MAX))
    conf->backup_interval = result;
}

static void backup_name(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->backup_name, value);
}

static void backup_ext(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->backup_ext, value);
}

static void config_editor_space_toggles(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_space_toggles, value);
}

static void config_editor_enter_splits(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_enter_splits, value);
}

static void config_editor_load_board_assets(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_load_board_assets, value);
}

static void config_editor_tab_focus(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_tab_focuses_view, value);
}

static void config_editor_thing_menu_places(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_thing_menu_places, value);
}

static void config_editor_show_thing_toggles(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->editor_show_thing_toggles, value);
}

static void config_editor_show_thing_blink_speed(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, INT_MAX))
    conf->editor_show_thing_blink_speed = result;
}

static void config_undo_history_size(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 1000))
    conf->undo_history_size = result;
}

static void config_saved_positions(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int pos;
  unsigned int board_num;
  unsigned int board_x;
  unsigned int board_y;
  unsigned int scroll_x;
  unsigned int scroll_y;
  unsigned int debug_x;
  int n;

  if(sscanf(name, "saved_position%u", &pos) != 1)
    return;

  if(sscanf(value, "%u, %u, %u, %u, %u, %u%n",
   &board_num, &board_x, &board_y, &scroll_x, &scroll_y, &debug_x, &n) != 6 ||
   value[n])
    return;

  // Check for sane values only. This is not guaranteed to properly bound these.
  if(pos < NUM_SAVED_POSITIONS && (board_num < MAX_BOARDS) &&
   (board_x < 32768) && (board_y < 32768) && (scroll_x < 32768) &&
   (scroll_y < 32768) && (debug_x < 80))
  {
    struct saved_position *s = &(conf->saved_positions[pos]);
    s->board_id = board_num;
    s->cursor_x = board_x;
    s->cursor_y = board_y;
    s->scroll_x = scroll_x;
    s->scroll_y = scroll_y;
    s->debug_x = debug_x ? 60 : 0;
  }
}

static void config_vlayer_positions(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  unsigned int pos;
  unsigned int vlayer_x;
  unsigned int vlayer_y;
  unsigned int scroll_x;
  unsigned int scroll_y;
  unsigned int debug_x;
  int n;

  if(sscanf(name, "vlayer_position%u", &pos) != 1)
    return;

  if(sscanf(value, "%u, %u, %u, %u, %u%n",
   &vlayer_x, &vlayer_y, &scroll_x, &scroll_y, &debug_x, &n) != 5 ||
   value[n])
    return;

  // Check for sane values only. The editor needs to properly bound these later.
  if(pos < NUM_SAVED_POSITIONS && (vlayer_x < 32768) && (vlayer_y < 32768) &&
   (scroll_x < 32768) && (scroll_y < 32768) && (debug_x < 80))
  {
    struct saved_position *s = &(conf->vlayer_positions[pos]);
    s->board_id = 0;
    s->cursor_x = vlayer_x;
    s->cursor_y = vlayer_y;
    s->scroll_x = scroll_x;
    s->scroll_y = scroll_y;
    s->debug_x = debug_x ? 60 : 0;
  }
}

/******************/
/* BOARD DEFAULTS */
/******************/

static void config_board_width(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 32767))
  {
    conf->board_width = result;
    if(conf->viewport_w > conf->board_width)
      conf->viewport_w = conf->board_width;
    if(conf->board_width * conf->board_height > MAX_BOARD_SIZE)
      conf->board_height = MAX_BOARD_SIZE / conf->board_width;
  }
}

static void config_board_height(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 32767))
  {
    conf->board_height = result;
    if(conf->viewport_h > conf->board_height)
      conf->viewport_h = conf->board_height;
    if(conf->board_width * conf->board_height > MAX_BOARD_SIZE)
      conf->board_width = MAX_BOARD_SIZE / conf->board_height;
  }
}

static void config_board_viewport_w(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 80))
  {
    conf->viewport_w = MIN(result, conf->board_width);
    if(conf->viewport_x + conf->viewport_w > 80)
      conf->viewport_x = 80 - conf->viewport_w;
  }
}

static void config_board_viewport_h(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 1, 25))
  {
    conf->viewport_h = MIN(result, conf->board_height);
    if(conf->viewport_y + conf->viewport_h > 25)
      conf->viewport_y = 25 - conf->viewport_h;
  }
}

static void config_board_viewport_x(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 79))
    conf->viewport_x = MIN(result, 80 - conf->viewport_w);
}

static void config_board_viewport_y(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 24))
    conf->viewport_y = MIN(result, 25 - conf->viewport_h);
}

static void config_board_can_shoot(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->can_shoot, value);
}

static void config_board_can_bomb(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->can_bomb, value);
}

static void config_board_fire_spaces(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fire_burns_spaces, value);
}

static void config_board_fire_fakes(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fire_burns_fakes, value);
}

static void config_board_fire_trees(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fire_burns_trees, value);
}

static void config_board_fire_brown(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fire_burns_brown, value);
}

static void config_board_fire_forever(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->fire_burns_forever, value);
}

static void config_board_forest(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->forest_to_floor, value);
}

static void config_board_collect_bombs(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->collect_bombs, value);
}

static void config_board_dragons_random_move(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->dragons_can_randomly_move, value);
}

static void config_board_restart(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->restart_if_hurt, value);
}

static void config_board_reset_on_entry(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->reset_on_entry, value);
}

static void config_board_reset_on_entry_same(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->reset_on_entry_same_board, value);
}

static void config_board_locked_ns(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->player_locked_ns, value);
}

static void config_board_locked_ew(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->player_locked_ew, value);
}

static void config_board_locked_att(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_boolean(&conf->player_locked_att, value);
}

static void config_board_time_limit(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_int(&result, value, 0, 32767))
    conf->time_limit = result;
}

static void config_board_charset(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->charset_path, value);
}

static void config_board_palette(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  config_string(conf->palette_path, value);
}

static void config_board_explosions(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, explosions_leave_values))
    conf->explosions_leave = result;
}

static void config_board_saving(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, saving_enabled_values))
    conf->saving_enabled = result;
}

static void config_board_overlay(struct editor_config_info *conf,
 const char *name, const char *value, const char *extended_data)
{
  int result;
  if(config_enum(&result, value, overlay_enabled_values))
    conf->overlay_enabled = result;
}

/**********************/
/* END BOARD DEFAULTS */
/**********************/

/* FAT NOTE: This is searched as a binary tree, the nodes must be
 *           sorted alphabetically, or they risk being ignored.
 */
static const struct editor_config_entry editor_config_options[] =
{
  { "backup_count", backup_count },
  { "backup_ext", backup_ext },
  { "backup_interval", backup_interval },
  { "backup_name", backup_name },
  { "board_default_can_bomb", config_board_can_bomb },
  { "board_default_can_shoot", config_board_can_shoot },
  { "board_default_charset_path", config_board_charset },
  { "board_default_collect_bombs", config_board_collect_bombs },
  { "board_default_dragons_can_randomly_move", config_board_dragons_random_move },
  { "board_default_explosions_leave", config_board_explosions },
  { "board_default_fire_burns_brown", config_board_fire_brown },
  { "board_default_fire_burns_fakes", config_board_fire_fakes },
  { "board_default_fire_burns_forever", config_board_fire_forever },
  { "board_default_fire_burns_spaces", config_board_fire_spaces },
  { "board_default_fire_burns_trees", config_board_fire_trees },
  { "board_default_forest_to_floor", config_board_forest },
  { "board_default_height", config_board_height },
  { "board_default_overlay", config_board_overlay },
  { "board_default_palette_path", config_board_palette },
  { "board_default_player_locked_att", config_board_locked_att },
  { "board_default_player_locked_ew", config_board_locked_ew },
  { "board_default_player_locked_ns", config_board_locked_ns },
  { "board_default_reset_on_entry", config_board_reset_on_entry },
  { "board_default_reset_on_entry_same_board", config_board_reset_on_entry_same },
  { "board_default_restart_if_hurt", config_board_restart },
  { "board_default_saving", config_board_saving },
  { "board_default_time_limit", config_board_time_limit },
  { "board_default_viewport_h", config_board_viewport_h },
  { "board_default_viewport_w", config_board_viewport_w },
  { "board_default_viewport_x", config_board_viewport_x },
  { "board_default_viewport_y", config_board_viewport_y },
  { "board_default_width", config_board_width },
  { "board_editor_hide_help", board_editor_hide_help },
  { "ccode_chars", config_ccode_chars },
  { "ccode_colors", config_ccode_colors },
  { "ccode_commands", config_ccode_commands },
  { "ccode_conditions", config_ccode_conditions },
  { "ccode_current_line", config_ccode_current_line },
  { "ccode_directions", config_ccode_directions },
  { "ccode_equalities", config_ccode_equalities },
  { "ccode_extras", config_ccode_extras },
  { "ccode_immediates", config_ccode_immediates },
  { "ccode_items", config_ccode_items },
  { "ccode_params", config_ccode_params },
  { "ccode_strings", config_ccode_strings },
  { "ccode_things", config_ccode_things },
  { "color_coding_on", config_ccode_on },
  { "default_invalid_status", config_default_invalid },
  { "disassemble_base", config_disassemble_base },
  { "disassemble_extras", config_disassemble_extras },
  { "editor_enter_splits", config_editor_enter_splits },
  { "editor_load_board_assets", config_editor_load_board_assets },
  { "editor_show_thing_blink_speed", config_editor_show_thing_blink_speed },
  { "editor_show_thing_toggles", config_editor_show_thing_toggles },
  { "editor_space_toggles", config_editor_space_toggles },
  { "editor_tab_focuses_view", config_editor_tab_focus },
  { "editor_thing_menu_places", config_editor_thing_menu_places },
  { "macro_*", config_macro },
  { "palette_editor_hide_help", palette_editor_hide_help },
  { "robot_editor_hide_help", robot_editor_hide_help },
  { "saved_position!", config_saved_positions },
  { "undo_history_size", config_undo_history_size },
  { "vlayer_position!", config_vlayer_positions },
};

static const struct editor_config_entry *find_editor_option(const char *name,
 const struct editor_config_entry options[], int num_options)
{
  int cmpval, top = num_options - 1, middle, bottom = 0;
  const struct editor_config_entry *base = options;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = match_function_counter(name, (base[middle]).option_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

static boolean editor_config_change_option(void *conf, const char *name,
 const char *value, const char *extended_data)
{
  const struct editor_config_entry *current_option = find_editor_option(name,
   editor_config_options, ARRAY_SIZE(editor_config_options));

  if(current_option)
  {
    current_option->change_option(conf, name, value, extended_data);
    return true;
  }
  return false;
}

struct editor_config_info *get_editor_config(void)
{
  return &editor_conf;
}

void default_editor_config(void)
{
  static boolean registered = false;
  memcpy(&editor_conf, &editor_conf_default, sizeof(struct editor_config_info));

  if(!registered)
  {
    register_config(SYSTEM_CNF, &editor_conf, editor_config_change_option);
    register_config(GAME_EDITOR_CNF, &editor_conf, editor_config_change_option);
    registered = true;
  }
}

void store_editor_config_backup(void)
{
  // Back up the config.
  memcpy(&editor_conf_backup, &editor_conf, sizeof(struct editor_config_info));
  editor_conf_backup.extended_macros = NULL;
}

void load_editor_config_backup(void)
{
  // Restore the editor config (but keep the macros).
  int num_macros = editor_conf.num_extended_macros;
  int num_macros_alloc = editor_conf.num_macros_allocated;
  struct ext_macro **macros = editor_conf.extended_macros;

  memcpy(&editor_conf, &editor_conf_backup, sizeof(struct editor_config_info));

  editor_conf.extended_macros = macros;
  editor_conf.num_extended_macros = num_macros;
  editor_conf.num_macros_allocated = num_macros_alloc;
}

static void __free_editor_config(struct editor_config_info *conf)
{
  // Extended Macros
  if(conf->extended_macros)
  {
    int i;
    for(i = 0; i < conf->num_extended_macros; i++)
      free_macro(conf->extended_macros[i]);

    free(conf->extended_macros);
    conf->extended_macros = NULL;
  }
}

void free_editor_config(void)
{
  __free_editor_config(&editor_conf);
  __free_editor_config(&editor_conf_backup);
}

static size_t editor_config_full_name(char *dest, size_t dest_len,
 const char *path, size_t path_len)
{
  size_t ret = snprintf(dest, dest_len, "%.*s.editor.cnf",
   (int)path_len - 4, path);
  dest[dest_len - 1] = '\0';
  return ret;
}

static size_t editor_config_dos_name(char *dest, size_t dest_len,
 const char *path, size_t path_len)
{
  size_t ret = snprintf(dest, dest_len, "%.*s.cne",
   (int)path_len - 4, path);
  dest[dest_len - 1] = '\0';
  return ret;
}

/* Get the existing world file editor config name, if present. It may have
 * the extension .editor.cnf (usual) or the extension .cne (DOS). */
size_t get_local_editor_config_name(char *dest, size_t dest_len,
 const char *mzx_file_path)
{
  struct stat file_info;
  size_t path_len = strlen(mzx_file_path);
  size_t ret;

  if(path_len < 4)
    return 0;

  ret = editor_config_full_name(dest, dest_len, mzx_file_path, path_len);
  if(ret < dest_len &&
   vstat(dest, &file_info) >= 0 && S_ISREG(file_info.st_mode))
  {
    debug("found editor config: %s\n", dest);
    return ret;
  }

  ret = editor_config_dos_name(dest, dest_len, mzx_file_path, path_len);
  if(ret < dest_len &&
   vstat(dest, &file_info) >= 0 && S_ISREG(file_info.st_mode))
  {
    debug("found editor config: %s\n", dest);
    return ret;
  }

  return 0;
}

// TODO: update this function once there's a good config rewriting routine.
void save_local_editor_config(struct editor_config_info *conf,
 const char *mzx_file_path)
{
  size_t mzx_file_len = strlen(mzx_file_path);
  char config_file_name[MAX_PATH];

  const char comment[]   = "\n############################################################";
  const char comment_a[] = "\n#####  Editor generated configuration - do not modify  #####";
  const char comment_b[] = "\n#####        End editor generated configuration        #####";
  const char *value;
  vfile *vf = NULL;
  boolean exists = false;
  int i;

  if(mzx_file_len < 4)
    return;

  // Does it exist?
  if(get_local_editor_config_name(config_file_name, MAX_PATH, mzx_file_path) > 0)
    vf = vfopen_unsafe(config_file_name, "rb");

  if(vf)
  {
    char *a, *b;
    char *config_file = NULL;
    char *config_file_b = NULL;
    int config_file_size = 0;
    int config_file_size_b = 0;

    exists = true;

    config_file_size = vfilelength(vf, true);
    config_file = cmalloc(config_file_size + 1);

    config_file_size = vfread(config_file, 1, config_file_size, vf);
    config_file[config_file_size] = 0;
    vfclose(vf);

    a = strstr(config_file, comment_a);
    b = strstr(config_file, comment_b);

    // Start of special section
    if(a)
    {
      a[0] = '\0';
      a = strrchr(config_file, '\n');
      if(a)
      {
        if(a > config_file && a[-1] == '\r')
          a[-1] = '\0';
        else
          a[0] = '\0';

        config_file_size = strlen(config_file);
      }
      else
      {
        config_file[0] = '\0';
        config_file_size = 0;
      }

      // End of special section
      // Skip matched line and comment line afterward
      if(b && (b = strchr(b + 1, '\n')) && (b = strchr(b + 1, '\n')))
      {
        config_file_b = b + 1;
        config_file_size_b = strlen(config_file_b);
      }
    }

    vf = vfopen_unsafe(config_file_name, "wb");
    if(!vf)
    {
      warn("failed to rewrite editor config file\n");
      return;
    }

    if(config_file_size)
      vfwrite(config_file, 1, config_file_size, vf);
    if(config_file_size_b)
      vfwrite(config_file_b, 1, config_file_size_b, vf);

    vfclose(vf);
    free(config_file);
  }

  if(!exists)
  {
    // Try creating a file with the normal name first. This may fail in DOS.
    editor_config_full_name(config_file_name, MAX_PATH,
     mzx_file_path, mzx_file_len);
  }

  vf = vfopen_unsafe(config_file_name, "a");
  if(!vf)
  {
    if(!exists)
    {
      editor_config_dos_name(config_file_name, MAX_PATH,
       mzx_file_path, mzx_file_len);
      vf = vfopen_unsafe(config_file_name, "a");
    }
    if(!vf)
    {
      warn("failed to rewrite editor config file\n");
      return;
    }
  }
  debug("writing editor config: %s\n", config_file_name);

  vf_printf(vf, "%s%s%s\n\n", comment, comment_a, comment);

  vf_printf(vf, "board_default_width = %d\n", conf->board_width);
  vf_printf(vf, "board_default_height = %d\n", conf->board_height);
  vf_printf(vf, "board_default_viewport_w = %d\n", conf->viewport_w);
  vf_printf(vf, "board_default_viewport_h = %d\n", conf->viewport_h);
  vf_printf(vf, "board_default_viewport_x = %d\n", conf->viewport_x);
  vf_printf(vf, "board_default_viewport_y = %d\n", conf->viewport_y);
  vf_printf(vf, "board_default_can_shoot = %d\n", conf->can_shoot);
  vf_printf(vf, "board_default_can_bomb = %d\n", conf->can_bomb);
  vf_printf(vf, "board_default_fire_burns_spaces = %d\n", conf->fire_burns_spaces);
  vf_printf(vf, "board_default_fire_burns_fakes = %d\n", conf->fire_burns_fakes);
  vf_printf(vf, "board_default_fire_burns_trees = %d\n", conf->fire_burns_trees);
  vf_printf(vf, "board_default_fire_burns_brown = %d\n", conf->fire_burns_brown);
  vf_printf(vf, "board_default_fire_burns_forever = %d\n", conf->fire_burns_forever);
  vf_printf(vf, "board_default_forest_to_floor = %d\n", conf->forest_to_floor);
  vf_printf(vf, "board_default_collect_bombs = %d\n", conf->collect_bombs);
  vf_printf(vf, "board_default_dragons_can_randomly_move = %d\n",
   conf->dragons_can_randomly_move);
  vf_printf(vf, "board_default_restart_if_hurt = %d\n", conf->restart_if_hurt);
  vf_printf(vf, "board_default_reset_on_entry = %d\n", conf->reset_on_entry);
  vf_printf(vf, "board_default_reset_on_entry_same_board = %d\n",
   conf->reset_on_entry_same_board);
  vf_printf(vf, "board_default_player_locked_ns = %d\n", conf->player_locked_ns);
  vf_printf(vf, "board_default_player_locked_ew = %d\n", conf->player_locked_ew);
  vf_printf(vf, "board_default_player_locked_att = %d\n", conf->player_locked_att);
  vf_printf(vf, "board_default_time_limit = %d\n", conf->time_limit);
  vf_printf(vf, "board_default_charset_path = %s\n", conf->charset_path);
  vf_printf(vf, "board_default_palette_path = %s\n", conf->palette_path);

  switch(conf->explosions_leave)
  {
    default:
    case EXPL_LEAVE_SPACE:
      value = "space";
      break;
    case EXPL_LEAVE_ASH:
      value = "ash";
      break;
    case EXPL_LEAVE_FIRE:
      value = "fire";
      break;
  }
  vf_printf(vf, "board_default_explosions_leave = %s\n", value);

  switch(conf->saving_enabled)
  {
    default:
    case CANT_SAVE:
      value = "disabled";
      break;
    case CAN_SAVE:
      value = "enabled";
      break;
    case CAN_SAVE_ON_SENSOR:
      value = "sensoronly";
      break;
  }
  vf_printf(vf, "board_default_saving = %s\n", value);

  switch(conf->overlay_enabled)
  {
    default:
    case OVERLAY_OFF:
      value = "disabled";
      break;
    case OVERLAY_ON:
      value = "enabled";
      break;
    case OVERLAY_STATIC:
      value = "static";
      break;
    case OVERLAY_TRANSPARENT:
      value = "transparent";
      break;
  }
  vf_printf(vf, "board_default_overlay = %s\n", value);

  vfputs("\n", vf);
  for(i = 0; i < NUM_SAVED_POSITIONS; i++)
  {
    struct saved_position *s = &(conf->saved_positions[i]);
    vf_printf(vf, "saved_position%u = %u, %u, %u, %u, %u, %u\n", i,
     s->board_id, s->cursor_x, s->cursor_y, s->scroll_x, s->scroll_y, s->debug_x);
  }
  for(i = 0; i < NUM_SAVED_POSITIONS; i++)
  {
    struct saved_position *s = &(conf->vlayer_positions[i]);
    vf_printf(vf, "vlayer_position%u = %u, %u, %u, %u, %u\n", i,
     s->cursor_x, s->cursor_y, s->scroll_x, s->scroll_y, s->debug_x);
  }

  vf_printf(vf, "%s%s%s\n", comment, comment_b, comment);
  vfclose(vf);
}
