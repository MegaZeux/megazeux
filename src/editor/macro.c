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

// Extended macro support

#include "configure.h"
#include "macro.h"
#include "macro_struct.h"
#include "../rasm.h"
#include "../algorithm/string_compare.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int cmp_variables(const void *dest, const void *src)
{
  struct macro_variable *m_src = *((struct macro_variable **)src);
  struct macro_variable *m_dest = *((struct macro_variable **)dest);

  return strcasecmp(m_dest->name, m_src->name);
}

void free_macro(struct ext_macro *macro_src)
{
  int i, i2;

  free(macro_src->name);
  free(macro_src->label);

  for(i = 0; i < macro_src->num_lines; i++)
  {
    free(macro_src->lines[i]);
    free(macro_src->variable_references[i]);
  }

  free(macro_src->lines);
  free(macro_src->variable_references);

  free(macro_src->line_element_count);

  for(i = 0; i < macro_src->num_types; i++)
  {
    if(macro_src->types[i].type == string)
    {
      for(i2 = 0; i2 < macro_src->types[i].num_variables; i2++)
      {
        free(macro_src->types[i].variables[i2].storage.str_storage);

        if(macro_src->types[i].variables[i2].def.str_storage)
          free(macro_src->types[i].variables[i2].def.str_storage);
      }
    }

    free(macro_src->types[i].variables_sorted);
    free(macro_src->types[i].variables);
  }

  free(macro_src->text);
  free(macro_src);
}

#ifdef CONFIG_DEBYTECODE
static
#endif
char *skip_to_next(char *src, char t, char a, char b)
{
  char *current = src;
  char current_char = *current;

  while(current_char && (current_char != t) && (current_char != '\n') &&
   (current_char != a) && (current_char != b))
  {
    if(current_char == '\r')
      *current = 0;
    current++;
    current_char = *current;
  }

  return current;
}

#ifdef CONFIG_DEBYTECODE
static
#endif
char *skip_whitespace(char *src)
{
  char *current = src;
  char current_char = *current;

  while(isspace((int)current_char))
  {
    current++;
    current_char = *current;
  }

  return current;
}

#ifdef CONFIG_DEBYTECODE
static
#endif
union variable_storage *find_macro_variable(const char *name,
 const struct macro_type *m)
{
  int bottom = 0, top = m->num_variables - 1, middle = 0;
  int cmpval = 0;
  struct macro_variable **base = m->variables_sorted;
  struct macro_variable *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return &(current->storage);
  }

  return NULL;
}

