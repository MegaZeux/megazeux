/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "Manifest.hpp"
#include "HTTPHost.hpp"
#include "Scoped.hpp"
#include "../util.h"
#include "../io/memfile.h"
#include "../io/path.h"
#include "../io/vio.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define BLOCK_SIZE    4096UL
#define LINE_BUF_LEN  256

void ManifestEntry::init(const uint32_t (&_sha256)[8], size_t _size,
 const char *name)
{
  size_t name_len = strlen(name);
  this->size = _size;
  for(size_t i = 0; i < ARRAY_SIZE(sha256); i++)
    this->sha256[i] = _sha256[i];

  this->name = new char[name_len + 1];
  memcpy(this->name, name, name_len + 1);
}

ManifestEntry::ManifestEntry(const uint32_t (&_sha256)[8], size_t _size,
 const char *name)
{
  this->init(_sha256, _size, name);
  this->next = nullptr;
}

ManifestEntry::ManifestEntry(const ManifestEntry &e)
{
  this->init(e.sha256, e.size, e.name);
  this->next = nullptr;
}

ManifestEntry &ManifestEntry::operator=(const ManifestEntry &e)
{
  delete[] this->name;
  this->init(e.sha256, e.size, e.name);
  return *this;
}

ManifestEntry::~ManifestEntry()
{
  delete[] this->name;
}

boolean ManifestEntry::compute_sha256(SHA256_ctx &ctx, vfile *vf, size_t len)
{
  unsigned long pos = 0;

  SHA256_init(&ctx);
  ScopedBuffer<char> block(BLOCK_SIZE);
  if(!block)
    return false;

  while(pos < len)
  {
    unsigned long block_size = MIN(BLOCK_SIZE, len - pos);

    if(vfread(block, block_size, 1, vf) != 1)
      return false;

    SHA256_update(&ctx, block, block_size);
    pos += block_size;
  }

  SHA256_final(&ctx);
  return true;
}

boolean ManifestEntry::validate(vfile *vf) const
{
  SHA256_ctx ctx;

  // It must be the same length
  if((size_t)vfilelength(vf, true) != this->size)
    return false;

  /* Compute the SHA256 digest for this file. Do it block-wise so as to
  * conserve RAM and scale to enormously large files.
  */
  if(!ManifestEntry::compute_sha256(ctx, vf, this->size))
    return false;

  // Verify the digest against the manifest
  if(memcmp(ctx.H, this->sha256, sizeof(uint32_t) * 8) != 0)
    return false;

  return true;
}

boolean ManifestEntry::validate() const
{
  // This should already have been checked but check it again anyway.
  if(!ManifestEntry::validate_filename(this->name))
    return false;

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(this->name, "rb");
  if(!vf)
    return false;

  return this->validate(vf);
}

boolean ManifestEntry::validate_filename(const char *filename)
{
  if(path_safety_check(filename, PATH_SAFE_ANY))
    return false;

  return true;
}

ManifestEntry *ManifestEntry::create_from_file(const char *filename)
{
  SHA256_ctx ctx;
  size_t size;

  if(!ManifestEntry::validate_filename(filename))
    return nullptr;

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(filename, "rb");
  if(!vf)
    return nullptr;

  size = vfilelength(vf, false);

  boolean ret = ManifestEntry::compute_sha256(ctx, vf, size);
  if(!ret)
    return nullptr;

  return new ManifestEntry(ctx.H, size, filename);
}

Manifest::~Manifest()
{
  this->clear();
}

void Manifest::clear()
{
  ManifestEntry *current = this->head;
  ManifestEntry *next;
  while(current)
  {
    next = current->next;
    delete current;
    current = next;
  }
  this->head = nullptr;
}

