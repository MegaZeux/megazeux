/* MegaZeux
 *
 * Copyright (C) 2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "Unit.hpp"
#include "../src/sfx.h"
#include "../src/world.h"

#ifdef CONFIG_DEBYTECODE
#define SAM_S "("
#define SAM_E ")"
#else
#define SAM_S "&"
#define SAM_E "&"
#endif

struct auto_free
{
  struct sfx_list &sfx_list;
  auto_free(struct sfx_list &s): sfx_list(s) {}
  ~auto_free()
  {
    sfx_free(&sfx_list);
  }
};

struct input_data
{
  int index;
  char label[16];
  const char *string;
};

struct io_data
{
  char label_in[16];
  const char *string_in;
  const char *label_out;
  const char *string_out;
};

UNITTEST(GetSet)
{
  static constexpr io_data data[] =
  {
    { "",         "",             nullptr,      nullptr },
    { "Gem",      "5c-gec-gec",   "Gem",        "5c-gec-gec" },
    { "EmptyStr", "",             "EmptyStr",   nullptr },
    { "",         "EmptyLabel",   nullptr,      "EmptyLabel" },
    { "TooLongLabel", "abc",      "TooLongLabe","abc", },
    // No SFX conversions are performed by the core SFX functions.
    { "duhhh",    "&lmao.sam&3c", "duhhh",      "&lmao.sam&3c", },
    { "a",        "&a.sam&3c_abc","a",          "&a.sam&3c_abc", },
  };
  static constexpr input_data invalid[] =
  {
    { -1, "negative", "negative", },
    { -1000000000, "negative2", "negative2", },
    { MAX_NUM_SFX, "past max", "past max", },
    { 1<<30, "large", "large", },
  };

  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  boolean ret;
  int i = 0;

  SECTION(SetString)
  {
    for(auto &d : data)
    {
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.string_in);
      if(d.string_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.string_in);
        ASSERTCMP(sfx_list.list[i]->string, d.string_out, "%d", i);
      }
      else
        ASSERT(!sfx_list.list || !sfx_list.list[i], "%d should be null", i);

      i++;
    }
  }

  SECTION(SetLabel)
  {
    for(auto &d : data)
    {
      ret = sfx_set_label(&sfx_list, i, d.label_in, strlen(d.label_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      if(d.label_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.label_in);
        ASSERTCMP(sfx_list.list[i]->label, d.label_out, "%d", i);
        ASSERTCMP(sfx_list.list[i]->string, "", "%d", i);
      }
      // Setting the string should retain the label.
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      if(d.label_out || d.string_out)
      {
        ASSERT(sfx_list.list[i], "%d: '%s'", i, d.label_in);
        ASSERTCMP(sfx_list.list[i]->label, d.label_out ? d.label_out : "", "%d", i);
        ASSERTCMP(sfx_list.list[i]->string, d.string_out ? d.string_out : "", "%d", i);
      }
      else
        ASSERT(!sfx_list.list || !sfx_list.list[i], "%d: '%s'", i, d.label_in);

      i++;
    }
  }

  SECTION(Get)
  {
    for(auto &d : data)
    {
      ret = sfx_set_label(&sfx_list, i, d.label_in, strlen(d.label_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);
      ret = sfx_set_string(&sfx_list, i, d.string_in, strlen(d.string_in));
      ASSERT(ret, "%d: '%s'", i, d.label_in);

      if(sfx_list.list)
      {
        auto *p = sfx_get(&sfx_list, i);
        ASSERTEQ(sfx_list.list[i], p, "%d: '%s'", i, d.label_in);
      }
      i++;
    }
  }

  SECTION(Unset)
  {
    // Only set strings.
    for(i = 0; i < 32; i++)
    {
      ret = sfx_set_string(&sfx_list, i, "abc", 3);
      ASSERT(ret, "%d", i);
      ASSERT(sfx_list.list, "%d", i);
      ASSERT(sfx_list.list[i], "%d", i);
    }
    // Only set labels.
    for(; i < 64; i++)
    {
      ret = sfx_set_label(&sfx_list, i, "def", 3);
      ASSERT(ret, "%d", i);
      ASSERT(sfx_list.list, "%d", i);
      ASSERT(sfx_list.list[i], "%d", i);
    }
    // Should unset either...
    for(i = 0; i < 64; i++)
    {
      sfx_unset(&sfx_list, i);
      ASSERT(!sfx_list.list[i], "%d", i);
    }
  }

  SECTION(Invalid)
  {
    auto *n = sfx_get(&sfx_list, 0);
    ASSERT(!n, "get from NULL sfx list");

    for(auto &d : invalid)
    {
      ret = sfx_set_string(&sfx_list, d.index, d.string, strlen(d.string));
      ASSERTEQ(ret, 0, "%d: '%s'", d.index, d.string);
      ret = sfx_set_label(&sfx_list, d.index, d.label, strlen(d.label));
      ASSERTEQ(ret, 0, "%d: '%s'", d.index, d.string);
      auto *p = sfx_get(&sfx_list, d.index);
      ASSERT(!p, "%d: '%s'", d.index, d.string);
    }
  }

  sfx_free(&sfx_list);
  ASSERT(!sfx_list.list, "");
  ASSERTEQ(sfx_list.num_alloc, 0, "");
}


static constexpr io_data save_data[] =
{
  { "0",  "abc",              "0",  "abc", },
  { "1",  "def",              "1",  "def", },
  { "2",  "123515",           "2",  "123515", },
  { "3",  "!!zc",             "3",  "!!zc", },
  { "4",  "[[[%%",            "4",  "[[[%%", },
  // SFX conversions are performed by the loading functions.
  { "5",  "&shot.sam&3c",     "5",  SAM_S "shot.sam" SAM_E "3c", },
  { "6",  SAM_S "a.sam" SAM_E,"6",  SAM_S "a.sam" SAM_E, },
  { "7",  "&a.sam&3c&a.sam&", "7",  SAM_S "a.sam" SAM_E "3c" SAM_S "a.sam" SAM_E, },
  { "8",  "&a.sam&3c_abc",    "8",  SAM_S "a.sam" SAM_E "3c_abc", },
  { "9",  "(((())(()",        "9",  "(((())(()", },
  { "10", "&&&&&&&",          "10", SAM_S SAM_E SAM_S SAM_E SAM_S SAM_E "&", },
  // Don't save empty fields...
  { "11", "",                 "11", "", },
  { "",   "hhhhhhh",          "",   "hhhhhhh", },
  { "",   "",                 "",   "", },
  // The rest should be omitted entirely.
};

static constexpr char v251_data[NUM_BUILTIN_SFX][LEGACY_SFX_SIZE] =
{
  "abc",
  "def",
  "123515",
  "!!zc",
  "[[[%%",
  "&shot.sam&3c",
  SAM_S "a.sam" SAM_E,
  "&a.sam&3c&a.sam&",
  "&a.sam&3c_abc",
  "(((())(()",
  "&&&&&&&",
  "",
  "hhhhhhh",
  "",
};

static constexpr char v293_data[] =
{
  "MZFX\x1a\x00\x02\x5d"
  "\x01\x00\x01\x00\x00\x00\x00"
  "\x02\x00\x03\x00\x00\x00" "abc"
  "\x03\x00\x01\x00\x00\x00" "0"
  "\x01\x00\x01\x00\x00\x00\x01"
  "\x02\x00\x03\x00\x00\x00" "def"
  "\x03\x00\x01\x00\x00\x00" "1"
  "\x01\x00\x01\x00\x00\x00\x02"
  "\x02\x00\x06\x00\x00\x00" "123515"
  "\x03\x00\x01\x00\x00\x00" "2"
  "\x01\x00\x01\x00\x00\x00\x03"
  "\x02\x00\x04\x00\x00\x00" "!!zc"
  "\x03\x00\x01\x00\x00\x00" "3"
  "\x01\x00\x01\x00\x00\x00\x04"
  "\x02\x00\x05\x00\x00\x00" "[[[%%"
  "\x03\x00\x01\x00\x00\x00" "4"
  "\x01\x00\x01\x00\x00\x00\x05"
  "\x02\x00\x0c\x00\x00\x00" "&shot.sam&3c"
  "\x03\x00\x01\x00\x00\x00" "5"
  "\x01\x00\x01\x00\x00\x00\x06"
  "\x02\x00\x07\x00\x00\x00" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "6"
  "\x01\x00\x01\x00\x00\x00\x07"
  "\x02\x00\x10\x00\x00\x00" "&a.sam&3c&a.sam&"
  "\x03\x00\x01\x00\x00\x00" "7"
  "\x01\x00\x01\x00\x00\x00\x08"
  "\x02\x00\x0d\x00\x00\x00" "&a.sam&3c_abc"
  "\x03\x00\x01\x00\x00\x00" "8"
  "\x01\x00\x01\x00\x00\x00\x09"
  "\x02\x00\x09\x00\x00\x00" "(((())(()"
  "\x03\x00\x01\x00\x00\x00" "9"
  "\x01\x00\x01\x00\x00\x00\x0a"
  "\x02\x00\x07\x00\x00\x00" "&&&&&&&"
  "\x03\x00\x02\x00\x00\x00" "10"
  "\x01\x00\x01\x00\x00\x00\x0b"
  "\x03\x00\x02\x00\x00\x00" "11"
  "\x01\x00\x01\x00\x00\x00\x0c"
  "\x02\x00\x07\x00\x00\x00" "hhhhhhh"
  "\x00" // + nul completes terminator
};

#ifdef CONFIG_DEBYTECODE
static constexpr char v300_data[] =
{
  "MZFX\x1a\x00\x03\x00"
  "\x01\x00\x01\x00\x00\x00\x00"
  "\x02\x00\x03\x00\x00\x00" "abc"
  "\x03\x00\x01\x00\x00\x00" "0"
  "\x01\x00\x01\x00\x00\x00\x01"
  "\x02\x00\x03\x00\x00\x00" "def"
  "\x03\x00\x01\x00\x00\x00" "1"
  "\x01\x00\x01\x00\x00\x00\x02"
  "\x02\x00\x06\x00\x00\x00" "123515"
  "\x03\x00\x01\x00\x00\x00" "2"
  "\x01\x00\x01\x00\x00\x00\x03"
  "\x02\x00\x04\x00\x00\x00" "!!zc"
  "\x03\x00\x01\x00\x00\x00" "3"
  "\x01\x00\x01\x00\x00\x00\x04"
  "\x02\x00\x05\x00\x00\x00" "[[[%%"
  "\x03\x00\x01\x00\x00\x00" "4"
  "\x01\x00\x01\x00\x00\x00\x05"
  "\x02\x00\x0c\x00\x00\x00" SAM_S "shot.sam" SAM_E "3c"
  "\x03\x00\x01\x00\x00\x00" "5"
  "\x01\x00\x01\x00\x00\x00\x06"
  "\x02\x00\x07\x00\x00\x00" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "6"
  "\x01\x00\x01\x00\x00\x00\x07"
  "\x02\x00\x10\x00\x00\x00" SAM_S "a.sam" SAM_E "3c" SAM_S "a.sam" SAM_E
  "\x03\x00\x01\x00\x00\x00" "7"
  "\x01\x00\x01\x00\x00\x00\x08"
  "\x02\x00\x0d\x00\x00\x00" SAM_S "a.sam" SAM_E "3c_abc"
  "\x03\x00\x01\x00\x00\x00" "8"
  "\x01\x00\x01\x00\x00\x00\x09"
  "\x02\x00\x09\x00\x00\x00" "(((())(()"
  "\x03\x00\x01\x00\x00\x00" "9"
  "\x01\x00\x01\x00\x00\x00\x0a"
  "\x02\x00\x07\x00\x00\x00" SAM_S SAM_E SAM_S SAM_E SAM_S SAM_E "&"
  "\x03\x00\x02\x00\x00\x00" "10"
  "\x01\x00\x01\x00\x00\x00\x0b"
  "\x03\x00\x02\x00\x00\x00" "11"
  "\x01\x00\x01\x00\x00\x00\x0c"
  "\x02\x00\x07\x00\x00\x00" "hhhhhhh"
  "\x00" // + nul completes terminator
};
#endif

UNITTEST(SaveSFX)
{
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  char buffer[NUM_BUILTIN_SFX * LEGACY_SFX_SIZE];
  size_t required = 0;
  size_t sz;
  boolean ret;
  int i = 0;

  for(auto &d : save_data)
  {
    ret = sfx_set_label(&sfx_list, i, d.label_out, strlen(d.label_out));
    ASSERT(ret, "%d", i);
    ret = sfx_set_string(&sfx_list, i, d.string_out, strlen(d.string_out));
    ASSERT(ret, "%d", i);
    i++;
  }

// FIXME: 3.x should either back-convert the SFX string or drop this feature entirely.
#ifndef CONFIG_DEBYTECODE
  SECTION(Array)
  {
    sz = sfx_save_to_memory(&sfx_list, V251, buffer, sizeof(buffer), nullptr);
    ASSERTEQ(sz, sizeof(v251_data), "");
    ASSERTMEM(buffer, v251_data, sizeof(buffer), "");
  }

  SECTION(Properties293)
  {
    sz = sfx_save_to_memory(&sfx_list, V293, nullptr, 0, &required);
    ASSERTEQ(sz, 0, "");
    ASSERTEQ(required, sizeof(v293_data), "");
    sz = sfx_save_to_memory(&sfx_list, V293, buffer, required, nullptr);
    ASSERTEQ(sz, required, "");
    ASSERTMEM(buffer, v293_data, required, "");
  }
#endif

#ifdef CONFIG_DEBYTECODE
  SECTION(Properties300)
  {
    sz = sfx_save_to_memory(&sfx_list, V300, nullptr, 0, &required);
    ASSERTEQ(sz, 0, "");
    ASSERTEQ(required, sizeof(v300_data), "");
    sz = sfx_save_to_memory(&sfx_list, V300, buffer, required, nullptr);
    ASSERTEQ(sz, required, "");
    ASSERTMEM(buffer, v300_data, required, "");
  }
#endif
}

template<int N>
static void compare_load(const struct sfx_list *sfx_list, const io_data (&data)[N], boolean skip_labels = false)
{
  int i = 0;
  for(auto &d : data)
  {
    const struct custom_sfx *sfx = sfx_get(sfx_list, i);
    size_t a = strlen(d.string_out);
    size_t b = strlen(d.label_out);

    if(a || (b && !skip_labels))
    {
      ASSERT(sfx, "%d", i);
      ASSERTCMP(sfx->string, d.string_out, "%d", i);
      if(!skip_labels)
        ASSERTCMP(sfx->label, d.label_out, "%d", i);
    }
    else
      ASSERTEQ(sfx, nullptr, "%d", i);

    i++;
  }
}

UNITTEST(LoadSFX)
{
  struct sfx_list sfx_list{};
  auto_free a(sfx_list);
  boolean ret;

  SECTION(Array)
  {
    ret = sfx_load_from_memory(&sfx_list, V251, (const char *)v251_data, sizeof(v251_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data, true);
  }

  SECTION(Properties293)
  {
    ret = sfx_load_from_memory(&sfx_list, V293, v293_data, sizeof(v293_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data);
  }

#ifdef CONFIG_DEBYTECODE
  SECTION(Properties300)
  {
    ret = sfx_load_from_memory(&sfx_list, V300, v300_data, sizeof(v300_data));
    ASSERT(ret, "");
    compare_load(&sfx_list, save_data);
  }
#endif
}
