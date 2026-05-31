/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_ROBOT_H
#define MEGAZEUX_ROBOT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"
#include "data.h"

struct zip_archive;

// Let's not let a robot's stack get larger than 64k right now.
// The value is a bit arbitrary, but it's mainly there to prevent MZX from
// crashing when under infinite recursion.

#define ROBOT_START_STACK 8
#define ROBOT_MAX_STACK   65536

#define DEBUG_EXIT 1
#define DEBUG_GOTO 2
#define DEBUG_HALT 3

/* Internal use only */
enum builtin_label
{
  LABEL_TOUCH,
  LABEL_BOMBED,
  LABEL_INVINCO,
  LABEL_PUSHED,
  LABEL_PLAYERSHOT,
  LABEL_NEUTRALSHOT,
  LABEL_ENEMYSHOT,
  LABEL_PLAYERHIT,
  LABEL_LAZER,
  LABEL_SPITFIRE,
  LABEL_JUSTLOADED,
  LABEL_JUSTENTERED,
  LABEL_GOOPTOUCHED,
  LABEL_PLAYERHURT,
  LABEL_PLAYERDIED
};

enum rel_prefix
{
  REL_NONE = 0,
  REL_TO_SELF = 1,
  REL_TO_PLAYER = 2,
  REL_TO_XPOS_YPOS = 3,
  REL_TO_SELF_FIRST_OR_LAST = 5,
  REL_TO_PLAYER_FIRST_OR_LAST = 6,
  REL_TO_XPOS_YPOS_FIRST_OR_LAST = 7,
};

#ifdef CONFIG_DEBYTECODE

CORE_LIBSPEC void change_robot_name(struct board *src_board,
 struct robot *cur_robot, char *new_name);
CORE_LIBSPEC void add_robot_name_entry(struct board *src_board,
 struct robot *cur_robot, char *name);
CORE_LIBSPEC int find_free_robot(struct board *src_board);

void prepare_robot_bytecode(struct world *mzx_world, struct robot *cur_robot);
void translate_robot_bytecode_offsets(struct world *mzx_world,
 struct robot *cur_robot, int file_version);

#else /* !CONFIG_DEBYTECODE */

CORE_LIBSPEC void reallocate_robot(struct robot *robot, int size);

void change_robot_name(struct board *src_board, struct robot *cur_robot,
 char *new_name);
void add_robot_name_entry(struct board *src_board, struct robot *cur_robot,
 char *name);
int find_free_robot(struct board *src_board);

void fix_robot_stack_offsets(struct robot *cur_robot);

#endif /* !CONFIG_DEBYTECODE */

CORE_LIBSPEC void cache_robot_labels(struct robot *robot);
CORE_LIBSPEC void clear_label_cache(struct robot *cur_robot);

CORE_LIBSPEC void clear_robot_contents(struct robot *cur_robot);
CORE_LIBSPEC void clear_robot_id(struct board *src_board, int id);
CORE_LIBSPEC void clear_scroll_id(struct board *src_board, int id);
CORE_LIBSPEC void clear_sensor_id(struct board *src_board, int id);
CORE_LIBSPEC void replace_robot(struct world *mzx_world,
 struct board *src_board, struct robot *src_robot, int dest_id);
CORE_LIBSPEC int duplicate_robot(struct world *mzx_world,
 struct board *src_board, struct robot *cur_robot, int x, int y,
 int preserve_state);
CORE_LIBSPEC int duplicate_scroll(struct board *src_board,
 struct scroll *cur_scroll);
CORE_LIBSPEC int duplicate_sensor(struct board *src_board,
 struct sensor *cur_sensor);
CORE_LIBSPEC int send_robot_id_def(struct world *mzx_world, int robot_id,
 const char *mesg, int ignore_lock);
CORE_LIBSPEC void send_robot_all_def(struct world *mzx_world, const char *mesg);
CORE_LIBSPEC void send_robot_def(struct world *mzx_world, int robot_id,
 enum builtin_label mesg_id);
CORE_LIBSPEC void optimize_null_objects(struct board *src_board);

CORE_LIBSPEC int place_at_xy(struct world *mzx_world, enum thing id,
 int color, int param, int x, int y);
