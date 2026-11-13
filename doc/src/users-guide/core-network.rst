The Core Network
================

Overview
--------

The core network connects the radio access network to the data networks. In a
5G core (3GPP TS 23.501), User Plane Functions (UPFs) forward the users'
traffic: the PDU Session Anchor connects a session to a data network over N6,
intermediate UPFs may relay it over N9, and the traffic between the radio
access network and the UPFs is carried in GTP-U tunnels over N3. The control
plane is a set of network functions with service-based interfaces: the AMF
handles registration and mobility, the SMF establishes the PDU sessions and
programs the UPFs over N4 with PFCP, and further functions handle policy (PCF)
and subscriber data (UDM). In an EPC (TS 23.401), the Serving Gateway and the
PDN Gateway carry the user plane, and the MME handles the control plane.

Simu5G models the user plane of the core network node by node, with UPFs and a
PDN Gateway that end the GTP-U tunnels of the sessions. The control plane is a
single network-level module that acts as the AMF and the SMF (or as the MME
and the gateways' control plane), and its signaling with the base stations and
the user plane nodes is modeled as direct calls. The Serving Gateway,
intermediate UPFs that end tunnels, the service-based interfaces, and the PCF
and UDM are not modeled.

The building blocks are:

:ned:`Upf`
   The User Plane Function of a 5G core. A UPF connected to the data network
   is the *anchor* of the sessions that enter there (the PDU Session Anchor of
   TS 23.501); a UPF between the anchor and the base stations is an
   *intermediate* UPF.

:ned:`PgwStandard`
   The PDN Gateway of an EPC, the EPC's counterpart of the anchor UPF.

:ned:`CoreControl`
   The control plane of the core network, a network-level module: the AMF and
   the SMF of a 5G core, or the MME and the control plane of the gateways of an
   EPC.

The rest of the core network is ordinary INET: the links between the nodes,
and any routers placed between them.

.. TODO: figure: the user plane path -- UE, base station relay, intermediate
   UPF, anchor UPF relay, N6, data network -- with the tunnel drawn through
   it. Drawn, not a NED screenshot.

User plane nodes
----------------

:ned:`Upf` and :ned:`PgwStandard` extend INET's router-like
:ned:`ApplicationLayerNodeBase`, so they can have any number of wired
interfaces (``pppg[]``, ``ethg[]``), and they forward IP traffic like a router.
Their own network layer and transport layer form the *transport network* of
the core network, which carries the tunnels.

What makes them user plane nodes are three submodules:

``relay`` (:ned:`UserPlaneRelay`)
   The user-plane relay between the data network and the tunnels (see below).

``dnPpp``
   The N6 interface of a UPF, the SGi interface of a P-GW, on the ``dnPppg``
   gate, which connects the node to the data network. The interface belongs to
   the user plane, not to the node's own network layer: it is not in the
   node's interface table, the node's routing does not use it, and the IP
   configurator gives it no address.

``nodeControl`` (:ned:`UserPlaneNodeControl`)
   The node's control-plane entry point: the node's end of the N4 interface
   (PFCP, TS 29.244) over which the core network's control plane programs the
   user plane. It installs the sessions' tunnels and the downlink QoS flow
   classification rules into the relay.

Two optional submodules serve particular session types:

``ndResponder`` (:ned:`Ipv6NdResponder`, with ``hasNdResponder``)
   Answers the IPv6 Neighbor Discovery of the UEs' sessions. The anchor is the
   router on the IP link of an IPv6 session, so it has to answer the UEs'
   Router and Neighbor Solicitations; the base station is transparent to them.
   Needed whenever UEs run IPv6.

``ethBridge`` (:ned:`EthernetSessionBridge`, with ``hasEthernetBridge``)
   The Ethernet bridge of the Ethernet sessions, with its N6 Ethernet
   interface ``dnEth`` on the ``dnEthg`` gate. A UPF without it rejects
   Ethernet sessions. See :doc:`ethernet-sessions`.

A UPF is an anchor or an intermediate UPF by its position in the network,
not by a parameter: the anchor is the UPF whose ``dnPppg`` (or ``dnEthg``) gate
is connected to the data network, and which the base stations name as their
``gateway``. An intermediate UPF leaves these gates unconnected, and forwards
the tunneled packets as an IP router.

The user-plane relay
--------------------

Every node that ends GTP-U tunnels -- base stations, anchor UPFs, P-GWs, and
the UPF of a MEC host -- has a :ned:`UserPlaneRelay` submodule named
``relay``. It is the "relay" between two protocol stacks in the user-plane
diagrams of 3GPP: at a base station, between the radio interface and the
tunnels; at an anchor, between the data network and the tunnels. It has two
parts:

:ned:`TrafficFlowFilter`
   Classifies the user data that enters the tunnels. At an anchor, it finds
   the session a downlink packet belongs to and assigns it a QoS flow
   identifier (QFI), following the downlink classification rules the control
   plane installed; a packet that matches no rule goes on the default QoS
   flow (QFI 0). At a base station, it passes on the QFI that the UE's SDAP
   layer gave an uplink packet. The rules are authored in the
   ``dlQfiRules`` parameter of the :ned:`BearerConfigurator` (see
   :doc:`qos`).

:ned:`GtpUser`
   The GTP-U entity: it encapsulates the classified user data into the
   session's tunnel, over UDP to the tunnel's other end, and decapsulates what
   arrives in a tunnel. At a base station, the decapsulated data goes to the
   cellular NIC; at an anchor, onto the data network -- unless its destination
   is another UE of the same core network, in which case the anchor tunnels it
   on to that UE's base station.

The tunnels are identified by their tunnel endpoint ids (TEIDs), which each
node allocates from its own TEID space when the control plane sets a tunnel
up. A session has an uplink tunnel ending at its anchor (and one at the UPF of
each MEC host behind the anchor), and a downlink tunnel ending at the base
station where the session's downlink enters the radio access network.

The control plane of the core network
-------------------------------------

:ned:`CoreControl` stands for the control plane of the core network in one
module, placed at the network level as ``coreControl``. It:

*  takes the UEs' registrations, which the base stations announce when a UE
   attaches (the INITIAL UE MESSAGE of S1AP and NGAP);

*  establishes each UE's session, of the type the UE requests (see
   :doc:`pdu-sessions`), and chooses its anchor: the gateway of the base
   station where the UE's downlink enters the radio access network when the
   session is established;

