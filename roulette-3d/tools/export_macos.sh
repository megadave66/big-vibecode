#!/bin/bash
# Headless macOS export. Usage: tools/export_macos.sh [output.zip]  (default build/macos/Roulette3D.zip)
# Fails if the import or export logs show script errors, or if the zip is missing.
set -u
cd "$(dirname "$0")/.."
GODOT_BIN="${GODOT_BIN:-/opt/homebrew/bin/godot}"
OUT="${1:-build/macos/Roulette3D.zip}"
LOG="build/export.log"
mkdir -p "$(dirname "$OUT")" build
rm -f "$OUT"
"$GODOT_BIN" --headless --path . --import > "$LOG" 2>&1
"$GODOT_BIN" --headless --path . --export-release "macOS" "$OUT" >> "$LOG" 2>&1
code=$?
if grep -nE "SCRIPT ERROR|Parse Error|Failed to load" "$LOG"; then
	echo "Export FAILED: script errors in $LOG" >&2
	exit 1
fi
if [ $code -ne 0 ] || [ ! -s "$OUT" ]; then
	echo "Export FAILED (exit $code), see $LOG" >&2
	exit 1
fi
echo "Export OK: $OUT ($(wc -c < "$OUT") bytes)"