static boolean manifest_parse_sha256(const char *p, uint32_t (&sha256)[8])
{
  int i, j;

  // A SHA256 digest is a fixed length (prefix zeroes)
  if(strlen(p) != 8 * 8)
    return false;

  // Walk the sha256 "registers" and decompose 8byte hex chunks
  for(i = 0, j = 0; i < 8; i++, j += 8)
  {
    char reg_hex[9], *end_ptr;

    // Pinch an 8 byte hex block from source string
    memcpy(reg_hex, p + j, 8);
    reg_hex[8] = 0;

    // Convert the hex string into an integer and check for errors
    sha256[i] = (uint32_t)strtoul(reg_hex, &end_ptr, 16);
    if(end_ptr[0])
      return false;
  }

  return true;
}

static boolean manifest_parse_size(char *p, size_t *size)
{
  char *endptr;
  *size = strtoul(p, &endptr, 10);
  if(endptr[0])
    return false;
  return true;
}

static ManifestEntry *manifest_parse_line(char *buffer)
{
  if(buffer[0])
  {
    uint32_t sha256[8];
    size_t size;
    char *m = buffer, *line;

    // Grab the sha256 token
    line = strsep(&m, " ");
    if(!line)
      return nullptr;

    /* Break the 64 character (8x8) hex string down into
     * eight constituent 32bit SHA result registers (32*8=256).
     */
    if(!manifest_parse_sha256(line, sha256))
      return nullptr;

    // Grab the size token
    line = strsep(&m, " ");
    if(!line)
      return nullptr;

    // Validate and parse it
    if(!manifest_parse_size(line, &size))
      return nullptr;

    // Grab the filename token
    line = strsep(&m, "\n");
    if(!line)
      return nullptr;

    // Reject any invalid or insecure filenames.
    if(!ManifestEntry::validate_filename(line))
      return nullptr;

    return new ManifestEntry(sha256, size, line);
  }
  return nullptr;
}

void Manifest::create(vfile *vf)
{
  char buffer[LINE_BUF_LEN];
  boolean has_errors = false;
  ManifestEntry *current = nullptr;
  this->clear();

  // Walk the manifest line by line
  while(vfsafegets(buffer, LINE_BUF_LEN, vf))
  {
    ManifestEntry *next = manifest_parse_line(buffer);
    if(!next)
    {
      has_errors = true;
      continue;
    }

    if(current)
      current->next = next;
    else
      this->head = next;
    current = next;
  }

  if(has_errors)
    warn("Malformed manifest file.\n");
}

boolean Manifest::create(const char *filename)
{
  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(filename, "rb");
  if(!vf)
  {
    warn("Failed to open manifest file '%s'!\n", filename);
    return false;
  }

  this->create(vf);
  return true;
}

void Manifest::create(const void *data, size_t data_len)
{
  char buffer[LINE_BUF_LEN];
  ManifestEntry *current = nullptr;
  struct memfile mf;
  this->clear();

  mfopen(data, data_len, &mf);

  while(mfsafegets(buffer, LINE_BUF_LEN, &mf))
  {
    ManifestEntry *next = manifest_parse_line(buffer);
    if(!next)
      continue;

    if(current)
      current->next = next;
    else
      this->head = next;
    current = next;
  }
}

void Manifest::create(const Manifest &src)
{
  const ManifestEntry *src_current = src.head;
  ManifestEntry *current = nullptr;
  ManifestEntry *next;
  this->clear();

  while(src_current)
  {
    next = new ManifestEntry(*src_current);
    src_current = src_current->next;

    if(current)
      current->next = next;
    else
      this->head = next;
    current = next;
  }
}

void Manifest::append(Manifest &src)
{
  this->append(src.head);
  src.head = nullptr;
}

void Manifest::append(ManifestEntry *entry)
{
  if(entry)
  {
    ManifestEntry *tail = this->head;
    if(tail)
    {
      while(tail->next)
        tail = tail->next;

      tail->next = entry;
    }
    else
      this->head = entry;
  }
}

