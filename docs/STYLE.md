# MegaZeux code formatting and style guide

This file is an attempt to summarize the coding style that should be used in
code contributed to MegaZeux.  It is a work-in-progress.  Contributions to the
MZX source code will not be merged unless they comply with this guide.

## License

Include the license at the start of every file in a block comment starting with
`MegaZeux` and followed by copyright notices for all authors that have
contributed significantly to the file. If you contribute significantly to a
pre-existing file, you are recommended to add your copyright.

## Margin

Wrap lines at 80 chars. Minor deviations from this are acceptible but generally
speaking lines significantly longer than this tend to have readability issues.

## Control statements

Control statements are not separated from their parentheses.

```
if(a == b)
```

```
while(cond)
```

```
for(i = 0; i < x; i++)
```

```
switch(cond)
```

```
do
{
  ...
}
while(cond);
```

## Curly braces

Curly braces belong on their own lines in function definitions, control
structures, structure/enum definitions, and in the outermost level of an array
of structures or a 2D array.

```
void main(int argc, char *argv[])
{
}
```

### if/else

The following spacing/curly brace formats are generally okay:

```
if(cond)
  do_thing();
```

```
if(cond)
  do_thing();

else
  do_other_thing();
```

```
if(cond)
{
  ...
}
```

```
if(cond)
{
  ...
}
else
{
  ...
}
```

```
if(cond)
{
  ...
}
else

if(other_cond)
{
  ...
}

else
{
  ...
}
```

Do not mix curly braces and no curly braces in the same if/else chain unless
you are excluding curly braces from the final else. For example, this is OK:

```
if(cond)
{
  ...
}
else
  thing();
```

but this is bad form (and generally looks tacky):

```
if(cond)
  lol();
else
{
  ...
}
```

### Structures/enums

Definition:
```
struct a_struct
{
  ...
};

enum an_enum
{
  ...
};
```

Assignment:
```
static struct a_struct thing =
{
  ...
};
```

### Array definitions

Arrays of structs or 2D arrays only need to have curly braces on their own lines
for the outermost brace level. When defining a struct on a single line you
should generally tabulate its values so it is more readable.

```
struct a things[] =
{
  { "abcd",       abcd_fn },
  { "hjsdfksd",   hjsdfksd_fn },
};
```

Putting the inner braces on their own lines is OK too:

```
struct a things[] =
{
  {
    "abcd",
    abcd_fn
  },
  {
    "hjsdfksd",
    hjsdfksd_fn
  },
};
```

A trailing comma on the final element of an array or enum is OK.


## Indentation

Use spaces, not tabs. Exception: shell scripts and Makefiles should use tabs
instead.

### Individual lines

Use two *spaces* for each indentation level.

```
{
  {
    {
      // ...
    }
  }
}
```

### Wrapped lines

Typically use one space after the current indentation level.

```
  if(a == b && ... &&
   a == f)
```

For certain expressions, it is OK and might be useful to indent further:

```
  if((a == b) &&
   (b == c ||
    c == d))
```

```
  if(a == SOME_THING_1 ||
     b == SOME_THING_2 ||
     c == SOME_THING_3)
```

When splitting a function call with many arguments to multiple lines,
this indentation may look better (and helps avoid the margin!):

```
  func([params,]
    params,
    params,
    ...
  );
```

### Macros

`#ifdef` et al. should not be indented. Aside from very short blocks, note the
original condition in a block comment after `#endif`.

```
    // some code
    ...

#ifdef CONFIG_DEBYTECODE
    ...
#endif /* CONFIG_DEBYTECODE */

    ...
    // more code
```

In complex cases where there is no other option, left-hash indentation is OK:

```
#ifdef CONFIG_GLES
# ifdef CONFIG_RENDER_GL_FIXED
#   if SDL_VERSION_ATLEAST(3,0,0)
#     include <SDL3/SDL_opengles.h>
#   else
#     ...
#   endif
# endif
# ifdef CONFIG_RENDER_GL_PROGRAM
#   ...
# endif
#else
# ...
#endif
```

