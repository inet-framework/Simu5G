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

PRR-vs-distance results (Highway config, 10s, seed 0, release build;
remeasured under SL-2's real link adaptation, grantMcs=16 / D15):

    d [m]    0-20  100-120  200-220  300-320  400-420  480-500
    PRR      0.989 0.974    0.966    0.959    0.961    0.958

  PRR decreases from 0.989 to ~0.958 over 0..500 m (total 0.969); PIR
  stays at ~101.2-102.7 ms, i.e. at the CAM period, as expected for
  near-unity PRR. Losses are dominated by resource collisions and
  half-duplex, not by the channel (LOS SINR is high throughout 500 m).
  The curve sits ~1-2 points below the SL-1 measurement: the BLER lookup
  now uses the coarse CQI-9 equivalent of MCS 16 instead of the SL-1
  stub's CQI-6 interpretation of the default MCS.

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

Highway-Platoon / Highway-Platoon-AckNack (milestone M6, WP-I / SL-2):
groupcast with PSFCH HARQ feedback. ue[0..4] form a platoon in lane 0 at
~60 m spacing; the leader groupcasts 300B platoon messages at 10 Hz over
a groupcast SLRB while the other 35 vehicles keep broadcasting CAMs
(blind mode) on the same PSFCH-enabled pool (psfchPeriod 4).

- Highway-Platoon: option 1 (psfchMode "nackOnly", mcr 150m). Only
  members within the MCR NACK a lost TB (ue[1] at 60 m, ue[2] at 120 m;
  ue[3]/ue[4] at 180/240 m stay silent); silence at the feedback deadline
  means success. Verified with a lowered leader TX power: in-MCR NACKs
  trigger retransmissions on the next occasion, the beyond-MCR loss is
  correctly ignored. Note a documented abstraction: option-1 NACKs share
  one PSFCH resource (as in Rel-16), but this model treats co-resource
  feedback as mutual interference rather than the energy-detection
  superposition real option 1 exploits - simultaneous same-distance NACKs
  can annihilate; the nearer NACK usually survives.

- Highway-Platoon-AckNack: option 2 (psfchMode "ackNack"): every member
  answers on its own member-rank-derived PSFCH resource; the leader frees
  the TB early once all 4 ACKs arrive and retransmits on any NACK or on
  DTX (per psfchDtxPolicy).

Highway-Congested / Highway-Congested-Control (WP-K / SL-2, D22):
CBR-based congestion control on a deliberately congested variant (80
vehicles, 20 Hz CAMs, single-subchannel pool, 50 ms reservation period).
The cbrConfig levels cap TX power and the own channel-occupancy ratio;
above the CR limit a UE skips TX occasions (slCrDeferred). Measured
(5s, seed 0, after the exact-drain LCP fix): mean CBR 0.509 with
control vs 0.610 without - the intended channel-load reduction. At
full offered load the delivery trade is now visibly one-sided: PRR at
0-20 m is 0.970 with control vs 0.979 without, total PRR 0.578 vs
0.702 - at this density the CR-limit deferrals queue whole CAMs and
cost delivery at every range. (The pre-fix figures showed a near-range
PRR *improvement*, but that was partly an artifact: deferral-induced
backlog was silently shed by the virtual-buffer accounting bug -
stranded packets were never transmitted and never entered the PRR
denominator, flattering the controlled variant. The honest conclusion:
this cbrConfig's CR limit is too aggressive for 20 Hz CAMs on a
single-subchannel pool; congestion control here buys channel headroom
and lower interference to OTHER services, not own-fleet PRR.)

Retransmission-efficiency exit gate (M6), measured on the unicast pair at
the PER knee (basic/, grantMcs=12, 2400 m, shadowing off, 2s, after the
exact-drain LCP fix): blind retx=1 delivers 93/95 using 380 TB
transmissions; PSFCH feedback delivers 95/95 using 195 (129 initial -
queued packets share TBs - + 66 NACK-driven) - full delivery at ~49%
fewer transmissions.
