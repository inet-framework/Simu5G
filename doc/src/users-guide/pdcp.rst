The PDCP Layer
==============

Overview
--------

The Packet Data Convergence Protocol (3GPP TS 36.323 for LTE, TS 38.323 for
NR) is the top of the radio protocol stack proper. In Simu5G it performs
sequence numbering, header handling, an (optional, simplified) header
compression, and -- for dual-connectivity bearers -- the splitting and
re-merging of the PDU stream over multiple legs.

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
