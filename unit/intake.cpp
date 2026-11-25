/* MegaZeux
 *
 * Copyright (C) 2021-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Automated test for the parts of intake that are testable.
 * Currently, this is mainly intake_apply_event_fixed.
 */

#include "Unit.hpp"
#include "../src/intake.c"

// TODO shut up the linker as this isn't MZX_LIBCORE currently.
boolean has_unicode_input() { return false; }

struct int_pair
{
  int input;
  int expected;
};

struct event_input_data
{
  const char *base;
  int start_pos;
  int new_pos;
  const char *input;
  const char *expected;
};

struct event_repeat_data
{
  const char *base;
  int start_pos;
  int new_pos;
  int repeat_times;
  const char *expected;
};

struct event_partial_data
{
  enum intake_event_type type;
  const char *base;
  int old_pos;
  int new_pos;
  int value;
};

struct event_ext_data
{
  enum intake_event_type type;
  const char *base;
  int old_pos;
  int new_pos;
  int value;
  const char *input;
  const char *expected;
};

struct input_string_data
{
  const char *base;
  int start_pos;
  const char *input;
  const char *expected;
  ssize_t retval;
  int linebreak_char;
  boolean insert_on;
};

static const int_pair pos_data[] =
{
  { 0, 0 },
  { 50, 50 },
  { 100, 100 },
  { 1000000, 100 },
  { -1, 0 },
  { -12781831, 0 },
};

static const int_pair length_data[] =
{
  { 0, 0 },
  { 50, 50 },
  { 240, 240 },
  { 1000, 240 },
  { 1000000, 240 },
  { -1, 0 },
  { -4983412, 0 },
};

static const char skip_template[256] =
 "aaa bb`cc~dd!ee@ff#gg$hh%ii^jj&kk*ll(mm)"
 "nn-oo_pp=qq+rr[ss]tt{uu}vv\\ww|xx;yy:zz'01\""
 "23,45<67.>89/00??0";

static const int skip_forward_positions[] =
{
   0,  3,  6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39,
  42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81,
  84, 87, 90, 94, 97, 100
};

static const int skip_backward_positions[] =
{
  100, 99, 95, 92, 88, 85, 82,
   79, 76, 73, 70, 67, 64, 61, 58, 55, 52, 49, 46, 43, 40,
   37, 34, 31, 28, 25, 22, 19, 16, 13, 10,  7,  4,  0
};

static const char non_ascii_input_data[] =
  "sjdhfklsjdfdabc\n\r\n\t\v\bsdfjksdlkfjsdkfkd"
  "\xE6\x52\xEB\xF2\x6D\x4D\x4A\xB7\x87\xB2\x92\x88\xDE\x91\x24";

static const input_string_data input_data[] =
{
  { "put  Solid p00 0 0", 4, "c0f", "put c0f Solid p00 0 0", -1, -1, true },
  { "put c0f Solid  0 0", 14, "p00", "put c0f Solid p00 0 0", -1, -1, true },
  { "char edit '@'", 14, " 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
    "char edit '@' 0 0 0 0 0 0 0 0 0 0 0 0 0 0", -1, -1, true },
  { "char edit '@'", 4, " 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
    "char 0 0 0 0 0 0 0 0 0 0 0 0 0 0", -1, -1, false },
  { ".comment", 0, ": justentered^", ": justentered.comment", 14, '^', true },
  { "abc\ndef\n", 8, "ghi\njkl\n", "abc\ndef\nghi", 4, '\n', true },
  { "abc\ndef\n", 8, "ghi\njkl\n", "abc\ndef\nghi\njkl\n", -1, -1, true },
  { "abc\ndef\n", 0, "ghi\njkl\n", "ghi\ndef\n", 4, '\n', false },
  { "abc\ndef\n", 0, "ghi\njkl\n", "ghi\njkl\n", -1, -1, false },
  { "", 0, non_ascii_input_data, non_ascii_input_data, -1, -1, true },
  { "", 0, non_ascii_input_data, non_ascii_input_data, -1, -1, false },
  { "", 0, "test\0test", "test", -1, -1, true },
  { "", 0, "test\0test", "test", -1, -1, false },
  { "whatever", 4, "", "whatever", -1, -1, true },
  { "whatever", 4, "", "whatever", -1, -1, false },
};

/**
 * Test internal and external functions for manipulating the position and
 * current length of the intake editor. A large portion of this is making sure
 * the external position and length pointers are syncronized correctly.
 */
