# Vendored: INET Protocol Test Framework

This directory is Simu5G's vendored copy of the INET protocol test framework
library (`tests/protocol/lib` in the INET repository), so that protocol
conformance suites can be built and run against the INET version Simu5G
currently uses (v4.5.4), where the framework does not exist yet.

See `AUTHORING.md` (vendored alongside, unmodified except for one package-name
note) for how to write tests with it.

## Provenance

Vendored on 2026-07-22 from the INET repository:

- Base: branch `topic/av/protocol-test-framework`, tip `2e15cb0658`
  ("wifi suite: report NOT-MODELED tests as EXPECTEDFAIL, not disguised PASS").
- Plus the one Simu5G-enabling commit from `topic/ta/protocol-test-framework`:
  `f23d53cb2a` ("protocoltest: observe Simu5G RLC/PDCP boundary signals") —
  maps Simu5G's `sentPacketToLowerLayer` / `receivedPacketFromLowerLayer` /
  `sentPacketToUpperLayer` / `receivedPacketFromUpperLayer` signal names to the
  framework's normalized EventKinds. With this baked in, no INET-side patch is
  needed anymore (older suite READMEs that instruct patching INET's
  `ProtocolTester.cc` are obsolete against this copy).

Not imported (INET-specific): the demo networks (`ProtocolTestDemo.ned`,
`ProtocolTestMitmDemo.ned`, `Mipv6Demo.ned`, `PlcaMultidropDemo.ned`,
`WifiBlockAckDemo.ned`), the cookbook programs (`ProtocolTests.cc`), the demo
runner (`run-demo.sh`, `omnetpp.ini`), and the wifi/ipv6 conformance suites.

## Local deltas vs upstream (keep this list current!)

1. `ProtocolTester.h`, `PacketTap.h`: base class `SimpleModule` →
   `cSimpleModule`, include `inet/common/INETDefs.h` instead of
   `inet/common/SimpleModule.h`. Upstream sits on a newer INET whose
   `SimpleModule` (a thin `ModuleMixin<cSimpleModule>` shim) does not exist in
   v4.5.4; nothing else of the mixin is used.
2. NED package renamed `inet.protocoltest` → `simu5g.protocoltest`
   (`package.ned`, `ProtocolTester.ned`, `PacketTap.ned`), so this copy can
   never collide with a future INET-shipped one. The C++ namespace stays
   `inet::protocoltest` to keep the code diff against upstream minimal.
3. `build.sh`: `INET_DIR` defaults to `$INET_ROOT` (the variable Simu5G's own
   Makefile uses) or the `../inet` sibling of the Simu5G tree; links
   `-lINET$(D)` so `MODE=debug` binds `libINET_dbg`.
4. `AUTHORING.md`: one-line note about the renamed NED package.

## Building

```sh
./build.sh                 # release; INET_DIR/INET_ROOT select the INET checkout
MODE=debug ./build.sh      # debug flavor (libprotocoltest_dbg.so)
```

Verified: builds warning-clean (release) with clang against INET v4.5.4 and
omnetpp 6.3; `inet::protocoltest::ProtocolTester` / `PacketTap` register on
load and the NED package parses.

## Using from a suite

A consumer suite (cf. `tests/protocol/rlc/` on the `dev-ta-portfwd` branch for
the pattern) adds this directory to its NEDPATH, links `-lprotocoltest$(D)`
with `-L` pointing here, declares a tester submodule
`tester: ProtocolTester` (import `simu5g.protocoltest.ProtocolTester`), and
selects a registered program via `*.tester.testName`.

## Re-syncing with upstream

Keep local edits minimal and listed above. To see what upstream changed since
this copy:

```sh
git -C ../../../../inet diff 2e15cb0658 topic/av/protocol-test-framework -- tests/protocol/lib
```

and re-apply the deltas above onto any refreshed copy.
