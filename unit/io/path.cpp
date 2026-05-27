/* MegaZeux
 *
 * Copyright (C) 2020-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../Unit.hpp"
#include "../../src/io/path.h"
#include "../../src/io/vio.h"

#include <errno.h>
#include <sys/stat.h>

/* path_is_absolute, path_clean, path_clean_copy, path_navigate, and
 * path_navigate_no_check have significantly different behavior between
 * platforms and require separate implementations. Many of these can be
 * tested in a cross-platform manner, so store test data for all platforms,
 * and use this selector to get the "native" data.
 */
#ifdef PATH_AMIGA_STYLE_ROOTS
#undef amiga
#define PATH_SELECTOR amiga
#elif defined(PATH_UNC_ROOTS)
#define PATH_SELECTOR win32
#elif defined(PATH_DOS_STYLE_ROOTS)
#if DIR_SEPARATOR_CHAR == '\\'
#define PATH_SELECTOR_DOS dos
#else
#define PATH_SELECTOR_DOS posixdos
#endif
#define PATH_SELECTOR PATH_SELECTOR_DOS
#else
#define PATH_SELECTOR posix
#endif


#define PATH_FILE_EXISTS        "path_file_exists"
#define PATH_DIR_EXISTS         "path_dir_exists"
#define PATH_DIR_EXISTS_2       PATH_DIR_EXISTS DIR_SEPARATOR "path_dir_exists"
#define PATH_DIR_NOT_EXISTS     "path_dir_kgdfsdlf"
#define PATH_DIR_RECURSIVE      "path_dir_recursive"
#define PATH_DIR_RECURSIVE_2    "path_dir_recursive" DIR_SEPARATOR PATH_DIR_RECURSIVE
#define PATH_DIR_RECURSIVE_3    "path_dir_recursive" DIR_SEPARATOR PATH_DIR_RECURSIVE_2
#define PATH_FILE_RECURSIVE     PATH_DIR_RECURSIVE DIR_SEPARATOR PATH_FILE_EXISTS


UNITTEST(Init)
{
  vfile *vf;
  vrmdir(PATH_DIR_RECURSIVE_3);
  vrmdir(PATH_DIR_RECURSIVE_2);
  vmkdir(PATH_DIR_EXISTS, 0755);
  vmkdir(PATH_DIR_EXISTS_2, 0755);
  vmkdir(PATH_DIR_RECURSIVE, 0755);
  vf = vfopen_unsafe(PATH_FILE_EXISTS, "wb");
  vfclose(vf);
  vf = vfopen_unsafe(PATH_FILE_RECURSIVE, "wb");
  vfclose(vf);
}


struct path_tokenize_result
{
  const char *input;
  const char *expected[8];
};

UNITTEST(path_tokenize)
{
  static const path_tokenize_result data[] =
  {
    { nullptr,        { nullptr }},
    { "",             { "", nullptr }},
    { "shdfkjshdf",   { "shdfkjshdf", nullptr }},
    { "/",            { "", "", nullptr }},
    { "\\\\unc\\p",   { "", "", "unc", "p", nullptr }},
    { "\\\\.\\C:",    { "", "", ".", "C:" }},
    { "C:\\a",        { "C:", "a", nullptr }},
    { "sdcard:/bees", { "sdcard:", "bees", nullptr }},
    { "a\\lomg/path", { "a", "lomg", "path", nullptr }},
    { "test///path",  { "test", "", "", "path", nullptr }},
    { "trailing/",    { "trailing", "", nullptr }},
    {
      u8"/home/\u00C8śŚ/megazeux/DE/saved.sav",
      { "", "home", u8"\u00C8śŚ", "megazeux", "DE", "saved.sav", nullptr }
    },
  };

  for(const path_tokenize_result &d : data)
  {
    char buffer[256];
    char *cursor;
    char *current;
    int pos = 0;

    if(d.input)
    {
      snprintf(buffer, sizeof(buffer), "%s", d.input);
      cursor = buffer;
    }
    else
      cursor = nullptr;

    while((current = path_tokenize(&cursor)))
    {
      ASSERTCMP(current, d.expected[pos], "%s : %d", d.input, pos);
      pos++;
    }
    ASSERTEQ(current, d.expected[pos], "%s : %d", d.input, pos);
  }
}


UNITTEST(path_reverse_tokenize)
{
  static constexpr path_tokenize_result data[] =
  {
    { nullptr,          { nullptr }},
    { "",               { "" }},
    { "abcde",          { "abcde" }},
    { "path1/path2",    { "path2", "path1" }},
    { "p1\\p2\\p3",     { "p3", "p2", "p1" }},
    { "m1/m2\\m3/m4",   { "m4", "m3", "m2", "m1" }},
    { "clean///first",  { "first", "", "", "clean" }},
    { "../duhh/./",     { "", ".", "duhh", ".." }},
    { "/",              { "", "" }},
    { "/dir",           { "dir", "" }},
    { "/dir1/dir2",     { "dir2", "dir1", "" }},
    { "C:\\",           { "", "C:" }},
    { "C:\\WINDOWS",    { "WINDOWS", "C:" }},
    { "C:\\a\\b",       { "b", "a", "C:" }},
    { "sdcard:/butt",   { "butt", "sdcard:" }},
    { "\\\\unc\\path",  { "path", "unc", "", "" }},
    { "\\\\.\\C:",      { "C:", ".", "", "" }},
    {
      u8"/home/\u00C8śŚ/megazeux/DE/saved.sav",
      { "saved.sav", "DE", "megazeux", u8"\u00C8śŚ", "home", "" }
    },
  };

  size_t base_length;
  char *base;
  char *token;

  token = path_reverse_tokenize(nullptr, nullptr);
  ASSERTEQ(token, nullptr, "");
  token = path_reverse_tokenize(nullptr, &base_length);
  ASSERTEQ(token, nullptr, "");

  for(auto &d : data)
  {
    char buffer[256];
    int pos = 0;

    const auto &init = [&]()
    {
      if(d.input)
      {
        base_length = snprintf(buffer, sizeof(buffer), "%s", d.input);
        base = buffer;
        ASSERT(base_length < sizeof(buffer), "");
      }
      else
      {
        base_length = 0xbaad; // Some junk, should be ignored.
        base = nullptr;
      }
      pos = 0;
    };

    init();
    while((token = path_reverse_tokenize(&base, &base_length)))
    {
      ASSERTCMP(token, d.expected[pos], "%s : %d", d.input, pos);
      pos++;
    }
    ASSERTEQ(token, d.expected[pos], "%s : %d", d.input, pos);

    // Should work identically without base_length, it's just slower.
    init();
    while((token = path_reverse_tokenize(&base, nullptr)))
    {
      ASSERT(d.expected[pos], "%s : %d", d.input, pos);
      ASSERTCMP(token, d.expected[pos], "%s : %d", d.input, pos);
      pos++;
    }
    ASSERTEQ(token, d.expected[pos], "%s : %d", d.input, pos);
  }
}


struct path_ext_result
{
  const char *path;
  const char *ext;
  const char *result;
};

