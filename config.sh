#!/bin/sh

### CONFIG.SH HELP TEXT #######################################################

usage() {
	echo "usage: ./config.sh --platform [platform] <--prefix [dir]> <--sysconfdir [dir]>"
	echo "                                         <--gamesdir [dir]> <--bindir [dir]>"
	echo "                                         <--sharedir [dir]> <--licensedir [dir]>"
	echo "                                         <--host [name]> <options..>"
	echo
	echo "  --prefix       Install prefix and where dependencies should be found. (/usr)"
	echo "  --sysconfdir   Where the config should be read from. (/etc)"
	echo "  --gamesdir     Where binaries should be installed. ([prefix]/games)"
	echo "  --libdir       Where libraries should be installed. ([prefix]/lib)"
	echo "  --bindir       Where utilities should be installed. ([prefix]/bin)"
	echo "  --sharedir     Where resources should be installed. ([prefix]/share)"
	echo "  --licensedir   Where licenses should be installed. ([sharedir]/licenses)"
	echo "  --metainfodir  Where metainfo should be installed. ([sharedir]/metainfo)"
	echo "  --host         Specify cross toolchain prefix for Linux et al. (none)"
	echo
	echo "  Install directories can be disregarded for builds for platforms"
	echo "  with a monolithic directory structure e.g. Windows."
	echo
	echo "Supported [platform] values:"
	echo
	echo "  win32          Microsoft Windows (x86)"
	echo "  win64          Microsoft Windows (x64)"
	echo "  mingw32        Use MinGW32 on Linux, to build for win32"
	echo "  mingw64        Use MinGW64 on Linux, to build for win64"
	echo "  unix           Unix-like / Linux / Solaris / BSD / Embedded"
	echo "  unix-devel     As above, but for running from current dir"
	echo "  darwin         Mac OS X Unix-like install"
	echo "  darwin-devel   Mac OS X running from current dir"
	echo "  darwin-dist    Mac OS X multiarchitecture .app (see arch/darwin/README.md)"
	echo "  psp            Experimental PSP port"
	echo "  gp2x           Experimental GP2X port"
	echo "  nds            Experimental NDS port"
	echo "  nds-blocksds   Experimental NDS (BlocksDS) port"
	echo "  3ds            Experimental 3DS port"
	echo "  switch         Experimental Switch port"
	echo "  wii            Experimental Wii port"
	echo "  wiiu           Experimental Wii U port"
	echo "  psvita         Experimental PS Vita port"
	echo "  dreamcast      Experimental Dreamcast port"
	echo "  amiga          Experimental AmigaOS 4 port"
	echo "  android        Experimental Android port"
	echo "  pandora        Experimental Pandora port"
	echo "  emscripten     Experimental HTML5 (Emscripten) port"
	echo "  djgpp          Experimental DOS port"
	echo
	echo "Supported <option> values (negatives can be used):"
	echo
	echo "Optimization and debug options:"
	echo "  --as-needed-hack          Pass --as-needed through to GNU ld."
	echo "  --enable-release          Optimize and remove debugging code."
	echo "  --enable-verbose          Build system is always verbose (V=1)."
	echo "  --optimize-speed          Perform speed optimizations (-O3, default)."
	echo "  --optimize-size           Perform size optimizations (-Os)."
	echo "  --enable-lto              Perform link-time optimizations (-flto)."
	echo "  --enable-asan             Enable AddressSanitizer for debug builds."
	echo "  --enable-msan             Enable MemorySanitizer for debug builds."
	echo "  --enable-tsan             Enable ThreadSanitizer for debug builds."
	echo "  --enable-ubsan            Enable UndefinedBehaviorSanitizer for debug builds."
	echo "  --enable-analyzer         Enable -fanalyzer (requires GCC 10+)."
	echo "  --enable-trace            Enable trace logging for debug builds."
	echo "  --enable-stdio-redirect   Redirect console logging to stdout.txt/stderr.txt."
	echo "  --disable-stack-protector Disable stack protector safety checks."
	echo
	echo "Platform-dependent options:"
	echo "  --enable-sdl              Enable SDL backend, typically SDL2 (default)."
	echo "  --enable-sdl3             Enable SDL3 backend (any version)."
	echo "  --enable-sdl2             Enable SDL2 backend (any version)."
	echo "  --enable-sdl1             Enable SDL1 backend (1.2.x)."
	echo "  --disable-sdl             Disable SDL dependencies and features."
	echo "  --enable-egl              Enable EGL backend (disables SDL)."
	echo "  --disable-x11             Disable X11, removing binary dependency."
	echo "  --disable-pthread         Use SDL's threads/locking instead of pthread."
	echo "  --disable-icon            Disable runtime window icon loading."
	echo "  --disable-modular         Disable dynamically shared objects."
	echo "  --enable-pledge           Enable experimental OpenBSD pledge(2) support"
	echo
	echo "General options:"
	echo "  --disable-datestamp       Disable adding date to version."
	echo "  --disable-editor          Disable the built-in editor."
	echo "  --disable-mzxrun          Disable generation of separate MZXRun."
	echo "  --disable-helpsys         Disable the built-in help system."
	echo "  --disable-utils           Disable compilation of utils."
	echo "  --disable-check-alloc     Disables memory allocator error handling."
	echo "  --disable-counter-hash    Disables hash tables for counter/string lookups."
	echo "  --disable-vfs             Disables MZX's built-in virtual filesystem."
	echo "  --enable-extram           Enable board memory compression and storage hacks."
	echo "  --enable-meter            Enable load/save meter display."
	echo "  --enable-debytecode       Enable experimental 'debytecode' transform."
	echo
	echo "Graphics options:"
	echo "  --enable-gles             Enable hacks for OpenGL ES platforms."
	echo "  --disable-software        Disable software renderer."
	echo "  --disable-softscale       Disable SDL accelerated software renderer."
	echo "  --enable-sdlaccel         Enable SDL accelerated hardware renderer."
	echo "  --disable-gl              Disable all GL renderers."
	echo "  --disable-gl-fixed        Disable GL renderers for fixed-function h/w."
	echo "  --disable-gl-prog         Disable GL renderers for programmable h/w."
	echo "  --disable-overlay         Disable SDL 1.2 overlay renderers."
	echo "  --enable-gp2x             Enables half-res software renderer."
	echo "  --disable-dos-svga        On the DOS platform, disable SVGA software renderer."
	echo "  --disable-libpng          Disable PNG screendump support."
	echo "  --disable-screenshots     Disable the screenshot hotkey."
	echo "  --enable-fps              Enable frames-per-second counter."
	echo
	echo "Audio options:"
	echo "  --disable-audio           Disable all audio (sound + music)."
	echo "  --disable-xmp             Disable XMP music engine."
	echo "  --enable-modplug          Enables ModPlug music engine."
	echo "  --enable-mikmod           Enables MikMod music engine."
	echo "  --enable-openmpt          Enables OpenMPT music engine."
	echo "  --disable-rad             Disables Reality Adlib Tracker (RAD) support."
	echo "  --disable-vorbis          Disable ogg/vorbis support."
	echo "  --enable-tremor           Use libvorbisidec instead of libvorbis."
	echo "  --enable-tremor-lowmem    Use libvorbisidec (lowmem) instead of libvorbis."
	echo "  --enable-stb-vorbis       Use stb_vorbis instead of libvorbis."
	echo
	echo "Network options:"
	echo "  --disable-network         Disable networking abilities."
	echo "  --disable-updater         Disable built-in updater."
	echo "  --disable-getaddrinfo     Disable getaddrinfo for name resolution."
	echo "  --disable-poll            Disable poll for socket monitoring."
	echo "  --disable-ipv6            Disable IPv6 support."
	echo
	echo "e.g.: ./config.sh --platform unix --prefix /usr"
	echo "                  --sysconfdir /etc --disable-x11"
	echo "e.g.: ./config.sh --platform win32"
	echo
}

### CHOMP CONFIG.SH FLAGS #####################################################

