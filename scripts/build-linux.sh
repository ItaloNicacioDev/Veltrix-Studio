#!/usr/bin/env bash
# ===========================================================================
#  Veltrix Studio core - Linux build script (GCC/Clang + Ninja + CMake presets)
#
#  Usage:
#      scripts/build-linux.sh            (release + tests)
#      scripts/build-linux.sh debug      (debug + tests)
#
#  Requires: cmake >= 3.24, ninja, and a C++20 compiler (g++ >= 11, clang >= 14).
#  Honors CC/CXX environment variables; falls back to the system compiler.
# ===========================================================================
set -euo pipefail

CONFIG="${1:-release}"
case "$CONFIG" in
    debug)   PRESET="linux-debug" ;;
    release) PRESET="linux-release" ;;
    *) echo "ERROR: unknown configuration '$CONFIG' (expected: debug or release)" >&2; exit 2 ;;
esac

command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found" >&2; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "ERROR: ninja not found" >&2; exit 1; }

echo
echo "=== veltrix core: configure ($PRESET) ==="
cmake --preset "$PRESET"

echo
echo "=== veltrix core: build ==="
cmake --build --preset "$PRESET"

echo
echo "=== veltrix core: tests ==="
ctest --preset "$PRESET" --output-on-failure
