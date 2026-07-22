/* MegaZeux
 *
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdint.h>

#include "../Unit.hpp"

#include "../../src/util.h"
#include "../../src/platform/log.h"
#include "../../src/network/Scoped.hpp"
#include "../../src/network/Manifest.hpp"

static const char *DATA_DIR = "unit/network/data";
static const char *TEMP_FILE = "DELETE_ME.txt";

class pushd
{
private:
  boolean success;
  char prev[MAX_PATH];

  pushd(const pushd &) {}

public:
  pushd(const char *new_dir)
  {
    trace("--UNIT-- attempting to chdir to %s.\n", new_dir);
    success = false;
    if(getcwd(prev, MAX_PATH))
      if(!chdir(new_dir))
        success = true;

    if(!success)
      FAIL("chdir failed");
  }

  ~pushd()
  {
    if(success)
    {
      trace("--UNIT-- attempting to chdir back to %s.\n", prev);
      chdir(prev);
    }
  }
};

struct manifestdata
{
  const char *filename;
  const char *value;
  size_t size;
  uint32_t sha256[8];
};

// The lines here should match filedata's contents.
// Intentionally using mixed line ends since either should be supported.
static const char manifest_str[] =
 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad 3 1.txt\n"
 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1 56 2.txt\r\n";

static const manifestdata filedata[]
{
  {
    "1.txt",
    "abc",
    3,
    {
      0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
      0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
    }
  },
  {
    "2.txt",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    56,
    {
      0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039,
      0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1
    }
  }
};

static const manifestdata dummy_file =
{
  "ascii.chr",
  nullptr,
  3584,
  {
    0x657ca658, 0x8b6bf729, 0xf0ed71a3, 0xd3c781a4,
    0x65bd30cd, 0xfb11eb5f, 0xc7e27594, 0xc0717ea1
  }
};

UNITTEST(ManifestEntry)
{
  SECTION(ManifestEntry)
  {
    for(const manifestdata &f : filedata)
    {
      ManifestEntry e(f.sha256, f.size, f.filename);
      ASSERTMEM(e.sha256, f.sha256, sizeof(f.sha256), "%s", f.filename);
      ASSERTEQ(e.size, f.size, "%s", f.filename);
      ASSERTCMP(e.name, f.filename, "");

      ManifestEntry e2(e);
      ASSERTMEM(e2.sha256, f.sha256, sizeof(f.sha256), "%s", f.filename);
      ASSERTEQ(e2.size, f.size, "%s", f.filename);
      ASSERTCMP(e2.name, f.filename, "");
    }
  }

  SECTION(operator=)
  {
    for(const manifestdata &f : filedata)
    {
      ManifestEntry e(dummy_file.sha256, dummy_file.size, dummy_file.filename);
      ManifestEntry e2(f.sha256, f.size, f.filename);
      e = e2;

      ASSERTMEM(e.sha256, f.sha256, sizeof(f.sha256), "%s", f.filename);
      ASSERTEQ(e.size, f.size, "%s", f.filename);
      ASSERTCMP(e.name, f.filename, "");
    }
  }

  SECTION(validate)
  {
    pushd ch(DATA_DIR);

    for(const manifestdata &f : filedata)
    {
      ManifestEntry e(f.sha256, f.size, f.filename);
      boolean valid = e.validate();
      ASSERT(valid, "%s", f.filename);
    }
  }

  SECTION(validate_filename)
  {
    static const char *valid[] =
    {
      "data/1.txt",
      "sdfjsdlkfjdskljfds/sdfjsdlkfjsdlkjfsdkl/sfdjfklsjdflksd",
      "aaaaaaaaaaa"
    };
    /* Internally, this should be path_safety_check with most/all flags. */
    static const char *invalid[] =
    {
      "..\\lol",
      "../lol",
      "totally/normal/path/../../../../../../../../../bootmgr",
      "..",
      "C:\\Windows\\System32",
      "C:/Windows/System32",
      "/dev",
      "\\dev",
      "amigaroot:lol",
      "amigaparent/////////System/Shell",
      "no>pe",
      "also<nope",
      "not|even",
      "\"nope\"",
      "? not a chance",
      "NO\r\n",
      "s\tahp",
      "n\x1f",
      "com1",
      "lpt1",
      "aux",
      "NUL",
      "prn",
      "con",
      "com9.chr",
      "lpt2.tar.gz",
      "com\xb2.html",
      "com7/default.chr",
      "awawa\\lpt\xb9",
    };

    for(const char *filename : valid)
      ASSERT(ManifestEntry::validate_filename(filename), "%s", filename);

    for(const char *filename : invalid)
      ASSERT(!ManifestEntry::validate_filename(filename), "%s", filename);
  }

  SECTION(create_from_file)
  {
    pushd ch(DATA_DIR);

    for(const manifestdata &f : filedata)
    {
      ScopedPtr<ManifestEntry> e = ManifestEntry::create_from_file(f.filename);
      ASSERT(e, "%s", f.filename);

      ASSERTMEM(e->sha256, f.sha256, sizeof(f.sha256), "%s", f.filename);
      ASSERTEQ(e->size, f.size, "%s", f.filename);
      ASSERTCMP(e->name, f.filename, "");
    }
  }
}

