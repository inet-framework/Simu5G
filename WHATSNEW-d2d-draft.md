<!--
  DRAFT entry for WHATSNEW.md, covering the D2D separation work.
  Not yet merged into WHATSNEW.md: the release/version heading under which this
  belongs is the maintainer's call. Placeholder heading below is "Upcoming
  release"; replace with the actual "## vX.Y.Z (date)" line at release time and
  prepend the section to WHATSNEW.md.
-->

## Upcoming release

### D2D support factored into a separate package

All device-to-device (D2D) code has been moved out of the core LTE/NR stack
into a dedicated `simu5g.stack.d2d` package, and D2D is now enabled per node
via a single `hasD2D` switch. The core LTE/NR modules no longer contain any D2D
machinery, and clean (non-D2D) NR nodes no longer construct it. D2D remains a
research prototype and is not based on any specific 3GPP specification.

- **`hasD2D` node switch.** `LteUe` and `eNodeB` (and, by inheritance, `NrUe`,
  `gNodeB`, `LteCar`, `NrCar`) gained a `bool hasD2D = default(false)`
  parameter. Setting `**.hasD2D = true` on a node (or a whole fleet) selects
  the D2D-capable `cellularNic` variant for that node. An explicit
  `cellularNic.typename` still works and takes precedence over the flag.

- **`d2dCapable` parameter removed.** The former `d2dCapable` node parameter no
  longer exists; use `hasD2D` (or an explicit D2D NIC typename) instead.

- **D2D module types moved to `simu5g.stack.d2d`.** The D2D module types were
  relocated into the new package, and the NR stack gained dedicated D2D leaf
  types (previously D2D was baked into the plain `NrNicUe`/`NrNicEnb` and their
  submodules):

  - NICs: `LteNicUeD2D`, `LteNicEnbD2D` (same names, new package), plus the new
    `NrNicUeD2D`, `NrNicEnbD2D`.
  - MAC: `LteMacUeD2D`, `LteMacEnbD2D`, `NrMacUeD2D`, `NrMacGnbD2D`, and the D2D
    AMC `LteAmcD2D` / `NrAmcD2D`.
  - PHY: `LtePhyUeD2D`, `LtePhyEnbD2D`, `NrPhyUeD2D`, and the D2D channel models
    `D2dRealisticChannelModel` / `D2dNrChannelModel_3GPP38_901`.
  - RLC / IP: `RlcMuxD2D`, `Ip2NicD2D`, `UmTxEntityD2D`, `UmRxEntityD2D`.
  - RRC: `RrcD2D`, `HandoverControllerD2D`, `D2DModeController`,
    `D2dModeSelectionBase`, `D2DModeSelectionBestCqi`.

  Because module typenames in ini files are unqualified, these package moves do
  **not** break existing ini files. For example
  `cellularNic.rrc.d2dModeSelection.typename = "D2DModeSelectionBestCqi"`
  continues to resolve unchanged. Most of these types are selected
  automatically by the D2D NIC (including the D2D channel model, chosen via the
  NIC's `lteChannelModelType` default), so D2D configurations rarely need to
  name them explicitly.

- **D2D mode selection configured through RRC.** The D2D-capable eNB/gNB NIC
  sets `rrc.hasD2DModeSelection = true`, which instantiates the periodic
  mode-selection module; the policy is chosen with
  `cellularNic.rrc.d2dModeSelection.typename`. (The older `d2dModeSelection` /
  `d2dModeSelectionType` NIC parameters are gone.)

Behavioral notes users may observe:

- Clean (non-D2D) NR nodes no longer construct any D2D machinery, so they no
  longer record D2D-specific statistics — the `-nan` D2D scalars that used to
  appear in non-D2D runs are gone.
- Clean NR nodes no longer run the periodic D2D mode-selection tick.
