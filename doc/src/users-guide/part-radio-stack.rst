Radio Protocol Stack
====================

This part describes the layers of the radio protocol stack in the order in
which a downlink packet passes them: PDCP (3GPP TS 38.323 and TS 36.323), RLC
(TS 38.322 and TS 36.322), MAC with its schedulers (TS 38.321 and TS 36.321),
and the physical layer, which Simu5G models as an abstraction rather than at
the level of OFDM symbols (TS 38.214 for the procedures it follows). SDAP, the
layer above PDCP in NR, is described with the QoS model in Sessions and QoS.

How the layers are assembled into a NIC is described in The Cellular NIC.
Propagation, interference and the channel models are not part of the node's
stack but of the radio medium; they are described in the Radio part.

.. toctree::
   :maxdepth: 1

   pdcp
   rlc
   mac
   phy