#
# Default settings for flags (used if unspecified)
#
PLATFORM=""
TOOL_PREFIX=""
PREFIX="/usr"
PREFIX_IS_SET="false"
SYSCONFDIR="/etc"
SYSCONFDIR_IS_SET="false"
GAMESDIR_IS_SET="false"
GAMESDIR_IN_PREFIX="/games"
GAMESDIR="${PREFIX}${GAMESDIR_IN_PREFIX}"
LIBDIR_IS_SET="false"
LIBDIR_IN_PREFIX="/lib"
LIBDIR="${PREFIX}${LIBDIR_IN_PREFIX}"
BINDIR_IS_SET="false"
BINDIR_IN_PREFIX="/bin"
BINDIR="${PREFIX}${BINDIR_IN_PREFIX}"
SHAREDIR_IS_SET="false"
SHAREDIR_IN_PREFIX="/share"
SHAREDIR="${PREFIX}${SHAREDIR_IN_PREFIX}"
LICENSEDIR_IS_SET="false"
LICENSEDIR_IN_SHAREDIR="/licenses"
LICENSEDIR="${SHAREDIR}${LICENSEDIR_IN_SHAREDIR}"
METAINFODIR_IS_SET="false"
METAINFODIR_IN_SHAREDIR="/metainfo"
METAINFODIR="${SHAREDIR}${METAINFODIR_IN_SHAREDIR}"
USERCONFFILE=""
DATE_STAMP="true"
AS_NEEDED="false"
RELEASE="false"
OPT_SIZE="false"
LTO="false"
SANITIZER="false"
ANALYZER="false"
STACK_PROTECTOR="true"
PLEDGE="false"
PLEDGE_UTILS="true"
EDITOR="true"
MZXRUN="true"
HELPSYS="true"
UTILS="true"
X11="true"
SOFTWARE="true"
SOFTSCALE="true"
SDLACCEL="false"
GL="true"
GL_FIXED="true"
GL_PROGRAM="true"
OVERLAY="true"
GP2X="false"
SCREENSHOTS="true"
XMP="true"
MODPLUG="false"
MIKMOD="false"
OPENMPT="false"
REALITY="true"
LIBPNG="true"
AUDIO="true"
VORBIS="true"
PTHREAD="true"
ICON="true"
MODULAR="true"
UPDATER="true"
NETWORK="true"
GETADDRINFO="true"
POLL="true"
IPV6="true"
VERBOSE="false"
EXTRAM="false"
METER="false"
SDL="true"
EGL="false"
GLES="false"
CHECK_ALLOC="true"
COUNTER_HASH="true"
DEBYTECODE="false"
TRACE_LOGGING="false"
STDIO_REDIRECT="false"
GAMECONTROLLERDB="true"
FPSCOUNTER="false"
LAYER_RENDERING="true"
DOS_SVGA="true"
DOS_ROOTS="false"
VFS="true"

#
# User may override above settings
#
while [ "$1" != "" ]; do
	# e.g. --platform unix-devel
	if [ "$1" = "--platform" ]; then
		shift
		PLATFORM="$1"
	fi

	# e.g. --prefix /usr
	if [ "$1" = "--prefix" ]; then
		shift
		PREFIX="$1"
		PREFIX_IS_SET="true"
	fi

	# e.g. --sysconfdir /etc
	if [ "$1" = "--sysconfdir" ]; then
		shift
		SYSCONFDIR="$1"
		SYSCONFDIR_IS_SET="true"
	fi

	# e.g. --gamesdir /usr/games
	if [ "$1" = "--gamesdir" ]; then
		shift
		GAMESDIR="$1"
		GAMESDIR_IS_SET="true"
	fi

	# e.g. --libdir /usr/lib
	if [ "$1" = "--libdir" ]; then
		shift
		LIBDIR="$1"
		LIBDIR_IS_SET="true"
	fi

	# e.g. --bindir /usr/bin
	if [ "$1" = "--bindir" ]; then
		shift
		BINDIR="$1"
		BINDIR_IS_SET="true"
	fi

	# e.g. --sharedir /usr/share
	if [ "$1" = "--sharedir" ]; then
		shift
		SHAREDIR="$1"
		SHAREDIR_IS_SET="true"
	fi

	# e.g. --licensedir /usr/share/licenses
	if [ "$1" = "--licensedir" ]; then
		shift
		LICENSEDIR="$1"
		LICENSEDIR_IS_SET="true"
	fi

	# e.g. --metainfodir /usr/share/metainfo
	if [ "$1" = "--metainfodir" ]; then
		shift
		METAINFODIR="$1"
		METAINFODIR_IS_SET="true"
	fi

	# e.g. --host arm-none-eabi
	if [ "$1" = "--host" ]; then
		shift
		TOOL_PREFIX="$1"
	fi

	[ "$1" = "--as-needed-hack" ] && AS_NEEDED="true"

	[ "$1" = "--enable-release" ]  && RELEASE="true"
	[ "$1" = "--disable-release" ] && RELEASE="false"

	[ "$1" = "--optimize-size" ]  && OPT_SIZE="true"
	[ "$1" = "--optimize-speed" ] && OPT_SIZE="false"

	[ "$1" = "--enable-lto" ]  && LTO="true"
	[ "$1" = "--disable-lto" ] && LTO="false"

	[ "$1" = "--enable-asan" ]  && SANITIZER="address"
	[ "$1" = "--disable-asan" ] && SANITIZER="false"

	[ "$1" = "--enable-msan" ]  && SANITIZER="memory"
	[ "$1" = "--disable-msan" ] && SANITIZER="false"

	[ "$1" = "--enable-tsan" ] &&  SANITIZER="thread"
	[ "$1" = "--disable-tsan" ] && SANITIZER="false"

	[ "$1" = "--enable-ubsan" ] &&  SANITIZER="undefined"
	[ "$1" = "--disable-ubsan" ] && SANITIZER="false"

	[ "$1" = "--enable-analyzer" ] &&  ANALYZER="true"
	[ "$1" = "--disable-analyzer" ] && ANALYZER="false"

	[ "$1" = "--enable-stack-protector" ]  && STACK_PROTECTOR="true"
	[ "$1" = "--disable-stack-protector" ] && STACK_PROTECTOR="false"

	[ "$1" = "--enable-pledge" ] &&  PLEDGE="true"  && PLEDGE_UTILS="true"
	[ "$1" = "--disable-pledge" ] && PLEDGE="false" && PLEDGE_UTILS="false"

	[ "$1" = "--disable-datestamp" ] && DATE_STAMP="false"
	[ "$1" = "--enable-datestamp" ]  && DATE_STAMP="true"

	[ "$1" = "--disable-editor" ] && EDITOR="false"
	[ "$1" = "--enable-editor" ]  && EDITOR="true"

	[ "$1" = "--disable-mzxrun" ] && MZXRUN="false"
	[ "$1" = "--enable-mzxrun" ]  && MZXRUN="true"

	[ "$1" = "--disable-helpsys" ] && HELPSYS="false"
	[ "$1" = "--enable-helpsys" ]  && HELPSYS="true"

	[ "$1" = "--disable-utils" ] && UTILS="false"
	[ "$1" = "--enable-utils" ] && UTILS="true"

	[ "$1" = "--disable-x11" ] && X11="false"
	[ "$1" = "--enable-x11" ]  && X11="true"

	[ "$1" = "--disable-software" ] && SOFTWARE="false"
	[ "$1" = "--enable-software" ]  && SOFTWARE="true"

	[ "$1" = "--disable-softscale" ] && SOFTSCALE="false"
	[ "$1" = "--enable-softscale" ]  && SOFTSCALE="true"

	[ "$1" = "--disable-sdlaccel" ] && SDLACCEL="false"
	[ "$1" = "--enable-sdlaccel" ]  && SDLACCEL="true"

	[ "$1" = "--disable-gl" ] && GL="false"
	[ "$1" = "--enable-gl" ]  && GL="true"

	[ "$1" = "--disable-gl-fixed" ] && GL_FIXED="false"
	[ "$1" = "--enable-gl-fixed" ]  && GL_FIXED="true"

	[ "$1" = "--disable-gl-prog" ] && GL_PROGRAM="false"
	[ "$1" = "--enable-gl-prog" ]  && GL_PROGRAM="true"

	[ "$1" = "--disable-overlay" ] && OVERLAY="false"
	[ "$1" = "--enable-overlay" ]  && OVERLAY="true"

	[ "$1" = "--disable-gp2x" ] && GP2X="false"
	[ "$1" = "--enable-gp2x" ]  && GP2X="true"

	[ "$1" = "--disable-screenshots" ] && SCREENSHOTS="false"
	[ "$1" = "--enable-screenshots" ]  && SCREENSHOTS="true"

	[ "$1" = "--disable-modplug" ] && MODPLUG="false"
	[ "$1" = "--enable-modplug" ]  && MODPLUG="true"

	[ "$1" = "--disable-mikmod" ] && MIKMOD="false"
	[ "$1" = "--enable-mikmod" ]  && MIKMOD="true"

	[ "$1" = "--disable-xmp" ] && XMP="false"
	[ "$1" = "--enable-xmp" ]  && XMP="true"

	[ "$1" = "--disable-openmpt" ] && OPENMPT="false"
	[ "$1" = "--enable-openmpt" ]  && OPENMPT="true"

	[ "$1" = "--disable-rad" ] && REALITY="false"
	[ "$1" = "--enable-rad" ]  && REALITY="true"

	[ "$1" = "--disable-libpng" ] && LIBPNG="false"
	[ "$1" = "--enable-libpng" ]  && LIBPNG="true"

	[ "$1" = "--disable-audio" ] && AUDIO="false"
	[ "$1" = "--enable-audio" ]  && AUDIO="true"

	[ "$1" = "--disable-vorbis" ] && VORBIS="false"
	[ "$1" = "--enable-vorbis" ]  && VORBIS="true"

	[ "$1" = "--disable-tremor" ] && VORBIS="true"
	[ "$1" = "--enable-tremor" ]  && VORBIS="tremor"

	[ "$1" = "--disable-tremor-lowmem" ] && VORBIS="true"
	[ "$1" = "--enable-tremor-lowmem" ]  && VORBIS="tremor-lowmem"

	[ "$1" = "--disable-stb-vorbis" ] && VORBIS="true"
	[ "$1" = "--enable-stb-vorbis" ]  && VORBIS="stb_vorbis"

	[ "$1" = "--disable-pthread" ] && PTHREAD="false"
	[ "$1" = "--enable-pthread" ]  && PTHREAD="true"

	[ "$1" = "--disable-icon" ] && ICON="false"
	[ "$1" = "--enable-icon" ]  && ICON="true"

	[ "$1" = "--disable-modular" ] && MODULAR="false"
	[ "$1" = "--enable-modular" ]  && MODULAR="true"

	[ "$1" = "--disable-updater" ] && UPDATER="false"
	[ "$1" = "--enable-updater" ]  && UPDATER="true"

	[ "$1" = "--disable-network" ] && NETWORK="false"
	[ "$1" = "--enable-network" ]  && NETWORK="true"

	[ "$1" = "--disable-getaddrinfo" ] && GETADDRINFO="false"
	[ "$1" = "--enable-getaddrinfo" ]  && GETADDRINFO="true"

	[ "$1" = "--disable-poll" ] && POLL="false"
	[ "$1" = "--enable-poll" ]  && POLL="true"

	[ "$1" = "--disable-ipv6" ] && IPV6="false"
	[ "$1" = "--enable-ipv6" ]  && IPV6="true"

	[ "$1" = "--disable-verbose" ] && VERBOSE="false"
	[ "$1" = "--enable-verbose" ]  && VERBOSE="true"

	[ "$1" = "--enable-extram" ]  && EXTRAM="true"
	[ "$1" = "--disable-extram" ] && EXTRAM="false"

	[ "$1" = "--enable-meter" ]  && METER="true"
	[ "$1" = "--disable-meter" ] && METER="false"

	[ "$1" = "--disable-sdl" ] && SDL="false"
	[ "$1" = "--enable-sdl" ]  && SDL="true"
	[ "$1" = "--enable-sdl1" ] && SDL="1"
	[ "$1" = "--enable-sdl2" ] && SDL="2"
	[ "$1" = "--enable-sdl3" ] && SDL="3"

	[ "$1" = "--enable-egl" ]  && EGL="true"
	[ "$1" = "--disable-egl" ] && EGL="false"

	[ "$1" = "--enable-gles" ]  && GLES="true"
	[ "$1" = "--disable-gles" ] && GLES="false"

	[ "$1" = "--disable-check-alloc" ] && CHECK_ALLOC="false"
	[ "$1" = "--enable-check-alloc" ]  && CHECK_ALLOC="true"

	[ "$1" = "--enable-counter-hash" ]  && COUNTER_HASH="true"
	[ "$1" = "--disable-counter-hash" ] && COUNTER_HASH="false"

	[ "$1" = "--enable-vfs" ]  && VFS="true"
	[ "$1" = "--disable-vfs" ] && VFS="false"

	[ "$1" = "--enable-debytecode" ]  && DEBYTECODE="true"
	[ "$1" = "--disable-debytecode" ] && DEBYTECODE="false"

	# TODO: compatibility options, eventually remove.
	[ "$1" = "--enable-libsdl2" ]  && SDL="2"
	[ "$1" = "--disable-libsdl2" ] && SDL="1"

	[ "$1" = "--enable-trace" ]  && TRACE_LOGGING="true"
	[ "$1" = "--disable-trace" ] && TRACE_LOGGING="false"

	[ "$1" = "--enable-stdio-redirect" ]  && STDIO_REDIRECT="true"
	[ "$1" = "--disable-stdio-redirect" ] && STDIO_REDIRECT="false"

	[ "$1" = "--enable-fps" ]  && FPSCOUNTER="true"
	[ "$1" = "--disable-fps" ] && FPSCOUNTER="false"

	[ "$1" = "--enable-dos-svga" ]  && DOS_SVGA="true"
	[ "$1" = "--disable-dos-svga" ] && DOS_SVGA="false"

	if [ "$1" = "--help" ]; then
		usage
		exit 0
	fi

	shift
