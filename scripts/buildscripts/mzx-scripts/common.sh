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
# Common checks and defines for the buildscripts.
#

if git ls-files --error-unmatch "$0" "$MZX_SCRIPTS" >/dev/null 2>&1; then
	echo "ERROR: attempting to run this script from 'scripts/'."
	echo "The scripts directory is tracked by Git and may be modified when"
	echo "checking out different tags/branches for MZX builds."
	echo ""
	echo "You should duplicate 'scripts/buildscripts/' to another directory and"
	echo "run this script from that directory instead."
	exit 1
fi

if [ ! -w "$MZX_SCRIPTS" ]; then
	echo "ERROR: current user lacks write permissions for '$MZX_SCRIPTS'."
	exit 1
fi

if [ ! -w "$(dirname "$MZX_TARGET")" ]; then
	echo "ERROR: current user lacks write permissions for '$(dirname "$MZX_TARGET")'."
	exit 1
fi

#
# Determine whether to use gmake or make.
# Both MZX_MAKE and MZX_MAKE_PARALLEL can be overriden by the user.
#
if [ -z "$MZX_MAKE" ]; then
	if command -v gmake >/dev/null 2>&1; then
		export MZX_MAKE="gmake"
	else
		export MZX_MAKE="make"
	fi
fi
if [ -z "$MZX_MAKE_PARALLEL" ]; then
	export MZX_MAKE_PARALLEL="$MZX_MAKE -j8"
fi

mzx_log()
{
	echo "$1" 1>&2
}

mzx_warn()
{
	echo "WARN:" "$1" "(code $2)" 1>&2
}

mzx_error()
{
	echo "ERROR:" "$1" "(code $2)" 1>&2
}

# $@ commands to check for (e.g. "7z")
cmd_check()
{
	MSG=""
	while [ -n "$1" ]; do
		if ! command -v "$1" >/dev/null 2>&1; then
			MSG="$MSG$1;"
		fi
		shift
	done
	if [ -n "$MSG" ]; then
		ERRNO="CMD:$MSG"
	fi
}
