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

export MZX_REPOSITORY="https://github.com/AliceLR/megazeux.git"

. "$MZX_SCRIPTS/common.sh"

common_setup_environment()
{
	if [ -n "$MSYSTEM" ]; then
		pacman --needed --noconfirm -S git make tar curl p7zip
	fi
	cmd_check git make tar curl 7z
	if [ "$ERRNO" != "0" ]; then
		mzx_error "failed setup; missing required tools" "$ERRNO"
		return
	fi

	mkdir -p "$MZX_WORKINGDIR"

	#
	# Set up the MegaZeux repository if this isn't being run from one.
	#
	if [ ! -f "config.sh" ]; then
		if [ ! -d "$MZX_WORKINGDIR/megazeux" ]; then
			if ! git clone "$MZX_REPOSITORY" "$MZX_WORKINGDIR/megazeux"; then
				ERRNO="GIT-1"
				mzx_error "failed to create MZX repository" "$ERRNO"
				return
			fi
		fi
	fi
}

# $@ - platforms to initialize.
setup_environment()
{
	export ERRNO=0
	common_setup_environment
	[ "$ERRNO" = "0" ] || { exit 1; }

	while [ -n "$1" ]; do
		mzx_log "Performing setup for: $1"
		if [ -f "$MZX_SCRIPTS/platforms/$1.sh" ]; then

			. "$MZX_SCRIPTS/platforms/default.sh"
			. "$MZX_SCRIPTS/platforms/$1.sh"

			export ERRNO=0
			platform_setup_environment
			if [ ! "$ERRNO" = "0" ]; then
				mzx_error "failed $1" "$ERRNO"
			fi
		fi
		shift
	done
}
