/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2021-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

// TODO: Confirmation dialog for mainstream releases (currently DBC-only)
// FIXME: Confirmation dialog appears when the source hasn't been modified sometimes

// Reconstructed robot editor. This is only a shell - the actual
// robot assembly/disassembly code is in rasm.cpp.

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../caption.h"
#include "../configure.h"
#include "../core.h"
#include "../error.h"
#include "../graphics.h"
#include "../helpsys.h"
#include "../intake.h"
#include "../rasm.h"
#include "../robot.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"
#include "../io/fsafeopen.h"
#include "../io/path.h"
#include "../io/vio.h"

#include "../event/event.h"
#include "../platform/platform_clipboard.h"

#include "char_ed.h"
#include "configure.h"
#include "edit.h"
#include "param.h"
#include "macro.h"
#include "macro_struct.h"
#include "robo_ed.h"
#include "stringsearch.h"
#include "undo.h"
#include "window.h"

#define combine_colors(a, b)  \
  (a) | (b << 4)              \

#define MAX_COMMAND_LEN 240
#define MAX_MACRO_RECURSION 16
#define MAX_MACRO_REPEAT 128
#define MAX_IDLE_TIMER 15

enum search_option
{
  SEARCH_OPTION_NONE,
  SEARCH_OPTION_SEARCH,
  SEARCH_OPTION_REPLACE,
  SEARCH_OPTION_REPLACE_ALL
};

static int copy_buffer_lines;
static int copy_buffer_total_length;
static char **copy_buffer;

static int last_color_selected = 0;
static int last_char_selected = 0;

static enum search_option last_search_action = SEARCH_OPTION_NONE;
static struct string_search_data search_index;
static char search_string[256];
static char replace_string[256];
static boolean search_wrap_enabled = true;
static boolean search_ignore_case_enabled = true;

static const char key_help[(81 * 3) + 1] =
{
  " F1:Help  F2:Color  F3:Char  F4:Param  F5:Char edit  F6-F10:Macros  (see Alt+O) \n"
  " Alt+Home/S:Mark start  Alt+End/E:Mark end  Alt+U:Unmark   Alt+B:Block Action   \n"
  " Alt+Ins:Paste  Alt+X:Export  Alt+I:Import  Alt+V:Verify  Ctrl+I/D/C:Valid Mark \n"
};

static const char key_help_hide[82] =
  "     Press Alt + H to view hotkey information.  Press F1 for extended help.     \n";

// Default colors for color coding:
// current line - 11
// immediates - 10
// characters - 14
// colors - color box or 2
// directions - 3
// things - 11
// params - 2
// strings - 14
// equalities - 0
// conditions - 15
// items - 11
// extras - 7
// commands and command fragments - 15

#define bg_color 8
static const char top_line_connect = 194;
static const char bottom_line_connect = 193;
static const char vertical_line = 179;
static const char horizontal_line = 196;
static const char top_char = 219;
static const char bg_char = 32;
static const char bg_color_solid = combine_colors(0, bg_color);
static const char top_color = 4;
static const char line_color = combine_colors(15, bg_color);
static const char top_text_color = combine_colors(15, 4);
static const char bottom_text_color = combine_colors(15, 1);
static const char top_highlight_color = combine_colors(14, 4);
static const char mark_color = combine_colors(0, 7);

static char macros[5][64];

static void add_blank_line(struct robot_editor_context *rstate, int relation)
{
#ifndef CONFIG_DEBYTECODE
  if(rstate->size + 3 < rstate->max_size)
#endif
  {
    struct robot_line *new_rline = cmalloc(sizeof(struct robot_line));
    struct robot_line *current_rline = rstate->current_rline;
    int current_line = rstate->current_line;

    new_rline->line_text_length = 0;
    new_rline->line_text = cmalloc(1);
    new_rline->line_text[0] = 0;

#ifdef CONFIG_DEBYTECODE
    new_rline->command_type = COMMAND_TYPE_BLANK_LINE;
    new_rline->color_codes = NULL;
#else
    new_rline->line_bytecode = cmalloc(3);
    new_rline->line_bytecode[0] = 1;
    new_rline->line_bytecode[1] = 47;
    new_rline->line_bytecode[2] = 1;
    new_rline->line_bytecode_length = 3;
    new_rline->validity_status = valid;
    rstate->size += 3;
#endif

    rstate->total_lines++;

    if(relation > 0)
    {
      new_rline->next = current_rline->next;
      new_rline->previous = current_rline;

      if(current_rline->next)
        current_rline->next->previous = new_rline;

      current_rline->next = new_rline;

      rstate->current_rline = new_rline;

      if(rstate->mark_mode)
      {
        if(rstate->mark_start > current_line)
          rstate->mark_start++;

        if(rstate->mark_end > current_line)
          rstate->mark_end++;
      }
    }
    else
    {
      new_rline->next = current_rline;
      new_rline->previous = current_rline->previous;

      current_rline->previous->next = new_rline;
      current_rline->previous = new_rline;

      if(rstate->mark_mode)
      {
        if(rstate->mark_start >= current_line)
          rstate->mark_start++;

        if(rstate->mark_end >= current_line)
          rstate->mark_end++;
      }
    }

    rstate->current_line = current_line + 1;
  }
}

static void delete_line_contents(struct robot_line *delete_rline)
{
#ifdef CONFIG_DEBYTECODE
  free(delete_rline->color_codes);
#else
  free(delete_rline->line_bytecode);
#endif
  free(delete_rline->line_text);
  free(delete_rline);
}

static void delete_current_line(struct robot_editor_context *rstate, int move)
{
  if(rstate->total_lines == 1)
  {
    add_blank_line(rstate, -1);
    move = -1;
  }

  if(rstate->total_lines > 1)
  {
    struct robot_line *current_rline = rstate->current_rline;
    struct robot_line *next = current_rline->next;
    struct robot_line *previous = current_rline->previous;

#ifndef CONFIG_DEBYTECODE
    rstate->size -= current_rline->line_bytecode_length;
#endif

    previous->next = next;
    if(next)
      next->previous = previous;

    delete_line_contents(current_rline);

    if(rstate->mark_mode)
    {
      if(rstate->mark_start_rline == current_rline)
        rstate->mark_start_rline = next;

      if(rstate->mark_end_rline == current_rline)
        rstate->mark_end_rline = previous;

      if(rstate->mark_start > rstate->current_line)
        rstate->mark_start--;

      if(rstate->mark_end >= rstate->current_line)
        rstate->mark_end--;

      if(rstate->mark_start > rstate->mark_end)
        rstate->mark_mode = 0;
    }

    if(move > 0)
    {
      if(next)
      {
        rstate->current_rline = next;
      }
      else
      {
        rstate->current_rline = previous;
        rstate->current_line--;
      }
    }
    else
    {
      if(rstate->current_line != 1)
      {
        rstate->current_rline = previous;
        rstate->current_line--;
      }
      else
      {
        rstate->current_rline = next;
      }
    }

    strcpy(rstate->command_buffer, rstate->current_rline->line_text);

    rstate->total_lines--;
  }
}

static void delete_robot_lines(struct robot *cur_robot,
 struct robot_editor_context *rstate)
{
  struct robot_line *current_rline = rstate->base.next;
  struct robot_line *next_rline;
#ifndef CONFIG_DEBYTECODE
  char *object_code_position;

  object_code_position = cur_robot->program_bytecode;
  object_code_position[0] = 0xFF;
  object_code_position++;
#endif

  while(current_rline)
  {
#ifndef CONFIG_DEBYTECODE
    if((current_rline->next != NULL) || (current_rline->line_text[0]))
    {
      if(current_rline->validity_status == invalid_comment)
      {
        object_code_position[0] = current_rline->line_text_length + 3;
        object_code_position[1] = 107;
        object_code_position[2] = current_rline->line_text_length + 1;

        strcpy(object_code_position + 3, current_rline->line_text);
        object_code_position[current_rline->line_text_length + 4] =
         current_rline->line_text_length + 3;
        object_code_position += current_rline->line_text_length + 5;
      }
      else

      if(current_rline->validity_status == valid)
      {
        memcpy(object_code_position, current_rline->line_bytecode,
         current_rline->line_bytecode_length);
        object_code_position += current_rline->line_bytecode_length;
      }
    }
    else
    {
      // Get rid of trailing three bytes if present
      rstate->size -= current_rline->line_bytecode_length;
      reallocate_robot(cur_robot, rstate->size);
      object_code_position = cur_robot->program_bytecode + rstate->size - 1;
    }
#endif /* !CONFIG_DEBYTECODE */

    next_rline = current_rline->next;
    delete_line_contents(current_rline);
    current_rline = next_rline;
  }

#ifndef CONFIG_DEBYTECODE
  object_code_position[0] = 0;

  if(rstate->size > 2)
  {
    cur_robot->used = 1;
    cur_robot->cur_prog_line = 1;
    clear_label_cache(cur_robot);
    cache_robot_labels(cur_robot);
  }
#endif
}

static void macro_default_values(struct robot_editor_context *rstate,
 const struct ext_macro *macro_src)
{
  const struct macro_type *current_type;
  int i, i2;

  for(i = 0, current_type = macro_src->types;
   i < macro_src->num_types; i++, current_type++)
  {
    for(i2 = 0; i2 < current_type->num_variables; i2++)
    {
      if(current_type->type == string)
      {
        if(current_type->variables[i2].def.str_storage)
        {
          strcpy(current_type->variables[i2].storage.str_storage,
           current_type->variables[i2].def.str_storage);
        }
        else
        {
          current_type->variables[i2].storage.str_storage[0] = 0;
        }
      }
      else
      {
        memcpy(&(current_type->variables[i2].storage),
         &(current_type->variables[i2].def), sizeof(union variable_storage));
      }
    }
  }
}

/**
 * Finish the current intake-based undo frame (if any).
 */
static void end_intake_undo_frame(struct robot_editor_context *rstate)
{
  if(rstate->current_frame_type != INTK_NO_EVENT)
  {
    update_undo_frame(rstate->h);
    rstate->current_frame_type = INTK_NO_EVENT;
    rstate->idle_timer = 0;
  }
}

/**
 * Add a sequence of lines to a line-based undo frame.
 * The type should be either TX_NEW_LINE or TX_SAME_LINE.
 */
static void add_undo_frame_lines(struct robot_editor_context *rstate,
 enum text_undo_line_type type, int start_line, int end_line)
{
  struct robot_line *rline = rstate->current_rline;
  int current_line = rstate->current_line;

  if(start_line > end_line)
    return;

  while(current_line > start_line && rline)
    current_line--, rline = rline->previous;
  while(current_line < start_line && rline)
    current_line++, rline = rline->next;

  while(current_line <= end_line && rline)
  {
    int pos = (current_line == rstate->current_line) ? rstate->current_x : 0;
    add_robot_editor_undo_line(rstate->h, type, current_line, pos,
     rline->line_text, rline->line_text_length);

    rline = rline->next;
    current_line++;
  }
}

#ifdef CONFIG_DEBYTECODE

static char *package_program(struct robot_line *start_rline,
 struct robot_line *end_rline, int *_source_size, char *existing_source)
{
  // TODO: Have some option for giving a size maybe.
  struct robot_line *current_rline = start_rline;
  char *packaged_program = NULL;
  char *source_pos;
  int source_size = 0;

  while(current_rline != end_rline)
  {
    source_size += current_rline->line_text_length + 1;
    current_rline = current_rline->next;
  }

  if(source_size)
    packaged_program = crealloc(existing_source, source_size);

  if(packaged_program != NULL)
  {
    current_rline = start_rline;
    source_pos = packaged_program;

    while(current_rline != end_rline)
    {
      memcpy(source_pos, current_rline->line_text,
       current_rline->line_text_length);

      source_pos += current_rline->line_text_length;
      current_rline = current_rline->next;

      *source_pos = '\n';
      source_pos++;
    }
    source_pos[-1] = 0;

    if(_source_size != NULL)
      *_source_size = source_size;
  }
  else
    free(existing_source);

  return packaged_program;
}

static void update_program_status(struct robot_editor_context *rstate,
 struct robot_line *start_rline, struct robot_line *end_rline)
{
  struct robot_line *current_rline = start_rline;
  char *source_block;
  char *next;
  char *parse_next;
  char *line_start;
  char *command_start;
  struct token *parse_tokens;
  struct token *current_token;
  struct color_code_pair *color_codes = NULL;
  int source_block_length;
  int sub_block_length;
  int num_parse_tokens;
  int num_color_codes = 0;
  int line_token_residual;
  int token_offset_from_line_start;
  int token_length;
  int token_position;
  int line_length;
  int first_line;

  source_block = package_program(start_rline, end_rline, &source_block_length,
   NULL);

  next = source_block;

  current_rline = start_rline;

  do
  {
    if(*next == 0)
    {
      if(current_rline != NULL)
      {
        current_rline->command_type = COMMAND_TYPE_BLANK_LINE;
        current_rline->num_color_codes = 0;
      }
      break;
    }

    if(*next == '\n')
    {
      if(current_rline == NULL)
        break;

      current_rline->command_type = COMMAND_TYPE_BLANK_LINE;
      current_rline->num_color_codes = 0;
      next = strchr(next, '\n');

      if(next == NULL)
        break;

      current_rline = current_rline->next;

      next++;
    }

    parse_tokens = parse_command(next, &parse_next, &num_parse_tokens);

    if(parse_tokens)
    {
      command_start = next;
      next = parse_next;

      sub_block_length = parse_next - command_start;
      num_color_codes = 0;

      // The number of color codes for any given line is at most the number
      // of tokens.
      color_codes =
       cmalloc(sizeof(struct color_code_pair) * num_parse_tokens);

      // Where the current program line begins.
      line_start = command_start;
      // Start at first line.
      first_line = 1;

      // How much of that token got split over the previous line (and wraps
      // around to the current one). We start out with none.
      line_token_residual = 0;

      // Which token we're currently looking at from the list. Some will
      // get added to multiple color code lists, if they get split over
      // multiple lines.
      token_position = 0;

      // This loop goes for every line in the command.
      while(sub_block_length > 0)
      {
        if(first_line)
        {
          current_rline->command_type = COMMAND_TYPE_COMMAND_START;
          first_line = 0;
        }
        else
        {
          current_rline->command_type = COMMAND_TYPE_COMMAND_CONTINUE;
        }

        line_length = current_rline->line_text_length;

        // Pull out as many tokens as will fit on the line.
        while(1)
        {
          current_token = &(parse_tokens[token_position]);
          token_offset_from_line_start = current_token->value - line_start;
          token_length = current_token->length;

          // If there's a residual then we already took this amount off.
          if(line_token_residual)
          {
            token_offset_from_line_start = 0;
            token_length -= line_token_residual;
          }

          if(token_offset_from_line_start < line_length)
          {
            // Okay, the token starts on the line. It might not end on this
            // line, in which case it'll get split over onto the next line.
            color_codes[num_color_codes].arg_type_indexed =
             current_token->arg_type_indexed;
            color_codes[num_color_codes].offset = token_offset_from_line_start;

            if((token_offset_from_line_start + token_length) > line_length)
            {
              // Token got split over a line. The length here is only the length
              // until the end of the line and the rest is saved as residual.
              // We do not advance to the next token because it has to go again
              // for the next line.

              // This also means there's no more hope for getting any more tokens
              // out of this line so we bail.

              int new_residual = line_length - token_offset_from_line_start;
              color_codes[num_color_codes].length = new_residual;
              line_token_residual += new_residual;
              num_color_codes++;

              break;
            }
            else
            {
              // Token can fit on the next line, swallow it whole and go to
              // the next token.
              line_token_residual = 0;
              color_codes[num_color_codes].length = token_length;

              num_color_codes++;
              token_position++;

              // If we ran out of tokens then that's it.
              if(token_position >= num_parse_tokens)
                break;
            }
          }
          else
          {
            break;
          }
        }

        if(num_color_codes)
        {
          current_rline->color_codes = crealloc(current_rline->color_codes,
           num_color_codes * sizeof(struct color_code_pair));

          memcpy(current_rline->color_codes, color_codes,
           num_color_codes * sizeof(struct color_code_pair));
        }
        else
        {
          free(current_rline->color_codes);
          current_rline->color_codes = NULL;
        }
        current_rline->num_color_codes = num_color_codes;

        num_color_codes = 0;

        line_start += line_length + 1;
        sub_block_length -= line_length + 1;
        current_rline = current_rline->next;

        // Propagate out.
        if(token_position == num_parse_tokens)
          break;
      }

      free(color_codes);
      free(parse_tokens);
    }
    else
    {
      current_rline->command_type = COMMAND_TYPE_INVALID;
      next = strchr(next, '\n');

      current_rline = current_rline->next;

      if(next == NULL)
        break;

      next++;
    }
  } while(1);

