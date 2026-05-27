##
# icons Makefile fragment
##

.PHONY: icons_clean icons_rebuild

.SUFFIXES: .rc

icons_src = contrib/icons
icons_obj = ${build_base}/${icons_src}
build_dirs += ${icons_obj}

icons_gen = contrib/icons/generated
icons_windows = ${icons_gen}
icons_nds = ${icons_gen}
icons_wii = ${icons_gen}
icons_wiiu = ${icons_gen}
icons_switch = ${icons_gen}
icons_psp = ${icons_gen}
icons_vita = arch/psvita/sce_sys
icons_emscripten = arch/emscripten/web/res
icons_android = arch/android/project/app/src/main/res

icon_svg = ${icons_src}/icon.svg
icon_name_svg = ${icons_src}/icon-name.svg

icons_pngs = \
	${icons_gen}/icon_16.png \
	${icons_gen}/icon_20.png \
	${icons_gen}/icon_22.png \
	${icons_gen}/icon_24.png \
	${icons_gen}/icon_30.png \
	${icons_gen}/icon_32.png \
	${icons_gen}/icon_36.png \
	${icons_gen}/icon_40.png \
	${icons_gen}/icon_48.png \
	${icons_gen}/icon_60.png \
	${icons_gen}/icon_64.png \
	${icons_gen}/icon_72.png \
	${icons_gen}/icon_80.png \
	${icons_gen}/icon_96.png \
	${icons_gen}/icon_128.png \
	${icons_gen}/icon_144.png \
	${icons_gen}/icon_192.png \
	${icons_gen}/icon_256.png \
	${icons_gen}/icon_256.jpg \
	${icons_gen}/icon_512.png \
	${icons_gen}/icon_1024.png \
	${icons_windows}/icon_win_32.png \
	${icons_windows}/icon_win_48.png \
	${icons_windows}/icon_win_16_8bpp.png \
	${icons_windows}/icon_win_32_8bpp.png \
	${icons_windows}/icon_win_48_8bpp.png \
	${icons_windows}/icon.ico \
	${icons_nds}/icon_nds.bmp \
	${icons_nds}/icon_nds.png \
	${icons_wii}/icon_wii.png \
	${icons_wiiu}/icon_wiiu.png \
	${icons_switch}/icon_switch.jpg \
	${icons_psp}/icon_psp.png \
	${icons_vita}/icon0.png \
	${icons_vita}/livearea/contents/startup.png \
	${icons_emscripten}/loading.png \
	${icons_android}/mipmap-mdpi/ic_launcher.png \
	${icons_android}/mipmap-hdpi/ic_launcher.png \
	${icons_android}/mipmap-xhdpi/ic_launcher.png \
	${icons_android}/mipmap-xxhdpi/ic_launcher.png \
	${icons_android}/mipmap-xxxhdpi/ic_launcher.png \

icons = ${icons_obj}/icons.o

icons_objs = ${icons_obj}/icon.o

${icons_obj}/%.o: ${icons_src}/%.rc
	$(if ${V},,@echo "  WINDRES " $<)
	${WINDRES} -i $< -o $@

ifeq (${EMBED_ICONS},1)
${icons}: ${icons_obj} ${icons_objs}
	$(if ${V},,@echo "  CP      " ${icons_objs} ${icons})
	${CP} ${icons_objs} ${icons}
else
${icons}:
	@echo "--> Embedding of icon branding disabled."
endif

${icons_objs}: $(filter-out $(wildcard ${icons_obj}), ${icons_obj})

icons_clean:
	$(if ${V},,@echo "  RM      " ${icons_obj})
	${RM} -r ${icons_obj}

#
# Rebuild raster icons from SVG.
#
icons_rebuild: ${icons_pngs}

${icons_pngs}: contrib/icons/Makefile.in

MAGICK ?= magick
ifneq ($V,1)
MAGICK := @${MAGICK}
endif
MAGICK += -define png:compression-level=9

# Base icon has a nominal size of 128px with 8px minimum margins.
# This meets KDE's Colorful guidelines for 32px (2px margin) and
# 64px (4px margin) but not 48px (4px margin), so scale down slightly
# for 48px.

