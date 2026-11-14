The RLC Layer
=============

Overview
--------

The Radio Link Control protocol (RLC; 3GPP TS 38.322 for NR, TS 36.322 for
LTE) sits above MAC and has one entity per logical channel: one per radio
bearer, and one per leg of a split bearer. Its main functions are the transfer
of upper-layer PDUs in one of three modes (transparent, unacknowledged,
acknowledged), sequence numbering, segmentation and reassembly, error
correction through ARQ, duplicate detection, and RLC SDU discard. LTE RLC also
concatenates SDUs and delivers them in order.
Transparent Mode (TM) passes packets through without a header, and carries
only certain control channels; Unacknowledged Mode (UM) adds a sequence number
and segments the SDUs to fit the transport block that the MAC offers at each
transmission opportunity; Acknowledged Mode (AM) adds retransmissions. In AM,
the receiver reports the PDUs it is missing in status reports, which the
transmitter can ask for by polling, and the transmitter retransmits them,
re-segmenting them to fit the current transmission opportunity. AM trades
delay for reliability: it recovers the losses that HARQ leaves, at the cost of
a status report round trip. An AM transmitter that reaches the maximum number
of retransmissions of a PDU declares radio link failure. NR RLC delivers each
SDU as soon as it is complete, out of order; restoring the order is left to
PDCP.

Simu5G models all three modes, UM and AM in an LTE and an NR flavor each:
segmentation and reassembly, with concatenation in LTE and byte-offset
segments in NR; reordering with t-Reordering in LTE and reassembly windows
with t-Reassembly in NR; duplicate detection; and in AM the ARQ loop of status
reports, polling, retransmission with re-segmentation, and radio link failure
after ``maxRtxThreshold`` retransmissions. RLC SDU discard is not modeled. TM
is available as an entity type, but data radio bearers use UM or AM only.

This chapter describes how Simu5G creates the RLC entities of a bearer and
chooses their mode, how a transmission opportunity offered by the MAC is filled,
and how the two modes used by data radio bearers, UM and AM, work. The last
sections show the common configuration tasks, the statistics, and the example
simulations that validate the model.

Entities and modes
------------------

RLC is organized per bearer. When the RRC establishes a data radio bearer, its
:ned:`BearerManagement` submodule creates one RLC entity for each leg of the
bearer, next to the static submodules of the NIC, and deletes it when the bearer
is released. An entity is a compound of a transmitting and a receiving side,
and serves one bearer towards one peer: at a base station, there is an entity
per UE and bearer; at a UE, one per bearer. A bearer with traffic in one
direction only leaves the other side of its entity idle. Below the entities,
the :ned:`RlcMux` module dispatches the packets between the entities and the
MAC.

The mode of a bearer comes from its definition (see :doc:`qos`). A definition
either states ``rlcMode``, ``"UM"`` or ``"AM"``, or leaves it to be derived
from its QoS profile: a bearer whose packet error rate target is at most the
:ned:`BearerConfigurator`'s ``amPerThreshold`` (``1e-4`` by default) gets AM,
because HARQ alone cannot meet such a target; any other bearer gets UM. TM is
rejected for data radio bearers: in 3GPP it carries only broadcast, paging and
similar control channels.

The entity's type follows from the mode and from the radio technology of the
leg. The LTE and NR protocols differ (sequence number spaces, segmentation,
reordering versus reassembly), so each mode has an LTE and an NR type:

UM
   :ned:`LteRlcUmEntity` on an LTE leg, :ned:`NrRlcUmEntity` on an NR leg.

AM
   :ned:`LteRlcAmEntity` on an LTE leg, :ned:`NrRlcAmEntity` on an NR leg.

TM
   :ned:`RlcTmEntity`, the same for both technologies.

The types are parameters of :ned:`BearerManagement`
(``lteRlcUmEntityModuleType``, ``nrRlcAmEntityModuleType`` and so on), so a
custom RLC implementation is plugged in by naming its compound there. The same
parameters keep an NR leg on LTE RLC, by pointing the NR types back to the LTE
compounds.

Filling a transmission opportunity
----------------------------------