done

#
# Platform cannot be sanely omitted
#
if [ "$PLATFORM" = "" ]; then
	usage
	exit 0
fi

#
# Derive unspecified install dirs from specified dirs.
#
if [ "$BINDIR_IS_SET" = "false" ]; then
	BINDIR="${PREFIX}${BINDIR_IN_PREFIX}"
fi
if [ "$GAMESDIR_IS_SET" = "false" ]; then
	GAMESDIR="${PREFIX}${GAMESDIR_IN_PREFIX}"
fi
if [ "$LIBDIR_IS_SET" = "false" ]; then
	LIBDIR="${PREFIX}${LIBDIR_IN_PREFIX}"
fi
if [ "$SHAREDIR_IS_SET" = "false" ]; then
	SHAREDIR="${PREFIX}${SHAREDIR_IN_PREFIX}"
fi
if [ "$LICENSEDIR_IS_SET" = "false" ]; then
	LICENSEDIR="${SHAREDIR}${LICENSEDIR_IN_SHAREDIR}"
fi
if [ "$METAINFODIR_IS_SET" = "false" ]; then
	METAINFODIR="${SHAREDIR}${METAINFODIR_IN_SHAREDIR}"
fi

### PLATFORM DEFINITION #######################################################

rm -f platform.inc

if [ "$PLATFORM" = "win32"   ] || [ "$PLATFORM" = "win64" ] ||
   [ "$PLATFORM" = "mingw32" ] || [ "$PLATFORM" = "mingw64" ]; then
	# Auto-prefix for the MSYS2 MINGW32/MINGW64 environments if a prefix wasn't
	# provided. This helps avoid errors that occur when gcc or libs exist in
	# /usr, which is used by MSYS2 for the MSYS environment.
	if [ "$PREFIX_IS_SET" = "false" ] && [ -n "$MSYSTEM" ] &&
	   [ "$(uname -o)" = "Msys" ] && [ "$MSYSTEM" != "MSYS" ]; then
		case "$MSYSTEM" in
		  "MINGW32"|"MINGW64")
			[ "$PLATFORM" = "win32" ] && PREFIX="/mingw32"
			[ "$PLATFORM" = "win64" ] && PREFIX="/mingw64"
			;;
		  "CLANG32"|"CLANG64")
			[ "$PLATFORM" = "win32" ] && PREFIX="/clang32"
			[ "$PLATFORM" = "win64" ] && PREFIX="/clang64"
			;;
		  "CLANGARM64")
			[ "$PLATFORM" = "win64" ] && PREFIX="/clangarm64"
			;;
		  "UCRT64")
			[ "$PLATFORM" = "win64" ] && PREFIX="/ucrt64"
			;;
		  *)
			echo "WARNING: Unknown MSYSTEM '$MSYSTEM'!"
			;;
		esac
	fi

	[ "$PLATFORM" = "win32" ] || [ "$PLATFORM" = "mingw32" ] && ARCHNAME=x86
	[ "$PLATFORM" = "win64" ] || [ "$PLATFORM" = "mingw64" ] && ARCHNAME=x64
	[ "$PLATFORM" = "mingw32" ] && [ "$TOOL_PREFIX" = "" ] && TOOL_PREFIX=i686-w64-mingw32
	[ "$PLATFORM" = "mingw64" ] && [ "$TOOL_PREFIX" = "" ] && TOOL_PREFIX=x86_64-w64-mingw32
	PLATFORM="mingw"
	echo "#define PLATFORM \"windows-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=windows-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=$PLATFORM"                    >> platform.inc
