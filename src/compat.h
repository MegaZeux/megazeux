/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

// Provide some compatibility macros for combined C/C++ binary

#ifndef MEGAZEUX_COMPAT_H
#define MEGAZEUX_COMPAT_H

#include "config.h"

#ifdef __cplusplus

#define MEGAZEUX_BEGIN_DECLS extern "C" {
#define MEGAZEUX_END_DECLS   }

/**
 * Compatibility define for restrict in C++, where it is a (commonly supported)
 * compiler extension. This has to be defined as 'RESTRICT' because defining
 * 'restrict' conflicts with MSVC's __declspec(restrict) attribute.
 */
#ifndef RESTRICT
#define RESTRICT __restrict
#endif

#if __cplusplus >= 201103
#define IS_CXX_11 1
#define maybe_constexpr constexpr
#define maybe_explicit explicit
#else /* !IS_CXX_11 */
// Compatibility defines so certain C++11 features can be used without checks.
// Don't create any variables named "final" or "noexcept"...
#include <stddef.h>
#ifndef nullptr
#define nullptr (NULL)
#endif
#define final
#define noexcept
#define override
#define maybe_constexpr
#define maybe_explicit
#endif /* !IS_CXX_11 */

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#else

#define MEGAZEUX_BEGIN_DECLS
#define MEGAZEUX_END_DECLS

#if !defined(CONFIG_WII) && !defined(CONFIG_NDS) && !defined(CONFIG_3DS) && \
 !defined(CONFIG_DREAMCAST)

#undef false
#undef true
#undef bool

enum
{
  false = 0,
  true  = 1,
};

#endif // !CONFIG_WII && !CONFIG_NDS

#endif /* __cplusplus */

typedef unsigned char boolean;

#ifdef CONFIG_3DS
#include <3ds.h>
#endif

#ifdef CONFIG_NDS
#include <nds.h>

#ifndef CONFIG_NDS_BLOCKSDS
// Use iprintf/iscanf on NDS to save ~50 KB
#define sscanf siscanf
#define printf iprintf
#define fprintf fiprintf
#define sprintf siprintf
#define snprintf sniprintf
#define vsnprintf vsniprintf
#endif
#endif

#ifdef CONFIG_WII
#define BOOL _BOOL
#include <gcbool.h>
#include <gctypes.h>
#undef BOOL
#undef FIXED
#endif

#ifdef CONFIG_EDITOR
#define __editor_maybe_static
#else
#define __editor_maybe_static static
#endif

#ifdef CONFIG_UPDATER
#define __updater_maybe_static
#else
#define __updater_maybe_static static
#endif

#ifdef CONFIG_UTILS
#define __utils_maybe_static
#else
#define __utils_maybe_static static
#endif

#ifdef _MSC_VER
#include "msvc.h"
#ifndef RESTRICT
#define RESTRICT __restrict
#endif
#endif

#ifdef __APPLE__
// Patch in some potentially missing defines for older platforms that
// may be required. Fixes older versions of SDL2 headers on Snow Leopard.
#include <TargetConditionals.h>
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
#include <Availability.h>
#endif

#ifndef TARGET_OS_TV
#define TARGET_OS_TV 0
#endif

#ifndef __has_feature
#define __has_feature(f) 0
#endif
#endif

#ifdef __WIN32__
// Usually defined in Windows headers but somehow those always seem to add 30%
// or more build time and we don't include them globally for any other purpose.
#define MAX_PATH 260
#endif

#ifdef __OpenBSD__
#include <sys/param.h>
// These macros conflict with internally-defined MZX macros
#undef MIN
#undef MAX

// unveil added in OpenBSD 6.3
#ifdef OpenBSD
#if OpenBSD >= 201805
#define PLEDGE_HAS_UNVEIL
#endif

#endif /* OpenBSD */
#endif /* __OpenBSD__ */

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#ifndef RESTRICT
#define RESTRICT restrict
#endif