RLC does not decide when it transmits; the MAC does. When an SDU arrives from
PDCP, the transmitting entity queues it and tells the MAC how much new data the
bearer has. The MAC's scheduler takes this into account in its buffer status,
and when it grants the bearer resources, the MAC asks the entity for a PDU of a
given size, the share of the transport block that the bearer gets. The entity
then builds a PDU that fits exactly, header included, from what it has queued.
A grant the entity cannot use, because its queue is empty, is recorded as
wasted granted bytes.

How the PDU is built differs between the technologies:

*  LTE RLC concatenates: one PDU can carry several SDUs, and the first and the
   last of them may be segments. The Framing Info field of the header says
   whether the PDU starts and ends on an SDU boundary. Each PDU has a sequence
   number.

*  NR RLC does not concatenate: a PDU carries one SDU, or one segment of it.
   The sequence number belongs to the SDU, and a segment carries its Segment
   Information and, after the first, its byte offset within the SDU (the
   Segment Offset). Several RLC PDUs then share the transport block as separate
   MAC SDUs.

In AM, the entity serves the request in a fixed order: first a pending status
report of its receiving side, then retransmissions, then new data.

Unacknowledged Mode
-------------------

UM transfers SDUs without recovering losses. The transmitter segments the SDUs
to fit the transmission opportunities; the receiver reassembles them, and drops
what it cannot complete.

On the receiving side, LTE UM keeps a reordering window of PDU sequence
numbers. A missing PDU is waited for until a timer, the t-Reordering of TS
36.322, expires; then the window moves on, and the SDUs that cannot be
completed are discarded. The SDUs are delivered to PDCP in order. NR UM
reassembles each SDU from its segments, and delivers it as soon as it is
complete, out of order: the order is restored by PDCP. An SDU whose segments do
not all arrive before the t-Reassembly timer expires is discarded. NR UM also
discards duplicate SDUs, such as the copies that a HARQ retransmission can
produce.

On the transmitting side, the LTE UM queue is limited (``queueSize``, 2 MiB by
default): an SDU that does not fit is dropped. The NR UM queue is unbounded.

Acknowledged Mode
-----------------

AM adds ARQ to the transfer: the losses that HARQ leaves are recovered by
retransmitting RLC PDUs.

The receiving side keeps a window of sequence numbers, and reports what it has
received and what it is missing in status reports, which travel on the same
logical channel as the data, in the opposite direction. A status report is sent
when the transmitter asks for one, or when a gap is not filled before the
receiver's reordering or reassembly timer expires (t-Reordering in LTE,
t-Reassembly in NR), and not more often than the t-StatusProhibit timer allows
(100 ms by default).

The transmitter asks for a status report by polling: it sets the poll bit in a
PDU after every ``pollPDU`` PDUs or ``pollByte`` bytes (64 PDUs and 125 kB by
default), when its buffer runs empty, and when its window is full. If no
status report arrives, it polls again when the t-PollRetransmit timer expires
(80 ms by default). The missing PDUs that a status report names are
retransmitted, and a retransmitted PDU that no longer fits the transmission
opportunity is re-segmented.

The transmitter can have at most ``AM_Window_Size`` SDUs (NR) or PDUs (LTE)
outstanding without acknowledgement: 512 in LTE, 2048 in NR, which corresponds
to 12-bit sequence numbers (131072 selects 18-bit sequence numbers). When the
window is full, new data waits until acknowledgements move the window on.

A PDU that has been retransmitted ``maxRtxThreshold`` times (4 by default)
makes the transmitter declare radio link failure. The RRC then releases the
link at both ends, deleting the MAC, RLC and PDCP state of its bearers, and
further traffic for the released peer is dropped. If RRC re-establishment is
configured, the UE tries to re-establish its connection after the timers T311
and T301; otherwise the link stays released. Radio link failure and
re-establishment are described in :doc:`handover`.

A radio link failure is not declared just because the peer went silent while
new data keeps arriving: on each t-PollRetransmit expiry, the transmitter
considers its newest SDU for retransmission (TS 38.322 5.3.3), whose
retransmission counter starts at zero. The
counter of a single PDU climbs to ``maxRtxThreshold`` only after the window has
stalled, so a small window makes the failure come sooner after a blackout.