UNITTEST(PosLength)
{
  struct intake_subcontext intk{};
  char dest[256];
  int ext = 0;

  intk.current_length = 100;
  intk.max_length = 240;

  samesize(dest, skip_template);
  memcpy(dest, skip_template, arraysize(dest));

  SECTION(intake_set_pos_no_external)
  {
    for(const int_pair &d : pos_data)
    {
      intake_set_pos(&intk, d.input);
      ASSERTEQ(intk.pos, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  SECTION(intake_set_pos_external)
  {
    intk.pos_external = &ext;

    for(const int_pair &d : pos_data)
    {
      intake_set_pos(&intk, d.input);
      ASSERTEQ(intk.pos, d.expected, "%d -> %d", d.input, d.expected);
      ASSERTEQ(ext, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  SECTION(intake_set_length_no_external)
  {
    for(const int_pair &d : length_data)
    {
      intake_set_length(&intk, d.input);
      ASSERTEQ(intk.current_length, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  SECTION(intake_set_length_external)
  {
    intk.length_external = &ext;

    for(const int_pair &d : length_data)
    {
      intake_set_length(&intk, d.input);
      ASSERTEQ(intk.current_length, d.expected, "%d -> %d", d.input, d.expected);
      ASSERTEQ(ext, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  // intake_sync should not crash when the provided intake is null.
  SECTION(intake_sync_null)
  {
    intake_sync(nullptr);
  }

  /**
   * When no buffer/external pointers are present, intake_sync should not
   * modify the intake position or current length values.
   */
  SECTION(intake_sync_no_data_or_external)
  {
    for(const int_pair &d : pos_data)
    {
      intake_set_pos(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.pos, d.expected, "%d -> %d", d.input, d.expected);
    }

    for(const int_pair &d : length_data)
    {
      intake_set_length(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.current_length, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  /**
   * When an external position pointer is present, intake_sync should update
   * the position to reflect the provided external position, bounded to the
   * current length.
   */
  SECTION(intake_sync_pos)
  {
    intk.pos_external = &ext;

    for(const int_pair &d : pos_data)
    {
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.pos, d.expected, "%d -> %d", d.input, d.expected);
      ASSERTEQ(ext, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  /**
   * When a buffer is present, intake_sync should update the current length to
   * the length of the buffer string.
   */
  SECTION(intake_sync_no_external)
  {
    int dest_len = strlen(dest);
    intk.current_length = 100;
    intk.dest = dest;
    intk.pos = 97;

    for(const int_pair &d : length_data)
    {
      dest_len = Unit::clamp(dest_len, 0, d.input);
      intake_set_length(&intk, d.input);
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.current_length, dest_len, "%d -> %d", d.input, dest_len);
      ASSERTEQ(intk.pos, 97, "%d -> %d", d.input, dest_len);
    }
  }

  /**
   * When external pointers are present but there is no buffer, intake_sync
   * should update the current length to the external length.
   */
  SECTION(intake_sync_no_dest)
  {
    intk.length_external = &ext;
    intk.dest = nullptr;

    for(const int_pair &d : length_data)
    {
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.current_length, d.expected, "%d -> %d", d.input, d.expected);
      ASSERTEQ(ext, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  /**
   * When both dest and external length are present, dest should be preferred
   * for updating the length.
   */
  SECTION(intake_sync_dest_and_external)
  {
    int dest_len = strlen(dest);
    intk.length_external = &ext;
    intk.dest = dest;

    for(const int_pair &d : length_data)
    {
      ext = d.input;
      intake_sync((subcontext *)&intk);
      ASSERTEQ(intk.current_length, dest_len, "%d -> %d", d.input, dest_len);
      ASSERTEQ(ext, dest_len, "%d -> %d", d.input, dest_len);
    }
  }
}

/**
 * Test the default fixed-size buffer intake editing operations.
 */
UNITTEST(EventFixed)
{
  static const event_input_data insert_data[] =
  {
    { "test string :)", 0,  9,  "Inserted ",                "Inserted test string :)" },
    { "abcdevwxyz",     5,  29, " put this in the middle ", "abcde put this in the middle vwxyz" },
    { "test string :)", 15, 27, " append this.",            "test string :) append this." },
    { "",               0,  12, "blank string",             "blank string" },
    { "whatever",       0,  0,  "",                         "whatever" },
    { "whatever",       4,  4,  "",                         "whatever" },
    { "whatever",       8,  8,  "",                         "whatever" },
  };
  static const event_input_data overwrite_data[] =
  {
    { "test string :)", 0,  11, "overwritten",              "overwritten :)" },
    { "abcdevwxyz",     5,  26, " put this at the end",     "abcde put this at the end" },
    { "test string :)", 15, 28, " append this.",            "test string :) append this." },
    { "",               0,  12, "blank string",             "blank string" },
    { "whatever",       0,  0,  "",                         "whatever" },
    { "whatever",       4,  0,  "",                         "whatever" },
    { "whatever",       8,  0,  "",                         "whatever" },
  };

  struct intake_subcontext intk{};
  subcontext *sub = reinterpret_cast<subcontext *>(&intk);
  char dest[256];
  boolean result;

  intk.dest = dest;
  intk.max_length = 240;
  intk.current_length = 100;
  intk.pos = 0;

  samesize(dest, skip_template);
  memcpy(dest, skip_template, arraysize(dest));

  SECTION(intake_skip_back)
  {
    int ext = 100;
    intk.pos = 100;
    intk.pos_external = &ext;

    for(int i : skip_backward_positions)
    {
      ASSERTEQ(intk.pos, i, "%d", i);
      ASSERTEQ(ext, i, "%d", i);
      intake_skip_back(&intk);
    }
  }

  SECTION(intake_skip_forward)
  {
    int ext = 0;
    intk.pos_external = &ext;

    for(int i : skip_forward_positions)
    {
      ASSERTEQ(intk.pos, i, "%d", i);
      ASSERTEQ(ext, i, "%d", i);
      intake_skip_forward(&intk);
    }
  }

  SECTION(intake_apply_event_fixed_errs)
  {
    // Should not crash with a null intake.
    result = intake_apply_event_fixed(nullptr, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(result, false, "");

    // Should not crash with a null dest.
    intk.dest = nullptr;
    result = intake_apply_event_fixed(sub, INTK_MOVE, 0, 0, nullptr);
    ASSERTEQ(result, false, "");
    intk.dest = dest;

    // Should not crash if intk->pos is somehow invalid.
    int invalid_pos[] = { -1, -12431, intk.current_length + 1, 100000 };
    for(int i : invalid_pos)
    {
      intk.pos = i;
      result = intake_apply_event_fixed(sub, INTK_MOVE, 0, 0, nullptr);
      ASSERTEQ(result, false, "");
    }
  }

  SECTION(INTK_NO_EVENT)
  {
    // Nothing should ever actually send this event (if it does, it's a bug).
    result = intake_apply_event_fixed(sub, INTK_NO_EVENT, 0, 0, nullptr);
    ASSERTEQ(result, false, "");
  }

  SECTION(INTK_MOVE)
  {
    // Applying this moves the cursor, that's it.
    for(const int_pair &d : pos_data)
    {
      intk.pos = 25;
      result = intake_apply_event_fixed(sub, INTK_MOVE, d.input, 0, nullptr);
      ASSERTEQ(result, true, "%d -> %d", d.input, d.expected);
      ASSERTEQ(intk.pos, d.expected, "%d -> %d", d.input, d.expected);
    }
  }

  SECTION(INTK_MOVE_WORDS)
  {
    // This just wraps the skip forward/back functions.
    int ext = 0;
    intk.pos_external = &ext;

    for(int i : skip_forward_positions)
    {
      ASSERTEQ(intk.pos, i, "%d (forward)", i);
      ASSERTEQ(ext, i, "%d (forward)", i);
      result = intake_apply_event_fixed(sub, INTK_MOVE_WORDS, intk.pos, 1, nullptr);
      ASSERTEQ(result, true, "%d (forward)", i);
    }

    ext = 100;
    intk.pos = 100;
    for(int i : skip_backward_positions)
    {
      ASSERTEQ(intk.pos, i, "%d (backward)", i);
      ASSERTEQ(ext, i, "%d (backward)", i);
      result = intake_apply_event_fixed(sub, INTK_MOVE_WORDS, intk.pos, -1, nullptr);
      ASSERTEQ(result, true, "%d (backward)", i);
    }
  }

  SECTION(INTK_INSERT)
  {
    // Insert text into the buffer.
    for(const event_input_data &d : insert_data)
    {
      int expected_len = strlen(d.expected);
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(size_t i = 0, len = strlen(d.input); i < len; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, d.input[i], nullptr);
        ASSERTEQ(result, true, "%s", d.expected);
      }
      ASSERTCMP(dest, d.expected, "");
      ASSERTEQ(intk.current_length, expected_len, "%s", d.expected);
      ASSERTEQ(intk.pos, d.new_pos, "%s", d.expected);
    }

    // Special: prevent inserting beyond the maximum length.
    intk.max_length = 8;
    intk.current_length = 7;
    snprintf(dest, sizeof(dest), "abcdefg");

    intk.pos = 7;
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'h', nullptr);
    ASSERTEQ(result, true, "");
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false, "");
    ASSERTEQ(intk.pos, 8, "");
    intk.pos = 0;
    result = intake_apply_event_fixed(sub, INTK_INSERT, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false, "");
    ASSERTEQ(intk.pos, 0, "");
  }

  SECTION(INTK_OVERWRITE)
  {
    // Overwrite text in the buffer. Can insert at the end of the line.
    for(const event_input_data &d : overwrite_data)
    {
      int expected_len = strlen(d.expected);
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(size_t i = 0, len = strlen(d.input); i < len; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, d.input[i], nullptr);
        ASSERTEQ(result, true, "%s", d.expected);
      }
      ASSERTCMP(dest, d.expected, "");
      ASSERTEQ(intk.current_length, expected_len, "");
    }

    // Special: prevent inserting beyond the maximum length.
    intk.max_length = 8;
    intk.current_length = 7;
    snprintf(dest, sizeof(dest), "abcdefg");

    intk.pos = 7;
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'h', nullptr);
    ASSERTEQ(result, true, "");
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, false, "");
    ASSERTEQ(intk.pos, 8, "");
    // This should work, though.
    intk.pos = 0;
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE, intk.pos + 1, 'i', nullptr);
    ASSERTEQ(result, true, "");
    ASSERTCMP(dest, "ibcdefgh", "");
  }

  SECTION(INTK_DELETE)
  {
    // Delete the char at the cursor.
    static const event_repeat_data data[] =
    {
      { "testing string :)",  8,  8,  7,  "testing :)" },
      { "abcdef",             0,  0,  6,  "" },
      { "abcdef",             1,  1,  4,  "af" },
      { "",                   0,  0,  10, "" },
      { "some stuff",         10, 10, 10, "some stuff" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(int i = 0; i < d.repeat_times; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_DELETE, intk.pos, 0, nullptr);
        ASSERTEQ(result, true, "%s", d.expected);
      }
      ASSERTCMP(dest, d.expected, "");
      ASSERTEQ(intk.current_length, expected_len, "");
    }
  }

  SECTION(INTK_BACKSPACE)
  {
    // Delete the char before the cursor.
    static const event_repeat_data data[] =
    {
      { "testing string :)",  14, 7, 7,  "testing :)" },
      { "abcdef",             6,  0, 6,  "" },
      { "abcdef",             5,  1, 4,  "af" },
      { "",                   0,  0, 10, "" },
      { "some stuff",         0,  0, 10, "some stuff" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      for(int i = 0; i < d.repeat_times; i++)
      {
        result = intake_apply_event_fixed(sub, INTK_BACKSPACE, intk.pos - 1, 0, nullptr);
        ASSERTEQ(result, true, "%s", d.expected);
      }
      ASSERTCMP(dest, d.expected, "");
      ASSERTEQ(intk.current_length, expected_len, "%s", d.expected);
      ASSERTEQ(intk.pos, d.new_pos, "%s", d.expected);
    }
  }

  SECTION(INTK_BACKSPACE_WORDS)
  {
    // Delete an entire word before the cursor.
    // NOTE: old intake() deletes an extra non-alphanumeric char before the
    // word, which seems like a bug and has not been replicated here.
    static const event_repeat_data data[] =
    {
      { "testing string :)",  17, 8, 1,  "testing " },
      { "testing string :)",  17, 0, 2,  "" },
      { "testing string :)",  15, 0, 2,  ":)" },
      { "testing{string}",    14, 8, 1,  "testing{}" },
      { "abc def ghi",        9,  4, 2,  "abc hi" },
      { "abcdef",             6,  0, 1,  "" },
      { "abcdef",             3,  0, 1,  "def" },
      { "",                   0,  0, 2,  "" },
      { "some stuff",         0,  0, 2,  "some stuff" },
      { "whatever",           8,  8, 0,  "whatever" },
      { "whatever",           4,  4, -1, "whatever" },
    };

    for(const event_repeat_data &d : data)
    {
      int expected_len = strlen(d.expected);
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      result = intake_apply_event_fixed(sub, INTK_BACKSPACE_WORDS, intk.pos, d.repeat_times, nullptr);
      ASSERTEQ(result, true, "%s", d.expected);
      ASSERTCMP(dest, d.expected, "");
      ASSERTEQ(intk.current_length, expected_len, "%s", d.expected);
      ASSERTEQ(intk.pos, d.new_pos, "%s", d.expected);
    }
  }

  SECTION(INTK_CLEAR)
  {
    static const event_repeat_data data[] =
    {
      { "testing string :)",  17, 0, 1, nullptr },
      { "testing string :)",  10, 0, 1, nullptr },
      { "testing string :)",  0,  0, 1, nullptr },
      { "abc def ghi",        9,  0, 1, nullptr },
      { "abcdef",             6,  0, 1, nullptr },
      { "abcdef",             3,  0, 1, nullptr },
      { "",                   0,  0, 1, nullptr },
    };

    for(const event_repeat_data &d : data)
    {
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      result = intake_apply_event_fixed(sub, INTK_CLEAR, 0, 0, nullptr);
      ASSERTEQ(result, true, "%s @ %d", d.base, d.start_pos);
      ASSERTCMP(dest, "", "%s @ %d", d.base, d.start_pos);
      ASSERTEQ(intk.current_length, 0, "%s @ %d", d.base, d.start_pos);
      ASSERTEQ(intk.pos, d.new_pos, "%s @ %d", d.base, d.start_pos);
    }
  }

  SECTION(INTK_INSERT_BLOCK)
  {
    for(const event_input_data &d : insert_data)
    {
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      int len = strlen(d.input);
      result = intake_apply_event_fixed(sub, INTK_INSERT_BLOCK, intk.pos + len, len, d.input);
      ASSERTEQ(result, true, "%s", d.expected);
      ASSERTCMP(dest, d.expected, "");
    }
    // Reject null data...
    result = intake_apply_event_fixed(sub, INTK_INSERT_BLOCK, intk.pos, 0, nullptr);
    ASSERTEQ(result, false, "");
  }

  SECTION(INTK_OVERWRITE_BLOCK)
  {
    for(const event_input_data &d : overwrite_data)
    {
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);

      int len = strlen(d.input);
      result = intake_apply_event_fixed(sub, INTK_OVERWRITE_BLOCK, intk.pos + len, len, d.input);
      ASSERTEQ(result, true, "%s", d.expected);
      ASSERTCMP(dest, d.expected, "");
    }
    // Reject null data...
    result = intake_apply_event_fixed(sub, INTK_OVERWRITE_BLOCK, intk.pos, 0, nullptr);
    ASSERTEQ(result, false, "");
  }

  SECTION(intake_input_string)
  {
    /**
     * Test the intake_input_string function with default intake event handling.
     * This should mostly just invoke INTK_INSERT_BLOCK/INTK_OVERWRITE_BLOCK.
     */
    for(const input_string_data &d : input_data)
    {
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_set_length(&intk, strlen(dest));
      intake_set_pos(&intk, d.start_pos);
      intake_set_insert(d.insert_on);

      const char *ret = intake_input_string(sub, d.input, d.linebreak_char);
      if(d.retval >= 0)
      {
        ASSERT(ret != nullptr, "%s", d.expected);
        ASSERTEQ((ssize_t)(ret - d.input), d.retval, "%s", d.expected);
      }
      else
        ASSERTEQ(ret, nullptr, "%s", d.expected);

      ASSERTCMP(dest, d.expected, "");
    }
  }
}

struct event_cb_data
{
  const char *expected;
  int *pos_external;
  int call_count;
};

static boolean event_callback(void *priv, subcontext *sub, enum intake_event_type type,
 int old_pos, int new_pos, int value, const char *data)
{
  struct intake_subcontext *intk = reinterpret_cast<struct intake_subcontext *>(sub);
  event_cb_data *d = reinterpret_cast<event_cb_data *>(priv);

  ASSERTEQ(old_pos, intk->pos, "%s", d->expected);
  if(intk->dest)
    ASSERTCMP(intk->dest, d->expected, "");

  if(d->pos_external)
    *(d->pos_external) = new_pos;

  d->call_count++;
  return true;
}

/**
 * Just make sure that the event callback is called and that the buffer (if
 * present) and intk->pos have not been modified at the time the callback is
 * called.
 */
UNITTEST(EventCallback)
{
#define na 0
  static constexpr char dummy_data[] = "whatever";
  static constexpr int dummy_data_len = arraysize(dummy_data) - 1;

  static const event_partial_data data[] =
  {
    { INTK_NO_EVENT,        "no event",       0, 0, na },
    { INTK_MOVE,            "move",           0, 4, na },
    { INTK_MOVE_WORDS,      "move words",     0, 0, 2 },
    { INTK_MOVE_WORDS,      "move words 2",   12, 12, -2 },
    { INTK_INSERT,          "insert",         3, 4, '$' },
    { INTK_OVERWRITE,       "overwrite",      4, 5, 'W' },
    { INTK_DELETE,          "delete",         5, 5, 1 },
    { INTK_BACKSPACE,       "backspace",      9, 8, 1 },
    { INTK_BACKSPACE_WORDS, "bkspace words",  13, 13, 1 },
    { INTK_CLEAR,           "clear string",   0, 0, na },
    { INTK_INSERT_BLOCK,    "insertblock",    0, dummy_data_len, dummy_data_len },
    { INTK_OVERWRITE_BLOCK, "overwriteblock", 0, dummy_data_len, dummy_data_len },
  };

  struct intake_subcontext intk{};
  subcontext *sub = reinterpret_cast<subcontext *>(&intk);
  char dest[256];
  int pos_external;

  intk.dest = dest;
  intk.max_length = 240;
  intk.current_length = 100;
  intk.event_cb = event_callback;
  intk.pos_external = &pos_external;

  SECTION(intake_event_ext_no_dest)
  {
    /**
     * Call intake_event_ext for an intake with no destination buffer.
     * Position and length modifications in such an intake will be determined
     * solely by those properties, max_length, and external pointers.
     */
    intk.dest = nullptr;

    for(const event_partial_data &d : data)
    {
      event_cb_data priv = { d.base, &pos_external, 0 };
      intk.event_priv = &priv;
      intk.pos = d.old_pos;
      intake_event_ext(&intk, d.type, d.old_pos, d.new_pos, d.value, dummy_data);
      ASSERTEQ(priv.call_count, 1, "%s", d.base);
      ASSERTEQ(intk.pos, d.new_pos, "%s", d.base);
    }
  }

  SECTION(intake_event_ext_dest)
  {
    /**
     * Call intake_event_ext for an intake with a destination buffer.
     * This is entirely just to make sure the destination buffer isn't modified.
     * Note the new position check after calling intake_event_ext relies on the
     * current length above being set beyond the actual string length (since
     * no modification of the buffer is ever actually performed).
     */
    for(const event_partial_data &d : data)
    {
      event_cb_data priv = { d.base, &pos_external, 0 };
      intk.event_priv = &priv;
      intk.pos = d.old_pos;
      snprintf(dest, sizeof(dest), "%s", d.base);
      intake_event_ext(&intk, d.type, d.old_pos, d.new_pos, d.value, dummy_data);
      ASSERTEQ(priv.call_count, 1, "%s", d.base);
      ASSERTEQ(intk.pos, d.new_pos, "%s", d.base);
    }
  }

  SECTION(intake_input_string)
  {
    /**
     * Test the intake_input_string function with an event callback.
     */
    intk.dest = nullptr;

    for(const input_string_data &d : input_data)
    {
      event_cb_data priv = { d.expected, &pos_external, 0 };
      intk.event_priv = &priv;

      intake_set_length(&intk, strlen(d.base));
      intake_set_pos(&intk, d.start_pos);
      intake_set_insert(d.insert_on);

      const char *ret = intake_input_string(sub, d.input, d.linebreak_char);
      if(strlen(d.input))
        ASSERTEQ(priv.call_count, 1, "%s", d.base);

      if(d.retval >= 0)
      {
        ASSERT(ret != nullptr, "%s", d.expected);
        ASSERTEQ((ssize_t)(ret - d.input), d.retval, "%s", d.expected);
      }
      else
        ASSERTEQ(ret, nullptr, "%s", d.expected);
    }
  }
}