elif [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "unix-devel" ] ||
     [ "$PLATFORM" = "darwin" ] || [ "$PLATFORM" = "darwin-devel" ]; then
	OS="$(uname -s)"
	MACH="$(uname -m)"
	DIRNAME=unix

	case "$OS" in
		"Linux")
			UNIX="linux"
			;;
		"FreeBSD")
			UNIX="freebsd"
			;;
		"OpenBSD")
			UNIX="openbsd"
			;;
		"NetBSD")
			UNIX="netbsd"
			;;
		"DragonFly")
			UNIX="dragonflybsd"
			;;
		"Haiku")
			UNIX="haiku"
			;;
		"Darwin")
			UNIX="darwin"
			DIRNAME="darwin"
			;;
		*)
			echo "WARNING: Should define proper UNIX name here!"
			UNIX="unix"
			;;
	esac

	if [ "$MACH" = "x86_64" ] || [ "$MACH" = "amd64" ]; then
		ARCHNAME=amd64
		#RAWLIBDIR=lib64
		# FreeBSD amd64 hack
		#[ "$UNIX" = "freebsd" ] && RAWLIBDIR=lib
	elif [ "$(echo "$MACH" | sed 's,i.86,x86,')" = "x86" ]; then
		ARCHNAME=x86
		#RAWLIBDIR=lib
	elif [ "$MACH" = "aarch64" ] || [ "$MACH" = "arm64" ]; then
		ARCHNAME=aarch64
	elif [ "$(echo "$MACH" | sed 's,^arm.*,arm,')" = "arm" ]; then
		ARCHNAME=arm
		#RAWLIBDIR=lib
	elif [ "$MACH" = "ppc" ]; then
		ARCHNAME=ppc
		#RAWLIBDIR=lib
	elif [ "$MACH" = "ppc64" ]; then
		ARCHNAME=ppc64
	elif [ "$MACH" = "mips" ]; then
		ARCHNAME=mips
	elif [ "$MACH" = "mips64" ]; then
		ARCHNAME=mips64
	elif [ "$MACH" = "m68k" ]; then
		ARCHNAME=m68k
	elif [ "$MACH" = "alpha" ]; then
		ARCHNAME=alpha
	elif [ "$MACH" = "hppa" ] || [ "$MACH" = "parisc" ]; then
		ARCHNAME=hppa
	elif [ "$MACH" = "sh4" ]; then
		ARCHNAME=sh4
	elif [ "$MACH" = "sparc" ]; then
		ARCHNAME=sparc
	elif [ "$MACH" = "sparc64" ]; then
		ARCHNAME=sparc64
	elif [ "$MACH" = "riscv64" ]; then
		ARCHNAME=riscv64
	elif [ "$MACH" = "s390" ]; then
		ARCHNAME=s390
	elif [ "$MACH" = "s390x" ]; then
		ARCHNAME=s390x
	elif [ "$MACH" = "loong64" ] || [ "$MACH" = "loongarch64" ]; then
		ARCHNAME=loong64
	else
		ARCHNAME=$MACH
		#RAWLIBDIR=lib
		echo "WARNING: Compiling on an unsupported architecture. Add a friendly MACH to config.sh."
	fi

	echo "#define PLATFORM \"$UNIX-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=$UNIX-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=$DIRNAME"                   >> platform.inc
elif [ "$PLATFORM" = "darwin-dist" ]; then
	# Multiarchitecture build--let the Makefile patch in a subplatform.

	echo "#define PLATFORM \"darwin-\" SUBPLATFORM"  > src/config.h
	echo "SUBPLATFORM=$PLATFORM"                    >> platform.inc
	echo "PLATFORM=darwin"                          >> platform.inc
elif [ "$PLATFORM" = "android" ]; then
	# Multiarchitecture build--let the Makefile patch in a subplatform.

	echo "#define PLATFORM \"android-\" SUBPLATFORM" > src/config.h
	echo "SUBPLATFORM=$PLATFORM"                    >> platform.inc
	echo "PLATFORM=$PLATFORM"                       >> platform.inc
else
	if [ ! -d "arch/$PLATFORM" ]; then
		echo "Invalid platform selection (see arch/)."
		exit 1
	fi

	echo "#define PLATFORM \"$PLATFORM\"" > src/config.h
	echo "SUBPLATFORM=$PLATFORM"         >> platform.inc
	echo "PLATFORM=$PLATFORM"            >> platform.inc
fi

echo "PREFIX:=$PREFIX" >> platform.inc

if [ "$TOOL_PREFIX" != "" ]; then
	echo "CROSS_COMPILE?=$TOOL_PREFIX-" >> platform.inc
fi

### SYSTEM DIRECTORIES ########################################################

#
# Some platforms (currently "unix", "darwin") are system installations.
# Their assets and licenses are located in subdirectories of their
# respective filesystem hierarchies. For other platforms, these files are
# located in the base directory of the archive (the executable location).
# These locations need to be hardcoded for various console platforms.
#
case "$PLATFORM" in
	# Set default directories for non-install platforms.
	# Most directories are irrelevant outside of install platforms.
	# Most of the time, only SHAREDIR needs to be overriden.
	"unix" | "darwin")
		;;
	*)
		GAMESDIR="."
		BINDIR="."
		LIBDIR="."
		SHAREDIR="."
		LICENSEDIR="."
		METAINFODIR="."
		if [ "$SYSCONFDIR_IS_SET" != "true" ]; then
			SYSCONFDIR="."
		fi
		;;
esac
case "$PLATFORM" in
	# Set custom per-platform paths.
	"unix" | "darwin")
		LIBDIR="${LIBDIR}/megazeux"
		USERCONFFILE=".megazeux-config"
		;;
	"nds" | "nds-blocksds")
		SHAREDIR=/games/megazeux
		;;
	"3ds")
		SHAREDIR=/3ds/megazeux
		;;
	"dreamcast")
		SHAREDIR=/cd
		;;
	"wii")
		SHAREDIR=/apps/megazeux
		;;
	"wiiu")
		SHAREDIR=fs:/vol/external01/wiiu/apps/megazeux
		;;
	"switch")
		SHAREDIR=/switch/megazeux
		;;
	"psvita")
		SHAREDIR="ux0:/data/megazeux"
		;;
	"darwin-dist")
		SHAREDIR="../Resources"
		LICENSEDIR="../Resources"
		SYSCONFDIR="../Resources"
		USERCONFFILE=".megazeux-config"
		;;
	"emscripten")
		SHAREDIR="/data"
		SYSCONFDIR="/data/etc"
		;;
	"android")
		STARTUPDIR="/storage/emulated/0"
		;;
esac

### SUMMARY OF OPTIONS ########################################################

echo "Building for platform:   $PLATFORM"
echo "Using prefix:            $PREFIX"
echo "      sharedir:          $SHAREDIR"
echo "      licensedir:        $LICENSEDIR"
if [ "$METAINFODIR" != "." ]; then
echo "      metainfodir:       $METAINFODIR"
fi
echo "      sysconfdir:        $SYSCONFDIR"
if [ "$GAMESDIR" != "." ]; then
echo "      gamesdir (mzx):    $GAMESDIR"
fi
if [ "$BINDIR" != "." ]; then
echo "      bindir (utils):    $BINDIR"
fi
if [ "$LIBDIR" != "." ]; then
echo "      libdir:            $LIBDIR"
fi
if [ -n "$STARTUPDIR" ]; then
echo "      startupdir:        $STARTUPDIR"
fi
if [ "$TOOL_PREFIX" != "" ]; then
echo "Using host:              $TOOL_PREFIX"
fi
echo

### GENERATE CONFIG.H HEADER ##################################################

. ./version.inc

#
# Set the version to build with
#

echo "#define VERSION \"$VERSION\"" >> src/config.h

if [ "$DATE_STAMP" = "true" ]; then
	echo "Stamping version with today's date."
	echo "#define VERSION_DATE \" ($(date -u +%Y%m%d))\"" >> src/config.h
else
	echo "Not stamping version with today's date."
fi

if command -v git >/dev/null 2>&1 && git rev-parse --short HEAD >/dev/null 2>&1; then
	echo "#define VERSION_HEAD \"$(git rev-parse --short HEAD)\"" >> src/config.h
fi

if [ "$PRERELEASE" = "1" ]; then
	echo "#define VERSION_PRERELEASE" >> src/config.h
fi

echo "#define CONFDIR \"$SYSCONFDIR/\"" >> src/config.h

if [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "darwin" ]; then
	echo "#define CONFFILE \"megazeux-config\""		>> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR/megazeux/\""		>> src/config.h
	echo "#define LICENSEDIR \"$LICENSEDIR/megazeux/\""	>> src/config.h
else
	echo "#define CONFFILE \"config.txt\""			>> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""			>> src/config.h
	echo "#define LICENSEDIR \"$LICENSEDIR\""		>> src/config.h
fi

if [ -n "$USERCONFFILE" ]; then
	echo "#define USERCONFFILE \"$USERCONFFILE\""		>> src/config.h
fi

if [ -n "$STARTUPDIR" ]; then
	echo "#define STARTUPDIR \"$STARTUPDIR\""		>> src/config.h
fi

#
# LIBDIR is required by several platforms that support modular builds.
# Some architectures define an "install" target, and need the rest.
#
echo "SYSCONFDIR=$SYSCONFDIR" >> platform.inc
echo "GAMESDIR=$GAMESDIR"     >> platform.inc
echo "LIBDIR=$LIBDIR"         >> platform.inc
echo "BINDIR=$BINDIR"         >> platform.inc
echo "SHAREDIR=$SHAREDIR"     >> platform.inc
echo "LICENSEDIR=$LICENSEDIR" >> platform.inc
echo "METAINFODIR=$METAINFODIR" >> platform.inc


### PLATFORM-SPECIFIC FEATURE CONFIG ##########################################

case "$PLATFORM" in
"darwin"*)
	echo "Force-disabling runtime icon loading (macOS)."
	ICON="false"

	echo "Force-disabling poll (macOS)."
	POLL="false"
	;;

