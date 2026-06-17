#!/usr/bin/env bash
#
#                  Simu5G
#
# Standalone unit-test runner for the Sionna feature's pure C++ utilities
# (ManifestReader, SionnaTable). These classes depend only on liboppsim
# (cRuntimeError) + the in-tree vendored nlohmann/json.hpp, so they compile
# and run without the OMNeT++/INET simulation kernel — enabling fast TDD.
#
# Usage: tests/sionna/unit/run_unit_tests.sh
# Exits nonzero if any check fails.

set -euo pipefail

# Resolve repo root from this script's location.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Locate the OMNeT++ install (need include/ + lib/liboppsim).
OPP_ROOT="${OMNETPP_ROOT:-}"
if [ -z "$OPP_ROOT" ]; then
    if command -v opp_run >/dev/null 2>&1; then
        OPP_ROOT="$(cd "$(dirname "$(command -v opp_run)")/.." && pwd)"
    fi
fi
if [ -z "$OPP_ROOT" ] || [ ! -f "$OPP_ROOT/include/omnetpp.h" ]; then
    echo "ERROR: could not locate OMNeT++ (set OMNETPP_ROOT or add opp_run to PATH)" >&2
    exit 2
fi

SIONNA_DIR="$REPO_ROOT/src/simu5g/stack/phy/channelmodel/sionna"
JSON_INC="$REPO_ROOT/src/simu5g/mec/utils/httpUtils"   # in-tree nlohmann/json.hpp 3.9.1

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -DWITH_SIONNA -I$REPO_ROOT/src -I$JSON_INC -I$OPP_ROOT/include"
LDFLAGS="-L$OPP_ROOT/lib -loppsim"

echo "Compiling Sionna unit tests..."
"$CXX" $CXXFLAGS \
    "$SCRIPT_DIR/test_manifest_table.cc" \
    "$SIONNA_DIR/ManifestReader.cc" \
    "$SIONNA_DIR/SionnaTable.cc" \
    $LDFLAGS \
    -o "$OUT/test_manifest_table"

echo "Running Sionna unit tests..."
LD_LIBRARY_PATH="$OPP_ROOT/lib:${LD_LIBRARY_PATH:-}" "$OUT/test_manifest_table"
