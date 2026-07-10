#
# MegaZeux common dependency builder code.
#

ifeq (${PLATFORM},)
all:
	$(error usage: make PLATFORM=<platform> [ARCH=<arch>] [all|<arch>|fetch|package|clean])

endif

VERSION		= 2.93e-r0
DIST		= megazeux-dependencies-${VERSION}
TARGET		= TARGET

# 1.3.2 is broken on DJGPP (EWOULDBLOCK) and Amiga m68k (-fPIC).
ZLIB		= zlib-1.3.1
PNG		= libpng-1.6.57
OGG		= libogg-1.3.6
VORBIS		= libvorbis-1.3.7
TREMOR		= tremor
STB		= stb
SDL12		= SDL-1.2.15
SDL12_AMIGA	= SDL-amiga
SDL2_TIGER	= panther_sdl2
SDL2_22		= SDL2-2.0.22
SDL2_LATEST	= SDL2-2.32.10
SDL3		= SDL3-3.4.4

# Mac OS X/macOS compatibility builds rely on different versions of SDL2
# for each architecture. Override ${SDL2} to select a different version.
SDL2		:= ${SDL2_LATEST}

#ZLIB_URL	= https://www.zlib.net/${ZLIB}.tar.gz
ZLIB_URL	= https://github.com/madler/zlib/releases/download/v1.3.1/${ZLIB}.tar.gz
PNG_URL		= http://download.sourceforge.net/libpng/${PNG}.tar.gz
OGG_URL		= https://downloads.xiph.org/releases/ogg/${OGG}.tar.gz
VORBIS_URL	= https://downloads.xiph.org/releases/vorbis/${VORBIS}.tar.gz
SDL12_URL	= https://www.libsdl.org/release/${SDL12}.tar.gz
SDL2_22_URL	= https://www.libsdl.org/release/${SDL2_22}.tar.gz
SDL2_URL	= https://www.libsdl.org/release/${SDL2_LATEST}.tar.gz
SDL3_URL	= https://www.libsdl.org/release/${SDL3}.tar.gz

# xiph still hasn't fixed the crash bugs in tremor yet
#TREMOR_GIT	= https://gitlab.xiph.org/xiph/tremor
TREMOR_GIT	= https://github.com/sezero/tremor

# stb_vorbis upstream is out of date and sezero's fork includes every
# patch from libxmp (including mandatory alloca removal)
#STB_GIT	= https://github.com/nothings/stb
STB_GIT		= https://github.com/sezero/stb

# Officially, SDL 2.0.2 is the last version to support Mac OS X Panther/Tiger.
# panther_SDL2 is a backport of SDL 2.0.3 to these Mac OS X releases.
#SDL2_TIGER_GIT	= https://github.com/panther_SDL2/panther_SDL2
SDL2_TIGER_GIT	= https://github.com/AliceLR/panther_SDL2

# SDL 1.2.14 fork for Amiga m68k, AROS, MorphOS.
# Forked from: https://github.com/AmigaPorts/libSDL12.git
SDL12_AMIGA_GIT	= https://github.com/AliceLR/AmigaPorts-libSDL12.git

LIB		= ${PLATFORM}/${ARCH}/lib
BIN		= ${PLATFORM}/${ARCH}/bin
INCLUDE		= ${PLATFORM}/${ARCH}/include
ZLIB_TARGET	= ${LIB}/libz.a
PNG_TARGET	= ${LIB}/libpng16.a
OGG_TARGET	= ${LIB}/libogg.a
VORBIS_TARGET	= ${LIB}/libvorbis.a
TREMOR_TARGET	= ${LIB}/libvorbisidec.a
STB_TARGET	= ${INCLUDE}/stb/stb_vorbis.c
SDL12_TARGET	= ${LIB}/libSDL.a
SDL2_TARGET	= ${LIB}/libSDL2.a
SDL3_TARGET	= ${LIB}/libSDL3.a
TARGETS		= ${ZLIB_TARGET} \
		  ${PNG_TARGET} \
		  ${OGG_TARGET} \
		  ${VORBIS_TARGET} \
		  ${SDL12_TARGET} \
		  ${SDL2_TARGET} \