# Expected DPI 12, but the icon looks better when scaled to ~1x.
# Icon is 96 DPI with the smiley scaled 9x -> want approx 10.667
${icons_gen}/icon_16.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 10.667 "$<" \
	  -gravity center -extent 16x16 png32:"$@"

${icons_gen}/icon_20.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 15 "$<" png32:"$@"

${icons_gen}/icon_22.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 16.5 "$<" png32:"$@"

${icons_gen}/icon_24.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 18 "$<" png32:"$@"

# Expected DPI 22.5. Slightly downscale to get nicer filtering.
${icons_gen}/icon_30.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 21.333 "$<" \
	  -gravity center -extent 30x30 png32:"$@"

# DPI 25 has nicer filtering but the 2px border is required by Colorful.
${icons_gen}/icon_32.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 24 "$<" png32:"$@"

${icons_gen}/icon_36.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 27 "$<" png32:"$@"

${icons_gen}/icon_40.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 30 "$<" png32:"$@"

# Expected DPI 36, use 33 (44px) to increase margin to >=Colorful spec.
${icons_gen}/icon_48.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 33 "$<" \
	  -gravity center -extent 48x48 png32:"$@"

${icons_gen}/icon_60.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 45 "$<" png32:"$@"

${icons_gen}/icon_64.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 48 "$<" png32:"$@"

${icons_gen}/icon_72.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 54 "$<" png32:"$@"

${icons_gen}/icon_80.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 60 "$<" png32:"$@"

${icons_gen}/icon_96.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 72 "$<" png32:"$@"

${icons_gen}/icon_128.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 96 "$<" png32:"$@"

${icons_gen}/icon_144.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 108 "$<" png32:"$@"

${icons_gen}/icon_192.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 144 "$<" png32:"$@"

${icons_gen}/icon_256.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 192 "$<" png32:"$@"

${icons_gen}/icon_512.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 384 "$<" png32:"$@"

${icons_gen}/icon_1024.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 768 "$<" png32:"$@"

#
# Windows: combine a variety of sizes into a single .ico.
# MSDN lists: 16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256
# The minimum set is supposed to be 16, 24, 32, 48, and 256, but
# these need to be explicitly RGBA or Windows XP onward will pick
# the wrong icons and resize them. (Include 8bpp too just in case.)
#
icons_in_ico = \
	${icons_gen}/icon_256.png \
	${icons_windows}/icon_win_48.png \
	${icons_windows}/icon_win_32.png \
	${icons_windows}/icon_24.png \
	${icons_windows}/icon_16.png \
	${icons_windows}/icon_win_48_8bpp.png \
	${icons_windows}/icon_win_32_8bpp.png \
	${icons_windows}/icon_win_16_8bpp.png

${icons_windows}/icon_win_16_8bpp.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 10.667 "$<" \
	  -gravity center -extent 16x16 -colors 256 png8:"$@"

# Expected DPI 24, use 25 for nicer scaling.
${icons_windows}/icon_win_32.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 25 "$<" \
	  -gravity center -extent 32x32 png32:"$@"

${icons_windows}/icon_win_32_8bpp.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 25 "$<" \
	  -gravity center -extent 32x32 -colors 256 png8:"$@"

# Expected DPI 36, scale up as much as possible to help fix downscales.
# This clips slightly but not in an especially noticeable way.
${icons_windows}/icon_win_48.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 40 "$<" \
	  -gravity center -extent 48x48 png32:"$@"

${icons_windows}/icon_win_48_8bpp.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 40 "$<" \
	  -gravity center -extent 48x48 -colors 256 png8:"$@"

${icons_windows}/icon.ico: ${icons_in_ico}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} ${icons_in_ico} $@

#
# NDS: icons are 32px 16 colors.
# Expected DPI 24, but 25 has nicer filtering properties.
# The BMP used by the original ndstool uses index 0 for alpha
# and for some reason is hardcoded to the BITMAPINFOHEADER DIB.
# The PNG used by the BlocksDS nsdtool uses a transparent color.
#
${icons_nds}/icon_nds.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 25 "$<" \
	  -gravity center -extent 32x32 -colors 16 "$@"

${icons_nds}/icon_nds.bmp: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background black -density 25 "$<" \
	  -gravity center -extent 32x32 -colors 16 -type palette BMP3:"$@"

