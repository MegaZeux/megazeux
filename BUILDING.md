Building MegaZeux
=================

MegaZeux can currently be built for Microsoft Windows, Linux, macOS,
NetBSD, FreeBSD, OpenBSD, Haiku, Amiga OS 4, Android, HTML5, MS-DOS (DJGPP),
NDS, 3DS, Wii, Wii U, Switch, PSP, Vita, Dreamcast, GP2X, and Pandora.

MegaZeux has three buildsystems--the GNU Make buildsystem for building most
ports, the Xcode project (`arch/xcode/`) for development and release builds
on macOS, and the MSVC project (`arch/msvc/`) for testing on Windows. This
document applies primarily to the GNU Make buildsystem.

For architecture specific build instructions, please see `arch/[platform]/README`.
[The DigitalMZX wiki](https://www.digitalmzx.com/wiki/Compiling_MegaZeux) has
more info, particularly for setting up console toolchains.


Requirements
------------

For the GNU Make buildsystem, most versions of these tools from the
past 20 years work with little issue. You should almost always use the
most recent versions available.

- POSIX-compatible base system e.g. Linux, macOS, BSD, MSYS2:
  - **GNU coreutils**: <=4.5.3 or newer (packaging MinGW requires `sha256sum`, 6.0+)
  - **BusyBox**: <=1.22 or newer (packaging MinGW requires `sha256sum`, 1.14.0+)
  - (**BSD** `sha256` or **Perl** `shasum -a256` also works.)
- GNU-compatible C and C++ compilers:
  - **GCC**: 3.4 or newer
  - **clang/LLVM**
- **GNU binutils** or **LLVM** or equivalent
- **GNU Make**: <=3.79 or newer
- **pkgconf** or **pkg-config** (SDL 3 builds only)
- **zip** or **7za** (packaging only)
- **tar** and **xz** (packaging source only)
- **ImageMagick** 7.0 or newer (rebuilding icons only)
  - Older versions may work if `MAGICK=convert` is provided to Make.
  - **GraphicsMagick** does not work as a substitute (no parentheses or clone).

Dependencies:

- **SDL** (all ports except DJGPP, NDS, 3DS, Wii, and Dreamcast)
  [(link)](https://www.libsdl.org/)
  - **SDL 3** (with `--enable-sdl3`)
  - **SDL 2** (default)
  - **SDL 1.2**: <=1.2.5 or newer (with `--enable-sdl1`)
    - Clipboard support for SDL 1.2 in Linux additionally requires X11 headers.
- **zlib**: 1.2.0 or newer
  [(link)](https://www.zlib.net/)
- **libpng**: 1.2.3 or newer (or `--disable-libpng`)
  [(link)](http://www.libpng.org/pub/png/libpng.html)
- **libogg**: 1.0 or newer (or **stb_vorbis** or `--disable-vorbis`)
  [(link)](https://xiph.org/downloads/)
- **libvorbis**: 1.2.2 or newer (or **Tremor** or **stb_vorbis** or `--disable-vorbis`)
  [(link)](https://xiph.org/downloads/)

For MinGW, DJGPP, and Darwin (macOS/Mac OS X), plus the Xcode and MSVC projects,
prebuilt dependencies can be found [here](https://github.com/AliceLR/megazeux-dependencies).
See below or the platform-specific README for more info.


Configuring and building
------------------------

All operations must be run within a POSIX-compatible shell environment
such as Bash, BusyBox, ksh, MSYS2 terminal, or similar.

Configure MegaZeux with the script `config.sh` located in the source root:
```sh
# You may need to specify --prefix [dir] or other options.
# For debug builds, omit --enable-release and --enable-lto.

./config.sh --platform [platform] --enable-release --enable-lto

# For a full list of supported options and platforms:
./config.sh
```
Several platforms have shell scripts named `CONFIG.[arch]` located in their
`arch/[platform]/` directory, which will run `config.sh` with appropriate
default configurations.

Compile MegaZeux with GNU Make:
```sh
# Use gmake on NetBSD et al.
# For verbose output, provide V=1. Parallel builds (-j#) are supported.
make

# For some platforms, it's safe to override the compilers if needed.
# (Most platforms that prefer clang will automatically select it.)
CC=clang CXX=clang++ make
```

MegaZeux comes with two regression test systems to verify that MegaZeux works
(for platforms and build environments where executables can be run locally).
The unit tests located in `unit/` are compiled to `mzxunit(.exe)` and then
run standalone. The test worlds located in `testworlds/` are run via MZXRun.
Both require data files located in their respective directory trees.
```sh
# Perform the unit tests and then run the test worlds. (-j# supported)
make test

# Perform just the unit tests. (-j# supported)
make unit

# Perform just the test worlds.
make testworlds
```


Installing
----------

For Linux/BSD/etc. using the `unix` platform and macOS/Mac OS X using the
`darwin` platform, MegaZeux can be installed to the install root that was
provided to the `--prefix` option. (Note the default install root is `/usr`,
so you may want to explicitly change that to `/usr/local` or similar):
```sh
# As superuser:
make install

# Can be reversed:
make uninstall
```


Packaging
---------

For all other platforms, MegaZeux is typically packaged instead.
MegaZeux's packaging varies from platform to platform.

First, for platforms that don't automatically separate or strip debug info
(mainly MinGW and DJGPP), separate the debug info. (For MinGW, this also runs
the locally compiled tool `pefix(.exe)` to strip PE timestamps.)
```sh
make debuglink -j8
```

Package the MegaZeux build into a zip archive. The prepared build directory can
be found at `build/[platform]/` and the archive at `build/dist/[platform]/`:
```sh
make archive
```

To prepare a directory (in `build/[platform]/`) for packaging without archiving
it, this can usually be used instead:
```sh
make build
```

To package a source tarball from a local Git repository:
```sh
make source
```


Platform-specific notes
-----------------------

Check `arch/[platform]/README[.md]` for more detailed platform-specific
information.

### Debian-based distributions

.deb packages can be generated with a single command.
See `debian/README` for more information.

### RPM-based distributions

.rpm packages can be generated with a single command:
```sh
rpmbuild -bb --build-in-place megazeux.spec
```

### Other Linux/BSD

Use platform `unix` for `make install` or `unix-devel` for running directly
from the source directory.

### MinGW and DJGPP

Download the latest dependencies tarball from
[here](https://github.com/AliceLR/megazeux-dependencies) and extract it into
`scripts/deps/`. Provide the following to `config.sh`:

- MINGW64: `--platform win64 --prefix scripts/deps/mingw/x64`
- MINGW32: `--platform win32 --prefix scripts/deps/mingw/x86`
- DJGPP: set `DJGPP` environment variable to `scripts/deps/djgpp/i386`
  and run `arch/djgpp/CONFIG.DJGPP` from the base directory.

When cross-compiling from Linux, use platforms `mingw64` and `mingw32` instead.

Additionally, DJGPP currently requires a copy of CWSDPMI.EXE to be placed in
`arch/djgpp/` for packaging.

### Darwin (macOS/Mac OS X via GNU Make)

Use platform `darwin` for `make install`, `darwin-devel` for running from the
source directory, and `darwin-dist` to build a portable multi-architecture .app
for distribution (see `arch/darwin/README.md` for more info on `darwin-dist`).

All three variants require Xcode to be installed with available command line
tools. It is easiest to get the dependency libraries from MacPorts (prefix
`/opt/local`) or from the MegaZeux prebuilt dependencies.

For `darwin-dist` (and optionally `darwin-devel`), download the latest
dependencies tarball from [here](https://github.com/AliceLR/megazeux-dependencies)
and extract it into `scripts/deps/`. In addition to the requirements listed in
the general requirements section, `darwin-dist` also requires dylibbundler,
which (along with other tools) is easiest to get via MacPorts.

### Other notes

Amiga OS 4, GP2X, and Pandora have not been tested recently and may have bitrot.
OpenSolaris was previously mentioned in the list of working platforms, but
neither old OpenSolaris nor modern Solaris can be verified currently.

MegaZeux builds in ancient environments with a little manual effort, if
for some reason you actually need this. The oldest tested is CentOS 3:

- Partial triage has been done on GCC 3.2 and 3.3, but this is low priority:
  - 3.3: `-Wextra` added in 3.4 (trivial but left as a canary)
  - 3.3: `-Wdeclaration-after-statement` added in 3.4
  - 3.3: requires `--disable-stack-protector`
  - 3.3: requires `--disable-modular`
    (`CORE_LIBSPEC export struct graphics graphics_data` doesn't work)
  - 3.2: `-std=gnu++98` added in 3.3
  - 3.2: `-Wunused-macros` added in 3.3
  - 3.2: `__attribute__(visibility("default"))` also doesn't work
- **zlib** <1.2.0 requires replacing/disabling `deflateBound` and `infback9`
  - The numeric version macros didn't exist at this point; `ZLIB_VERNUM` was
    added in 1.2.0.2 and the rest later.
- **libpng** <1.2.3 works if `libpng-config` is backported or if
  `LIBPNG_CFLAGS` and `LIBPNG_LDFLAGS` are manually supplied to `platform.inc`.
- **libvorbis**: <1.2.2 works if `vorbis_version_string` is manually disabled.
  libvorbis has never had numeric version macros to test.
