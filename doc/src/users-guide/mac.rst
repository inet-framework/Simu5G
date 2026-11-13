The MAC Layer and Scheduling
============================

Overview
--------

The MAC layer is where radio resources are allocated: the base station's MAC
runs the downlink and uplink schedulers, grants resources to UEs, and
operates HARQ; the UE's MAC requests resources (random access, buffer status
reports) and assembles MAC PDUs according to received grants. Adaptive
Modulation and Coding (AMC) -- the choice of transport format based on
channel quality feedback -- also lives here, as a submodule of the base
station MAC.

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
