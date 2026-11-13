The Base Station
================

Overview
--------

In 3GPP, the base station is the node of the radio access network: the eNB in
LTE (TS 36.300), the gNB in NR (TS 38.300). It terminates the radio protocols
towards the UEs of its cells, and hosts radio resource management: the
scheduling of the radio resources, admission control, and the control of the
UEs' connections and mobility. It connects to the core network over S1 (eNB)
or NG (gNB), with a control-plane part towards the MME or the AMF and a
user-plane part, GTP-U tunnels, towards the Serving Gateway or the UPF. It
connects to neighboring base stations over X2 or Xn. A gNB can be split into a
central unit (CU) and distributed units (DU), connected over F1.

Simu5G models the eNB as :ned:`eNodeB` and the gNB as :ned:`gNodeB`, which
extends it, and differs from it in its cellular NIC, which carries the NR
protocol stack, and in the name of its default core network gateway. A base
station carries the full radio protocol stack with its schedulers, ends the
GTP-U tunnels of the user plane, and runs X2 links to its neighbors for
handover, dual connectivity and CoMP. Its control-plane interfaces are modeled
as direct calls, and an NR base station uses X2 in place of Xn. Admission
control and the split into CU and DU are not modeled. This chapter describes
both node types.

A base station combines three sides:

*  the radio side, the cellular NIC, which carries the protocol stack towards
   the UEs;

*  the core network side, where the UEs' traffic travels in GTP-U tunnels to
   and from the UPF or the P-GW;

*  the X2 side, the links to neighboring base stations, used by handover, dual
   connectivity and CoMP.

.. TODO: figure: the inside of a base station -- the cellular NIC, the relay
   and the tunnels, the X2 side -- drawn, not a NED screenshot.

The radio side
--------------

The ``cellularNic`` submodule carries the base station's side of the radio
protocol stack: :ned:`LteNicEnb` in an eNB, :ned:`NrNicEnb` in a gNB, or their
D2D-capable variants when ``hasD2D`` is true. The NIC is described in
:doc:`cellular-nic`; the parts that are specific to base stations are:

*  the MAC's schedulers, which allocate the radio resources of the cell to the
   UEs in downlink and uplink (see :doc:`mac`);

*  the carriers the base station serves, listed in the
   ``cellularNic.phy.componentCarrierModules`` parameter, the primary carrier
   first (see :doc:`spectrum`);

*  the transmission power, ``cellularNic.phy.eNodeBTxPower`` (46 dBm by
   default), and the antenna of the radio, ``cellularNic.radio.antenna``,
   which is omnidirectional by default and can be made sectorial with its
   ``txDirection`` and ``txAngle`` parameters (see :doc:`radio-medium`);

*  the X2 manager and the modules that use the X2 links (see below).

Two node-level submodules support the radio side. ``mobility`` gives the cell
its position; it is :ned:`StationaryMobility`, and the position is set with
``mobility.initialX`` and ``mobility.initialY``. ``cellInfo``
(:ned:`CellInfo`) holds per-cell settings, among them the interval of the
beacons the base station broadcasts for the UEs' cell selection and handover
(``beaconInterval``, 1 second by default).

.. code:: ini

   *.gnb.mobility.initialX = 300m
   *.gnb.mobility.initialY = 300m
   *.gnb.cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0]"

The core network side
---------------------

A base station is connected to the core network through its ``ppp`` gate, on
an ordinary link to a UPF, a P-GW or a router of the core network. It has a
full INET network layer and transport layer on this side, but these carry only
the tunnels: the UEs' traffic does not pass through the base station's IP
routing. This transport network is IPv4 by default (``hasIpv4``), and it can
stay IPv4 when the UEs' sessions are IPv6.

