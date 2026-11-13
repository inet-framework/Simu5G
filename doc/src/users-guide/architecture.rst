Architecture Overview
=====================

Overview
--------

Simu5G models a mobile network after the 3GPP system architecture: its nodes
are the 3GPP network entities, its interfaces are the 3GPP reference points,
and the protocol stack inside its nodes is the 3GPP protocol stack. A reader
who knows the architecture of the 5G system (3GPP TS 23.501) or of the EPS
(TS 23.401) already knows the map of Simu5G; this chapter shows where each
part of that map is in the model, and how faithfully it is modeled.

The chapter first lists the 3GPP entities and the Simu5G modules that model
them. It then follows the user plane and the control plane through the model,
introduces the network-level modules that every Simu5G network contains, and
explains the node identifiers that configurations refer to. The rest of this
part describes the nodes one by one.

.. TODO: figure: the 3GPP architecture (UE, gNB, UPF, AMF/SMF, DN) with the
   Simu5G module types written next to the entities. Drawn, not a NED
   screenshot.

The 3GPP entities in Simu5G
---------------------------

User Equipment (UE)
   :ned:`LteUe` is an LTE terminal, and :ned:`NrUe` a terminal with both an
   LTE and an NR protocol stack, which can attach to an eNB, a gNB, or both
   at once (dual connectivity). :ned:`LteCar` and :ned:`NrCar` are their
   vehicular variants. See :doc:`ue`.

Base stations: eNB and gNB
   :ned:`eNodeB` and :ned:`gNodeB`. A gNB is an eNB with an NR protocol stack;
   both serve the UEs of their cell over the radio interface and connect to
   the core network through GTP-U tunnels. See :doc:`base-station`.

User Plane Function (UPF) and PDN Gateway (P-GW)
   :ned:`Upf` is the user plane function of a 5G core, :ned:`PgwStandard` the
   gateway of an EPC. Both end the GTP-U tunnels of the UEs' sessions and
   connect the mobile network to the data network. See :doc:`core-network`.

AMF and SMF, or MME and the gateways' control plane
   :ned:`CoreControl`, a single network-level module that takes the UEs'
   registrations, establishes their sessions, and programs the user plane
   nodes and the base stations. See :doc:`core-network` and
   :doc:`control-plane`.

Data network (DN)
   Ordinary INET nodes: routers, servers and hosts, connected to the anchor
   UPF's N6 interface (the SGi interface of a P-GW).

The radio interface (Uu)
   :ned:`CellularRadioMedium`, the network-level radio medium that carries
   every transmission between base stations and UEs, and computes what each
   receiver sees of it. See :doc:`radio-medium`.

The N3 and S1-U interfaces
   GTP-U tunnels over UDP/IP, on ordinary INET links between the base stations
   and the UPFs (or the P-GW). Intermediate routers and UPFs forward the
   tunneled packets as IP traffic.

The X2 and Xn interfaces
   Point-to-point links between base stations, with an X2 protocol stack on
   SCTP and a GTP-U entity for forwarding user data. Handover, dual
   connectivity and CoMP use them. See :doc:`x2`.

The user plane
--------------

The user plane carries the UEs' traffic. As an example, follow a downlink IP
datagram from a server in the data network to a UE in a 5G standalone
network:

1. The server sends the datagram to the UE's IP address. The data network
   routes it to the anchor UPF, the UPF that owns the UE's session.

2. In the anchor UPF, the datagram arrives on the N6 interface and goes to
   the node's user-plane relay (:ned:`UserPlaneRelay`). The relay's traffic
   flow filter (:ned:`TrafficFlowFilter`) identifies the session and the QoS
   flow the datagram belongs to, and the GTP-U entity (:ned:`GtpUser`) puts it
   into the session's tunnel towards the base station that serves the UE.

3. The tunneled packet crosses the core network as a UDP/IP packet.
   Intermediate UPFs and routers forward it like any other IP packet.

4. At the base station, the relay takes the datagram out of the tunnel and
   hands it to the cellular NIC directly: a base station does not IP-route
   the UEs' traffic. The NIC's protocol stack (SDAP, PDCP, RLC, MAC and PHY)
   carries it over the data radio bearer of its QoS flow and transmits it.

5. The radio medium delivers the transmission to the UE's radio, which
   decides whether the reception is successful. The UE's NIC passes the
   datagram up the stack to the UE's IP layer and the application.

Uplink traffic takes the same path in the opposite direction. The details are
in :doc:`core-network` (tunnels and relays), :doc:`cellular-nic` (the radio
stack), and :doc:`qos` (how traffic is classified onto QoS flows and bearers).

A session need not carry IP. In an Ethernet session, the tunnels carry
Ethernet frames between the UE and an Ethernet bridge at the anchor UPF; in an
Unstructured session, they carry opaque payload between an application at the
UE and a server in the data network. See :doc:`pdu-sessions` and
:doc:`ethernet-sessions`.

The control plane
-----------------

The control plane configures the user plane: it attaches UEs to cells,
establishes their sessions and bearers, and moves them between cells. Simu5G
models it through the same entities that 3GPP defines:

*  the RRC (:ned:`Rrc`) in every cellular NIC, whose connection control
   submodule is the control-plane entry point of a base station or a UE leg;

