NR sidelink (PC5) — basic out-of-coverage example
=================================================

Two cell-less NrUe nodes (no gNodeB anywhere, servingNodeId = 0), 20 m apart.
ue[0] runs an AlertSender towards the multicast group 224.0.0.10; ue[1] runs
an AlertReceiver and is a member of the group (see demo.xml).

Configs:

- OutOfCoverage: sidelink disabled (WP-A spike). Verifies that cell-less UEs
  initialize cleanly and that all application packets are dropped at the
  serving-node checks (HandoverPacketHolderUe / TechnologyDecision).

- Broadcast (milestone M1, WP-C): hasSidelink = true. Alert messages are
  delivered app-to-app over the PC5 leg:
  ip2nic (SL classification) -> PDCP -> RLC UM -> slMac -> slPhy
  -> peer slPhy -> slRlcMux -> PDCP -> ip2nic -> app.
  WP-C simplifications: one static preconfigured periodic grant per UE
  (staticGrantSlotOffset), ideal PHY decoding (SlIdealChannelModel),
  no HARQ, no sensing.

- Broadcast-Tr37885 (milestone M2, WP-D): same, over the TR 37.885 highway
  V2V channel (pathloss + per-pair shadowing, SINR with co-slot interference
  from the SL transmission map, PSCCH threshold decode, PSSCH BLER decode)
  with SL-RSRP/SINR/CBR/frame-loss statistics.

  PER-vs-distance validation sweep (shadowing off, grantMcs=12, 26 dBm,
  10-PRB subchannel, mu=1). Since SL-2's real link adaptation (SlMcsTable,
  D15), MCS 12 means 16QAM R=434/1024 -> TBS 233B and a coarse CQI-7
  equivalent for the BLER lookup (SL-1 used the MCS index directly as the
  CQI, i.e. a far less robust CQI-12 curve, with a 1000B TBS stub):

     dist [m]   50   1600  2000  2400  2800  3200  3600
     received   95   95    72    51    10    2     0     (of 96)

  Loss onset ~2000 m matches the expected SINR = 77.6 - 20log10(d) dB
  (~11.6 dB at 2000 m) against the per-CQI-7 BLER curve with the framework's
  per-PRB success-probability convention ((1-BLER)^numPRBs, as on Uu).
  At the default grantMcs=6 (QPSK, CQI-5 equivalent, TBS 123B) the range is
  correspondingly longer.
  Sweep command:
    for d in ...; do simu5g -u Cmdenv -c Broadcast-Tr37885 \
      --'*.ue[1].mobility.initialX'=$((290+d))m \
      --'*.ue[*].cellularNic.slChannelModel.shadowing'=false \
      --'*.ue[*].cellularNic.slMac.grantMcs'=12 omnetpp.ini; done

- Mode2-50UE / Random-50UE (milestone M3, WP-E): 50 UEs broadcasting
  CAM-style 300B alerts at 10 Hz on one deliberately tight pool (1
  subchannel, 200 slot-resources per 100ms period, ~25% occupancy) over the
  TR 37.885 channel. Mode2-50UE uses the TS 38.321 5.22 sensing-based
  selection (the default); Random-50UE picks uniformly from the same
  selection window without sensing - the standard baseline.

  Since SL-2's real link adaptation (SlMcsTable, D15) the configs pin
  grantMcs = 16 (TBS 349B on the 10-PRB subchannel) so a whole CAM still
  fits one TB and the pool occupancy stays ~25%. Measured sensing gain
  (5s, seed 0, 122549 possible receptions):

                     received   PRR
     Mode2-50UE      116285     ~94.9%
     Random-50UE     95317      ~77.8%

  With SPS, a random pick that collides keeps colliding for the lifetime of
  its reselection counter; sensing avoids reserved resources, leaving mostly
  cold-start and simultaneous-reselection collisions.

  (The mode-2 figure moved down ~1 point when the step-6 exclusion started
  reaching the occurrences of reservations sensed more than one selection
  window ago - see the SL-3 step-6 work. A stricter exclusion shrinks the
  surviving candidate set, and in this deliberately loaded pool that makes
  simultaneous reselections collide slightly more often. The random control
  is unaffected: it selects from an empty sensing database.)