"amiga")
	echo "Force-disabling getaddrinfo, poll, and IPv6 (Amiga)."
	GETADDRINFO="false"
	POLL="false"
	IPV6="false"
	;;

"android")
	echo "Force-disabling modular build (Android)."
	MODULAR="false"

	echo "Force-enabling OpenGL ES support (Android)."
	GLES="true"

	echo "Force-disabling runtime icon loading (Android)."
	ICON="false"
	;;

"emscripten")
	echo "Enabling Emscripten-specific hacks."
	echo "BUILD_EMSCRIPTEN=1" >> platform.inc

	echo "Force-disabling modular build (Emscripten)."
	MODULAR="false"

	echo "Force-disabling stack protector (Emscripten)."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading (Emscripten)."
	ICON="false"

	echo "Force-disabling virtual filesystem (Emscripten)."
	VFS="false"

	EDITOR="false"
	SCREENSHOTS="false"
	UPDATER="false"
	UTILS="false"

	GLES="true"
	GL_FIXED="false"
	;;

"nds"*)
	echo "Enabling NDS-specific hacks."
	echo "#define CONFIG_NDS" >> src/config.h
	echo "BUILD_NDS=1" >> platform.inc
	DOS_ROOTS="true"

	if [ "$PLATFORM" = "nds-blocksds" ]; then
		echo "Enabling BlocksDS-specific hacks."
		echo "#define CONFIG_NDS_BLOCKSDS" >> src/config.h
		echo "BUILD_NDS_BLOCKSDS=1" >> platform.inc
	fi

	echo "Force-disabling modular build on NDS."
	MODULAR="false"

	echo "Building custom NDS renderer."

	echo "Force-disabling SDL on NDS."
	SDL="false"
	SOFTWARE="false"

	echo "Force-disabling runtime icon loading on NDS."
	ICON="false"

	echo "Force-disabling hash tables on NDS."
	COUNTER_HASH="false"

	echo "Force-disabling layer rendering on NDS."
	LAYER_RENDERING="false"

	echo "Force-disabling virtual filesystem on NDS."
	VFS="false"

	echo "Force-disabling existing music playback libraries on NDS."
	MODPLUG="false"
	MIKMOD="false"
	XMP="false"
	OPENMPT="false"
	REALITY="false"
	VORBIS="false"

	if [ "$PLATFORM" = "nds" ]; then
		echo "Force-disabling stack protector on NDS."
		STACK_PROTECTOR="false"
	fi

	echo "Force-disabling networking on NDS."
	NETWORK="false"
	;;

"3ds")
	echo "Enabling 3DS-specific hacks."
	echo "#define CONFIG_3DS" >> src/config.h
	echo "BUILD_3DS=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on 3DS."
	MODULAR="false"

	#
	# If the 3DS arch is enabled and SDL 1.2 is used, softscale is not
	# available. On SDL 2.0, due to the rendering pipeline not supporting
	# hardware acceleration as of writing, it is available, but with
	# unsatisfactory performance. As a workaround, use the GP2X
	# 320x240 renderer.
	#
	if [ "$SDL" != "false" ]; then
		echo "Force-disabling overlay and OpenGL renderers (3DS)."
		OVERLAY="false"
		GL="false"

		echo "Force-enabling GP2X 320x240 renderer (SDL on 3DS)."
		GP2X="true"
	else
		echo "Force-disabling software renderer on 3DS."
		echo "Building custom 3DS renderer."
		SOFTWARE="false"
	fi

	echo "Force-disabling stack protector on 3DS."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading on 3DS."
	ICON="false"

	echo "Disabling utils on 3DS."
	UTILS="false"

	echo "Force-disabling IPv6 on 3DS (not implemented)."
	IPV6="false"
	;;

"wii")
	echo "Enabling Wii-specific hacks."
	echo "#define CONFIG_WII" >> src/config.h
	echo "BUILD_WII=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on Wii."
	MODULAR="false"

	if [ "$SDL" = "false" ]; then
		echo "Force-disabling software renderer on Wii."
		echo "Building custom Wii renderers."
		SOFTWARE="false"
	else
		echo "Force-disabling overlay and OpenGL renderers on Wii."
		OVERLAY="false"
		GL="false"
	fi

	echo "Force-disabling stack protector on Wii."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading on Wii."
	ICON="false"

	echo "Force-disabling utils on Wii."
	UTILS="false"

	echo "Force-disabling getaddrinfo and IPv6 on Wii."
	GETADDRINFO="false"
	IPV6="false"
	;;

"wiiu")
	echo "Enabling Wii U-specific hacks."
	echo "#define CONFIG_WIIU" >> src/config.h
	echo "BUILD_WIIU=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on Wii U."
	MODULAR="false"

	echo "Force-disabling overlay and OpenGL renderers on Wii U."
	OVERLAY="false"
	GL="false"

	echo "Force-disabling runtime icon loading on Wii U."
	ICON="false"

	echo "Disabling utils on Wii U."
	UTILS="false"

	# Doesn't seem to be fully populated on the Wii U.
	GAMECONTROLLERDB="false"
	;;

"switch")
	echo "Enabling Switch-specific hacks."
	echo "#define CONFIG_SWITCH" >> src/config.h
	echo "BUILD_SWITCH=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on Switch."
	MODULAR="false"

	echo "Force-disabling runtime icon loading on Switch."
	ICON="false"

	echo "Disabling utils on Switch."
	UTILS="false"

	echo "Force-enabling OpenGL ES support (Switch)."
	GLES="true"

	echo "Force-disabling IPv6 on Switch (FIXME getaddrinfo seems to not support it)."
	IPV6="false"

	# This may or may not be totally useless for the Switch, disable it for now.
	GAMECONTROLLERDB="false"
	;;

"psp")
	echo "Enabling PSP-specific hacks."
	echo "#define CONFIG_PSP" >> src/config.h
	echo "BUILD_PSP=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on PSP."
	MODULAR="false"

	echo "Force-disabling OpenGL and overlay renderers on PSP."
	GL="false"
	OVERLAY="false"

	echo "Force-disabling stack protector on PSP."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading on PSP."
	ICON="false"

	echo "Force-disabling getaddrinfo, poll, and IPv6 on PSP."
	GETADDRINFO="false"
	POLL="false"
	IPV6="false"
	;;

"psvita")
	echo "Enabling PS Vita-specific hacks."
	echo "#define CONFIG_PSVITA" >> src/config.h
	echo "BUILD_PSVITA=1" >> platform.inc
	DOS_ROOTS="true"

	echo "Force-disabling modular build on PS Vita."
	MODULAR="false"

	echo "Force-disabling utils on PS Vita."
	UTILS="false"

	echo "Force-disabling stack protector on PS Vita."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading on PS Vita."
	ICON="false"
	;;

"djgpp")
	echo "Enabling DOS-specific hacks."
	echo "#define CONFIG_DJGPP" >> src/config.h
	echo "BUILD_DJGPP=1" >> platform.inc

	echo "Force-disabling modular build (DOS)."
	MODULAR="false"

	echo "Force-disabling SDL (DOS)."
	SDL="false"
	SOFTWARE="false"

	echo "Force-disabling stack protector (DOS)."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading (DOS)."
	ICON="false"

	if [ "$VORBIS" != "false" ] && [ "$VORBIS" != "stb_vorbis" ]; then
		echo "Force-switching ogg/vorbis to stb_vorbis."
		VORBIS="stb_vorbis"
	fi

	if [ "$DOS_SVGA" = "true" ]; then
		echo "#define CONFIG_DOS_SVGA" >> src/config.h
		echo "BUILD_DOS_SVGA=1" >> platform.inc
		echo "SVGA software renderer enabled."
	else
		echo "SVGA software renderer disabled."
	fi

	echo "Force-disabling networking (DOS)."
	NETWORK="false"
	;;

"dreamcast")
	echo "Enabling Dreamcast-specific hacks."
	echo "#define CONFIG_DREAMCAST" >> src/config.h
	echo "BUILD_DREAMCAST=1" >> platform.inc

	echo "Force-disabling modular build (Dreamcast)."
	MODULAR="false"

	echo "Force-disabling SDL (Dreamcast)."
	SDL="false"
	SOFTWARE="false"

	echo "Force-disabling runtime icon loading (Dreamcast)."
	ICON="false"
	;;

"gp2x")
	echo "Enabling GP2X-specific hacks."
	echo "#define CONFIG_GP2X" >> src/config.h
	echo "BUILD_GP2X=1" >> platform.inc

	echo "Force-disabling modular build on GP2X."
	MODULAR="false"

	echo "Force-disabling stack protector on GP2X."
	STACK_PROTECTOR="false"

	echo "Force-disabling runtime icon loading on GP2X."
	ICON="false"

	echo "Force disabling software, overlay, and OpenGL renderers on GP2X."
	SOFTWARE="false"
	OVERLAY="false"
	GL="false"

	echo "Force-enabling GP2X 320x240 renderer."
	GP2X="true"

	echo "Force-disabling Modplug audio."
	MODPLUG="false"

	if [ "$VORBIS" != "false" ] && [ "$VORBIS" != "tremor-lowmem" ]; then
		echo "Force-switching ogg/vorbis to tremor-lowmem."
		VORBIS="tremor-lowmem"
	fi
	;;