boolean Manifest::check_if_remote_exists(HTTPHost &http,
 HTTPRequestInfo &request, const char *basedir)
{
  HTTPHostStatus ret;

  trace("--MANIFEST-- Manifest::check_if_remote_exists\n");

  request.clear();
  snprintf(request.url, LINE_BUF_LEN, "%s/" MANIFEST_TXT, basedir);

  ret = http.head(request);
  if(ret != HOST_SUCCESS)
  {
    warn("Check for remote " MANIFEST_TXT " failed (code %d; error: %s)\n",
     request.status_code, HTTPHost::get_error_string(ret));
    request.print_response();
    return false;
  }
  trace("Check for remote " MANIFEST_TXT " successful, code %d\n",
   request.status_code);

  if(request.status_code == 200)
    return true;

  return false;
}

HTTPHostStatus Manifest::get_remote(HTTPHost &http, HTTPRequestInfo &request,
 const char *basedir)
{
  HTTPHostStatus ret;

  trace("--MANIFEST-- Manifest::get_remote\n");

  this->clear();
  request.clear();
  snprintf(request.url, LINE_BUF_LEN, "%s/" MANIFEST_TXT, basedir);
  request.allowed_types = HTTPRequestInfo::plaintext_types;

  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(REMOTE_MANIFEST_TXT, "w+b");
  if(!vf)
  {
    warn("Failed to open local " REMOTE_MANIFEST_TXT " for writing\n");
    return HOST_FWRITE_FAILED;
  }

  ret = http.get(request, vf);
  if(ret != HOST_SUCCESS)
  {
    warn("Processing " REMOTE_MANIFEST_TXT " failed (code %d; error: %s)\n",
     request.status_code, HTTPHost::get_error_string(ret));
    request.print_response();
    return ret;
  }

  vrewind(vf);
  this->create(vf);
  return ret;
}

/**
 * Filter duplicates between the two provided Manifests into this Manifest.
 * Duplicates from the local list will be deleted and duplicates from the
 * remote list will be moved into this Manifest.
 *
 * @param local     Manifest to filter (duplicates are deleted).
 * @param remote    Manifest to filter (duplicates are moved).
 */
void Manifest::filter_duplicates(Manifest &local, Manifest &remote)
{
  ManifestEntry *l, *prev_l, *next_l;
  ManifestEntry *tail = this->head;

  trace("--MANIFEST-- Manifest::filter_duplicates\n");

  // Skip to the end of this manifest.
  if(tail)
    while(tail->next)
      tail = tail->next;

  for(l = local.head, prev_l = nullptr; l; l = next_l)
  {
    ManifestEntry *r, *prev_r, *next_r;

    for(r = remote.head, prev_r = nullptr; r; r = next_r)
    {
      next_r = r->next;

      if(strcmp(l->name, r->name) == 0)
      {
        if(prev_r)
          prev_r->next = next_r;
        else
          remote.head = next_r;

        // Move duplicate element from the remote manifest to this manifest.
        r->next = nullptr;
        if(tail)
          tail->next = r;
        else
          this->head = r;
        tail = r;
        break;
      }
      else
        prev_r = r;
    }

    next_l = l->next;

    if(r)
    {
      if(prev_l)
        prev_l->next = next_l;
      else
        local.head = next_l;

      delete l;
    }
    else
      prev_l = l;
  }
}

void Manifest::filter_existing_files()
{
  ManifestEntry *prev = nullptr;
  ManifestEntry *e, *e_next;

  trace("--MANIFEST-- Manifest::filter_existing_files\n");

  for(e = this->head; e; e = e_next)
  {
    e_next = e->next;

    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(e->name, "rb");
    if(vf)
    {
      if(e->validate(vf))
      {
        trace("Local file '%s' matches the remote manifest; ignoring.\n", e->name);
        if(prev)
          prev->next = e_next;
        else
          this->head = e_next;

        delete e;
      }
      else
      {
        trace("Local file '%s' doesn't match the remote manifest entry.\n", e->name);
        prev = e;
      }
    }
    else
    {
      trace("Local file '%s' from remote manifest doesn't exist.\n", e->name);
      prev = e;
    }
  }
}