*  :ned:`CoreControl`, the control plane of the core network;

*  :ned:`UserPlaneNodeControl` in every UPF and P-GW, the node's end of the
   interface over which the core network programs the user plane (N4 with
   PFCP in a 5G core).

The procedures -- attach, session establishment, handover, radio link failure
and RRC re-establishment, dual connectivity setup -- run between these
entities, each step at the node that performs it in 3GPP. The signaling
messages themselves (RRC, NAS, S1AP or NGAP, X2AP or XnAP, PFCP) are not
simulated as packets: each message is a direct call, named after the message
it stands for. Where the duration of a procedure matters, as for a handover
or an RRC re-establishment, it is modeled with timers.

What the control plane establishes is described declaratively: the bearers,
their QoS profiles and the classification rules come from the configuration of
the :ned:`BearerConfigurator`, not from the outcome of a negotiation. The
control plane is described in :doc:`control-plane`.

Network-level modules
---------------------

Besides its nodes, every Simu5G network contains a few modules that exist once
per network:

``binder`` (:ned:`Binder`)
   The directory of the cellular network: which nodes exist, which UE is
   attached to which cell, the dual connectivity relations, the X2 peerings.
   The other modules look nodes up through it; it has almost no parameters of
   its own.

``coreControl`` (:ned:`CoreControl`)
   The control plane of the core network (see above).

``bearerConfigurator`` (:ned:`BearerConfigurator`)
   The configuration of the bearers, the QoS profiles and the QoS flow
   classification rules. See :doc:`qos`.

``carrierAggregation`` (:ned:`CarrierAggregation`)
   The component carriers of the network: their frequencies, bandwidths,
   numerologies and duplexing. See :doc:`spectrum`.

``radioMedium`` (:ned:`CellularRadioMedium`)
   The radio medium, with one channel model per carrier and radio access
   technology. See :doc:`radio-medium`.

``configurator`` (INET's :ned:`Ipv4NetworkConfigurator`)
   Assigns IP addresses and routes, as in any INET network. Networks that run
   IPv6 add an IPv6 configurator as well.

The modules inside the nodes reach the network-level modules through
module-path parameters (``binderModule``, ``coreControlModule``,
``bearerConfiguratorModule``, ``radioMediumModule`` and so on), whose defaults
are the names above. A network that names these modules differently has to set
the parameters accordingly. The same mechanism lets one simulation hold
several independent cellular networks, each with its own set of network-level
modules, provided every module refers to those of its own network.

Node identifiers
----------------

Every base station and every UE stack leg has a numeric node identifier, its
*MAC node id*. Configurations refer to nodes by these ids: a UE's
``servingNodeId`` and ``nrServingNodeId`` parameters name its serving cells,
and a secondary base station's ``masterId`` names its master node.

The ids are assigned from three sequences, in the order in which the network
creates the nodes, which is the order of their submodule declarations in the
network:

*  base stations, eNBs and gNBs together, from 1 (the ``macNodeId`` parameter
   of :ned:`eNodeB` and :ned:`gNodeB`);

*  the LTE stack legs of the UEs, from 1025 (the ``macNodeId`` parameter of
   :ned:`LteUe`);

*  the NR stack legs of the UEs, from 2049 (the ``nrMacNodeId`` parameter of
   :ned:`NrUe`).

For example, in a network that declares ``masterEnb`` before ``secondaryGnb``,
the eNB gets id 1 and the gNB id 2, so ``*.secondaryGnb.masterId = 1`` makes
the gNB the eNB's secondary node, and ``*.ue[0].nrServingNodeId = 2`` attaches
the UE's NR leg to the gNB. A base station's cell id (``macCellId``) is its
node id by default.

Relationship to INET
--------------------

Simu5G nodes are INET nodes, and every part of a Simu5G network that is not
cellular is plain INET:

*  a UE is an INET :ned:`StandardHost` with the cellular NIC as one more
   network interface, so it has INET's applications, transport protocols,
   network layers and mobility models;

*  the base stations are INET network nodes with their own composition: an
   INET network layer and transport layer for the core network and the X2
   links, and the user-plane relay between the cellular NIC and the tunnels;

*  :ned:`Upf` and :ned:`PgwStandard` extend INET's router-like
   :ned:`ApplicationLayerNodeBase`;

*  the data network, the links and the IP address configuration are INET's.

The radio medium follows INET's division of responsibilities: the medium
models everything up to the receiving antenna -- propagation, path loss,
shadowing, fading, interference -- and computes the SINR, while the node owns
its antenna and decides what to make of a reception.

Fidelity
--------

The architecture is modeled at the level of the 3GPP entities and their
interfaces; the following parts of it are abstracted or left out:

*  The control-plane signaling is not simulated as packets: the messages are
   direct calls, and their latency is not modeled except where a procedure
   has timers (see :doc:`control-plane`).

*  A UE has one PDU session (one PDN connection in an EPC).

*  The EPC has no separate S-GW: :ned:`PgwStandard` ends the tunnels from the
   eNBs. Intermediate UPFs do not end tunnels; they forward the tunneled
   packets as IP routers.

*  The physical layer is modeled at the level of transport blocks and resource
   blocks, not OFDM symbols (see :doc:`phy`).

The fidelity of each part of the model is detailed in its own chapter.