#if defined(CONFIG_MODULAR) && defined(__WIN32__)
#define LIBSPEC __declspec(dllimport)
#elif defined(CONFIG_MODULAR)
#define LIBSPEC __attribute__((visibility("default")))
#else
#define LIBSPEC
#endif

#ifndef CORE_LIBSPEC
#define CORE_LIBSPEC LIBSPEC
#endif

#ifndef EDITOR_LIBSPEC
#define EDITOR_LIBSPEC LIBSPEC
#endif

#ifndef AUDIO_LIBSPEC
#define AUDIO_LIBSPEC LIBSPEC
#endif

#ifndef UTILS_LIBSPEC
#define UTILS_LIBSPEC CORE_LIBSPEC
#endif

#ifdef CONFIG_UPDATER
#define UPDATER_LIBSPEC CORE_LIBSPEC
#else
#define UPDATER_LIBSPEC
#endif

MEGAZEUX_BEGIN_DECLS

#ifdef __GNUC__

// On GNU compilers we can mark fopen() as "deprecated" to remind callers
// to properly audit calls to it.
//
// Any case where a file to open is specified by a game, the path must first
// be translated, so that any case-insensitive matches will be used and
// safety checks will be applied. Callers should use either
// fsafetranslate()+fopen_unsafe(), or call fsafeopen() directly.
//
// Any case where a file comes from the editor, or from MZX-proper, it
// probably should not be translated. In this case fopen_unsafe() can be
// used directly.

#include <stdio.h>

static inline FILE *fopen_unsafe(const char *path, const char *mode)
 { return fopen(path, mode); }
static inline FILE *check_fopen(const char *path, const char *mode)
 __attribute__((deprecated));
static inline FILE *check_fopen(const char *path, const char *mode)
 { return fopen_unsafe(path, mode); }

#define fopen(path, mode) check_fopen(path, mode)

// Also deprecate some notoriously bad string functions.
// strncpy and strncat are unsafe functions that get cargo cult usage as "safe"
// versions of strcpy and strcat (which they aren't). strtok is non-reentrant.

#ifdef _WIN32
// Some MinGW headers use these functions inline for some reason...
#include <io.h>
#endif
#include <string.h>
static inline char *check_strncpy(char *, const char *, size_t)
 __attribute__((deprecated));
static inline char *check_strncat(char *, const char *, size_t)
 __attribute__((deprecated));
static inline char *check_strtok(char *, const char *)
 __attribute__((deprecated));
static inline char *check_strncpy(char *d, const char *s, size_t n)
 { return strncpy(d,s,n); }
static inline char *check_strncat(char *d, const char *s, size_t n)
 { return strncat(d,s,n); }
static inline char *check_strtok(char *d, const char *delim)
 { return strtok(d,delim); }
#undef strncpy
#undef strncat
#undef strtok
#define strncpy(d,s,n) check_strncpy(d,s,n)
#define strncat(d,s,n) check_strncat(d,s,n)
#define strtok(d,delim) check_strtok(d,delim)

#else // !__GNUC__

#define fopen_unsafe(file, mode) fopen(file, mode)

#endif // __GNUC__

#ifdef CONFIG_CHECK_ALLOC

#include <stddef.h>

CORE_LIBSPEC void check_alloc_init(void);

#define ccalloc(nmemb, size) check_calloc(nmemb, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_calloc(size_t nmemb, size_t size, const char *file,
 int line);

#define cmalloc(size) check_malloc(size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_malloc(size_t size, const char *file,
 int line);

#define crealloc(ptr, size) check_realloc(ptr, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_realloc(void *ptr, size_t size, const char *file,
 int line);

#else /* CONFIG_CHECK_ALLOC */

#include <stdlib.h>

static inline void check_alloc_init(void) {}

static inline void *ccalloc(size_t nmemb, size_t size)
{
  return calloc(nmemb, size);
}

static inline void *cmalloc(size_t size)
{
  return malloc(size);
}

static inline void *crealloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

#endif /* CONFIG_CHECK_ALLOC */

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_COMPAT_H */