### Tabulated definitions

In some places where many values or repetitive structs or enums are defined,
tabulation is used to keep these definitions readable. When editing a definition
or set of definitions with tabulation, you should follow the existing tabulation.

Tabulation examples from MZX's source code:

world.h
```
enum mzx_version
{
  V100            = 0x0100, // Magic: MZX
  V200            = 0x0205, // Magic: MZ2
  V251            = 0x0205,
  ...
  VERSION_DECRYPT = 0x0211, // Special version used for decrypted worlds only.
```

counter.c
```
static const struct function_counter builtin_counters[] =
{
  { "$*",               V262,   string_counter_read,  string_counter_write },
  { "abs!",             V268,   abs_read,             NULL },
  { "acos!",            V268,   acos_read,            NULL },
  { "arctan!,!",        V284,   atan2_read,           NULL },
```

sai.frag
```
#define SHARPEN_EDGES     1
#define MULTIPLIER        10.0

#define TEX_SCREEN_WIDTH  1024.0
#define TEX_SCREEN_HEIGHT 512.0
#define PIXEL_X           1.0 / TEX_SCREEN_WIDTH
#define PIXEL_Y           1.0 / TEX_SCREEN_HEIGHT
#define HALF_PIXEL_X      0.5 / TEX_SCREEN_WIDTH
#define HALF_PIXEL_Y      0.5 / TEX_SCREEN_HEIGHT
```

## Naming

Variables, structs, and functions in C should generally be named with lowercase
words separated by underscores (examples: `thing`, `other_thing`).

Constants should be `ALL_CAPS_WORDS_SEPARATED_BY_UNDERSCORES`.

C++ code should follow the same guidelines as C with the exception that class
names (and only class names) may use `CamelCase` instead of C naming.

## Types

### Integers

The following ways of expressing integer types are generally OK.
Other variants/orderings of specifiers may be redundant or undesirable,
but not necessarily bad. Don't mix cv specifiers between type words.

| Typical | OK variants |
|---------|-------------|
| `char`                    | `unsigned char` (MZX uses -funsigned-char)
| `signed char`             |
| `short`                   |
| `unsigned short`          |
| `int`                     |
| `unsigned int`            | `unsigned`
| `long`                    | `long int`
| `unsigned long`           | `unsigned long int`
| `ssize_t` and `ptrdiff_t` |
| `size_t`                  |

C99 stdint.h fixed-width integer types should be used in cases where the
width of a particular field needs to be guaranteed and/or emphasized. `int64_t`
and `uint64_t` should be preferred over `long long` and `unsigned long long`.
The types `size_t`, `ssize_t`, and `ptrdiff_t` should not be assumed to be
64-bits wide.

Don't use SDL's fixed-width integer typedefs outside of SDL-specific code.
Previously these were used in quite a few places, and it caused `platform.h`
includes to creep into most files.

### Pointers

Pointers are written in this style:
```
int *an_int_pointer;
```
```
const void *data;
```
```
const char * const a;
```

### Casting
```
  int an_int = (int)not_an_int;
```
```
  int *an_int_ptr = (int *)not_an_int_ptr;
```

### Constants

Constants should typically be preprocessor defines in C-compatible code/headers
and `const` or `constexpr` in C++-only code (see C++98 vs C++11 below).

```
#define CHAR_W  8
#define CHAR_H  14
```
```
const int CXX_CONSTANT        = 123;
constexpr int CXX_CONSTANT_2  = (CXX_CONSTANT * 456);
```

Generally don't make constants in C code `const`. Also note that constants
typically exist for a reason (usually to make the code more readable and
maintainable), so code with consistent constant usage shouldn't be polluted
with magic numbers unless you have a good excuse.

### `struct` and `class` in C++

