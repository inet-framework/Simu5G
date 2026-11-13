PDU Sessions
============

Overview
--------

In the 5G system, a PDU session (3GPP TS 23.501 5.6, procedures in TS 23.502
4.3) is the association between a UE and a data network that carries the UE's
traffic. A UE can have several PDU sessions at once, to the same or to
different data networks. Its main properties are the session type, the
session anchor, and session and service continuity. The session type says
what the session carries: IPv4, IPv6 or IPv4v6 packets, Ethernet frames, or
Unstructured payload that the network does not interpret. For the anchor, the
SMF selects a UPF as the PDU Session Anchor, which connects the session to the
data network over N6 and allocates the UE's IP address or prefix; the traffic
between the anchor and the base station travels in GTP-U tunnels of the
session. For continuity, the session's SSC mode decides what happens when the
UE moves: in SSC mode 1 the anchor stays for the lifetime of the session and
only the tunnel towards the radio access network moves, while modes 2 and 3
let the network change the anchor. The UE requests a session with the PDU
Session Establishment procedure, and the session is released when the UE or
the network ends it. In LTE (TS 23.401), the counterpart is the PDN
connection, whose PDN type is IPv4, IPv6 or IPv4v6, anchored at the PDN
Gateway.

Simu5G models the PDU session of a UE end to end: its establishment by the
core network when the UE first attaches, its tunnels at the anchor and at the
base station, the move of its downlink tunnel at a handover, and its release.
All five session types are modeled. An EPC carries the IP types only, as in
3GPP; Ethernet and Unstructured sessions need a 5G core. A UE has one session,
whose type it states in its configuration; the anchor is chosen by the
network topology and stays for the lifetime of the session (SSC mode 1). The
signaling of session establishment and release is modeled as direct calls,
the UE's IP addresses are configured statically rather than allocated by the
anchor, and SSC modes 2 and 3 and more than one session per UE are not
modeled.

This chapter describes the session model and the IP and Unstructured session
types. Ethernet sessions have a chapter of their own, :doc:`ethernet-sessions`,
and how a session's traffic is classified into QoS flows and carried on data
radio bearers is described in :doc:`qos`.

Modules
-------

A session involves the following modules, each of which is described in the
chapter of its node:

*  the UE's ``sessionType`` parameter (:ned:`LteUe`), which states the type of
   the session the UE requests;

*  :ned:`CoreControl`, the core network's control plane, which establishes and
   releases the sessions;

*  the base station's connection control (:ned:`ConnectionControlEnb`), which
   announces a UE's attachment to the core network and sets up the session's
   tunnel at the base station;

*  the user plane node control (:ned:`UserPlaneNodeControl`) of the anchor
   UPF or P-GW, which sets up the session's tunnel at the anchor, and the N6
   end of an Unstructured session;

*  the relays (:ned:`UserPlaneRelay`) of the anchor and of the base station,
   whose GTP-U entities (:ned:`GtpUser`) carry the session's traffic;

*  for IPv6 sessions, the anchor's Neighbor Discovery responder
   (:ned:`Ipv6NdResponder`);

*  for Unstructured sessions, the UE's :ned:`UnstructuredApp`.

The life of a session
---------------------

**Establishment.** When a UE's first stack leg attaches to a base station --
at initialization, if the UE has a configured serving cell, or later, when a
detached leg finds a cell -- the base station registers the UE with the core
network, passing on the session type the UE requests. :ned:`CoreControl` then
establishes the UE's session:

1. It chooses the anchor: the core network gateway of the base station where
   the UE's downlink enters the radio access network, that is, the node the
   base station's ``gateway`` parameter names. In dual connectivity, this is
   the master node's gateway.

2. It checks that the anchor can carry the session type. A P-GW carries the IP
   types only, and an Unstructured or Ethernet session also needs a UE stack
   with SDAP.

3. It has the anchor's :ned:`UserPlaneNodeControl` set up the session's uplink
   tunnel, and the base station's connection control the downlink tunnel, each
   under a tunnel endpoint id the node allocates itself. With the downlink
   tunnel, the base station also installs the UE's uplink QoS rules and its
   static bearers (see :doc:`qos`).

If the UE's base station is not connected to a core network, the UE gets no
session. A UE that is detached at initialization gets its session when it
first attaches.

**Mobility.** The anchor stays the same for the lifetime of the session. When
the UE hands over to another base station, the target sets up the session's
downlink tunnel during the handover preparation, and :ned:`CoreControl` has
the anchor switch the session's downlink to it; the anchor closes the old path
with an End Marker. See :doc:`handover`. A UE leg that loses its cell keeps
the session: the core network only stops sending its downlink to the base
station that lost it.

**Release.** The session is released when the UE leaves the simulation: the
anchor and the base stations forget its tunnels, and an Unstructured
session's N6 address becomes free again.

Session types
-------------

