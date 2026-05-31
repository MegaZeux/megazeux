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

#ifndef MEGAZEUX_MANIFEST_HPP
#define MEGAZEXU_MANIFEST_HPP

#include "../compat.h"
#include "../io/vfile.h"

#include "sha256.h"
#include "HTTPHost.hpp"

#include <stdint.h>

#define MANIFEST_TXT "manifest.txt"
#define LOCAL_MANIFEST_TXT MANIFEST_TXT
#define REMOTE_MANIFEST_TXT "manifest.remote.txt"

class UPDATER_LIBSPEC ManifestEntry
{
public:
  ManifestEntry *next;
  uint32_t sha256[8];
  size_t size;
  char *name;

  ManifestEntry(const uint32_t (&_sha256)[8], size_t _size, const char *name);
  ManifestEntry(const ManifestEntry &e);
  ManifestEntry &operator=(const ManifestEntry &e);
  ~ManifestEntry();

  /**
   * Check this manifest entry against its corresponding file.
   *
   * @param vf          File pointer to validate.
   *
   * @return `true` if the file exists and is an exact match, otherwise `false`.
   */
  boolean validate(vfile *vf) const;

  /**
   * Check this manifest entry against its corresponding file (indicated by
   * its `name` field).
   *
   * @return `true` if the file has a valid filename, exists, and matches,
   *         otherwise `false`.
   */
  boolean validate() const;

  /**
   * Filter invalid ManifestEntry filenames.
   *
   * @return `true` if the filename is a valid filename, otherwise `false`.
   */
  static boolean validate_filename(const char *filename);

  /**
   * Create a ManifestEntry from an existing file.
   *
   * @param filename    Filename of file to create ManifestEntry from.
   *
   * @return pointer to a new ManifestEntry object on success, otherwise null.
   */
  static ManifestEntry *create_from_file(const char *filename);

private:
  void init(const uint32_t (&_sha256)[8], size_t _size, const char *name);
  static boolean compute_sha256(SHA256_ctx &ctx, vfile *vf, size_t len);
};

class UPDATER_LIBSPEC Manifest
{
public:
  ManifestEntry *head;

private:
  Manifest(const Manifest &) {}
  Manifest &operator=(const Manifest &) { return *this; }
#ifdef IS_CXX_11
  Manifest(Manifest &&) {}
  Manifest &operator=(Manifest &&) { return *this; }
#endif

  void create(vfile *vf);
  HTTPHostStatus get_remote(HTTPHost &http, HTTPRequestInfo &request,
   const char *basedir);
  void filter_duplicates(Manifest &local, Manifest &remote);
  void filter_existing_files();

public:
  Manifest(): head(nullptr) {}
  ~Manifest();
  void clear();

  boolean create(const char *filename);
  void create(const void *data, size_t data_len);
  void create(const Manifest &src);

  /**
   * Move the contents of the provided manifest to the end of this manifest.
   * The provided manifest will be empty after this operation.
   *
   * @param src           Manifest to move the contents of.
   */
  void append(Manifest &src);

  /**
   * Append a ManifestEntry to the end of this manifest.
   * This operation will not duplicate the ManifestEntry or modify entry.next.
   *
   * @param entry         ManifestEntry to append to this Manifest.
   */
  void append(ManifestEntry *entry);

  boolean has_entries() const { return this->head != nullptr; }
  ManifestEntry *first() const { return this->head; }
  boolean write_to_file(const char *filename) const;

  static boolean check_if_remote_exists(HTTPHost &http, HTTPRequestInfo &request,
   const char *basedir);
  static HTTPHostStatus get_updates(HTTPHost &http, HTTPRequestInfo &request,
   const char *basedir, Manifest &removed, Manifest &replaced, Manifest &added);

  /**
   * Download the remote file represented by a manifest entry and replace the
   * local file if it exists. If the local file can't be deleted, it will be
   * added to the provided delete list to be deleted at a later time.
   *
   * @param http          Active connection to download from.
   * @param request       Request object to return response data in. Its
   *                      contents will be overwritten.
   * @param basedir       Remote basedir (/VERSION/PLATFORM).
   * @param e             Manifest entry to fetch remote file for.
   * @param delete_list   List to log file deletions to perform at a later time.
   *                      Files are only added to this if they could not be
   *                      deleted during the execution of this function.
   *
   * @return `true` on success, otherwise `false`.
   */
  static boolean download_and_replace_entry(HTTPHost &http,
   HTTPRequestInfo &request, const char *basedir, const ManifestEntry *e,
   Manifest &delete_list);
};

#endif /* MEGAZEUX_MANIFEST_HPP */
