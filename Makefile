##
# MegaZeux Build System (GNU Make)
#
# NOTE: This build system is designed to not use recursive Makefiles.
#       The rationale for this is documented here:
#
# https://web.archive.org/web/20200128044903/http://aegis.sourceforge.net/auug97.pdf
##

#
# Remove all built-in rules.
#
.SUFFIXES:
ifeq ($(filter -r,$(MAKEFLAGS)),)
MAKEFLAGS += -r
endif

.PHONY: all clean help_check mzx mzx.debug build build_clean source
.PHONY: test testworlds unit unittest test_clean unit_clean

#
# Define this first so arch-specific rules don't hijack the default target.
#
all:

-include platform.inc
include version.inc

#
# ${build_base}: where temporary object/dependency files should be stored.
# ${build_root}: base location where builds are copied to be archived for release.
# ${build}: location where the MegaZeux executable and data files should be placed.
# Defaults to ${build_root}, but the architecture Makefile may override this to
# use a subdirectory instead. This is useful when a platform expects a particular
# path hierarchy within the archive (e.g. MacOS .app bundles).
#
build_base ?= .build
build_root := build/${SUBPLATFORM}
build := ${build_root}

EXTRA_LICENSES ?=
LICENSE_CC0    ?= arch/LICENSE.CC0
LICENSE_DJGPP  ?= arch/LICENSE.DJGPP
LICENSE_LGPL2  ?= arch/LICENSE.LGPL2
LICENSE_MPL2   ?= arch/LICENSE.MPL2
LICENSE_NEWLIB ?= arch/LICENSE.Newlib

-include arch/${PLATFORM}/Makefile.in

CC      ?= gcc
CXX     ?= g++
AR      ?= ar
AS      ?= as
STRIP   ?= strip
OBJCOPY ?= objcopy
WINDRES ?= windres
PEFIX   ?= true

CHMOD   ?= chmod
CP      ?= cp
HOST_CC ?= gcc
LN      ?= ln
MKDIR   ?= mkdir
MV      ?= mv
RM      ?= rm

ifneq (${CROSS_COMPILE},)
ifeq (${CC},cc)
CC      = gcc
endif
CC      := ${CROSS_COMPILE}${CC}
CXX     := ${CROSS_COMPILE}${CXX}
AR      := ${CROSS_COMPILE}${AR}
AS      := ${CROSS_COMPILE}${AS}
STRIP   := ${CROSS_COMPILE}${STRIP}
OBJCOPY := ${CROSS_COMPILE}${OBJCOPY}
WINDRES := ${CROSS_COMPILE}${WINDRES}
endif

include arch/compat.inc

#
# Set up CFLAGS/LDFLAGS for all MegaZeux external dependencies.
#

ifneq (${BUILD_SDL},)

#
# SDL 3
#
ifeq (${BUILD_SDL},3)
# Check SDL_PKG_CONFIG_PATH and PREFIX/lib/pkgconfig for sdl3.pc.
# Note --with-path is a pkgconf extension.
ifneq ($(and ${SDL_PKG_CONFIG_PATH},$(wildcard ${SDL_PKG_CONFIG_PATH}/sdl3.pc)),)
# nop
else
# Check dependencies prefix instead for LIBDIR=. platforms.
# This is useless for unix/darwin, which should have sdl3.pc in a
# place pkgconf can find through normal means.
ifeq (${LIBDIR},.)
ifneq ($(wildcard ${PREFIX}/lib/pkgconfig/sdl3.pc),)
SDL_PKG_CONFIG_PATH ?= ${PREFIX}/lib/pkgconfig
endif
endif # LIBDIR=.
endif
ifneq (${SDL_PKG_CONFIG_PATH},)
SDL_PKG_CONFIG_FLAGS = --with-path=${SDL_PKG_CONFIG_PATH}
SDL_PKG_CONFIG_FORCE := PKG_CONFIG_PATH="${SDL_PKG_CONFIG_PATH}"
else
SDL_PKG_CONFIG_FORCE :=
endif

SDL_PREFIX  := $(shell ${SDL_PKG_CONFIG_FORCE} ${PKGCONF} ${SDL_PKG_CONFIG_FLAGS} sdl3 --variable=prefix)
SDL_CFLAGS  ?= $(shell ${SDL_PKG_CONFIG_FORCE} ${PKGCONF} ${SDL_PKG_CONFIG_FLAGS} sdl3 --cflags)
SDL_LDFLAGS ?= $(shell ${SDL_PKG_CONFIG_FORCE} ${PKGCONF} ${SDL_PKG_CONFIG_FLAGS} sdl3 --libs)
endif # SDL3