The type of a UE's session is set with the UE's ``sessionType`` parameter. The
same UE type, :ned:`NrUe` or :ned:`LteUe`, serves every session type; what
changes with the type is the UE's protocol stack above the NIC:

``"IPv4"``, ``"IPv6"``, ``"IPv4v6"``
   IP sessions, carried by both an EPC and a 5G core. ``"IPv4"`` is the
   default. The UE's applications are INET applications over its IP stack.

``"Ethernet"``
   The session carries Ethernet frames, and the UE becomes an Ethernet node.
   See :doc:`ethernet-sessions`.

``"Unstructured"``
   The session carries opaque payload from an application attached to the
   NIC directly, with no IP stack in between. See `Unstructured sessions`_.

A UE can have only one session type, so all its traffic must fit it: a UE with
an IPv6 session runs IPv6 only, and a UE with an Unstructured session runs no
IP at all. One network can serve UEs of different session types side by side,
through the same anchor.

IP sessions
-----------

An IP session carries the UE's IP datagrams between the UE and the data
network. A session of type ``"IPv4"`` carries IPv4 only, ``"IPv6"`` IPv6 only,
and ``"IPv4v6"`` both, in one session: nothing on the way signals the version
of a datagram, and each receiver reads it from the datagram itself. A stack
with SDAP stops the simulation with an error when a datagram arrives on a
bearer of a session that does not carry its IP version.

**Addressing.** The UE's IP addresses are configured statically, by INET's
network configurators, like the addresses of any other INET host: the
:ned:`Ipv4NetworkConfigurator` of the network for IPv4, and an IPv6
configurator for IPv6. The reference networks have an optional
``ipv6Configurator`` submodule for this purpose, empty by default. The
:ned:`Binder` learns the UE's addresses, and that is how the anchor and the
base station find the UE that a downlink datagram is addressed to.

**IPv6 sessions.** An IPv6 session needs three things beyond IPv4:

*  the UE runs IPv6 (``hasIpv6 = true``), and with an IPv6-only session no
   IPv4 (``hasIpv4 = false``);

*  the network has an IPv6 configurator, and every node of the data network
   that the UE talks to runs IPv6 as well;

*  the anchor answers the UE's IPv6 Neighbor Discovery, with its
   :ned:`Ipv6NdResponder` (``hasNdResponder = true``). The anchor is the
   router on the IP link of an IPv6 session, so the UE's Router and Neighbor
   Solicitations must end there; without the responder, the simulation stops
   with an error at the UE's first link-local packet, before the packet can
   leak into the data network.

The transport network of the core -- the links that carry the tunnels between
the base stations and the UPFs -- can stay IPv4. The ``Ipv6`` configuration of
``simulations/nr/standalone_drb`` shows the whole setup:

.. code:: ini

   *.ipv6Configurator.typename = "Ipv6NetworkConfigurator"
   *.ipv6Configurator.config = xml("<config><interface hosts='**' \
           prefix='2001:db8:0:x::/64'/></config>")
   **.ipv6.configurator.networkConfiguratorModule = "ipv6Configurator"
   *.ue[*].hasIpv4 = false
   *.ue[*].hasIpv6 = true
   *.server.hasIpv4 = false
   *.server.hasIpv6 = true
   *.router.hasIpv4 = false
   *.router.hasIpv6 = true
   *.upf.hasNdResponder = true
   *.ue[*].sessionType = "IPv6"

**IPv4v6 sessions.** A UE with an IPv4v6 session runs both IP versions
(``hasIpv4`` and ``hasIpv6``), and so does the data network. An application
picks the version with INET's address syntax, for example ``"ue[0](ipv4)"``
and ``"ue[0](ipv6)"``. The ``Ipv4v6`` configurations of
``simulations/nr/standalone`` send voice over both versions on one session.

Unstructured sessions
---------------------

An Unstructured session (TS 23.501 5.6.10.3) carries payload that the 5G system
does not interpret. At the UE, the payload comes from an application attached
to the cellular NIC directly, with no transport or network layer in between.
At the anchor, the session ends in a point-to-point tunnel to one application
server in the data network, based on UDP/IP (TS 29.561 9.2): the anchor sends
the session's uplink payload to the server in UDP datagrams, from an N6 address
of the session, and the server's datagrams to that address are the session's
downlink, whose payload the anchor puts into the session's tunnel without the
UDP/IP headers.

**The UE.** The UE gets the application with ``numUnstructuredApps = 1``, in
its ``unstructuredApp[0]`` submodule; a UE has at most one. The default
application, :ned:`UnstructuredApp`, both sends and receives; its traffic
source and sink are INET queueing elements, configured as in INET's Ethernet
applications. :ned:`UnstructuredSourceApp` and :ned:`UnstructuredSinkApp` only
send or only receive. The UE normally has no IP layer.

