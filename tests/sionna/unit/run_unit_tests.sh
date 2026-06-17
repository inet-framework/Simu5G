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

# INET include path (SionnaManager.cc includes <inet/common/InitStages.h>).
INET_SRC="${INET_ROOT:-}"
if [ -n "$INET_SRC" ] && [ -d "$INET_SRC/src" ]; then
    INET_SRC="$INET_SRC/src"
elif [ -d "$REPO_ROOT/../inet/src" ]; then
    INET_SRC="$(cd "$REPO_ROOT/../inet/src" && pwd)"
else
    INET_SRC=""
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

# -I$REPO_ROOT/src lets "simu5g/mec/utils/httpUtils/json.hpp" resolve exactly as in
# the real opp_makemake build (INCLUDE_PATH = -I. == -Isrc).
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -DWITH_SIONNA -DINET_IMPORT -I$REPO_ROOT/src -I$OPP_ROOT/include"
[ -n "$INET_SRC" ] && CXXFLAGS="$CXXFLAGS -I$INET_SRC"
LDFLAGS="-L$OPP_ROOT/lib -loppsim"

rc=0

echo "Compiling ManifestReader + SionnaTable unit tests..."
"$CXX" $CXXFLAGS \
    "$SCRIPT_DIR/test_manifest_table.cc" \
    "$SIONNA_DIR/ManifestReader.cc" \
    "$SIONNA_DIR/SionnaTable.cc" \
    $LDFLAGS \
    -o "$OUT/test_manifest_table"

echo "Running ManifestReader + SionnaTable unit tests..."
LD_LIBRARY_PATH="$OPP_ROOT/lib:${LD_LIBRARY_PATH:-}" "$OUT/test_manifest_table" || rc=1

# SionnaManager.cc references inet::INITSTAGE_LOCAL (defined in libINET, not header-
# only), so the contract test links libINET in addition to liboppsim.
INET_LIB_DIR=""
if [ -n "$INET_SRC" ] && [ -f "$INET_SRC/libINET.so" ]; then
    INET_LIB_DIR="$INET_SRC"
fi

if [ -n "$INET_SRC" ] && [ -n "$INET_LIB_DIR" ]; then
    echo "Compiling SionnaManager contract-assertion unit tests..."
    "$CXX" $CXXFLAGS \
        "$SCRIPT_DIR/test_contract_assertion.cc" \
        "$SIONNA_DIR/SionnaManager.cc" \
        "$SIONNA_DIR/ManifestReader.cc" \
        "$SIONNA_DIR/SionnaTable.cc" \
        $LDFLAGS -L"$INET_LIB_DIR" -lINET \
        -o "$OUT/test_contract_assertion"

    echo "Running SionnaManager contract-assertion unit tests..."
    LD_LIBRARY_PATH="$OPP_ROOT/lib:$INET_LIB_DIR:${LD_LIBRARY_PATH:-}" "$OUT/test_contract_assertion" || rc=1
else
    echo "WARNING: INET lib not found; skipping SionnaManager contract-assertion test." >&2
fi

exit $rc
