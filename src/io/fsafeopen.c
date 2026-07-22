/* MegaZeux
 *
 * Copyright (C) 2004-2005 Alistair Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "fsafeopen.h"
#include "path.h"
#include "vio.h"

#include "../platform/log.h"

#if defined(CONFIG_3DS)
// Enable hacks to reduce the number of filesystem operations on platforms
// with extremely slow filesystem access.
#define SLOW_FILESYSTEM_HACKS
#endif

/* As of Windows 7, short filenames can be disabled per-volume.
 * In Windows 8+ they are disabled by default except on the boot volume.
 * Github CI Windows runners now disable them too, so case5 must be enabled.
 */
//#ifndef _WIN32
#define ENABLE_DOS_COMPAT_TRANSLATIONS
//#endif

#ifdef ENABLE_DOS_COMPAT_TRANSLATIONS
#if defined(SLOW_FILESYSTEM_HACKS) || defined(_WIN32) || defined(CONFIG_DJGPP)
// Skip the extra all caps/lower case checks, either because the current
// platform will always be case-insensitive, or because they are slower than
// just scanning the directory.
#define NO_EXTRA_CASE_CHECKS
#endif

enum sfn_type
{
  NOT_AN_SFN,
  SFN,
  SFN_TRUNCATED,
};

#define SFN_BUFFER_LEN 13

/**
 * Determine if a given character is a valid SFN character.
 * NOTE: ~ will return false for this for the purposes of detecting truncated
 * SFNs. Spaces also cause this function to return false, as though they were
 * technically valid in the SFN, they were stripped in practice. Conversely,
 * lowercase letters will be treated as valid even though they technically
 * are not as these strings are going to be compared case-insensitively anyway.
 *
 * Valid SFN chars:     Space ! # $ % & ' ( ) - 0-9 @ A-Z ^ _ ` { } ~ \x80-\xFF
 * Invalid SFN chars:   \x00-\x1F a-z " * + , . / : ; < = > ? [ \ ] | \x7F
 *
 * https://en.wikipedia.org/wiki/8.3_filename
 *
 * @param  chr  Character to test.
 * @return      True if the character is a valid SFN character, otherwise false.
 */
static boolean is_sfn_char(unsigned char chr)
{
  static const unsigned char sfn_chars[256] =
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x1
    0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, // 0x2
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 0x3
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x4
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, // 0x5
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x6
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, // 0x7
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x8
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x9
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xA
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xB
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xC
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xD
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xE
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0xF
  };
  return sfn_chars[chr];
}

/**
 * Determine if an input filename is a valid SFN.
 *
 * @param  filename   Filename to check.
 * @param  check_len  Length of filename to check.
 * @return            "NOT_AN_SFN" if not an SFN; "SFN" if an SFN;
 *                    or "SFN_TRUNCATED" if an SFN in the format "NAME~1.EXT".
 */
static enum sfn_type is_sfn(const char *filename, size_t check_len)
{
  boolean is_truncated = false;
  size_t i;

  if(check_len < SFN_BUFFER_LEN)
  {
    for(i = 0; i < check_len; i++)
    {
      if(!is_sfn_char(filename[i]))
      {
        if(i <= 6 && filename[i] == '~')
        {
          size_t tilde_pos = i;
          i++;
          while(isdigit((unsigned char)filename[i]))
            i++;

          if(i > tilde_pos + 1 && (filename[i] == '.' || filename[i] == '\0'))
            is_truncated = true;
        }
        break;
      }
    }

    // The filename portion must be no longer than 8 chars...
    if(i > 8)
      return NOT_AN_SFN;

    if(i < check_len)
    {
      // These are the only valid terminators for the filename portion.
      if(filename[i] != '.' && filename[i] != '\0')
        return NOT_AN_SFN;

      if(filename[i++] == '.')
      {
        size_t j;
        for(j = i; j < check_len; j++)
          if(!is_sfn_char(filename[j]))
            break;

        // The extension portion must be no longer than 3 chars and must not
        // be terminated before the end of the string...
        if(j < check_len || (j - i) > 3)
          return NOT_AN_SFN;
      }
    }
    return is_truncated ? SFN_TRUNCATED : SFN;
  }
  return NOT_AN_SFN;
}

/**
 * Translate a given filename to an SFN. The provided buffer must be long
 * enough to hold an SFN. If the given filename is already an SFN, the filename
 * pointer will be returned; otherwise, the buffer pointer will be returned.
 *
 * @param  dest         Destination buffer.
 * @param  buffer_len   Length of destination buffer.
 * @param  filename     Filename to translate to an SFN.
 * @return              filename if already an SFN; dest on a successful
 *                      translation; NULL if an error occured.
 */