  free(source_block);
}

static boolean update_current_line(struct robot_editor_context *rstate,
 boolean ignore)
{
  char *command_buffer = rstate->command_buffer;
  int line_text_length = strlen(command_buffer);
  struct robot_line *current_rline = rstate->current_rline;

  // Check to see if we're actually changing anything.
  if((current_rline->line_text_length != line_text_length) ||
   (current_rline->line_text == NULL) ||
   strcmp(current_rline->line_text, command_buffer))
  {
    current_rline->line_text =
     crealloc(current_rline->line_text, line_text_length + 1);

    current_rline->line_text_length = line_text_length;
    memcpy(current_rline->line_text, command_buffer, line_text_length + 1);

    rstate->program_modified = true;
  }

  return true;
}

#else /* !CONFIG_DEBYTECODE */

static void trim_whitespace(char *buffer, int length)
{
  int i;

  // trim any leading spaces

  for(i = 0; i < length; i++)
    if(buffer[i] != ' ')
      break;

  if(i != 0)
    memmove(buffer, buffer + i, length - i + 1);

  // and any trailing spaces
  for (i = length - 1; i >= 0; i--)
    if(buffer[i] != ' ')
      break;

  // bounds should be OK as caller has already nul-terminated
  buffer[i + 1] = '\0';
}

#ifndef CONFIG_DEBYTECODE
// fix cyclic dependency (could be done in several ways)
static boolean execute_named_macro(struct robot_editor_context *rstate,
 char *macro_name);

static boolean named_macro_exists(struct robot_editor_context *rstate,
 char *macro_name)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  const struct ext_macro *macro_src;
  char *line_pos;
  char *lone_name;
  int next;

  line_pos = skip_to_next(macro_name, ',', '(', 0);

  // extract just the name of the macro, if valid
  lone_name = cmalloc(line_pos - macro_name + 1);
  memcpy(lone_name, macro_name, line_pos - macro_name);
  lone_name[line_pos - macro_name] = 0;

  // see if such a macro exists
  macro_src = find_macro(editor_conf, lone_name, &next);
  free(lone_name);

  return macro_src != NULL;
}
#endif

static boolean update_current_line(struct robot_editor_context *rstate,
 boolean allow_macro_undo_frame)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  char bytecode_buffer[COMMAND_BUFFER_LEN];
  char error_buffer[COMMAND_BUFFER_LEN];
  char new_command_buffer[COMMAND_BUFFER_LEN];
  char arg_types[32];
  struct robot_line *current_rline = rstate->current_rline;
  char *command_buffer = rstate->command_buffer;
  int arg_count;
  int bytecode_length;
  int last_bytecode_length = current_rline->line_bytecode_length;
  int current_size = rstate->size;
  enum validity_types use_type = rstate->default_invalid;
  int line_text_length;

  line_text_length = (int)strlen(command_buffer);
  if(line_text_length > MAX_COMMAND_LEN)
  {
    line_text_length = MAX_COMMAND_LEN;
    command_buffer[MAX_COMMAND_LEN] = 0;
  }
  trim_whitespace(command_buffer, line_text_length);

  bytecode_length = legacy_assemble_line(command_buffer, bytecode_buffer,
   error_buffer, arg_types, &arg_count);

  // Trigger macro expansion; if the macro exists, return false to let the
  // caller know this line can be discarded if necessary.
  if(command_buffer[0] == '#' && named_macro_exists(rstate, command_buffer + 1))
  {
    int start_line = rstate->current_line;
    boolean macro_success;

    if(allow_macro_undo_frame)
    {
      int command_buffer_len = strlen(command_buffer);
      end_intake_undo_frame(rstate);
      add_robot_editor_undo_frame(rstate->h, rstate);
      add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, rstate->current_line,
       command_buffer_len, command_buffer, command_buffer_len);
    }

    macro_success = execute_named_macro(rstate, command_buffer + 1);

    if(allow_macro_undo_frame)
    {
      add_undo_frame_lines(rstate, TX_NEW_LINE, start_line, rstate->current_line);
      update_undo_frame(rstate->h);
    }

    if(macro_success)
      return false;
  }

  if((bytecode_length != -1) &&
   (current_size + bytecode_length - last_bytecode_length) <=
   rstate->max_size)
  {
    char *next;
    int line_text_length;

    disassemble_line(bytecode_buffer, &next, new_command_buffer,
     error_buffer, &line_text_length, editor_conf->disassemble_extras,
     arg_types, &arg_count, editor_conf->disassemble_base);

    if(line_text_length <= MAX_COMMAND_LEN)
    {
      current_rline->line_text_length = line_text_length;
      current_rline->line_text =
       crealloc(current_rline->line_text, line_text_length + 1);

      current_rline->line_bytecode_length = bytecode_length;
      current_rline->line_bytecode =
       crealloc(current_rline->line_bytecode, bytecode_length);
      current_rline->num_args = arg_count;
      current_rline->validity_status = valid;

      memcpy(current_rline->arg_types, arg_types, 20);
      strcpy(current_rline->line_text, new_command_buffer);
      memcpy(current_rline->line_bytecode, bytecode_buffer, bytecode_length);

      rstate->size = current_size + bytecode_length - last_bytecode_length;
    }
    else
    {
      current_rline->line_text_length = MAX_COMMAND_LEN;
      current_rline->line_text =
       crealloc(current_rline->line_text, MAX_COMMAND_LEN + 1);
      memcpy(current_rline->line_text, new_command_buffer, MAX_COMMAND_LEN);
      current_rline->line_text[MAX_COMMAND_LEN] = 0;

      if((current_rline->validity_status != valid) &&
       (current_rline->validity_status != invalid_comment))
      {
        use_type = current_rline->validity_status;
      }

      current_rline->line_bytecode_length = 0;
      current_rline->validity_status = use_type;
      rstate->size = current_size - last_bytecode_length;
    }
  }
  else
  {
    if(current_rline->validity_status != valid)
      use_type = current_rline->validity_status;

    current_rline->line_text =
     crealloc(current_rline->line_text, line_text_length + 1);

    if(use_type == invalid_comment &&
     (current_size + (line_text_length + 5) - last_bytecode_length) >
     rstate->max_size)
    {
      use_type = invalid_uncertain;
    }

    if((use_type == invalid_comment) && (line_text_length > 236))
    {
      line_text_length = 236;
      command_buffer[236] = 0;
    }

    current_rline->line_text_length = line_text_length;
    strcpy(current_rline->line_text, command_buffer);

    if(use_type != invalid_comment)
    {
      current_rline->line_bytecode_length = 0;
      current_rline->validity_status = use_type;
      rstate->size = current_size - last_bytecode_length;
    }
    else
    {
      current_rline->validity_status = invalid_comment;
      current_rline->line_bytecode_length = line_text_length + 5;
      rstate->size =
       current_size + (line_text_length + 5) - last_bytecode_length;
    }
  }

  return true;
}

#endif /* !CONFIG_DEBYTECODE */

// TODO: I'd like to unify this with what add_blank_line does, but
// unfortunately that just isn't working that well right now. Needs to
// be very carefully modularized down.

static void add_line(struct robot_editor_context *rstate, char *value, int relation)
{
#ifndef CONFIG_DEBYTECODE
  if(rstate->size + 3 + (int)strlen(value) < rstate->max_size)
#endif
  {
    struct robot_line *new_rline = cmalloc(sizeof(struct robot_line));
    struct robot_line *current_rline = rstate->current_rline;
    char *tmp = rstate->command_buffer;
    rstate->command_buffer = value;

    new_rline->line_text_length = 0;
    new_rline->line_text = NULL;

#ifdef CONFIG_DEBYTECODE
    new_rline->color_codes = NULL;
    new_rline->num_color_codes = 0;
#else
    new_rline->line_bytecode = NULL;
    new_rline->line_bytecode_length = 0;
    new_rline->validity_status = valid;
#endif

    rstate->current_rline = new_rline;
    new_rline->next = current_rline;
    new_rline->previous = current_rline->previous;

    if(relation > 0)
    {
      new_rline->next = current_rline->next;
      new_rline->previous = current_rline;

      if(current_rline->next)
        current_rline->next->previous = new_rline;

      current_rline->next = new_rline;
    }
    else
    {
      current_rline->previous->next = new_rline;
      current_rline->previous = new_rline;
    }

    if(update_current_line(rstate, false))
    {
      int current_line = rstate->current_line;

      if(relation < 0)
        rstate->current_rline = current_rline;

      rstate->current_line++;

      if(rstate->mark_mode)
      {
        if(relation > 0)
        {
          if(rstate->mark_start > current_line)
            rstate->mark_start++;

          if(rstate->mark_end > current_line)
            rstate->mark_end++;
        }
        else
        {
          if(rstate->mark_start >= current_line)
            rstate->mark_start++;

          if(rstate->mark_end >= current_line)
            rstate->mark_end++;
        }
      }

      rstate->total_lines++;
    }
    else
    {
      // Line was consumed by update_current_line (likely because of a macro),
      // so remove it. Space and bytecode size might have been counted for this
      // as part of processing the macro.
#ifndef CONFIG_DEBYTECODE
      rstate->size -= new_rline->line_bytecode_length;
#endif
      rstate->current_rline = current_rline;
      if(new_rline->previous)
        new_rline->previous->next = new_rline->next;
      if(new_rline->next)
        new_rline->next->previous = new_rline->previous;
      delete_line_contents(new_rline);
    }

    rstate->command_buffer = tmp;
  }
}

static void split_current_line(struct robot_editor_context *rstate)
{
  char *command_buffer = rstate->command_buffer;
  int start_line = rstate->current_line;
  size_t remainder_len;
  char tmp;

  /* undo */
  {
    end_intake_undo_frame(rstate);
    add_robot_editor_undo_frame(rstate->h, rstate);
    add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, start_line,
     rstate->current_x, command_buffer, strlen(command_buffer));
  }

  remainder_len = strlen(command_buffer + rstate->current_x);
  if(remainder_len > MAX_COMMAND_LEN)
    remainder_len = MAX_COMMAND_LEN;

  tmp = command_buffer[rstate->current_x];
  command_buffer[rstate->current_x] = '\0';
  add_line(rstate, command_buffer, -1);
  command_buffer[rstate->current_x] = tmp;

  if(rstate->current_x)
    memmove(command_buffer, command_buffer + rstate->current_x, remainder_len);
  command_buffer[remainder_len] = '\0';
  rstate->current_x = 0;
  update_current_line(rstate, false);

  /* undo */
  {
    add_undo_frame_lines(rstate, TX_NEW_LINE, start_line, rstate->current_line);
    update_undo_frame(rstate->h);
  }
}

static void combine_current_line(struct robot_editor_context *rstate, int move)
{
  struct robot_line *rline;
  char *command_buffer = rstate->command_buffer;
  char line_buffer[MAX_COMMAND_LEN + 1];
  size_t line_len;
  size_t command_buffer_len;
  int start_line;

  update_current_line(rstate, true);
  line_len = strlen(command_buffer);

  rline = rstate->current_rline;
  if(move > 0)
    rline = rline->next;
  else
    rline = rline->previous;

  if(!rline || !rline->line_text)
    return;

  command_buffer_len = strlen(rline->line_text);

  // Only attempt to merge lines if there is space.
  if(line_len + command_buffer_len <= MAX_COMMAND_LEN)
  {
    /* undo */
    {
      struct robot_line *current_rline = rstate->current_rline;
      end_intake_undo_frame(rstate);
      add_robot_editor_undo_frame(rstate->h, rstate);
      add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, rstate->current_line,
       rstate->current_x, current_rline->line_text, current_rline->line_text_length);

      start_line = rstate->current_line + ((move > 0) ? 0 : -1);
      add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, start_line, 0,
       rline->line_text, rline->line_text_length);
    }

    memcpy(line_buffer, command_buffer, line_len);
    line_buffer[line_len] = 0;

    delete_current_line(rstate, move);
    start_line = rstate->current_line;

    if(move > 0)
    {
      // Insert in front of following line
      rstate->current_x = line_len;
      memmove(command_buffer + line_len, command_buffer, command_buffer_len);
      memcpy(command_buffer, line_buffer, line_len);
    }
    else
    {
      // Append to previous line
      rstate->current_x = command_buffer_len;
      memcpy(command_buffer + command_buffer_len, line_buffer, line_len);
    }

    command_buffer[command_buffer_len + line_len] = 0;
    update_current_line(rstate, false);

    /* undo */
    {
      add_undo_frame_lines(rstate, TX_NEW_LINE, start_line, rstate->current_line);
      update_undo_frame(rstate->h);
    }
  }
}

static void insert_string(struct robot_editor_context *rstate, const char *src,
 char linebreak_char)
{
  while(src)
  {
    src = intake_input_string(rstate->intk, src, linebreak_char);
    if(src)
      split_current_line(rstate);
  }
}

