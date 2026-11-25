/* MegaZeux
 *
 * Copyright (C) 2021-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_BASE_H
#define MEGAZEUX_BASE_H

/**
 * Base defines for MegaZeux, including header macros, min/max/clamp
 * implementations, restrict, attributes, and a consistently-sized bool type.
 *
 * These are usually prefixed with MZX_ or mzx_ in case this header is
 * included by external files; for the internal versions (where applicable),
 * conditional visibility defines, and deprecated standard function wrappers,
 * see compat.h. Internal headers should typically include this via compat.h.
 */
#if defined(__cplusplus)
#define MEGAZEUX_BEGIN_DECLS  extern "C" {
#define MEGAZEUX_END_DECLS    }
#else
#define MEGAZEUX_BEGIN_DECLS
#define MEGAZEUX_END_DECLS
#endif

/* Prefer __restrict, the common C/C++ compatibility name for restrict. */
#ifndef MZX_RESTRICT
#if (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1400)) || \
    (defined(__WATCOMC__) && (__WATCOMC__ >= 1250) && !defined(__cplusplus))
#define MZX_RESTRICT __restrict
#elif defined(RESTRICT)
#define MZX_RESTRICT RESTRICT
#else
#define MZX_RESTRICT
#endif
#endif

#define MZX_MIN(a,b)          ((a) < (b) ? (a) : (b))
#define MZX_MAX(a,b)          ((a) > (b) ? (a) : (b))
#define MZX_CLAMP(x,a,b)      ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
#define MZX_SGN(x)            (((x) > 0) - ((x) < 0))
#define MZX_ARRAY_SIZE(arr)   (sizeof(arr) / sizeof(*(arr)))

typedef unsigned char mzx_bool;
#define MZX_FALSE 0
#define MZX_TRUE  1


/**
 * Visibility
 * MZX_MODULAR and MZX_MODULE_* are provided by gcc options via the Makefile.
 */
#ifdef MZX_MODULAR
#ifdef _WIN32
#define MZX_IMPORT __declspec(dllimport)
#define MZX_EXPORT __declspec(dllexport)
#else
#define MZX_IMPORT __attribute__((visibility("default")))
#define MZX_EXPORT __attribute__((visibility("default")))
#endif

#ifdef MZX_MODULE_MAIN
#define MZX_LIBMAIN     MZX_EXPORT
#else
#define MZX_LIBMAIN     MZX_IMPORT
#endif

#ifdef MZX_MODULE_CORE
#define MZX_LIBCORE     MZX_EXPORT
#define MZX_LIBIO       MZX_EXPORT
#define MZX_LIBPLATFORM MZX_EXPORT
#define MZX_LIBAUDIO    MZX_EXPORT
#define MZX_LIBEVENT    MZX_EXPORT
#define MZX_LIBRENDER   MZX_EXPORT
#define MZX_LIBNETWORK  MZX_EXPORT
#else
#define MZX_LIBCORE     MZX_IMPORT
#define MZX_LIBIO       MZX_IMPORT
#define MZX_LIBPLATFORM MZX_IMPORT
#define MZX_LIBAUDIO    MZX_IMPORT
#define MZX_LIBEVENT    MZX_IMPORT
#define MZX_LIBRENDER   MZX_IMPORT
#define MZX_LIBNETWORK  MZX_IMPORT
#endif

#ifdef MZX_MODULE_EDITOR
#define MZX_LIBEDITOR   MZX_EXPORT
#else
#define MZX_LIBEDITOR   MZX_IMPORT
#endif

#else /* !MZX_MODULAR */
#define MZX_IMPORT
#define MZX_EXPORT
#define MZX_LIBMAIN
#define MZX_LIBCORE
#define MZX_LIBIO
#define MZX_LIBPLATFORM
#define MZX_LIBAUDIO
#define MZX_LIBEVENT
#define MZX_LIBRENDER
#define MZX_LIBNETWORK
#define MZX_LIBEDITOR
#endif


/**
 * Attributes
 */
#ifdef __has_attribute
#define MZX_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define MZX_HAS_ATTRIBUTE(x) 0
#endif

/**
 * Catch-all for GCC printf instrumentation. GCC will emit warnings
 * if this attribute is not used on printf-like functions.
 */
#if (defined(__GNUC__) && !defined(__clang__)) || MZX_HAS_ATTRIBUTE(format)
#if __GNUC__ >= 5 || (__GNUC__ == 4 &&  __GNUC_MINOR__ >= 4)

// Note: param numbers are 1-indexed for normal functions and
// 2-indexed for member functions (including constructors).
#define MZX_ATTRIBUTE_PRINTF(string_index, first_to_check) \
 __attribute__((format(gnu_printf, string_index, first_to_check)))

#else

#define MZX_ATTRIBUTE_PRINTF(string_index, first_to_check) \
 __attribute__((format(printf, string_index, first_to_check)))

#endif
#endif

#ifndef MZX_ATTRIBUTE_PRINTF
#define MZX_ATTRIBUTE_PRINTF(string_index, first_to_check)
#endif

/**
 * (Attempt) compile-time NULL safety.
 */
#if (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 303)) || \
 defined(__clang__)
#define MZX_ATTRIBUTE_NONNULL __attribute__((nonnull))
#define MZX_ATTRIBUTE_NONNULL_AT(args) __attribute__((nonnull args))
#else
#define MZX_ATTRIBUTE_NONNULL
#define MZX_ATTRIBUTE_NONNULL_AT(args)
#endif

/**
 * Turn off sanitizer instrumentation for a particular location.
 */
#if (defined(__GNUC__) && __GNUC__ >= 9) || \
 (defined(__clang__) && MZX_HAS_ATTRIBUTE(no_sanitize))
#define MZX_ATTRIBUTE_NO_SANITIZE(...) __attribute__((no_sanitize(__VA_ARGS__)))
#endif

#ifndef MZX_ATTRIBUTE_NO_SANITIZE
#define MZX_ATTRIBUTE_NO_SANITIZE(...)
#endif

#undef MZX_HAS_ATTRIBUTE

#endif /* MEGAZEUX_BASE_H */