UNITTEST(path_force_ext)
{
  static const path_ext_result data[] =
  {
    {
      "/here/is/a/path.chr",
      ".chr",
      "/here/is/a/path.chr"
    },
    {
      "/dont/modify/this/one/either/a.palidx",
      ".palidx",
      "/dont/modify/this/one/either/a.palidx"
    },
    {
      "/Ext/Should/Be/Case/Insensitive.EXT",
      ".ext",
      "/Ext/Should/Be/Case/Insensitive.EXT"
    },
    {
      "/Ext/Should/Be/Case/Insensitive.ext",
      ".EXT",
      "/Ext/Should/Be/Case/Insensitive.ext"
    },
    {
      "/add/the/ext/to/this/one.pal",
      ".chr",
      "/add/the/ext/to/this/one.pal.chr"
    },
    {
      "/lol.mxz",
      ".mzx",
      "/lol.mxz.mzx"
    },
    {
      "/hellowwolrd",
      ".sav",
      "/hellowwolrd.sav"
    },
    {
      u8"/home/\u00C8śŚ/megazeux/DE/saved.sav",
      ".sav",
      u8"/home/\u00C8śŚ/megazeux/DE/saved.sav",
    },
    {
      u8"/home/\u00C8śŚ/megazeux/DE/saved.svaśŚ",
      ".sav",
      u8"/home/\u00C8śŚ/megazeux/DE/saved.svaśŚ.sav",
    },
  };
  // These should result in failures in the second section.
  // All of them assume a buffer size of 32.
  static const path_ext_result bad_data[] =
  {
    {
      "/an/input/path",
      ".verylongextensionwtfwhydidyoudothis",
      "/an/input/path"
    },
    {
      "/some/really/long/path/i.guess",
      ".lol",
      "/some/really/long/path/i.guess"
    },
  };
  char buffer[MAX_PATH];
  boolean result;

  SECTION(NormalCases)
  {
    for(const path_ext_result &d : data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      result = path_force_ext(buffer, MAX_PATH, d.ext);

      ASSERT(result, "%s", d.path);
      ASSERTCMP(buffer, d.result, "%s", d.path);
    }
  }

  SECTION(FailureCases)
  {
    for(const path_ext_result &d : bad_data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      result = path_force_ext(buffer, 32, d.ext);

      ASSERT(!result, "%s", d.path);
      ASSERTCMP(buffer, d.result, "%s", d.path);
    }
  }
}


struct path_is_abs_result
{
  ssize_t root_len;
  boolean is_root;
};

struct path_is_abs_data
{
  const char *path;
  struct path_is_abs_result posix;    /* POSIX with added root:// roots */
  struct path_is_abs_result posixdos; /* POSIX with added root:/ roots (console SDKs) */
  struct path_is_abs_result dos;      /* DOS */
  struct path_is_abs_result win32;    /* DOS with UNC roots */
  struct path_is_abs_result amiga;    /* Amiga */

  constexpr static struct path_is_abs_data all(const char *p,
   struct path_is_abs_result _all)
  {
    return path_is_abs_data{ p, _all, _all, _all, _all, _all };
  }

  constexpr static struct path_is_abs_data posixroot(const char *p,
   struct path_is_abs_result _posix, struct path_is_abs_result _noposix)
  {
    return path_is_abs_data{ p, _posix, _posix, _posix, _posix, _noposix };
  }

  constexpr static struct path_is_abs_data dosroot(const char *p,
   struct path_is_abs_result _posix, struct path_is_abs_result _dos)
  {
    return path_is_abs_data{ p, _posix, _dos, _dos, _dos, _dos };
  }

  constexpr static struct path_is_abs_data amigaroot(const char *p,
   struct path_is_abs_result _noamiga, struct path_is_abs_result _amiga)
  {
    return path_is_abs_data{ p, _noamiga, _noamiga, _noamiga, _noamiga, _amiga };
  }

  constexpr static struct path_is_abs_data uncroot(const char *p,
   struct path_is_abs_result _posix, struct path_is_abs_result _win32,
   struct path_is_abs_result _amiga)
  {
    return path_is_abs_data{ p, _posix, _posix, _posix, _win32, _amiga };
  }
};

UNITTEST(path_is_absolute)
{
  static constexpr const path_is_abs_data data[]
  {
    path_is_abs_data::all(
      "",
      { 0, false }
    ),
    path_is_abs_data::all(
      "sdhfjkshfjkds",
      { 0, false }
    ),
    path_is_abs_data::all(
      "relative/path/here",
      { 0, false }
    ),
    path_is_abs_data::amigaroot(
      ":/wtf",
      { 0, false },
      { 2, false }
    ),
    // Unix-style roots (all platforms but Amiga)
    path_is_abs_data::posixroot(
      "/",
      { 1, true },
      { 0, false }
    ),
    path_is_abs_data::posixroot(
      "/absolute/but/not/root",
      { 1, false },
      { 0, false }
    ),
    // DOS-style paths (Windows, DOS, Amiga, and various consoles).
    path_is_abs_data::dosroot(
      "A:",
      { 0, false },
      { 2, true }
    ),
    path_is_abs_data::dosroot(
      "C:\\",
      { 0, false },
      { 3, true }
    ),
    path_is_abs_data::dosroot(
      "sdcard:/",
      { 0, false },
      { 8, true }
    ),
    path_is_abs_data::dosroot(
      "C:\\absolute/not\\root",
      { 0, false },
      { 3, false }
    ),
    path_is_abs_data::dosroot(
      "software:",
      { 0, false },
      { 9, true }
    ),
    // Amiga-style paths (Amiga-only).
    path_is_abs_data::amigaroot(
      "sys:some/path",
      { 0, false },
      { 4, false }
    ),
    path_is_abs_data::amigaroot(
      ":",
      { 0, false },
      { 1, true }
    ),
    path_is_abs_data::amigaroot(
      ":System/Shell",
      { 0, false },
      { 1, false }
    ),
    path_is_abs_data::amigaroot(
      "are/you/serious:",
      { 0, false },
      { 16, true }
    ),
    path_is_abs_data::amigaroot(
      "w/t:f",
      { 0, false },
      { 4, false }
    ),
    path_is_abs_data::amigaroot(
      "/lol:",
      { 1, false },
      { 5, true }
    ),
    path_is_abs_data::amigaroot(
      "/lol:lmao",
      { 1, false },
      { 5, false }
    ),
    // Modified DOS-style paths (all platforms).
    path_is_abs_data::all(
      "C://",
      { 4, true }
    ),
    path_is_abs_data::all(
      "sdcard://",
      { 9, true }
    ),
    path_is_abs_data::all(
      "fat://absolute/but/not/root",
      { 6, false }
    ),
    // Windows UNC paths
    path_is_abs_data::uncroot(
      "\\\\.\\C:",
      { 1, false },
      { 6, true },
      { 6, true }
    ),
    path_is_abs_data::uncroot(
      "\\\\?\\Z:",
      { 1, false },
      { 6, true },
      { 6, true }
    ),
    path_is_abs_data::uncroot(
      "\\\\unc\\root\\",
      { 1, false },
      { 11, true },
      { 0, false }
    ),
    path_is_abs_data::uncroot(
      "\\\\.\\unc\\localhost\\c$",
      { 1, false },
      { 20, true },
      { 0, false }
    ),
    path_is_abs_data::uncroot(
      "//?/unc/thisworks/too",
      { 1, false },
      { 21, true },
      { 0, false }
    ),
    path_is_abs_data::uncroot(
      "\\\\.\\unc\\localhost\\c$\\somefile",
      { 1, false },
      { 21, false },
      { 0, false }
    ),
    path_is_abs_data::posixroot(
      "\\\\bad_unc",
      { 1, false },
      { 0, false }
    ),
  };

  SECTION(path_is_absolute)
  {
    for(const path_is_abs_data &d : data)
    {
      ssize_t len = path_is_absolute(d.path);
      ASSERTEQ(len, d.PATH_SELECTOR.root_len, "%s", d.path);
    }
  }

  SECTION(path_is_absolute_dos)
  {
    for(const path_is_abs_data &d : data)
    {
      ssize_t len = path_is_absolute_dos(d.path);
      ASSERTEQ(len, d.posixdos.root_len, "%s", d.path);
      ASSERTEQ(len, d.dos.root_len, "%s", d.path);
    }
  }

  SECTION(path_is_root)
  {
    for(const path_is_abs_data &d : data)
    {
      boolean is_root = path_is_root(d.path);
      ASSERTEQ(is_root, d.PATH_SELECTOR.is_root, "%s", d.path);
    }
  }
}


struct path_output_pair
{
  const char *path;
  const char *expected;
  ssize_t return_value;
};