The ``relay`` submodule (:ned:`UserPlaneRelay`) sits between the cellular NIC
and the tunnels. Uplink user data from the NIC is classified by the relay's
traffic flow filter, which picks the session's tunnel, and is encapsulated
into GTP-U by the relay's GTP-U entity, which sends it over UDP to the
gateway. Downlink user data that arrives in a tunnel is decapsulated and handed
to the NIC. The relay is described in :doc:`core-network`.

The tunnels are set up by the control plane: the base station's connection
control, ``cellularNic.rrc.connectionControl`` (:ned:`ConnectionControlEnb`),
is the node's control-plane entry point, which the core network's
:ned:`CoreControl` asks to set up a session's tunnels when a UE attaches, and
which allocates the tunnel endpoint ids of the node (see
:doc:`control-plane`).

The ``gateway`` parameter names the base station's core network gateway, the
node at the core end of its tunnels: ``"pgw"`` for an eNB, ``"upf"`` for a gNB
by default. It is a module path relative to the network, so a network whose
gateway node has a different name, or sits inside a compound module, has to
set it:

.. code:: ini

   *.gnb*.gateway = "upf1"

The X2 side
-----------

Every X2 link of a base station is a connection on its ``x2`` gate vector, and
each gets a PPP interface (``x2ppp[]``). On top of the X2 links, the base
station runs:

*  the X2 applications (``x2App[]``, :ned:`LteX2App`), one per peer, which
   carry the X2 messages of CoMP and dual connectivity over SCTP; their number
   is ``numX2Apps``, and each must be told the address of its peer;

*  ``gtpUserX2`` (:ned:`GtpUserX2`), the GTP-U entity of the X2 user plane,
   which forwards user data to a peer: the downlink of a UE in handover, and
   the data of a dual connectivity bearer;

*  inside the NIC, the X2 manager (:ned:`LteX2Manager`), which dispatches X2
   messages to the modules that use them: the handover forwarder, the dual
   connectivity forwarder and the CoMP manager.

The configuration of the X2 links and applications is shown in
:doc:`deployments`, and the X2 stack is described in :doc:`x2`.

Dual connectivity
-----------------

A base station becomes the secondary node of another with its ``masterId``
parameter, which names the master by its node id; dual connectivity is
enabled with ``dualConnectivityEnabled`` in the NICs. A secondary node has no
core network link of its own: its traffic reaches the core through the master,
over X2. See :doc:`dual-connectivity`.

.. code:: ini

   *.secondaryGnb.masterId = 1
   **.dualConnectivityEnabled = true

MEC support
-----------

A base station takes part in a MEC scenario in two ways. It steers the traffic
of the MEC applications to the MEC host named in its ``mecHost`` parameter,
through a link on its ``pppMEHost`` gate. And with ``hasRniSupport = true``, it
gets the statistics collector from which the MEC platform's Radio Network
Information service reads the cell's measurements; the service identifies the
network by the base station's ``mcc`` and ``mnc``. See :doc:`mec`.

Statistics
----------

A base station records the statistics of its protocol layers: the
schedulers' resource usage and the per-UE throughput and delay of the MAC,
RLC and PDCP layers, and the SINR and CQI values of the PHY. They are
described in the chapters of the layers and in :doc:`results`.

Example simulations
-------------------

*  ``simulations/nr/standalone`` and ``simulations/nr/standalone_multicell``:
   gNBs in standalone networks, with one and two cells;

*  ``simulations/nr/dualConnectivity``: an eNB and a gNB as master and
   secondary node;

*  ``simulations/lte/``: eNBs in LTE networks.

Fidelity
--------

*  The base station is not split into a central unit (CU) and distributed
   units (DU); the F1 interface is not modeled.

*  The X2 and Xn interfaces are the same in the model: an NR base station
   uses the X2 stack.

*  The handover preparation between base stations (the Handover Request and
   its acknowledgement, the UE Context Release) is modeled as direct calls,
   not as X2AP or XnAP messages; only the forwarded user data crosses the X2
   link (see :doc:`handover`).