static void output_macro(struct robot_editor_context *rstate,
 const struct ext_macro *macro_src)
{
  int num_lines = macro_src->num_lines;
  char line_buffer[COMMAND_BUFFER_LEN];
  char number_buffer[16];
  char *line_pos, *line_pos_old;
  struct macro_variable_reference *current_reference;
  int i, i2;
  size_t len;

  if(rstate->macro_recurse_level == MAX_MACRO_RECURSION ||
   rstate->macro_repeat_level == MAX_MACRO_REPEAT)
  {
    rstate->command_buffer[0] = 0;
    update_current_line(rstate, false);
    return;
  }

  rstate->macro_recurse_level++;
  rstate->macro_repeat_level++;

  // OK, output the lines

  for(i = 0; i < num_lines; i++)
  {
    line_pos = line_buffer;
    line_pos_old = line_pos;

    len = strlen(macro_src->lines[i][0]);
    if((line_pos - line_pos_old) + len >= COMMAND_BUFFER_LEN)
      goto err_cancel_expansion;

    strcpy(line_pos, macro_src->lines[i][0]);
    line_pos += len;

    for(i2 = 0; i2 < macro_src->line_element_count[i]; i2++)
    {
      current_reference = &(macro_src->variable_references[i][i2]);
      if(current_reference->type)
      {
        switch(current_reference->type->type)
        {
          case number:
          {
            if(current_reference->ref_mode == hexidecimal)
            {
              sprintf(number_buffer, "%02x",
               current_reference->storage->int_storage);
            }
            else
            {
              sprintf(number_buffer, "%d",
               current_reference->storage->int_storage);
            }

            len = strlen(number_buffer);
            if((line_pos - line_pos_old) + len >= COMMAND_BUFFER_LEN)
              goto err_cancel_expansion;

            memcpy(line_pos, number_buffer, len);
            line_pos += len;
            break;
          }

          case string:
          {
            len = strlen(current_reference->storage->str_storage);
            if((line_pos - line_pos_old) + len >= COMMAND_BUFFER_LEN)
              goto err_cancel_expansion;

            memcpy(line_pos, current_reference->storage->str_storage, len);
            line_pos += len;
            break;
          }

          case character:
          {
            if((line_pos - line_pos_old) + 3 >= COMMAND_BUFFER_LEN)
              goto err_cancel_expansion;

            sprintf(line_pos, "'%c'",
             current_reference->storage->int_storage);
            line_pos += 3;
            break;
          }

          case color:
          {
            if((line_pos - line_pos_old) + 3 >= COMMAND_BUFFER_LEN)
              goto err_cancel_expansion;

            print_color(current_reference->storage->int_storage, line_pos);
            line_pos += 3;
            break;
          }

          default:
            break;
        }
      }
      else
      {
        const char undef[] = "(undef)";

        len = strlen(undef);
        if((line_pos - line_pos_old) + len >= COMMAND_BUFFER_LEN)
          goto err_cancel_expansion;

        strcpy(line_pos, undef);
        line_pos += len;
      }

      len = strlen(macro_src->lines[i][i2 + 1]);
      if((line_pos - line_pos_old) + len >= COMMAND_BUFFER_LEN)
        goto err_cancel_expansion;

      memcpy(line_pos, macro_src->lines[i][i2 + 1], len);
      line_pos += len;
    }

err_cancel_expansion:
    *line_pos = 0;
    add_line(rstate, line_buffer, -1);
  }

  rstate->macro_recurse_level--;
}

#ifndef CONFIG_DEBYTECODE

static boolean execute_named_macro(struct robot_editor_context *rstate,
 char *macro_name)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  char *line_pos, *line_pos_old, *lone_name;
  const struct macro_type *param_type = NULL;
  const struct ext_macro *macro_src;
  char last_char;
  int next;

  line_pos = skip_to_next(macro_name, ',', '(', 0);

  // extract just the name of the macro, if valid
  lone_name = cmalloc(line_pos - macro_name + 1);
  memcpy(lone_name, macro_name, line_pos - macro_name);
  lone_name[line_pos - macro_name] = 0;

  // see if such a macro exists
  macro_src = find_macro(editor_conf, lone_name, &next);
  free(lone_name);

  // it doesn't, carefully abort
  if(!macro_src)
    return false;

  last_char = *line_pos;

  if(macro_src->num_types && last_char)
  {
    union variable_storage *param_storage;
    int param_current_type = 0;
    int param_current_var = 0;
    char *value = NULL;
    int i;

    // Fill in parameters
    macro_default_values(rstate, macro_src);

    // Get name, may stop at equals sign first
    while(1)
    {
      line_pos = skip_whitespace(line_pos + 1);
      line_pos_old = line_pos;
      line_pos = skip_to_next(line_pos, '=', ',', ')');

      last_char = *line_pos;
      *line_pos = 0;

      param_storage = NULL;

      if(last_char == '=')
      {
        // It's named, find which index this corresponds to.
        *line_pos = 0;
        param_type = macro_src->types;

        for(i = 0; i < macro_src->num_types; i++, param_type++)
        {
          param_storage = find_macro_variable(line_pos_old,
           param_type);
          if(param_storage)
            break;
        }

        line_pos++;
        value = line_pos;
        line_pos = skip_to_next(line_pos, '=', ',', ')');
        last_char = *line_pos;
        *line_pos = 0;
      }
      else

      if(*(line_pos - 1) != '(')
      {
        param_type = macro_src->types + param_current_type;

        if(param_current_var == param_type->num_variables)
        {
          param_current_type++;
          param_type++;
          param_current_var = 0;
        }

        if(param_current_type < macro_src->num_types)
        {
          param_storage =
           &((param_type->variables[param_current_var]).storage);
        }

        value = line_pos_old;
        param_current_var++;
      }

      if(param_storage)
      {
        switch(param_type->type)
        {
          case number:
          {
            param_storage->int_storage = strtol(value, NULL, 10);

            if(param_storage->int_storage <
             param_type->type_attributes[0])
            {
              param_storage->int_storage =
               param_type->type_attributes[0];
            }

            if(param_storage->int_storage >
             param_type->type_attributes[1])
            {
              param_storage->int_storage =
               param_type->type_attributes[1];
            }

            break;
          }

          case string:
          {
            value[param_type->type_attributes[0]] = 0;
            strcpy(param_storage->str_storage, value);
            break;
          }

          case character:
          {
            if(*value == '\'')
            {
              param_storage->int_storage =
               *(value + 1);
            }
            else
            {
              param_storage->int_storage =
               strtol(value, NULL, 10);
            }
            break;
          }

          case color:
          {
            param_storage->int_storage =
             get_color(value);
            break;
          }
        }
      }

      if((last_char == ')') || (!last_char))
        break;
    }
  }

  // Wipe any existing buffered content for this line
  rstate->command_buffer[0] = '\0';
  update_current_line(rstate, false);

  // And replace it with the macro contents
  output_macro(rstate, macro_src);
  return true;
}

#endif /* !CONFIG_DEBYTECODE */

static int block_menu(struct robot_editor_context *rstate)
{
  struct world *mzx_world = ((context *)rstate)->world;
  int dialog_result, block_op = 0;
  struct dialog di;
  const char *radio_strings[] =
  {
    "Copy block", "Cut block",
    "Clear block", "Export block"
  };
  struct element *elements[3] =
  {
    construct_radio_button(2, 2, radio_strings,
     4, 21, &block_op),
    construct_button(5, 7, "OK", 0),
    construct_button(15, 7, "Cancel", -1)
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  construct_dialog(&di, "Choose Block Command", 26, 6, 28, 10,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result == -1)
    return -1;
  else
    return block_op;
}

static void copy_block_to_buffer(struct robot_editor_context *rstate)
{
  struct robot_line *current_rline = rstate->mark_start_rline;
  int num_lines = (rstate->mark_end - rstate->mark_start) + 1;
  int line_length;
  int i;

  copy_buffer_total_length = 0;

  /* First, if there's something already there, clear all the
   * lines and the buffer.
   */
  if(copy_buffer)
  {
    for(i = 0; i < copy_buffer_lines; i++)
      free(copy_buffer[i]);
    free(copy_buffer);
  }

  copy_buffer = ccalloc(num_lines, sizeof(char *));
  copy_buffer_lines = num_lines;

  for(i = 0; i < num_lines; i++)
  {
    line_length = current_rline->line_text_length + 1;
    copy_buffer[i] = cmalloc(line_length);
    memcpy(copy_buffer[i], current_rline->line_text, line_length);
    current_rline = current_rline->next;

    copy_buffer_total_length += line_length;
  }

  /* It may be possible to copy this buffer out of process,
   * to an operating system clipboard.
   */
  copy_buffer_to_clipboard(copy_buffer, copy_buffer_lines,
   copy_buffer_total_length);
}

static void paste_buffer(struct robot_editor_context *rstate)
{
  char *ext_buffer = get_clipboard_buffer();
  int start_line = rstate->current_line;
  int i;

  // If we can use an OS buffer, do so
  if(ext_buffer)
  {
    char *src_ptr;
    int line_length;
    int copy_length;
    char tmp;

    src_ptr = ext_buffer;

    while(*src_ptr)
    {
      line_length = (int)strcspn(src_ptr, "\r\n");

      copy_length = line_length;
      if(copy_length >= COMMAND_BUFFER_LEN)
        copy_length = COMMAND_BUFFER_LEN - 1;

      tmp = src_ptr[copy_length];
      src_ptr[copy_length] = '\0';
      add_line(rstate, src_ptr, -1);
      src_ptr[copy_length] = tmp;
      src_ptr += line_length;

#ifdef __WIN32__
      if(*src_ptr == '\r')
        src_ptr++;

      if(*src_ptr == '\n')
        src_ptr++;
#else
      if(*src_ptr)
        src_ptr++;
#endif
    }

    /* This may need SDL_free if it was allocated by SDL. */
    free_clipboard_buffer(ext_buffer);
  }
  else

  if(copy_buffer)
  {
    for(i = 0; i < copy_buffer_lines; i++)
      add_line(rstate, copy_buffer[i], -1);
  }

  /* undo */
  if(start_line < rstate->current_line)
  {
    end_intake_undo_frame(rstate);
    add_robot_editor_undo_frame(rstate->h, rstate);
    add_undo_frame_lines(rstate, TX_NEW_LINE, start_line, rstate->current_line - 1);
    update_undo_frame(rstate->h);
  }
}

static void clear_block(struct robot_editor_context *rstate)
{
  struct robot_line *current_rline = rstate->mark_start_rline;
  struct robot_line *line_after = rstate->mark_end_rline->next;
  struct robot_line *line_before = current_rline->previous;
  struct robot_line *next_rline;
  int num_lines = rstate->mark_end - rstate->mark_start + 1;
  int i;

  end_intake_undo_frame(rstate);
  add_robot_editor_undo_frame(rstate->h, rstate);

  for(i = 0; i < num_lines; i++)
  {
    next_rline = current_rline->next;

#ifndef CONFIG_DEBYTECODE
    rstate->size -= current_rline->line_bytecode_length;
#endif

    add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, rstate->mark_start,
     current_rline->line_text_length, current_rline->line_text,
     current_rline->line_text_length);

    delete_line_contents(current_rline);

    current_rline = next_rline;
  }

  update_undo_frame(rstate->h);

  line_before->next = line_after;
  if(line_after)
    line_after->previous = line_before;

  rstate->total_lines -= num_lines;

  if(!rstate->total_lines)
  {
    rstate->current_line = 0;
    rstate->current_rline = &(rstate->base);
    add_blank_line(rstate, 1);
  }
  else
  {
    if(rstate->current_line >= rstate->mark_start)
    {
      if(rstate->current_line <= rstate->mark_end)
      {
        // Current line got swallowed.
        if(line_after)
        {
          rstate->current_line = rstate->mark_start;
          rstate->current_rline = line_after;
        }
        else
        {
          rstate->current_line = rstate->mark_start - 1;
          rstate->current_rline = line_before;
        }
      }
      else
      {
        rstate->current_line -= num_lines;
      }
    }
  }

  strcpy(rstate->command_buffer, rstate->current_rline->line_text);
}

static void export_block(struct robot_editor_context *rstate,
 int region_default)
{
  struct world *mzx_world = ((context *)rstate)->world;
  struct robot_line *current_rline;
  struct robot_line *end_rline;
  int export_region = region_default;
  int export_type = 0;
  char export_name[MAX_PATH];
  int num_formats = 1;
  int num_elements;
  const char *export_ext[] = { ".TXT", NULL, NULL };
  const char *radio_strings_1[] =
  {
    "Text", NULL
  };
  const char *radio_strings_2[] =
  {
    "Entire robot", "Current block"
  };
  struct element *elements[4];

#ifndef CONFIG_DEBYTECODE
  export_ext[1] = ".BC";
  radio_strings_1[1] = "Bytecode";
  num_formats++;
#endif

  if(region_default)
  {
    elements[0] = construct_label(45, 19,
     "Export\nregion as: ");
    elements[1] = construct_radio_button(56,
     19, radio_strings_1, num_formats, 8, &export_type);
    elements[2] = construct_label(4, 19,
     "Export the\nfollowing region: ");
    elements[3] = construct_radio_button(22, 19,
     radio_strings_2, 2, 13, &export_region);
    num_elements = 4;
  }
  else
  {
    elements[0] = construct_label(25, 19,
     "Export\nregion as: ");
    elements[1] = construct_radio_button(36,
     19, radio_strings_1, num_formats, 8, &export_type);
    num_elements = 2;
  }

  export_name[0] = 0;

  if(!file_manager(mzx_world, export_ext, NULL, export_name,
   "Export robot", ALLOW_ALL_DIRS, ALLOW_NEW_FILES, elements, num_elements, 3))
  {
    vfile *export_file;

    if(!export_region)
    {
      current_rline = rstate->base.next;
      end_rline = NULL;
    }
    else
    {
      current_rline = rstate->mark_start_rline;
      end_rline = rstate->mark_end_rline->next;
    }

#ifndef CONFIG_DEBYTECODE
    if(export_type)
    {
      path_force_ext(export_name, MAX_PATH, ".bc");
      export_file = vfopen_unsafe(export_name, "wb");

      vfputc(0xFF, export_file);

      while(current_rline != end_rline)
      {
        vfwrite(current_rline->line_bytecode,
         current_rline->line_bytecode_length, 1, export_file);
        current_rline = current_rline->next;
      }

      vfputc(0, export_file);
    }
    else
#endif
    {
      path_force_ext(export_name, MAX_PATH, ".txt");
      export_file = vfopen_unsafe(export_name, "w");

      while(current_rline != end_rline)
      {
        vfputs(current_rline->line_text, export_file);
        vfputc('\n', export_file);
        current_rline = current_rline->next;
      }
    }

    vfclose(export_file);
  }
}

static void import_block(struct robot_editor_context *rstate)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ((context *)rstate)->world;
  const char *txt_ext[] = { ".TXT", NULL, NULL };
  char import_name[MAX_PATH];
  char line_buffer[256];
  vfile *import_file;
  int start_line = rstate->current_line;
