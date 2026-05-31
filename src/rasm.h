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

#ifndef MEGAZEUX_RASM_H
#define MEGAZEUX_RASM_H

#include "compat.h"
#include "legacy_rasm.h"

#ifdef CONFIG_DEBYTECODE

#include "data.h"

#include <limits.h>

// I really didn't want to have all this stuff in the header file, but exposing
// tokens to the outside world dragged it all in.


// The argument type both specifies what it is lexically and what it is
// functionally. The functionality is a finer grained specification than
// the lexical type. For instance, any "name" token will match any of the
// arg types ending in "NAME", but the type of name could affect what kind
// of further compile time checking is done (can that name be available,
// for instance) and it can affect what it is compiled to.

// A command's parameters can accept any one of the following parameter
// types, but in practice it'll use a few common groupings (basically,
// anything that will resolve to the same type, IE immediate and
// counter_load_name). The exceptions to this are for "ignore" and
// "fragment", which use the lower bits to determine a specific type
// that has to be matched.

// These are listed in order of priority. What that means is that if a
// token can ambiguously be multiple kinds of arguments then it will
// match in the order here from top to bottom. In practice I don't
// think that much ambiguity will be possible, but for instance something
// like "c00" will always be a color before it is a counter load name.

enum arg_type
{
  ARG_TYPE_IMMEDIATE          = (1 << 0),
  ARG_TYPE_STRING             = (1 << 1),
  ARG_TYPE_CHARACTER          = (1 << 2),
  ARG_TYPE_COLOR              = (1 << 3),
  ARG_TYPE_PARAM              = (1 << 4),
  ARG_TYPE_COUNTER_LOAD_NAME  = (1 << 5),
  ARG_TYPE_COUNTER_STORE_NAME = (1 << 6),
  ARG_TYPE_LABEL_NAME         = (1 << 7),
  ARG_TYPE_EXT_LABEL_NAME     = (1 << 8),
  ARG_TYPE_BOARD_NAME         = (1 << 9),
  ARG_TYPE_ROBOT_NAME         = (1 << 10),
  ARG_TYPE_DIRECTION          = (1 << 11),
  ARG_TYPE_THING              = (1 << 12),
  ARG_TYPE_EQUALITY           = (1 << 13),
  ARG_TYPE_CONDITION          = (1 << 14),
  ARG_TYPE_ITEM               = (1 << 15),
  ARG_TYPE_UNKNOWN            = (1 << 16),

  ARG_TYPE_IGNORE             = (1 << 30),

  /**
   * Hi, I'm GCC. Having (1 << 31) would be too easy, so I'll complain about
   * that not being a constant integer expression. Lower the shift values so
   * there aren't negative numbers? Guess what, now it's an unsigned enum!
   * Think you can still get an unsigned enum with (1u << 31)? Nope, enum is
   * restricted to the range of int and I'm going to complain about that too.
   * This is what you have to do to get this value to work:
   */
  ARG_TYPE_FRAGMENT           = (int)(1u << 31)
};

enum arg_type_indexed
{
  ARG_TYPE_INDEXED_IMMEDIATE,
  ARG_TYPE_INDEXED_STRING,
  ARG_TYPE_INDEXED_CHARACTER,
  ARG_TYPE_INDEXED_COLOR,
  ARG_TYPE_INDEXED_PARAM,
  ARG_TYPE_INDEXED_NAME,
  ARG_TYPE_INDEXED_DIRECTION,
  ARG_TYPE_INDEXED_THING,
  ARG_TYPE_INDEXED_EQUALITY,
  ARG_TYPE_INDEXED_CONDITION,
  ARG_TYPE_INDEXED_ITEM,
  ARG_TYPE_INDEXED_IGNORE,
  ARG_TYPE_INDEXED_FRAGMENT,
  ARG_TYPE_INDEXED_COMMAND,
  ARG_TYPE_INDEXED_EXPRESSION,
  ARG_TYPE_INDEXED_LABEL,
  ARG_TYPE_INDEXED_COMMENT,
};

enum token_type
{
  TOKEN_TYPE_NUMERIC_LITERAL_BASE10,
  TOKEN_TYPE_NUMERIC_LITERAL_BASE16,
  TOKEN_TYPE_STRING_LITERAL,
  TOKEN_TYPE_CHARACTER_LITERAL,
  TOKEN_TYPE_NAME,
  TOKEN_TYPE_EXPRESSION,
  TOKEN_TYPE_EQUALITY,
  TOKEN_TYPE_BASIC_IDENTIFIER,
  TOKEN_TYPE_COMMENT,
  TOKEN_TYPE_BASIC_COLOR,
  TOKEN_TYPE_BASIC_PARAM,
  TOKEN_TYPE_WILDCARD_COLOR,
  TOKEN_TYPE_WILDCARD_PARAM,
  TOKEN_TYPE_INVALID              = 0x4000000,
  TOKEN_TYPE_CONTAINS_EXPRESSIONS = 0x8000000,
};

enum token_basic_string_type
{
  TOKEN_BASIC_STRING_DIRECTION,
  TOKEN_BASIC_STRING_THING,
  TOKEN_BASIC_STRING_CONDITION,
  TOKEN_BASIC_STRING_ITEM,
  TOKEN_BASIC_STRING_IGNORE,
  TOKEN_BASIC_STRING_FRAGMENT
};