all:

-include Makefile.${PLATFORM}.in

ZLIB_PACKAGE	= ${TARGET}/${LIB}/libz.a \
		  ${TARGET}/${INCLUDE}/zlib.h \
		  ${TARGET}/${INCLUDE}/zconf.h
PNG_PACKAGE	= ${TARGET}/${LIB}/libpng16.a \
		  ${TARGET}/${INCLUDE}/libpng16 \
		  ${TARGET}/${BIN}/libpng-config
VORBIS_PACKAGE	= ${TARGET}/${LIB}/libogg.a \
		  ${TARGET}/${LIB}/libvorbis.a \
		  ${TARGET}/${LIB}/libvorbisfile.a \
		  ${TARGET}/${INCLUDE}/ogg \
		  ${TARGET}/${INCLUDE}/vorbis
TREMOR_PACKAGE	= ${TARGET}/${LIB}/libvorbisidec.a \
		  ${TARGET}/${INCLUDE}/tremor
ifneq (${TREMOR_LOWMEM},1)
TREMOR_PACKAGE	+= \
		  ${TARGET}/${LIB}/libogg.a \
		  ${TARGET}/${INCLUDE}/ogg
endif
STB_PACKAGE	= ${TARGET}/${INCLUDE}/stb
SDL12_PACKAGE	= ${TARGET}/${LIB}/libSDL.a \
		  ${TARGET}/${INCLUDE}/SDL \
		  ${TARGET}/${BIN}/sdl-config
SDL2_PACKAGE	= ${TARGET}/${LIB}/libSDL2.a \
		  ${TARGET}/${INCLUDE}/SDL2 \
		  ${TARGET}/${BIN}/sdl2-config
SDL3_PACKAGE	= ${TARGET}/${LIB}/libSDL3.a \
		  ${TARGET}/${INCLUDE}/SDL3

.PHONY: all
.PHONY: package package_dir package_clean package_lazy
.PHONY: zlib_package png_package
.PHONY: vorbis_package tremor_package stb_package
.PHONY: SDL12_package SDL2_package SDL3_package
.PHONY: clean zlib_clean png_clean ogg_clean vorbis_clean tremor_clean
.PHONY: SDL12_clean SDL12_amiga_clean
.PHONY: SDL2_tiger_clean SDL2_22_clean SDL2_clean SDL3_clean
.PHONY: fetch distclean

.NOTPARALLEL:
.SUFFIXES:

#
# Initial setup rules.
#

build:
	mkdir -p build/cmake

build/${ZLIB}.tar.gz: | build
	cd build && \
	wget '${ZLIB_URL}'

build/${PNG}.tar.gz: | build
	cd build && \
	wget '${PNG_URL}'

build/${OGG}.tar.gz: | build
	cd build && \
	wget '${OGG_URL}'

build/${VORBIS}.tar.gz: | build
	cd build && \
	wget '${VORBIS_URL}'

build/${SDL12}.tar.gz: | build
	cd build && \
	wget '${SDL12_URL}'

build/${SDL2_22}.tar.gz: | build
	cd build && \
	wget '${SDL2_22_URL}'

build/${SDL2_LATEST}.tar.gz: | build
	cd build && \
	wget '${SDL2_URL}'

build/${SDL3}.tar.gz: | build
	cd build && \
	wget '${SDL3_URL}'

fetch: build/${ZLIB}
build/${ZLIB}: build/${ZLIB}.tar.gz
	cd build && \
	tar -xzf '${ZLIB}.tar.gz'

fetch: build/${PNG}
build/${PNG}: build/${PNG}.tar.gz | build
	cd build && \
	tar -xzf '${PNG}.tar.gz'

fetch: build/${OGG}
build/${OGG}: build/${OGG}.tar.gz | build
	cd build && \
	tar -xzf '${OGG}.tar.gz'