- Broadcast-Tr37885-BlindRetx (WP-F): one blind HARQ retransmission per TB
  (preconfig blindRetx: 1); the copy rides the next occasion of the same
  grant train, receivers soft-combine (harqReduction convention) and
  suppress duplicate deliveries by (source, HARQ process, NDI).

  Measured deep in the PER knee (initialX=3090m i.e. 2800 m, grantMcs=12,
  shadowing off, 2s): 48/48 of the transmittable packets are delivered with
  one blind copy (the copy halves the grant-train capacity), against 16/48
  without. App-level counts amplify TB losses via RLC UM reordering, so the
  retx gain is end-to-end.

- Unicast-UM (milestone M5, WP-H / SL-2): PC5 unicast between the two
  out-of-coverage UEs with bidirectional CBR traffic (40B / 20ms each way)
  and NO broadcast SLRB configured. The first packet toward the peer is
  classified by the D16 static rule (unicast destination address resolves
  to an SL-capable peer -> PC5) and triggers the genie PC5-RRC link
  establishment (D17): one UM SLRB per direction from unicastSlrbDefaults
  (DRB id 32, allocated dynamically), full TX+RX chains at BOTH endpoints
  (symmetric establishment, D18). Mode-2 selection over the TR 37.885
  channel; 95/95 packets delivered app-to-app in each direction at 20 m.

- Unicast-AM (milestone M5, WP-H / SL-2): the same unicast pair on an
  acknowledged-mode SLRB (unicastSlrbDefaults rlcType "AM"), on the NR AM
  entities of TS 38.322. The receiver's RLC generates STATUS PDUs which its
  co-located TX side transmits on the same logical channel, taking their own
  mode-2 grants through the receiver's SL MAC like any data - this is what
  the symmetric link establishment (D18) exists for. 19/19 packets delivered
  app-to-app each way (CBR 40B / 100ms).

- Unicast-UM-Psfch (milestone M6, WP-I / SL-2): the unicast pair at the
  PER knee (grantMcs=12, 2400 m, shadowing off) with PSFCH-based HARQ
  feedback (psfchPeriod 4). Lost TBs are NACKed by the receiver in the
  PSSCH's PSFCH slot and retransmitted on the next grant occasion (up to
  harqMaxRtx); missing feedback resolves per psfchDtxPolicy. Measured
  (2s, seed 0): 95/95 delivered in each direction, on 95-96 transport-block
  transmissions per UE - i.e. the NACK-driven retransmissions cost far less
  channel time than blind copies would at the same delivery, which is the
  M6 exit gate.

- Unicast-QoS (milestone M7, WP-J / SL-2): two PC5 QoS flows to one
  destination over one unicast link. DSCP resolves the PFI (tos >> 2);
  the per-destination SL-SDAP entity (D20) maps PFI 1 -> a PQI-21 SLRB
  (LCP priority 1) and PFI 2 -> a PQI-90 SLRB (priority 25); the
  PQI-aware LCP (D21) fills the single 123B TB per 20 ms occasion in
  strict priority order across the destination's backlogged SLRBs.
  Measured (2s, seed 0): the PQI-21 flow keeps a 6.9 ms mean delay while
  the best-effort flow queues at ~338 ms under the deliberately tight
  pool - strict priority visible end-to-end in per-flow delay. 168 of the
  190 offered packets arrive within the run; the remainder is the
  best-effort flow's backlog, still queued at finishTime by design.

- Unicast-OTA (milestone M8, WP-L / SL-2): the unicast pair with the
  over-the-air PC5-RRC handshake (D23) instead of the genie. The first
  CBR packet parks the link in ESTABLISHING and is held by SlRrc; the
  proposed SLRBs travel as a real SlLinkEstablishRequest over the
  reserved TM SL-SRB (DRB 63; the SRB itself bootstraps via the genie
  mechanism - documented simplification of the pre-provisioned SRB0-3),
  the peer adopts them and answers, and the held packet resumes.
  Measured (2s, seed 0): the first packet arrives with an establishment
  transient of a few tens of milliseconds (vs a few ms steady state);
  delivery matches genie (95/95 both ways), while per-packet
  steady-state delay shifts by a constant
  grant-train phase offset (the handshake traffic perturbs the mode-2
  resource selection - inherent, since SPS keeps the selected train).

See the sidelink implementation plan for the WP-G roadmap (PRR/PIR-vs-
distance statistics, highway calibration scenario, scalability check).