#
# Wii: icons are 128x48.
# Compose "MegaZeux" onto the main icon.
# Drop shadow needed to provide bordering in Homebrew Channel.
#
${icons_wii}/icon_wii.png: ${icon_svg} ${icon_name_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none \
	  -density 36            "${icon_svg}" \
	  \( +clone -background black -shadow 100x4+0+0 \) \
	  +swap -background none -layers merge +repage \
	  -gravity center        -extent  40x48 \
	  -gravity west          -extent 128x48 \
	  \( -filter point \
	     -density 144        "${icon_name_svg}" \
	     -gravity center     -extent 98x44 \
	     -gravity southeast  -extent 128x48 \) \
	  -layers merge +repage  "$@"

#
# Wii U: icons are 256x96.
# Compose "MegaZeux" onto the main icon.
#
${icons_wiiu}/icon_wiiu.png: ${icon_svg} ${icon_name_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none \
	  -density 72            "${icon_svg}" \
	  \( +clone -background black -shadow 100x4+0+0 \) \
	  +swap -background none -layers merge +repage \
	  -gravity center        -extent  76x96 \
	  -gravity west          -extent 256x96 \
	  \( -density 288        "${icon_name_svg}" \
	     -gravity center     -extent 197x88 \
	     -gravity southeast  -extent 256x96 \) \
	  -layers merge +repage  "$@"

#
# Switch: icon is the standard 256px, except is a JPEG for some reason.
#
${icons_switch}/icon_switch.jpg: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background "#000080" -density 192 "$<" "$@"

#
# PSP: icons are 144x80.
# Background is blue, give it a drop shadow.
#
${icons_psp}/icon_psp.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 60 "$<" \
	  \( +clone -background black -shadow 100x4+0+0 \) \
	  +swap -background none -layers merge +repage \
	  -gravity center -extent 144x80 "$@"

#
# PlayStation Vita: several of these, generate directly into arch tree.
# This icon needs to be 128px, downscaled to work around the
# forced circle crop, and needs a solid color background as
# the stock Vita shell does not support transparency (Vita3K does).
#
${icons_vita}/icon0.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background none -density 78 "$<" \
	  \( +clone -background black -shadow 100x6+0+0 \) \
	  +swap -background '#000040' -layers merge +repage \
	  -gravity center -extent 128x128 "$@"

# Vita launch screen logo image, 280x158, transparency allowed.
${icons_vita}/livearea/contents/startup.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -background '#202024' -density 81 "$<" \
	  -gravity north -extent 280x140 \
	  -gravity center -extent 280x158 "$@"

#
# Emscripten: no real "icon" but the loading splash uses the icon.
#
${icons_emscripten}/loading.png: ${icon_svg}
	$(if ${V},,@echo "  MAGICK  " $@)
	${MAGICK} -size 640x350 xc:black \
	  -fill '#aaaaaa' -draw 'rectangle 25,221 614,246' \
	  -fill black     -draw 'rectangle 26,222 613,245' \
	  \( -background none -density 96 "$<"   \
	     -gravity center  -extent 640x220    \
	     -gravity north   -extent 640x350 \) \
	  -layers merge +repage png24:"$@"

#
# Android: icons are standard sizes, just copy them into project tree.
#
${icons_android}/mipmap-mdpi/ic_launcher.png: ${icons_gen}/icon_48.png
	$(if ${V},,@echo "  CP      " $< $@)
	${CP} "$<" "$@"

${icons_android}/mipmap-hdpi/ic_launcher.png: ${icons_gen}/icon_72.png
	$(if ${V},,@echo "  CP      " $< $@)
	${CP} "$<" "$@"

${icons_android}/mipmap-xhdpi/ic_launcher.png: ${icons_gen}/icon_96.png
	$(if ${V},,@echo "  CP      " $< $@)
	${CP} "$<" "$@"

${icons_android}/mipmap-xxhdpi/ic_launcher.png: ${icons_gen}/icon_144.png
	$(if ${V},,@echo "  CP      " $< $@)
	${CP} "$<" "$@"

${icons_android}/mipmap-xxxhdpi/ic_launcher.png: ${icons_gen}/icon_192.png
	$(if ${V},,@echo "  CP      " $< $@)
	${CP} "$<" "$@"
