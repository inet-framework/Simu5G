# Running existing scenarios with the Sionna channel (Plan A)

The Sionna channel model is opt-in and needs one network-level module
(`SionnaManager`, like `binder`). OMNeT++ cannot inject a submodule into an
existing network from the ini, so each network you want to "Sionna-ize" needs a
tiny wrapper that adds it. Everything else is just ini settings.

**Scope: static scenarios only** (Plan A v1). A moving node leaves the ray-traced
table and the run errors out with a "Plan A v1 is static" hint - pin mobility
(`StationaryMobility`) or use a static config.

## Recipe

1. **Generate wrapper network(s)** for the networks you want to run:

   ```sh
   python3 sionnaize.py simu5g.simulations.nr.networks.MultiCell_Standalone \
                        simu5g.simulations.nr.networks.SingleCell_Standalone
   ```

   This writes `generated/<Name>Sionna.ned` (each just `extends <Name>` and adds a
   `sionnaManager`). The `generated/` dir is git-ignored - regenerate any time.

2. **Add a config** that points at the wrapper and includes the shared settings:

   ```ini
   [Config MyScenario-Sionna]
   extends = <the original config, e.g. CBR-DL>
   include ../sionna/sionna-common.ini
   network = simu5g.simulations.nr.sionna.generated.<Name>Sionna

   # if the scenario places nodes at z=0, give them height here (section overrides
   # the original [General]); bump the z constraint first:
   **.mobility.constraintAreaMaxZ = 50m
   *.gnb*.mobility.initialZ = 25m
   *.ue*.mobility.initialZ  = 1.5m
   ```

3. **Run** it (the two-ray backend generates the table at startup - no GPU, no
   committed artifact):

   ```sh
   simu5g -u Cmdenv -c MyScenario-Sionna omnetpp.ini
   ```

`sionna-common.ini` switches both ends to `SionnaChannelModel`, points the
`SionnaManager` at the bundled `sionna_rt.py` (tworay backend), and records results
to `results/`. For real ray tracing set `*.sionnaManager.backend = "sionna"`
(needs `sionna-rt` installed); for inter-cell interference set
`*.sionnaManager.interferenceMode = "allPairs"`.

See `../sionna/omnetpp.ini` for a worked example (configs `Sionna`,
`SionnaLive`, `SionnaCompare`).