fetch: build/${VORBIS}
build/${VORBIS}: build/${VORBIS}.tar.gz | build
	cd build && \
	tar -xzf '${VORBIS}.tar.gz'
ifeq (${VORBIS},libvorbis-1.3.7)
	patch build/${VORBIS}/lib/CMakeLists.txt fix-vorbis-1.3.7-build-framework.patch
endif

fetch: build/${TREMOR}
build/${TREMOR}: | build
	cd build && \
	git clone "${TREMOR_GIT}"

fetch: build/${STB}
build/${STB}: | build
	cd build && \
	git clone "${STB_GIT}"

fetch: build/${SDL12}
build/${SDL12}: build/${SDL12}.tar.gz | build
	cd build && \
	tar -xzf '${SDL12}.tar.gz'

# fetch: build/${SDL12_AMIGA}
build/${SDL12_AMIGA}: | build
	cd build && \
	git clone "${SDL12_AMIGA_GIT}" "${SDL12_AMIGA}" && \
	cd "${SDL12_AMIGA}" && \
	git apply ../../sdl-amiga-fix-morphos.patch

# fetch: build/${SDL2_TIGER}
build/${SDL2_TIGER}: | build
	cd build && \
	git clone "${SDL2_TIGER_GIT}" && \
	cd "${SDL2_TIGER}" && \
	git apply ../../panther_SDL2_fixes.patch

fetch: build/${SDL2_22}
build/${SDL2_22}: build/${SDL2_22}.tar.gz | build
	cd build && \
	tar -xzf '${SDL2_22}.tar.gz'
	# SDL 2.0.22 doesn't actually need -fobjc-weak but CMakeLists.txt
	# tries to force it. This doesn't affect autotools.
	sed -ibak 's/^.*fobjc-weak.*$$//g' build/${SDL2_22}/CMakeLists.txt
	# SDL 2.0.22 doesn't need AVFoundation.framework either, but
	# the CMakeLists.txt links it anyway just to break Snow Leopard.
	# This also does not affect autotools.
	sed -ibak 's/set(SDL_FRAMEWORK_AVFOUNDATION 1)//g' build/${SDL2_22}/CMakeLists.txt

fetch: build/${SDL2_LATEST}
build/${SDL2_LATEST}: build/${SDL2_LATEST}.tar.gz | build
	cd build && \
	tar -xzf '${SDL2_LATEST}.tar.gz'

fetch: build/${SDL3}
build/${SDL3}: build/${SDL3}.tar.gz | build
	cd build && \
	tar -xzf '${SDL3}.tar.gz'

zlib_clean:
	cd build/${ZLIB} && make distclean || true; make -fwin32/Makefile.gcc clean || true

png_clean:
	rm -rf build/cmake/${PNG}
	cd build/${PNG} && make distclean || true

ogg_clean:
	rm -rf build/cmake/${OGG}
	cd build/${OGG} && make distclean || true

vorbis_clean:
	rm -rf build/cmake/${VORBIS}
	cd build/${VORBIS} && make distclean || true

tremor_clean:
	cd build/${TREMOR} && make distclean || true

SDL12_clean:
	cd build/${SDL12} && make distclean || true

SDL12_amiga_clean:
	cd build/${SDL12_AMIGA}; make clean || true
	cd build/${SDL12_AMIGA}; make -f Makefile.morphos clean || true

SDL2_tiger_clean:
	rm -rf build/cmake/${SDL2_TIGER}
	cd build/${SDL2_TIGER} && make distclean || true

SDL2_22_clean:
	rm -rf build/cmake/${SDL2_22}
	cd build/${SDL2_22} && make distclean || true

SDL2_clean:
	rm -rf build/cmake/${SDL2_LATEST}
	cd build/${SDL2_LATEST} && make distclean || true

SDL3_clean:
	rm -rf build/cmake/${SDL3}

ifeq (${ARCH},)
#
# Architecture meta targets.
#

all: ${ALL_ARCH}

${ALL_ARCH}:
	${MAKE} ARCH='$@'