#
# SDL 2
#
ifeq (${BUILD_SDL},2)
# Check PREFIX for sdl2-config.
ifneq ($(and ${SDL_PREFIX},$(wildcard ${SDL_PREFIX}/bin/sdl2-config)),)
SDL_CONFIG  := ${SDL_PREFIX}/bin/sdl2-config
else
ifneq ($(wildcard ${PREFIX}/bin/sdl2-config),)
SDL_CONFIG  := ${PREFIX}/bin/sdl2-config
else
SDL_CONFIG  := sdl2-config
endif
endif

SDL_PREFIX  ?= $(shell ${SDL_CONFIG} --prefix)
SDL_CFLAGS  ?= $(shell ${SDL_CONFIG} --prefix=${SDL_PREFIX} --cflags | sed 's,-I,-isystem ,g')
SDL_LDFLAGS ?= $(shell ${SDL_CONFIG} --prefix=${SDL_PREFIX} --libs)
endif # SDL2

#
# SDL 1.2
#
ifeq (${BUILD_SDL},1)
EXTRA_LICENSES += ${LICENSE_LGPL2}

# Check PREFIX for sdl-config.
ifneq ($(and ${SDL_PREFIX},$(wildcard ${SDL_PREFIX}/bin/sdl-config)),)
SDL_CONFIG  := ${SDL_PREFIX}/bin/sdl-config
else
ifneq ($(wildcard ${PREFIX}/bin/sdl-config),)
SDL_CONFIG  := ${PREFIX}/bin/sdl-config
else
SDL_CONFIG  := sdl-config
endif
endif

SDL_PREFIX  ?= $(shell ${SDL_CONFIG} --prefix)
SDL_CFLAGS  ?= $(shell ${SDL_CONFIG} --prefix=${SDL_PREFIX} --cflags | sed 's,-I,-isystem ,g')
SDL_LDFLAGS ?= $(shell ${SDL_CONFIG} --prefix=${SDL_PREFIX} --libs)
endif # SDL1

# Make these immediate so the scripts run only once.
SDL_PREFIX  := $(SDL_PREFIX)
SDL_CFLAGS  := $(SDL_CFLAGS)
SDL_LDFLAGS := $(LINK_DYNAMIC_IF_MIXED) $(SDL_LDFLAGS)

endif # SDL

#
# libvorbis/tremor/stb_vorbis
#

VORBIS_CFLAGS  ?= -I${PREFIX}/include -DOV_EXCLUDE_STATIC_CALLBACKS
ifeq (${VORBIS},vorbis)
VORBIS_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lvorbisfile -lvorbis -logg
endif
ifeq (${VORBIS},tremor)
VORBIS_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lvorbisidec -logg
endif
ifeq (${VORBIS},tremor-lowmem)
VORBIS_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lvorbisidec
endif
ifeq (${VORBIS},stb_vorbis)
VORBIS_LDFLAGS ?= -L${PREFIX}/lib
endif

#
# MikMod (optional mod engine)
#

MIKMOD_CFLAGS  ?= -I${PREFIX}/include
MIKMOD_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lmikmod
ifneq (${BUILD_MIKMOD},)
EXTRA_LICENSES += ${LICENSE_LGPL2}
endif

#
# libopenmpt (optional mod engine)
#

OPENMPT_CFLAGS  ?= -I${PREFIX}/include
OPENMPT_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lopenmpt

#
# zlib
#

ZLIB_CFLAGS  ?= -I${PREFIX}/include
ZLIB_LDFLAGS ?= $(LINK_STATIC_IF_MIXED) -L${PREFIX}/lib -lz

#
# libpng
#

ifeq (${LIBPNG},1)

# Check PREFIX for libpng-config.
ifneq ($(and ${LIBPNG_PREFIX},$(wildcard ${LIBPNG_PREFIX}/bin/libpng-config)),)
LIBPNG_CONFIG  := ${LIBPNG_PREFIX}/bin/libpng-config
else
ifneq ($(wildcard ${PREFIX}/bin/libpng-config),)
LIBPNG_CONFIG  := ${PREFIX}/bin/libpng-config
else
LIBPNG_CONFIG  := libpng-config
endif
endif

