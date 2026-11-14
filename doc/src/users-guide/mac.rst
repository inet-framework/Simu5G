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
advance.

This chapter describes the MAC of the base station and of the UE, the cycle
in which the base station schedules each slot, how downlink and uplink
resources are allocated, HARQ, link adaptation, carrier aggregation, and the
MAC PDU. The coordination of schedulers between cells (CoMP) is described in
:doc:`x2`.

Modules
-------

Every NIC has a MAC submodule per stack leg: ``mac`` for the LTE leg, and
``nrMac`` for the NR leg of a UE. Their types follow the node's role and
technology:

*  :ned:`LteMacEnb` at an eNB, :ned:`NrMacGnb` at a gNB;

*  :ned:`LteMacUe` on the LTE leg of a UE, :ned:`NrMacUe` on its NR leg.

All of them extend :ned:`LteMacBase`, which holds what both roles share: the
buffers, the HARQ entities and the statistics. A base station's MAC has three
submodules of its own: the downlink scheduler ``schedulerDl``
(:ned:`LteSchedulerEnbDl`), the uplink scheduler ``schedulerUl``
(:ned:`LteSchedulerEnbUl`, or :ned:`NrSchedulerGnbUl` at a gNB), and the
adaptive modulation and coding module ``amc`` (:ned:`LteAmc`, or :ned:`NrAmc`
at a gNB).

The MAC schedules *connections*: a connection is one logical channel of one
UE, so each bearer of a UE is a connection of its own in the downlink. In the
uplink, the base station sees a UE's backlog per logical channel group, so
there the connections are the UE's groups (see `Uplink scheduling`_).

The slot cycle
--------------

A base station's MAC runs in slots. It ticks at the slot length of the
highest numerology among the carriers it serves (1 ms for numerology 0, half
of that for numerology 1, and so on; see :doc:`spectrum`), and serves each
carrier at that carrier's own slot rate: a carrier of a lower numerology is
served every second, fourth, ... tick. An LTE base station has numerology 0
carriers only, so its slot is the 1 ms TTI.

At every tick, the MAC:

1. resolves the random access preambles received since the previous tick;

2. takes the uplink transport blocks that were received correctly out of the
   HARQ buffers, and passes their contents up to RLC;

3. runs the uplink scheduler, and sends the resulting grants to the UEs;

4. runs the downlink scheduler, and requests from RLC the data the scheduled
   connections are to send;

5. builds the downlink MAC PDUs from that data, and hands them to HARQ for
   transmission.

Within a carrier, a scheduler serves three kinds of demand in a fixed order:
first the grants of the UEs that have just won random access, then the HARQ
retransmissions, and only then new transmissions, with the resource blocks
that are left.

Downlink scheduling
-------------------

The downlink scheduler decides, for each carrier and each slot, which
connections are served and how many resource blocks each gets. It works from
two pieces of information:

*  **The backlog of each connection.** The MAC does not hold the downlink data
   itself until it is scheduled: the data waits in the RLC entities, and RLC
   notifies the MAC of every new SDU, which the MAC records in a *virtual
   buffer* per connection. A connection with a nonempty virtual buffer is
   *active*, a candidate for scheduling.

*  **The channel quality of each UE.** The AMC module turns the UE's CQI
   reports into transmission parameters: the bands the UE can use, and how
   many bytes one resource block carries for it (see `Link adaptation`_).

The scheduling policy orders the active connections, and the scheduler then
serves them in that order: each connection gets as many resource blocks as
its backlog needs, until the carrier's resource blocks run out. A connection
whose CQI is 0 is not scheduled, and neither is a UE that has no free HARQ
process.

After scheduling, the MAC asks each scheduled connection's RLC entity for
exactly as many bytes as the connection's resource blocks carry, minus the
subheaders of the MAC SDUs it will carry. RLC answers with a PDU of that size,
segmenting or concatenating SDUs as needed (see :doc:`rlc`), and the MAC builds
the MAC PDU from it.

The policy is chosen per direction with the base station MAC's
``schedulingDisciplineDl`` and ``schedulingDisciplineUl`` parameters. The same
policy serves every carrier of the base station. The policies are:

