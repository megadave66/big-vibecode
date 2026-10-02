#!/bin/bash
# Smoke-test the exported macOS build: unzip it, run the real binary headless for
# 600 frames, and fail on script errors. Usage: tools/verify_build.sh [zip]
set -u
cd "$(dirname "$0")/.."
ZIP="${1:-build/macos/Roulette3D.zip}"
DIR="build/macos/verify"
rm -rf "$DIR" && mkdir -p "$DIR"
unzip -q "$ZIP" -d "$DIR" || { echo "unzip FAILED" >&2; exit 1; }
APP=$(find "$DIR" -maxdepth 1 -name "*.app" | head -1)
BIN=$(find "$APP/Contents/MacOS" -type f -perm -u+x | head -1)
[ -x "$BIN" ] || { echo "no executable in $APP" >&2; exit 1; }
"$BIN" --headless --quit-after 600 > build/verify.log 2>&1
code=$?
if grep -nE "SCRIPT ERROR|Parse Error|Failed to load|Can't open" build/verify.log; then
	echo "Build run FAILED: errors in build/verify.log" >&2
	exit 1
fi
[ $code -eq 0 ] || { echo "Build run exit code $code" >&2; exit 1; }
grep -q "Roulette 3D ready" build/verify.log || { echo "main scene did not start" >&2; exit 1; }
echo "Build run OK: $(grep "Roulette 3D ready" build/verify.log); 600 frames headless, exit 0"

# Spin check in the release build: each launch must start on the ball track (not left in
# the last pocket), and the results must not all be one number.
SPINS=6
"$BIN" --headless -- --autospin=$SPINS > build/verify_spins.log 2>&1
grep -E "^autospin" build/verify_spins.log
launches=$(grep -c "^autospin launch" build/verify_spins.log)
[ "$launches" -eq "$SPINS" ] || { echo "Spin check FAILED: $launches of $SPINS launches" >&2; exit 1; }
off_track=$(grep "^autospin launch" build/verify_spins.log | awk '{split($4,a,"="); if (a[2] < 0.37) n++} END {print n+0}')
[ "$off_track" -eq 0 ] || { echo "Spin check FAILED: $off_track launches did not start on the track" >&2; exit 1; }
distinct=$(grep "^autospin settled" build/verify_spins.log | awk '{print $3}' | sort -u | wc -l | tr -d ' ')
[ "$distinct" -gt 1 ] || { echo "Spin check FAILED: every spin landed on the same number" >&2; exit 1; }
echo "Spin check OK: $SPINS launches on the track, $distinct different numbers"
