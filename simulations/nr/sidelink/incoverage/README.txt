In-coverage sidelink examples (SL-3)
====================================

The first configurations that combine a Uu attachment with the sidelink
(PC5) leg on the same UEs, and the serving-cell pool provisioning
("SIB12-equivalent", genie) added in SL-3 WP-N.

Configurations
--------------

InCoverage-Mode2-Preconfig (WP-M spike)
  1 gNB + 4 attached NrUes with hasSidelink=true, concurrent Uu VoIP
  DL/UL and mode-2 sensing-based SL broadcast on separate carriers
  (Uu 2 GHz component carrier, SL 5.9 GHz preconfig pool).
  Observed: VoIP DL MOS 4.4 / 0 frame loss, UL delivered; 95/95 alerts
  at all three PC5 receivers, mean delay ~6.9 ms.

InCoverage-Mode2 (milestone M9)
  Same scenario, but the pool is provisioned from the serving cell:
  the gNB has hasSidelink=true and carries the pool in
  slGnbRrc.slPoolConfig; the UEs use poolSource="servingCell". The
  UE-side preconfig's pool section is deliberately WRONG (2.4 GHz,
  1 subchannel, 100 ms period) to prove provisioning is effective:
  - with poolSource="servingCell": 95/95 alerts, ~6.9 ms delay, and
    the event-order fingerprint (tplx) is identical to
    InCoverage-Mode2-Preconfig;
  - negative control (poolSource="preconfig", i.e. the wrong pool
    actually used): 24/95 alerts, ~783 ms delay.

InCoverage-Mode2-Mixed
  As InCoverage-Mode2, but ue[3] is detached (out of coverage) and
  falls back to its local preconfig, which carries the true pool.
  Attached and detached UEs interoperate on the same pool: ue[3]
  receives 95/95 alerts like the attached receivers.

Mode1-Dynamic (milestone M10)
  gNB-scheduled sidelink ("mode 1") with dynamic grants: two of four
  attached UEs broadcast sporadic alerts (exponential inter-arrival,
  mean 100 ms), every TB obtained through the full request ladder
    SL backlog -> preamble RAC -> BSR-grant (Uu UL) -> SL-BSR
    -> SlEnbScheduler -> DCI (SlSchedulingGrant, Uu DL) -> finite
    occasion train (1 + 2 preallocated retx occasions) -> PC5 TX.
  Observed (5 s, seed 0): 107/107 alerts delivered OTA; grant-cycle
  latency (slMode1GrantLatency) deterministic 9 ms (contention-free
  RAC ladder on 1 ms Uu TTIs); mean app-to-app delay 10.3 ms;
  95 grant cycles served 107 TBs (spare preallocated occasions pick
  up follow-on packets); concurrent requesters get disjoint
  subchannels (collision-free by construction).

Mode2-Sporadic
  The mode-2 baseline at identical offered load: no request cycle,
  sensing-based self-selection. Observed: 91/91 delivered, mean delay
  9.8 ms - at this light load both modes are lossless and the mode-1
  request ladder (9 ms) costs about as much as mode-2's selection
  window wait; mode 1's value is determinism under load (no selection
  collisions), mode 2's is gNB-independence.

InCoverage-Handover (WP-M / G25 audit)
  2 gNBs, an SL-broadcasting UE forced through a handover (serving
  cell 1 -> 2 at t=3.05 s, 60 mps linear mobility) with a trailing PC5
  peer, Uu VoIP DL running across the handover. Observed: SL bearers,
  sensing state and SL traffic survive the handover teardown untouched
  (243/243 alerts, zero loss), Uu VoIP frame loss 0.
