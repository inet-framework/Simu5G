The RLC Layer
=============

Overview
--------

The Radio Link Control layer (3GPP TS 36.322 for LTE, TS 38.322 for NR)
provides segmentation and, depending on the mode, in-sequence delivery and
retransmission on top of the MAC layer. Simu5G implements all three RLC
modes:

*  *Transparent Mode* (TM) -- pass-through, no headers, no buffers beyond a
   simple queue;
*  *Unacknowledged Mode* (UM) -- segmentation/reassembly and sequence
   numbering, no retransmissions;
*  *Acknowledged Mode* (AM) -- UM functionality plus ARQ: status reporting,
   polling and retransmissions.

Like PDCP, RLC is organized per bearer: each DRB owns a pair of TX/RX
entities in the mode configured for it. The mode comes from the bearer
definition (its ``rlcMode`` field, possibly derived from the QoS profile --
see :doc:`qos`); entities are created by the bearer management
machinery. A multiplexer module (``RlcMux``) dispatches PDUs between the
entities and the MAC layer.

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