clean: zlib_clean png_clean ogg_clean vorbis_clean
clean: SDL12_clean SDL12_amiga_clean
clean: SDL2_tiger_clean SDL2_22_clean SDL2_clean SDL3_clean

distclean:
	rm -rf ${TARGET}
	rm -rf build
	rm -rf djgpp
	rm -rf macos
	rm -rf mingw

package:
ifeq (${PLATFORM},)
	$(error define a platform!)
endif
	for ARCH in ${ALL_ARCH}; do \
		${MAKE} PLATFORM='${PLATFORM}' ARCH="$$ARCH" package_clean; \
		${MAKE} PLATFORM='${PLATFORM}' ARCH="$$ARCH" package; \
	done
	cp ../../arch/LICENSE.3rd ${TARGET}/${PLATFORM}/
	A=''; \
	for ARCH in ${ALL_ARCH}; do \
		if [ -d "${PLATFORM}/$$ARCH" ]; then \
			A="$$A ${PLATFORM}/$$ARCH"; \
		fi; \
	done; \
	mkdir -p "PACKAGES"; \
	cd ${TARGET}; \
	XZ_OPT="-T0 -9" tar -cJf '../PACKAGES/${DIST}-${PLATFORM}.tar.xz' \
				 ${TAR_FLAGS} $$A ${PLATFORM}/LICENSE.3rd

else

#
# Per-architecture targets.
#
# For libpng, libogg, and libvorbis, an explicit --build value is
# required to prevent Autoconf's broken interaction with binfmt_misc
# when autodetecting cross compilation. This doesn't work with SDL.
#

PREFIX		:= $(shell pwd)/${PLATFORM}/${ARCH}
CONFIGURE_ARGS	= --prefix='${PREFIX}'
ifneq (${HOST},)
ZLIB_CONFIGURE	= CHOST='${HOST}'
CONFIGURE_ARGS	+= --build=$$(./config.guess) --host='${HOST}'
endif
CONFIGURE_ARGS	+= ${ARCH_CONFIGURE_ARGS}

ifneq (${FORCE_STATIC_LIBS},)
ZLIB_ARGS	+= --static
CONFIGURE_ARGS	+= --disable-shared --enable-static
endif

CONFIGURE	?= ./configure
CMAKE		?= cmake

CONFIGURE	+= ${CONFIGURE_ARGS}
CMAKE		+= -S "$$SRCDIR" -B . -D CMAKE_INSTALL_PREFIX='${PREFIX}' ${ARCH_CMAKE_ARGS}

ifeq (${ARCH_INIT},)
$(error must define ARCH_INIT.)
endif

.PHONY: zlib png ogg vorbis tremor SDL12 SDL2 SDL3
.PHONY: SDL12_AMIGA_TARGET SDL12_MORPHOS_TARGET

zlib: ${ZLIB_TARGET}
png: ${PNG_TARGET}
ogg: ${OGG_TARGET}
vorbis: ${VORBIS_TARGET}
tremor: ${TREMOR_TARGET}
stb: ${STB_TARGET}
SDL12: ${SDL12_TARGET}
SDL2: ${SDL2_TARGET}
SDL3: ${SDL3_TARGET}

# zlib has its own buildsystem.
# MinGW requires some involved special handling...
${ZLIB_TARGET}: | build/${ZLIB} zlib_clean
ifeq (${PLATFORM},mingw)
	cd build/${ZLIB}; \
	set -e; \
	${ARCH_INIT}; \
	export DESTDIR='${PREFIX}/'; \
	export INCLUDE_PATH=include; \
	export LIBRARY_PATH=lib; \
	export BINARY_PATH=bin; \
	${MAKE} -fwin32/Makefile.gcc PREFIX='${HOST}-' prefix='${PREFIX}'; \
	${MAKE} -fwin32/Makefile.gcc PREFIX='${HOST}-' prefix='${PREFIX}' install SHARED_MODE=1;