#ifndef CONFIG_DEBYTECODE
  ssize_t ext_pos;

  txt_ext[1] = ".BC";

  if(choose_file(mzx_world, txt_ext, import_name, "Import Robot", ALLOW_ALL_DIRS))
    return;

#else // CONFIG_DEBYTECODE
  const char *label[] = { "Legacy source code" };
  int is_legacy = 0;

  struct element *elements[] = {
    construct_check_box(21, 20, label, 1, strlen(label[0]), &is_legacy)
  };

  if(file_manager(mzx_world, txt_ext, NULL, import_name, "Import Robot",
   ALLOW_ALL_DIRS, NO_NEW_FILES, elements, 1, 2))
    return;

#endif

  import_file = vfopen_unsafe(import_name, "rb");
  if(!import_file)
    return;

#ifndef CONFIG_DEBYTECODE
  ext_pos = (ssize_t)strlen(import_name) - 3;

  if(ext_pos >= 1 && !strcasecmp(import_name + ext_pos, ".BC"))
  {
    long file_size = vfilelength(import_file, true);

    // 0xff + length + cmd + length + 0x00
    if(file_size >= 5)
    {
       char *buffer = cmalloc(file_size - 1);
       size_t ret;

       // skip 0xff
       vfgetc(import_file);

       // put the rest in a buffer for disassemble_line()
       ret = vfread(buffer, file_size - 1, 1, import_file);

       // copied to buffer, must now disassemble
       if(ret == 1)
       {
         char *current_robot_pos = buffer, *next;
         char error_buffer[256];
         int line_text_length;

         while(1)
         {
           int new_line = disassemble_line(current_robot_pos, &next,
            line_buffer, error_buffer, &line_text_length,
            editor_conf->disassemble_extras, NULL, NULL,
            editor_conf->disassemble_base);

           if(new_line)
             add_line(rstate, line_buffer, -1);
           else
             break;

           current_robot_pos = next;
         }
       }

       free(buffer);
    }
  }

#else //CONFIG_DEBYTECODE
  if(is_legacy)
  {
    char command_buffer[512];
    char bytecode_buffer[256];
    char errors[256];

    int disasm_length;

    while(vfsafegets(line_buffer, 256, import_file) != NULL)
    {
      legacy_assemble_line(line_buffer, bytecode_buffer, errors,
       NULL, NULL);

      legacy_disassemble_command(bytecode_buffer, command_buffer,
       &disasm_length, 256, editor_conf->disassemble_extras,
       editor_conf->disassemble_base
      );

      command_buffer[disasm_length] = 0;

      add_line(rstate, command_buffer, -1);
    }
  }

#endif //CONFIG_DEBYTECODE
  else
  {
    // fsafegets ensures that no line terminators are present
    while(vfsafegets(line_buffer, 255, import_file) != NULL)
      add_line(rstate, line_buffer, -1);
  }

  vfclose(import_file);

  /* undo */
  if(start_line < rstate->current_line)
  {
    end_intake_undo_frame(rstate);
    add_robot_editor_undo_frame(rstate->h, rstate);
    add_undo_frame_lines(rstate, TX_NEW_LINE, start_line, rstate->current_line - 1);
    update_undo_frame(rstate->h);
  }
}