LIBPNG_CFLAGS  ?= $(shell ${LIBPNG_CONFIG} --cflags)
LIBPNG_LDFLAGS ?= $(shell ${LIBPNG_CONFIG} --ldflags)

# Make these immediate so the scripts run only once.
LIBPNG_CFLAGS  := $(LIBPNG_CFLAGS)
LIBPNG_LDFLAGS := $(LINK_STATIC_IF_MIXED) $(LIBPNG_LDFLAGS)
endif

#
# X11
#

ifneq (${X11DIR},)
# BSD needs this but Fedora rpmbuild will whine about it and fail.
ifneq (${X11DIR},/usr)
X11RPATH    ?= -Wl,-rpath,${X11LIBDIR}
endif
X11_CFLAGS  ?= -I${X11DIR}/include
X11_LDFLAGS ?= -L${X11LIBDIR} -lX11 ${X11RPATH}
# Make these immediate
X11_CFLAGS := $(X11_CFLAGS)
X11_LDFLAGS := $(X11_LDFLAGS)
endif

#
# pthread
#

PTHREAD_LDFLAGS ?= -lpthread

#
# Set up general CFLAGS/LDFLAGS
#

#
# Usually, just disable the optimizer for "true" debug builds and
# define "DEBUG" to enable optional code at compile time.
# Optimized builds have assert() compiled out.
#
ifneq (${DEBUG},1)
OPTIMIZE_FLAGS := -O3 ${OPTIMIZE_CFLAGS} ${OPTIMIZE_FLAGS}
OPTIMIZE_DEF   ?= -DNDEBUG
else
OPTIMIZE_FLAGS := -O0 ${DEBUG_CFLAGS} ${OPTIMIZE_FLAGS}
OPTIMIZE_DEF   ?= -DDEBUG
endif

#
# Enable a sanitizer and its corresponding extra options, if selected.
#
ifeq (${SANITIZER},address)
OPTIMIZE_FLAGS += -fsanitize=address -fno-omit-frame-pointer
endif
ifeq (${SANITIZER},undefined)
# Signed integer overflows (shift-base, signed-integer-overflow)
# are pretty much inevitable in Robotic, so ignore them.
OPTIMIZE_FLAGS += -fsanitize=undefined -fno-omit-frame-pointer \
 -fno-sanitize-recover=all -fno-sanitize=shift-base,signed-integer-overflow
endif
ifeq (${SANITIZER},thread)
OPTIMIZE_FLAGS += -fsanitize=thread -fno-omit-frame-pointer -fPIE
ARCH_EXE_LDFLAGS += -pie
endif
ifeq (${SANITIZER},memory)
# Note: to be useful, this requires a fairly special build with most
# external libraries turned off or re-built with instrumentation.
# This sanitizer is only implemented by clang.
OPTIMIZE_FLAGS += -fsanitize=memory -fno-omit-frame-pointer -fPIC \
 -fsanitize-memory-track-origins=2
ARCH_EXE_LDFLAGS += -pie
endif

#
# Enable link-time optimization.
#
ifeq (${LTO},1)
ifeq (${HAS_F_LTO},1)
OPTIMIZE_FLAGS += -flto
else
$(warning link-time optimization not supported, ignoring.)
endif
endif

CFLAGS   += ${OPTIMIZE_FLAGS} ${OPTIMIZE_DEF}
CXXFLAGS += ${OPTIMIZE_FLAGS} ${OPTIMIZE_DEF}
LDFLAGS  += ${OPTIMIZE_FLAGS}

#
# Enable C++11 for compilers that support it.
# Anything actually using C++11 should be optional or platform-specific,
# as features using C++11 reduce portability.
#
ifeq (${HAS_CXX_11},1)
CXX_STD = -std=gnu++11
else
CXX_STD = -std=gnu++98
endif

#
# Always generate debug information; this may end up being
# stripped (on embedded platforms) or objcopy'ed out.
#
CFLAGS   += -std=gnu99 -g ${ARCH_CFLAGS}
CXXFLAGS += ${CXX_STD} -g -fno-exceptions -fno-rtti ${ARCH_CXXFLAGS}
LDFLAGS  += ${ARCH_LDFLAGS}