Configuration
-------------

Carry a bearer over AM, or over UM, by stating its mode in its definition:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [{coreNetwork: "5gc", isDefault: true,
       suppressSdapHeader: true, lcg: 3, rlcMode: "AM"}]

Let the QoS profiles decide, with a stricter or a looser threshold for AM:

.. code:: ini

   *.bearerConfigurator.amPerThreshold = 1e-3

Make AM tolerate more retransmissions before it declares radio link failure,
for example on a deliberately lossy link:

.. code:: ini

   **.maxRtxThreshold = 32

Enable RRC re-establishment after a radio link failure (it is off by default,
``t311 = 0s``):

.. code:: ini

   **.bearerManagement.t311 = 1s
   **.bearerManagement.t301 = 0.1s

The AM timers and thresholds (``pollPDU``, ``pollByte``, ``t_PollRetransmit``,
``t_StatusProhibit``, ``t_Reassembly`` or ``t_Reordering``) and the windows
are parameters of the transmitting and receiving entity types, and are set the
same way, with a ``**.`` pattern or a full path to one node's entities. Their
complete list is in the NED Reference of the entity types.

Statistics
----------

The receiving side of every entity records the delay and the throughput of its
bearer, per direction:

*  ``rlcDelayDl`` and ``rlcDelayUl``: the delay of an SDU, from its arrival at
   the transmitting entity to its delivery to PDCP, reassembled;

*  ``rlcThroughputDl`` and ``rlcThroughputUl``: the bytes of the delivered SDUs,
   divided by the time since the end of the warm-up period;

*  ``rlcPduDelayDl``/``Ul`` and ``rlcPduThroughputDl``/``Ul``: the same per
   PDU, as received from the air interface, before reassembly and including
   duplicates.

The :ned:`RlcMux` of each NIC records the same signals as vectors. The AM
entities also record the occupation of their windows, the window-full events,
the number of retransmitted PDUs, and the wasted granted bytes; the NR UM
transmitter records the requested and the sent PDU sizes. The AM receiver
records ``rlcPacketLossDl``/``Ul``, which is always 0, because AM delivers
every SDU it is given (unless the link fails).

Example simulations
-------------------

The configurations in ``simulations/nr/rlc`` validate the NR RLC. They replace
propagation with a fixed error rate per direction (an ideal channel model and
the :ned:`ConfiguredRateErrorModel`), with independent HARQ attempts, so that
the loss rate RLC sees is an exact, configured number, and they use constant
bit rate traffic, so that the offered load is deterministic. Each
configuration states its expected outcome:

``AM-Clean``
   An error-free reference: AM adds nothing when there is nothing to recover.

``AM-Lossy``, ``AM-Lossy-UL``
   A lossy downlink, or uplink, with a clean opposite direction for the status
   reports: every offered SDU is still delivered.

``UM-Lossy``
   The same lossy downlink over UM: no retransmissions, and an SDU loss rate at
   the application that matches the residual loss of HARQ.

``AM-Segmentation``
   SDUs much larger than the grants: segmentation and re-segmentation, with
   every SDU delivered intact.

``AM-WindowStall``
   A small window on a lossy link: the transmitter stalls on its window and
   recovers.

``AM-RLF``, ``AM-RLF-Reestablish``
   The downlink fails completely: one radio link failure, a clean teardown at
   both ends, and, with re-establishment, the flow resuming when the link comes
   back.

Fidelity
--------

*  RLC SDU discard, triggered by PDCP's discard timer in 3GPP, is not modeled:
   an SDU stays queued until it is transmitted. The NR UM and the AM queues are
   unbounded, so under overload the queueing delay grows without limit; only
   the LTE UM queue drops SDUs, when it is full.

*  TM entities exist, but data radio bearers use UM or AM only, as in 3GPP.

*  The status reports and the polling of AM run as in TS 38.322 and TS 36.322,
   on the bearer's own logical channel, so they compete with the data for the
   grants and can be lost on the radio link like any PDU.

*  A radio link failure detected by RLC releases the link at both ends at
   once; the peer does not have to detect the failure on its own.
