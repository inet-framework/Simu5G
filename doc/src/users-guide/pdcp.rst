The PDCP Layer
==============

Overview
--------

The Packet Data Convergence Protocol (PDCP; 3GPP TS 38.323 for NR, TS 36.323
for LTE) sits above RLC and has one entity per radio bearer. Its main
functions are header compression, ciphering and integrity protection,
reordering with in-order delivery, and the routing and joining of the packets
of a split bearer. For reordering, the transmitting entity numbers every
packet with a sequence number, and the receiving entity uses the numbers to
put the packets back in order and to discard duplicates. For a split bearer in
dual connectivity, the transmitting entity routes the packets over the two
legs, in two cell groups, and the receiving entity joins the two streams by
sequence number. Header compression (ROHC) matters for small packets: it
reduces the tens of bytes of IP, UDP and RTP headers of a voice frame to a few
bytes. PDCP also discards packets that waited too long for transmission, and
can duplicate packets over two legs for reliability.

Simu5G models the PDCP functions that change what is transmitted and when: the
PDCP header, whose size follows the bearer's RLC mode; header compression, as
a size model of ROHC that follows the states of the compressor; and the split
of a dual connectivity bearer over two legs, with reordering of the merged
stream at the receiver. Ciphering and integrity protection, packet
duplication, the discard timer and PDCP status reports are not modeled, and a
receiving NR entity with a single leg delivers packets in arrival order, a
known deviation from TS 38.323.

PDCP is organized per bearer: each established DRB gets its own pair of
transmitting and receiving PDCP entities, created and torn down by the
bearer management machinery in the RRC module (see :doc:`qos`). A
dispatcher module (``PdcpMux``) routes SDUs arriving from the upper layer
(SDAP, or the IP-to-NIC adapter where there is no SDAP) to the entity of the
right bearer.

Entities
--------

The per-bearer entity is a compound module, ``PdcpEntityBase``, packaging a
``tx`` and an ``rx`` entity and, for multi-leg bearers, the leg
splitter/joiner stage. Two concrete variants bind the technology-specific
entity types:

*  ``LtePdcpEntity`` -- with ``LtePdcpTxEntity`` / ``LtePdcpRxEntity``
   (TS 36.323 behavior);
*  ``NrPdcpEntity`` -- with ``NrPdcpTxEntity`` / ``NrPdcpRxEntity``
   (TS 38.323 behavior, adding receive-side reordering for multi-leg
   bearers).

Which variant a node uses is selected by the ``pdcpEntityModuleType``
parameter of the ``BearerManagement`` module in the RRC; the appropriate
default is set by the NIC type.

Parameters
----------

The user-relevant entity parameters are:

``headerCompressedSize``
   The size PDCP compresses upper-layer headers to; ``-1B`` (the default)
   disables header compression. This is a deliberately simple stand-in for
   ROHC: the model does not implement the ROHC state machines, it just
   accounts for the configured compressed size.

``rlcMode``
   The RLC mode of the bearer, pushed down by RRC from the bearer
   definition; it determines the PDCP header size and is not set by the
   user directly.

``NrPdcpRxEntity`` additionally has reordering-related parameters
(``timeout``, ``rxWindowSize``, ``outOfOrderDelivery``); reordering itself
is enabled by the entity's container for bearers that receive from more
than one leg.

Dual connectivity support
-------------------------

For split bearers, the PDCP entity at the master node feeds two legs: its
own RLC (MCG leg) and, over the X2 interface, the RLC of the secondary node
(SCG leg). The modules involved -- the ``DcPdcpLegSplitter`` on the transmit
side, the ``PdcpLegJoiner`` feeding the reordering receive entity, the
``DcMux``/``DcX2Forwarder`` pair that tunnels PDCP PDUs over X2, and the
``PdcpRelayEntity`` standing in for the bearer at the secondary node -- are
described from the user's perspective in :doc:`dual-connectivity`; the
user-facing configuration (leg selection policies, the uplink data split
threshold) lives in the bearer definitions of :doc:`qos`.