*  programs the session's tunnels at the anchor, through its
   :ned:`UserPlaneNodeControl`, and at the base station, through its
   connection control (:ned:`ConnectionControlEnb`);

*  delivers the QoS flow classification rules of the
   :ned:`BearerConfigurator` to the places where they are evaluated: the
   downlink rules to the user plane nodes, the uplink rules to the UEs;

*  switches the downlink path of a session when its UE hands over to another
   base station (the path switch of TS 23.502 4.9.1.2.2), and has the anchor
   close the old path with an End Marker.

The anchor stays the same for the lifetime of the session, as in session and
service continuity mode 1: a handover moves only the downlink end of the
tunnel. The procedures are described in :doc:`control-plane`.

The signaling between these entities -- NGAP or S1AP towards the base
stations, PFCP or GTP-C towards the user plane nodes -- is modeled as direct
calls, named after the messages they stand for.

Configuration
-------------

The core network needs little configuration of its own. The base stations'
``gateway`` parameter names the anchor of their tunnels: ``"upf"`` for a gNB
and ``"pgw"`` for an eNB by default, as a module path relative to the network.
A network with several anchors, or with differently named ones, sets it per
base station:

.. code:: ini

   *.gnb1.gateway = "upf1"
   *.gnb2.gateway = "upf2"

The optional parts of an anchor are switched on per node:

.. code:: ini

   *.upf.hasNdResponder = true      # the UEs run IPv6
   *.upf.hasEthernetBridge = true   # the UPF anchors Ethernet sessions

An anchor of Unstructured sessions needs the address prefix of the sessions'
N6 addresses, and the server and ports of its N6 tunnels, all set on its
``nodeControl`` (see :ned:`UserPlaneNodeControl` and :doc:`pdu-sessions`).

The links of the core network are ordinary INET links, so the transport
network's capacity and delay are configured through the channel types that
connect the nodes (for example ``Eth10G``).

MEC hosts
---------

A MEC host (:ned:`MecHost`) contains a UPF of its own (``upf_mec``), which
ends tunnels like an anchor, and is placed behind the core network gateway
named by its ``gateway`` parameter. The base stations steer the traffic of
the MEC applications to it (their ``mecHost`` parameter). See :doc:`mec`.

Example simulations
-------------------

*  ``simulations/nr/standalone``: a 5G core with an anchor and an intermediate
   UPF;

*  ``simulations/lte/``: EPC networks with a P-GW;

*  ``simulations/nr/standalone_drb``: the ``Ipv6`` configurations run IPv6
   sessions with the anchor's ND responder;

*  ``simulations/nr/ethernet``: an anchor UPF with an Ethernet bridge;

*  ``simulations/nr/unstructured``: an anchor UPF with Unstructured sessions.

Fidelity
--------

*  The core network's control plane is one module, and its signaling is
   modeled as direct calls: NGAP, S1AP, PFCP and GTP-C messages take no time
   and use no transport network capacity.

*  The EPC has no separate S-GW: the P-GW ends the tunnels from the eNBs.

*  The intermediate UPF of TS 23.501 (I-UPF), which ends N3 and N9 tunnels,
   is not modeled: the intermediate UPFs of the reference networks forward
   the tunneled packets as IP routers. Session and service continuity modes
   2 and 3, in which the anchor changes, are not modeled either.

*  A UPF serves one data network.