"pandora")
	echo "Enabling Pandora-specific hacks."
	echo "#define CONFIG_PANDORA" >> src/config.h
	echo "BUILD_PANDORA=1" >> platform.inc
	;;
esac


### RESOLVE CONFLICTING OPTIONS ###############################################

#
# SDL was disabled above; must also disable SDL-dependent modules
#
if [ "$SDL" = "false" ]; then
	echo "Force-disabling SDL dependent components:"
	echo " -> SOFTSCALE, OVERLAY"
	SOFTSCALE="false"
	SDLACCEL="false"
	OVERLAY="false"
#
# If no specific version of SDL is selected, select SDL2.
#
elif [ "$SDL" != "1" ] && [ "$SDL" != "2" ] && [ "$SDL" != "3" ]; then
	SDL="2"
fi

#
# For now, force-enable OpenGL ES hacks, since that's most
# likely what is being used with EGL.
#
if [ "$EGL" = "true" ]; then
	echo "Force-enabling OpenGL ES support (EGL)."
	GLES="true"
fi

#
# Force-disable OpenGL renderers for non-SDL, non-EGL.
#
if [ "$SDL" = "false" ] && [ "$EGL" = "false" ] && [ "$GL" = "true" ]; then
	echo "Force-disabling OpenGL (no SDL or EGL backend)."
	GL="false"
fi

#
# Force-disable the softscale renderer for SDL 1.2 (requires SDL_Renderer).
#
if [ "$SDL" = "1" ] && [ "$SOFTSCALE" = "true" ]; then
	echo "Force-disabling softscale renderer (requires SDL 2+)."
	SOFTSCALE="false"
	SDLACCEL="false"
fi

#
# Force-disable overlay renderers for SDL 2. The SDL 2 answer to SDL_Overlay
# involves planar YUV modes which don't mesh well with MZX's internal rendering.
#
if [ "$SDL" != "false" ] && [ "$SDL" -ge "2" ] && [ "$OVERLAY" = "true" ]; then
	echo "Force-disabling overlay renderers (requires SDL 1.2)."
	OVERLAY="false"
fi

#
# Must have at least one OpenGL renderer enabled
#
if [ "$GL_FIXED" = "false" ] && [ "$GL_PROGRAM" = "false" ] && [ "$GL" = "true" ]; then
	echo "Force-disabling OpenGL (no OpenGL renderers enabled)"
	GL="false"
fi

#
# If OpenGL is globally disabled, disable all renderers
#
if [ "$GL_FIXED" = "true" ] || [ "$GL_PROGRAM" = "true" ] && [ "$GL" = "false" ]; then
	echo "Force-disabling OpenGL renderers (OpenGL disabled)."
	GL_FIXED="false"
	GL_PROGRAM="false"
fi

#
# Runtime icon loading requires MinGW or SDL + libpng.
#
if [ "$SDL" = "false" ] || [ "$LIBPNG" = "false" ] &&
   [ "$PLATFORM" != "mingw" ] && [ "$ICON" = "true" ]; then
	echo "Force-disabling runtime icon loading (requires MinGW or SDL + libpng)"
	ICON="false"
fi

#
# Force-disable PNG support on platforms without screenshots, utils, icon enabled.
# The 3DS port requires PNG for other purposes.
#
if [ "$SCREENSHOTS" = "false" ] &&
   [ "$UTILS" = "false" ] &&
   [ "$ICON" = "false" ] &&
   [ "$LIBPNG" = "true" ] &&
   [ "$PLATFORM" != "3ds" ]; then
	echo "Force-disabling PNG support (screenshots, utils, icon disabled)"
	LIBPNG="false"
fi

#
# Force-disable all music playback libraries if audio is disabled
#
if [ "$AUDIO" = "false" ]; then
	echo "Force-disabling music playback libraries (audio disabled)"
	MODPLUG="false"
	MIKMOD="false"
	XMP="false"
	OPENMPT="false"
	REALITY="false"
	VORBIS="false"
fi

#
# Force-disable pthread on non-POSIX platforms
#
if [ "$PLATFORM" != "unix" ] &&
   [ "$PLATFORM" != "unix-devel" ] &&
   [ "$PLATFORM" != "gp2x" ] &&
   [ "$PLATFORM" != "android" ]; then
	echo "Force-disabling pthread on non-POSIX platforms."
	PTHREAD="false"
fi

#
# Force disable updater (non-MinGW or non-editor)
#
if [ "$PLATFORM" != "mingw" ] || [ "$EDITOR" = "false" ]; then
	echo "Force-disabling updater (currently requires MinGW and editor)."
	UPDATER="false"
fi

#
# Force disable network applications (network disabled)
#
if [ "$NETWORK" = "false" ]; then
	echo "Force-disabling network-dependent features (networking disabled)"
	UPDATER="false"
fi

#
# Force disable networking (no applications enabled)
#
if [ "$NETWORK" = "true" ] && [ "$UPDATER" = "false" ]; then
	echo "Force-disabling networking (no network-dependent features enabled)."
	NETWORK="false"
fi

#
# Force disable utils.
#
if [ "$DEBYTECODE" = "true" ]; then
	echo "Force-disabling utils (debytecode)."
	UTILS="false"
fi


### EMIT OPTIONS ##############################################################

#
# Enable SDL.
#
if [ "$SDL" != "false" ]; then
	echo "SDL $SDL.x enabled."
	echo "#define CONFIG_SDL $SDL" >> src/config.h
	echo "BUILD_SDL=$SDL" >> platform.inc
else
	echo "SDL disabled."
fi

#
# Enable EGL.
#
if [ "$EGL" = "true" ]; then
	echo "EGL enabled."
	echo "#define CONFIG_EGL" >> src/config.h
	echo "BUILD_EGL=1" >> platform.inc
fi

#
# As GNU ld supports recursive dylib dependency tracking, we don't need to
# explicitly link to as many libraries as the authors would have us provide.
#
# Instead, pass the linker --as-needed to discard libraries explicitly
# passed through but not directly used. Fixes warnings with the Debian
# packaging infrastructure.
#
if [ "$AS_NEEDED" = "true" ]; then
	echo "Assuming GNU ld and passing --as-needed through."
	echo "LDFLAGS+=-Wl,--as-needed" >> platform.inc
fi

#
# Enable -fanalyzer support.
#
if [ "$ANALYZER" = "true" ]; then
	echo "Enabling -fanalyzer."
	echo ""
	echo "  *** WARNING ***: this is an experimental GCC feature! Its output"
	echo "  currently isn't very reliable, but it did help find some places"
	echo "  check_alloc wasn't being used and one legitimate memory leak."
	echo ""
	echo "BUILD_F_ANALYZER=1" >> platform.inc
fi

#
# Enable pledge(2) support (OpenBSD only)
#
if [ "$UNIX" = "openbsd" ]; then
	if [ "$PLEDGE" = "true" ]; then
		echo "Enabling OpenBSD pledge(2) support for main executable(s)."
		echo "#define CONFIG_PLEDGE" >> src/config.h
	else
		echo "OpenBSD pledge(2) support disabled for main executable(s)."
	fi
	if [ "$UTILS" = "true" ]; then
		if [ "$PLEDGE_UTILS" = "true" ]; then
			echo "Enabling OpenBSD pledge(2) support for utils"
			echo "#define CONFIG_PLEDGE_UTILS" >> src/config.h
		else
			echo "OpenBSD pledge(2) support disabled for utils"
		fi
	fi
else
	PLEDGE="false"
fi

#
# Users may enable release mode
#
if [ "$RELEASE" = "true" ]; then
	#
	# Users may want size optimizations
	#
	if [ "$OPT_SIZE" = "true" ]; then
		echo "Optimizing for size."
		echo "OPTIMIZE_FLAGS=-Os" >> platform.inc
	else
		echo "Optimizing for speed."
	fi
	#
	# Enable link-time optimizations.
	#
	if [ "$LTO" = "true" ]; then
		echo "Link-time optimizations enabled."
		echo "LTO=1" >> platform.inc
	else
		echo "Link-time optimizations disabled."
	fi
else
	echo "Disabling optimization, debug enabled."
	echo "DEBUG=1" >> platform.inc
	#
	# Trace logging for debug builds, if enabled.
	#
	if [ "$TRACE_LOGGING" = "true" ]; then
		echo "Enabling trace logging."
		echo "#define DEBUG_TRACE" >> src/config.h
	fi
fi

#
# Enable sanitizers.
#
if [ "$SANITIZER" != "false" ]; then
	echo "Enabling $SANITIZER sanitizer (may enable some optimizations)."
	echo "SANITIZER=$SANITIZER" >> platform.inc
