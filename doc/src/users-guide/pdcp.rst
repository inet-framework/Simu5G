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

Simu5G models four PDCP functions: the PDCP header, whose size follows the
bearer's RLC mode; header compression, as a size model of ROHC that follows
the states of the compressor; reordering with in-order delivery, in the NR
receiving entity; and the split of a dual connectivity bearer over two legs.
Ciphering and integrity protection, packet duplication, the discard timer and
PDCP status reports are not modeled.

This chapter describes how the PDCP entities come into being, what the
transmitting and the receiving side do with a packet, how header compression
is modeled, and how a split bearer is routed and joined. How a bearer is
defined is described in :doc:`qos`, and how the legs of a dual connectivity
bearer are chosen in :doc:`dual-connectivity`.

PDCP entities
-------------

A NIC has no PDCP entities of its own type: the RRC creates them at run time.
When a data radio bearer is established, the bearer management of the RRC
(:ned:`BearerManagement`) creates the bearer's PDCP entity in the NIC, next to
its static submodules, and connects it to the bearer's RLC entity or
entities; when the bearer is released, the entity is deleted. An entity is
named after the peer node and the bearer, for example ``pdcp-1025-1`` at a
base station for DRB 1 of the UE with node id 1025.

The entity is a compound module (:ned:`PdcpEntityBase`) with a transmitting
side, ``tx``, and a receiving side, ``rx``. Its type is chosen per node by the
``pdcpEntityModuleType`` parameter of the RRC's bearer management:
:ned:`LtePdcpEntity` (TS 36.323) is the default of LTE NICs, and
:ned:`NrPdcpEntity` (TS 38.323) that of NR NICs. A bearer with more than one
leg also has a leg splitter on its transmitting side and a leg joiner on its
receiving side (see `Split bearers`_ below).

Above the entities, the NIC's ``pdcpMux`` (:ned:`PdcpMux`) dispatches each
packet that comes from SDAP, or from ``ip2nic`` in a stack without SDAP, to the
entity of its bearer, and passes the packets that the entities deliver
upwards. It is a pure dispatcher: which bearer a packet belongs to has been
decided above it (see :doc:`qos`).

The transmitting side
---------------------

The transmitting side (:ned:`LtePdcpTxEntity`, or :ned:`NrPdcpTxEntity` in an
NR entity) does three things with each packet, in this order:

*  It compresses the headers, if the bearer has ROHC (see `Header
   compression`_ below).

*  It numbers the packet with the next sequence number of the bearer.

*  It adds the PDCP header, whose size follows the bearer's RLC mode: 1 byte
   on an RLC UM bearer, 2 bytes on an RLC AM bearer, none on an RLC TM bearer.

The resulting PDCP PDU goes to the bearer's RLC entity, or, on a bearer with
several legs, to the leg splitter.

The receiving side
------------------

The receiving side (:ned:`LtePdcpRxEntity`, or :ned:`NrPdcpRxEntity` in an NR
entity) removes the PDCP header, restores the headers that ROHC compressed,
and delivers the packet upwards.

The NR receiving entity reorders: it delivers the packets in sequence number
order, buffering a packet that arrives ahead of a missing one, and when the
gap is not filled within the reordering timer (t-Reordering, the ``timeout``
parameter of the receiving side, 100 ms by default), it skips the missing
packets and delivers the buffered ones behind them, up to the next gap. This
is needed on every NR bearer, because NR RLC passes complete SDUs up out of
order, and on a split bearer it also restores the order across the two legs.
With ``outOfOrderDelivery = true``, the entity delivers every packet as it
arrives instead. The LTE receiving entity does not reorder: LTE RLC already
delivers in order.

.. TODO: this describes reordering on every NR bearer, which assumes the fix
   of PdcpEntityBase.ned (on dev, `*.reorderingEnabled = this.numLegs > 1`
   switches it off for single-leg bearers, a known deviation from TS 38.323
   5.2.2.2). Check that the fix has landed before the guide is published.

Header compression
------------------

Header compression is configured per bearer, as TS 38.331 configures it in
the bearer's PDCP-Config: the ``rohc`` field of the bearer's definition in the
:ned:`BearerConfigurator` turns it on, either with every modeled profile
(``rohc: true``) or with a chosen set of profiles
(``rohc: {profiles: ["udp", "ip"]}``). A definition that does not state
``rohc`` follows the operator policy of the configurator's
``rohcForDrbProfiles`` parameter, which by default turns ROHC on for the voice
bearers (the ``5qi-1`` and ``qci-1`` profiles). ROHC compresses IP headers, so
it applies to the bearers of IP sessions only. See :doc:`qos`.

Simu5G models ROHC (RFC 3095) as a size model of its unidirectional mode: it
follows the compressor's states and replaces the compressed headers with a
ROHC header of the corresponding size, while the receiver puts the original
headers back. In more detail:

*  Each packet is compressed with the most specific of the bearer's profiles
   that fits it: ``"rtp"`` when an RTP header follows the UDP header, then
   ``"udp"``, ``"tcp"``, and finally ``"ip"``, which covers the IP header only
   and leaves the transport header as it is. A fragment can use ``"ip"`` only,
   and a packet that no profile fits goes uncompressed.

*  Each flow has a context, identified by a context id (CID). A bearer has up
   to 16 contexts; CID 0 costs nothing on the air, CIDs 1 to 15 one byte per
   packet, and a new flow that finds all of them taken reuses the least
   recently used one.

*  A context starts in the IR state, whose packets carry the full headers
   plus a few bytes of ROHC fields. After three IR packets it moves to the FO
   state, and after three FO packets to the SO state, where the headers shrink
   to their smallest size (for example 3 bytes for IP/UDP/RTP). The context is
   refreshed periodically: back to IR every 1700 packets, and back to FO every
   700 SO packets.