static void test_manifest(const Manifest &m, const char *comment)
{
  const ManifestEntry *e = m.first();
  for(const manifestdata &f : filedata)
  {
    ASSERT(e, "%s, %s", f.filename, comment);
    ASSERTMEM(e->sha256, f.sha256, sizeof(f.sha256), "%s, %s", f.filename, comment);
    ASSERTEQ(e->size, f.size, "%s, %s", f.filename, comment);
    ASSERTCMP(e->name, f.filename, "%s, %s", f.filename, comment);
    ASSERT(e->validate(), "%s, %s", f.filename, comment);
    e = e->next;
  }
  ASSERT(!e, "unexpected data at end of Manifest");
}

UNITTEST(Manifest)
{
  SECTION(create)
  {
    pushd ch(DATA_DIR);

    Manifest m;
    m.create("manifest.txt");
    test_manifest(m, "create(filename)");
    m.clear();
    ASSERT(!m.head, "clear failed");

    m.create(manifest_str, arraysize(manifest_str));
    test_manifest(m, "create(buffer, size)");

    Manifest m2;
    m2.create(m);
    test_manifest(m2, "create(const Manifest &)");
  }

  SECTION(append)
  {
    Manifest m1, m2;
    m1.create(manifest_str, arraysize(manifest_str));
    m2.create(manifest_str, arraysize(manifest_str));

    // append(Manifest &) consumes the source Manifest's data.
    m1.append(m2);
    ASSERT(!m2.head, "append failed to clear source Manifest");

    ManifestEntry *tmp =
     new ManifestEntry(dummy_file.sha256, dummy_file.size, dummy_file.filename);
    m1.append(tmp);

    const ManifestEntry *e = m1.first();
    for(int j = 0; j < 2; j++)
    {
      for(const manifestdata &f : filedata)
      {
        ASSERT(e, "%s", f.filename);
        ASSERTCMP(e->name, f.filename, "");
        e = e->next;
      }
    }

    ASSERT(e, "%s", dummy_file.filename);
    ASSERTCMP(e->name, dummy_file.filename, "");
    e = e->next;
    ASSERT(!e, "unexpected data at end of Manifest");
  }

  SECTION(write_to_file)
  {
    pushd ch(DATA_DIR);

    Manifest m;
    m.create(manifest_str, arraysize(manifest_str));
    m.write_to_file(TEMP_FILE);
    m.clear();
    ASSERT(!m.head, "clear failed");

    m.create(TEMP_FILE);
    test_manifest(m, "create(filename)");

    unlink(TEMP_FILE);
  }

  // NOTE: check_if_remote_exists requires networking.
  // NOTE: get_updates requires networking.
  // NOTE: download_and_replace_entry requires networking.
}
