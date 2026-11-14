Device-to-device (D2D) communications
=====================================

Overview
--------

Device-to-device (D2D) communication lets nearby UEs exchange data directly,
over the sidelink (the PC5 interface), instead of through a base station. 3GPP
specifies it as Proximity Services (ProSe, TS 23.303) and as sidelink
communication in the radio access network (TS 36.300 for LTE, TS 38.300 for
NR). Its main functions are sidelink discovery, sidelink communication in
unicast, groupcast and broadcast, and the allocation of sidelink resources,
either scheduled by the base station or selected by the UEs themselves from
configured resource pools. For scheduled allocation, the base station keeps
control of the direct link: it assigns the radio resources of every sidelink
transmission. Sidelink communication also works out of network coverage,
where only UE-selected resources are possible.

Simu5G models network-assisted D2D, with a research prototype that is not based
on the 3GPP sidelink specifications: two UEs served by the same base station
exchange data over a direct link whose radio resources the base station
schedules, either one-to-one or one-to-many (multicast). The base station also
decides, statically or dynamically, whether a flow takes the direct path or the
conventional path through the base station. All of this happens below the IP
layer, in both the LTE and the NR stack, so applications are unaware of the
path in use. Sidelink discovery, UE-selected resources from resource pools, and
out-of-coverage operation are not modeled. D2D is a legacy feature of Simu5G:
it is kept working, but it gets no new features.

The modeling is described in A. Virdis, G. Nardini, G. Stea, "Modeling unicast
device-to-device communications with SimuLTE" (IWSLS2 2016) and in G. Nardini,
A. Virdis, G. Stea, "Modeling network-controlled device-to-device
communications in SimuLTE" (MDPI Sensors, 2018).

Modules
-------

The D2D modules live in the ``simu5g.stack.d2d`` package, separate from the
core LTE and NR stack:

*  the D2D-capable NICs :ned:`LteNicUeD2D`, :ned:`NrNicUeD2D`,
   :ned:`LteNicEnbD2D` and :ned:`NrNicEnbD2D`, which swap in the D2D variants
   of the IP-to-NIC adapter (:ned:`Ip2NicD2D`), the MAC (:ned:`LteMacUeD2D`,
   :ned:`LteMacEnbD2D` and their NR counterparts), the PHY (:ned:`PhyUeD2D`,
   :ned:`PhyEnbD2D`), the RLC UM entities and the RRC (:ned:`RrcD2D`);

*  in the base station's RRC, the mode selection module
   (:ned:`D2dModeSelectionBase` or a subtype such as
   :ned:`D2DModeSelectionBestCqi`), and in every D2D RRC the mode controller
   (:ned:`D2DModeController`);

*  :ned:`D2dBinder`, a network-level module next to the :ned:`Binder`, which
   holds the D2D state of the network: which UE pairs communicate directly,
   and the transmitters of one-to-many flows.

Enabling D2D
------------

D2D is turned on per node: ``hasD2D = true`` on a UE and on its serving base
station selects the D2D-capable NIC on those nodes (an explicit
``cellularNic.typename`` still takes precedence). The network must also
contain a :ned:`D2dBinder`, normally named ``d2dBinder``; the D2D reference
networks (:ned:`SingleCell_D2D`, :ned:`SingleCell_D2DMulticast`,
:ned:`SingleCell_Standalone_D2D`) have one.

.. code:: ini

   *.gnb.hasD2D = true
   **.ue*[*].hasD2D = true
   **.amcMode = "D2D"

The base station's AMC needs the D2D pilot (``amcMode = "D2D"``) to choose the
transmission format of the direct links; the default pilot handles the
cellular links only.

How D2D works
-------------

**The path of a flow.** When a UE sends an IP packet to another UE of the
cellular network, the D2D IP-to-NIC adapter checks whether the destination is
D2D-capable, and records the pair. If it is, the pair communicates in one of
two modes: in direct mode (DM) over the direct link, or in infrastructure mode
(IM) through the base station, as any other traffic. The initial mode is
direct if both UEs are served by the same cell and the sender's NIC has
``d2dInitialMode = true``; a pair served by different cells always uses
infrastructure mode.

**The direct link.** A D2D flow is carried on a sidelink bearer with a fixed
configuration, RLC UM on logical channel group 3, outside the bearer
definitions of :doc:`qos`. Its transmissions are scheduled by the base
station's uplink scheduler, in the uplink resources, like an uplink grant: the
sender reports its buffer, and the base station grants it the resource blocks
on which it transmits to its peer. The direct links are evaluated by the radio
medium's channel models, like every other link (see :doc:`radio-medium`).

**Channel quality.** The base station chooses the transmission format of a
direct link from the link's CQI, which the receiving UE reports when the base
station's PHY has ``enableD2DCqiReporting = true`` (the default), or from a
fixed CQI (``usePreconfiguredTxParams = true`` and ``d2dCqi``).

**One-to-many communication.** A UE that sends to an IPv4 multicast address
transmits to the group over the direct link: the packet becomes a one-to-many
(D2D_MULTI) transmission addressed to the group, and the UEs whose interface
is enrolled in the multicast group receive it. One-to-many links work with a
fixed CQI only. IPv6 multicast is not mapped to D2D; it goes up the session
like any other packet.