static void edit_single_line_macros(struct robot_editor_context *rstate)
{
  struct world *mzx_world = ((context *)rstate)->world;
  int dialog_result;
  struct dialog di;
  struct element *elements[] =
  {
    construct_input_box(3, 2, "F6-  ", 63, macros[0]),
    construct_input_box(3, 3, "F7-  ", 63, macros[1]),
    construct_input_box(3, 4, "F8-  ", 63, macros[2]),
    construct_input_box(3, 5, "F9-  ", 63, macros[3]),
    construct_input_box(3, 6, "F10- ", 63, macros[4]),
    construct_button(25, 8, "OK", 0),
    construct_button(45, 8, "Cancel", -1)
  };
  char new_macros[5][64];

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  memcpy(new_macros, macros, 64 * 5);

  construct_dialog(&di, "Edit Single Line Macros", 3, 7, 74, 10,
   elements, ARRAY_SIZE(elements), 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    memcpy(macros, new_macros, 64 * 5);
}

static void block_action(struct robot_editor_context *rstate)
{
  int block_command = block_menu(rstate);

  if(!rstate->mark_mode)
  {
    rstate->mark_start_rline = rstate->current_rline;
    rstate->mark_end_rline = rstate->current_rline;
    rstate->mark_start = rstate->current_line;
    rstate->mark_end = rstate->current_line;
  }

  switch(block_command)
  {
    case 0:
    {
      // Copy block into buffer
      copy_block_to_buffer(rstate);
      break;
    }

    case 1:
    {
      // Copy block into buffer, then clear block
      copy_block_to_buffer(rstate);
      clear_block(rstate);
      rstate->mark_mode = 0;
      break;
    }

    case 2:
    {
      // Clear block
      clear_block(rstate);
      rstate->mark_mode = 0;
      break;
    }

    case 3:
    {
      // Export block - text only
      export_block(rstate, 1);
      break;
    }
  }
}

static void move_line_up(struct robot_editor_context *rstate, int count)
{
  int i;

  end_intake_undo_frame(rstate);
  for(i = 0; (i < count); i++)
  {
    if(rstate->current_rline->previous == &(rstate->base))
      break;

    rstate->current_rline = rstate->current_rline->previous;
  }

  rstate->current_line -= i;
}

static void move_line_down(struct robot_editor_context *rstate, int count)
{
  int i;

  end_intake_undo_frame(rstate);
  for(i = 0; (i < count); i++)
  {
    if(rstate->current_rline->next == NULL)
    {
      // Add a new line if the last line isn't blank.
      if(rstate->current_rline->line_text[0])
        add_blank_line(rstate, 1);

      break;
    }

    rstate->current_rline = rstate->current_rline->next;
  }

  rstate->current_line += i;
}

static void move_and_update(struct robot_editor_context *rstate, int count)
{
  update_current_line(rstate, true);
  if(count < 0)
  {
    move_line_up(rstate, -count);
  }
  else
  {
    move_line_down(rstate, count);
  }

  strcpy(rstate->command_buffer, rstate->current_rline->line_text);
}

static void goto_line(struct robot_editor_context *rstate, int line, int column)
{
  if(line > rstate->total_lines)
    line = rstate->total_lines;

  if(line < 1)
    line = 1;

  move_and_update(rstate, line - rstate->current_line);
  rstate->current_x = CLAMP(column, 0, rstate->current_rline->line_text_length);
}

static void goto_position(struct robot_editor_context *rstate)
{
  struct world *mzx_world = ((context *)rstate)->world;
  int dialog_result;
  int line_number = rstate->current_line;
  int column_number = rstate->current_x + 1;
  struct dialog di;

  struct element *elements[4] =
  {
    construct_number_box(2, 2, "Line:   ", 1, rstate->total_lines, NUMBER_BOX,
     &line_number),
    construct_number_box(2, 3, "Column: ", 1, MAX_COMMAND_LEN + 1, NUMBER_BOX,
     &column_number),
    construct_button(3, 5, "OK", 0),
    construct_button(14, 5, "Cancel", -1)
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  construct_dialog(&di, "Goto position", 28, 8, 25, 7,
   elements, 4, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result != -1)
    goto_line(rstate, line_number, column_number - 1);

  // Prevent UI keys from carrying through.
  force_release_all_keys();
}

/**
 * Replace a span of the current Robotic line with a replacement string.
 * The current Robotic line should be synchronized (e.g. by move_and_update).
 */
static void replace_current_line(struct robot_editor_context *rstate,
 int r_pos, const char *str, size_t str_size, const char *replace,
 size_t replace_size)
{
  struct robot_line *current_rline = rstate->current_rline;
  int start_line = rstate->current_line;
  size_t tail_size;

  /* undo (NOTE: frame must be set up externally). */
  add_robot_editor_undo_line(rstate->h, TX_OLD_LINE, start_line, r_pos,
   current_rline->line_text, current_rline->line_text_length);

  if(replace_size > (size_t)(COMMAND_BUFFER_LEN - r_pos - 1))
    replace_size = (size_t)(COMMAND_BUFFER_LEN - r_pos - 1);

  tail_size = current_rline->line_text_length - r_pos - str_size;
  if(tail_size > COMMAND_BUFFER_LEN - r_pos - replace_size - 1)
    tail_size = COMMAND_BUFFER_LEN - r_pos - replace_size - 1;

  memcpy(rstate->command_buffer + r_pos, replace, replace_size);
  memcpy(rstate->command_buffer + r_pos + replace_size,
   current_rline->line_text + r_pos + str_size, tail_size);
  rstate->command_buffer[r_pos + replace_size + tail_size] = '\0';

  update_current_line(rstate, false);
  strcpy(rstate->command_buffer, rstate->current_rline->line_text);

  /* undo */
  add_undo_frame_lines(rstate, TX_SAME_LINE, start_line, rstate->current_line);
}

static int robo_ed_find_string(struct robot_editor_context *rstate, char *str,
 struct string_search_data *data, int *position, boolean wrap,
 boolean ignore_case, boolean allow_current)
{
  struct robot_line *current_rline = rstate->current_rline;
  int current_line = rstate->current_line;
  const char *text = rstate->command_buffer;
  const char *pos = NULL;
  size_t text_len;
  size_t str_len = strlen(str);
  int first_pos;

  update_current_line(rstate, true);
  strcpy(rstate->command_buffer, current_rline->line_text);

  if(!str_len)
    return -1;

  // Check the first line first
  text_len = strlen(text);
  first_pos = (allow_current) ? rstate->current_x : rstate->current_x + 1;
  if(first_pos < (int)text_len)
  {
    text += first_pos;
    text_len -= first_pos;
    pos = string_search(text, text_len, str, str_len, data, ignore_case);

    text = rstate->command_buffer;
  }

  if(pos == NULL)
  {
    current_rline = current_rline->next;
    current_line++;

    // Now check the next lines
    while(current_rline != NULL)
    {
      text = current_rline->line_text;
      pos = string_search(text, strlen(text), str, str_len, data, ignore_case);

      if(pos)
        break;

      current_rline = current_rline->next;
      current_line++;
    }
  }

  // Wrap around?
  if(wrap && (pos == NULL))
  {
    current_rline = rstate->base.next;
    current_line = 1;
    while(current_rline != rstate->current_rline->next)
    {
      text = current_rline->line_text;
      pos = string_search(text, strlen(text), str, str_len, data, ignore_case);

      if(pos)
        break;

      current_rline = current_rline->next;
      current_line++;
    }
  }

  if(pos)
  {
    *position = (int)(pos - text);
    return current_line;
  }

  return -1;
}

static void robo_ed_search_action(struct robot_editor_context *rstate,
 enum search_option action)
{
  switch(action)
  {
    case SEARCH_OPTION_NONE:
    {
      return;
    }

    case SEARCH_OPTION_SEARCH:
    {
      // Find
      int l_pos;
      int l_num = robo_ed_find_string(rstate, search_string, &search_index,
       &l_pos, search_wrap_enabled, search_ignore_case_enabled, false);

      if(l_num != -1)
        goto_line(rstate, l_num, l_pos);

      break;
    }

    case SEARCH_OPTION_REPLACE:
    {
      // Find & Replace
      int l_pos;
      int l_num = robo_ed_find_string(rstate, search_string, &search_index,
       &l_pos, search_wrap_enabled, search_ignore_case_enabled, false);

      if(l_num != -1)
      {
        goto_line(rstate, l_num, l_pos);

        end_intake_undo_frame(rstate);
        add_robot_editor_undo_frame(rstate->h, rstate);

        replace_current_line(rstate, l_pos, search_string, strlen(search_string),
         replace_string, strlen(replace_string));

        update_undo_frame(rstate->h);
      }

      break;
    }

    case SEARCH_OPTION_REPLACE_ALL:
    {
      char old_buffer[COMMAND_BUFFER_LEN];
      int l_pos;
      int l_num;
      int r_len = (int)strlen(replace_string);
      int s_len = (int)strlen(search_string);
      int start_line = rstate->current_line;
      int start_pos = rstate->current_x;
      int last_line = start_line;
      int last_pos = start_pos;
      int dif = r_len - s_len;
      int wrapped = 0;
      boolean undo_started = false;

      do
      {
        l_num = robo_ed_find_string(rstate, search_string, &search_index,
         &l_pos, search_wrap_enabled, search_ignore_case_enabled, true);

        // Is it on the starting line and below the starting cursor?
        // If so modify the starting cursor because the line was
        // changed.
        if((l_num == start_line) && (l_num <= start_pos))
        {
          start_pos += dif;
        }

        // A decrease had better indicate a wrap, and you only get one
        if(((l_num == last_line) && (l_pos < last_pos)) || (l_num < last_line))
        {
          if((((l_num == start_line) && (l_pos <= start_pos)) ||
           (l_num < start_line)) && (!wrapped))
          {
            wrapped = 1;
          }
          else
          {
            break;
          }
        }

        // If it has already wrapped don't let it past the start
        if((((l_num == start_line) && (l_pos > start_pos)) ||
         (l_num > start_line)) && wrapped)
        {
          break;
        }

        if(l_num != -1 && l_pos <= MAX_COMMAND_LEN)
        {
          goto_line(rstate, l_num, l_pos);

          if(!undo_started)
          {
            end_intake_undo_frame(rstate);
            add_robot_editor_undo_frame(rstate->h, rstate);
            undo_started = true;
          }

          if(r_len == 0)
            memcpy(old_buffer, rstate->command_buffer, COMMAND_BUFFER_LEN);

          replace_current_line(rstate, l_pos, search_string, s_len,
           replace_string, r_len);

          if(r_len == 0 && !strcmp(old_buffer, rstate->command_buffer))
          {
            /* Anti-hang hack: if the line didn't change due to compilation
             * adding quotes or extras and the replacement length is 0, the
             * loop will get stuck. "Help" it out of this situation. */
            l_pos++;
          }

          l_pos += r_len;
          rstate->current_x = l_pos;

          last_line = l_num;
          last_pos = l_pos;
        }
        else
        {
          break;
        }
      } while(1);

      if(undo_started)
        update_undo_frame(rstate->h);

      break;
    }
  }

  last_search_action = action;
}

static void robo_ed_search_dialog(struct robot_editor_context *rstate)
{
  struct world *mzx_world = ((context *)rstate)->world;
  struct dialog di;
  const char *wrap_opt[] = { "Wrap" };
  const char *case_opt[] = { "Case sensitive" };
  int wrap = search_wrap_enabled;
  int casesens = !search_ignore_case_enabled;
  struct element *elements[] =
  {
    construct_check_box(14, 3, wrap_opt, 1, strlen(wrap_opt[0]), &wrap),
    construct_check_box(30, 3, case_opt, 1, strlen(case_opt[0]), &casesens),
    construct_button(60, 3, "Cancel", -1),
    construct_input_box(2, 1, "Search: ", 47, search_string),
    construct_button(60, 1, "Search", SEARCH_OPTION_SEARCH),
  };
  int num_elements = ARRAY_SIZE(elements);
  int result;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  construct_dialog(&di, "Search (repeat: Ctrl+R; replace: Ctrl+H)", 5, 10, 70, 5,
   elements, num_elements, 3);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  search_wrap_enabled = wrap;
  search_ignore_case_enabled = !casesens;
  string_search_index(search_string, strlen(search_string), &search_index,
   search_ignore_case_enabled);

  if(result != -1)
    robo_ed_search_action(rstate, result);
}

static void robo_ed_replace_dialog(struct robot_editor_context *rstate)
{
  struct world *mzx_world = ((context *)rstate)->world;
  struct dialog di;
  const char *wrap_opt[] = { "Wrap" };
  const char *case_opt[] = { "Case sensitive" };
  int wrap = search_wrap_enabled;
  int casesens = !search_ignore_case_enabled;
  struct element *elements[] =
  {
    construct_check_box(14, 4, wrap_opt, 1, strlen(wrap_opt[0]), &wrap),
    construct_check_box(30, 4, case_opt, 1, strlen(case_opt[0]), &casesens),
    construct_button(61, 4, "Cancel", -1),
    construct_input_box(2, 1, "Search:  ", 47, search_string),
    construct_input_box(2, 2, "Replace: ", 47, replace_string),
    construct_button(61, 1, "Replace", SEARCH_OPTION_REPLACE),
    construct_button(61, 2, "Replace All", SEARCH_OPTION_REPLACE_ALL),
  };
  int num_elements = ARRAY_SIZE(elements);
  int result;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  construct_dialog(&di, "Replace (repeat: Ctrl+R; search: Ctrl+F)", 2, 9, 76, 6,
   elements, num_elements, 3);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  search_wrap_enabled = wrap;
  search_ignore_case_enabled = !casesens;
  string_search_index(search_string, strlen(search_string), &search_index,
   search_ignore_case_enabled);

  if(result != -1)
    robo_ed_search_action(rstate, result);
}

static void execute_macro(struct robot_editor_context *rstate,
 const struct ext_macro *macro_src)
{
  struct world *mzx_world = ((context *)rstate)->world;
  const struct macro_type *current_type;
  int i, i2, i3;

  // First, the dialogue box must be generated. This will have a text
  // label for the variable name and an input widget for the variable's
  // value itself. An attempt will be made to make this look as nice
  // as possible, but these will be compromised (in this order) if
  // space is critical.
  // 1) All variables within a type group will be be aligned at a
  //    a fixed with (of whatever the largest width of the name +
  //    input widgit is) so that if they take multiple lines the
  //    boxes will line up.
  // 2) A blank line will be placed between each type group
  // 3) The width/height ratio will be made as close to 4:3 as possible,
  //    and the box will be centered.
  // The first two dialogue elements are reserved for okay/cancel

  int total_dialog_elements = macro_src->total_variables + 3;
  int num_types = macro_src->num_types;
  struct element **elements =
   cmalloc(sizeof(struct element *) * total_dialog_elements);
  int *nominal_column_widths = cmalloc(sizeof(int) * num_types);
  int *nominal_column_subwidths = cmalloc(sizeof(int) * num_types);
  int *vars_per_line = cmalloc(sizeof(int) * num_types);
  int largest_column_width = 0;
  int total_width;
  int largest_total_width;
  int *lines_needed = cmalloc(sizeof(int) * num_types);
  int nominal_width = 77, old_width = 77;
  int nominal_height, old_height = 25;
  int total_lines_needed;
  int largest;
  int current_len;
  double optimal_delta, old_optimal_delta = 1000000.0;
  int x, y, dialog_index = 2;
  struct macro_variable *current_variable;
  int draw_on_line;
  int start_x, start_y;
  struct dialog di;
  int dialog_value;
  int label_size = (int)strlen(macro_src->label) + 1;
  int subwidths[3];
  int start_line = rstate->current_line;

  // No variables? Just print bare lines...
  if(!num_types)
  {
    output_macro(rstate, macro_src);
    goto exit_free;
  }

  current_type = macro_src->types;

  for(i = 0; i < num_types; i++, current_type++)
  {
    largest = (int)strlen(current_type->variables[0].name);
    for(i2 = 1; i2 < current_type->num_variables; i2++)
    {
      current_len = (int)strlen(current_type->variables[i2].name);
      if(current_len > largest)
        largest = current_len;
    }

    nominal_column_subwidths[i] = largest;

    switch(current_type->type)
    {
      case number:
      {
        largest += 15;
        break;
      }

      case string:
      {
        largest += current_type->type_attributes[0] + 3;
        break;
      }

      case character:
      {
        largest += 5;
        break;
      }

      case color:
      {
        largest += 5;
        break;
      }
    }

    nominal_column_widths[i] = largest;

    if(largest > largest_column_width)
      largest_column_width = largest;
  }

  do
  {
    largest_total_width = 0;
    total_lines_needed = 0;

    // Determine how many lines each type needs
    for(i = 0; i < num_types; i++)
    {
      total_width = nominal_width / nominal_column_widths[i];
      if(total_width == 0)
      {
        lines_needed[i] = 1;
      }
      else
      {
        lines_needed[i] = (macro_src->types[i].num_variables +
         (total_width - 1)) / total_width;
      }

      if(total_width > macro_src->types[i].num_variables)
        total_width = macro_src->types[i].num_variables;

      total_width *= nominal_column_widths[i];

      total_lines_needed += lines_needed[i] + 1;
      if(total_width > largest_total_width)
        largest_total_width = total_width;
    }

    nominal_width = largest_total_width;
    // Add a line between each, a top line, two for buttons, and
    // two for borders
    nominal_height = total_lines_needed + 5;

    optimal_delta = fabs((80.0 / 25) -
     (double)(nominal_width + 3) / nominal_height);

    if((old_optimal_delta < optimal_delta) ||
     (nominal_width < largest_column_width) || (nominal_height > 25))
    {
      // Use the old one instead
      nominal_width = old_width;
      nominal_height = old_height;
      break;
    }

    old_width = nominal_width;
    old_height = nominal_height;
    old_optimal_delta = optimal_delta;

    nominal_width -= largest_column_width;
  } while(1);

  for(i = 0; i < num_types; i++)
  {
    vars_per_line[i] = nominal_width / nominal_column_widths[i];
  }

  if(nominal_width < label_size)
    nominal_width = label_size;

  if(nominal_width < 25)
    nominal_width = 25;

  if(nominal_width < 30)
  {
    subwidths[0] = nominal_width - 20;
    subwidths[1] = 10;
  }
  else
  {
    subwidths[0] = nominal_width / 3;
    subwidths[1] = subwidths[0];
  }

  nominal_width += 3;

  do
  {
    // Generate the dialogue box
    start_x = 40 - (nominal_width / 2);
    start_y = 12 - (nominal_height / 2);

    x = 2;
    y = 2;

    current_type = macro_src->types;

    for(i = 0, dialog_index = 3; i < num_types; i++, current_type++)
    {
      current_variable = current_type->variables;
      // Draw this many
      draw_on_line = vars_per_line[i];
      for(i2 = current_type->num_variables; i2 > 0; i2 -= draw_on_line,
       y++)
      {
        if(y >= (nominal_height - 3))
        {
          total_dialog_elements = dialog_index;
          break;
        }

        if(i2 < draw_on_line)
          draw_on_line = i2;

        x += ((nominal_width - 3) / 2) -
         ((nominal_column_widths[i] * draw_on_line) / 2);

        for(i3 = 0; i3 < draw_on_line; i3++, current_variable++,
         dialog_index++)
        {
          current_len = nominal_column_subwidths[i] -
           (int)strlen(current_variable->name);
          x += current_len;

          switch(current_type->type)
          {
            case number:
            {
              elements[dialog_index] = construct_number_box(x, y,
                current_variable->name,
                current_type->type_attributes[0],
                current_type->type_attributes[1], NUMBER_BOX,
                &(current_variable->storage.int_storage)
              );
              break;
            }

            case string:
            {
              elements[dialog_index] = construct_input_box(x, y,
                current_variable->name,
                current_type->type_attributes[0],
                current_variable->storage.str_storage
              );
              break;
            }

            case character:
            {
              elements[dialog_index] = construct_char_box(x, y,
                current_variable->name,
                1, &(current_variable->storage.int_storage)
              );
              break;
            }

            case color:
            {
              elements[dialog_index] = construct_color_box(x, y,
                current_variable->name,
                1, &(current_variable->storage.int_storage)
              );
              break;
            }
          }

          x += nominal_column_widths[i] - current_len;
        }
        // Start over on next line
        x = 2;
      }
      // Start over after skipping a line
      x = 2;

      if(y >= (nominal_height - 3))
        break;

      y++;
    }

    // Place OK/Cancel/Default

    elements[0] =
     construct_button(subwidths[0] / 2, y, "OK", 0);
    elements[1] =
     construct_button(subwidths[0] + (subwidths[1] / 2) - 2, y, "Cancel", -1);
    elements[2] =
     construct_button(subwidths[0] + subwidths[1] + (subwidths[1] / 2) - 3, y,
     "Default", -2);

    construct_dialog_ext(&di, macro_src->label, start_x,
     start_y, nominal_width, nominal_height, elements,
     total_dialog_elements, 0, 1, 2, NULL);

    // Prevent previous keys from carrying through.
    force_release_all_keys();

    dialog_value = run_dialog(mzx_world, &di);
    destruct_dialog(&di);

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    switch(dialog_value)
    {
      case 0:
      {
        // OK, output the lines
        output_macro(rstate, macro_src);
        break;
      }

      case -2:
      {
        // Plug in default values, allow to run again
        macro_default_values(rstate, macro_src);
        break;
      }
    }
  } while(dialog_value == -2);

exit_free:
  free(lines_needed);
  free(vars_per_line);
  free(nominal_column_subwidths);
  free(nominal_column_widths);
  free(elements);

  /* undo */
  if(start_line < rstate->current_line)
  {
    end_intake_undo_frame(rstate);
    add_robot_editor_undo_frame(rstate->h, rstate);
    add_undo_frame_lines(rstate, TX_NEW_LINE, start_line,
     rstate->current_line - 1);
    update_undo_frame(rstate->h);
  }
}

static void execute_numbered_macro(struct robot_editor_context *rstate, int num)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  const struct ext_macro *macro_src;
  char macro_name[32];
  int next;

  sprintf(macro_name, "%d", num);
  macro_src = find_macro(editor_conf, macro_name, &next);

  if(macro_src)
    execute_macro(rstate, macro_src);
  else
    insert_string(rstate, macros[num - 1], '^');
}

static void toggle_current_line_comment(struct robot_editor_context *rstate)
{
  /* undo */
  end_intake_undo_frame(rstate);
  add_robot_editor_undo_frame(rstate->h, rstate);
  add_robot_editor_undo_line(rstate->h, TX_OLD_BUFFER, rstate->current_line,
   rstate->current_x, rstate->command_buffer, strlen(rstate->command_buffer));

#ifdef CONFIG_DEBYTECODE

  if((rstate->command_buffer[0] == '/') && (rstate->command_buffer[1] == '/'))
  {
    size_t line_length = strlen(rstate->command_buffer + 2);
    memmove(rstate->command_buffer, rstate->command_buffer + 2, line_length + 1);
  }
  else
  {
    size_t line_length = strlen(rstate->command_buffer);
    memmove(rstate->command_buffer + 2, rstate->command_buffer, line_length + 1);
    rstate->command_buffer[0] = '/';
    rstate->command_buffer[1] = '/';
  }
  update_current_line(rstate, false);

#else /* !CONFIG_DEBYTECODE */

  if(rstate->command_buffer[0] != '.')
  {
    char comment_buffer[COMMAND_BUFFER_LEN];
    char current_char;
    char *in_position = rstate->command_buffer;
    char *out_position = comment_buffer + 3;

    comment_buffer[0] = '.';
    comment_buffer[1] = ' ';
    comment_buffer[2] = '"';

    do
    {
      current_char = *in_position;
      if((current_char == '\"') || (current_char == '\\'))
      {
        *out_position = '\\';
        out_position++;
      }

      *out_position = current_char;
      out_position++;
      in_position++;
    } while(out_position - comment_buffer < MAX_COMMAND_LEN && current_char);

    *(out_position - 1) = '"';
    *out_position = 0;

    strcpy(rstate->command_buffer, comment_buffer);
  }
  else

  if((rstate->command_buffer[0] == '.') &&
   (rstate->command_buffer[1] == ' ') &&
   (rstate->command_buffer[2] == '"') &&
   (rstate->command_buffer[strlen(rstate->command_buffer) - 1] == '"'))
  {
    char uncomment_buffer[COMMAND_BUFFER_LEN];
    char current_char;
    char *in_position = rstate->command_buffer + 3;
    char *out_position = uncomment_buffer;

    do
    {
      current_char = *in_position;
      if((current_char == '\\') && (in_position[1] == '"'))
      {
        current_char = '"';
        in_position++;
      }

      if((current_char == '\\') && (in_position[1] == '\\'))
      {
        current_char = '\\';
        in_position++;
      }

      *out_position = current_char;
      out_position++;
      in_position++;
    } while(current_char);

    *(out_position - 2) = 0;

    strcpy(rstate->command_buffer, uncomment_buffer);
  }

#endif /* !CONFIG_DEBYTECODE */

  /* undo */
  intake_sync(rstate->intk);
  add_robot_editor_undo_line(rstate->h, TX_SAME_BUFFER, rstate->current_line,
   rstate->current_x, rstate->command_buffer, strlen(rstate->command_buffer));
  update_undo_frame(rstate->h);
}

static boolean robot_editor_intake_callback(void *priv, subcontext *sub,
 enum intake_event_type type, int old_pos, int new_pos, int value, const char *data)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)priv;

  if(rstate->current_frame_type != type)
    end_intake_undo_frame(rstate);

  switch(type)
  {
    case INTK_NO_EVENT:
    case INTK_MOVE:
    case INTK_MOVE_WORDS:
      intake_apply_event_fixed(sub, type, new_pos, value, data);
      break;
    case INTK_INSERT:
    case INTK_OVERWRITE:
    case INTK_DELETE:
    case INTK_BACKSPACE:
    case INTK_BACKSPACE_WORDS:
    case INTK_CLEAR:
    case INTK_INSERT_BLOCK:
    case INTK_OVERWRITE_BLOCK:
      if(rstate->current_frame_type != type)
      {
        rstate->current_frame_type = type;
        add_robot_editor_undo_frame(rstate->h, rstate);
        add_robot_editor_undo_line(rstate->h, TX_OLD_BUFFER, rstate->current_line,
         old_pos, rstate->command_buffer, strlen(rstate->command_buffer));
      }
      intake_apply_event_fixed(sub, type, new_pos, value, data);
      add_robot_editor_undo_line(rstate->h, TX_SAME_BUFFER, rstate->current_line,
       new_pos, rstate->command_buffer, strlen(rstate->command_buffer));

      rstate->idle_timer = MAX_IDLE_TIMER;
      break;
  }
  return true;
}