In C++ code, `struct` and `class` may be omitted when using classes exclusive
to the C++ code. When referencing types defined and used in C code, preface the
type name with `struct` for consistency between the two languages.

## Language features

### C vs. C++

MZX is almost entirely written in C.  Add C++ only where absolutely necessary
e.g. when interfacing with a library that only has a C++ interface available,
or for things that need inheritance or access specifiers or templates to be
maintainable (example: the network code was mostly a mess prior to getting a C++
makeover).

### C++98 vs. C++11

When possible, MegaZeux's build system will attempt to select the C++11 standard
with GNU extensions for compilation. However, when this is not available, it
will fall back to C++98. For compatibility purposes, all platform-independent
C++ code in MZX should be C++98 compatible. If such code uses C++11 features,
these should be enabled at compile time. Optional parts of MZX that rely on
C++11 (such as the unit tests) should be disabled when C++11 support is not
available or is unreliable.

To avoid including libstdc++/libc++ for platforms where it would just bloat
(Android NDK) or pull in ugly DLL dependency chains (MinGW via MSYS2),
features dependent on these libraries should be completely avoided. This means
no exceptions and using MZX-specific implementations of new/delete/etc.
(see `src/nostdc++.cpp`).

Features introduced in C++11 that can be used without compatibility checks due
to defines in `src/compat.h` are:

| Feature | C++98 `compat.h` define |
|---------|-------------------------|
| `nullptr`             | `(NULL)`
| `final` qualifier     | ` `
| `noexcept` qualifier  | ` `
| `override` qualifier  | ` `

### C99 Features

Most C99 features are allowed as MSVC now properly supports them. The primary
exceptions are:

* Never use VLAs.
* Never use stdbool.h or any C type declared as "bool". There's a nasty
  portability problem when using `bool` between C and C++ where the size of
  this type will not necessarily be the same between the two languages.
  Use `boolean` as defined in compat.h instead.
* Only use designated array initializers in code that will never be compiled
  as C++ (as C++ does not support them).
* As a general rule, mixed declarations and code should be avoided as well.
  The primary exception to this is when using declarations for RAII in C++.
* The `restrict` keyword should be used only through the macro `RESTRICT`.
  This is for intercompatibility with C++. See GNU extensions for more info
  and restrictions on usage.

### C standard library functions

Avoid use of the following C standard library functions:

* `strcpy`, `strcat`, and `sprintf` do not check the size of the provided buffer
  and should only be used in circumstances where there is no chance of an overflow.
  Generally speaking, `snprintf` (followed by manual null termination for MSVC
  safety) or `memcpy`/`memmove` is preferable.
* `strncpy` and `strncat` do not do what you think they do. `strncpy` was
  intended to null-pad fixed size buffers and does not insert a null terminator
  if there is an overflow. The length parameter of `strncat` does not specify
  the size of the provided buffer but instead how many bytes from the source
  should be copied. Do not use these functions, ever.
* `strtok` is non-reentrant; it stores the initial pointer provided to it in a
  static variable. Use `strsep` instead (see POSIX features).

### GNU extensions

Generally speaking, MZX should not use GNU extensions as they will worsen
compatibility. The following GNU extensions are allowed:

* Variadic macros.
* Attributes (such as unused or format(printf) or format(gnu_printf)) to avoid
  spurious warnings. Generally these should be wrapped in `#ifdef __GNUC__`.
* Attributes to specify deprecated functions (wrapped in `#ifdef __GNUC__`).
* `long long` (should usually use stdint.h instead).
* Pre-C++11 code that relies on stdint.h.
* Pragmas to locally disable warnings are acceptable in rare cases.
* `__PRETTY_FUNCTION__` (C++ only; an MSVC compatibility define is included).
* The `__restrict` keyword provided for C and C++ by GCC, Clang, ICC, and MSVC
  may be used via the macro `RESTRICT` defined in compat.h. Since this is an
  extension in C++, it's probably best to use it only on pointers. `RESTRICT`
  should generally only be used in places where it's verified to produce a
  significant performance increase (example: the software renderers).