HTTPHostStatus Manifest::get_updates(HTTPHost &http, HTTPRequestInfo &request,
 const char *basedir, Manifest &removed, Manifest &replaced, Manifest &added)
{
  Manifest local, remote;
  HTTPHostStatus status;

  trace("--MANIFEST-- Manifest::get_updates\n");

  status = remote.get_remote(http, request, basedir);
  if(status != HOST_SUCCESS)
    return status;

  local.create(LOCAL_MANIFEST_TXT);
  replaced.clear();
  removed.clear();
  added.clear();

  if(local.has_entries())
  {
    /**
     * Filter duplicates out of both local and remote. The duplicates from
     * remote will be moved into the replaced list; the duplicates from local
     * will be deleted.
     */
    replaced.filter_duplicates(local, remote);

    /**
     * The local list is now the removed list and the remote list is now the
     * added list.
     */
    removed.append(local);
    added.append(remote);
  }
  else
  {
    // If there's no local manifest added vs. replaced can't really be
    // determined reliably. Just treat everything as replaced.
    replaced.append(remote);
  }

  /**
   * Check the duplicates from the remote manifest against their corresponding
   * files. If the file exists and matches the manifest entry, remove it. The
   * remaining list will contain files that need to be replaced.
   */
  replaced.filter_existing_files();
  return HOST_SUCCESS;
}

boolean Manifest::download_and_replace_entry(HTTPHost &http,
 HTTPRequestInfo &request, const char *basedir, const ManifestEntry *e,
 Manifest &delete_list)
{
  char buf[LINE_BUF_LEN];
  HTTPHostStatus ret;

  assert(e);

  /* Try to open our target file. If we can't open it, it might be
   * write protected or in-use. In this case, it may be possible to
   * rename the original file out of the way. Try this trick first.
   */
  ScopedFile<vfile, vfclose> vf = vfopen_unsafe(e->name, "w+b");
  if(!vf)
  {
    snprintf(buf, LINE_BUF_LEN, "%s-%u~", e->name, (unsigned int)time(NULL));
    if(vrename(e->name, buf))
    {
      warn("Failed to rename in-use file '%s' to '%s'\n", e->name, buf);
      return false;
    }

    delete_list.append(ManifestEntry::create_from_file(buf));

    vf.reset(vfopen_unsafe(e->name, "w+b"));
    if(!vf)
    {
      warn("Unable to open file '%s' for writing\n", e->name);
      return false;
    }
  }

  request.clear();
  request.allowed_types = HTTPRequestInfo::binary_types;
  snprintf(request.url, LINE_BUF_LEN, "%s/" SHA256_PRINT_FMT, basedir,
    e->sha256[0], e->sha256[1], e->sha256[2], e->sha256[3],
    e->sha256[4], e->sha256[5], e->sha256[6], e->sha256[7]);

  ret = http.get(request, vf);
  if(ret != HOST_SUCCESS)
  {
    warn("File '%s' could not be downloaded (code %d; error: %s)\n", e->name,
     request.status_code, HTTPHost::get_error_string(ret));
    return false;
  }

  vrewind(vf);
  if(!e->validate(vf))
  {
    warn("File '%s' failed validation\n", e->name);
    return false;
  }

  return true;
}

boolean Manifest::write_to_file(const char *filename) const
{
  if(this->head)
  {
    ScopedFile<vfile, vfclose> vf = vfopen_unsafe(filename, "ab");
    if(!vf)
      return false;

    for(ManifestEntry *e = this->head; e; e = e->next)
    {
      vf_printf(vf, SHA256_PRINT_FMT " %zu %s\n",
       e->sha256[0], e->sha256[1], e->sha256[2], e->sha256[3],
       e->sha256[4], e->sha256[5], e->sha256[6], e->sha256[7],
       e->size, e->name);
    }
  }
  return true;
}
