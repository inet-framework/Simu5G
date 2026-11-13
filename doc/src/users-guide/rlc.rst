The RLC Layer
=============

Overview
--------

The Radio Link Control protocol (RLC; 3GPP TS 38.322 for NR, TS 36.322 for
LTE) sits above MAC and has one entity per logical channel: one per radio
bearer, and one per leg of a split bearer. Its main functions are the transfer
of upper-layer PDUs in one of three modes, sequence numbering, segmentation
and reassembly, error correction through ARQ, duplicate detection, and RLC SDU
discard. LTE RLC also concatenates SDUs and delivers them in order.
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

Like PDCP, RLC is organized per bearer: each DRB has, on each of its legs, an
entity in the mode configured for it, a compound of a transmitting and a
receiving entity. The mode comes from the bearer definition (its ``rlcMode``
field, possibly derived from the QoS profile -- see :doc:`qos`); entities are
created by the bearer management machinery. A multiplexer module
(:ned:`RlcMux`) dispatches PDUs between the entities and the MAC layer.

Entity types
------------

The LTE and NR flavors of the protocol differ (sequence number spaces,
reassembly vs. reordering procedures, segmentation rules), so each mode has
technology-specific entity types:

======================  ==========================  =========================
Mode                    LTE                         NR
======================  ==========================  =========================
Transparent             ``RlcTmEntity``             (same)
Unacknowledged          ``LteRlcUmEntity``          ``NrRlcUmEntity``
Acknowledged            ``LteRlcAmEntity``          ``NrRlcAmEntity``
======================  ==========================  =========================

Each is a compound module wrapping the corresponding TX and RX entity
(e.g. ``NrRlcAmTxEntity`` / ``NrRlcAmRxEntity``). The concrete types used
by a node are selected via parameters of the ``BearerManagement`` module
(``lteRlcUmEntityModuleType``, ``nrRlcAmEntityModuleType``, etc.), with
defaults set by the NIC; replacing them is the extension point for custom
RLC behavior.

UM parameters
-------------

The transmit side buffers SDUs up to ``queueSize`` (default 2 MiB). On the
receive side, LTE UM uses a reordering window (``rxWindowSize``, plus a
``timeout`` for missing PDUs), while NR UM follows the NR procedure with a
reassembly timer (``t_Reassembly``) and window (``UM_Window_Size``). The NR
UM sequence number length is configurable (``sn_FieldLength``, 6 or 12
bits).

AM parameters
-------------

The AM entities expose the standard protocol knobs, with 3GPP-conforming
defaults:

*  ``AM_Window_Size`` -- 512 for LTE (10-bit sequence numbers), 2048 for NR
   (12-bit; 131072 with 18-bit sequence numbers);
*  polling: ``pollPDU``, ``pollByte``, ``t_PollRetransmit``;
*  status reporting: ``t_StatusProhibit``;
*  loss detection: ``t_Reordering`` (LTE) / ``t_Reassembly`` (NR);
*  ``maxRtxThreshold`` -- the retransmission limit; reaching it signals
   *radio link failure* to the bearer management, which reacts according to
   its RRC re-establishment configuration.

Statistics
----------

The RLC entities record per-bearer throughput and delay statistics
(``rlcThroughputDl``/``Ul``, ``rlcDelayDl``/``Ul``, and D2D variants); see
:doc:`results`.