### POSIX features

The following POSIX features are allowed, though special restrictions may apply.
This list is probably not complete.

Features with existing abstractions that shouldn't be used directly:
* `dirent.h` (use `src/io/dir.{c,h}` instead)
* `pthread.h` (use `src/platform.h` instead)
* `getaddrinfo` (use `src/network/DNS.{cpp,hpp}` instead)
* Sockets and other network features (use `src/network/Host.{cpp,hpp}` instead)

Features with partial or complete fallback implementations for MSVC:
* `ssize_t` (`msvc.h`)
* `sys/stat.h` (`msvc.h`)
* `unistd.h` (`msvc.h`) (includes of this header should be wrapped in `#ifndef _MSC_VER`).
* `dirent.h` (`arch/msvc/dirent.h`)
* `gettimeofday` (`arch/msvc/win32time.{c,h}`)

Features with fallback implementations for MSVC and/or MinGW and/or AmigaOS 4:
* `strsep` (`util.{c,h}`)
* `strcasecmp` (`util.{c,h}` and/or `msvc.h`)
* `strncasecmp` (`util.{c,h}` and/or `msvc.h`)
* `mkdir(name,mode)` (`util.h` and/or `msvc.h` and/or `src/io/vfile.{c,h}`)
* Various network features (`src/network/Socket.{cpp,hpp}`)

Other notes:
* Never use `fdopen` on a file descriptor obtained from using `fileno` on a
  `FILE` pointer. This usage of `fdopen` will trick most operating systems into
  thinking you leaked a file descriptor, and it is impossible to close either
  resulting `FILE` pointer safely without corrupting the other.

## Organization

### Opaqueness and typedefs

Typedefs are appropriate in MZX two cases:
1) Hiding really ugly/long function pointer definitions.
2) Ubiquitous basic types (e.g. boolean) or opaque types (e.g. FILE).
Otherwise, don't use them.

Some subsets of the MZX source (particularly audio, rendering, and networking)
heavily practice hiding structs and implementation-specific functions.  Look at
other files in these areas to make sure your audio handler/renderer/etc follows
conventions.

### Headers

General format is:

```
[GPL Copyright notice/license - see LICENSE]

#ifndef MEGAZEUX_[YOUR FILE HERE]_H
#define MEGAZEUX_[YOUR FILE HERE]_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

[all other contents]

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_[YOUR FILE HERE]_H */
```

Notes:
1) The GPL notice must be in every source file. See LICENSE for more information.
2) Each header needs a unique include guard to ensure that it isn't included multiple times.
3) `src/compat.h` defines some required compatibility macros, including `MEGAZEUX_BEGIN_DECLS`
and `MEGAZEUX_END_DECLS`. These macros are used to provide portability when the MZX headers
are included in C++ files.
4) Functions/structs/variables in these MUST be required elsewhere; otherwise, use static and
put it in the C/CPP file!

### Includes

Includes should generally be organized into several blocks separated by blank
lines. The order of these blocks may vary but the order specified here is
usually preferred. An effort should be made to keep each of these blocks in
alphabetical order. Use angle brackets for system and external library includes
and use quotes for MZX and contrib includes.

* C standard library includes and other library includes (e.g. `zlib.h`).
  In some cases the other library includes are in a separate block. External
  library includes (particularly `SDL.h`) should be restricted to places that
  explicitly need them to help reduce build times.
* For Windows, `windows.h` may need to be included and certain headers may
  need to be replaced with `_MSC_VER`-specific replacements. This should be its
  own block, and in most cases `WIN32_LEAN_AND_MEAN` should be defined immediately
  before the include. This header should be restricted to places that explicitly
  use it, because it can make MinGW builds SLOW!