UNITTEST(path_get_ext_offset)
{
  static const path_output_pair data[]
  {
    {
      "",
      nullptr,
      -1
    },
    {
      "return -1 for directories/",
      nullptr,
      -1
    },
    {
      "no really return -1 for directories\\",
      nullptr,
      -1
    },
    {
      "filewithnoext",
      nullptr,
      -1
    },
    {
      "somepath.not.an.ext/filewithnoext",
      nullptr,
      -1
    },
    {
      "somepath.still.not.an.ext\\filewithnoext",
      nullptr,
      -1
    },
    {
      "yeah this really counts.",
      ".",
      23
    },
    {
      "CAVERNS.MZX",
      ".MZX",
      7
    },
    {
      "zeux/forest/FOREST.MZX",
      ".MZX",
      18
    },
    {
      "assets/glsl/scalers/softscale.frag",
      ".frag",
      29
    },
    {
      "testgame\\smzx2.palidx",
      ".palidx",
      14
    },
    {
      "/lol.mxz.mzx",
      ".mzx",
      8
    },
    {
      ".mzx",
      ".mzx",
      0
    },
    {
      u8"unicode_fileśŚ.mzx",
      ".mzx",
      16
    },
    {
      u8"unicode_pathśŚ/caverns.mzx",
      ".mzx",
      24
    },
    /* More absolute paths... */
    {
      "c:/caverns.mzx",
      ".mzx",
      10
    },
    {
      "a:\\.mzx",
      ".mzx",
      3
    },
    {
      "fat://.awawa",
      ".awawa",
      6
    },
    {
      "any.oof://nope",
      nullptr,
      -1
    },
    {
      "amiga:filename.ext",
      ".ext",
      14
    },
    {
      "amiga:.thistoo",
      ".thistoo",
      6,
    },
    {
      "amiga.oof:nope",
#ifndef PATH_AMIGA_STYLE_ROOTS
      ".oof:nope",
      5,
#else
      nullptr,
      -1
#endif
    }
  };
  ssize_t result;

  for(const path_output_pair &d : data)
  {
    result = path_get_ext_offset(d.path);
    ASSERTEQ(result, d.return_value, "%s", d.path);
    if(result >= 0 && d.expected)
      ASSERTCMP(d.path + result, d.expected, "%s", d.path);
  }
}


struct path_clean_result
{
  const char *result;
};

struct path_clean_data
{
  const char *path;
  struct path_clean_result posix;     /* POSIX with added root:// roots */
  struct path_clean_result posixdos;  /* POSIX with added root:/ roots (console SDKs) */
  struct path_clean_result dos;       /* DOS */
  struct path_clean_result win32;     /* DOS with UNC roots */
  struct path_clean_result amiga;     /* Amiga */

  constexpr static struct path_clean_data all(const char *p,
   struct path_clean_result _all)
  {
    return path_clean_data{ p, _all, _all, _all, _all, _all };
  }

  constexpr static struct path_clean_data posixroot(const char *p,
   struct path_clean_result _posix, struct path_clean_result _dos)
  {
    return path_clean_data{ p, _posix, _posix, _dos, _dos, _posix };
  }

  constexpr static struct path_clean_data amigaroot(const char *p,
   struct path_clean_result _posix, struct path_clean_result _dos,
   struct path_clean_result _amiga)
  {
    return path_clean_data{ p, _posix, _posix, _dos, _dos, _amiga };
  }

  constexpr static struct path_clean_data extroot(const char *p,
   struct path_clean_result _posix, struct path_clean_result _posixdos,
   struct path_clean_result _dos, struct path_clean_result _amiga)
  {
    return path_clean_data{ p, _posix, _posixdos, _dos, _dos, _amiga };
  }
};