CORE_LIBSPEC int place_player_xy(struct world *mzx_world, int x, int y);
CORE_LIBSPEC void setup_overlay(struct board *src_board, int mode);
CORE_LIBSPEC void replace_player(struct world *mzx_world);

void load_robot(struct world *mzx_world, struct robot *cur_robot,
 struct zip_archive *zp, int savegame, int file_version);

struct robot *load_robot_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version);

struct scroll *load_scroll_allocate(struct zip_archive *zp);
struct sensor *load_sensor_allocate(struct zip_archive *zp);

size_t save_robot_calculate_size(struct world *mzx_world,
 struct robot *cur_robot, int savegame, int file_version);

void save_robot(struct world *mzx_world, struct robot *cur_robot,
 struct zip_archive *zp, int savegame, int file_version, const char *name);

void save_scroll(struct scroll *cur_scroll, struct zip_archive *zp,
 int file_version, const char *name);
void save_sensor(struct sensor *cur_sensor, struct zip_archive *zp,
 int file_version, const char *name);

void create_blank_robot(struct robot *cur_robot);
void create_blank_robot_program(struct robot *cur_robot);

void clear_robot(struct robot *cur_robot);
void clear_scroll(struct scroll *cur_scroll);
void clear_sensor(struct sensor *cur_sensor);
void reallocate_scroll(struct scroll *scroll, size_t size);

CORE_LIBSPEC void send_robot(struct world *mzx_world, char *name,
 const char *mesg, int ignore_lock);
CORE_LIBSPEC int send_robot_id(struct world *mzx_world, int id,
 const char *mesg, int ignore_lock);

int find_robot(struct board *src_board, const char *name,
 int *first, int *last);
void send_robot_all(struct world *mzx_world, const char *mesg, int ignore_lock);
int send_robot_self(struct world *mzx_world, struct robot *src_robot,
 const char *mesg, int ignore_lock);
int move_dir(struct board *src_board, int *x, int *y, enum dir dir);
void prefix_first_last_xy(struct world *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty);
void prefix_mid_xy_unbound(struct world *mzx_world, int *mx, int *my, int x, int y);
void prefix_mid_xy(struct world *mzx_world, int *mx, int *my, int x, int y);
void prefix_mid_xy_ext(struct world *mzx_world, struct board *dest_board,
 int *mx, int *my, int x, int y);
void prefix_last_xy_var(struct world *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height);
void prefix_mid_xy_var(struct world *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height);
void prefix_first_xy_var(struct world *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height);
int restore_label(struct robot *cur_robot, char *label);
int zap_label(struct robot *cur_robot, char *label);
int next_param(char *ptr, int pos);
char *next_param_pos(char *ptr);
int parse_param(struct world *mzx_world, char *robot, int id);
enum thing parse_param_thing(struct world *mzx_world, char *program);
enum dir parse_param_dir(struct world *mzx_world, char *program);
enum equality parse_param_eq(struct world *mzx_world, char *program);
enum condition parse_param_cond(struct world *mzx_world, char *program,
 enum dir *direction);
boolean is_robot_box_command(int cmd);
void robot_box_display(struct world *mzx_world, char *program,
 char *label_storage, int id);
void push_sensor(struct world *mzx_world, int id);
void step_sensor(struct world *mzx_world, int id);
int get_robot_id(struct board *src_board, const char *name);

CORE_LIBSPEC void get_robot_position(struct robot *cur_robot, int *xpos,
 int *ypos);

void run_robot(context *ctx, int id, int x, int y);

CORE_LIBSPEC void duplicate_robot_direct(struct world *mzx_world,
 struct robot *cur_robot, struct robot *copy_robot, int x, int y,
 int preserve_state);
CORE_LIBSPEC void duplicate_scroll_direct(struct scroll *cur_scroll,
 struct scroll *copy_scroll);
CORE_LIBSPEC void duplicate_sensor_direct(struct sensor *cur_sensor,
 struct sensor *copy_sensor);

CORE_LIBSPEC int get_program_command_num(struct robot *cur_robot,
 int program_pos);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern const int def_params[128];
#endif // CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ROBOT_H */
