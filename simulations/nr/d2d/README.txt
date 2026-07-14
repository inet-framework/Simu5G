One-to-one device-to-device (D2D) communications (NR)
=====================================================

This example is the 5G NR counterpart of simulations/lte/d2d. Two UEs served by
the same gNodeB (standalone deployment) exchange traffic either over the
traditional two-hop infrastructure path (UE -> gNodeB -> UE) or directly over a
D2D link (UE -> UE), under the control of the gNodeB. D2D in Simu5G is a
research prototype and is not based on any specific 3GPP specification.

Modeling details are described in:
  A. Virdis, G. Nardini, G. Stea, "Modeling unicast device-to-device
  communications with SimuLTE", IWSLS2 2016, Vienna, July 1st, 2016.

Network
-------
All configurations use
simu5g.simulations.nr.networks.SingleCell_Standalone_D2D: one gNodeB plus
vectors of transmitter/receiver UEs (ueD2DTx[]/ueD2DRx[]), placed far from the
gNodeB but close to each other, so that the direct link is much better than the
cellular one.

Configurations (omnetpp.ini)
----------------------------
- SinglePair-UDP-Infra / SinglePair-TCP-Infra: one UE pair communicating
  through the gNodeB (infrastructure mode); CBR over UDP, or a TCP bulk
  transfer.
- SinglePair-UDP-D2D / SinglePair-TCP-D2D: the same pair communicating over the
  direct D2D link.
- MultiplePairs-UDP-{Infra,D2D} / MultiplePairs-TCP-{Infra,D2D}: N pairs
  (5, 20, 50) instead of one.
- MultiplePairs-{UDP,TCP}-D2D-wReuse: as above, with frequency reuse among D2D
  pairs enabled (see below).
- SinglePair-modeSwitching-UDP / SinglePair-modeSwitching-TCP: one pair moving
  back and forth so the link quality changes over time; the gNodeB periodically
  re-selects the best communication mode, causing dynamic switching between D2D
  and infrastructure mode.

Enabling D2D
------------
Unlike the plain NR NICs, this example needs the D2D-capable NIC variants, so
D2D capability is switched on for the whole network in the [General] section:

    **.ue*[*].hasD2D = true
    *.gnb.hasD2D = true

This selects the D2D-capable NIC variants (NrNicEnbD2D / NrNicUeD2D). The
Infra configs then simply leave their flows in infrastructure mode, while the
D2D configs move them to the direct link with the knobs below:

  **.amcMode = "D2D"
      Use the D2D AMC pilot, which selects transmission parameters for the
      direct links. The default, "AUTO", is cellular-only.

  *.ueD2D*[*].cellularNic.d2dInitialMode = true
      Start D2D-capable flows in direct (D2D) mode instead of infrastructure
      mode.

  CQI for the D2D links, either reported per link or fixed:
      *.gnb.cellularNic.phy.enableD2DCqiReporting = true   # per-link CQI feedback
      **.usePreconfiguredTxParams = false                  # or use a fixed CQI...
      **.d2dCqi = 7                                         # ...this one
      When usePreconfiguredTxParams is true, the fixed d2dCqi is used and CQI
      reporting is redundant.

  Dynamic mode selection (mode-switching configs):
      *.gnb.cellularNic.rrc.d2dModeSelection.typename = "D2DModeSelectionBestCqi"
      The D2D-capable gNodeB NIC already enables the periodic mode-selection
      tick (rrc.hasD2DModeSelection = true); this line plugs in the policy that
      keeps each flow on whichever mode currently has the better CQI. Extend
      D2dModeSelectionBase to implement your own policy.

  Frequency reuse (wReuse configs):
      *.gnb.cellularNic.mac.schedulingDisciplineUl = "ALLOCATOR_BESTFIT"
      *.gnb.cellularNic.mac.reuseD2D = true
      *.gnb.cellularNic.mac.conflictGraphUpdatePeriod = 1s
      *.gnb.cellularNic.mac.conflictGraphThreshold = 90    # dBm
      Lets mutually non-interfering D2D pairs reuse the same resource blocks,
      based on a periodically recomputed conflict graph.

  Separate D2D transmit power (optional):
      *.ueD2D*[*].cellularNic.phy.d2dTxPower = 20dBm       # ueTxPower is used for UL