**Mode selection.** The base station's mode selection module runs
periodically (``modeSelectionPeriod``, 1 s by default) and switches flows
between direct and infrastructure mode. The default module,
:ned:`D2dModeSelectionBase`, makes no periodic switch;
:ned:`D2DModeSelectionBestCqi` puts each flow on the mode whose link has the
better CQI. Handovers switch flows too, with any module: before a UE hands
over, its D2D flows are switched to infrastructure mode, and when the handover
completes, every flow of the UE whose two ends are now served by the same cell
is switched to direct mode. At a switch, the base station's MAC parameters
``msClearRlcBuffer`` and ``msHarqInterrupt`` say whether the flow's queued RLC
SDUs are dropped and its HARQ processes interrupted.

**Frequency reuse.** With the best-fit uplink allocator, D2D pairs that do not
interfere with each other can share resource blocks: the base station keeps a
conflict graph of the D2D links, by received power or by distance, and
allocates the same blocks to pairs that are not in conflict.

Configuration
-------------

**Start D2D-capable flows in direct mode:**

.. code:: ini

   *.ueD2D*[*].cellularNic.d2dInitialMode = true

**Use a fixed CQI for the direct links** (required for one-to-many
communication):

.. code:: ini

   **.usePreconfiguredTxParams = true
   **.d2dCqi = 7

**Switch modes dynamically** by the better CQI:

.. code:: ini

   *.gnb.cellularNic.rrc.d2dModeSelection.typename = "D2DModeSelectionBestCqi"

**Reuse resource blocks among D2D pairs:**

.. code:: ini

   *.gnb.cellularNic.mac.schedulingDisciplineUl = "ALLOCATOR_BESTFIT"
   *.gnb.cellularNic.mac.reuseD2D = true
   *.gnb.cellularNic.mac.conflictGraphUpdatePeriod = 1s
   *.gnb.cellularNic.mac.conflictGraphThreshold = 90   # dBm

**Send to a group.** The sender's application sends to an IPv4 multicast
address, and the receivers enroll in that group in the IP configuration (see
``demo.xml`` of ``simulations/lte/d2d_multicast``):

.. code:: ini

   *.ueD2D[0].app[*].destAddress = "224.0.0.10"

Module organization and extension points
----------------------------------------

The core stack reaches the D2D code only through a small set of interfaces,
which are the natural extension points for custom D2D behavior: the C++
interfaces ``ID2dMacUe``, ``ID2dMacEnb`` and ``ID2dAmc``, and the NED module
interface :ned:`ID2DModeSelection`. To add a mode selection policy, subclass
:ned:`D2dModeSelectionBase`, which implements the periodic evaluation and the
switch notifications, override its ``doModeSelection()`` method, and select
the new type with ``cellularNic.rrc.d2dModeSelection.typename``.

Statistics
----------

The D2D links have their own variants of the layer statistics: the RLC and
MAC delay and throughput (``rlcDelayD2D``, ``rlcThroughputD2D``,
``macDelayD2D``, ``macThroughputD2D``), the HARQ error rate
(``harqErrorRateD2D``), the reported CQI (``averageCqiD2D``) and the SINR of
the direct receptions (``rcvdSinrD2D``). The mode switches are counted by
``rcvdD2DModeSwitchNotification``. The D2D multihop application records its
own dissemination statistics, such as the delivery ratio and the delay of
each event (see :doc:`apps`).

Example simulations
-------------------

*  ``simulations/lte/d2d`` and ``simulations/nr/d2d``: single and multiple
   D2D pairs, in direct and in infrastructure mode, with UDP and TCP traffic,
   with frequency reuse, and with dynamic mode switching
   (``SinglePair-modeSwitching``);

*  ``simulations/lte/d2d_multicast``: one-to-many communication;

*  ``simulations/lte/d2d_multihop``: the multihop dissemination of events
   with :ned:`MultihopD2D`.

Fidelity
--------

*  The model is a research prototype of network-assisted D2D, not an
   implementation of the 3GPP sidelink: there is no PC5 protocol stack, no
   sidelink control information, no discovery, no UE-selected resources or
   resource pools, and no out-of-coverage operation. Direct transmissions use
   the uplink resources, granted by the base station's uplink scheduler.

*  A pair uses direct mode only while both UEs are served by the same cell.

*  The sidelink bearers have a fixed configuration (RLC UM, logical channel
   group 3); RLC AM and the QoS model of :doc:`qos` do not apply to them.

*  When a UE has direct links to several peers, the base station chooses the
   transmission format from the feedback of the first peer it knows of.

*  One-to-many communication needs a fixed CQI and works over IPv4 only.

*  The default mode selection module switches flows only at handovers. After
   a handover, a pair served by the same cell is put in direct mode even if
   its flows started in infrastructure mode (``d2dInitialMode = false``).

*  Known deviation: the path loss of a UE-to-UE link uses base-station
   conventions (the breakpoint distance of TR 38.901 UMi computed with the
   base station's height, and the cable loss subtracted), while TR 36.843
   A.2.1.2 and TR 37.885 define the D2D path loss with both ends at UE height
   (``tests/doc/model-deviations.md`` item 4).
