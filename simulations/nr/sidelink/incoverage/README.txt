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

Mode1-CG1 (milestone M11)
  A type-1 configured grant: ue[0] sends 300 B CAMs at 10 Hz over a
  standing 100 ms train reserved at the gNB (slRrc.configuredGrant),
  active from initialization. Observed: 49/49 delivered; app delay a
  constant 52 ms (the phase wait to the next CG occasion - the
  allocator picks the earliest free phase, not a traffic-aligned one);
  ZERO Uu signaling after init.

Mode1-CG1-DynamicBaseline
  The same CAM traffic on dynamic grants: 49/49 delivered at 10 ms
  (the 9 ms request ladder), but every packet costs a full Uu cycle
  (RAC + BSR-grant + SL-BSR + DCI = 4 Uu control messages per packet).
  The honest trade: dynamic = lower latency at this load but per-packet
  signaling; CG = zero signaling and guaranteed capacity, delay set by
  the train phase.

Mode1-CG2 (milestone M11)
  A type-2 configured grant with on/off traffic (two 1.4 s bursts,
  1.6 s silence). The CG train is reserved but dormant; the first
  burst's backlog runs one RAC/SL-BSR cycle and receives a
  cgAction=activate DCI (not a dynamic grant); after each burst the UE
  self-releases (5 empty occasions = 500 ms); the second burst
  re-activates. EV log (t): activate 0.059, release 1.90, re-activate
  3.009, release 4.80; 28/28 delivered.

  Note on CG TB sizing: size tbBytes so whole packets fit the TB
  (here 700 B for two 300 B CAMs +headers). A TB request that spans an
  announced-packet boundary can strand the boundary packet's tail in
  RLC if the flow then goes idle - a pre-existing limitation of the
  D21 virtual-buffer approximation, exposed by bursty traffic on
  releasable grants (with tbBytes 330 the run delivers 27/28).

Mode1Mode2-SharedPool / Mode1Mode2-SharedPool-Random (D34)
  One deliberate shared-pool config: ue[0]'s mode-1 CG train (100 ms,
  subchannel 0) + three 20 ms mode-2 CAM senders on a deliberately tight
  single-subchannel pool, TR 37.885 channel. Demonstrates the one-way
  G26 coupling and its measured limits (5 s, seed 0):
  - mode-2 vs mode-2: sensing works perfectly (0 collisions; the random
    variant's 0 is seed luck - expected phase-alignments/run ~ 0.4).
  - mode-2 vs the CG train: the train is only protected while one of
    its occasions falls inside the 10 ms selection window (~10% of
    selections). In this seed one reselection landed on the CG phase
    (RC=61, outliving the run): its 20 ms train hit every 5th own
    occasion = all remaining CG occasions. Receiver outcomes decompose
    cleanly: the aligned sender missed 5 CG packets by HALF-DUPLEX, one
    receiver lost 5 to INTERFERENCE (SINR -2.7 dB), the other received
    all 49 by CAPTURE (+12.6 dB persistent per-pair shadowing margin).
  - KNOWN LIMITATION (found by this config, logged in the SL-3 plan):
    SlMode2Selector::computeExclusion projects sensed reservations only
    into the T2 selection window - TS 38.214 8.1.4 step 6's exclusion
    of candidates whose OWN future repetitions (j < C_resel) collide
    with a projected reservation is not implemented, so reservations
    longer than the selection window (like a 100 ms CG train against
    20 ms selectors) are under-protected. Fixing it changes mode-2
    selection everywhere (a fingerprint re-anchor), so it is documented
    here and deferred for a consented fix.
  - The gNB stays blind to mode-2 reservations by design (G26): CG
    placement never adapts to mode-2 traffic.

SharedRadio / SharedRadio-Off (milestone M12, D32)
  The opt-in Uu/SL half-duplex arbiter: ue[0] runs concurrent Uu VoIP
  UL+DL and its 100 ms CG CAM train; every UE sends periodic CQI
  feedback on the Uu. Observed (5 s, seed 0):
  - arbiter OFF (independent legs, pre-SL-3): 49/49 CG alerts at every
    receiver, VoIP loss 0 - the physically impossible baseline.
  - arbiter ON: CG PRR drops to 22/49 per receiver. Direct cause: the
    receivers' own periodic CQI feedback transmissions align with every
    ~3rd CG occasion (16 slHalfDuplexUuDrops each; the slots also count
    as unmonitored for sensing); the losses then induce ~11 secondary
    suppressions via the known SL-1 HARQ NDI ambiguity (after a lost
    TB, the next new TB on that process is indistinguishable from a
    retransmission). The Uu direction is untouched (VoIP frame loss 0
    both ways: Uu control is lossless by model, and no DL data frame
    happened to overlap the sparse 0.5 ms SL TXs); the CG sender logs
    27 slUuTxConflicts (own CG TXs during its Uu UL/feedback TXs -
    counted, not suppressed, D32).
  This makes the Rel-16 half-duplex pain point visible: a single-radio
  in-coverage V2X UE loses a large share of its sidelink reception to
  routine Uu signaling, invisibly to the Uu side.

PathPolicy-UuFallback / PathPolicy-Pc5Fallback (milestone M13, D33)
  The Uu/PC5 path-selection policy: unicast CBR between two SL-capable
  UEs under pathSelectionPolicy="uuIfServed". Attached
  (PathPolicy-UuFallback): the flow goes over the Uu through the gNB
  and core - delay 20.1 ms. Detached (PathPolicy-Pc5Fallback,
  identical apps): the policy falls back to PC5 mode-2 - delay 2.4 ms.
  Same throughput, zero loss on both paths; all three classification
  seams (packet holder, technology decision, ip2nic) consult the same
  SlIp2Nic decision (G27). The default pc5IfPeer policy reproduces the
  SL-2 behavior byte-identically.

Mode1-50UE (WP-R scale check, G28)
  50 attached mode-1 UEs, all with sporadic broadcast alerts on dynamic
  grants (~500 request ladders/s through one gNB). Observed (2 s,
  seed 0): RAC preamble collision rate 0.48% (4 of 839 attempts; 64
  preambles are ample), grant-cycle latency a deterministic 9 ms for
  46 of 50 UEs with a worst case of 38 ms (collision -> backoff ->
  retry), ~97.5% alert delivery (the missing share is sender-side
  half-duplex among 50 concurrent trains), 2 s of simulation in under 4 s
  of wall time. Conclusion: no parameter tuning needed at this scale;
  for dense periodic fleets configured grants remain the recommended
  answer (zero request-channel load).

InCoverage-Handover (WP-M / G25 audit)
  2 gNBs, an SL-broadcasting UE forced through a handover (serving
  cell 1 -> 2 at t=3.05 s, 60 mps linear mobility) with a trailing PC5
  peer, Uu VoIP DL running across the handover. Observed: SL bearers,
  sensing state and SL traffic survive the handover teardown untouched
  (243/243 alerts, zero loss), Uu VoIP frame loss 0.