static struct ext_macro *process_macro(const char *line_data,
 const char *name, const char *label)
{
  char *line_position, *line_position_old;
  struct macro_variable *variables;
  char ***text_lines;
  char **line_text_segments;
  struct macro_variable_reference *line_variable_references;
  union variable_storage *current_storage;
  struct macro_variable_reference **variable_references;
  int *line_variables_count;
  int num_variables;
  int num_types = 0;
  int num_line_variables;
  int num_lines = 0;
  int num_lines_allocated = 32;
  int total_variables = 0;
  struct macro_type *current_type;
  char current_char;
  struct ext_macro *macro_dest;
  int i;
  int def_val;

  line_text_segments = ccalloc(256, sizeof(char *));
  variables = ccalloc(256, sizeof(struct macro_variable));
  line_variable_references = ccalloc(256,
   sizeof(struct macro_variable_reference));

  text_lines = ccalloc(num_lines_allocated, sizeof(char **));
  variable_references =
   ccalloc(num_lines_allocated, sizeof(struct macro_variable_reference *));
  line_variables_count = ccalloc(num_lines_allocated, sizeof(int));

  macro_dest = cmalloc(sizeof(struct ext_macro));
  macro_dest->name = cmalloc(strlen(name) + 1);
  strcpy(macro_dest->name, name);

  macro_dest->label = cmalloc(strlen(label) + 1);
  strcpy(macro_dest->label, label);

  current_type = macro_dest->types;

  macro_dest->text = cmalloc(strlen(line_data) + 1);
  strcpy(macro_dest->text, line_data);
  line_position = macro_dest->text;

  do
  {
    if(*line_position == '(')
    {
      // Line is for variables
      line_position++;

      // Determine type
      if(!strncmp(line_position, "number", 6))
      {
        // Number
        current_type->type = number;
        current_type->type_attributes[1] =
         strtol(line_position + 6, &line_position, 10);

        if(*line_position == '-')
        {
          current_type->type_attributes[0] =
           current_type->type_attributes[1];
          current_type->type_attributes[1] =
           strtol(line_position + 1, &line_position, 10);
        }
        else
        {
          current_type->type_attributes[0] = 0;
        }
      }

      if(!strncmp(line_position, "string", 6))
      {
        // Number
        current_type->type = string;
        current_type->type_attributes[0] =
         strtol(line_position + 6, &line_position, 10);
      }

      if(!strncmp(line_position, "char", 4))
      {
        // Character
        current_type->type = character;
        line_position += 4;
      }

      if(!strncmp(line_position, "color", 5))
      {
        // Character
        current_type->type = color;
        line_position += 5;
      }

      num_variables = 0;

      // Parse names
      while(1)
      {
        line_position = skip_whitespace(line_position);
        line_position_old = line_position;
        line_position = skip_to_next(line_position, ',', ')', '=');
        variables[num_variables].name = line_position_old;

        if(*line_position == '=')
        {
          // Initialize default value
          def_val = 1;
          *line_position = 0;
          line_position++;

          switch(current_type->type)
          {
            case number:
            {
              variables[num_variables].def.int_storage =
               strtol(line_position, &line_position, 10);
              break;
            }

            case string:
            {
              line_position_old = line_position;
              break;
            }

            case character:
            {
              if(*line_position == '\'')
              {
                variables[num_variables].def.int_storage =
                 *(line_position + 1);
                line_position += 2;
              }
              else
              {
                variables[num_variables].def.int_storage =
                 (char)strtol(line_position, &line_position, 10);
              }

              break;
            }

            case color:
            {
              variables[num_variables].def.int_storage =
               get_color(line_position);
              line_position += 3;
              break;
            }

            default:
            {
              break;
            }
          }

          line_position = skip_to_next(line_position, ',', ')', 0);
        }
        else
        {
          switch(current_type->type)
          {
            case number:
            {
              variables[num_variables].def.int_storage =
               current_type->type_attributes[0];
              break;
            }

            case character:
            {
              variables[num_variables].def.int_storage = 0;
              break;
            }

            case color:
            {
              variables[num_variables].def.int_storage = 288;
              break;
            }

            default:
            {
              break;
            }
          }
          def_val = 0;
        }

        current_char = *line_position;
         *line_position = 0;

        if(current_type->type == string)
        {
          variables[num_variables].storage.str_storage =
           cmalloc(current_type->type_attributes[0] + 1);

          if(def_val)
          {
            variables[num_variables].def.str_storage =
             cmalloc(current_type->type_attributes[0] + 1);
            memcpy(variables[num_variables].def.str_storage,
             line_position_old, current_type->type_attributes[0]);
            variables[num_variables].def.
             str_storage[current_type->type_attributes[0]] = 0;
            memcpy(variables[num_variables].storage.str_storage,
             variables[num_variables].def.str_storage,
             current_type->type_attributes[0] + 1);
          }
          else
          {
            variables[num_variables].def.str_storage = NULL;
            variables[num_variables].storage.str_storage[0] = 0;
          }
        }
        else
        {
          memcpy(&(variables[num_variables].storage),
           &(variables[num_variables].def), sizeof(union variable_storage));
        }

        num_variables++;

        if(current_char == ')')
        {
          line_position++;
          break;
        }
        else

        if(current_char)
        {
          line_position++;
        }
        else
        {
          break;
        }
      }

      current_type->num_variables = num_variables;
      current_type->variables =
       ccalloc(num_variables, sizeof(struct macro_variable));
      current_type->variables_sorted =
       ccalloc(num_variables, sizeof(struct macro_variable *));
      memcpy(current_type->variables, variables,
       sizeof(struct macro_variable) * num_variables);

      for(i = 0; i < num_variables; i++)
      {
        current_type->variables_sorted[i] = current_type->variables + i;
      }

      total_variables += num_variables;

      qsort(current_type->variables_sorted,
       num_variables, sizeof(struct macro_variable *), cmp_variables);

      current_type++;
      num_types++;

      line_position = skip_whitespace(line_position);
    }
    else
    {
      current_char = *line_position;

      while((current_char == '\n') || (current_char == '\r'))
      {
        line_position++;
        current_char = *line_position;
      }

      num_line_variables = 0;
      line_text_segments[0] = line_position;

      while(current_char && (current_char != '\n'))
      {
        current_char = *line_position;
        while(current_char && (current_char != '\n'))
        {
          line_position = skip_to_next(line_position, '!', 0, 0);
          current_char = *line_position;

          if(current_char == '!')
          {
            // Embedded variable, close off
            *line_position = 0;
            line_position_old = line_position + 1;

            if(*line_position_old == '#')
            {
              line_position_old++;
              line_variable_references[num_line_variables].ref_mode =
               hexidecimal;
            }
            else
            {
              line_variable_references[num_line_variables].ref_mode =
               decimal;
            }

            // Get name
            line_position = skip_to_next(line_position_old, '!', 0, 0);
            // Close off end
            *line_position = 0;
            line_position++;

            // Find macro element this corresponds to and link.
            for(i = 0; i < num_types; i++)
            {
              current_storage = find_macro_variable(line_position_old,
               macro_dest->types + i);
              if(current_storage)
              {
                line_variable_references[num_line_variables].storage =
                 current_storage;
                line_variable_references[num_line_variables].type =
                 macro_dest->types + i;
                break;
              }
            }

            if(i == num_types)
            {
              line_variable_references[num_line_variables].type =
               NULL;
            }

            line_text_segments[num_line_variables + 1] = line_position;
            num_line_variables++;
          }

          current_char = *line_position;
        }
        *line_position = 0;
        line_position++;
      }

      line_variables_count[num_lines] = num_line_variables;

      variable_references[num_lines] =
       ccalloc(num_line_variables, sizeof(struct macro_variable_reference));
      memcpy(variable_references[num_lines], line_variable_references,
       sizeof(struct macro_variable_reference) * num_line_variables);

      text_lines[num_lines] =
       ccalloc(num_line_variables + 1, sizeof(char *));
      memcpy(text_lines[num_lines], line_text_segments,
       sizeof(char *) * (num_line_variables + 1));

      num_lines++;

      if(num_lines == num_lines_allocated)
      {
        num_lines_allocated *= 2;

        text_lines = crealloc(text_lines,
         sizeof(char **) * num_lines_allocated);
        variable_references = crealloc(variable_references,
         sizeof(struct macro_variable_reference *) * num_lines_allocated);
        line_variables_count = crealloc(line_variables_count,
         sizeof(int) * num_lines_allocated);
      }
    }
  } while(*line_position);

  macro_dest->num_lines = num_lines;
  macro_dest->total_variables = total_variables;

  macro_dest->lines = ccalloc(num_lines, sizeof(char **));
  memcpy(macro_dest->lines, text_lines, sizeof(char **) * num_lines);

  macro_dest->variable_references =
   ccalloc(num_lines, sizeof(struct macro_variable_reference *));
  memcpy(macro_dest->variable_references, variable_references,
   sizeof(struct macro_variable_reference *) * num_lines);

  macro_dest->line_element_count = ccalloc(num_lines, sizeof(int));
  memcpy(macro_dest->line_element_count, line_variables_count,
   sizeof(int) * num_lines);

  macro_dest->num_types = num_types;

  free(line_variable_references);
  free(line_variables_count);
  free(line_text_segments);
  free(variable_references);
  free(variables);
  free(text_lines);

  return macro_dest;
}

