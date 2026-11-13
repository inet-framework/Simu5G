The Base Station
================

Overview
--------

A base station serves the UEs of its cell over the radio interface and
connects them to the core network. Simu5G models the LTE base station, the eNB
(:ned:`eNodeB`), and the NR base station, the gNB (:ned:`gNodeB`). The two share
almost everything: :ned:`gNodeB` extends :ned:`eNodeB`, and differs from it
in its cellular NIC, which carries the NR protocol stack, and in the name of
its default core network gateway. This chapter describes both.

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
routing.

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

Other base station parameters
-----------------------------

``macNodeId``, ``macCellId``
   The node id of the base station and the id of its cell, assigned
   automatically (see :doc:`architecture`).

``hasIpv4``, ``hasIpv6``
   The network layers of the core network side, which carry the tunnels: the
   transport network. It is IPv4 by default, and can stay IPv4 when the UEs'
   sessions are IPv6.

``numApps``
   Applications running on the base station itself, rarely needed.

``mecHost``, ``hasRniSupport``, ``extMeAppsAddress``, ``mcc``, ``mnc``
   MEC support: the MEC host the base station steers traffic to, the
   statistics collector of the Radio Network Information service, and the
   identity of the network the service reports. See :doc:`mec`.

The complete list is in the NED Reference of :ned:`eNodeB` and
:ned:`gNodeB`.

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
