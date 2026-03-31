# What's New in Simu5G

## v1.3.1-rlcam (2026-03-31)

This is a specialized topic release on top of v1.3.1, integrating contributions
by Esteban Egea Lopez (Universidad Politécnica de Cartagena). The main highlights
are the addition of NR RLC-AM (Acknowledged Mode) and the refactoring of
NR RLC-UM (Unacknowledged Mode) to be closer to the 3GPP specifications.
Integration work (splitting up, organizing, and fingerprint testing the original
patches) was done by Andras Varga (OMNeT++ Core Team).

NR RLC Acknowledged Mode (RLC-AM):

- Added NR RLC-AM implementation: NrRlcAm module with NrAmTxQueue and NrAmRxQueue
  entities, sliding window TX/RX buffers, retransmission buffer, and AM
  data/status PDUs. Implements the core ARQ procedures of 3GPP TS 38.322.
- RLC-AM infrastructure: made LteRlcAm methods virtual, added ILteRlcAm interface
  NED type, made AM/UM submodules pluggable via NED typename, added
  NackInfo/StatusPduData structs, fixed AM header size.

NR RLC Unacknowledged Mode (RLC-UM) refactoring:

- Extracted buffer management from NrUmTxEntity/NrUmRxEntity into dedicated
  RlcUmTransmitterBuffer and RlcUmReceptionBuffer classes, implementing proper
  3GPP TS 38.322 Section 5.2.2 procedures for UM transmission and reception.
- RlcUmReceptionBuffer: fixed reassembly window check, split timer management
  into separate start/stop methods, added proper memory management for buffered
  packets, return SDU completion status from handleSegment().
- RlcUmTransmitterBuffer: SDU segmentation based on MAC grants with byte-level
  interval tracking, SN assignment on last-byte transmission, BSR reporting
  support.
- Changed NR RLC-UM default t_Reassembly from 80ms to 50ms.

Radio Link Failure (RLF):

- Added Radio Link Failure detection and handling: RadioLinkFailure tag,
  MAC/PDCP/HARQ RLF queue cleanup (deleteQueuesRadioLinkFailure in MAC,
  handleRadioLinkFailure in PDCP).

MAC improvements:

- Made HARQ process count configurable via NED parameter (replaced hardcoded
  ENB_TX/RX_HARQ_PROCESSES, UE_TX/RX_HARQ_PROCESSES constants), changed NR
  default to 5.
- Made RAC/BSR timer configuration into NED parameters in LteMacUe, set sensible
  defaults (maxRacAttempts=10, racBackoffMax=20), adjusted defaults for NR.

Other changes:

- Added new statistics signals across channel model, MAC, and RLC layers (RSRP,
  interference, fading, throughput samples, grantedBlocks, bsrSize).
- Added X2HandoverDataMsgSerializer for proper X2 handover data serialization.
- PHY: added RLC-AM module reference for handover buffer cleanup.
- Added example simulation configs for NR RLC-AM testing (VoIP-DL-AM,
  VoIP-DL-AM-Lossy, VoIP-UL-AM).
- Misc small fixes: BackgroundScheduler init order, LtePhyBase channelModel_ type,
  Binder.h include guard, LtePhyEnb debug flag, LcgScheduler TODO.

## v1.3.1 (2025-09-18)

This is a minor update for Simu5G-1.3.0. In addition to fixing regressions
in the previous release and making some cosmetic improvements, the main highlight
of this release is the revamp of the fingerprint test suite, which now provides
a more comprehensive safety net against future regressions. Changes in this
release were contributed by Andras Varga (OMNeT++ Core Team).

Changes:
- Example simulations: Marked abstract configs as such (abstract=true) in omnetpp.ini files
- FlowControlInfo's MacNodeId fields are now properly shown in Qtenv object inspectors
- Replaced EV_ERROR << lines with throwing cRuntimeError
- TrafficLightController: fixed startState NED parameter (also changed type from int to string)
- Fingerprints: CSV files merged into simulations.csv, added missing simulations,
  standardized on the set of fingerprints computed (tplx, ~tNl, sz), translated
  gen_runallexamples.py into Python and improved it

Fix regressions in v1.3.0:
- MECResponseApp: fixed wrong @class annotation
- BackgroundScheduler: fix "binder_ not initialized" error
- MecRequestForegroundApp, MecRequestBackgroundGeneratorApp: add back lost parameter defaults
- BackgroundCellTrafficManager: fix "Not implemented" thrown from getBackloggedUeBytesPerBlock()
- tutorials/nr/omnetpp.ini: fix missing unit for txPower parameter (dBm)

## v1.3.0 (2025-02-06)

- Compatible with **OMNeT++ 6.1.0** and **INET 4.5.4**
- New modules: MultiUEMECApp, MecRnisTestApp, UeRnisTestApp
- Added NED documentation for modules
- Increased reusability of modules via changes such as replacing hardcoded module
  paths in the C++ code with NED parameters (binderModule, macModule, etc.), and
  elimination of ancestorPar() calls by introducing local parameters instead. *
- Other NED adjustments, such as removal of unused NED parameters and splitting
  NED files to have one module per file. See doc/NED-changes.txt for details. *
- Extensive C++ modernization, and adaption of more OMNeT++ best practices. *
- Various bug fixes.
- Changes marked with an asterisk were contributed by Andras Varga (OMNeT++ Core
  Team).

## v1.2.3 (2025-01-10)

- Added support for **OMNeT++ 6.1.0** and **INET 4.5.4**.

## v1.2.2 (2023-04-19)

- Compatible with **OMNeT++ 6.0.1** and **INET 4.5**.
- Tested on Ubuntu 22.04 and macOS Ventura.

## v1.2.1 (2022-07-19)

- Compatible with **OMNeT++ 6.0** and **INET 4.4.0**.
- Tested on Ubuntu 20.04.
- Modifications to support the latest versions of OMNeT++ 6.0 and INET v4.4.0.
- Refactoring of simulation and emulation folders.
- Various bug fixes.

## v1.2.0 (2021-08-30)

- Compatible with **OMNeT++ 6.0 (pre10 and pre11)** and **INET 4.3.2**.
- Tested on Ubuntu 16.04, 18.04, 20.04, macOS Catalina, and Windows 7.
- Added modelling of **ETSI MEC** entities.
- Support for **real-time emulation capabilities** (Linux OS only).
- Modelling of background cells and background UEs for **larger scale** simulations and emulations.
- Several bug fixes.

## v1.1.0 (2021-04-16)

- Compatible with **OMNeT++ 5.6.2** and **INET 4.2.2**.
- Tested on Ubuntu 16.04, 18.04, 20.04, macOS Catalina, and Windows 7.
