One-to-one device-to-device (D2D) communications (LTE)
======================================================

This example demonstrates network-assisted one-to-one D2D communications in an
LTE network. Two UEs served by the same eNodeB exchange traffic either over the
traditional two-hop infrastructure path (UE -> eNodeB -> UE) or directly over a
D2D link (UE -> UE), still under the control of the eNodeB. D2D in Simu5G is a
research prototype and is not based on any specific 3GPP specification.

Modeling details are described in:
  A. Virdis, G. Nardini, G. Stea, "Modeling unicast device-to-device
  communications with SimuLTE", IWSLS2 2016, Vienna, July 1st, 2016.

Network
-------
All configurations use simu5g.simulations.lte.networks.SingleCell_D2D: one
eNodeB plus vectors of transmitter/receiver UEs (ueD2DTx[]/ueD2DRx[]), placed
far from the eNodeB but close to each other, so that the direct link is much
better than the cellular one.

Configurations (omnetpp.ini)
----------------------------
- SinglePair-UDP-Infra / SinglePair-TCP-Infra: one UE pair communicating
  through the eNodeB (infrastructure mode); VoIP over UDP, or a TCP bulk
  transfer.
- SinglePair-UDP-D2D / SinglePair-TCP-D2D: the same pair communicating over the
  direct D2D link.
- MultiplePairs-UDP-{Infra,D2D} / MultiplePairs-TCP-{Infra,D2D}: N pairs
  (5, 20, 50) instead of one.
- MultiplePairs-{UDP,TCP}-D2D-wReuse: as above, with frequency reuse among D2D
  pairs enabled (see below).
- SinglePair-modeSwitching-UDP / SinglePair-modeSwitching-TCP: one pair moving
  back and forth so the link quality changes over time; the eNodeB periodically
  re-selects the best communication mode, causing dynamic switching between D2D
  and infrastructure mode.
- SinglePair-Validation: a parameter study that sweeps the receiver distance.

Enabling D2D
------------
D2D capability is a per-node switch. Setting

    *.eNB*.hasD2D = true
    *.ueD2D*[*].hasD2D = true

selects the D2D-capable NIC variants (LteNicEnbD2D / LteNicUeD2D) on those
nodes; the Infra-only configs set hasD2D = false. The relevant knobs are:

  **.amcMode = "D2D"
      Use the D2D AMC pilot, which selects transmission parameters for the
      direct links. The default, "AUTO", is cellular-only.

  *.ueD2D*[*].cellularNic.d2dInitialMode = true
      Start D2D-capable flows in direct (D2D) mode instead of infrastructure
      mode.

  CQI for the D2D links, either reported per link or fixed:
      *.eNB.cellularNic.phy.enableD2DCqiReporting = true   # per-link CQI feedback
      **.usePreconfiguredTxParams = false                  # or use a fixed CQI...
      **.d2dCqi = 7                                         # ...this one
      When usePreconfiguredTxParams is true, the fixed d2dCqi is used and CQI
      reporting is redundant.

  Dynamic mode selection (mode-switching configs):
      *.eNB.cellularNic.rrc.d2dModeSelection.typename = "D2DModeSelectionBestCqi"
      The D2D-capable eNB NIC already enables the periodic mode-selection tick
      (rrc.hasD2DModeSelection = true); this line plugs in the policy that keeps
      each flow on whichever mode currently has the better CQI. Extend
      D2dModeSelectionBase to implement your own policy.

  Frequency reuse (wReuse configs):
      *.eNB.cellularNic.mac.schedulingDisciplineUl = "ALLOCATOR_BESTFIT"
      *.eNB.cellularNic.mac.reuseD2D = true
      *.eNB.cellularNic.mac.conflictGraphUpdatePeriod = 1s
      *.eNB.cellularNic.mac.conflictGraphThreshold = 90    # dBm
      Lets mutually non-interfering D2D pairs reuse the same resource blocks,
      based on a periodically recomputed conflict graph.

  Separate D2D transmit power (optional):
      *.ueD2D*[*].cellularNic.phy.d2dTxPower = 20dBm       # ueTxPower is used for UL
