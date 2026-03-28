/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2008 Simon Parzer <simon.parzer@gmail.com>
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

#include "SDLmzx.h"
#include "../platform.h"
#include "../../util.h"

#ifdef CONFIG_PSP
#include <psppower.h>
#endif

#ifdef CONFIG_GP2X
#include <unistd.h> //for chdir, execl
#endif

#ifdef CONFIG_WII
#include <sys/iosupport.h>
#include <fat.h>
#endif

/* Verify that SDL's endianness matches what platform_endian.h claims... */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN && PLATFORM_BYTE_ORDER != PLATFORM_BIG_ENDIAN
#error Endian mismatch: SDL detected big, but MZX detected little. Report this!
#endif
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && PLATFORM_BYTE_ORDER != PLATFORM_LIL_ENDIAN
#error Endian mismatch: SDL detected little, but MZX detected big. Report this!
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void delay(uint32_t ms)
{
  emscripten_sleep(ms);
}
#else
void delay(uint32_t ms)
{
  SDL_Delay(ms);
}
#endif

uint64_t get_ticks(void)
{
  // SDL_GetTicks returns a 64-bit value in SDL3, but
  // SDL_GetTicks64 had to be used prior to that.
#if SDL_VERSION_ATLEAST(2,0,18) && !SDL_VERSION_ATLEAST(3,0,0)
  return SDL_GetTicks64();
#else
  return SDL_GetTicks();
#endif
}

/* TODO: Amiga is missing these SDL functions, MorphOS has them hardcoded.
 * MinGW currently is the only platform to use these functions. */
#if !defined(CONFIG_AMIGA)

#if SDL_VERSION_ATLEAST(3,0,0)
typedef SDL_SharedObject *dso_library_ptr;
#else
typedef void *dso_library_ptr;
#endif

/* Note: for OpenGL, use (SDL_)GL_LoadLibrary instead. */
struct dso_library *platform_load_library(const char *name)
{
  dso_library_ptr handle = SDL_LoadObject(name);
  if(handle)
    return (struct dso_library *)handle;
  return NULL;
}

void platform_unload_library(struct dso_library *library)
{
  dso_library_ptr handle = (dso_library_ptr)library;
  SDL_UnloadObject(handle);
}

/* Note: for OpenGL, use (SDL_)GL_GetProcAddress (via gl_load_syms) instead. */
boolean platform_load_function(struct dso_library *library,
 const struct dso_syms_map *syms_map)
{
  dso_library_ptr handle = (dso_library_ptr)library;

#if SDL_VERSION_ATLEAST(3,0,0)
  SDL_FunctionPointer *dest = (SDL_FunctionPointer *)syms_map->sym_ptr.value;
#else
  void **dest = (void **)syms_map->sym_ptr.in;
#endif
  if(!syms_map->name || !dest)
    return false;

  *dest = SDL_LoadFunction(handle, syms_map->name);
  if(!*dest)
  {
    debug("--DSO--- failed to load: %s\n", syms_map->name);
    return false;
  }
  return true;
}
#endif /* !CONFIG_AMIGA */

#ifdef __WIN32__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/**
 * Set DPI awareness for Windows to avoid bad window scaling and various
 * fullscreen-related bugs. This function was added in Windows Vista so it
 * needs to be dynamically loaded.
 */
static void set_dpi_aware(void)
{
  BOOL (*_SetProcessDPIAware)(void) = NULL;

  struct dso_library *handle = platform_load_library("User32.dll");
  if(handle)
  {
    struct dso_syms_map sym = { "SetProcessDPIAware", { &_SetProcessDPIAware } };

    if(platform_load_function(handle, &sym) && !_SetProcessDPIAware())
    {
      warn("failed to SetProcessDPIAware!\n");
    }
    else

    if(!_SetProcessDPIAware)
      debug("couldn't load SetProcessDPIAware.\n");

    platform_unload_library(handle);
  }
  else
    debug("couldn't load User32.dll: %s\n", SDL_GetError());
}
#endif

static inline boolean sdl_init(Uint32 flags)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  return SDL_Init(flags);
#else
  return SDL_Init(flags) >= 0;
#endif
}

boolean platform_init(void)
{
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;

#if SDL_VERSION_ATLEAST(2,0,0)
  flags |= SDL_INIT_GAMEPAD;
#endif

#ifdef CONFIG_PSP
  scePowerSetClockFrequency(333, 333, 166);
#endif

#ifdef CONFIG_WII
  if(!fatInitDefault())
    return false;
#endif

#if defined(DEBUG) && !SDL_VERSION_ATLEAST(3,0,0)
  // Removed in SDL 3.
  flags |= SDL_INIT_NOPARACHUTE;
#endif

#ifdef CONFIG_AUDIO
  flags |= SDL_INIT_AUDIO;
#endif

#ifdef __WIN32__
  set_dpi_aware();
#endif

  if(!sdl_init(flags))
  {
    debug("Failed to initialize SDL; attempting with joystick support disabled: %s\n", SDL_GetError());

    // try again without joystick support
    flags &= ~SDL_INIT_JOYSTICK;
#if SDL_VERSION_ATLEAST(2,0,0)
    flags &= ~SDL_INIT_GAMEPAD;
#endif

    if(!sdl_init(flags))
    {
      warn("Failed to initialize SDL: %s\n", SDL_GetError());
      return false;
    }
  }
  return true;
}

void platform_quit(void)
{
  SDL_Quit();

#ifdef CONFIG_WII
  {
    int i;

    for(i = 0; i < STD_MAX; i++)
      if(devoptab_list[i] && devoptab_list[i]->chdir_r)
        fatUnmount(devoptab_list[i]->name);
  }
#endif

#ifdef CONFIG_GP2X
  chdir("/usr/gp2x");
  execl("/usr/gp2x/gp2xmenu", "/usr/gp2x/gp2xmenu", NULL);
#endif
}
