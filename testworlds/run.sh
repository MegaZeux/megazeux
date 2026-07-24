#!/bin/sh

# Use make test
usage()
{
	echo "USAGE: ./run.sh [-q] {PLATFORM}"
	echo ""
	echo "OR:    ./run.sh [-q] {PLATFORM} /path/to/{libcore.so|libcore.dylib} (if modular enabled and platform is 'unix' or 'darwin'"
	echo ""
	echo "OR:    make test"
}

get_asan()
{
	if libs=$(ldd "$1" 2>/dev/null); then
		ASAN=$(echo "$libs" | grep -i "lib[^ ]*asan" | awk '{print $3}')
	fi
}

get_preload()
{
	preload="$1"
	get_asan "$1"
	if [ -z "$ASAN" ]; then
		# Some compilers may link it to mzxrun but not libcore.so...
		get_asan "../mzxrun"
	fi
	if [ -n "$ASAN" ]; then
		preload="$ASAN $preload"
	fi
}

quiet="no"
if [ "$1" = "-q" ];
then
	quiet="yes"
	shift
fi

if [ -z "$1" ];
then
	usage
	exit 1
fi

TESTS_DIR=$(dirname "$0")
cd "$TESTS_DIR" || { echo "ERROR: failed to cd to test dir: $TESTS_DIR"; exit 1; }
(
cd .. || { echo "ERROR: failed to cd to megazeux dir"; exit 1; }

# Unix release builds will try to find this if it isn't installed.
cp config.txt megazeux-config

# Give tests.mzx the MZX configuration so it can decide which tests to skip.
cp src/config.h "$TESTS_DIR"
)

mkdir -p log

# Clear out any backup files so they aren't mistaken for tests.
rm -f backup*.mzx

# Clear out the temp folder.
find temp/* ! -name README.md -exec rm -f {} \;

# Clear out the default logs
rm -f log/detailed
rm -f log/summary
rm -f log/failures

# Force mzxrun to use the libraries in its own directory.
# Linux/BSD
export LD_LIBRARY_PATH="."
# macOS
export DYLD_FALLBACK_LIBRARY_PATH="."
# Haiku
export LIBRARY_PATH="$LIBRARY_PATH:."

preload=""
if [ -n "$2" ];
then
	get_preload "$2"
	[ "$quiet" = "yes" ] || echo "Test worlds preload: $preload"
fi

# Coupled with the software renderer, this will disable video in MZX, speeding things up
# and allowing for automated testing. Disabling the SDL audio driver will prevent annoying
# noises from occuring during tests, but shouldn't affect audio-related tests.
# SDL 1.2 and SDL2
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy
# SDL3
export SDL_VIDEO_DRIVER=dummy
export SDL_AUDIO_DRIVER=dummy

# Standalone mode will allow tests.mzx to terminate MZX and no_titlescreen mode
# simplifies things. Disable auto update checking to save time. Some platforms
# might have issues detecting libraries and running from this folder, so run from
# the base folder.
[ "$quiet" = "yes" ] || printf "Running test worlds"

dir="$(pwd)"
cd ..

LD_PRELOAD="$preload" \
./mzxrun \
  "$TESTS_DIR/tests.mzx" \
  video_output=software \
  update_auto_check=off \
  standalone_mode=1 \
  no_titlescreen=1 \
  &

# Attempt to detect a hang (e.g. an error occurred).
# Note: running mzxrun in a subshell breaks this.
mzxrun_pid=$!
psopt=""
i="0"

# shellcheck disable=SC2064 # Warning misses the entire point of this usage.
trap "kill -9 $mzxrun_pid; exit 1" INT

cd "$dir"

# BusyBox ps may not support -e; mzxrun might not appear WITHOUT -e in a chroot.
# -A is equivalent to -e in POSIX, but NetBSD uses -e for something else.
# MSYS2 only supports -e (not -A) but it doesn't need it either.
if ps -A 1>/dev/null 2>/dev/null; then psopt='-A'; fi
# Haiku has completely different formatting and needs reordered fields:
if [ "$(uname -s 2>/dev/null)" = "Haiku" ]; then psopt='-o Id Team'; fi

# In some versions of MSYS2, mzxrun doesn't always appear in ps right away. :(
sleep 1
[ "$quiet" = "yes" ] || printf "."

# shellcheck disable=SC2009 # Not available/relevant on all targets.
# shellcheck disable=SC2086 # Word splitting is deliberate here...
while ps $psopt | grep -q "[ \t]*$mzxrun_pid .*[m]zxrun"
do
	sleep 1
	i=$((i+1))
	[ "$quiet" = "yes" ] || printf "."
	if [ $i -ge 180 ];
	then
		kill -9 $mzxrun_pid
		echo "killing frozen process."
		break
	fi
done
[ "$quiet" = "yes" ] || echo ""

# Clean up some files that MegaZeux currently can't.

rm -f next
rm -f "test"
rm -f saved.sav
rm -f saved2.sav
rm -f LOCKED.MZX
rm -f LOCKED.MZX.locked
rm -f ../megazeux-config
rm -f config.h
rm -f data/audio/drivin.s3m

if [ "$quiet" != "yes" ];
then
	if command -v tput >/dev/null;
	then
		# Color code PASS/FAIL tags and important numbers.
		COL_END=$(tput sgr0)
		COL_RED=$(tput bold)$(tput setaf 1)
		COL_GREEN=$(tput bold)$(tput setaf 2)
		COL_YELLOW=$(tput bold)$(tput setaf 3)

		cat log/failures \
		  | sed -e "s/\[PASS\]/\[${COL_GREEN}PASS${COL_END}\]/g" \
		  | sed -e "s/\[FAIL\]/\[${COL_RED}FAIL${COL_END}\]/g" \
		  | sed -e "s/\[\(WARN\|SKIP\)\]/\[${COL_YELLOW}\1${COL_END}\]/g" \
		  | sed -e "s/passes: \([1-9][0-9]*\)/passes: ${COL_GREEN}\1${COL_END}/g" \
		  | sed -e "s/failures: \([1-9][0-9]*\)/failures: ${COL_RED}\1${COL_END}/g" \

		tput bel
	else
		cat log/failures
	fi
fi

# Exit 1 if there are any failures.

if [ ! -e log/failures ] || grep -q "FAIL" log/failures || grep -q "ERROR" log/failures;
then
	exit 1
else
	exit 0
fi
