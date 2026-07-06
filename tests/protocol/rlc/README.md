# Simu5G RLC Conformance / Behaviour Test Suite

A **spec-driven** behavioural test suite for the Simu5G RLC layer (`src/simu5g/stack/rlc/`),
covering **TM / UM / AM** for **both LTE (TS 36.322) and NR (TS 38.322)**. It uses Simu5G's
**vendored copy of the INET protocol test framework** ([`../lib/`](../lib/), provenance in
its `README-VENDORED.md`; originally `inet/tests/protocol/lib` on branch
`topic/av/protocol-test-framework`) — the same machine that drives the INET WiFi and IPv6
conformance suites — to match the live RLC packet trace against expected patterns and emit a
`PROTOCOLTEST <name>: PASS`/`FAIL` verdict.

Tests are written against the **standard and the repo's spec extracts**
([`36322-j00.md`](../../../36322-j00.md), [`38322-j20.md`](../../../38322-j20.md)), not against
the implementation. A test that fails because Simu5G is incomplete is a **finding**, not a
suite defect — the suite doubles as a live conformance report and cross-checks
[`RLC-spec-gap-analysis.md`](../../../RLC-spec-gap-analysis.md).

> **Status: 16 cases built and green** using the **vendored framework** (`../lib/`) against
> **stock INET v4.5.4 + omnetpp 6.3** (originally authored on omnetpp 6.4 + the INET
> `topic/av/protocol-test-framework` checkout). They build into one
> `rlctests` binary and run to their expected verdicts: **14 CONFORMS (PASS) + 2 NOT-MODELED
> (EXPECTEDFAIL), aggregate PASS**. These cover the data-plane mechanisms of all six
> mode/RAT combinations plus two of the confirmed spec gaps. The ARQ/loss-driven and
> lifecycle mechanisms are **designed but not coded** — they are not deterministically
> exercisable in this harness (see [Conformance findings](#conformance-findings) and
> [Testability limits](#testability-limits)). No INET-side changes are needed: the
> Simu5G signal mapping (below) is baked into the vendored framework.

## Outcome semantics (as in the WiFi suite)

Every test asserts the **spec** behaviour, so `%contains` always expects the program to
`PASS`; the two real outcomes differ only in whether Simu5G does it:

- **CONFORMS** — Simu5G produces the spec behaviour, the program PASSes → opp_test **PASS**.
- **NOT-MODELED** — Simu5G lacks the feature, the faithful assertion misses its deadline and
  the program FAILs. The `.test` carries `%expected-failure:`, so opp_test reports
  **EXPECTEDFAIL** (an honest, first-class expected failure) and the run stays green. If
  Simu5G later implements the feature the program PASSes and the row flips to CONFORMS.
- **COVERAGE-LTD** — the mechanism *is* implemented, but exercising it needs stimulus the
  loss-free single-flow harness doesn't produce (deterministic loss, reordering, window
  stall, duplicates). Listed for completeness; enable with a degraded-channel ini variant
  and greedy cardinalities (`oneOrMoreTimes`/`atLeastTimes`).

A red opp_test **FAIL** therefore means a **regression** of a CONFORMS test.

## How the framework hears Simu5G signals (no action needed)

The framework subscribes network-wide to INET's seven `LayeredProtocolBase` packet signals
(`packetSentToLower`, …). **Simu5G RLC emits the same packets under different signal names**
(`sentPacketToLowerLayer`, `receivedPacketFromLowerLayer`, `sentPacketToUpperLayer`,
`receivedPacketFromUpperLayer`), all carrying an `inet::Packet*`. Selectors match by the
**exact emitted signal-name string** (`EventPattern::scopeMatches`: `selSignal !=
event.signalName`), so the tester just has to **subscribe** to those names too: its
`signalKinds` table in `ProtocolTester::initialize()` maps the four Simu5G names to the same
`EventKind`s (and its `subscribeStateSignals()` knows they are packet signals, so a
`.signal("...")` step is not re-subscribed as a scalar). **This mapping is already part of
the vendored framework** in [`../lib/`](../lib/) (upstream commit `f23d53cb2a` on
`topic/ta/protocol-test-framework`) — nothing to patch, in INET or anywhere else.

Nothing else in the matcher is Simu5G-specific. Everything else the suite
needs — content predicates, module-path scoping — is done in the tests.

## How the RLC layer is observed

Simu5G's RLC is **flattened**: no `LteRlc` compound module. Each NIC has a fixed **`rlcMux`**
dispatcher (NR UEs additionally have **`nrRlcMux`**), plus **dynamically-created per-bearer
entity modules** named `<prefix>-<mode>-<dir>-<peerId>-<drbId>` (e.g. `rlc-um-tx-1-1`,
`nrRlc-am-tx-1-1`), children of `cellularNic`.

| Want to observe | Where | Signal | Works for |
|---|---|---|---|
| **RLC PDU on the wire** (SN, poll bit, STATUS, segmentation) | `…cellularNic.rlcMux` / `nrRlcMux` | `sentPacketToLowerLayer` (TX), `receivedPacketFromLowerLayer` (RX, incl. STATUS) | **all modes, LTE + NR** — the universal point |
| **SDU reassembled up to PDCP** | `…cellularNic.<prefix>-{um,am}-rx-*` entity | `sentPacketToUpperLayer` | **NR UM/AM only** (LTE entities register but never emit) |
| **SDU down from PDCP** | `…cellularNic.<prefix>-am-tx-*` entity | `receivedPacketFromUpperLayer` | **NR AM only** |
| SDU delivery for **LTE / TM** | the PDCP RX entity (RLC RX `out` feeds it) | PDCP signal | LTE UM/AM, TM |
| AM state/counters | `…cellularNic` (emitted on the entity) | `txWindowOccupation`, `txWindowFull`, `retransmissionPdu` (scalar; `.signal(...).is(v)`) | NR AM |

`on("path")` matches the emitter as a **path-segment prefix** (the character after the prefix
must be `.`), so to target a dynamic per-bearer entity you must spell its **full** module
name including the `-<peerId>-<drbId>` suffix (e.g.
`on("ue[0].cellularNic.nrRlc-um-rx-1-1")` — peer id 1, DRB 1 on these single-cell nets), or
scope at the stable parent `on("ue[0].cellularNic")`. Filter out MAC *new-data notifications*
(marker packets
carrying `LteRlcNewDataTag`) — the `Simu5gRlcTestSupport.h` predicates already do, since they
require a concrete RLC chunk at the front.

### Content matching — why C++ predicates

The PacketFilter **string** engine (`.packet("LteRlcAmPdu.snoMainPacket == 3")`) only reaches
whole-content chunks or headers with a registered `ProtocolDissector`; Simu5G registers none,
and the NR data PDUs (`NrRlcUmDataPdu`, `NrRlcAmDataPdu`) are hand-written with no
`cClassDescriptor`. So the suite reads fields with the **`.match(lambda)` + `peekAtFront<T>()`
escape hatch** in [`Simu5gRlcTestSupport.h`](Simu5gRlcTestSupport.h) (the same approach as
INET's `WifiTestSupport.h` for the opaque PHY-mode tag). Predicates cover LTE/NR UM SN + FI
framing, LTE/NR AM SN, the NR poll bit, and LTE/NR STATUS `ackSn`/NACKs.

## Layout

```
rlc/
  README.md                 this file (canonical doc + conformance matrix)
  run-tests.sh              build (--deep, one binary) + run via opp_test
  Simu5gRlcTestSupport.h    C++ content predicates (peekAtFront<RLC PDU>)
  ned/                      RlcTestNetworks.ned: SingleCell(+_Standalone) + a `tester`
  ini/                      _common.ini + _lte.ini + _nr.ini (single-UE DL flows)
  common/  lte/  nr/        one <Name>.test per case
```

Each `.test` carries its program in `%file: <Name>.cc` (a named `Define_ProtocolTest(...)`),
selects it via `*.tester.testName`, includes `../ini/_{lte,nr}.ini`, sets the RLC-mode knob,
and asserts the verdict with `%contains` (+ `%expected-failure` for NOT-MODELED).

## Selecting mode & RAT (the ini knobs)

- **RLC mode** per bearer comes from the app's traffic category → an `ip2nic` knob
  (`@enum(TM,UM,AM)`): `**.cellularNic.ip2nic.conversationalRlc` (VoIP\* apps),
  `…interactiveRlc` (gaming\*), `…streamingRlc` (VoD\*), `…backgroundRlc` (everything else).
  Set the one matching your app to `"TM"`/`"UM"`/`"AM"`.
- **LTE vs NR framing** is topology-driven (an NR UE↔gNB flow is an NR bearer →
  `soFraming=true`) and pinned by `rrc.bearerManagement.{rlc,nrRlc}*EntityModuleType`. The
  `_lte.ini`/`_nr.ini` bases put you on the right leg by choice of network.

## Build & run

Prereqs: INET built (`libINET`), Simu5G built (`libsimu5g`), and the vendored framework
built (`../lib/build.sh`; `run-tests.sh` builds it on demand). The `simu5g.protocoltest` +
`simu5g` NED packages are put on the NEDPATH by the runner. Then:

```sh
cd tests/protocol/rlc
INET_DIR=../../../../inet SIMU5G_DIR=../../.. ./run-tests.sh          # all cases
./run-tests.sh nr/Nr_Am_PollAndStatus.test                           # a subset
```

Author against the real trace by adding `*.tester.logEvents = true` and reading
`work/<Name>/test.out`.

> **Build/ini caveats to finalise on first run.** (1) `SingleCell*.ned` pulls IPv4 addressing
> from a scenario-local `demo.xml`; wire `*.configurator.config` to a resolvable path or swap
> in an auto-assigning configurator (noted in `ini/_lte.ini`). (2) `--deep` links every
> `%file` into one `rlctests` binary, so each program is a **named** `Define_ProtocolTest`
> (not `Define_ProtocolTestProgram`) and the `tester` submodule selects it.

## Conformance matrix

**Result** is the actual opp_test outcome of a run (not a prediction): ✅ **PASS** = CONFORMS ·
⛔ **EXPECTEDFAIL** = NOT-MODELED (faithful assertion the implementation can't satisfy) · ◐ =
**not coded** (mechanism needs stimulus this harness can't produce deterministically — see
[Testability limits](#testability-limits)). Gap numbers reference
[`RLC-spec-gap-analysis.md`](../../../RLC-spec-gap-analysis.md).

### TM — transparent mode
| Test | RAT | Mechanism | Spec | Result |
|------|-----|-----------|------|--------|
| `Tm_Lte_PassThrough` | LTE | SDU forwarded verbatim, no RLC header | 36.322 §5.1.1 | ✅ PASS |
| `Tm_Nr_PassThrough` | NR | SDU forwarded verbatim, no RLC header | 38.322 §5.2.1 | ✅ PASS |

### UM — unacknowledged mode
| Test | RAT | Mechanism | Spec | Result |
|------|-----|-----------|------|--------|
| `Lte_Um_SnIncrement` | LTE | SN = VT(US), VT(US)++ per PDU | 36.322 §5.1.2.1 | ✅ PASS |
| `Lte_Um_Segmentation` | LTE | FI field marks a fragmented SDU | 36.322 §5.1.2 / §6.2.2.6 | ✅ PASS |
| `Lte_Um_Delivery` | LTE | reassemble + in-order deliver to PDCP | 36.322 §5.1.2.2 | ✅ PASS |
| `Nr_Um_Segmentation` | NR | SO segmentation `[startOffset,endOffset]`+SN, reassembly | 38.322 §5.2.2 | ✅ PASS |
| `Nr_Um_Delivery` | NR | reassemble + deliver to PDCP | 38.322 §5.2.2.2 | ✅ PASS |
| `Lte_Um_Reordering` | LTE | in-order delivery via `t-Reordering`, VR(UR/UX/UH) | 36.322 §5.1.2.2 | ◐ needs loss |
| `Nr_Um_OutOfWindowDiscard` | NR | discard SN outside reassembly window | 38.322 §5.2.2.2.2 | ◐ needs loss |

### AM — acknowledged mode
| Test | RAT | Mechanism | Spec | Result |
|------|-----|-----------|------|--------|
| `Lte_Am_SnIncrement` | LTE | SN = VT(S), VT(S)++ per PDU (snoFragment) | 36.322 §5.1.3.1 | ✅ PASS |
| `Lte_Am_Segmentation` | LTE | SDU split into first…last AMD fragments | 36.322 §5.1.3 / §6.2.1.4 | ✅ PASS |
| `Lte_Am_Delivery` | LTE | reassemble + in-order deliver to PDCP | 36.322 §5.1.3.2 | ✅ PASS |
| `Nr_Am_SnIncrement` | NR | SN = TX_Next, TX_Next++ per SDU (snoMainPacket) | 38.322 §5.2.3.1 | ✅ PASS |
| `Nr_Am_PollAndStatus` | NR | poll bit P set; STATUS triggered by poll | 38.322 §5.3.3 / §5.3.4 | ✅ PASS |
| `Nr_Am_StatusAckAdvances` | NR | STATUS ACK_SN advances (cumulative positive ACK) | 38.322 §5.2.3.1 / §5.3.4 | ✅ PASS |
| `Nr_Am_Delivery` | NR | reassemble + deliver to PDCP | 38.322 §5.2.3.2 | ✅ PASS |
| `Lte_Am_PollBit` | LTE | AMD PDU carries poll bit P | 36.322 §5.2.2 | ⛔ EXPECTEDFAIL — no P field (gap #4) |
| `Nr_Am_MaxRetxRlf` | NR | RETX_COUNT = maxRetxThreshold → RLF indication | 38.322 §5.3.2 | ⛔ EXPECTEDFAIL — silent local flag (gap #1) |
| `Nr_Am_NackRetransmit` | NR | NACKed SN retransmitted | 38.322 §5.3.2 | ◐ needs loss |
| `Nr_Am_PollRetransmit` | NR | `t-PollRetransmit` expiry re-polls | 38.322 §5.3.3.4 | ◐ needs a lost poll/STATUS |
| `Nr_Am_StatusProhibit` | NR | `t-StatusProhibit` rate-limits STATUS | 38.322 §5.3.4 | ◐ needs bursty NACKs |
| `Nr_Am_RetxTailDrop` | NR | partial-fit NACK must retransmit the **whole** range | 38.322 §5.3.2 | ◐/⛔ needs loss; broken (gap #3) |
| `Lte_Am_StatusReport` | LTE | STATUS PDU emitted on a reception gap | 36.322 §5.1.3.2 / §5.2.3 | ◐ needs loss (none emitted when loss-free) |
| `Lte_Am_NackRetransmit` | LTE | NACKed PDU retransmitted (txNumber++) | 36.322 §5.2.1 | ◐ needs loss |
| `Lte_Am_ResegmentationOnRetx` | LTE | retx AMD PDU too big for grant is re-segmented | 36.322 §5.2.1 | ◐/⛔ needs loss; 1-bit placeholder (gap #4) |
| `Lte_Am_StatusProhibit` | LTE | STATUS rate-limited by `t-StatusProhibit` | 36.322 §5.2.3 | ◐/⛔ ad-hoc interval (gap #4) |

### Cross-cutting (lifecycle)
| Test | RAT | Mechanism | Spec | Result |
|------|-----|-----------|------|--------|
| `Am_WindowStalling` | LTE/NR | no TX of SN outside the TX window | 38.322 §5.2.3.1 / 36.322 §5.1.3.1 | ◐ needs stalled ACKs |
| `Am_DuplicateDiscard` | LTE/NR | discard duplicate SN / byte range on RX | 38.322 §5.2.3.2.2 / 36.322 §5.1.3.2.2 | ◐ needs a duplicate |
| `Rlc_ReEstablishment` | LTE/NR | re-establish: RX flush-then-deliver, reset state | 38.322 §5.1.2 / 36.322 §5.4 | ⛔ delete-and-recreate, no RX flush (gap #2) — needs handover |
| `Rlc_SduDiscard` | LTE/NR | PDCP SDU-discard prunes RLC TX buffer | 38.322 §5.4 / 36.322 §5.3 | ⛔ no PDCP→RLC discard (gap #6) — needs a discard timer |

## Conformance findings

Across the six mode/RAT combinations, the **data-transfer core is spec-faithful and confirmed**:

- **TM (§5.1.1 / §5.2.1)** — pass-through with no RLC header, both RATs. ✅
- **UM (§5.1.2 / §5.2.2)** — SN assignment, segmentation (LTE FI framing; NR byte-offset SO), and
  reassembly/in-order delivery, both RATs. ✅
- **AM data (§5.1.3 / §5.2.3)** — SN assignment, segmentation, reassembly/delivery, both RATs;
  and on **NR** the ARQ feedback loop that is observable without loss — **polling (P bit)**,
  **STATUS-on-poll**, and **advancing cumulative ACK_SN**. ✅

The **two confirmed gaps** the suite asserts as EXPECTEDFAIL match the gap analysis exactly:

- **LTE AM has no poll (P) field** (`Lte_Am_PollBit`, gap #4) — LTE AM ARQ is timer-driven, not
  spec polling.
- **NR AM `maxRetxThreshold` → RLF is a silent local flag** (`Nr_Am_MaxRetxRlf`, gap #1) — no
  indication reaches upper layers, so radio-link failure is invisible to the stack.

Two behavioural details surfaced while authoring (both legitimate observations, not bugs):

- **LTE AM fragments even a 69 B VoIP SDU** (fixed fragmentation unit → `frag0/1/2`), whereas
  **LTE UM packs the same SDU whole** — different segmentation policy per mode.
- **LTE AM emits no STATUS at all in a loss-free run** (STATUS is gap-triggered, and with no
  polling there is nothing to trigger it), while **NR AM emits periodic STATUS** with a steadily
  advancing ACK_SN. This is the practical face of the LTE-vs-NR ARQ-fidelity gap.

## Testability limits

The remaining spec mechanisms are **ARQ- or lifecycle-driven** and are *designed* (rows above)
but **not coded as pass/fail tests**, because this harness cannot produce their stimulus
deterministically:

- **Loss-driven ARQ** (NACK retransmission, `t-PollRetransmit`, `t-StatusProhibit` bursts,
  retransmission tail-drop, LTE STATUS-on-gap, reordering, out-of-window/duplicate discard):
  Simu5G's **HARQ absorbs sub-catastrophic block errors before RLC ever sees a loss**, and
  degrading the channel far enough to defeat HARQ instead breaks attachment (the AM TX window
  stalls with nothing to retransmit). Verified empirically: sweeping Tx power, distance,
  `targetBler`, and `**.mac.maxHarqRtx = 0` yielded **zero** RLC-level retransmissions until the
  link collapsed entirely. This is the same limitation the INET WiFi suite documents for
  `WifiRetransmission`; deterministic RLC loss needs a **MAC-level packet-drop hook** (a
  `PacketTap` equivalent spliced on the rlc↔mac gate), which does not exist in-stack.
- **Lifecycle** (`Rlc_ReEstablishment` needs a handover; `Rlc_SduDiscard` needs a PDCP
  discard-timer trigger): both are **NOT-MODELED** per the gap analysis (gaps #2, #6) and also
  require a scenario this single-cell, single-flow harness does not set up.

To exercise these, add a MAC-gate `PacketTap` (drop the Nth AMD PDU to force a NACK) and a
handover/discard scenario, then the ◐ rows can be promoted to real ✅/⛔ tests.

## Adding a test

Copy an existing `.test` (e.g. `nr/Nr_Am_PollAndStatus.test`). Put the program in
`%file: <Name>.cc` (unique basename, a named `Define_ProtocolTest`), select it via
`*.tester.testName`, include the right `ini/_{lte,nr}.ini`, set the RLC-mode knob, and set
`%contains` to the expected verdict (`PASS` for CONFORMS; add `%expected-failure` for a
faithful NOT-MODELED assertion). Match RLC fields with the `Simu5gRlcTestSupport.h`
predicates; observe PDUs on the mux and SDU delivery on the RX entity (NR) or PDCP (LTE).
Author against the real trace with `*.tester.logEvents = true`.