UNITTEST(path_clean)
{
  static constexpr const path_clean_data data[] =
  {
    path_clean_data::all(
      "",
      { "" }
    ),
    path_clean_data::posixroot(
      "/a/path",
      { "/a/path" },
      { "\\a\\path" }
    ),
    path_clean_data::posixroot(
      "/remove/trailing/slash/",
      { "/remove/trailing/slash" },
      { "\\remove\\trailing\\slash" }
    ),
    path_clean_data::posixroot(
      "/normalize\\slashes/that\\are/like\\this",
      { "/normalize/slashes/that/are/like/this" },
      { "\\normalize\\slashes\\that\\are\\like\\this" }
    ),
    path_clean_data::amigaroot(
      "/////remove////duplicate//////slashes/////",
      { "/remove/duplicate/slashes" },
      { "\\remove\\duplicate\\slashes" },
      { "/////remove////duplicate//////slashes/////" }
    ),
    path_clean_data::amigaroot(
      "C:\\work\\on\\dos\\style\\paths\\",
      { "C:/work/on/dos/style/paths" },
      { "C:\\work\\on\\dos\\style\\paths" },
      { "C:work/on/dos/style/paths" }
    ),
    path_clean_data::extroot(
      "C:\\\\\\remove\\\\\\duplicate\\\\slashes\\\\here\\\\too\\",
      { "C://remove/duplicate/slashes/here/too" },
      { "C:/remove/duplicate/slashes/here/too" },
      { "C:\\remove\\duplicate\\slashes\\here\\too" },
      { "C:remove///duplicate//slashes//here//too" }
    ),
    path_clean_data::extroot(
      "C:\\",
      { "C:" },
      { "C:/" },
      { "C:\\" },
      { "C:" }
    ),
    path_clean_data::extroot(
      "C:/",
      { "C:" },
      { "C:/" },
      { "C:\\" },
      { "C:" }
    ),
    path_clean_data::extroot(
      "C:\\\\/",
      { "C://" },
      { "C:/" },
      { "C:\\" },
      { "C:" }
    ),
    path_clean_data::posixroot(
      "/",
      { "/" },
      { "\\" }
    ),
    path_clean_data::posixroot(
      "\\",
      { "/" },
      { "\\" }
    ),
    // Windows UNC paths: do not remove duplicate slashes from the prefix.
    path_clean_data::amigaroot(
      "\\\\.\\C:\\",
      { "/./C:" },
      { "\\\\.\\C:\\" },
      { "\\\\.\\C:" }
    ),
    path_clean_data::amigaroot(
      "\\\\?\\C:\\",
      { "/?/C:" },
      { "\\\\?\\C:\\" },
      { "\\\\?\\C:" }
    ),
    path_clean_data::amigaroot(
      "\\\\.\\unc\\\\localhost\\\\\\duhhr",
      { "/./unc/localhost/duhhr" },
      { "\\\\.\\unc\\localhost\\duhhr" },
      { "//./unc//localhost///duhhr" }
    ),
    path_clean_data::amigaroot(
      "\\\\?\\unc\\\\localhost\\\\\\duhhr",
      { "/?/unc/localhost/duhhr" },
      { "\\\\?\\unc\\localhost\\duhhr" },
      { "//?/unc//localhost///duhhr" }
    ),
  };
  /* TODO: this should be combined into the prior array. */
  static constexpr const path_clean_data clean_current_tokens[] =
  {
    /* path_clean* functions remove redundant POSIX current directory tokens,
     * but keep non-redundant tokens. Don't do this on Amiga. path_navigate*
     * is required to resolve parent directory tokens.
     *
     * TODO: this is a separate function from the clean functions, so it
     * currently doesn't clean slashes. Every platform should get the POSIX
     * results below currently; the other results assume this is part of
     * a regular clean function.
     */
    path_clean_data::all(
      ".",
      { "." }
    ),
    path_clean_data::amigaroot(
      "./.",
      { "." },
      { "." },
      { "./." }
    ),
    path_clean_data::amigaroot(
      "../..",
      { "../.." },
      { "..\\.." },
      { "../.." }
    ),
    path_clean_data::amigaroot(
      "./what/././././dafuq/./?/./",
      { "what/dafuq/?/" },
      { "what\\dafuq\\?\\" },
      { "./what/././././dafuq/./?/./" }
    ),
    path_clean_data::amigaroot(
      "a/./mixe/../of/./tokens",
      { "a/mixe/../of/tokens" },
      { "a\\mixe\\..\\of\\tokens" },
      { "a/./mixe/../of/./tokens" }
    ),
    path_clean_data::extroot(
      "fat://././.",
      { "fat://" },
      { "fat:/" },
      { "fat:\\" },
      { "fat:././." }
    ),
  };
  // Data for testing truncation (assume buffer size == 32). This only matters
  // for path_clean_copy, which should return false in this case.
  static constexpr const path_clean_data truncate_data[] =
  {
    path_clean_data::posixroot(
      "/a/rly/long/path/looooooooooooooooooooool/",
      { "/a/rly/long/path/looooooooooooo" },
      { "\\a\\rly\\long\\path\\looooooooooooo" }
    ),
    path_clean_data::amigaroot(
      "C:\\truncate\\my\\dos\\style\\path\\pls",
      { "C:/truncate/my/dos/style/path/p" },
      { "C:\\truncate\\my\\dos\\style\\path\\p" },
      { "C:truncate/my/dos/style/path/pl" }
    ),
  };

  char buffer[MAX_PATH];

  SECTION(path_clean)
  {
    for(const path_clean_data &d : data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      path_clean(buffer, MAX_PATH);
      ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_clean_copy)
  {
    for(const path_clean_data &d : data)
    {
      size_t result = path_clean_copy(buffer, MAX_PATH, d.path);
      ASSERTEQ(result, strlen(d.PATH_SELECTOR.result), "%s", d.path);
      ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_clean_copy_truncation)
  {
    for(const path_clean_data &d : truncate_data)
    {
      size_t result = path_clean_copy(buffer, 32, d.path);
      ASSERTEQ(result, strlen(d.PATH_SELECTOR.result), "%s", d.path);
      ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_clean_copy_posixdos)
  {
    for(const path_clean_data &d : data)
    {
      size_t result = path_clean_copy_posixdos(buffer, MAX_PATH, d.path);
      ASSERTEQ(result, strlen(d.posixdos.result), "%s", d.path);
      ASSERTCMP(buffer, d.posixdos.result, "%s", d.path);
    }
  }

  SECTION(path_clean_current_tokens)
  {
    for(const path_clean_data &d : clean_current_tokens)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      path_clean_current_tokens(buffer, MAX_PATH);
      ASSERTCMP(buffer, d.posix.result, "%s", d.path);
    }
  }

  SECTION(path_clean_root)
  {
    static constexpr const path_clean_data root_clean_data[] =
    {
      path_clean_data::all(
        "/",
        { DIR_SEPARATOR }
      ),
      path_clean_data::all(
        "fat:/",
        { "fat" }
      ),
      path_clean_data::all(
        "fat://",
        { "fat" }
      ),
      path_clean_data::all(
        "C:\\",
        { "C" }
      ),
      path_clean_data::all(
        "sys:",
        { "sys" }
      ),
      path_clean_data::all(
        ":",
        { ":" }
      ),
      path_clean_data::all(
        "hello",
        { "hello" }
      ),
    };

    for(const path_clean_data &d : root_clean_data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      path_clean_root(buffer, MAX_PATH);
      ASSERTCMP(buffer, d.posix.result, "%s", d.path);
    }
  }
}


struct path_return_result
{
  ssize_t return_value;
  const char *result;
};

struct path_split_data
{
  const char *path;
  struct path_return_result posix;    /* POSIX with added root:// roots */
  struct path_return_result posixdos; /* POSIX with added root:/ roots (console SDKs) */
  struct path_return_result dos;      /* DOS */
  struct path_return_result win32;    /* DOS with UNC roots */
  struct path_return_result amiga;    /* Amiga */
  struct path_return_result filename;
  boolean dir_and_file_return_value;

  static constexpr struct path_split_data all(const char *p,
   struct path_return_result dir, struct path_return_result file,
   boolean dir_and_file_ret)
  {
    return path_split_data{ p, dir, dir, dir, dir, dir, file, dir_and_file_ret };
  }

  static constexpr struct path_split_data posixroot(const char *p,
   struct path_return_result posix_dir, struct path_return_result dos_dir,
   struct path_return_result file, boolean dir_and_file_ret)
  {
    return path_split_data{ p, posix_dir, posix_dir, dos_dir, dos_dir,
                            posix_dir, file, dir_and_file_ret };
  }

  static constexpr struct path_split_data posixroot(const char *p,
   struct path_return_result posix_dir, struct path_return_result dos_dir,
   struct path_return_result amiga_dir, struct path_return_result file,
   boolean dir_and_file_ret)
  {
    return path_split_data{ p, posix_dir, posix_dir, dos_dir, dos_dir,
                            amiga_dir, file, dir_and_file_ret };
  }

  static constexpr struct path_split_data posixdosroot(const char *p,
   struct path_return_result posix_dir, struct path_return_result posixdos_dir,
   struct path_return_result dos_dir, struct path_return_result amiga_dir,
   struct path_return_result file, boolean dir_and_file_ret)
  {
    return path_split_data{ p, posix_dir, posixdos_dir, dos_dir, dos_dir,
                            amiga_dir, file, dir_and_file_ret };
  }
};

UNITTEST(path_split_functions)
{
  // All of these tests assume a directory stat fail on the input path.
  static constexpr const path_split_data data[] =
  {
    path_split_data::all(
      "",
      { -1, nullptr },
      { -1, nullptr },
      false
    ),
    path_split_data::all(
      "a",
      { 0, "" },
      { 1, "a" },
      true
    ),
    path_split_data::all(
      "filename.ext",
      { 0, "" },
      { 12, "filename.ext" },
      true
    ),
    path_split_data::all(
      "input/filename.ext",
      { 5, "input" },
      { 12, "filename.ext" },
      true
    ),
    path_split_data::all(
      "input\\filename.ext",
      { 5, "input" },
      { 12, "filename.ext" },
      true
    ),
    path_split_data::all(
      "input/",
      { 5, "input" },
      { 0, "" },
      true
    ),
    path_split_data::all(
      "input\\",
      { 5, "input" },
      { 0, "" },
      true
    ),
    path_split_data::posixroot(
      "C:\\Users\\MegaZeux\\Desktop\\MegaZeux\\Zeux\\Caverns\\CAVERNS.MZX",
      { 47, "C:/Users/MegaZeux/Desktop/MegaZeux/Zeux/Caverns" },
      { 47, "C:\\Users\\MegaZeux\\Desktop\\MegaZeux\\Zeux\\Caverns" },
      { 46, "C:Users/MegaZeux/Desktop/MegaZeux/Zeux/Caverns" },
      { 11, "CAVERNS.MZX" },
      true
    ),
    path_split_data::posixroot(
      u8"/home/\u00C8śŚ/megazeux/DE/DE_START.MZX",
      { 24, u8"/home/\u00C8śŚ/megazeux/DE" },
      { 24, u8"\\home\\\u00C8śŚ\\megazeux\\DE" },
      { 12, "DE_START.MZX" },
      true
    ),
    path_split_data::posixroot(
      u8"/home/ćçáö/megazeux/DE/saved.\u00C8śŚ.sav",
      { 26, u8"/home/ćçáö/megazeux/DE" },
      { 26, u8"\\home\\ćçáö\\megazeux\\DE" },
      { 16, u8"saved.\u00C8śŚ.sav" },
      true
    ),
    path_split_data::posixroot(
      "/",
      { 1, "/" },
      { 1, "\\" },
      { 0, "" },
      true
    ),
    path_split_data::posixdosroot(
      "C:\\",
      { 2, "C:" },
      { 3, "C:/" },
      { 3, "C:\\" },
      { 2, "C:" },
      { 0, "" },
      true
    ),
    path_split_data::posixroot(
      "/sdfjklfdjdskfdsfgdfsggdfgdfgfdgsgdfgfdgfgg",
      { 1, "/" },
      { 1, "\\" },
      { 42, "sdfjklfdjdskfdsfgdfsggdfgdfgfdgsgdfgfdgfgg" },
      true
    ),
    path_split_data::posixdosroot(
      "C:\\sdfjklfdjdskfdsfgdfsggdfgdfgfdgsgdfgfdgfgg",
      { 2, "C:" },
      { 3, "C:/" },
      { 3, "C:\\" },
      { 2, "C:" },
      { 42, "sdfjklfdjdskfdsfgdfsggdfgdfgfdgsgdfgfdgfgg" },
      true
    ),
    path_split_data::posixdosroot(
      "C://sdfjklf",
      { 4, "C://" },
      { 3, "C:/" },
      { 3, "C:\\" },
      { 2, "C:" },
      { 7, "sdfjklf" },
      true
    ),
    // Internally all of these functions may stat the provided directory to
    // determine how much of it is/isn't a path. These paths all assume that
    // a directory stat succeeds for the input path.
    path_split_data::all(
      PATH_DIR_EXISTS,
      { 15, PATH_DIR_EXISTS },
      { 0, "" },
      true
    ),
    path_split_data::all(
      PATH_DIR_EXISTS_2,
      { 31, PATH_DIR_EXISTS_2 },
      { 0, "" },
      true
    ),
  };
  char dir_buffer[MAX_PATH];
  char file_buffer[MAX_PATH];
  ssize_t result;

  SECTION(path_has_directory)
  {
    for(const path_split_data &d : data)
    {
      boolean result = path_has_directory(d.path);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value > 0, "%s", d.path);
    }
  }

  SECTION(path_to_directory)
  {
    for(const path_split_data &d : data)
    {
      snprintf(dir_buffer, MAX_PATH, "%s", d.path);
      dir_buffer[MAX_PATH - 1] = '\0';

      result = path_to_directory(dir_buffer, MAX_PATH);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result >= 0 && d.PATH_SELECTOR.result)
        ASSERTCMP(dir_buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_get_directory)
  {
    for(const path_split_data &d : data)
    {
      result = path_get_directory(dir_buffer, MAX_PATH, d.path);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result >= 0 && d.PATH_SELECTOR.result)
        ASSERTCMP(dir_buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_to_filename)
  {
    for(const path_split_data &d : data)
    {
      snprintf(file_buffer, MAX_PATH, "%s", d.path);
      file_buffer[MAX_PATH - 1] = '\0';

      result = path_to_filename(file_buffer, MAX_PATH);
      ASSERTEQ(result, d.filename.return_value, "%s", d.path);
      if(result >= 0 && d.filename.result)
        ASSERTCMP(file_buffer, d.filename.result, "%s", d.path);
    }
  }

  SECTION(path_get_filename)
  {
    for(const path_split_data &d : data)
    {
      result = path_get_filename(file_buffer, MAX_PATH, d.path);
      ASSERTEQ(result, d.filename.return_value, "%s", d.path);
      if(result >= 0 && d.filename.result)
        ASSERTCMP(file_buffer, d.filename.result, "%s", d.path);
    }
  }

  SECTION(path_get_directory_and_filename)
  {
    boolean result;

    for(const path_split_data &d : data)
    {
      result = path_get_directory_and_filename(
       dir_buffer, MAX_PATH, file_buffer, MAX_PATH, d.path);
      ASSERTEQ(result, d.dir_and_file_return_value, "%s", d.path);
      if(result)
      {
        if(d.PATH_SELECTOR.result)
          ASSERTCMP(dir_buffer, d.PATH_SELECTOR.result, "%s", d.path);

        if(d.filename.result)
          ASSERTCMP(file_buffer, d.filename.result, "%s", d.path);
      }
    }
  }

  SECTION(path_get_parent)
  {
    for(auto &d : data)
    {
      // Special: the tests that expect stat() are different.
      if(!strcmp(d.path, PATH_DIR_EXISTS))
      {
        result = path_get_parent(dir_buffer, MAX_PATH, PATH_DIR_EXISTS);
        ASSERTEQ(result, 0, "%s", PATH_DIR_EXISTS);
        continue;
      }
      if(!strcmp(d.path, PATH_DIR_EXISTS_2))
      {
        result = path_get_parent(dir_buffer, MAX_PATH, PATH_DIR_EXISTS_2);
        ASSERTEQ(result, strlen(PATH_DIR_EXISTS), "%s", PATH_DIR_EXISTS_2);
        ASSERTCMP(dir_buffer, PATH_DIR_EXISTS, "%s", PATH_DIR_EXISTS_2);
        continue;
      }

      // The rest behave exactly like path_get_directory.
      result = path_get_parent(dir_buffer, MAX_PATH, d.path);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result >= 0 && d.PATH_SELECTOR.result)
        ASSERTCMP(dir_buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }
}


struct path_target_output
{
  const char *path;
  const char *target;
  struct path_return_result posix;    /* POSIX with added root:// roots */
  struct path_return_result posixdos; /* POSIX with added root:/ roots (console SDKs) */
  struct path_return_result dos;      /* DOS */
  struct path_return_result win32;    /* DOS with UNC roots */
  struct path_return_result amiga;    /* Amiga */

  static constexpr struct path_target_output all(const char *path,
   const char *target, struct path_return_result any)
  {
    return path_target_output{ path, target, any, any, any, any, any };
  }

  static constexpr struct path_target_output posixroot(const char *path,
   const char *target, struct path_return_result posix,
   struct path_return_result dos)
  {
    return path_target_output{ path, target, posix, posix, dos, dos, posix };
  }

  static constexpr struct path_target_output posixroot(const char *path,
   const char *target, struct path_return_result posix,
   struct path_return_result dos, struct path_return_result amiga)
  {
    return path_target_output{ path, target, posix, posix, dos, dos, amiga };
  }

  static constexpr struct path_target_output posixdosroot(const char *path,
   const char *target, struct path_return_result posix,
   struct path_return_result posixdos, struct path_return_result dos,
   struct path_return_result amiga)
  {
    return path_target_output{ path, target, posix, posixdos, dos, dos, amiga };
  }

  static constexpr struct path_target_output uncroot(const char *path,
   const char *target, struct path_return_result posix,
   struct path_return_result dos, struct path_return_result win32,
   struct path_return_result amiga)
  {
    return path_target_output{ path, target, posix, posix, dos, win32, amiga };
  }
};

UNITTEST(path_append_and_path_join)
{
  static constexpr const path_target_output data[] =
  {
    path_target_output::all(
      "",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "/a/base",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "",
      "a/target",
      { -1, nullptr }
    ),
    path_target_output::posixroot(
      "/",
      "awa",
      { 4, "/awa" },
      { 4, "\\awa" }
    ),
    path_target_output::posixroot(
      "/base/path",
      "a/target.ext",
      { 23, "/base/path/a/target.ext" },
      { 23, "\\base\\path\\a\\target.ext" }
    ),
    path_target_output::posixroot(
      "/do/not/duplicate/",
      "this/slash",
      { 28, "/do/not/duplicate/this/slash" },
      { 28, "\\do\\not\\duplicate\\this\\slash" }
    ),
    path_target_output::posixroot(
      "/loool/",
      "loool/",
      { 12, "/loool/loool" },
      { 12, "\\loool\\loool" }
    ),
    path_target_output::posixroot(
      "C:\\dos\\path",
      "to\\join",
      { 19, "C:/dos/path/to/join" },
      { 19, "C:\\dos\\path\\to\\join" },
      { 18, "C:dos/path/to/join" }
    ),
    path_target_output::posixroot(
      "some:amiga/nonsense///",
      "augh",
      { 24, "some:amiga/nonsense/augh" },
      { 24, "some:amiga\\nonsense\\augh" },
      { 26, "some:amiga/nonsense///augh" }
    ),
  };
  // Assume a buffer size of 32 for these.
  static constexpr const path_target_output small_data[] =
  {
    path_target_output::posixroot(
      "/should/barely/fit",
      "this/string",
      { 30, "/should/barely/fit/this/string" },
      { 30, "\\should\\barely\\fit\\this\\string" }
    ),
    path_target_output::posixroot(
      "C:/an/exact/fit",
      "and/should/pass",
      { 31, "C:/an/exact/fit/and/should/pass" },
      { 31, "C:\\an\\exact\\fit\\and\\should\\pass" },
      { 30, "C:an/exact/fit/and/should/pass" }
    ),
    path_target_output::all(
      "/should/not/be/able/to/fit",
      "these/strings",
      { -1, nullptr }
    ),
    path_target_output::all(
      "C:\\wow\\this\\path\\is\\kinda\\very\\long\\",
      "whatever",
      { -1, nullptr }
    ),
    path_target_output::all(
      "C:\\",
      "wtf\\is\\wrong\\with\\you\\no\\seriously",
      { -1, nullptr }
    ),
  };

  char buffer[MAX_PATH];
  ssize_t result;

  SECTION(path_append_NormalCases)
  {
    for(const path_target_output &d : data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_append(buffer, MAX_PATH, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_append_SmallBufferCases)
  {
    for(const path_target_output &d : small_data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_append(buffer, 32, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
      {
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
      }
      else
        ASSERTCMP(buffer, d.path, "");
    }
  }

  SECTION(path_join_NormalCases)
  {
    for(const path_target_output &d : data)
    {
      result = path_join(buffer, MAX_PATH, d.path, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_join_SmallBufferCases)
  {
    for(const path_target_output &d : small_data)
    {
      static const char *def = "DO NOT MODIFY";
      snprintf(buffer, MAX_PATH, "%s", def);

      result = path_join(buffer, 32, d.path, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
      {
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
      }
      else
        ASSERTCMP(buffer, def, "%s", d.path);
    }
  }
}


UNITTEST(path_remove_prefix)
{
  static constexpr const path_target_output data[] =
  {
    path_target_output::all(
      "",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "valid path",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "",
      "valid prefix",
      { -1, nullptr }
    ),
    path_target_output::all(
      "/some/path/here/with/an/invalid/prefix",
      "/some/p",
      { -1, nullptr }
    ),
    path_target_output::all(
      "/some/regular/path",
      "/some/regular/path/except/the/prefix/is/really/long",
      { -1, nullptr }
    ),
    path_target_output::all(
      "C:\\dont\\mix\\root\\styles",
      "\\dont\\mix\\root",
      { -1, nullptr }
    ),
    path_target_output::all(
      "/some/path/here",
      "/some/path",
      { 4, "here" }
    ),
    path_target_output::posixroot(
      "/some/prefix/some/path/here",
      "/some/prefix/",
      { 14, "some/path/here" },
      { 14, "some\\path\\here" }
    ),
    path_target_output::all(
      "C:\\a\\dos\\style\\prefixed\\path",
      "C:\\a\\dos\\style\\prefixed",
      { 4, "path" }
    ),
    path_target_output::posixroot(
      "work\\on\\relative\\paths\\too\\thanks",
      "work\\on\\relative\\",
      { 16, "paths/too/thanks" },
      { 16, "paths\\too\\thanks" }
    ),
    path_target_output::all(
      "consume/all/slashes////////////////////////////////////thanks",
      "consume/all/slashes",
      { 6, "thanks" }
    ),
    path_target_output::all(
      "/allow/mixed/slash/styles",
      "\\allow/mixed\\slash",
      { 6, "styles" }
    ),
    path_target_output::all(
      "merge//prefix\\\\slashes//////thanks",
      "merge/\\//\\\\prefix///////////\\slashes",
      { 6, "thanks" }
    ),
  };
  char buffer[MAX_PATH];
  ssize_t result;

  SECTION(NoPrefixLength)
  {
    for(const path_target_output &d : data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_remove_prefix(buffer, MAX_PATH, d.target, 0);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result >= 0)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
      else
        ASSERTCMP(buffer, d.path, "");
    }
  }

  SECTION(PrefixLength)
  {
    for(const path_target_output &d : data)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_remove_prefix(buffer, MAX_PATH, d.target,
       strlen(d.target));
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result >= 0)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
      else
        ASSERTCMP(buffer, d.path, "");
    }
  }
}


struct path_safe_output
{
  const char *path;
  int result_mask;
};

UNITTEST(path_safety_check)
{
  static constexpr const struct path_safe_output data[] =
  {
    /* Should not be rejected by any of the checks. */
    { "", 0 },
    { "a", 0 },
    { "01", 0 },
    { "filename.ext", 0 },
    { "longfilename.longext", 0, },
    { "longfi~1.lon", 0, },
    { ".", 0, },
    { "./duhh", 0, },
    { "why/./did/./you/./put\\.\\this/./in/./your/./game\\", 0, },
    { "..wat", 0, },
    { "samd..wich", 0, },
    { "awful../", 0, },
    { "a/..crime/", 0, },
    { "another../crime", 0, },
    { "its/allowed..", 0, },
    { "...", 0, },
    { "........", 0, },
    { "con5", 0, },
    { "auxx", 0, },
    { "null", 0, },
    { "hello.lpt5.txt", 0, },
    { ".aux", 0, },
    { "hello.prn", 0, },
    { "com", 0, },
    { "lpt", 0, },
    { "com#", 0, },
    { "lpt$", 0, },

    /* Unix root check */
    {
      "/hellow",
      PATH_SAFE_UNIX_ROOT,
    },
    {
      "\\mealso",
      PATH_SAFE_UNIX_ROOT,
    },
    /* DOS/Amiga root check (reject anything containing ':')
     * This check is fully overlapped by PATH_SAFE_DOS_CHARACTER */
    {
      "C:/dosroot",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      "C:\\WINDOWS\\SYSTEM32\\SVCHOST.EXE",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      "this:too",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      "amiga/can\\do:this",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      ":also/amiga",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      "/twoofthem:",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_DOS_ROOT | PATH_SAFE_DOS_CHARACTER,
    },
    /* Unix parent check (".." at start/end or surrounded by slashes) */
    {
      "..",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "..\\",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "loololol/../WINDOWS/SYSTEM32/BONER.DLL",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "../lol",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "not\\here\\either\\..",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "......./..",
      PATH_SAFE_UNIX_PARENT,
    },
    {
      "/..",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_UNIX_PARENT,
    },
    {
      "C:\\..\\a",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_UNIX_PARENT | PATH_SAFE_DOS_CHARACTER,
    },
    /* Amiga parent check (any double-slashes) */
    {
      "escape///sandbox//",
      PATH_SAFE_AMIGA_PARENT
    },
    {
      "\\\\unc\\localhost\\top\\seekrit",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_AMIGA_PARENT,
    },
    {
      "\\\\.\\c:\\windows\\system32\\lsass.exe",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_DOS_ROOT | PATH_SAFE_AMIGA_PARENT |
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "//?/c:/windows/system32/lsass.exe",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_DOS_ROOT | PATH_SAFE_AMIGA_PARENT |
      PATH_SAFE_DOS_CHARACTER,
    },
    /* DOS reserved characters (NTFS/VFAT/exFAT only). */
    {
      "\"hewwo\"",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "a*_path_finder.txt",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "<duhhh",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "invalid>",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "hope no one uses this??",
      PATH_SAFE_DOS_CHARACTER
    },
    {
      "ugh|hhh",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "this i guess\r\n",
      PATH_SAFE_DOS_CHARACTER,
    },
    {
      "/barf/../o//matiq\x1f",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_UNIX_PARENT |
      PATH_SAFE_AMIGA_PARENT | PATH_SAFE_DOS_CHARACTER,
    },
    {
      "com\x02",
      PATH_SAFE_DOS_CHARACTER,
    },
    /* DOS devices in final tokens. */
    { "AUX",  PATH_SAFE_DOS_DEVICE, },
    { "aux",  PATH_SAFE_DOS_DEVICE, },
    { "cOn",  PATH_SAFE_DOS_DEVICE, },
    { "nul",  PATH_SAFE_DOS_DEVICE, },
    { "prn",  PATH_SAFE_DOS_DEVICE, },
    { "com1", PATH_SAFE_DOS_DEVICE, },
    { "com2", PATH_SAFE_DOS_DEVICE, },
    { "com3", PATH_SAFE_DOS_DEVICE, },
    { "com4", PATH_SAFE_DOS_DEVICE, },
    { "com5", PATH_SAFE_DOS_DEVICE, },
    { "com6", PATH_SAFE_DOS_DEVICE, },
    { "com7", PATH_SAFE_DOS_DEVICE, },
    { "com8", PATH_SAFE_DOS_DEVICE, },
    { "com9", PATH_SAFE_DOS_DEVICE, },
    { "com\xb9", PATH_SAFE_DOS_DEVICE, },
    { "com\xb2", PATH_SAFE_DOS_DEVICE, },
    { "com\xb3", PATH_SAFE_DOS_DEVICE, },
    { "lpt1", PATH_SAFE_DOS_DEVICE, },
    { "lpt2", PATH_SAFE_DOS_DEVICE, },
    { "lpt3", PATH_SAFE_DOS_DEVICE, },
    { "lpt4", PATH_SAFE_DOS_DEVICE, },
    { "lpt5", PATH_SAFE_DOS_DEVICE, },
    { "lpt6", PATH_SAFE_DOS_DEVICE, },
    { "lpt7", PATH_SAFE_DOS_DEVICE, },
    { "lpt8", PATH_SAFE_DOS_DEVICE, },
    { "lpt9", PATH_SAFE_DOS_DEVICE, },
    { "lpt\xb9", PATH_SAFE_DOS_DEVICE, },
    { "lpt\xb2", PATH_SAFE_DOS_DEVICE, },
    { "lpt\xb3", PATH_SAFE_DOS_DEVICE, },
    {
      "fat://com4",
      PATH_SAFE_DOS_ROOT | PATH_SAFE_AMIGA_PARENT |
      PATH_SAFE_DOS_CHARACTER | PATH_SAFE_DOS_DEVICE,
    },
    {
      "/home/awawa/lpt1",
      PATH_SAFE_UNIX_ROOT | PATH_SAFE_DOS_DEVICE,
    },
    {
      "xxydata/m/con.ogg",
      PATH_SAFE_DOS_DEVICE,
    },
    {
      "dagamezone\\aux.tar.gz",
      PATH_SAFE_DOS_DEVICE,
    },
    {
      "lpt7.sdjfsdjfds.sdjgjfdkhdfh.yes.really.txt",
      PATH_SAFE_DOS_DEVICE,
    },
    /* DOS devices in directory tokens.
     * These only seem to cause issues when accessed as files, but check and
     * reject them anyway to discourage Linux users from naming directories.
     */
    {
      "prn/some_files",
      PATH_SAFE_DOS_DEVICE,
    },
    {
      "com2\\yep\\reject\\this",
      PATH_SAFE_DOS_DEVICE
    },
    {
      "maybe/com1/this/should/fail",
      PATH_SAFE_DOS_DEVICE,
    },
    {
      "tiresome\\tbh\\nul.sdfjsdkljgfdlgfgm.b.cv.bvc.bkwke..bvc\\hewo.txt",
      PATH_SAFE_DOS_DEVICE,
    },
  };

  const auto &do_tests = [&](int testvals)
  {
    enum path_safe_mask result;

    for(const path_safe_output &d : data)
    {
      int shared = testvals & d.result_mask;
      /* Any path that would flag multiple issues should
       * always return the lowest-order flag. */
      enum path_safe_mask expected = shared ?
       static_cast<enum path_safe_mask>(shared & -shared) : PATH_SAFE_OK;

      result = path_safety_check(d.path, testvals);
      ASSERTEQ(result, expected, "%s: %04xh & %04xh", d.path, (int)result, d.result_mask);
    }
  };

  SECTION(UnixRoot)
    do_tests(PATH_SAFE_UNIX_ROOT);

  SECTION(DosRoot)
    do_tests(PATH_SAFE_DOS_ROOT);

  SECTION(AnyRoot)
    do_tests(PATH_SAFE_ANY_ROOT);

  SECTION(UnixParent)
    do_tests(PATH_SAFE_UNIX_PARENT);

  SECTION(AmigaParent)
    do_tests(PATH_SAFE_AMIGA_PARENT);

  SECTION(AnyRootParent)
    do_tests(PATH_SAFE_ANY_ROOT | PATH_SAFE_UNIX_PARENT | PATH_SAFE_AMIGA_PARENT);

  SECTION(DosCharacter)
    do_tests(PATH_SAFE_DOS_CHARACTER);

  SECTION(DosDevice)
    do_tests(PATH_SAFE_DOS_DEVICE);

  SECTION(Any)
    do_tests(PATH_SAFE_ANY);

  SECTION(path_safety_strerror)
  {
    size_t i;
    size_t j;

    for(i = 0; i < PATH_SAFE_ANY * 4; i++)
    {
      const char *ret = path_safety_strerror(static_cast<enum path_safe_mask>(i));

      if(i == 0)
      {
        ASSERTEQ(ret, path_safety_strerror(PATH_SAFE_OK), "");
        continue;
      }
      for(j = 1; j < PATH_SAFE_ANY; j <<= 1)
      {
        if(~i & j)
          continue;

        ASSERTEQ(ret, path_safety_strerror(static_cast<enum path_safe_mask>(j)),
         "%zx and %zx should get the same error string", i, j);
        break;
      }
      if(j >= PATH_SAFE_ANY)
        ASSERTCMP(ret, "unknown", "");
    }
  }
}


UNITTEST(path_navigate)
{
  static constexpr const path_target_output no_check[] =
  {
    path_target_output::all(
      "",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "lol",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      "",
      "lol",
      { -1, nullptr }
    ),
    path_target_output::posixdosroot(
      "/abc",
      "malformed/root:/path/",
      { 25, "/abc/malformed/root:/path" },
      { -1, nullptr },
      { -1, nullptr },
      { 19, "malformed/root:path"}
    ),
    path_target_output::posixroot(
      "/start/path",
      "relative/target",
      { 27, "/start/path/relative/target" },
      { 27, "\\start\\path\\relative\\target" }
    ),
    path_target_output::posixroot(
      "/",
      "hello",
      { 6, "/hello" },
      { 6, "\\hello" }
    ),
    path_target_output::posixroot(
      "C:\\",
      "a",
      { 4, "C:/a" },
      { 4, "C:\\a" },
      { 3, "C:a" }
    ),
    path_target_output::posixroot(
      "/some/path",
      "..",
      { 5, "/some" },
      { 5, "\\some" },
      { 13, "/some/path/.." }
    ),
    path_target_output::posixroot(
      "/",
      "..",
      { 1, "/" },
      { 1, "\\" },
      { 3, "/.." }
    ),
    path_target_output::posixroot(
      "/another/path",
      "./../path/../../../another/./",
      { 8, "/another" },
      { 8, "\\another" },
      { 42, "/another/path/./../path/../../../another/." }
    ),
    path_target_output::posixroot(
      "/start/path",
      "/an/absolute/path",
      { 17, "/an/absolute/path" },
      { 17, "\\an\\absolute\\path" },
      { 23, "/start/an/absolute/path" }
    ),
    path_target_output::posixroot(
      "jdflkjsdlfjksdklfjsdlksjdfklsd",
      "\\also\\an\\absolute\\path",
      { 22, "/also/an/absolute/path" },
      { 22, "\\also\\an\\absolute\\path" },
      { 52, "jdflkjsdlfjksdklfjsdlksjdfklsd/also/an/absolute/path" }
    ),
    path_target_output::posixdosroot(
      "C:\\start\\path",
      "D:\\folder",
      { 23, "C:/start/path/D:/folder" },
      { 9, "D:/folder" },
      { 9, "D:\\folder" },
      { 8, "D:folder" }
    ),
    path_target_output::posixdosroot(
      "/some/directory",
      "C:",
      { 18, "/some/directory/C:" },
      { 3, "C:/" },
      { 3, "C:\\" },
      { 2, "C:" }
    ),
    path_target_output::posixdosroot(
      "C:\\\\start\\path",
      "D:\\\\folder",
      { 10, "D://folder" },
      { 9, "D:/folder" },
      { 9, "D:\\folder" },
      { 8, "D:folder" }
    ),
    path_target_output::posixdosroot(
      "/some/directory2",
      "C://",
      { 4, "C://" },
      { 3, "C:/" },
      { 3, "C:\\" },
      { 2, "C:" }
    ),
    path_target_output::posixdosroot(
      "ahhkillme://",
      "..",
      { 12, "ahhkillme://" },
      { 11, "ahhkillme:/" },
      { 11, "ahhkillme:\\" },
      { 12, "ahhkillme:.." }
    ),
    path_target_output::posixroot(
      "/cwd",
      "mix\\up/some\\of/these\\slashes/lol",
      { 37, "/cwd/mix/up/some/of/these/slashes/lol" },
      { 37, "\\cwd\\mix\\up\\some\\of\\these\\slashes\\lol" }
    ),
    path_target_output::posixroot(
      "/skdlfjlskdjfklsd/",
      "i/am\\sure/..\\someone/relies\\..\\on/this",
      { 38, "/skdlfjlskdjfklsd/i/am/someone/on/this" },
      { 38, "\\skdlfjlskdjfklsd\\i\\am\\someone\\on\\this" },
      { 56, "/skdlfjlskdjfklsd/i/am/sure/../someone/relies/../on/this" }
    ),
    path_target_output::posixroot(
      "/.yeah/.actually/.dotfiles/.should/.work",
      "../../.work",
      { 32, "/.yeah/.actually/.dotfiles/.work" },
      { 32, "\\.yeah\\.actually\\.dotfiles\\.work" },
      { 52, "/.yeah/.actually/.dotfiles/.should/.work/../../.work" }
    ),
    path_target_output::posixroot(
      "look/more/nonsense",
      ".../lol",
      { 26, "look/more/nonsense/.../lol" },
      { 26, "look\\more\\nonsense\\...\\lol"}
    ),
    // Amiga-style parent directory navigation
    path_target_output::posixroot(
      "sys:software/octamed",
      "/",
      { 1, "/" },
      { 1, "\\" },
      { 12, "sys:software" }
    ),
    path_target_output::posixroot(
      "a1200:da_megazeux_zone",
      "/",
      { 1, "/" },
      { 1, "\\" },
      { 6, "a1200:" }
    ),
    path_target_output::posixroot(
      "fd0:this/is/pretty/horrible/tbh",
      "aaaa//wtf/awful//////working",
      { 54, "fd0:this/is/pretty/horrible/tbh/aaaa/wtf/awful/working" },
      { 54, "fd0:this\\is\\pretty\\horrible\\tbh\\aaaa\\wtf\\awful\\working" },
      { 19, "fd0:this/is/working" }
    ),
    path_target_output::posixdosroot(
      "daharddisk:ok/surely/this/will/be/normal",
      ":",
      { 42, "daharddisk:ok/surely/this/will/be/normal/:" },
      { -1, nullptr },
      { -1, nullptr },
      { 1, ":" }
    ),
    path_target_output::posixdosroot(
      "awawa:hello/world",
      ":Software",
      { 27, "awawa:hello/world/:Software" },
      { -1, nullptr },
      { -1, nullptr },
      { 9, ":Software" }
    ),
    path_target_output::posixroot(
      "cant:go/past/root",
      "//////",
      { 1, "/" },
      { 1, "\\" },
      { 5, "cant:" }
    ),
    // Windows UNC paths
    path_target_output::uncroot(
      "\\\\.\\C:",
      "Program Files",
      { 19, "/./C:/Program Files" },
      { -1, nullptr },
      { 20, "\\\\.\\C:\\Program Files" },
      { 19, "\\\\.\\C:Program Files" }
    ),
    path_target_output::uncroot(
      "\\\\localhost\\share\\folder",
      "..",
      { 16, "/localhost/share" },
      { 16, "\\localhost\\share" },
      { 18, "\\\\localhost\\share\\" },
      { 27, "//localhost/share/folder/.." }
    ),
    path_target_output::uncroot(
      "\\\\?\\unc\\localhost\\c$\\",
      "whymustyoudothis/..",
      { 19, "/?/unc/localhost/c$" },
      { 19, "\\?\\unc\\localhost\\c$" },
      { 21, "\\\\?\\unc\\localhost\\c$\\" },
      { 40, "//?/unc/localhost/c$/whymustyoudothis/.." }
    ),
    path_target_output::uncroot(
      "\\\\.\\C:\\nope\\",
      "..\\..",
      { 2, "/." },
      { 2, "\\." },
      { 7, "\\\\.\\C:\\" },
      { 16, "\\\\.\\C:nope/../.." }
    ),
    path_target_output::uncroot(
      "\\\\127.0.0.1\\why",
      "..",
      { 10, "/127.0.0.1" },
      { 10, "\\127.0.0.1" },
      { 16, "\\\\127.0.0.1\\why\\" },
      { 18, "//127.0.0.1/why/.." }
    ),
  };
  static const path_target_output with_check[] =
  {
    path_target_output::all(
      "",
      "",
      { -1, nullptr }
    ),
    path_target_output::all(
      ".",
      PATH_DIR_NOT_EXISTS,
      { -1, nullptr }
    ),
    path_target_output::posixroot(
      ".",
      PATH_DIR_EXISTS,
      { 17, "./" PATH_DIR_EXISTS },
      { 17, ".\\" PATH_DIR_EXISTS },
      { -1, nullptr }
    ),
    path_target_output::posixroot(
      PATH_FILE_RECURSIVE,
      "..",
      { 18, PATH_DIR_RECURSIVE },
      { 18, PATH_DIR_RECURSIVE },
      { -1, nullptr }
    ),
    path_target_output::posixroot(
      PATH_FILE_RECURSIVE,
      "/",
      { 1, "/" },
      { 1, "\\" },
      { 18, PATH_DIR_RECURSIVE }
    ),
  };
  char buffer[MAX_PATH];
  ssize_t result;

  SECTION(path_navigate_no_check)
  {
    for(const path_target_output &d : no_check)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_navigate_no_check(buffer, MAX_PATH, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }

  SECTION(path_navigate)
  {
    for(const path_target_output &d : with_check)
    {
      snprintf(buffer, MAX_PATH, "%s", d.path);
      buffer[MAX_PATH - 1] = '\0';

      result = path_navigate(buffer, MAX_PATH, d.target);
      ASSERTEQ(result, d.PATH_SELECTOR.return_value, "%s", d.path);
      if(result && d.PATH_SELECTOR.result)
        ASSERTCMP(buffer, d.PATH_SELECTOR.result, "%s", d.path);
    }
  }
}


struct path_mkdir_data
{
  const char *path;
  enum path_create_error expected;
};

UNITTEST(path_create_parent_recursively)
{
  static const path_mkdir_data data[] =
  {
    // Parent does not exist, call succeeds.
    { PATH_DIR_RECURSIVE_3 DIR_SEPARATOR PATH_FILE_EXISTS,
      PATH_CREATE_SUCCESS },
    // No parent in the input path, call succeeds.
    { "config.txt", PATH_CREATE_SUCCESS },
    { "lol.cnf", PATH_CREATE_SUCCESS },
    { "megazeux.exe", PATH_CREATE_SUCCESS },
    // Parent exists, call succeeds.
    { PATH_FILE_RECURSIVE, PATH_CREATE_SUCCESS },
    // mkdir fails due to existing file.
    { PATH_FILE_RECURSIVE DIR_SEPARATOR PATH_DIR_RECURSIVE,
      PATH_CREATE_ERR_FILE_EXISTS },
    // TODO: PATH_CREATE_ERR_MKDIR_FAILED is returned when mkdir fails.
    // TODO: PATH_CREATE_ERR_STAT_ERROR is returned when stat fails.
  };

  for(const path_mkdir_data &cur : data)
  {
    enum path_create_error ret = path_create_parent_recursively(cur.path);
    ASSERTEQ(ret, cur.expected, "%s", cur.path);
  }
}