``MAXCI`` (the default)
   Maximum carrier-to-interference: connections are served in the order of how
   many bytes a resource block carries for them, so the UEs with the best
   channels are served first. It maximizes cell throughput and ignores
   fairness: a UE with a poor channel can starve.

``PF``
   Proportional fair: connections are ordered by the ratio of the bytes a
   resource block carries for them now to the rate they have received
   recently. The recent rate is an exponential average, in which
   ``pfAlpha`` (0.95 by default) is the weight of the latest slot. A UE whose
   channel is good relative to its own history is preferred, which balances
   throughput and fairness.

``QOS_PF``
   Proportional fair, with each connection's score multiplied by a weight
   derived from its bearer's QoS profile: higher for a higher priority (a
   lower priority level), for a GBR bearer, and for a tighter packet delay
   budget. A connection whose bearer has no profile is scheduled as in
   ``PF``. See :doc:`qos` for the profiles.

``DRR``
   Deficit round robin: the active connections take turns, each with an equal
   byte quantum per round, regardless of their channel quality.

``MAXCI_MB``
   Maximum carrier-to-interference that scores each connection with the CQI
   of each band separately (multiband CQI), instead of one CQI for the whole
   carrier.

``MAXCI_OPT_MB``
   Formulates the multiband allocation as an optimization problem, writes it
   to a file and solves it with an external CPLEX solver (the ``cplex``
   command must be installed). Meant for research on optimal allocation, not
   for general use.

``MAXCI_COMP``
   Maximum carrier-to-interference restricted to the bands the CoMP manager
   assigns to the cell (see :doc:`x2`).

``ALLOCATOR_BESTFIT``
   An uplink allocator for D2D frequency reuse, usable only with a
   D2D-capable uplink scheduler (see :doc:`d2d`).

Writing a scheduling policy of one's own is a common research use of Simu5G.
The policies are C++ classes derived from ``LteScheduler``, under
``src/simu5g/stack/mac/scheduling_modules/``; the existing ones are the
templates to follow.

Uplink scheduling
-----------------

In the uplink, the data waits at the UEs, so the base station has to learn
about it before it can schedule it:

1. **Random access.** A UE that has data and no grant sends a random access
   preamble, chosen at random from ``numPreambles`` preambles (64 by default).
   If two UEs choose the same preamble in the same slot, both attempts fail,
   and each UE retries after a random backoff, up to ``maxRacAttempts`` times.

2. **The first grant.** A UE that wins random access gets a grant from the
   base station's uplink scheduler, ahead of all other uplink traffic.

3. **The buffer status report.** In the transport block of that grant, the UE
   sends a buffer status report (BSR): how many bytes wait in each of its four
   logical channel groups (LCGs).

4. **Grants.** The base station records the reported backlog per UE and LCG,
   and its uplink scheduler grants resource blocks to those (UE, LCG) pairs
   as the downlink scheduler does to connections, with the policy of
   ``schedulingDisciplineUl``. Every grant reduces the recorded backlog, and
   a new report replaces it.

5. **Logical channel prioritization.** Within a grant, the UE divides the
   bytes among its logical channels: it serves the groups in the order of
   their index, LCG 0 first, and the backlogged channels of a group in turns.
   Which group a bearer belongs to comes from its definition, or is derived
   from its QoS priority (see :doc:`qos`).

A UE reports its buffer only after a won random access. While backlog
remains after the report, the UE waits for grants for ``retxBsrTimer`` slots
before it starts a new random access; data that arrives after the report
therefore needs a new random access before the base station learns about it.

The uplink HARQ processes run differently in the two technologies: an LTE
eNB's uplink scheduler cycles through a UE's processes synchronously, one per
TTI, while a gNB's NR uplink is asynchronous, and does not grant a UE whose
HARQ processes are all busy.

HARQ
----

Every transport block is sent in a HARQ process. A MAC has a HARQ entity per
peer and carrier, with ``harqProcesses`` parallel processes: 8 by default on
LTE, 5 on NR. The receiver evaluates a transport block and sends its HARQ
feedback after ``harqFbEvaluationTimer`` slots (4 by default). On a negative
acknowledgement the block is retransmitted, up to ``maxHarqRtx`` times (3 by
default, so up to four transmissions in all); after the last failed attempt,
the block is discarded, and its loss is left to RLC: an RLC AM bearer
recovers it by its own retransmission, a UM bearer loses it.

