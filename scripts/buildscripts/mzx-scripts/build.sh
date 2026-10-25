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

. "$MZX_SCRIPTS/common.sh"
. "$MZX_SCRIPTS/caverns.sh"
. "$MZX_SCRIPTS/crlf.sh"

OLD_PATH="$PATH"

build_init()
{
	#
	# Check if the current working directory is the MZX repo.
	# Otherwise, use the repo that (should) exist in the working directory.
	#
	if [ -f "config.sh" ]; then
		MZX_BUILD_DIR="$(pwd)"
	else
		MZX_BUILD_DIR="$MZX_WORKINGDIR/megazeux"

		[ -d "$MZX_BUILD_DIR" ] || { mzx_log "Couldn't find MegaZeux repository dir!"; exit 1; }
		cd "$MZX_BUILD_DIR" || { mzx_log "failed to cd to build dir!"; exit 1; }
		[ -f "config.sh" ] || { mzx_log "Couldn't find config.sh!"; exit 1; }
		MZX_BUILD_DIR="$(pwd)"
	fi
	mzx_log "Using MegaZeux repository at $MZX_BUILD_DIR for builds"
	export MZX_BUILD_DIR

	#
	# Make sure the target dir exists.
	#
	mkdir -p "$MZX_TARGET"

	#
	# Reset the git repository and make sure all branches/tags are up-to-date.
	#
	cd "$MZX_BUILD_DIR" || { mzx_log "failed to cd to build dir!"; exit 1; }
	git reset --hard
	git fetch
	git fetch --tags
}

#
# Perform a check to see if git branch(es) have updates. Use after build_init.
# Returns 0 if there are updates or 1 if there are no updates.
#
# $@ - branches to check
#
build_check_branch_updates()
{
	if [ -z "$MZX_BUILD_DIR" ] || [ ! -d "$MZX_BUILD_DIR" ]
	then
		mzx_error "Use after build_init!" 1
		exit 1
	fi
  cd "$MZX_BUILD_DIR" || { mzx_error "failed to cd to build dir!" 2; exit 1; }

  RETVAL=1
  while [ -n "$1" ]
  do
    git checkout "$1"
    shift

    CURRENT=$(git rev-parse "@")
    REMOTE=$(git rev-parse "@{u}")

    if [ "$CURRENT" = "$REMOTE" ]; then
      mzx_log "No updates required for branch '$1'."
    else
      mzx_log "Branch '$1' has updates."
      RETVAL=0
    fi
  done
  return $RETVAL
}

