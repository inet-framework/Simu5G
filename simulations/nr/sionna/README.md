# Running existing scenarios with the Sionna channel (Plan A)

The Sionna channel model is opt-in. It needs one network-level module
(`SionnaManager`, like `binder`). The supported networks already carry an optional
`sionnaManager` submodule behind a `hasSionnaManager` parameter; for any other
network you generate a thin wrapper.

**Scope: static scenarios only** (Plan A v1). A moving node leaves the ray-traced
table and the run errors out with a "Plan A v1 is static" hint - pin mobility
(`StationaryMobility`) or use a static config.

## A. Networks that already have `hasSionnaManager`

These declare `sionnaManager: <default("SionnaManager")> like ISionnaManager if
hasSionnaManager` (default off, so existing runs are unaffected):

- `simu5g.simulations.nr.networks.{SingleCell_Standalone, SingleCell_Standalone_D2D,
  MultiCell_Standalone, SingleCell_withSecondaryGnb, MultiCell_withSecondaryGnb}`
- `simu5g.simulations.lte.networks.{SingleCell, SingleCell_D2D, SingleCell_D2DMulticast,
  MultiCell, MultiCell_X2Mesh, MultiCell_D2DMultihop}`

Just add a config that turns it on and includes the shared settings:

```ini
[Config MyScenario-Sionna]
extends = <the original config, e.g. SingleCell-DL>
include <path to>/simulations/nr/sionna/sionna-common.ini
*.hasSionnaManager = true

# if the scenario places nodes at z=0, give them height (section overrides the
# original [General]); bump the z constraint first:
**.mobility.constraintAreaMaxZ = 50m
*.gnb*.mobility.initialZ = 25m    # or *.eNB*.mobility.initialZ for LTE
*.ue*.mobility.initialZ  = 1.5m
```

Run it (the two-ray backend generates the table at startup - no GPU, no committed
artifact): `simu5g -u Cmdenv -c MyScenario-Sionna omnetpp.ini`

## B. Any other network: generate a wrapper

OMNeT++ can't inject a submodule from the ini, so for a network that does NOT
already have the parameter (i.e. one that is NOT listed in section A above - e.g.
the cars/emulation networks, or your own), generate a `<Name>Sionna` wrapper that
adds the `sionnaManager`. (Do NOT run it on a section-A network: the wrapper would
re-declare the inherited `sionnaManager` and the NED fails to compile.)

```sh
python3 sionnaize.py simu5g.simulations.nr.cars.Highway
# -> generated/HighwaySionna.ned (git-ignored; regenerate any time)
```

Then point your config at it: `network =
simu5g.simulations.nr.sionna.generated.HighwaySionna` (no `hasSionnaManager`
needed - the wrapper always adds the manager) plus the same `include` and height
settings as above. (Highway is a mobility scenario, so pin `StationaryMobility`
for the static channel - see the scope note at the top.)

## Notes

`sionna-common.ini` switches both ends to `SionnaChannelModel`, points the
`SionnaManager` at the bundled `sionna_rt.py` (tworay backend), and records results
to `results/`. For real ray tracing set `*.sionnaManager.backend = "sionna"` (needs
`sionna-rt` installed); for inter-cell interference set
`*.sionnaManager.interferenceMode = "allPairs"`.

See `omnetpp.ini` here for a worked example (configs `Sionna`, `SionnaLive`,
`SionnaCompare`, on `SingleCell_Standalone` with `hasSionnaManager = true`).