* Includes from the current source dir or "module", followed by blocks for
  includes from other dirs/"modules". These includes generally should be relative.
  * Core includes (files in `src/` and `src/io/` should be in the same block,
    with `src/` files first and `src/io/` files at the end).
  * Audio includes (files in `src/audio/`).
  * Editor includes (files in `src/editor/`).
  * Network includes (files in `src/network/`).
  * Contributed includes (files in `contrib/*/`).

### Declarations

Usually declarations occur in the following order, though there's no real
consistency between different files:

1) `#define`s, enum declarations, and struct declarations.

Then, in .c/.cpp files:

2) static variable declarations.
3) function definitions.

Or in headers:

2) extern variable declarations.
3) `LIBSPEC` external function declarations.
4) Other external function declarations.
5) Conditionally external function declarations.
6) static inline function declarations.

### `LIBSPEC` macros

The `LIBSPEC` macros are used in function declarations to specify conditionally
external functions/variables for modular builds. The `LIBSPEC` macro is defined
as follows:

| Build type                | `LIBSPEC` |
|---------------------------|-----------|
| Non-modular               | ` `
| Windows, modular          | `__declspec(dllimport)`
| Everything else, modular  | `__attribute__((visibility("default")))`

Then, the following `LIBSPEC` macros are defined:

| Macro             | core      | editor    | megazeux/mzxrun | utils             | Used in |
|-------------------|-----------|-----------|-----------------|-------------------|---------|
| `CORE_LIBSPEC`    | Export    | `LIBSPEC` | `LIBSPEC`       | `LIBSPEC` or ` `  | `src/`, `src/io/`
| `EDITOR_LIBSPEC`  | `LIBSPEC` | Export    | `LIBSPEC`       | n/a               | `src/editor/`
| `AUDIO_LIBSPEC`   | Export    | `LIBSPEC` | `LIBSPEC`       | n/a               | `src/audio/`
| `UTILS_LIBSPEC`   | Export    | `LIBSPEC` | `LIBSPEC`       | ` `               | `src/`, `src/io/`
| `UPDATER_LIBSPEC` | Export    | `LIBSPEC` | `LIBSPEC`       | n/a               | `src/network/`

Export = `__declspec(dllexport)` for Windows modular builds and `LIBSPEC` for
all other builds.

These macros should only be used in declarations, e.g.:
```
CORE_LIBSPEC void *check_malloc(size_t size, const char *file, int line);
```

### Conditionally external function macros

The following conditionally external function/variable macros are defined:

| Macro                     | Notes |
|---------------------------|-------|
| `__editor_maybe_static`   | Is `static` when the editor is disabled.
| `__updater_maybe_static`  | Is `static` when the updater is disabled. Unused.
| `__utils_maybe_static`    | Is `static` when the utils are disabled. Unused.

These macros are typically used for function/variable definitions and the header
declarations of these functions/variables are typically enabled by an `#ifdef`
for the particular build option that expects them.

Definition (board.c):
```
__editor_maybe_static
int load_board_direct(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id)
{
  ...
}
```

Declaration (board.h):
```
#ifdef CONFIG_EDITOR
CORE_LIBSPEC int load_board_direct(struct world *mzx_world,
 struct board *cur_board, struct zip_archive *zp, int savegame,
 int file_version, unsigned int board_id);
#endif /* CONFIG_EDITOR */
```

### Cross-platform threading

For general usage see `src/thread_sdl.h` or `src/thread_pthread.h`. To use
threading in a file, just include `src/platform.h`; this will include the
appropriate platform-specific thread implementation.

Use the macro `THREAD_RES` as the return type of the thread function and `THREAD_RETURN;` to
exit the thread function, even if control would otherwise reach the end of the function.
This is necessary due to some platforms expecting a pointer, some expecting an integer,
and some that use no return type at all.

In general, parts of MegaZeux likely to be used in a multithreaded context
should be thread-safe (particularly the `check_alloc` series of functions and
the `src/nostdc++.cpp` implementations of new/delete, which internally use
`check_malloc`). Calling anything related to MZX gameplay from a thread should
generally be avoided.
