#!/bin/bash
#
# Simu5G RLC conformance suite -- build + run via opp_test.
#
# Mirrors inet/tests/protocol/wifi/run-tests.sh: opp_test extracts every .test's %file
# into ./work and a single --deep build links them (+ Simu5gRlcTestSupport.h +
# libprotocoltest + libINET + libsimu5g) into ONE `rlctests` binary. Each test's
# %inifile selects its program via *.tester.testName; %contains / %expected-failure
# assert the verdict.
#
# Prereqs (see README.md): INET built (libINET), Simu5G built (libsimu5g), and the
# vendored protocol test framework built (../lib/build.sh -> libprotocoltest; this
# script builds it on demand). The Simu5G signal mapping is baked into the vendored
# framework -- no INET patch needed.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
#
set -e
cd "$(dirname "$0")"
RLC_DIR="$(pwd)"
# REBUILD=0 reuses an existing ./work build (iterate the run phase without recompiling).
: "${REBUILD:=1}"

INET_DIR="${INET_DIR:-$(cd ../../../../inet && pwd)}"
SIMU5G_DIR="${SIMU5G_DIR:-$(cd ../../.. && pwd)}"
LIB_DIR="$(cd ../lib && pwd)"   # the framework vendored into Simu5G
TESTS="${*:-$(find common lte nr -name '*.test' | sort)}"

# Build the vendored framework lib if it isn't there yet.
[ -e "$LIB_DIR/libprotocoltest.so" ] || (cd "$LIB_DIR" && INET_DIR="$INET_DIR" ./build.sh)

if [ "$REBUILD" != "0" ] || [ ! -e work/rlctests ]; then
    rm -rf work
    mkdir -p work
    opp_test gen $TESTS

    cd work
    printf 'LIBS += -Wl,-rpath,%s/src -Wl,-rpath,%s/src -Wl,-rpath,%s\n' \
        "$INET_DIR" "$SIMU5G_DIR" "$LIB_DIR" > makefrag
    opp_makemake -f --deep -o rlctests \
        -I"$RLC_DIR" -I"$LIB_DIR" -I"$INET_DIR/src" -I"$SIMU5G_DIR/src" \
        -L"$INET_DIR/src" -lINET \
        -L"$SIMU5G_DIR/src" -lsimu5g \
        -L"$LIB_DIR" -lprotocoltest
    make MODE="${MODE:-release}" -j"$(nproc)"
    BIN="$(find "$PWD" -name rlctests -type f -perm -u+x | head -1)"
    [ -n "$BIN" ] && [ "$BIN" != "$PWD/rlctests" ] && ln -sf "$BIN" rlctests
    cd ..
fi

# NED path: this suite's networks, the framework types, Simu5G, INET.
export NEDPATH="$RLC_DIR/ned:$LIB_DIR:$SIMU5G_DIR/src:$SIMU5G_DIR/simulations:$INET_DIR/src"
opp_test run -v -p rlctests $TESTS