Retransmissions are scheduled like new data, but ahead of it (see `The slot
cycle`_). Soft combining is modeled in the receiver's error model: each
retransmission is received with a lower error probability than the attempt
before it (see :doc:`phy`).

Link adaptation
---------------

The base station's AMC module chooses, for every UE and direction, the
transport format: which modulation and coding scheme a resource block is sent
with, and thus how many bytes it carries. It works from channel quality
indicators (CQI). In the downlink, the UEs report them (see :doc:`phy`); in
the uplink, the base station computes them itself from the SINR of the UEs'
transmissions. The CQI maps to a transport block size through the tables of
the specifications (for NR, those of TS 38.214).

A CQI report has a value per band, and the AMC's *pilot* summarizes them into
the one value it uses, as the base station MAC's ``pilotMode`` parameter says:

``ROBUST_CQI`` (the default)
   The average of the bands' CQIs; only the bands whose CQI reaches the
   average are used for the UE.

``AVG_CQI``, ``MEDIAN_CQI``
   The mean or the median of the bands' CQIs, over all bands.

``MAX_CQI``
   The CQI of the best band.

A minimum mode exists in the code under the name ``MIN_CQI``, but the
parameter's declared values list it as ``IN_CQI``, a recorded defect.

The target block error rate that the CQI computation aims for is not a MAC
parameter: it belongs to the physical layer and the channel model (the
``targetBler`` parameters of :ned:`PhyEnb` and of the channel models; see
:doc:`phy`).

Carrier aggregation
-------------------

A base station that serves several carriers runs a scheduler instance per
carrier and per direction, each at its carrier's slot rate, and has a HARQ
entity per UE and carrier. A UE served on several carriers receives grants on
each of them, and its transport blocks on each carrier are acknowledged and
retransmitted independently. The carriers of a base station and of a UE are
listed in their PHY's ``componentCarrierModules`` parameter (see
:doc:`spectrum`).

The MAC PDU
-----------

A MAC PDU carries the MAC SDUs of one or more logical channels of one UE, and
MAC control elements such as a buffer status report. Each SDU carries the
identity of its logical channel, from which the receiving MAC finds the
bearer, and each SDU and control element has a subheader whose size follows
TS 38.321 6.1.2 in NR and TS 36.321 6.1.2 in LTE:

*  In NR, an SDU's subheader is 2 bytes, or 3 bytes for an SDU longer than
   255 bytes; a fixed-size control element, such as a Short BSR, has a 1-byte
   subheader, and a variable-size one, such as a Long BSR, a 2-byte one.

*  In LTE, an SDU's subheader is 2 bytes, or 3 bytes for an SDU longer than
   127 bytes, except the last subheader of the PDU, which is 1 byte; a control
   element has a 1-byte subheader.

A buffer status report is a Short BSR (1 byte) when one logical channel group
has data, and a Long BSR otherwise (in NR a bitmap byte plus one byte per
reported group, in LTE 3 bytes).

When the scheduler and the UE's logical channel prioritization size the data
they ask RLC for, they leave room for the full subheader of every SDU they
plan, and for a buffer status report that will go with the data. The PDU's
actual header can come out a byte or two shorter, for example for LTE's last
subheader; the rest of the transport block is padding.

