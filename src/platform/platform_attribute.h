/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __PLATFORM_ATTRIBUTE_H
#define __PLATFORM_ATTRIBUTE_H

/**
 * Header for misc. compiler-specific attributes.
 */

#ifdef __has_attribute
#define HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define HAS_ATTRIBUTE(x) 0
#endif

/**
 * Catch-all for GCC printf instrumentation. GCC will emit warnings
 * if this attribute is not used on printf-like functions.
 */
#if (defined(__GNUC__) && !defined(__clang__)) || HAS_ATTRIBUTE(format)
#if __GNUC__ >= 5 || (__GNUC__ == 4 &&  __GNUC_MINOR__ >= 4)

// Note: param numbers are 1-indexed for normal functions and
// 2-indexed for member functions (including constructors).
#define ATTRIBUTE_PRINTF(string_index, first_to_check) \
 __attribute__((format(gnu_printf, string_index, first_to_check)))

#else

#define ATTRIBUTE_PRINTF(string_index, first_to_check) \
 __attribute__((format(printf, string_index, first_to_check)))

#endif
#endif

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(string_index, first_to_check)
#endif

/**
 * Turn off sanitizer instrumentation for a particular location.
 */
#if (defined(__GNUC__) && __GNUC__ >= 9) || \
 (defined(__clang__) && HAS_ATTRIBUTE(no_sanitize))
#define ATTRIBUTE_NO_SANITIZE(...) __attribute__((no_sanitize(__VA_ARGS__)))
#endif

#ifndef ATTRIBUTE_NO_SANITIZE
#define ATTRIBUTE_NO_SANITIZE(...)
#endif

#undef HAS_ATTRIBUTE

#endif // __PLATFORM_ATTRIBUTE_H
