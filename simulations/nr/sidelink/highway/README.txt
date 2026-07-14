NR sidelink — TR 37.885 highway calibration scenario
=====================================================

170 vehicles on a 2 km, 6-lane highway (3 lanes per direction, 4 m lane
spacing), all out-of-coverage (no gNodeB, no core network) and moving at a
constant 100 km/h. Vehicle x positions are drawn uniformly along the 2 km
stretch, which is equivalent to the Poisson drop of TR 37.885 clause 6.1.2
for a fixed vehicle count.

Every UE broadcasts CAM-style periodic alert messages (300B @ 10 Hz) to the
multicast group 224.0.0.10 and also receives them from its neighbors, over
the mode-2 sensing-based sidelink (TS 38.321 5.22 resource selection) and
the TR 37.885 highway channel model (pathloss + shadowing, SINR with
co-slot interference from the SL transmission map, PSCCH/PSSCH BLER
decoding).

The network-level slStatsCollector submodule records PRR (packet reception
ratio) and PIR (packet inter-reception time) per 20 m transmitter-receiver
distance bin as end-of-run scalars. View the results with opp_scavetool or
the IDE's Analysis tool (result files under results/<configname>/).

Configs:

- Highway: the full 170-UE calibration scenario described above.

- Highway-Small: a reduced 40-UE, 2s variant for the automated regression
  suite (fingerprint-row variant).

- Highway-300: a 300-UE, 2s variant to check scalability.

PRR-vs-distance results (Highway config, 10s, seed 0, release build):

    d [m]    0-20  100-120  200-220  300-320  400-420  480-500
    PRR      0.993 0.986    0.983    0.978    0.975    0.972

  PRR decreases monotonically from 0.993 to ~0.972 over 0..500 m
  (total 0.981); PIR stays at ~100.7-102.3 ms, i.e. at the CAM period, as
  expected for near-unity PRR. Losses are dominated by resource collisions
  and half-duplex, not by the channel (LOS SINR is high throughout 500 m).

  Ready-made analysis charts live in highway.anf (open in the IDE, or
  export with:
    opp_charttool imageexport -p inet-4.5.4=$INET_ROOT simu5G=$SIMU5G_ROOT \
        -f png -d doc/media highway.anf
  ): PRR/PIR vs distance, per-UE CBR over time, and the SL SINR
  distribution. Sequence charts of the PC5 message flows are rendered from
  recorded eventlogs by doc/make-sequence-charts.py. See doc/showcase.md
  for a guided tour.

  Compared to published 5G-LENA NR-V2X mode-2 highway results (which are
  typically ~0.85-0.95 at 300-500 m for comparable densities), this curve
  sits at the optimistic end. Known modeling deltas responsible: LOS-only
  in this config (no NLOSv vehicle blockage), no fast fading, ideal (GNSS)
  synchronization, threshold-based SCI decoding, and no in-band emission /
  adjacent-channel effects.

Scalability (Highway-300, 2s, release): ~72 s wall clock, ~1.0 GB RSS;
with slPhy.slTxRange = 1000m fan-out pruning ~61 s / 0.93 GB. Total PRR
drops to ~0.917 at this density (30% pool occupancy).