else
	cd build/${ZLIB}; \
	set -e; \
	${ARCH_INIT}; \
	${ZLIB_CONFIGURE} ./configure --prefix='${PREFIX}' ${ZLIB_ARGS}; \
	${MAKE}; \
	${MAKE} install;
endif

# libpng supports Autotools and CMake.
# Its ./configure is barely capable of finding the correct zlib at the
# best of times and doesn't use pkg-config.
${PNG_TARGET}: | build/${PNG} png_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${PNG}
	cd build/cmake/${PNG}; \
	export SRCDIR='../../${PNG}'; \
	set -e; \
	${ARCH_INIT}; \
	${CMAKE}; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${PNG}; \
	set -e; \
	${ARCH_INIT}; \
	CFLAGS="$$CFLAGS -I${PREFIX}/include" \
	CPPFLAGS="$$CPPFLAGS -I${PREFIX}/include" \
	LDFLAGS="$$LDFLAGS -L${PREFIX}/lib -lz" \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;
endif
# Doesn't seem to be required anymore
#ifeq (${PLATFORM},macos)
#	install_name_tool -change /usr/lib/libz.1.dylib "${PREFIX}/lib/libz.dylib" "${PREFIX}/lib/libpng.dylib"
#endif

# libogg supports Autotools and CMake.
# The Xiph CMake buildsystems can't build static and shared at the same time.
${OGG_TARGET}: | build/${OGG} ogg_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${OGG}
	cd build/cmake/${OGG}; \
	export SRCDIR='../../${OGG}'; \
	set -e; \
	${ARCH_INIT}; \
	${CMAKE} -D BUILD_SHARED_LIBS=ON; \
	${MAKE}; \
	${MAKE} install; \
	${CMAKE} -D BUILD_SHARED_LIBS=OFF; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${OGG}; \
	set -e; \
	${ARCH_INIT}; \
	${CONFIGURE}; \
	${MAKE}; \
	${MAKE} install;
endif

# libvorbis supports Autotools and CMake.
# The Xiph CMake buildsystems can't build static and shared at the same time.
${VORBIS_TARGET}: | build/${VORBIS} vorbis_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${VORBIS}
	cd build/cmake/${VORBIS}; \
	export SRCDIR='../../${VORBIS}'; \
	set -e; \
	${ARCH_INIT}; \
	${CMAKE} -D BUILD_SHARED_LIBS=ON; \
	${MAKE}; \
	${MAKE} install; \
	${CMAKE} -D BUILD_SHARED_LIBS=OFF; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${VORBIS}; \
	set -e; \
	${ARCH_INIT}; \
	${CONFIGURE} --disable-oggtest; \
	${MAKE}; \
	${MAKE} install;
endif

# libvorbisidec supports Autotools.
# The lowmem branch can be selected optionally.
${TREMOR_TARGET}: | build/${TREMOR} tremor_clean
ifeq (${TREMOR_LOWMEM},1)
	cd build/${TREMOR}; \
	git checkout lowmem
else
	cd build/${TREMOR}; \
	git checkout master
endif
	cd build/${TREMOR}; \
	set -e; \
	${ARCH_INIT}; \
	./autogen.sh ${CONFIGURE_ARGS}; \
	${MAKE}; \
	${MAKE} install;

# stb_vorbis is just a .c source file include.
${STB_TARGET}: | build/${STB}
	mkdir -p "${PREFIX}/include/stb"
	cd build/${STB} && git checkout stb_vorbis-sezero
	cp build/${STB}/stb_vorbis.c "${PREFIX}/include/stb"

# SDL1.2 only supports Autotools.
# Rerun autogen.sh to fix ./configure's --build/--host cross compilation detection.
${SDL12_TARGET}: | build/${SDL12} SDL12_clean
	cd build/${SDL12}; \
	set -e; \
	${ARCH_INIT}; \
	./autogen.sh; \
	ln -sf build-scripts/config.guess config.guess; \
	${CONFIGURE} ${ARCH_CONFIGURE_SDL12_ARGS}; \
	${MAKE}; \
	${MAKE} install;