static const char *get_sfn(char *dest, size_t buffer_len, const char *filename)
{
  size_t len = strlen(filename);
  ssize_t _ext_pos = path_get_ext_offset(filename);
  size_t ext_pos = _ext_pos >= 0 ? (size_t)_ext_pos : len;
  size_t i;
  size_t j;

  if(is_sfn(filename, len))
    return filename;

  if(buffer_len < SFN_BUFFER_LEN)
    return NULL;

  // 1) Copy the first 6 valid chars of the filename.
  // Spaces and periods before the extension position should be stripped.
  // Other invalid characters should be replaced with an underscore.
  for(i = 0, j = 0; i < ext_pos && j < 6; i++)
  {
    char c = filename[i];

    if(!is_sfn_char(c))
    {
      if(c == ' ' || c == '.')
        continue;

      dest[j++] = '_';
    }
    else
      dest[j++] = c;
  }

  // 2) Append ~1.
  dest[j++] = '~';
  dest[j++] = '1';

  // 3) If there's an extension, place one period and the first 3 valid
  // characters of the extension. In the case of a trailing period, just
  // skip it.
  if(ext_pos + 1 < len)
  {
    size_t ext_max = j + 4;
    dest[j++] = '.';

    for(i = ext_pos + 1; i < len && j < ext_max; i++)
    {
      char c = filename[i];

      if(!is_sfn_char(c))
      {
        if(c == ' ')
          continue;

        dest[j++] = '_';
      }
      else
        dest[j++] = c;
    }
  }
  dest[j] = '\0';
  return dest;
}

// convert to lowercase

static void case1(char *string)
{
  int i, len = strlen(string);

  // lowercase it
  for(i = 0; i < len; i++)
    string[i] = tolower((int)string[i]);
}

// convert to uppercase

static void case2(char *string)
{
  int i, len = strlen(string);

  // uppercase it
  for(i = 0; i < len; i++)
    string[i] = toupper((int)string[i]);
}

// convert from anything to filename.EXT