build_remove_debug()
{
	#
	# Clear the debug files out of the updates dir (if applicable).
	#
	if [ -d "build/$SUBPLATFORM/" ]; then
		find "build/$SUBPLATFORM/" -type f -name '*.debug' -delete
	fi

	#
	# Clear the debug files out of the release archive and put them in their own archive.
	#
	cd "build/dist/$SUBPLATFORM/" || { mzx_error "failed to cd to build/dist/$SUBPLATFORM" 14; return; }

	FILES=$(find . -name "*.zip")
	for SRC in $FILES
	do
		# NOTE: 7z uses its own internal globs, hence the quotes.
		if [ -n "$SRC" ] && 7z l "$SRC" "*.debug" -r | grep -q ".debug"
		then
			DEST=$(echo "$SRC" | sed "s/\.zip\$/\.debug\.zip/g")
			7z e "$SRC"        "*.debug" -r
			7z d "$SRC"        "*.debug" -r
			7z a -tzip "$DEST" ./*.debug
			rm ./*.debug
		fi
	done
	cd "$MZX_BUILD_DIR" || { mzx_error "failed to cd to build dir" 15; return; }
}

# $1 subplatform
# $2 tag or branch
# $3 update branch or ""
# $4 extra configuration flags or ""
# $5 "release" or "debug"
build_common()
{
	export SUBPLATFORM=$1
	MZX_GIT_BRANCH=$2
	MZX_UPDATE_BRANCH=$3
	MZX_EXTRA_CONFIG_FLAGS=$4
	MZX_RELEASE_TYPE=$5
	[ -n "$SUBPLATFORM" ] || { mzx_error "no SUBPLATFORM defined" 1; return; }
	[ -n "$MZX_GIT_BRANCH" ] || { mzx_error "no git branch/tag provided" 2; return; }
	[ -f "$MZX_SCRIPTS/platforms/$SUBPLATFORM.sh" ] ||\
	 { mzx_error "couldn't find $MZX_SCRIPTS/platforms/$SUBPLATFORM.sh" 3; return; }

	# This may be set from a previous build...
	unset SDL_PREFIX

	export PATH="$OLD_PATH"
	. "$MZX_SCRIPTS/platforms/default.sh"
	. "$MZX_SCRIPTS/platforms/$SUBPLATFORM.sh"

	export ERRNO=0
	export IS_HOST="false"
	export PLATFORM_CRLF=""
	export PLATFORM_CAVERNS_EXEC=""
	export PLATFORM_CAVERNS_BASE=""
	export PLATFORM_CAVERNS_WHICH=""

	mzx_log ""
	mzx_log "Starting build:"
	mzx_log "  Platform:      $SUBPLATFORM"
	mzx_log "  Type:          $MZX_RELEASE_TYPE"
	mzx_log "  Branch/tag:    $MZX_GIT_BRANCH"
	mzx_log "  Update branch: $MZX_UPDATE_BRANCH"
	mzx_log "  Options:       $MZX_EXTRA_CONFIG_FLAGS"

	cd "$MZX_BUILD_DIR" || { mzx_error "failed to cd to build dir" 4; return; }

	#
	# Initialize platform-dependent variables and perform platform-dependent
	# checks to make sure MegaZeux will be able to build.
	#
	platform_init
	[ "$ERRNO" = "0" ] || { mzx_warn "failed to initialize platform; skipping" $ERRNO; return; }

	#
	# Check out the requested branch.
	#
	git reset --hard
	git checkout "$MZX_GIT_BRANCH"
	git merge
	$MZX_MAKE distclean
	rm -rf build

	#
	# Run config.sh.
	#
	if [ "$MZX_RELEASE_TYPE" = "release" ]; then
		platform_config_release "$MZX_EXTRA_CONFIG_FLAGS"
	else
		platform_config_debug "$MZX_EXTRA_CONFIG_FLAGS"
	fi
	[ "$ERRNO" = "0" ] || { mzx_error "failed to configure for $MZX_RELEASE_TYPE" $ERRNO; return; }
	[ -e "platform.inc" ] || { mzx_error "config.sh failed to create platform.inc for $MZX_RELEASE_TYPE" 10; return; }

	#
	# Build MegaZeux.
	#
	platform_make
	[ "$ERRNO" = "0" ] || { mzx_error "make failed" $ERRNO; return; }

	#
	# Perform make test (if applicable).
	# If this fails it's not a fatal error but it definitely should be looked at.
	#
	platform_make_test
	[ "$ERRNO" = "0" ] || { mzx_warn "make test failed" $ERRNO; ERRNO=0; }

	#
	# Perform any other checks on the build that may be necessary.
	#
	platform_check_build
	[ "$ERRNO" = "0" ] || { mzx_error "build check failed" $ERRNO; return; }

	#
	# Perform LF->CRLF conversion for text files if requested by this platform.
	# This needs to be done before packaging so manifest.sh can compute the
	# correct SHA-256 sums.
	#
	if [ -n "$PLATFORM_CRLF" ]; then
		crlf_convert_repository
	fi

	#
	# Package the build.
	# This will generally produce a build/SUBPLATFORM/ folder containing the
	# build and a build/dist/SUBPLATFORM/ folder containing the release archive.
	# Only check for the build/dist/SUBPLATFORM/ right now.
	#
	platform_package
	[ "$ERRNO" = "0" ] || { mzx_error "package failed" $ERRNO; return; }
	[ -d "build/dist/$SUBPLATFORM" ] || { mzx_error "couldn't find build/dist/$SUBPLATFORM/" 11; return; }

	#
	# If this platform wants to package games in its release archive, add it now.
	# Skip this for testing builds since it's just bloat.
	#
	if [ "$MZX_RELEASE_TYPE" = "release" ] && [ -n "$PLATFORM_CAVERNS_EXEC" ]; then
		build_package_caverns
		[ "$ERRNO" = "0" ] || { mzx_warn "build_package_caverns failed", $ERRNO; }
	fi

	#
	# If this is a release build, separate the debug symbols to their own archive
	# and remove them from the updates directory (if applicable).
	#
	if [ "$MZX_RELEASE_TYPE" = "release" ]; then
		build_remove_debug
	fi

	#
	# All files in build/dist/SUBPLATFORM/ (there may be multiple depending on the
	# platform or if there is a .debug zip) should be moved to the $MZX_TARGET/zips/ folder.
	#
	ZIP_DIR="$MZX_GIT_BRANCH"
	if [ "$MZX_GIT_BRANCH" = "master" ] && [ "$MZX_UPDATE_BRANCH" != "" ]; then
		ZIP_DIR="$MZX_UPDATE_BRANCH"
	fi
	mkdir -p "$MZX_TARGET/zips/$ZIP_DIR/"
	mv build/dist/"$SUBPLATFORM"/* "$MZX_TARGET/zips/$ZIP_DIR/"

	#
	# The build/SUBPLATFORM/ folder should be moved to the $MZX_TARGET/releases/ folder
	# if the platform has requested that updates be generated from this build. If
	# updates were requested, there additionally should be a file called "manifest.txt"
	# in the build path.
	#
	if [ -n "$MZX_UPDATE_BRANCH" ]; then
		if [ ! -d "build/$SUBPLATFORM" ]; then
			mzx_error "couldn't find build/$SUBPLATFORM/" 12
			return
		fi
		if ! find "build/$SUBPLATFORM" -name "manifest.txt" | grep -q "manifest.txt"; then
			mzx_error "couldn't find manifest.txt for $MZX_UPDATE_BRANCH/$SUBPLATFORM" 13
			return
		fi
		mkdir -p "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/"
		rm -rf "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/$SUBPLATFORM/"
		mv "build/$SUBPLATFORM/" "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/"
	fi

	#
	# Clean the working tree.
	#
	$MZX_MAKE distclean
}

# $1 subplatform
# $2 tag or branch
# $3 update branch (optional)
# $4 extra configuration flags (optional)
build_release()
{
	build_common "$1" "$2" "$3" "$4" "release"
}

# $1 subplatform
# $2 tag or branch
# $3 update branch (optional)
# $4 extra configuration flags (optional)
build_debug()
{
	build_common "$1" "$2" "$3" "$4" "debug"
}
