#!/bin/bash
# Run the full gdUnit4 suite headless. Usage: tools/run_tests.sh [-a res://tests/some_test.gd]
# Default runs everything under res://tests.
set -u
cd "$(dirname "$0")/.."
GODOT_BIN="${GODOT_BIN:-/opt/homebrew/bin/godot}"
# Import once so class_name caches exist.
"$GODOT_BIN" --headless --path . --import >/dev/null 2>&1
ARGS="$*"
if [ -z "$ARGS" ]; then ARGS="-a res://tests"; fi
"$GODOT_BIN" --headless --path . -s -d --remote-debug tcp://127.0.0.1:0 res://addons/gdUnit4/bin/GdUnitCmdTool.gd $ARGS --ignoreHeadlessMode -c
code=$?
"$GODOT_BIN" --headless --path . --quiet -s res://addons/gdUnit4/bin/GdUnitCopyLog.gd $ARGS >/dev/null 2>&1
echo "Run tests ends with $code"
exit $code