#ifdef CONFIG_DEBYTECODE

// Just a place holder until the config file is adapted to the new system.

// From configure.c:

// Default colors for color coding:
// 0 current line - 11
// 1 immediates - 10
// 2 characters - 14
// 3 colors - color box or 2
// 4 directions - 3
// 5 things - 11
// 6 params - 2
// 7 strings - 14
// 8 equalities - 0
// 9 conditions - 15
// 10 items - 11
// 11 extras - 7
// 12 commands and command fragments - 15

static const char _new_color_code_table[] =
{
  11,   // current line
  10,   // ARG_TYPE_INDEXED_IMMEDIATE,
  14,   // ARG_TYPE_INDEXED_STRING,
  14,   // ARG_TYPE_INDEXED_CHARACTER,
  2,    // ARG_TYPE_INDEXED_COLOR,
  14,   // ARG_TYPE_INDEXED_PARAM,
  12,   // ARG_TYPE_INDEXED_NAME,
  3,    // ARG_TYPE_INDEXED_DIRECTION,
  11,   // ARG_TYPE_INDEXED_THING,
  0,    // ARG_TYPE_INDEXED_EQUALITY,
  15,   // ARG_TYPE_INDEXED_CONDITION,
  11,   // ARG_TYPE_INDEXED_ITEM,
  7,    // ARG_TYPE_INDEXED_IGNORE,
  15,   // ARG_TYPE_INDEXED_FRAGMENT,
  15,   // ARG_TYPE_INDEXED_COMMAND,
  13,   // ARG_TYPE_INDEXED_EXPRESSION
  2,    // ARG_TYPE_INDEXED_LABEL
  0,    // ARG_TYPE_INDEXED_COMMENT
};

// TODO: write_string seriously needs to have a length field,
// so we don't have to keep stuffing null terminators in the thing.

static void display_robot_line(struct robot_editor_context *rstate,
 struct robot_line *current_rline, int y)
{
  int x = 2;
  int color_code = 1;
  const char *color_code_table = _new_color_code_table;
  int line_color = color_code_table[ARG_TYPE_INDEXED_FRAGMENT + 1];
  char *line_text = current_rline->line_text;
  char temp_char;

  switch(current_rline->command_type)
  {
    case COMMAND_TYPE_BLANK_LINE:
      // No need to print a blank line.
      return;

    case COMMAND_TYPE_INVALID:
    case COMMAND_TYPE_UNKNOWN:
      color_code = 0;
      line_color = combine_colors(1, bg_color);
      break;

    case COMMAND_TYPE_COMMAND_START:
    case COMMAND_TYPE_COMMAND_CONTINUE:
      color_code = combine_colors(1, bg_color);
      break;
  }

  if(color_code == 0)
  {
    if(current_rline->line_text[0] != 0)
    {
      if(strlen(current_rline->line_text) > 76)
      {
        temp_char = line_text[76];
        line_text[76] = 0;
        write_string(line_text, x, y, line_color, WR_MASK);
        line_text[76] = temp_char;
      }
      else
      {
        write_string(line_text, x, y, line_color, WR_MASK);
      }
    }
  }
  else
  {
    struct color_code_pair *color_codes = current_rline->color_codes;
    enum arg_type_indexed arg_type_indexed;
    int offset;
    int length;
    int color;
    int i;

    for(i = 0; i < current_rline->num_color_codes; i++)
    {
      offset = color_codes[i].offset;
      length = color_codes[i].length;
      arg_type_indexed = color_codes[i].arg_type_indexed;

      if(arg_type_indexed == ARG_TYPE_INDEXED_COLOR)
      {
        color = get_color(line_text + offset);
        draw_color_box(color & 0xFF, color >> 8, x + offset, y, 78);
      }
      else
      {
        color = color_code_table[arg_type_indexed + 1];
        color = combine_colors(color, bg_color);

        if((offset + length) > 76)
        {
          if(offset < 76)
          {
            temp_char = line_text[76];
            line_text[76] = 0;

            write_string(line_text + offset, x + offset, y, color, WR_MASK);
            line_text[76] = temp_char;
          }
          break;
        }

        temp_char = line_text[offset + length];
        line_text[offset + length] = 0;
        write_string(line_text + offset, x + offset, y, color, WR_MASK);
        line_text[offset + length] = temp_char;
      }
    }
  }
}

static inline int validate_lines(struct robot_editor_context *rstate,
 int show_none)
{
  return 0;
}

#else /* !CONFIG_DEBYTECODE */

static void display_robot_line(struct robot_editor_context *rstate,
 struct robot_line *current_rline, int y)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  int i;
  int x = 2;
  int current_color, current_arg;
  boolean color_coding_on = editor_conf->color_coding_on;
  const char *color_codes = editor_conf->color_codes;
  char temp_char;
  char temp_buffer[COMMAND_BUFFER_LEN];
  char *line_pos = current_rline->line_text;
  int chars_offset = 0;
  size_t arg_length;
  int use_mask;

  if(current_rline->line_text[0] != 0)
  {
    if(current_rline->validity_status != valid)
    {
      current_color =
       combine_colors(color_codes[S_CMD + current_rline->validity_status + 1],
       bg_color);

      if(strlen(current_rline->line_text) > 76)
      {
        temp_char = current_rline->line_text[76];
        current_rline->line_text[76] = 0;
        write_string(current_rline->line_text, x, y, current_color, WR_MASK);
        current_rline->line_text[76] = temp_char;
      }
      else
      {
        write_string(current_rline->line_text, x, y, current_color, WR_MASK);
      }
    }
    else
    {
      use_mask = 0;
      arg_length = 0;

      if(!color_coding_on)
      {
        current_color =
         combine_colors(color_codes[S_STRING + 1], bg_color);
      }

      if(color_coding_on)
        current_color = combine_colors(color_codes[S_CMD + 1], bg_color);

      arg_length = strcspn(line_pos, " ") + 1;
      memcpy(temp_buffer, line_pos, arg_length);
      temp_buffer[arg_length] = 0;

      write_string(temp_buffer, x, y, current_color, WR_NONE);

      line_pos += arg_length;
      x += (int)arg_length;

      for(i = 0; i < current_rline->num_args; i++)
      {
        current_arg = current_rline->arg_types[i];

        if(current_arg == S_STRING)
        {
          char current;
          arg_length = 1;

          do
          {
            current = line_pos[arg_length];
            if((current == '\\') && ((line_pos[arg_length + 1] == '"') ||
             (line_pos[arg_length + 1] == '\\')))
            {
              arg_length++;
            }
            arg_length++;

          } while((current != '"') && current);

          arg_length++;

          chars_offset = 0;

          use_mask = get_config()->mask_midchars;
        }
        else

        if(current_arg == S_CHARACTER)
        {
          int offset_hack = (line_pos[1] == '\'') ? 1 : 0;
          arg_length = strcspn(line_pos + 1, "\'") + 3 + offset_hack;
          chars_offset = 0;
        }
        else
        {
          arg_length = strcspn(line_pos, " ") + 1;
          chars_offset = PRO_CH;
        }

        if((current_arg == S_COLOR) && (color_codes[current_arg + 1] == 255))
        {
          unsigned int color = get_color(line_pos);
          draw_color_box(color & 0xFF, color >> 8, x, y, 78);
        }
        else
        {
          if(color_coding_on)
          {
            current_color =
             combine_colors(color_codes[current_arg + 1], bg_color);
          }

          memcpy(temp_buffer, line_pos, arg_length);
          temp_buffer[arg_length - 1] = 0;

          if((x + arg_length) > 78)
            temp_buffer[78 - x] = 0;

          if(use_mask)
          {
            write_string(temp_buffer, x, y, current_color, WR_MASK);
          }
          else
          {
            if(current_arg == S_CHARACTER)
            {
              temp_buffer[arg_length - 2] = 0;
              write_string("'", x, y, current_color, WR_MASK);
              write_string_ext(temp_buffer + 1, x + 1, y, current_color,
               WR_NONE, chars_offset, 16);
              write_string("'", x + (int)arg_length - 2, y, current_color, WR_MASK);
            }
            else
            {
              write_string_ext(temp_buffer, x, y, current_color,
               WR_NONE, chars_offset, 16);
            }
          }

          if((x + arg_length) > 78)
            break;
        }

        line_pos += (int)arg_length;
        x += (int)arg_length;
      }
    }
  }
}

#define VALIDATE_ELEMENTS 60
#define MAX_ERRORS 256

// This will only check for errors after current_rline, so the
// base should be passed.

static int validate_lines(struct robot_editor_context *rstate, int show_none)
{
  // 70x5-21 square that displays up to 13 error messages,
  // and up to 48 buttons for delete/comment out/ignore. In total,
  // the elements are text display to tell how many erroroneous
  // lines there are, 13 text bars to display the error message
  // and validity status, 13 delete buttons, 13 comment buttons,
  // 13 ignore buttons, a next button, a previous button,
  // and yes/no buttons. There can thus be up to 57 or so elements.
  // The first one is always text, and the second are always
  // yes/no buttons. The rest are variable depending on how
  // many errors there are. The text button positions cannot be
  // determined until the number of errors is.
  // Doesn't take any more errors after the number of erroneous
  // lines exceeds MAX_ERRORS.

  struct world *mzx_world = ((context *)rstate)->world;
  struct element *elements[VALIDATE_ELEMENTS];
  struct dialog di;
  char information[72] = { 0 };
  int start_line = 0;
  int num_errors = 0;
  char error_messages[MAX_ERRORS][64];
  char null_buffer[256];
  struct robot_line *line_pointers[MAX_ERRORS];
  struct robot_line *current_rline = rstate->base.next;
  enum validity_types validity_options[MAX_ERRORS];
  int redo = 1;
  int element_pos;
  int errors_shown;
  int multi_pages = 0;
  int line_number = 0;
  int dialog_result;
  int num_ignore = 0;
  int current_size = rstate->size;
  int current_element = 4;
  int i;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  // First, collect the number of errors, and process error messages
  // by calling assemble_line.

  while(current_rline != NULL)
  {
    if(current_rline->validity_status != valid)
    {
      size_t err_len, i;
      memset(error_messages[num_errors], ' ', 64);
      sprintf(error_messages[num_errors], "%05d: ", line_number + 1);
      legacy_assemble_line(current_rline->line_text, null_buffer,
       error_messages[num_errors] + 7, NULL, NULL);
      /* Filter out control chars. */
      err_len = strlen(error_messages[num_errors]);
      for(i = 0; i < err_len; i++)
        if(error_messages[num_errors][i] == '\n')
          error_messages[num_errors][i] = ' ';

      error_messages[num_errors][err_len] = ' ';
      line_pointers[num_errors] = current_rline;
      validity_options[num_errors] = current_rline->validity_status;

      if(current_rline->validity_status == invalid_uncertain)
        num_ignore++;

      num_errors++;
      if(num_errors == MAX_ERRORS)
        break;
    }
    current_rline = current_rline->next;
    line_number++;
  }

  // The button return messages are as such:
  // 0 - ok
  // -1 - cancel
  // 1 - previous
  // 2 - next
  // 100-113 - ignore line
  // 200-213 - delete line
  // 300-313 - comment line

  if(num_errors > 12)
  {
    multi_pages = 1;
  }

  if(num_ignore || show_none)
  {
    do
    {
      errors_shown = num_errors - start_line;

      if(errors_shown > 12)
        errors_shown = 12;

      elements[0] = construct_label(5, 2, information);
      elements[1] = construct_button(28, 18, "OK", 0);
      elements[2] = construct_button(38, 18, "Cancel", -1);

      for(i = 0, element_pos = 3; i < errors_shown; i++,
       element_pos += 4)
      {
        switch(validity_options[start_line + i])
        {
          case invalid_uncertain:
          {
            sprintf(error_messages[start_line + i] + 44, " (ignore)");
            break;
          }

          case invalid_discard:
          {
            sprintf(error_messages[start_line + i] + 44, " (delete)");
            break;
          }

          case invalid_comment:
          {
            sprintf(error_messages[start_line + i] + 44, "(comment)");
            break;
          }

          default:
          {
            break;
          }
        }

        elements[element_pos] = construct_label(2, i + 4,
         error_messages[start_line + i]);
        elements[element_pos + 1] =
         construct_button(56, i + 4, "I", start_line + i + 100);
        elements[element_pos + 2] =
         construct_button(60, i + 4, "D", start_line + i + 200);
        elements[element_pos + 3] =
         construct_button(64, i + 4, "C", start_line + i + 300);
      }

      if(multi_pages)
      {
        if(start_line)
        {
          elements[element_pos] =
           construct_button(5, 17, "Previous", 1);
          element_pos++;
        }

        if((start_line + 12) < num_errors)
        {
          elements[element_pos] =
           construct_button(61, 17, "Next", 2);
          element_pos++;
        }

        sprintf(information, "%d errors found; displaying %d through %d.\n",
         num_errors, start_line + 1, start_line + errors_shown);
      }
      else
      {
        if(num_errors == 1)
        {
          sprintf(information, "1 error found.");
        }
        else

        if(!num_errors)
        {
          sprintf(information, "No errors found.");
        }
        else
        {
          sprintf(information, "%d errors found.", num_errors);
        }
      }

      if(current_element >= element_pos)
        current_element = 1;

      construct_dialog(&di, "Command Summary", 5, 2, 70, 21,
       elements, element_pos, current_element);

      dialog_result = run_dialog(mzx_world, &di);
      current_element = di.current_element;
      destruct_dialog(&di);

      if(dialog_result == -1)
      {
        // Cancel - bails
        redo = 0;
        break;
      }
      else

      if(dialog_result == 0)
      {
        // Okay, update validity options
        redo = 0;
        for(i = 0; i < num_errors; i++)
        {
          (line_pointers[i])->validity_status = validity_options[i];

          if(validity_options[i] == invalid_comment)
          {
            (line_pointers[i])->line_bytecode_length =
             (line_pointers[i])->line_text_length + 5;
          }
          else
          {
            (line_pointers[i])->line_bytecode_length = 0;
            (line_pointers[i])->line_text_length = 0;
          }
        }

        rstate->size = current_size;
        continue;
      }
      else

      if((dialog_result >= 100) && (dialog_result < 200))
      {
        // If the last is comment, adjust size
        if(validity_options[dialog_result - 100] == invalid_comment)
        {
          current_size -=
           (line_pointers[dialog_result - 100])->line_text_length + 5;
        }

        // Make it ignore
        validity_options[dialog_result - 100] = invalid_uncertain;
      }
      else

      if((dialog_result >= 200) && (dialog_result < 300))
      {
        // If the last is comment, adjust size
        if(validity_options[dialog_result - 200] == invalid_comment)
        {
          current_size -=
           (line_pointers[dialog_result - 200])->line_text_length + 5;
        }

        // Make it delete
        validity_options[dialog_result - 200] = invalid_discard;
      }
      else

      if((dialog_result >= 300) && (dialog_result < 400))
      {
        if(current_size +
         ((line_pointers[dialog_result - 300])->line_text_length + 5) <
         MAX_OBJ_SIZE)
        {
          // If the last is not comment, adjust size
          if(validity_options[dialog_result - 300] != invalid_comment)
          {
            current_size +=
             (line_pointers[dialog_result - 300])->line_text_length + 5;
          }

          // Make it comment
          validity_options[dialog_result - 300] = invalid_comment;
        }
      }
      else

      if(dialog_result == 1)
      {
        start_line -= 12;
        current_element = 4;
        continue;
      }
      else

      if(dialog_result == 2)
      {
        start_line += 12;
        current_element = 4;
        continue;
      }

      // Next issue
      current_element += 4;
      if(current_element >= element_pos)
      {
        // Next
        if(num_errors - start_line > 12)
          current_element = element_pos - 1;

        // Okay
        else
          current_element = 1;
      }

    } while(redo);
  }

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  update_current_line(rstate, false);
  return num_ignore;
}

