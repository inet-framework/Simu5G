#!/bin/sh
# Build the protocol-test framework library (Simu5G's vendored copy, see
# README-VENDORED.md).
#
# Links against a pre-built INET checkout (headers + libINET.so). The default is
# $INET_ROOT (the variable Simu5G's own Makefile uses), falling back to the ../inet
# sibling of the Simu5G tree; override INET_DIR if your built INET lives elsewhere.
set -e
cd "$(dirname "$0")"
INET_DIR="${INET_DIR:-${INET_ROOT:-$(cd ../../../../inet && pwd)}}"

printf 'LIBS += -Wl,-rpath,%s/src\n' "$INET_DIR" > makefrag
opp_makemake -f --deep -s -o protocoltest -I"$INET_DIR/src" -L"$INET_DIR/src" '-lINET$(D)'
make MODE="${MODE:-release}" -j"$(nproc)"
