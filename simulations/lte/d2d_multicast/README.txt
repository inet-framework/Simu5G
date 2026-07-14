One-to-many device-to-device (D2D) communications (LTE)
=======================================================

This example demonstrates network-assisted one-to-many (multicast) D2D
communications in an LTE network. A transmitting UE broadcasts periodic alert
messages directly to a group of nearby UEs, without relaying them through the
eNodeB. At the PHY layer the transmission is a single broadcast that receivers
accept or drop based on their IP multicast group membership; HARQ feedback is
suppressed for the 1:M link. D2D in Simu5G is a research prototype and is not
based on any specific 3GPP specification. See simulations/lte/d2d for the
one-to-one case and the underlying D2D mechanism.

Network
-------
All configurations use
simu5g.simulations.lte.networks.SingleCell_D2DMulticast: one eNodeB plus a
vector of D2D UEs (ueD2D[]); ueD2D[0] is the sender and the rest are receivers.

Applications and multicast group
--------------------------------
The sender runs an AlertSender application addressing the multicast group
(224.0.0.10); the receivers run AlertReceiver. UEs join the multicast group via
the IPv4 configurator in demo.xml, e.g.:

    <multicast-group hosts="ueD2D[*]" interfaces="cellular" address="224.0.0.10"/>

Configurations (omnetpp.ini)
----------------------------
- D2DMulticast-1to2: one transmitter and two nearby receivers.
- D2DMulticast-1toM: one transmitter and M receivers (50) randomly deployed in
  the cell; sweeps the fixed CQI value.
- checkMulticastRange: receivers dropped at increasing distance from the
  transmitter to exercise the PHY-level multicast range check.

Enabling D2D
------------
D2D capability is a per-node switch:

    *.eNB*.hasD2D = true
    *.ueD2D*[*].hasD2D = true

selects the D2D-capable NIC variants (LteNicEnbD2D / LteNicUeD2D). The relevant
knobs are:

  **.amcMode = "D2D"
      Use the D2D AMC pilot.

  **.usePreconfiguredTxParams = true
  **.d2dCqi = 7
      One-to-many transmissions use a fixed CQI only (there is no CQI feedback
      for a broadcast link), so usePreconfiguredTxParams must be true and d2dCqi
      selects the modulation/coding.

  PHY-level multicast range check (checkMulticastRange config):
      **.phy.enableMulticastD2DRangeCheck = true
      **.phy.multicastD2DRange = 1000m
      When enabled, the PHY delivers a multicast packet only to receivers within
      the given range of the transmitter.
