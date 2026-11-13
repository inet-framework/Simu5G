The MAC Layer and Scheduling
============================

Overview
--------

The Medium Access Control protocol (MAC; 3GPP TS 38.321 for NR, TS 36.321 for
LTE) sits above the physical layer and has one entity per cell group: a base
station has one for its cell, and a dual connectivity UE one for each leg. Its
main functions are the mapping between logical channels and transport channels,
the multiplexing of MAC SDUs from several logical channels into one transport
block, scheduling, error correction through HARQ, logical channel
prioritization, scheduling information reporting, and random access. For
multiplexing, the MAC PDU carries a subheader for each SDU and for each control
element, such as a buffer status report. Scheduling is the base station's: in
every slot, it decides which UEs get which resource blocks of each carrier, in
the downlink and the uplink, and with which modulation and coding scheme, chosen
from the channel quality the UEs report; the decisions reach the UEs as grants
on the downlink control channel (PDCCH). For the uplink, a UE reports how much
data it has waiting in buffer status reports, per logical channel group, and
asks for the resources to send them with a scheduling request on the uplink
control channel (PUCCH), or by random access when it has no such resource.
Within a grant, the UE divides the bytes among its logical channels by logical
channel prioritization, which serves the channels in priority order, each up to
its prioritized bit rate first. HARQ retransmits a transport block that the
receiver could not decode, and the receiver combines the copies (soft
combining); several HARQ processes run in parallel, so that the link keeps
transmitting while it waits for feedback. The MAC also reports the UE's power
headroom, and handles discontinuous reception (DRX) and timing advance.

Simu5G models the MAC at the level of transport blocks and resource blocks.
The base station's schedulers allocate the resource blocks of each carrier in
every slot, with a choice of scheduling policies, and its adaptive modulation
and coding (AMC) submodule chooses the transport format from the UEs' CQI
reports. HARQ runs with a configurable number of processes; soft combining is
modeled in the receiver's error model, as a lower error rate for each
retransmission. A UE reports its buffer per logical channel group, and serves
its logical channels in the order of their groups. Random access is modeled as
preamble contention with backoff. Not modeled are the control channels (grants
and HARQ feedback are error-free and take no resources), scheduling requests
(a UE asks for uplink resources by random access only), the prioritized bit
rate of logical channel prioritization, power headroom reports, DRX and timing
advance. Instead of the subheaders, the MAC PDU has a header of a fixed 2
bytes, whatever the number of SDUs and control elements it carries.

Module types
------------

``LteMacBase`` implements the functionality common to all MAC modules
(SDU/PDU exchange with RLC and PHY, buffers, HARQ machinery); ``LteMacEnb``
and ``LteMacUe`` add the base-station and UE roles; ``NrMacGnb`` and
``NrMacUe`` specialize them for NR (different HARQ process count, NR AMC,
the NR uplink scheduler, NR timing). The right type is instantiated by the
NIC; users normally interact with the MAC through its parameters only.

Downlink scheduling
-------------------

The downlink scheduler allocates the resource blocks of each carrier among
the backlogged connections once per TTI. The scheduling policy is selected
with the ``schedulingDisciplineDl`` parameter of the base station MAC:

``"MAXCI"`` (default)
   Max C/I: grants the connection with the best channel condition first --
   maximizes cell throughput, ignores fairness.

``"PF"``
   Proportional Fair: scores connections by the ratio of instantaneous to
   average rate; the ``pfAlpha`` parameter controls the averaging memory.

``"DRR"``
   Deficit Round Robin: throughput-agnostic fair queuing among connections.

``"QOS_PF"``
   QoS-aware Proportional Fair: PF weighted by the per-bearer QoS profile
   (GBR flag, packet delay budget, packet error rate, priority level) taken
   from the bearer configuration (:doc:`qos`).

``"MAXCI_MB"``, ``"MAXCI_OPT_MB"``, ``"MAXCI_COMP"``
   Multiband variants of Max C/I; the ``_OPT_MB`` variant formulates the
   allocation as an LP problem solved by an external ``glpsol`` process, and
   the ``_COMP`` variant cooperates with the CoMP manager (see
   :doc:`x2`).

Writing a custom scheduling policy is a common Simu5G research task; the
policies above are C++ classes with a small common interface, and the
example set under ``src/simu5g/stack/mac/scheduling_modules/`` is the
template to follow.

Uplink scheduling
-----------------

Uplink allocation is grant-based, mirroring the standard: UEs signal their
demand through the Random Access procedure (``numPreambles``,
``maxRacAttempts``, ``raResponseWindow`` and backoff parameters on the UE
MAC) and Buffer Status Reports (retransmitted per ``retxBsrTimer``). The
base station's uplink scheduler (``schedulingDisciplineUl``, same policy set
as the downlink) grants resources per UE; within a grant, the UE's own
``LcgScheduler`` distributes the allocation across its *logical channel
groups*.

Each bearer belongs to one of four logical channel groups (LCGs), assigned
in its bearer definition or derived from its QoS priority (see
:doc:`qos`); buffer status is reported per LCG, so the LCG choice
determines which traffic a UE serves first when uplink resources are
scarce.

HARQ
----

Hybrid ARQ is modeled with the standard parallel stop-and-wait processes:

*  ``harqProcesses`` -- number of parallel processes (8 for LTE, 5 for NR);
*  ``maxHarqRtx`` -- maximum retransmissions before the PDU is dropped (and
   the loss, for AM bearers, is left to RLC to recover);
*  ``harqFbEvaluationTimer`` -- feedback timing in slots.

HARQ behavior is observable through a rich set of statistics
(per-transmission-attempt error rates, attempt counts); see
:doc:`results`.

AMC and CQI handling
--------------------

The base station MAC contains the AMC module (``amcType``: ``LteAmc`` /
``NrAmc``, or D2D variants), which turns CQI feedback into transport-format
decisions. Its main knobs, all parameters of the base station MAC:

*  ``pilotMode`` -- how the per-band CQI reports are summarized into the
   value AMC acts on (``"ROBUST_CQI"``, ``"MAX_CQI"``, ``"AVG_CQI"``,
   ``"MEDIAN_CQI"``, ``"IN_CQI"``);
*  ``amcMode`` -- ``"AUTO"`` for infrastructure traffic, ``"D2D"`` when
   D2D links are in use (:doc:`d2d`);
*  ``targetBler`` -- the block error rate the link adaptation aims for
   (also honored by the channel model when generating feedback).

How the CQI feedback itself is generated -- periodicity, wideband vs.
per-band -- is configured at the PHY layer (:doc:`phy`).
