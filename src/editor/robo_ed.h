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

#ifndef MEGAZEUX_EDITOR_ROBO_ED_H
#define MEGAZEUX_EDITOR_ROBO_ED_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"
#include "../intake.h"
#include "../rasm.h"

#define COMMAND_BUFFER_LEN 512

#ifdef CONFIG_DEBYTECODE

enum command_type
 {
  COMMAND_TYPE_COMMAND_START,
  COMMAND_TYPE_COMMAND_CONTINUE,
  COMMAND_TYPE_BLANK_LINE,
  COMMAND_TYPE_INVALID,
  COMMAND_TYPE_UNKNOWN,
};

struct color_code_pair
{
  enum arg_type_indexed arg_type_indexed;
  int offset;
  int length;
};

#else /* !CONFIG_DEBYTECODE */

enum validity_types
{
  valid,
  invalid_uncertain,
  invalid_discard,
  invalid_comment
};

#endif /* !CONFIG_DEBYTECODE */

struct robot_line
{
  int line_text_length;
  char *line_text;

#ifdef CONFIG_DEBYTECODE
  struct color_code_pair *color_codes;
  enum command_type command_type;
  int num_color_codes;
#else
  enum validity_types validity_status;
  int line_bytecode_length;
  char *line_bytecode;
  char arg_types[20];
  int num_args;
#endif

  struct robot_line *next;
  struct robot_line *previous;
};

struct undo_history;

struct robot_editor_context
{
  context ctx;
  subcontext *intk;
  struct robot *cur_robot;

  int current_line;
  struct robot_line *current_rline;
  int total_lines;
  int size;
  int max_size;
  int current_x;
  int current_line_len;
  int mark_mode;
  int mark_start;
  int mark_end;
  boolean scr_hide_mode;
  int scr_line_start;
  int scr_line_middle;
  int scr_line_end;
  struct robot_line base;
  struct robot_line *mark_start_rline;
  struct robot_line *mark_end_rline;
  char *command_buffer;
  char command_buffer_space[COMMAND_BUFFER_LEN];
  int macro_recurse_level;
  int macro_repeat_level;

#ifdef CONFIG_DEBYTECODE
  boolean program_modified;
  boolean confirm_changes;
#else
  enum validity_types default_invalid;
#endif

  /* Undo history and related variables. */
  struct undo_history *h;
  enum intake_event_type current_frame_type;
  int idle_timer;
};

void robot_editor(context *parent, struct robot *cur_robot);

EDITOR_LIBSPEC void init_macros(void);

/* Internal functions. */
void robo_ed_goto_line(struct robot_editor_context *rstate, int line, int column);
void robo_ed_delete_current_line(struct robot_editor_context *rstate, int move);
void robo_ed_add_line(struct robot_editor_context *rstate, char *value, int relation);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_ROBO_ED_H */