fi

#
# Enable stack protector.
#
if [ "$STACK_PROTECTOR" = "true" ]; then
	echo "Stack protector enabled."
	echo "BUILD_STACK_PROTECTOR=1" >> platform.inc
else
	echo "Stack protector disabled."
fi

#
# User may disable the built-in editor
#
if [ "$EDITOR" = "true" ]; then
	echo "Built-in editor enabled."
	echo "BUILD_EDITOR=1" >> platform.inc
	echo "#define CONFIG_EDITOR" >> src/config.h
else
	echo "Built-in editor disabled."
fi

#
# User may disable `MZXRun' component
#
if [ "$MZXRUN" = "true" ]; then
	echo "Building MZXRun executable."
	echo "BUILD_MZXRUN=1" >> platform.inc
else
	echo "Not building MZXRun executable."
fi

#
# User may disable the built-in help system.
#
if [ "$HELPSYS" = "true" ]; then
	echo "Built-in help system enabled."
	echo "BUILD_HELPSYS=1" >> platform.inc
	echo "#define CONFIG_HELPSYS" >> src/config.h
else
	echo "Built-in help system disabled."
fi

#
# User may want to compile utils (checkres, downver, txt2hlp)
#
if [ "$UTILS" = "true" ]; then
	echo "Building utils (checkres, downver, png2smzx, hlp2txt, txt2hlp)."
	echo "BUILD_UTILS=1" >> platform.inc
	echo "#define CONFIG_UTILS" >> src/config.h
else
	echo "Disabled utils (checkres, downver, png2smzx, hlp2txt, txt2hlp)."
fi

#
# X11 support (linked against and needs headers installed)
#
if [ "$PLATFORM" = "unix" ] ||
   [ "$PLATFORM" = "unix-devel" ] ||
   [ "$PLATFORM" = "pandora" ]; then
	#
	# Confirm the user's selection of X11, if they enabled it
	#
	if [ "$X11" = "true" ]; then
		#
		# Locate X11 binary path.
		#
		X11="false"
		for XBIN in X Xorg; do
			# check if X exists
			X11PATH=$(command -v "$XBIN" 2>/dev/null)
			if [ -n "$X11PATH" ]; then
				X11="true"
				break
			fi
		done

		#
		# Locate X11 lib dir. "*/lib" should be the last element in the
		# list so it will be used as the default in case of failure.
		#
		if [ "$X11" = "true" ]; then
			X11DIR=$(dirname "$(dirname "$X11PATH")")

			for X11LIBDIR in "$X11DIR/lib64" "$X11DIR/lib"; do
				for EXT in "so" "dylib"; do
					if [ -f "$X11LIBDIR/libX11.$EXT" ] || \
					   [ -L "$X11LIBDIR/libX11.$EXT" ]; then
						break 2
					fi
				done
			done
		fi

		if [ "$X11" = "false" ]; then
			echo "Force-disabling X11 (could not be queried)."
		fi
	fi
else
	echo "Force-disabling X11 (unsupported platform)."
	X11="false"
fi

if [ "$X11" = "true" ]; then
	echo "X11 support enabled."
	echo "#define CONFIG_X11" >> src/config.h
	echo "X11DIR=${X11DIR}" >> platform.inc
	echo "X11LIBDIR=${X11LIBDIR}" >> platform.inc
fi

#
# Enable OpenGL ES hacks if required.
#
if [ "$GL" = "true" ] && [ "$GLES" = "true" ]; then
	echo "OpenGL ES support enabled."
	echo "#define CONFIG_GLES" >> src/config.h
else
	echo "OpenGL ES support disabled."
fi

#
# Software renderer
#
if [ "$SOFTWARE" = "true" ]; then
	echo "Software renderer enabled."
	echo "#define CONFIG_RENDER_SOFT" >> src/config.h
	echo "BUILD_RENDER_SOFT=1" >> platform.inc
else
	echo "Software renderer disabled."
fi

#
# Softscale renderer (SDL 2+)
#
if [ "$SOFTSCALE" = "true" ]; then
	echo "Softscale renderer enabled."
	echo "#define CONFIG_RENDER_SOFTSCALE" >> src/config.h
	echo "BUILD_RENDER_SOFTSCALE=1" >> platform.inc
else
	echo "Softscale renderer disabled."
fi

#
# SDL accelerated hardware renderer (SDL 2+)
#
if [ "$SDLACCEL" = "true" ]; then
	echo "SDL accelerated hardware renderer enabled."
	echo "#define CONFIG_RENDER_SDLACCEL" >> src/config.h
	echo "BUILD_RENDER_SDLACCEL=1" >> platform.inc
else
	echo "SDL accelerated hardware renderer disabled."
fi

#
# Fixed-function H/W OpenGL renderers
#
if [ "$GL_FIXED" = "true" ]; then
	echo "Fixed-function H/W OpenGL renderers enabled."
	echo "#define CONFIG_RENDER_GL_FIXED" >> src/config.h
	echo "BUILD_RENDER_GL_FIXED=1" >> platform.inc
else
	echo "Fixed-function OpenGL renderers disabled."
fi

#
# Programmable H/W OpenGL renderers
#
if [ "$GL_PROGRAM" = "true" ]; then
	echo "Programmable H/W OpenGL renderer enabled."
	echo "#define CONFIG_RENDER_GL_PROGRAM" >> src/config.h
	echo "BUILD_RENDER_GL_PROGRAM=1" >> platform.inc
else
	echo "Programmable H/W OpenGL renderer disabled."
fi

#
# Overlay renderers
#
if [ "$OVERLAY" = "true" ]; then
	echo "Overlay renderers enabled."
	echo "#define CONFIG_RENDER_YUV" >> src/config.h
	echo "BUILD_RENDER_YUV=1" >> platform.inc
else
	echo "Overlay renderers disabled."
fi

#
# GX renderer (Wii and GameCube)
#
if [ "$PLATFORM" = "wii" ] && [ "$SDL" = "false" ]; then
	echo "Building custom GX renderer."
	echo "#define CONFIG_RENDER_GX" >> src/config.h
	echo "BUILD_RENDER_GX=1" >> platform.inc
fi

#
# CTR renderer (3DS)
#
if [ "$PLATFORM" = "3ds" ] && [ "$SDL" = "false" ]; then
	echo "Building custom CTR renderer."
	echo "#define CONFIG_RENDER_CTR" >> src/config.h
	echo "BUILD_RENDER_CTR=1" >> platform.inc
fi

#
# GP2X renderer
#
if [ "$GP2X" = "true" ]; then
	echo "GP2X half-res renderer enabled."
	echo "#define CONFIG_RENDER_GP2X" >> src/config.h
	echo "BUILD_RENDER_GP2X=1" >> platform.inc
else
	echo "GP2X half-res renderer disabled."
fi

#
# Screenshot hotkey
#
if [ "$SCREENSHOTS" = "true" ]; then
	echo "Screenshot hotkey enabled."
	echo "#define CONFIG_ENABLE_SCREENSHOTS" >> src/config.h
	echo "BUILD_ENABLE_SCREENSHOTS=1" >> platform.inc
else
	echo "Screenshot hotkey disabled."
fi

#
# Frames-per-second counter
#
if [ "$FPSCOUNTER" = "true" ]; then
	echo "fps counter enabled."
	echo "#define CONFIG_FPS" >> src/config.h
else
	echo "fps counter disabled."
fi

#
# Layer rendering, if enabled
#
if [ "$LAYER_RENDERING" = "true" ]; then
	echo "Layer rendering enabled."
else
	echo "Layer rendering disabled."
	echo "#define CONFIG_NO_LAYER_RENDERING" >> src/config.h
fi

#
# GP2X needs Mikmod, other platforms can pick
# Keep the default at the bottom so it doesn't override others.
#

if [ "$MODPLUG" = "true" ]; then
	echo "Selected Modplug music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_MODPLUG" >> src/config.h
	echo "BUILD_MODPLUG=1" >> platform.inc
elif [ "$MIKMOD" = "true" ]; then
	echo "Selected Mikmod music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_MIKMOD" >> src/config.h
	echo "BUILD_MIKMOD=1" >> platform.inc
elif [ "$OPENMPT" = "true" ]; then
	echo "Selected OpenMPT music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_OPENMPT" >> src/config.h
	echo "BUILD_OPENMPT=1" >> platform.inc
elif [ "$XMP" = "true" ]; then
	echo "Selected XMP music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_XMP" >> src/config.h
	echo "BUILD_XMP=1" >> platform.inc
else
	echo "Music engine disabled."
fi

#
# Handle RAD support, if enabled
#

if [ "$REALITY" = "true" ]; then
	echo "Reality Adlib Tracker (RAD) support enabled."
	echo "#define CONFIG_REALITY" >> src/config.h
	echo "BUILD_REALITY=1" >> platform.inc