static struct ext_macro *_find_macro(const struct editor_config_info *conf,
 const char *name, int *next)
{
  int bottom = 0, top = (conf->num_extended_macros) - 1, middle = 0;
  int cmpval = 0;
  struct ext_macro **base = conf->extended_macros;
  struct ext_macro *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return current;
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
}

const struct ext_macro *find_macro(const struct editor_config_info *conf,
 const char *name, int *next)
{
  return _find_macro(conf, name, next);
}

void add_ext_macro(struct editor_config_info *conf, const char *name,
 const char *line_data, const char *label)
{
  struct ext_macro *macro_dest;
  struct ext_macro **macro_list;
  int next = 0;

  if(!(conf->num_macros_allocated))
  {
    conf->extended_macros = cmalloc(sizeof(struct ext_macro *));
    conf->extended_macros[0] = process_macro(line_data, name, label);
    conf->num_extended_macros = 1;
    conf->num_macros_allocated = 1;
  }
  else
  {
    macro_dest = _find_macro(conf, name, &next);

    if(macro_dest)
    {
      free_macro(macro_dest);
      conf->extended_macros[next] =
       process_macro(line_data, name, label);
    }
    else
    {
      if(conf->num_extended_macros == conf->num_macros_allocated)
      {
        conf->num_macros_allocated *= 2;
        conf->extended_macros =
         crealloc(conf->extended_macros,
         sizeof(struct ext_macro *) * conf->num_macros_allocated);
      }

      macro_list = conf->extended_macros;

      if(next != conf->num_extended_macros)
      {
        macro_list += next;
        memmove((char *)(macro_list + 1),
         (char *)macro_list, (conf->num_extended_macros - next) *
         sizeof(struct ext_macro *));
      }

      conf->extended_macros[next] =
       process_macro(line_data, name, label);

      conf->num_extended_macros++;
    }
  }
}
