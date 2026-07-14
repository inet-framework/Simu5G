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

  PER-vs-distance validation sweep (shadowing off, grantMcs=12 i.e.
  CQI 12 for the BLER lookup, 26 dBm, 10-PRB subchannel, mu=1):

     dist [m]   50   400  500  600  700  800  900  1400
     received   95   95   95   58   6    3    1    0     (of 96)

  Loss onset ~600 m matches the expected SINR = 77.6 - 20log10(d) dB
  (~22 dB at 600 m) against the per-CQI-12 BLER curve with the framework's
  per-PRB success-probability convention ((1-BLER)^numPRBs, as on Uu).
  At the default grantMcs=6 the range is correspondingly longer.
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

  Measured sensing gain (5s, seed 0, ~122500 possible receptions):

                     received   PRR     SCI lost  TB lost  half-duplex lost
     Mode2-50UE      120322     ~98.2%  2757      1598     128
     Random-50UE     95176      ~77.7%  15369     9736     678

  With SPS, a random pick that collides keeps colliding for the lifetime of
  its reselection counter; sensing avoids reserved resources, leaving mostly
  cold-start and simultaneous-reselection collisions.

- Broadcast-Tr37885-BlindRetx (WP-F): one blind HARQ retransmission per TB
  (preconfig blindRetx: 1); the copy rides the next occasion of the same
  grant train, receivers soft-combine (harqReduction convention) and
  suppress duplicate deliveries by (source, HARQ process, NDI).

  Measured at the PER knee (--dist=600, grantMcs=12, shadowing off, 2s,
  48 packets): blindRetx 0 -> 16 delivered; blindRetx 1 -> 48 delivered
  (26 duplicate copies suppressed). App-level counts amplify TB losses via
  RLC UM reordering, so the retx gain is end-to-end.

See the sidelink implementation plan for the WP-G roadmap (PRR/PIR-vs-
distance statistics, highway calibration scenario, scalability check).