struct special_word
{
  const char *const name;
  int instance_type;
  enum arg_type arg_type;
};


// A token is exactly one of these.

// All tokens are separated by spaces. Tokens may contain spaces if they're
// delimitted by (double) quotes, backticks, or are expressions. If
// delimitted by a particular character but can contain expressions, then
// that character does not act as a delimitter if it shows up within the
// expressions themselves. If a token contains expressions then its type
// will have TOKEN_TYPE_CONTAINS_EXPRESSIONS logically ORed into it.

// Numeric literal base10: a base 10 number.
//  Can range from -2147483648 to 2147483647.
//  Eg: 123456789

// Numeric literal base16: a base 16 number starting with 0x (not $ like
//  the old style).
//  Can range from 0x00000000 to 0xFFFFFFFF.
//  Eg: 0xDEADF00D

// String literal: a character string enclosed in double quote characters.
//  Refers to text that is processed directly as such in-game.
//  May contain expressions.
//  Eg: "Hello world!"

// Character literal: a single character enclosed in single quotes.
//  Eg: 'q'

// Name: a character string enclosed in backticks. May contain expressions.
//  Refers to some named entity like a counter, robot, board, label, etc.
//  Eg: `x marks the counter`

// Equality symbol: symbol used for comparison
//  One of: <, >, =, ==, <>, ><, and !=

// Expression: A mathematical expression separated by parentheses. The
//  grammar for expressions is as follows (\| is literal | character)
//  expr           := (expr_terms)
//  expr_terms     := expr_value | expr_value expr_binary_op expr_terms |
//                    expr_unary_op expr_terms | expr
//  expr_value     := numeric_literal | name
//  expr_binary_op := + | - | * | / | % | ** | >> | >>> | << | > | < | >= |
//                    <= | = | != | & | \| | ^
//  expr_unary_op  := - | ~

// Where the following binary operations correspond to the following things:
// +    Addition
// -    Subtraction
// *    Multiplication
// /    Division
// %    Modulo
// **   Exponent (a ** b is a to the b order/power)
// >>   Logical bitshift right
// >>>  Arithmetic bitshift right
// <<   Bitshift left
// >    Greater than; evaluates to 1 if true, 0 if false
// <    Less than; evaluates to 1 if true, 0 if false
// >=   Greater than or equal to; evaluates to 1 if true, 0 if false
// <=   Less than or equal to; evaluates to 1 if true, 0 if false
// =    Equal to; evaluates to 1 if true, 0 if false
// !=   Not equal to; evaluates to 1 if true, 0 if false
// &    Bitwise AND (not logical AND)
// |    Bitwise OR (not logical OR)
// ^    Bitwise XOR (not logical XOR)

// And the following unary operations correspond to the follow things:
// -    Two's complement negation
// ~    One's complement negation (bitwise NOT)

// Basic identifier: A string not delimitted by a special character that
//  could be a name or could be part of a command, aliased argument, param,
//  color, etc.
//  Must not contain spaces. Note that this does not include anything that
//  falls under "ignored" strings.
//  Eg: this_counter
//  Eg: NORTH
//  Eg: p??

// Ignore: Connecting words that are thrown out by the tokenizer and are
//  only allowed to make code more expressive/readible.
//  Includes:
//  , (comma)
//  ; (semicolon)
//  a
//  an
//  and
//  as
//  at
//  by
//  else
//  for
//  from
//  into
//  is
//  of
//  the
//  then
//  there
//  through
//  thru
//  to
//  with

// Invalid means that it won't let it pass. This includes malformed expressions
// (anything that begins with an ( and goes wrong from there)

union token_value
{
  int command_number;
  int numeric_literal;
  char char_literal;
  enum equality equality_type;
  const struct special_word *special_word;
};

struct token
{
  enum token_type type;
  union token_value arg_value;
  enum arg_type_indexed arg_type_indexed;
  boolean value_is_cached;
  char *value;
  int length;
};


MEGAZEUX_BEGIN_DECLS

char *legacy_disassemble_program(char *program_bytecode, int bytecode_length,
 int *_disasm_length, boolean print_ignores, int base);
char *legacy_convert_file(char *file_name, int *_disasm_length,
 boolean print_ignores, int base);
char *legacy_convert_program(char *src, int len, int *_disasm_length,
 boolean print_ignores, int base);

char *find_non_identifier_char(char *str);

CORE_LIBSPEC void assemble_program(char *program_source, char **_bytecode,
 int *_bytecode_length, struct command_mapping **_command_map,
 int *_command_map_length);

#ifdef CONFIG_EDITOR

CORE_LIBSPEC int legacy_disassemble_command(char *command_base, char *output_base,
 int *line_length, int bytecode_length, boolean print_ignores, int base);

CORE_LIBSPEC struct token *parse_command(char *src, char **_next,
 int *num_parse_tokens);
CORE_LIBSPEC void print_color(int color, char *color_buffer);
CORE_LIBSPEC int unescape_char(char *dest, char c);
CORE_LIBSPEC int get_thing(char *name, int name_length);

#endif // CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif // CONFIG_DEBYTECODE

#endif /* MEGAZEUX_RASM_H */
