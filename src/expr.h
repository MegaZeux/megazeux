/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_EXPR_H
#define MEGAZEUX_EXPR_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include <limits.h>
#include <stddef.h>
#include "world_struct.h"

int parse_expression(struct world *mzx_world, char **expression, int *error,
 int id);

#ifdef CONFIG_DEBYTECODE
int parse_string_expression(struct world *mzx_world, char **_expression,
 int id, char *output, size_t output_left);
#endif

CORE_LIBSPEC char *tr_msg_ext(struct world *mzx_world, char *mesg, int id,
 char *buffer, char terminating_char);

static inline char *tr_msg(struct world *mzx_world, char *mesg, int id,
 char *buffer)
{
  return tr_msg_ext(mzx_world, mesg, id, buffer, 0);
}

/* Guard against division by 0 and dividing INT_MIN by -1. */
static inline int safe_divide_32(int a, int b)
{
  if(b == 0 || (a == INT_MIN && b == -1))
    return 0;

  return a / b;
}

/* Guard against division by 0 and dividing INT_MIN by -1. */
static inline int safe_modulo_32(int a, int b)
{
  if(b == 0 || (a == INT_MIN && b == -1))
    return 0;

  return a % b;
}

/* Guard against shift by values under 0 and over 31. */
static inline int safe_left_shift_32(int a, int b)
{
  return ((unsigned int)b < 32) ? a << b : 0;
}

/* Guard against shift by values under 0 and over 31. */
static inline int safe_logical_right_shift_32(int a, int b)
{
  return ((unsigned int)b < 32) ? (unsigned int)a >> b : 0;
}

/* Guard against shift by values under 0 and over 31. */
static inline int safe_arithmetic_right_shift_32(int a, int b)
{
  // Preserve sign for invalid exponents.
  return ((unsigned int)b < 32) ? (signed int)a >> b : (signed int)a >> 31;
}

static inline char *tr_int_to_string(char dest[12], int value, size_t *len)
{
  char *pos = dest + 11;

  *(pos--) = '\0';

  if(value >= 0)
  {
    while(value >= 10)
    {
      *(pos--) = '0' + value % 10;
      value /= 10;
    }
    *pos = '0' + value;
  }
  else
  {
    while(value <= -10)
    {
      *(pos--) = '0' - value % 10;
      value /= 10;
    }
    *(pos--) = '0' - value;
    *pos = '-';
  }

  *len = dest + 11 - pos;
  return pos;
}

static inline char *tr_int_to_hex_string(char dest[9], int value, size_t *len)
{
  static const char hex[] = "0123456789abcdef";
  char *pos = dest + 8;

  *(pos--) = '\0';

  if(value >= 0)
  {
    while(value >= 0x10)
    {
      *(pos--) = hex[value & 0x0F];
      value >>= 4;
    }
    *pos = hex[value & 0x0F];
  }
  else
  {
    while(dest < pos)
    {
      *(pos--) = hex[value & 0x0F];
      value >>= 4;
    }
    *pos = hex[value & 0x0F];
  }

  *len = dest + 8 - pos;
  return pos;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EXPR_H */
