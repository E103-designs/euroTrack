#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GFX_DIR=${GFX_DIR:-"$HOME/Documents/Arduino/libraries/Adafruit_GFX_Library"}
CXX=${CXX:-clang++}
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/eurotrack-tests.XXXXXX")
trap 'rm -rf "$TEST_ROOT"' 0 HUP INT TERM
mkdir -p "$TEST_ROOT/preview"
for FORM in modular single; do
  if [ "$FORM" = modular ]; then SKETCH="$ROOT/firmware/Eurotrack_V4_1/Eurotrack_V4_1.ino"; else SKETCH="$ROOT/firmware/Eurotrack_V4_1_SingleFile/Eurotrack_V4_1_SingleFile.ino"; fi
  python3 - "$ROOT/firmware/Eurotrack_V4_1/tests/firmware_tests.cpp" "$SKETCH" "$TEST_ROOT/firmware_tests.cpp" <<'PYTEST'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
old = '#include "../Eurotrack_V4_1.ino"'
assert source.count(old) == 1
pathlib.Path(sys.argv[3]).write_text(source.replace(old, '#include "' + sys.argv[2] + '"'))
PYTEST
  "$CXX" -std=c++17 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -DARDUINO=10819 -DEUROTRACK_HOST_TEST -I"$ROOT/firmware/Eurotrack_V4_1/tests/stubs" -I"$GFX_DIR" "$TEST_ROOT/firmware_tests.cpp" "$GFX_DIR/Adafruit_GFX.cpp" -o "$TEST_ROOT/$FORM-test"
  (cd "$TEST_ROOT" && "./$FORM-test")
done