#
# Default warnings.
# Note: -Wstrict-prototypes was previously turned off for Android/NDS/Wii/PSP.
#
warnings := -Wall -Wextra -Wno-unused-parameter -Wwrite-strings
warnings += -Wundef -Wunused-macros -Wpointer-arith
CFLAGS   += ${warnings} -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes
CXXFLAGS += ${warnings}

#
# Optional compile flags.
#

#
# Warn against global functions defined without a previous declaration (C++).
#
ifeq (${HAS_W_MISSING_DECLARATIONS_CXX},1)
CXXFLAGS += -Wmissing-declarations
endif

#
# Warn against variable-length array (VLA) usage, which is technically valid
# C99 but is in bad taste and isn't supported by MSVC.
#
ifeq (${HAS_W_VLA},1)
CFLAGS   += -Wvla
CXXFLAGS += -Wvla
endif

#
# Linux GCC gives spurious format truncation warnings. The snprintf
# implementation on Linux will terminate even in the case of truncation,
# making this largely useless. It does not trigger using mingw (where it
# would actually matter).
#
ifeq (${HAS_W_NO_FORMAT_TRUNCATION},1)
CFLAGS   += -Wno-format-truncation
CXXFLAGS += -Wno-format-truncation
endif

#
# Old GCC versions emit false positive warnings for C++11 value initializers.
#
ifeq (${HAS_BROKEN_W_MISSING_FIELD_INITIALIZERS},1)
CXXFLAGS += -Wno-missing-field-initializers
endif

#
# We enable pedantic warnings here, but this ends up turning on some things
# we must disable by hand.
#
# Variadic macros are arguably less portable, but all the compilers we
# support have them.
#
ifeq (${HAS_PEDANTIC},1)
CFLAGS   += -pedantic
CXXFLAGS += -pedantic

ifeq (${HAS_W_NO_VARIADIC_MACROS},1)
CFLAGS   += -Wno-variadic-macros
CXXFLAGS += -Wno-variadic-macros
endif
endif

#
# KallistiOS has a pretty dire header situation
#
ifeq (${BUILD_DREAMCAST},1)
CFLAGS   += -Wno-strict-prototypes -Wno-pedantic
CXXFLAGS += -Wno-pedantic
endif

#
# As does BlocksDS, currently
#
ifeq (${BUILD_NDS_BLOCKSDS},1)
CFLAGS   += -Wno-strict-prototypes -Wno-pedantic -Wno-declaration-after-statement
CXXFLAGS += -Wno-pedantic
endif

#
# The following flags are not applicable to mingw or djgpp builds.
#
ifneq (${PLATFORM},mingw)
ifneq (${PLATFORM},djgpp)

#
# Symbols in COFF binaries are implicitly hidden unless exported; this
# flag just confuses GCC and must be disabled.
#
ifeq (${HAS_F_VISIBILITY},1)
CFLAGS   += -fvisibility=hidden
CXXFLAGS += -fvisibility=hidden
endif

endif # PLATFORM=djgpp
endif # PLATFORM=mingw

#
# The stack protector is optional and is generally only built for Linux/BSD and
# Mac OS X. It also works on Windows. GCC's -fstack-protector-strong is
# preferred when available due to better performance.
#
ifeq (${BUILD_STACK_PROTECTOR},1)
ifeq (${HAS_F_STACK_PROTECTOR_STRONG},1)
CFLAGS   += -fstack-protector-strong
CXXFLAGS += -fstack-protector-strong
else
ifeq (${HAS_F_STACK_PROTECTOR},1)
CFLAGS   += -fstack-protector-all
CXXFLAGS += -fstack-protector-all
else
$(warning stack protector not supported, ignoring.)
endif
endif
endif

#
# Enable -fanalyzer if supported.
#
ifeq (${BUILD_F_ANALYZER},1)
ifeq (${HAS_F_ANALYZER},1)
CFLAGS   += -fanalyzer
CXXFLAGS += -fanalyzer
else
$(warning GCC 10+ is required for -fanalyzer, ignoring.)
endif
endif

#
# Enable position-independent code across the board for modular builds.
#
ifeq (${BUILD_MODULAR},1)
CFLAGS += -fPIC
CXXFLAGS += -fPIC
endif

#
# We don't want these commands to be echo'ed in non-verbose mode
#
ifneq (${V},1)
override V:=

CC      := @${CC}
CXX     := @${CXX}
AR      := @${AR}
AS      := @${AS}
STRIP   := @${STRIP}
OBJCOPY := @${OBJCOPY}
WINDRES := @${WINDRES}
PEFIX   := @${PEFIX}