.. TODO: this describes the fix planned in ai-plans
   plans/pending/simu5g-2026-11-14_mac-subheader-sizes-plan.md (on dev, every
   MAC PDU has a fixed 2-byte header, and control elements take no bytes; the
   BSR sizing follows the plan's decision D2). Check that the fix has landed,
   and that the decisions came out as written here, before the guide is
   published.

Configuration
-------------

**Choose the scheduling policy.** Set it per direction at the base station's
MAC. The ``Standalone`` configuration of ``simulations/nr/standalone_drb``
uses the QoS-aware scheduler in both directions:

.. code:: ini

   *.gnb.cellularNic.mac.schedulingDisciplineDl = "QOS_PF"
   *.gnb.cellularNic.mac.schedulingDisciplineUl = "QOS_PF"

To compare policies, make the policy an iteration variable, as
``simulations/lte/demo`` does:

.. code:: ini

   **.schedulingDisciplineDl = ${sched="PF","MAXCI"}

**Allow fewer HARQ retransmissions.** ``maxHarqRtx`` must be the same at both
ends of a link, so set it for all MACs at once. The ``nr/standalone``
configuration that exercises RLC AM allows a single retransmission, and raises
the target block error rate so that HARQ has losses to deal with:

.. code:: ini

   **.maxHarqRtx = 1
   **.targetBler = 0.3

**Change how the CQI reports are summarized.**

.. code:: ini

   **.pilotMode = "AVG_CQI"

**Write a scheduler of your own.** Derive a class from ``LteScheduler``, modeled
on one of the existing policies under
``src/simu5g/stack/mac/scheduling_modules/``. Then give it a name in the
``SchedDiscipline`` enumeration, create it in the scheduler factory
(``LteSchedulerEnb::getScheduler()``), and add the name to the values of the
``schedulingDisciplineDl`` and ``schedulingDisciplineUl`` parameters.

Statistics
----------

The MAC records, per UE and direction:

*  ``macDelayDl``, ``macDelayUl``: the time from a MAC PDU's creation to its
   correct reception, HARQ retransmissions included;

*  ``macThroughputDl``, ``macThroughputUl``: the rate of the correctly received
   MAC PDUs, averaged over time;

*  ``harqErrorRateDl``, ``harqErrorRateUl``: the share of transmissions that
   fail, and ``harqErrorRate_1st_Dl`` to ``harqErrorRate_4th_Dl`` (and their
   uplink counterparts) the same per attempt;

*  ``harqTxAttemptsDl``, ``harqTxAttemptsUl``: how many attempts a
   successfully delivered MAC PDU took;

*  ``macPacketLossDl``, ``macPacketLossUl``: the share of MAC PDUs discarded
   after the last HARQ attempt.

The base station records the cell's view: ``avgServedBlocksDl`` and
``avgServedBlocksUl``, the number of resource blocks allocated per slot, and
``macCellPacketLossDl`` and ``macCellPacketLossUl``, the MAC PDU loss over all
of its UEs. Buffer overflows of the MAC queues are recorded in
``macBufferOverFlowDl`` and ``macBufferOverFlowUl``. See :doc:`results` for
recording and analysis.

Example simulations
-------------------

*  ``simulations/nr/standalone_drb``: the ``QOS_PF`` scheduler with bearers of
   different QoS profiles;

*  ``simulations/lte/demo``: scheduler comparison (``PF`` versus ``MAXCI``);

*  ``simulations/nr/standalone``: HARQ with a single retransmission, under RLC
   AM;

*  ``simulations/lte/x2``: ``MAXCI_COMP`` with CoMP;

*  ``simulations/nr/test_numerology`` and ``simulations/nr/test_multiCarrier``:
   carriers of different numerologies, and carrier aggregation.

Fidelity
--------

*  **Control channels are not modeled.** Grants and HARQ feedback reach their
   destination without errors and without consuming radio resources; there is
   no PDCCH or PUCCH capacity limit, and no grant is ever missed.

*  **No scheduling requests.** A UE with uplink data and no grant always uses
   random access to ask for resources; the scheduling request on PUCCH is not
   modeled.

*  **Buffer status reports are sent only after a won random access.** The
   regular, periodic and padding BSR triggers of TS 38.321 5.4.5 are not
   modeled, and neither is the truncated BSR.

*  **Logical channel prioritization is strict priority by group.** The
   prioritized bit rate, which in TS 38.321 5.4.3.1 protects lower-priority
   channels from starvation, is not modeled, and the priority is that of the
   logical channel group, not of the individual channel.

*  **No power headroom reports, DRX or timing advance,** and no
   semi-persistent or configured-grant scheduling.

*  **One codeword.** The scheduler allocates a single codeword per UE; spatial
   multiplexing is not modeled (see :doc:`phy`).