#endif /* !CONFIG_DEBYTECODE */

static void init_robot_lines(struct robot_editor_context *rstate,
 struct robot *cur_robot)
{
#ifdef CONFIG_DEBYTECODE
  char *source_pos;
  char *newline_pos;
  int line_length;
#else
  const struct editor_config_info *editor_conf = get_editor_config();
  char text_buffer[COMMAND_BUFFER_LEN], error_buffer[COMMAND_BUFFER_LEN];
  int line_text_length, line_bytecode_length, new_line, arg_count;
  char *current_robot_pos = cur_robot->program_bytecode + 1;
  struct robot_line *previous_rline = &(rstate->base);
  struct robot_line *current_rline = NULL;
  char arg_types[32], *next;
#endif

  rstate->base.previous = NULL;
  rstate->base.next = NULL;

#ifdef CONFIG_DEBYTECODE
  // This should take care of everything for loading the program from
  // source.
  source_pos = cur_robot->program_source;

  do
  {
    newline_pos = strchr(source_pos, '\n');

    if(newline_pos == NULL)
    {
      strcpy(rstate->command_buffer, source_pos);
    }
    else
    {
      line_length = newline_pos - source_pos;
      if(line_length)
        memcpy(rstate->command_buffer, source_pos, line_length);

      rstate->command_buffer[line_length] = 0;
    }

    add_line(rstate, rstate->command_buffer, 1);
    source_pos = newline_pos + 1;
  }
  while(newline_pos);

  // Move it back to the start
  rstate->current_rline = rstate->base.next;
  rstate->current_line = 1;

#else /* !CONFIG_DEBYTECODE */

  rstate->base.line_bytecode_length = -1;

  // Disassemble robots into lines
  do
  {
    new_line = disassemble_line(current_robot_pos, &next,
     text_buffer, error_buffer, &line_text_length,
     editor_conf->disassemble_extras, arg_types, &arg_count,
     editor_conf->disassemble_base);

    if(new_line)
    {
      current_rline = cmalloc(sizeof(struct robot_line));

      line_bytecode_length = (int)(next - current_robot_pos);
      current_rline->line_text_length = line_text_length;
      current_rline->line_bytecode_length = line_bytecode_length;
      current_rline->line_text = cmalloc(line_text_length + 1);
      current_rline->line_bytecode = cmalloc(line_bytecode_length);
      current_rline->num_args = arg_count;
      current_rline->validity_status = valid;

      memcpy(current_rline->arg_types, arg_types, 20);
      memcpy(current_rline->line_bytecode, current_robot_pos,
       line_bytecode_length);
      memcpy(current_rline->line_text, text_buffer, line_text_length + 1);

      previous_rline->next = current_rline;
      current_rline->previous = previous_rline;

      rstate->total_lines++;

      current_robot_pos = next;
      previous_rline = current_rline;

      rstate->size += line_bytecode_length;
    }
    else
    {
      previous_rline->next = NULL;
    }
  }
  while(new_line);

  // Add a blank line to the end too if there aren't any lines
  if(!rstate->total_lines)
  {
    add_blank_line(rstate, 1);
  }
  else
  {
    move_line_down(rstate, 1);
  }
#endif /* !CONFIG_DEBYTECODE */
}

static boolean robot_editor_draw(context *ctx)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;

  const struct editor_config_info *editor_conf = get_editor_config();
  int intk_color = combine_colors(editor_conf->color_codes[0], bg_color);
  boolean use_mask = get_config()->mask_midchars;

  struct robot_line *draw_rline;
  int first_line_draw_position;
  int first_line_count_back;
  int middle_line_len;
  int start_offset = 0;
  int temp_char = 0;
  int temp_pos = 0;
  int cursor_x;
  int i;

#ifdef CONFIG_DEBYTECODE
  // Update program status if it has been modified.
  // This needs to be done after all input and prior to drawing lines.
  if(rstate->program_modified)
  {
    update_program_status(rstate, rstate->base.next, NULL);
    rstate->program_modified = false;
    rstate->confirm_changes = true;
  }
#endif

  fill_line(80, 0, 0, top_char, top_color);

  if(rstate->scr_hide_mode)
  {
    write_string(key_help_hide, 0, 24, bottom_text_color, WR_NEWLINE);
    rstate->scr_line_start = 1;
    rstate->scr_line_middle = 12;
    rstate->scr_line_end = 23;
  }
  else
  {
    write_string(key_help, 0, 22, bottom_text_color, WR_NEWLINE);
    rstate->scr_line_start = 2;
    rstate->scr_line_middle = 11;
    rstate->scr_line_end = 20;
  }

  draw_char(top_line_connect, line_color, 0, 1);
  draw_char(top_line_connect, line_color, 79, 1);
  draw_char(bottom_line_connect, line_color, 0, 21);
  draw_char(bottom_line_connect, line_color, 79, 21);

  fill_line(78, 1, 1, horizontal_line, line_color);
  fill_line(78, 1, 21, horizontal_line, line_color);

  for(i = rstate->scr_line_start; i < rstate->scr_line_middle; i++)
  {
    draw_char(vertical_line, line_color, 0, i);
    draw_char(vertical_line, line_color, 79, i);
    fill_line(78, 1, i, bg_char, bg_color_solid);
  }

  for(i++; i <= rstate->scr_line_end; i++)
  {
    draw_char(vertical_line, line_color, 0, i);
    draw_char(vertical_line, line_color, 79, i);
    fill_line(78, 1, i, bg_char, bg_color_solid);
  }

  // Make sure the line length and current char are up-to-date.
  intake_sync(rstate->intk);

  write_string("Line:", 2, 0, top_highlight_color, WR_NONE);
  write_string("/", 14, 0, top_highlight_color, WR_NONE);
  write_number(rstate->current_line, top_text_color, 8, 0, 6, false, 10);
  write_number(rstate->total_lines, top_text_color, 15, 0, 6, false, 10);

  write_string("Col:", 23, 0, top_highlight_color, WR_NONE);
  write_string("/", 31, 0, top_highlight_color, WR_NONE);
  write_number(rstate->current_x + 1, top_text_color, 28, 0, 3, false, 10);
  write_number(rstate->current_line_len + 1, top_text_color, 32, 0, 3, false,
   10);

  write_string("Size:", 37, 0, top_highlight_color, WR_NONE);
  write_string("/", 50, 0, top_highlight_color, WR_NONE);
  write_number(rstate->size, top_text_color, 43, 0, 7, false, 10);
  write_number(rstate->max_size, top_text_color, 51, 0, 7, false, 10);

  write_string("X:", 60, 0, top_highlight_color, WR_NONE);
  write_number(rstate->cur_robot->xpos, top_text_color, 63, 0, 5, false, 10);

  write_string("Y:", 70, 0, top_highlight_color, WR_NONE);
  write_number(rstate->cur_robot->ypos, top_text_color, 73, 0, 5, false, 10);

  // Now, draw the lines. Start with 9 back from the current.

  if(rstate->current_line >
   (rstate->scr_line_middle - rstate->scr_line_start))
  {
    first_line_draw_position = rstate->scr_line_start;
    first_line_count_back =
     rstate->scr_line_middle - rstate->scr_line_start;
  }
  else
  {
    // Or go back as many as we can
    first_line_draw_position = 1 +
     (rstate->scr_line_middle - rstate->current_line);
    first_line_count_back = rstate->current_line - 1;
  }

  draw_rline = rstate->current_rline;
  for(i = 0; i < first_line_count_back; i++)
  {
    draw_rline = draw_rline->previous;
  }

  // Now draw start - scr_end + 1 lines, or until bails
  for(i = first_line_draw_position;
   (i <= rstate->scr_line_end) && draw_rline; i++)
  {
    if(i != rstate->scr_line_middle)
      display_robot_line(rstate, draw_rline, i);

    draw_rline = draw_rline->next;
  }

  // Mark block selection area
  if(rstate->mark_mode)
  {
    int draw_start = rstate->current_line -
     (rstate->scr_line_middle - rstate->scr_line_start);
    int draw_end = rstate->current_line +
     (rstate->scr_line_end - rstate->scr_line_middle) + 1;

    if(rstate->mark_start <= draw_end)
    {
      if(rstate->mark_start >= draw_start)
      {
        draw_start = rstate->mark_start +
         rstate->scr_line_middle - rstate->current_line;
        draw_char('S', mark_color, 0, draw_start);
      }
      else
      {
        draw_start += rstate->scr_line_middle - rstate->current_line;
      }

      if(rstate->mark_end <= draw_end)
      {
        draw_end = rstate->mark_end + rstate->scr_line_middle -
        rstate->current_line;
      }
      else
      {
        draw_end += rstate->scr_line_middle - rstate->current_line;
      }

      if(draw_start < rstate->scr_line_start)
      {
        draw_start = rstate->scr_line_start;
      }

      if(draw_end > rstate->scr_line_end)
      {
        draw_end = rstate->scr_line_end;
      }
      else
      {
        if((rstate->mark_start != rstate->mark_end) &&
         (draw_end >= rstate->scr_line_start))
          draw_char('E', mark_color, 0, draw_end);
      }

      if(draw_end >= rstate->scr_line_start)
      {
        for(i = draw_start; i <= draw_end; i++)
        {
          color_line(80, 0, i, mark_color);
        }

        if((draw_start <= rstate->scr_line_middle) &&
         (draw_end >= rstate->scr_line_middle))
          intk_color = mark_color;
      }
    }
  }

  // Current line arrows
  if(rstate->current_x < 76)
    draw_char('\x10', intk_color, 0, rstate->scr_line_middle);
  else
    draw_char('\xae', intk_color, 0, rstate->scr_line_middle);

  if(rstate->current_line_len < 76 ||
   rstate->current_x == rstate->current_line_len)
    draw_char('\x11', intk_color, 79, rstate->scr_line_middle);
  else
    draw_char('\xaf', intk_color, 79, rstate->scr_line_middle);

  // Current line.
  middle_line_len = rstate->current_line_len;
  cursor_x = rstate->current_x;
  if(rstate->current_x >= 76)
  {
    if(rstate->command_buffer[rstate->current_x])
    {
      temp_pos = rstate->current_x + 1;
      temp_char = rstate->command_buffer[temp_pos];
      rstate->command_buffer[temp_pos] = 0;
    }
    start_offset = rstate->current_x - 76 + 1;
    middle_line_len = strlen(rstate->command_buffer + start_offset);
    cursor_x = 75;
  }
  else

  if(middle_line_len > 76)
  {
    temp_pos = 76;
    temp_char = rstate->command_buffer[temp_pos];
    rstate->command_buffer[temp_pos] = 0;
    middle_line_len = 76;
  }

  if(intake_get_insert())
    cursor_underline(cursor_x + 2, rstate->scr_line_middle);
  else
    cursor_solid(cursor_x + 2, rstate->scr_line_middle);

  if(use_mask)
  {
    write_string(rstate->command_buffer + start_offset,
     2, rstate->scr_line_middle, intk_color, WR_MASK);
  }
  else
  {
    write_string_ext(rstate->command_buffer + start_offset,
     2, rstate->scr_line_middle, intk_color, WR_NONE, 0, 16);
  }

  // Fill non-text portions of the middle line.
  fill_line(76 + 1 - middle_line_len,
   2 + middle_line_len, rstate->scr_line_middle, 32, intk_color);
  draw_char(bg_char, intk_color, 1, rstate->scr_line_middle);

  if(temp_pos)
    rstate->command_buffer[temp_pos] = temp_char;

  return true;
}

static boolean robot_editor_idle(context *ctx)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;

  /* Time out the current editing undo frame if no text has been entered. */
  if(rstate->idle_timer > 0)
  {
    rstate->idle_timer--;
    if(!rstate->idle_timer)
      end_intake_undo_frame(rstate);
  }

  // Disable the cursor so it doesn't display over other interfaces.
  // The draw function will enable it again if needed.
  cursor_off();

  rstate->macro_repeat_level = 0;
  rstate->macro_recurse_level = 0;
  return false;
}

static boolean robot_editor_mouse(context *ctx, int *key, int button,
 int x, int y)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;

  if(button && (button <= MOUSE_BUTTON_RIGHT))
  {
    if((y >= rstate->scr_line_start) && (y <= rstate->scr_line_end) &&
     (x >= 2) && (x <= 78))
    {
      move_and_update(rstate, y - rstate->scr_line_middle);
      rstate->current_x = x - 2;

      if(y != rstate->scr_line_middle)
        warp_mouse(x, rstate->scr_line_middle);
      return true;
    }
  }
  else

  if(button == MOUSE_BUTTON_WHEELUP)
  {
    move_and_update(rstate, -3);
    return true;
  }
  else

  if(button == MOUSE_BUTTON_WHEELDOWN)
  {
    move_and_update(rstate, 3);
    return true;
  }

  return false;
}

static boolean robot_editor_joystick(context *ctx, int *key, int action)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;

  // Note: intake context also handles X, Y.
  switch(action)
  {
    // Special case for backspace at the start of a line.
    // Otherwise, intake should handle this.
    case JOY_Y:
    {
      if(rstate->current_x == 0 && rstate->current_line > 1)
      {
        combine_current_line(rstate, -1);
        return true;
      }
      break;
    }

    // Thing param menu
    case JOY_LSHOULDER:
      *key = IKEY_F4;
      return true;

    // Defaults for everything else...
    default:
    {
      enum keycode ui_key = get_joystick_ui_key();
      if(ui_key)
      {
        *key = ui_key;
        return true;
      }
      break;
    }
  }
  return false;
}

