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
     Mode2-50UE      117560     ~95.9%
     Random-50UE     84252      ~68.7%

  With SPS, a random pick that collides keeps colliding for the lifetime of
  its reselection counter; sensing avoids reserved resources, leaving mostly
  cold-start and simultaneous-reselection collisions.

- Broadcast-Tr37885-BlindRetx (WP-F): one blind HARQ retransmission per TB
  (preconfig blindRetx: 1); the copy rides the next occasion of the same
  grant train, receivers soft-combine (harqReduction convention) and
  suppress duplicate deliveries by (source, HARQ process, NDI).

  Measured deep in the PER knee (initialX=3090m i.e. 2800 m, grantMcs=12,
  shadowing off, 2s): blindRetx 0 -> 10 of 96 delivered; blindRetx 1 -> 27
  delivered of the 48 transmittable (the copy halves the grant-train
  capacity), a ~2.7x PRR gain from soft combining at equal resource use per
  packet. App-level counts amplify TB losses via RLC UM reordering, so the
  retx gain is end-to-end.

See the sidelink implementation plan for the WP-G roadmap (PRR/PIR-vs-
distance statistics, highway calibration scenario, scalability check).