**The anchor.** Only a UPF can anchor Unstructured sessions; a P-GW anchor
stops the simulation with an error at the session's establishment. The UPF's
``nodeControl`` (:ned:`UserPlaneNodeControl`) holds the N6 end of its
Unstructured sessions:

*  ``unstructuredPrefix`` is the address prefix of the sessions' N6 addresses.
   The core network allocates one address per session from it, and the data
   network must route the prefix to the UPF. The prefix may be IPv4 or IPv6.

*  ``unstructuredServer`` names the application server, which is resolved in
   the address family of the prefix, and ``unstructuredServerPort`` the port
   that the uplink datagrams go to.

*  ``unstructuredPort`` is the UDP port the UPF sends from and receives the
   downlink on. TS 29.561 defines no port numbers, so both ports have no
   default and must be set.

The server can be any UDP application; it sees the UE as the session's N6
address. The ``simulations/nr/unstructured`` example uses INET's echo
application, which sends every datagram back:

.. code:: ini

   # the UE: an Unstructured session, and an application attached to the NIC
   *.ue[*].sessionType = "Unstructured"
   *.ue[*].hasIpv4 = false
   *.ue[*].numUnstructuredApps = 1
   *.ue[*].unstructuredApp[0].source.packetLength = 100B
   *.ue[*].unstructuredApp[0].source.productionInterval = 20ms

   # the server, which echoes the payload
   *.server.numApps = 1
   *.server.app[0].typename = "UdpEchoApp"
   *.server.app[0].localPort = 5000

   # the N6 end of the Unstructured sessions at the UPF
   *.upf.nodeControl.unstructuredServer = "server"
   *.upf.nodeControl.unstructuredServerPort = 5000
   *.upf.nodeControl.unstructuredPort = 5001
   *.upf.nodeControl.unstructuredPrefix = "2001:db8:5::/64"

The data network's configuration adds a route for the prefix towards the UPF.

**Restrictions.** An Unstructured session has one QoS flow, the default one,
on the default bearer. The following follow from that, and each is an error
at the session's establishment or when the bearer is installed:

*  no uplink QoS rule (``ulQfiRules`` of the :ned:`BearerConfigurator`) may be
   scoped to the UE;

*  the UE may have no bearer besides the default one;

*  the bearer may not use header compression (ROHC), which applies to IP
   sessions only.

The UE's stack must have SDAP, which a 5G core network gives it (see
:doc:`deployments`), and the UE may not be D2D-capable (``hasD2D``). The
traffic of an Unstructured session is not steered to MEC hosts, because that
goes by the IP destination, which the payload does not have.

Ethernet sessions
-----------------

An Ethernet session carries Ethernet frames between the UE and an Ethernet
bridge at the anchor UPF, and turns the UE into an Ethernet host or bridge.
Ethernet sessions are described in :doc:`ethernet-sessions`.

Statistics
----------

Sessions record no statistics of their own. The performance of a session's
traffic is measured by the applications at its ends and by the layers of the
radio protocol stack (see :doc:`results`).

Example simulations
-------------------

*  ``simulations/nr/standalone``: IPv4 sessions, and in the ``Ipv4v6``
   configurations IPv4v6 sessions;

*  ``simulations/nr/standalone_drb``: the ``Ipv6`` configurations run IPv6
   sessions with the anchor's Neighbor Discovery responder;

*  ``simulations/nr/unstructured``: Unstructured sessions over an IPv6 or an
   IPv4 data network, during a handover, and next to an IPv4 session at the
   same anchor;

*  ``simulations/nr/ethernet``: Ethernet sessions.

The module tests in ``tests/module`` show the configurations that are
rejected, with the error each one produces (the files named
``UnstructuredSessionRejected_*`` and ``Ipv6Session_NoNdResponder``).

Fidelity
--------

*  A UE has one PDU session, with session id 1, of the type its
   configuration states. The type is fixed for the run: a UE cannot request a
   second session or a session of another type. One session per UE is a
   policy of :ned:`CoreControl`, not a limit of the protocol modules.

*  The session is established when the UE first attaches and released when it
   leaves the simulation; neither the UE nor the network releases it earlier.

*  The anchor is the gateway of the base station where the UE's downlink
   enters the radio access network, a choice the network topology makes; there
   is no UPF selection by the SMF. The anchor stays for the lifetime of the
   session (SSC mode 1); SSC modes 2 and 3 are not modeled.

*  The anchor does not allocate the UE's IP addresses: INET's configurators
   assign them statically, before the session exists. The Router
   Advertisements of the anchor's Neighbor Discovery responder carry the
   session's prefix for information only.

*  The signaling of session establishment and release (NAS, NGAP or S1AP, and
   PFCP or GTP-C) is modeled as direct calls, which take no time.

*  An Unstructured session's N6 end is a UPF's, not a data network's: a UPF
   anchors Unstructured sessions to one server, through one prefix and one
   pair of ports.