CHMOD   := @${CHMOD}
CP      := @${CP}
HOST_CC := @${HOST_CC}
LN      := @${LN}
MKDIR   := @${MKDIR}
MV      := @${MV}
RM      := @${RM}
endif

build_clean:
	$(if ${V},,@echo "  RM      " build)
	${RM} -r build

source: build/${TARGET}src

#
# Build source target
# Targeting unix primarily, so turn off autocrlf if necessary.
#
build/${TARGET}src:
	${RM} -r build/${TARGET}
	${MKDIR} -p build/dist/source
	@git -c "core.autocrlf=false" checkout-index -a --prefix build/${TARGET}/
	${RM} -r build/${TARGET}/scripts
	${RM} build/${TARGET}/.gitignore build/${TARGET}/.gitattributes
	@cd build/${TARGET} && ${MAKE} distclean
	@tar -C build -Jcf build/dist/source/${TARGET}src.tar.xz ${TARGET}

#
# The SUPPRESS_ALL_TARGETS hack is required to allow the placebo "dist"
# Makefile to provide an 'all:' target, which allows it to print
# a message. Pulling in any other targets would confuse Make.
#
# Additionally, there are several other SUPPRESS flags that can be set to
# conditionally disable rules. This is useful for cross-compiling builds that
# have no use for host-based rules or for platforms that use meta targets.
#
# * SUPPRESS_CC_TARGETS prevents "mzx", etc from being added to "all".
# * SUPPRESS_BUILD_TARGETS suppresses "build".
# * SUPPRESS_HOST_TARGETS suppresses "assets/help.fil", "test", etc.
#
ifneq (${SUPPRESS_ALL_TARGETS},1)

mzxrun = mzxrun${BINEXT}
mzx = megazeux${BINEXT}

mzx: ${mzxrun} ${mzx}
mzx.debug: ${mzxrun}.debug ${mzx}.debug

ifeq (${BUILD_MODPLUG},1)
BUILD_GDM2S3M=1
endif

%.debug: %
	$(if ${V},,@echo "  OBJCOPY " --only-keep-debug $< $@)
	${OBJCOPY} --only-keep-debug $< $@
	${PEFIX} $@
	${CHMOD} a-x $@
ifneq (${NO_STRIP_IN_DEBUGLINK},1)
	$(if ${V},,@echo "  STRIP   " --strip-unneeded $<)
	${STRIP} --strip-unneeded $<
endif
	$(if ${V},,@echo "  OBJCOPY " --add-gnu-debuglink $@ $<)
	${OBJCOPY} --add-gnu-debuglink=$@ $<
	${PEFIX} $<
	@touch $@

include src/Makefile.in

clean: mzx_clean test_clean unit_clean

ifneq (${SUPPRESS_CC_TARGETS},1)
all: mzx
debuglink: all mzx.debug
endif

include src/utils/Makefile.in
ifeq (${BUILD_UTILS},1)
clean: utils_clean
ifneq (${SUPPRESS_CC_TARGETS},1)
debuglink: utils utils.debug
all: utils
endif
endif

ifneq (${SUPPRESS_BUILD_TARGETS},1)

ifeq (${build},)
build := ${build_root}
endif

.PHONY: ${build}

build: ${build} ${build}/assets ${build}/docs

${build}:
	${RM} -r ${build_root}
	${MKDIR} -p ${build}
	${CP} config.txt LICENSE arch/LICENSE.3rd ${EXTRA_LICENSES} ${build}
	@if test -f ${mzxrun}; then \
		cp ${mzxrun} ${build}; \
	fi
	@if test -f ${mzxrun}.debug; then \
		cp ${mzxrun}.debug ${build}; \
	fi
ifeq (${BUILD_EDITOR},1)
	@if test -f ${mzx}; then \
		cp ${mzx} ${build}; \
	fi
	@if test -f ${mzx}.debug; then \
		cp ${mzx}.debug ${build}; \
	fi
endif
ifeq (${BUILD_MODULAR},1)
	${CP} ${core_target} ${editor_target} ${build}
	@if test -f ${core_target}.debug; then \
		cp ${core_target}.debug ${build}; \
	fi
	@if test -f ${editor_target}.debug; then \
		cp ${editor_target}.debug ${build}; \
	fi