static void case3(char *string)
{
  int i, len = strlen(string);

  // upper case extension
  for(i = len; i > 0; i--)
  {
    string[i] = toupper((int)string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // lowercase rest
  for(i--; i >= 0; i--)
    string[i] = tolower((int)string[i]);
}

// convert from anything to FILENAME.ext

static void case4(char *string)
{
  int i, len = strlen(string);

  // lowercase extension
  for(i = len; i > 0; i--)
  {
    string[i] = tolower((int)string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // uppercase rest
  for(i--; i >= 0; i--)
    string[i] = toupper((int)string[i]);
}

// brute force method; returns -1 if no permutation can be found to work

static int case5(char *path, size_t buffer_len, char *string, boolean check_sfn)
{
  int ret = -FSAFE_BRUTE_FORCE_FAILED;
  int dirlen = string - path;
  vdir *wd;
  char *newpath;

  newpath = cmalloc(MAX_PATH);

  // prepend the working directory
  snprintf(newpath, MAX_PATH, "./");

  // copy everything sans last token
  if(dirlen > 0)
  {
    if(dirlen + 2 >= MAX_PATH)
      dirlen = MAX_PATH - 2;
    memcpy(newpath + 2, path, dirlen - 1);
    newpath[dirlen + 2 - 1] = 0;
  }

  wd = vdir_open_ext(newpath, VDIR_FAST);
  if(wd)
  {
    const char *string_cmp = string;
    char string_sfn[SFN_BUFFER_LEN];
    char newpath_sfn[SFN_BUFFER_LEN];
    boolean has_sfn_match = false;
    enum sfn_type type;

    // If the input path is a truncated SFN, it may need to be aggressively
    // checked against generated SFNs from files in the directory.
    type = is_sfn(string, strlen(string));
    if(check_sfn && type == SFN_TRUNCATED)
    {
      memcpy(string_sfn, string, strlen(string) + 1);
      string_cmp = string_sfn;
    }

#ifdef CONFIG_DJGPP
    if(type == NOT_AN_SFN)
    {
      get_sfn(string_sfn, SFN_BUFFER_LEN, string);
    }
#endif

    while(vdir_read(wd, newpath, MAX_PATH, NULL))
    {
      // okay, we got something, but does it match?
      if(strcasecmp(string_cmp, newpath) == 0)
      {
        memcpy(string, newpath, strlen(newpath) + 1);
        ret = FSAFE_SUCCESS;
        break;
      }
      else

#ifdef CONFIG_DJGPP
      // Is there a file matching the generated SFN for the requested LFN?
      if(type == NOT_AN_SFN)
      {
        if(!strcasecmp(string_sfn, newpath))
        {
          trace("truncated LFN '%s' to '%s'\n", string, newpath);
          memcpy(string, newpath, strlen(newpath) + 1);
          ret = FSAFE_SUCCESS;
          break;
        }
      }
      else
#endif

      if(type == SFN_TRUNCATED)
      {
        const char *newpath_cmp = get_sfn(newpath_sfn, SFN_BUFFER_LEN, newpath);
        if(!strcasecmp(string_cmp, newpath_cmp))
        {
          size_t newpath_len = strlen(newpath);

          // If there are duplicate SFN matches, there is no unambiguous
          // result and thus it is not possible to guarantee a correct match.
          if(has_sfn_match)
          {
            trace("ambiguous match for SFN '%s' to '%s', aborting.\n",
             string_sfn, newpath);
            memcpy(string, string_sfn, strlen(string_sfn) + 1);
            ret = -FSAFE_BRUTE_FORCE_SFN_AMBIGUOUS;
            break;
          }
          else

          // Make sure the SFN expansion won't overflow the buffer.
          if(newpath_len + dirlen + 1 > buffer_len)
          {
            trace("expansion for SFN '%s' to '%s' would overflow buffer,"
             " aborting.\n", string_sfn, newpath);
            ret = -FSAFE_BRUTE_FORCE_SFN_OVERFLOW;
            break;
          }
          else
          {
            // Overwrite the old path with the expanded match, then continue
            // searching the directory for duplicate matches or an exact match.
            memcpy(string, newpath, newpath_len + 1);
            trace("expanded SFN '%s' to '%s'\n", string_sfn, newpath);
            has_sfn_match = true;
            ret = FSAFE_SUCCESS;
          }
        }
      }
    }

    vdir_close(wd);
  }

  free(newpath);
  return ret;
}

static int match(char *path, size_t buffer_len)
{
  char *nexttoken = path;
  char *token = NULL;
  struct stat inode;
  int i;

  if(path == NULL)
    return -FSAFE_MATCH_FAILED;

  /* FOUR likely permutations on files, and two likely permutations on
   * directories before we start having to do anything fancy:
   *
   * filename.ext FILENAME.EXT filename.EXT FILENAME.ext
   * directory    DIRECTORY
   */

  while(1)
  {
    {
      token = path_tokenize(&nexttoken);

      // this token is the file
      if(nexttoken == NULL)
      {
        for(i = 0; i < 5; i++)
        {
          // check file
          if(vstat(path, &inode) == 0)
            break;

#ifdef NO_EXTRA_CASE_CHECKS
          // Skip the "normal" cases.
          i = 4;
#endif

          // try normal cases, then try brute force
          switch(i)
          {
            case 0:
              case1(token);
              break;

            case 1:
              case2(token);
              break;

            case 2:
              case3(token);
              break;

            case 3:
              case4(token);
              break;

            default:
              // try brute force
              if(case5(path, buffer_len, token, true) < 0)
              {
                trace("file matches for %s failed.\n", path);
                return -FSAFE_MATCH_FAILED;
              }
          }
        }
        break;
      }

      for(i = 0; i < 3; i++)
      {
        // check directory
        if(vstat(path, &inode) == 0)
          break;

#ifdef NO_EXTRA_CASE_CHECKS
        // Skip the "normal" cases.
        i = 2;
#endif

        // try normal cases, then try brute force
        switch(i)
        {
          case 0:
            case1(token);
            break;

          case 1:
            case2(token);
            break;

          default:
            // try brute force
            if(case5(path, buffer_len, token, false) < 0)
            {
              trace("directory matches for %s failed.\n", path);
              return -FSAFE_MATCH_FAILED;
            }
        }
      }

      /* this "hack" overwrites the token's \0 to re-formulate
       * the string versus path_tokenize(); it has the nice side-effect of
       * also converting windows style path to UNIX ones, so they'll
       * work on everything.
       */
      token[strlen(token)] = '/';
    }
  }

  return FSAFE_SUCCESS;
}
#endif /* ENABLE_DOS_COMPAT_TRANSLATIONS */

/* OK before we do anything, we need to make some security checks. MZX games
 * shouldn't be able to open C:\Windows\Explorer.exe and overwrite it, so
 * we need to filter out any absolute pathnames, or relative pathnames
 * including "..". Do so here.
 */

static int fsafetest(const char *path, char *newpath, size_t buffer_len,
 boolean safety_checks)
{
  enum path_safe_mask ret;

  // we don't accept it if it's NULL or too short
  if((path == NULL) || (path[0] == 0))
    return -FSAFE_INVALID_ARGUMENT;

  path_clean_copy_posixdos(newpath, buffer_len, path);
#ifdef CONFIG_AMIGA
  /* TODO: this should be done in the clean functions, but for compatibility,
   * right now only do it on the platforms where it is required (Amiga). */
  path_clean_current_tokens(newpath, buffer_len);
#endif

  if(!safety_checks)
    return FSAFE_SUCCESS;

  ret = path_safety_check(newpath, PATH_SAFE_ANY_ROOT | PATH_SAFE_UNIX_PARENT |
                                   PATH_SAFE_DOS_CHARACTER | PATH_SAFE_DOS_DEVICE);

  if(ret == PATH_SAFE_UNIX_ROOT)
    return -FSAFE_ABSOLUTE_PATH_ERROR;

  if(ret == PATH_SAFE_DOS_ROOT)
    return -FSAFE_WINDOWS_DRIVE_LETTER_ERROR;

  if(ret == PATH_SAFE_UNIX_PARENT)
    return -FSAFE_PARENT_DIRECTORY_ERROR;

  if(ret == PATH_SAFE_DOS_CHARACTER)
    return -FSAFE_DOS_RESERVED_CHARACTER_ERROR;

  if(ret == PATH_SAFE_DOS_DEVICE)
    return -FSAFE_DOS_DEVICE_NAME_ERROR;

  return FSAFE_SUCCESS;
}

int fsafetranslate(const char *path, char *newpath, size_t buffer_len)
{
  struct stat file_info;
  int ret;

  // try to pass the basic security tests
  ret = fsafetest(path, newpath, buffer_len, true);
  if(ret == FSAFE_SUCCESS)
  {
    // see if file is already there
    if(vstat(newpath, &file_info) != 0)
    {
#ifdef ENABLE_DOS_COMPAT_TRANSLATIONS
      // it isn't, so try harder..
      ret = match(newpath, buffer_len);
      if(ret == FSAFE_SUCCESS)
      {
        // ..and update the stat information for the new path
        if(vstat(newpath, &file_info) != 0)
          ret = -FSAFE_MATCH_FAILED;
      }
      else
      {
        // Replace the failed match with the original user-supplied path
        fsafetest(path, newpath, buffer_len, false);
      }
#else
      // on WIN32 we can't, so fail hard
      ret = -FSAFE_MATCH_FAILED;
#endif
    }

    if(ret == FSAFE_SUCCESS)
    {
      // most callers don't want directories
      if(S_ISDIR(file_info.st_mode))
        ret = -FSAFE_MATCHED_DIRECTORY;
    }
  }

#ifdef ENABLE_DOS_COMPAT_TRANSLATIONS
  if(ret == -FSAFE_SUCCESS || ret == -FSAFE_MATCHED_DIRECTORY)
  {
    trace("translated %s to %s%s.\n", path, newpath,
     (ret == -FSAFE_MATCHED_DIRECTORY) ? "/" : "");
  }
  else
  {
    trace("failed to translate %s (err %d).\n", path, ret);
  }
#endif

  return ret;
}

vfile *fsafeopen(const char *path, const char *mode)
{
  char *newpath;
  int i, ret;
  vfile *f;

  newpath = cmalloc(MAX_PATH);

  // validate pathname, and optionally retrieve a better name
  ret = fsafetranslate(path, newpath, MAX_PATH);

  // filename couldn't be "found", but there were no security issues
  if(ret == -FSAFE_MATCH_FAILED)
  {
    // if we're reading, any failure to translate the name means we back out
    for(i = 0; i < (int)strlen(mode); i++)
    {
      if(mode[i] == 'r' || mode[i] == '+')
      {
        free(newpath);
        return NULL;
      }
    }
  }
  else

  if(ret < 0)
  {
    // bad name, or security checks failed
    free(newpath);
    return NULL;
  }

  // _TRY_ opening the file
  f = vfopen_unsafe(newpath, mode);
  free(newpath);
  return f;
}

/* It's conceivable that on some platforms (like Linux, or Macintosh classic),
 * fgets may return a string that still contains "EOL" characters considered
 * by another platform. For example, if a file is written out by Windows,
 * and a Linux user reads it, the buffers will not remove the \r character.
 *
 * This function provides a "safe" wrapper that removes all kinds of line
 * endings from the buffer, and should work at least until somebody invents
 * a new three byte string terminator ;-(
 */
/*
char *fsafegets(char *s, int size, FILE *stream)
{
  char *ret = fgets(s, size, stream);

  if(ret)
  {
    size_t len = strlen(ret);

    if(len > 0)
      if(s[len - 1] == '\r' || s[len - 1] == '\n')
        s[len - 1] = '\0';

    if(len > 1)
      if(s[len - 2] == '\r' || s[len - 2] == '\n')
        s[len - 2] = '\0';
  }

  return ret;
}
*/