The compressed header sizes, the number of packets per state and the refresh
intervals are parameters of the transmitting side (see :ned:`LtePdcpTxEntity`).

Split bearers
-------------

In dual connectivity, a bearer can have a leg in each cell group: the master
cell group (MCG) and the secondary cell group (SCG). In Simu5G every such
bearer is terminated at the master node: its PDCP entity lives at the master,
and the secondary node only relays its PDCP PDUs. At the UE, the bearer's
PDCP entity has both legs locally, one on the RLC of each stack.

On the transmitting side, the entity's leg splitter (:ned:`DcPdcpLegSplitter`)
chooses the leg of each PDU. At the UE, in the uplink, the bearer stays on its
primary path until the data waiting for transmission reaches the split
threshold (ul-DataSplitThreshold), and above it either leg may be used; at the
master, in the downlink, a per-PDU leg selection expression decides, or the
bearer stays on its primary leg. The policy comes from the bearer's
definition (see :doc:`dual-connectivity`).

A PDU that takes the SCG leg at the master leaves the entity through the
NIC's ``pdcpDcMux`` (:ned:`DcMux`), which hands it to the dual connectivity
X2 forwarder, ``dcX2Forwarder``, to be tunneled to the secondary node over X2.
At the secondary, a PDCP relay entity (:ned:`PdcpRelayEntity`) takes the place
of the PDCP entity: it passes the PDUs from the master to its local RLC, and
the uplink PDUs from its RLC back to the master, without PDCP processing of
its own.

On the receiving side, the entity's leg joiner (:ned:`PdcpLegJoiner`) merges
the streams of the legs into the receiving side, whose reordering restores
the sequence number order across the legs. The sequence numbers are common to
both legs, since one transmitting entity numbers all PDUs.

Configuration
-------------

**Turn on header compression for a bearer.** State ``rohc`` in the bearer's
definition. This example, the ``VoIP-DL-Rohc`` configuration of
``simulations/nr/standalone``, compresses the IP and UDP headers of a voice
call on the UE's default bearer:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [{coreNetwork: "5gc", isDefault: true,
       suppressSdapHeader: true, lcg: 3, rlcMode: "UM", rohc: true}]

**Restrict the profiles.** To compress only the IP header, for example:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [{coreNetwork: "5gc", isDefault: true,
       rlcMode: "UM", rohc: {profiles: ["ip"]}}]

**Change the ROHC refresh intervals.** The PDCP entities are created at run
time, but they take their parameters from the ini file like any module; their
names start with ``pdcp-``:

.. code:: ini

   **.cellularNic.pdcp-*.tx.rohcIrRefresh = 500
   **.cellularNic.pdcp-*.tx.rohcFoRefresh = 200

**Shorten the reordering timer:**

.. code:: ini

   **.cellularNic.pdcp-*.rx.timeout = 50ms

**Use NR PDCP at an LTE master node.** In EN-DC, the master eNB terminates
bearers that have an NR leg, so it needs NR PDCP entities:

.. code:: ini

   *.masterEnb.cellularNic.rrc.bearerManagement.pdcpEntityModuleType = \
           "simu5g.stack.pdcp.NrPdcpEntity"

Statistics
----------

PDCP records no statistics of its own. Its entities emit signals for every
PDU they send and every SDU they deliver; at nodes with ``hasRniSupport``,
the packet flow observer of the NIC listens to them and computes the layer 2
measurements (packet delay, data volume, discard and loss rates) that the MEC
Radio Network Information service reports (see :doc:`mec` and
:doc:`results`). The traffic that enters and leaves PDCP is visible in the
statistics of RLC and of the applications.

Example simulations
-------------------

*  ``simulations/nr/standalone``: the ``VoIP-DL-Rohc`` and
   ``VoIP-DL-IPv6-Rohc`` configurations, voice with header compression over
   IPv4 and IPv6;

*  ``simulations/lte/test_handover``: ``VoIP-DL-StaticDrb``, where the voice
   bearer gets ROHC from the ``rohcForDrbProfiles`` policy and keeps it across
   handovers;

*  ``simulations/nr/dualConnectivity``: split bearers, among them the
   ``DualConn-SplitBearer-DL`` and ``DualConn-SplitBearer-UL`` configurations,
   which alternate the PDUs between the legs.

Fidelity
--------

*  Ciphering and integrity protection are not modeled: a PDU carries no
   integrity check (MAC-I), and no time is spent on security.

*  Packet duplication, the discard timer and PDCP status reports are not
   modeled. No packet is discarded for having waited too long for
   transmission.

*  ROHC is a size model. The compressed bytes are not produced; the header
   sizes per state are configured, not computed from the header fields. Only
   the unidirectional mode (U-mode) is modeled, without feedback, so context
   damage after losses and the decompression failures it causes do not occur.
   The bidirectional modes, ROHCv2 and Ethernet header compression (EHC) are
   not modeled. Of the PDCP-Config fields, maxCID (a bearer has up to 16
   contexts), drb-ContinueROHC and uplinkOnlyROHC are not modeled: ROHC works
   in both directions when it is on.

*  At a handover, the source base station releases the UE's PDCP entities,
   together with its RLC and MAC queues, and the target creates new ones. The
   sequence numbers start over, both ROHC ends restart in the IR state, and
   the PDUs still queued at the source are lost; only the packets that reach
   the source during the handover are forwarded to the target (see
   :doc:`handover`). PDCP status reports and the retransmission of
   unacknowledged PDUs at the target, which make a 3GPP handover lossless for
   RLC AM bearers, are not modeled.

*  Every split bearer is terminated at the master node; SN-terminated
   bearers are not modeled.