endif
ifeq (${BUILD_UTILS},1)
	${MKDIR} ${build}/utils
	${CP} ${checkres} ${downver} ${build}/utils
	${CP} ${hlp2txt} ${txt2hlp} ${build}/utils
	${CP} ${ccv} ${png2smzx} ${y4m2smzx} ${build}/utils
	@if [ -f "${checkres}.debug" ]; then cp ${checkres}.debug ${build}/utils; fi
	@if [ -f "${downver}.debug" ]; then cp ${downver}.debug ${build}/utils; fi
	@if [ -f "${hlp2txt}.debug" ]; then cp ${hlp2txt}.debug ${build}/utils; fi
	@if [ -f "${txt2hlp}.debug" ]; then cp ${txt2hlp}.debug ${build}/utils; fi
	@if [ -f "${ccv}.debug" ]; then cp ${ccv}.debug ${build}/utils; fi
	@if [ -f "${png2smzx}.debug" ]; then cp ${png2smzx}.debug ${build}/utils; fi
	@if [ -f "${y4m2smzx}.debug" ]; then cp ${y4m2smzx}.debug ${build}/utils; fi
endif

${build}/docs: ${build}
	${MKDIR} -p ${build}/docs
	${CP} docs/macro.txt docs/keycodes.html docs/mzxhelp.html ${build}/docs
	${CP} docs/joystick.html docs/cycles_and_commands.txt ${build}/docs
	${CP} docs/fileform.html ${build}/docs
	${CP} docs/changelog.txt docs/platform_matrix.html ${build}/docs

${build}/assets: ${build}
	${MKDIR} -p ${build}/assets
	${CP} assets/default.chr assets/edit.chr ${build}/assets
	${CP} assets/smzx.pal ${build}/assets
ifeq (${BUILD_EDITOR},1)
	${CP} assets/ascii.chr assets/blank.chr ${build}/assets
	${CP} assets/smzx.chr assets/smzx2.chr ${build}/assets
endif
ifeq (${BUILD_HELPSYS},1)
	${CP} assets/help.fil ${build}/assets
endif
ifeq (${BUILD_RENDER_GL_PROGRAM},1)
	${MKDIR} -p ${build}/assets/glsl/scalers
	${CP} assets/glsl/*.vert ${build}/assets/glsl
	${CP} assets/glsl/*.frag ${build}/assets/glsl
	${CP} assets/glsl/README.md ${build}/assets/glsl
	${CP} assets/glsl/scalers/*.frag assets/glsl/scalers/*.vert \
		${build}/assets/glsl/scalers
endif
ifeq (${BUILD_GAMECONTROLLERDB},1)
	${CP} assets/gamecontrollerdb.txt \
	 ${build}/assets
endif
ifeq (${BUILD_UTILS},1)
	${CP} contrib/mzvplay/mzvplay.txt \
	 ${build}/utils
endif

endif # !SUPPRESS_BUILD_TARGETS

distclean: clean
	@echo "  DISTCLEAN"
	@rm -f src/config.h
	@echo "PLATFORM=none" > platform.inc

ifneq (${SUPPRESS_HOST_TARGETS},1)

assets/help.fil: ${txt2hlp} docs/WIPHelp.txt
	$(if ${V},,@echo "  txt2hlp " $@)
	@src/utils/txt2hlp docs/WIPHelp.txt $@

docs/mzxhelp.html: ${hlp2html} docs/WIPHelp.txt
	$(if ${V},,@echo "  hlp2html" $@)
	@src/utils/hlp2html docs/WIPHelp.txt docs/mzxhelp.html

help_check: ${hlp2txt} assets/help.fil
	@src/utils/hlp2txt assets/help.fil help.txt
	@echo @ >> help.txt
	@diff --strip-trailing-cr -q docs/WIPHelp.txt help.txt
	@rm -f help.txt

-include unit/Makefile.in

test: unit

test testworlds:
ifeq (${BUILD_MODULAR},1)
	@${SHELL} testworlds/run.sh ${PLATFORM} "$(realpath ${core_target})"
else
	@${SHELL} testworlds/run.sh ${PLATFORM}
endif

endif # !SUPPRESS_HOST_TARGETS

test_clean:
	@rm -rf testworlds/log

# At the end because build_dirs is affected by most fragments.
${build_dirs}:
	$(if ${V},,@echo "  MKDIR   " $@)
	${MKDIR} -p $@

endif # !SUPPRESS_ALL_TARGETS