static boolean robot_editor_key(context *ctx, int *key)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;
  const struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ((context *)rstate)->world;

  // Exit event - ignore other input
  boolean exit_status = get_exit_status();
  if(exit_status)
    *key = 0;

  switch(*key)
  {
    case IKEY_UP:
    {
      move_and_update(rstate, -1);
      return true;
    }

    case IKEY_DOWN:
    {
      move_and_update(rstate, 1);
      return true;
    }

    case IKEY_PAGEUP:
    {
      move_and_update(rstate, -9);
      return true;
    }

    case IKEY_PAGEDOWN:
    {
      move_and_update(rstate, 9);
      return true;
    }

    case IKEY_BACKSPACE:
    {
      // Let intake handle Alt+Backspace and Ctrl+Backspace.
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
        break;

      if(rstate->current_x == 0 && rstate->current_line > 1)
      {
        combine_current_line(rstate, -1);
        return true;
      }
      break;
    }

    case IKEY_DELETE:
    {
      if(rstate->command_buffer[rstate->current_x] == 0 &&
       rstate->current_rline->next)
      {
        combine_current_line(rstate, 1);
        return true;
      }
      break;
    }

    case IKEY_RETURN:
    {
      // Block action menu (also see Alt+B).
      if(get_alt_status(keycode_internal))
      {
        end_intake_undo_frame(rstate);
        update_current_line(rstate, true);
        block_action(rstate);
      }
      else

      // Split line into two
      // NOTE: versions prior to 2.93 always inserted a blank line at column 0.
      // That behavior was merged into this to simplify the undo code.
      if(rstate->current_x == 0 || editor_conf->editor_enter_splits)
      {
        split_current_line(rstate);
      }

      // Old way, just navigate down a line
      else
        move_and_update(rstate, 1);

      return true;
    }

    case IKEY_F2:
    {
      int new_color;

      // Ignore Alt+F2, Ctrl+F2
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
        break;

      new_color = color_selection(last_color_selected, 1);
      if(new_color >= 0)
      {
        char color_buffer[16];
        last_color_selected = new_color;
        print_color(new_color, color_buffer);
        insert_string(rstate, color_buffer, 0);
      }

      return true;
    }

    case IKEY_F3:
    {
      int new_char = char_selection(last_char_selected);

      if(new_char >= 0)
      {
        char char_buffer[16];
        int chars_length = unescape_char(char_buffer, new_char);
        char_buffer[chars_length] = 0;

        insert_string(rstate, char_buffer, 0);

        last_char_selected = new_char;
      }
      return true;
    }

    case IKEY_F4:
    {
      int thing_index, end_x, start_x = rstate->current_x;

      // ALT+F4 - do nothing.
      if(get_alt_status(keycode_internal))
        return false;

      if(!rstate->command_buffer[start_x])
        start_x--;

      while(start_x && (rstate->command_buffer[start_x] == ' '))
        start_x--;

      end_x = start_x + 1;

      while(start_x && (rstate->command_buffer[start_x] != ' '))
        start_x--;

      if(start_x)
        start_x++;

#ifdef CONFIG_DEBYTECODE
      {
        thing_index =
         get_thing(rstate->command_buffer + start_x, end_x - start_x);

        if((thing_index == -1) || (thing_index >= SENSOR))
          return true;
      }
#else /* !CONFIG_DEBYTECODE */
      {
        const struct search_entry_short *matched_arg;
        char temp_char;

        temp_char = rstate->command_buffer[end_x];
        rstate->command_buffer[end_x] = 0;
        matched_arg = find_argument(rstate->command_buffer + start_x);
        rstate->command_buffer[end_x] = temp_char;

        if(!matched_arg || (matched_arg->type != S_THING) ||
         (matched_arg->offset >= 122))
          return true;

        thing_index = matched_arg->offset;
      }
#endif /* !CONFIG_DEBYTECODE */

      {
        int new_param = edit_param(mzx_world, thing_index, -1);
        if(new_param >= 0)
        {
          char param_buffer[16];
          sprintf(param_buffer, " p%02x", new_param);
          insert_string(rstate, param_buffer, 0);
        }
      }
      return true;
    }

    case IKEY_F5:
    {
      int i;
      int char_edited;
      char char_buffer[14];
      char char_string_buffer[64];
      char *buffer_pos = char_string_buffer;

      char_edited = char_editor(mzx_world);

      ec_read_char(char_edited, char_buffer);

      for(i = 0; i < 14; i++)
      {
        sprintf(buffer_pos, " %d", char_buffer[i]);
        buffer_pos += strlen(buffer_pos);
      }

      insert_string(rstate, char_string_buffer, 0);
      return true;
    }

    case IKEY_F6:
    {
      execute_numbered_macro(rstate, 1);
      return true;
    }

    case IKEY_F7:
    {
      execute_numbered_macro(rstate, 2);
      return true;
    }

    case IKEY_F8:
    {
      execute_numbered_macro(rstate, 3);
      return true;
    }

    case IKEY_F9:
    {
      execute_numbered_macro(rstate, 4);
      return true;
    }

    case IKEY_F10:
    {
      execute_numbered_macro(rstate, 5);
      return true;
    }

    case IKEY_v:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        end_intake_undo_frame(rstate);
        update_current_line(rstate, true);
        validate_lines(rstate, 1);
        return true;
      }
      break;
    }

#ifdef CONFIG_DEBYTECODE
    case IKEY_c:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        toggle_current_line_comment(rstate);
        return true;
      }
      break;
    }
#else /* !CONFIG_DEBYTECODE */
    case IKEY_c:
    {
      if(!get_ctrl_status(keycode_internal) ||
       get_alt_status(keycode_internal) || get_shift_status(keycode_internal))
        break;

      if(rstate->current_rline->validity_status != valid)
      {
        rstate->current_rline->validity_status = invalid_comment;
        update_current_line(rstate, true);
      }
      else
        toggle_current_line_comment(rstate);

      return true;
    }

    case IKEY_d:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(rstate->current_rline->validity_status != valid)
        {
          rstate->current_rline->validity_status = invalid_discard;
          update_current_line(rstate, true);
        }
        return true;
      }
      break;
    }
#endif /* !CONFIG_DEBYTECODE */

    case IKEY_ESCAPE:
    {
      exit_status = 1;
      break;
    }

    // Mark start/end
    case IKEY_s:
    case IKEY_e:
    case IKEY_HOME:
    case IKEY_END:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(*key == IKEY_HOME)
        {
          // Jump to the first line of the program.
          goto_line(rstate, 1, 0);
          return true;
        }
        else

        if(*key == IKEY_END)
        {
          // Jump to the last line of the program.
          goto_line(rstate, rstate->total_lines, INT_MAX);
          return true;
        }
      }
      else

      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        int mark_switch;

        update_current_line(rstate, true);

        if(rstate->mark_mode == 2)
        {
          if((*key == IKEY_HOME) || (*key == IKEY_s))
            mark_switch = 0;
          else
            mark_switch = 1;
        }
        else
        {
          mark_switch = rstate->mark_mode;
          rstate->mark_mode++;
        }

        switch(mark_switch)
        {
          case 0:
          {
            // Always mark the start
            rstate->mark_start = rstate->current_line;
            rstate->mark_start_rline = rstate->current_rline;
            if(rstate->mark_mode == 1)
            {
              // If this is the real start, mark the end too
              rstate->mark_end = rstate->current_line;
              rstate->mark_end_rline = rstate->current_rline;
            }
            break;
          }

          case 1:
          {
            // Always mark the end
            rstate->mark_end = rstate->current_line;
            rstate->mark_end_rline = rstate->current_rline;
            break;
          }
        }

        // We have more than a start, so possibly swap
        if(rstate->mark_mode && (rstate->mark_start > rstate->mark_end))
        {
          int mark_swap;
          struct robot_line *mark_swap_rline;

          mark_swap = rstate->mark_start;
          rstate->mark_start = rstate->mark_end;
          rstate->mark_end = mark_swap;
          mark_swap_rline = rstate->mark_start_rline;
          rstate->mark_start_rline = rstate->mark_end_rline;
          rstate->mark_end_rline = mark_swap_rline;
        }
        return true;
      }
      break;
    }

    // Block action menu (also see Alt+Enter).
    case IKEY_b:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        end_intake_undo_frame(rstate);
        update_current_line(rstate, true);
        block_action(rstate);
        return true;
      }
      break;
    }

    case IKEY_g:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        goto_position(rstate);
        return true;
      }
      break;
    }

    case IKEY_p:
    case IKEY_INSERT:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        paste_buffer(rstate);
        return true;
      }
      break;
    }

    // Import file
    case IKEY_i:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        import_block(rstate);
        return true;
      }
#ifndef CONFIG_DEBYTECODE
      else

      if(get_ctrl_status(keycode_internal) &
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(rstate->current_rline->validity_status != valid)
        {
          rstate->current_rline->validity_status = invalid_uncertain;
        }
        return true;
      }
#endif
      break;
    }

    // Export file
    case IKEY_x:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        if(rstate->mark_mode)
        {
          export_block(rstate, 1);
        }
        else
        {
          export_block(rstate, 0);
        }
        return true;
      }
      break;
    }

    // Single line macros
    case IKEY_o:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        edit_single_line_macros(rstate);
        return true;
      }
      break;
    }

    // Unmark
    case IKEY_u:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        rstate->mark_mode = 0;
        return true;
      }
      break;
    }

    // Hide
    case IKEY_h:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        robo_ed_replace_dialog(rstate);
        return true;
      }
      else

      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        rstate->scr_hide_mode = !rstate->scr_hide_mode;
        return true;
      }
      break;
    }

    case IKEY_f:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        robo_ed_search_dialog(rstate);
        return true;
      }
      break;
    }

    case IKEY_r:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        robo_ed_search_action(rstate, last_search_action);
        return true;
      }
      break;
    }

    case IKEY_m:
    {
      if(get_alt_status(keycode_internal) &&
       !get_ctrl_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        char macro_line[256];
        macro_line[0] = 0;

        if(!input_window(mzx_world, "Configure macro:", macro_line, 29))
        {
          const struct ext_macro *macro_src;
          int next;

          macro_src = find_macro(editor_conf, macro_line, &next);

          if(macro_src)
            execute_macro(rstate, macro_src);
        }
        return true;
      }
      break;
    }

    case IKEY_y:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // Update the current line here to avoid repeat macro expansion bugs.
        // Unfortunately, this means macro expansion here will clobber the
        // redo stack.
        update_current_line(rstate, true);
        end_intake_undo_frame(rstate);
        apply_redo(rstate->h);
      }
      break;
    }

    case IKEY_z:
    {
      if(get_ctrl_status(keycode_internal) &&
       !get_alt_status(keycode_internal) && !get_shift_status(keycode_internal))
      {
        // See above. If the current frame is a buffer-based modification,
        // it will not trigger macro expansion and should be safe.
        if(robot_editor_undo_frame_type(rstate->h) != TX_SAME_BUFFER)
          update_current_line(rstate, true);
        end_intake_undo_frame(rstate);
        apply_undo(rstate->h);
      }
      break;
    }
  }

  if(exit_status)
  {
    update_current_line(rstate, true);

#ifdef CONFIG_DEBYTECODE
    if(rstate->confirm_changes)
    {
      char confirm_prompt[80] = "Program modified. Save changes?";
      int confirm_changes_res = ask_yes_no(mzx_world, confirm_prompt);

      if(confirm_changes_res < 0)
        exit_status = false;

      if(!confirm_changes_res)
      {
        struct robot *cur_robot = rstate->cur_robot;
        cur_robot->program_source = package_program(rstate->base.next,
         NULL, &(cur_robot->program_source_length), cur_robot->program_source);
      }
    }
#endif
    if(validate_lines(rstate, 0))
      exit_status = false;

    if(exit_status)
      destroy_context(ctx);

    return true;
  }

  return false;
}

static void robot_editor_destroy(context *ctx)
{
  struct robot_editor_context *rstate = (struct robot_editor_context *)ctx;

#ifndef CONFIG_DEBYTECODE
  // Package time
  reallocate_robot(rstate->cur_robot, rstate->size);
#endif

  delete_robot_lines(rstate->cur_robot, rstate);
  destruct_undo_history(rstate->h);

  restore_screen();
  cursor_off();
}

void robot_editor(context *parent, struct robot *cur_robot)
{
  struct robot_editor_context *rstate =
   ccalloc(1, sizeof(struct robot_editor_context));
  struct context_spec spec;

  const struct editor_config_info *editor_conf = get_editor_config();

  rstate->cur_robot = cur_robot;
  rstate->current_line = 0;
  rstate->current_rline = &(rstate->base);
  rstate->total_lines = 0;
  rstate->max_size = MAX_OBJ_SIZE;
  rstate->mark_mode = 0;
  rstate->mark_start = -1;
  rstate->mark_end = -1;
  rstate->mark_start_rline = NULL;
  rstate->mark_end_rline = NULL;
  rstate->current_x = 0;
  rstate->command_buffer = rstate->command_buffer_space;
  rstate->scr_hide_mode = editor_conf->robot_editor_hide_help;

#ifdef CONFIG_DEBYTECODE
  rstate->size = 0;
  rstate->program_modified = false;
  rstate->confirm_changes = false;
#else
  rstate->size = 2;
  rstate->default_invalid =
   (enum validity_types)(editor_conf->default_invalid_status);
#endif

  init_robot_lines(rstate, cur_robot);
  strcpy(rstate->command_buffer, rstate->current_rline->line_text);

  rstate->h = construct_robot_editor_undo_history(editor_conf->undo_history_size);

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw           = robot_editor_draw;
  spec.idle           = robot_editor_idle;
  spec.click          = robot_editor_mouse;
  spec.joystick       = robot_editor_joystick;
  spec.key            = robot_editor_key;
  spec.destroy        = robot_editor_destroy;
  create_context((context *)rstate, parent, &spec, CTX_ROBO_ED);

  rstate->intk =
   intake2((context *)rstate, rstate->command_buffer, MAX_COMMAND_LEN,
   &(rstate->current_x), &(rstate->current_line_len));
  intake_event_callback(rstate->intk, rstate, robot_editor_intake_callback);

  caption_set_robot(parent->world, cur_robot);
  save_screen();
}

void init_macros(void)
{
  const struct editor_config_info *editor_conf = get_editor_config();
  memcpy(macros, editor_conf->default_macros, 5 * 64);
}

/**
 * Exposed internal functions for undo implementation.
 */

void robo_ed_goto_line(struct robot_editor_context *rstate, int line, int column)
{
  goto_line(rstate, line, column);
}

void robo_ed_delete_current_line(struct robot_editor_context *rstate, int move)
{
  delete_current_line(rstate, move);
}

void robo_ed_add_line(struct robot_editor_context *rstate, char *value, int relation)
{
  add_line(rstate, value, relation);
  // If relation > 0 then the current line is set to the new line, so update
  // the command buffer as well.
  if(relation > 0)
  {
    snprintf(rstate->command_buffer, COMMAND_BUFFER_LEN, "%s", value);
    rstate->command_buffer[MAX_COMMAND_LEN] = '\0';
  }
}
