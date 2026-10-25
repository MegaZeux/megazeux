#!/bin/sh

# Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#
# Functions for packaging Caverns of Zeux into a build archive.
#

CAVERNS_URL="https://www.digitalmzx.com/download/182/26f59999fc55953e81a66cf01d0207fe3b124387a4a4c9288a155f6a44134641/"
CAVERNS_BASE="$MZX_WORKINGDIR/caverns"

CAVERNS_PATH="caverns"
CAVERNS_MMUTIL_PATH="caverns_mmutil"

. "$MZX_SCRIPTS/common.sh"
. "$MZX_SCRIPTS/common-dkp.sh"

caverns_init()
{
	cmd_check 7z wget
	[ "$ERRNO" = "0" ] || { return; }

	mkdir -p "$CAVERNS_BASE"

	if [ ! -f "$CAVERNS_BASE/caverns.zip" ]; then
		wget "$CAVERNS_URL" -O "$CAVERNS_BASE/caverns.zip"
	fi

	if [ ! -d "$CAVERNS_BASE/$CAVERNS_PATH" ]; then
		7z e "$CAVERNS_BASE/caverns.zip" "-o$CAVERNS_BASE/$CAVERNS_PATH"
	fi

	dkp_dependency_check mmutil
	if [ "$ERRNO" = "0" ]; then
		if [ ! -d "$CAVERNS_BASE/$CAVERNS_MMUTIL_PATH" ]; then
			7z e "$CAVERNS_BASE/caverns.zip" "-o$CAVERNS_BASE/$CAVERNS_MMUTIL_PATH"
			OLD_DIR="$(pwd)"
			cd "$CAVERNS_BASE/$CAVERNS_MMUTIL_PATH" || { ERRNO="CV-1"; return; }
			for f in ./*.MOD; do
				"$DEVKITPRO/tools/bin/mmutil" -d -m "$f"
			done
			cd "$OLD_DIR" || { ERRNO="CV-2"; return; }
		fi
	fi
	ERRNO="0"
}

caverns_platform_init()
{
	# A pretty convoluted directory structure is required to get 7z to
	# put anything where we want it to go.

	CAVERNS_SETUP=$CAVERNS_PATH
	if [ -n "$PLATFORM_CAVERNS_WHICH" ]; then
		CAVERNS_SETUP=$PLATFORM_CAVERNS_WHICH
	fi

	rm -rf "${CAVERNS_BASE:?}/$SUBPLATFORM"
	mkdir -p "$CAVERNS_BASE/$SUBPLATFORM/$PLATFORM_CAVERNS_BASE"
	cp -r "$CAVERNS_BASE/$CAVERNS_SETUP/"* "$CAVERNS_BASE/$SUBPLATFORM/$PLATFORM_CAVERNS_BASE"
}

# $1 - archive to check
# $2 - filename to check for
caverns_check_archive()
{
	7z l "$1" "$2" -r | grep -q "$2"
	return $?
}

# $1 - archive to check
caverns_add()
{
	# 7z actually requires changing to the directory for the copy to not
	# include the containing directory. No, really.

	OLD_DIR="$(pwd)"
	cd "$CAVERNS_BASE/$SUBPLATFORM" || { ERRNO="CV-1"; return; }

	7z a "$1" ./* -y -bsp0 -bso0

	cd "$OLD_DIR" || { ERRNO="CV-2"; return; }
}

build_package_caverns()
{
	caverns_init
	[ "$ERRNO" = "0" ] || { return; }

	caverns_platform_init
	[ "$ERRNO" = "0" ] || { return; }

	for f in "$MZX_BUILD_DIR/build/dist/$SUBPLATFORM"/*.zip
	do
		if caverns_check_archive "$f" "$PLATFORM_CAVERNS_EXEC"
		then
			mzx_log "Found '$PLATFORM_CAVERNS_EXEC' in '$f'; adding Caverns of Zeux at '$PLATFORM_CAVERNS_BASE'"
			caverns_add "$f"
			[ "$ERRNO" = "0" ] || { mzx_error "failed to add caverns to '$f'" $ERRNO; }
		fi
	done
}
