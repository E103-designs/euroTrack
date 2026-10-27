#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
GFX_DIR="${GFX_DIR:-$HOME/Documents/Arduino/libraries/Adafruit_GFX_Library}"
TEST_BIN="$(mktemp /private/tmp/eurotrack-tests.XXXXXX)"
trap 'rm -f "$TEST_BIN"' EXIT
clang++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
  -DARDUINO=10819 -DEUROTRACK_HOST_TEST -Itests/stubs -I"$GFX_DIR" \
  tests/firmware_tests.cpp "$GFX_DIR/Adafruit_GFX.cpp" -o "$TEST_BIN"
"$TEST_BIN"