# These require different Makefiles from ${SDL12_TARGET} but still create it.
SDL12_AMIGA_TARGET: | build/${SDL12_AMIGA} SDL12_amiga_clean
	cd build/${SDL12_AMIGA}; \
	set -e \
	${ARCH_INIT}; \
	export INSTALL_PREFIX="${PREFIX}"; \
	${MAKE} -f Makefile; \
	${MAKE} -f Makefile install;

SDL12_MORPHOS_TARGET: | build/${$SDL12_AMIGA} SDL12_amiga_clean
	cd build/${SDL12_AMIGA}; \
	set -e \
	${ARCH_INIT}; \
	export INSTALL_PREFIX="${PREFIX}"; \
	${MAKE} -f Makefile.morphos; \
	${MAKE} -f Makefile.morphos install;

# SDL2 supports Autotools and CMake. Early versions have a broken CMakeFiles.txt.
${SDL2_TARGET}: | build/${SDL2} SDL2_tiger_clean SDL2_22_clean SDL2_clean
ifeq (${USE_CMAKE},1)
	mkdir -p build/cmake/${SDL2}
	cd build/cmake/${SDL2}; \
	export SRCDIR='../../${SDL2}'; \
	set -e; \
	${ARCH_INIT}; \
	${CMAKE}; \
	${MAKE}; \
	${MAKE} install;
else
	cd build/${SDL2}; \
	set -e; \
	${ARCH_INIT}; \
	ln -sf build-scripts/config.guess config.guess; \
	${CONFIGURE} ${ARCH_CONFIGURE_SDL2_ARGS}; \
	${MAKE}; \
	${MAKE} install;
endif

# SDL3 only supports CMake.
${SDL3_TARGET}: | build/${SDL3} SDL3_clean
	mkdir -p build/cmake/${SDL3}
	cd build/cmake/${SDL3}; \
	export SRCDIR='../../${SDL3}'; \
	set -e; \
	${ARCH_INIT}; \
	${CMAKE} ${ARCH_CMAKE_SDL3_ARGS}; \
	${MAKE}; \
	${MAKE} install;

#
# Packaging rules.
#

package_clean:
	rm -rf ${TARGET}/${LIB}
	rm -rf ${TARGET}/${INCLUDE}
	rm -rf ${TARGET}/${BIN}

package_dir:
	if [ -d "${PLATFORM}/${ARCH}" ]; then \
		mkdir -p ${TARGET}/${LIB}; \
		mkdir -p ${TARGET}/${INCLUDE}; \
		mkdir -p ${TARGET}/${BIN}; \
	fi

# macOS dylibbundler requires libraries with intact full names,
# which the normal package targets can't handle. This copies the
# entirety of bin, lib, and include instead, and then strips
# out known bloat.
package_lazy: | package_dir
	cp -RP "${LIB}" "${TARGET}/${LIB}/../"
	cp -RP "${INCLUDE}" "${TARGET}/${INCLUDE}/../"
	cp -RP "${BIN}" "${TARGET}/${BIN}/../"
	rm -rf "${TARGET}/${LIB}/png.framework"
	rm -f "${TARGET}/${BIN}/png"*

%.debug: % | package_dir
	${HOST}-objcopy --only-keep-debug $< $@
	${HOST}-strip --strip-unneeded $<
	${HOST}-objcopy --add-gnu-debuglink=$@ $<

${TARGET}/%: % | package_dir
	if [ -d "$<" ]; then \
		cp -r $< $@; \
	else \
		cp -L $< $@; \
	fi

package: ${TARGET}/${LIB}/pkgconfig | package_dir

zlib_package: ${ZLIB_PACKAGE}
png_package: ${PNG_PACKAGE}
vorbis_package: ${VORBIS_PACKAGE}
tremor_package: ${TREMOR_PACKAGE}
stb_package: ${STB_PACKAGE}
SDL12_package: ${SDL12_PACKAGE}
SDL2_package: ${SDL2_PACKAGE}
SDL3_package: ${SDL3_PACKAGE}

endif # ARCH != ''