else
	echo "Reality Adlib Tracker (RAD) support disabled."
fi

#
# Handle audio subsystem, if enabled
#
if [ "$AUDIO" = "true" ]; then
	echo "Audio subsystem enabled."
	echo "#define CONFIG_AUDIO" >> src/config.h
	echo "BUILD_AUDIO=1" >> platform.inc
else
	echo "Audio subsystem disabled."
fi

#
# Handle PNG support, if enabled
#
if [ "$LIBPNG" = "true" ]; then
	echo "PNG support enabled."
	echo "#define CONFIG_PNG" >> src/config.h
	echo "LIBPNG=1" >> platform.inc
else
	echo "PNG support disabled."
fi

#
# Handle vorbis support, if enabled
#
if [ "$VORBIS" = "true" ]; then
	echo "Using ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "VORBIS=vorbis" >> platform.inc

elif [ "$VORBIS" = "tremor" ]; then
	echo "Using tremor in place of ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "#define CONFIG_TREMOR" >> src/config.h
	echo "VORBIS=tremor" >> platform.inc

elif [ "$VORBIS" = "tremor-lowmem" ]; then
	echo "Using tremor (lowmem) in place of ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "#define CONFIG_TREMOR" >> src/config.h
	echo "VORBIS=tremor-lowmem" >> platform.inc

elif [ "$VORBIS" = "stb_vorbis" ]; then
	echo "Using stb_vorbis in place of ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "#define CONFIG_STB_VORBIS" >> src/config.h
	echo "VORBIS=stb_vorbis" >> platform.inc
else
	echo "Ogg/vorbis disabled."
fi

#
# Handle pthread mutexes, if enabled
#
if [ "$PTHREAD" = "true" ]; then
	echo "Using pthread for threads/locking primitives."
	echo "#define CONFIG_PTHREAD" >> src/config.h
	echo "PTHREAD=1" >> platform.inc
else
	echo "Not using pthread for threads/locking primitives."
fi

#
# Handle runtime icon loading (SDL_SetWindowIcon or LoadIcon), if enabled
# For MinGW, this also disables embedding the icon.
#
if [ "$ICON" = "true" ]; then
	echo "Runtime window icon loading enabled."
	echo "#define CONFIG_ICON" >> src/config.h

	#
	# On Windows we want the icons to be compiled in
	#
	if [ "$PLATFORM" = "mingw" ]; then
		echo "Embedded application icon enabled."
		echo "EMBED_ICONS=1" >> platform.inc
	else
		#
		# Also get the (probable) icon path...
		#
		ICONFILE="megazeux.png"
		ICONDIR="$SHAREDIR/icons/hicolor/128x128/apps"

		if [ "$SHAREDIR" = "." ]; then
			ICONFILE="icon_256.png"
			ICONDIR="contrib/icons/generated"
		elif [ "$SHAREDIR" = "/app/share" ]; then
			# TODO: flatpak hack
			ICONFILE="com.digitalmzx.MegaZeux.png"
		fi
		echo "#define ICONFILE \"$ICONDIR/$ICONFILE\"" >> src/config.h
	fi
else
	echo "Runtime window icon loading disabled."
	[ "$PLATFORM" = "mingw" ] && echo "Embedded application icon disabled."
fi

#
# Inform build system of modular build, if appropriate
#
if [ "$MODULAR" = "true" ]; then
	echo "Modular build enabled."
	echo "BUILD_MODULAR=1" >> platform.inc
	echo "#define CONFIG_MODULAR" >> src/config.h

else
	echo "Modular build disabled."
fi

#
# Handle networking, if enabled
#
if [ "$NETWORK" = "true" ]; then
	echo "Networking enabled."
	echo "#define CONFIG_NETWORK" >> src/config.h
	echo "BUILD_NETWORK=1" >> platform.inc

	#
	# Handle networking options.
	#
	if [ "$GETADDRINFO" = "true" ]; then
		echo "getaddrinfo name resolution enabled."
		echo "#define CONFIG_GETADDRINFO" >> src/config.h
	else
		echo "getaddrinfo name resolution disabled."
	fi

	if [ "$POLL" = "true" ]; then
		echo "poll enabled."
		echo "#define CONFIG_POLL" >> src/config.h
	else
		echo "poll disabled; falling back to select."
	fi

	if [ "$IPV6" = "true" ]; then
		echo "IPv6 support enabled."
		echo "#define CONFIG_IPV6" >> src/config.h
	else
		echo "IPv6 support disabled."
	fi
else
	echo "Networking disabled."
fi

#
# Handle built-in updater, if enabled
#
if [ "$UPDATER" = "true" ]; then
	echo "Built-in updater enabled."
	echo "#define CONFIG_UPDATER" >> src/config.h
	echo "BUILD_UPDATER=1" >> platform.inc
else
	echo "Built-in updater disabled."
fi

#
# Some users may prefer the build system to ALWAYS be verbose
#
if [ "$VERBOSE" = "true" ]; then
	echo "Verbose build system (always)."
	echo "export V=1" >> platform.inc
fi

#
# Enable extra memory hacks and memory compression, if enabled.
#
if [ "$EXTRAM" = "true" ]; then
	echo "Board memory compression and extra memory hacks enabled."
	echo "#define CONFIG_EXTRAM" >> src/config.h
	echo "BUILD_EXTRAM=1" >> platform.inc
else
	echo "Board memory compression and extra memory hacks disabled."
fi

#
# Users may enable the load/save meter display.
#
if [ "$METER" = "true" ]; then
	echo "Load/save meter display enabled."
	echo "#define CONFIG_LOADSAVE_METER" >> src/config.h
else
	echo "Load/save meter display disabled."
fi

#
# Memory allocation error handling, if enabled
#
if [ "$CHECK_ALLOC" = "true" ]; then
	echo "Memory allocation error checking enabled."
	echo "#define CONFIG_CHECK_ALLOC" >> src/config.h
else
	echo "Memory allocation error checking disabled."
fi

#
# Allow use of hash table counter/string lookups, if enabled
#
if [ "$COUNTER_HASH" = "true" ]; then
	echo "Hash table counter/string lookups enabled."
	echo "#define CONFIG_COUNTER_HASH_TABLES" >> src/config.h
	echo "BUILD_COUNTER_HASH_TABLES=1" >> platform.inc
else
	echo "Hash table counter/string lookups disabled (using binary search)."
fi

#
# DOS-style roots in POSIX-like environments, if enabled
#
if [ "$DOS_ROOTS" = "true" ]; then
	echo "Enabling DOS-style roots."
	echo "#define CONFIG_DOS_STYLE_ROOTS" >> src/config.h
fi

#
# Virtual filesystem, if enabled
#
if [ "$VFS" = "true" ]; then
	echo "Virtual filesystem enabled."
	echo "#define CONFIG_VFS" >> src/config.h
else
	echo "Virtual filesystem disabled."
fi

#
# Experimental 'debytecode' transformation, if enabled
#
if [ "$DEBYTECODE" = "true" ]; then
	echo "Experimental 'debytecode' transform enabled."
	echo "#define CONFIG_DEBYTECODE" >> src/config.h
	echo "BUILD_DEBYTECODE=1" >> platform.inc
else
	echo "Experimental 'debytecode' transform disabled."
fi

#
# stdio redirect, if enabled
#
if [ "$SDL" = "1" ]; then
	echo "Using SDL 1.x default stdio redirect behavior."
elif [ "$STDIO_REDIRECT" = "true" ]; then
	echo "Redirecting stdio to stdout.txt and stderr.txt."
	echo "#define CONFIG_STDIO_REDIRECT" >> src/config.h
else
	echo "stdio redirect disabled."
fi

#
# SDL_GameControllerDB, if enabled. This depends on SDL 2+.
#
if [ "$SDL" != "false" ] && [ "$SDL" -ge "2" ] && \
   [ "$GAMECONTROLLERDB" = "true" ]; then
	echo "SDL_GameControllerDB enabled."
	echo "#define CONFIG_GAMECONTROLLERDB" >> src/config.h
	echo "BUILD_GAMECONTROLLERDB=1" >> platform.inc
else
	echo "SDL_GameControllerDB disabled."
fi

#
# Autoconf-esque detection for some compilation flags.
#
echo
if command -v gmake >/dev/null 2>&1; then
	gmake -f Makefile.config || exit 1
else
	make -f Makefile.config || exit 1
fi

#
# Pledge(2) on main executable warning
#
if [ "$PLEDGE" = "true" ]; then
	echo
	echo "  WARNING: pledge will probably: break renderer switching; crash when"
	echo "  switching to fullscreen (use fullscreen=1) or exiting when using"
	echo "  the software renderer; crash when switching to fullscreen with the"
	echo "  scaling renderers (use fullscreen=1 and/or fullscreen_windowed=1);"
	echo "  crash when using any scaling renderer with some Mesa versions."
	echo "  You've been warned!"
fi

echo
echo "Now type \"make\" (or \"gmake\" on BSD)."
