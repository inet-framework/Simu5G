The UE
======

Overview
--------

The User Equipment (UE) is the mobile terminal of the network. In Simu5G, a UE
is an INET host with a cellular network interface: it has INET's
applications, transport protocols, network layers and mobility models, and the
cellular NIC, which carries the LTE or NR protocol stack, connects it to the
network through the radio medium.

Simu5G provides two UE types and their vehicular variants:

:ned:`LteUe`
   An LTE terminal: its NIC (:ned:`LteNicUe`) has one LTE protocol stack,
   which attaches to an eNB.

:ned:`NrUe`
   A terminal with two protocol stacks, an LTE one and an NR one, in one NIC
   (:ned:`NrNicUe`). Each stack is a *leg* that attaches to a base station of
   its own technology: the LTE leg to an eNB, the NR leg to a gNB. With only
   the NR leg attached, the UE is a 5G standalone terminal; with both legs
   attached, it is a dual connectivity terminal (see
   :doc:`dual-connectivity`). :ned:`NrUe` is the UE type of every 5G network.

:ned:`LteCar` and :ned:`NrCar`
   The same terminals with vehicular mobility, driven by the Veins framework.
   See :doc:`mobility`.

The UE as an INET host
----------------------

:ned:`LteUe` extends INET's :ned:`StandardHost`, so everything the INET
documentation says about :ned:`StandardHost` applies: applications are
configured in the ``app[]`` vector (``numApps``), the transport protocols and
the network layers are switched on and off with ``hasUdp``, ``hasTcp``,
``hasIpv4``, ``hasIpv6`` and the like, and the mobility model is chosen with
``mobility.typename``. The default mobility of a UE is stationary.

.. code:: ini

   *.ue[*].numApps = 1
   *.ue[*].app[0].typename = "UdpSink"
   *.ue[*].app[0].localPort = 3000
   *.ue[*].mobility.typename = "LinearMobility"
   *.ue[*].mobility.speed = 10mps

The cellular NIC is the UE's network interface named ``cellular``, and it
comes after the loopback interface and the Ethernet interfaces (``eth[]``) in
the interface table. INET resolves a node name in an address parameter to the
first non-loopback interface that has an address, so ``"ue[0]"`` is the
cellular interface's address only when the UE has no Ethernet interface with
an address; ``"ue[0]%cellular"`` names the cellular interface explicitly.

The Ethernet interfaces of a UE have two uses: in emulation, they connect the
UE to a real network outside the simulator (see :doc:`emulation`); in an
Ethernet session, they connect the devices behind a UE that acts as a bridge
(see :doc:`ethernet-sessions`).

The UE's session
----------------

A UE has one PDU session (a PDN connection in an EPC), and its type is set
with the ``sessionType`` parameter. The UE requests the session with this type
when it registers with the core network, and the network establishes it, or
stops the simulation with an error if its core network cannot carry the type:

``"IPv4"``, ``"IPv6"``, ``"IPv4v6"``
   IP sessions, carried by both a 5G core and an EPC. ``"IPv4"`` is the
   default.

``"Ethernet"``
   The session carries Ethernet frames, and the UE becomes an Ethernet node:
   an Ethernet host (``ethernetMode = "host"``, the default) or an Ethernet
   bridge for the devices on its ``eth[]`` interfaces
   (``ethernetMode = "bridge"``). Needs a 5G core whose anchor UPF has an
   Ethernet bridge. See :doc:`ethernet-sessions`.

``"Unstructured"``
   The session carries opaque payload between an application attached to the
   NIC directly (``numUnstructuredApps``, at most one) and a server in the
   data network. Needs a 5G core. See :doc:`pdu-sessions`.

A UE whose session is not IP normally has no IP layer either
(``hasIpv4 = false``): nothing configures IP over such a session. The session
model is described in :doc:`pdu-sessions`.

Cell association
----------------

Each leg of a UE starts out attached to the base station that a node-level
parameter names by its node id (see :doc:`architecture` for how node ids are
assigned):

*  ``servingNodeId``: the eNB of the LTE leg;

*  ``nrServingNodeId``: the gNB of the NR leg (:ned:`NrUe` only).

A value of 0, the default, leaves the leg detached. A 5G standalone UE sets
``nrServingNodeId`` only; an EN-DC UE sets both, the LTE leg to the master eNB
and the NR leg to the secondary gNB.

.. code:: ini

   *.ue[*].nrServingNodeId = 1

Each leg has a connection control module in the RRC of the NIC, which handles
the leg's cell association: ``cellularNic.rrc.connectionControl`` for the LTE
leg, ``cellularNic.rrc.nrConnectionControl`` for the NR leg (see
:ned:`ConnectionControlUe`). Two of its parameters change how a UE finds its
cells:

``dynamicCellAssociation``
   At initialization, the leg measures the received power of the base
   stations of its technology and attaches to the strongest one, instead of
   the configured one. The configured serving node is kept if no base station
   is found.

``enableHandover``
   The leg keeps measuring the base stations' beacons during the run, and
   hands over when another cell becomes better than the serving one by a
   margin. A detached leg with handover enabled attaches when it finds a
   cell. See :doc:`handover`.

Both are usually switched on for all legs at once:

.. code:: ini

   **.dynamicCellAssociation = true
   **.enableHandover = true

The cellular NIC
----------------

The UE's NIC is chosen by the node: :ned:`LteNicUe` for :ned:`LteUe`,
:ned:`NrNicUe` for :ned:`NrUe`. With ``hasD2D = true``, the node selects the
D2D-capable variants instead (:ned:`LteNicUeD2D`, :ned:`NrNicUeD2D`; see
:doc:`d2d`). An explicit ``cellularNic.typename`` in the configuration
overrides the node's choice. The NIC's structure and its replaceable parts are
described in :doc:`cellular-nic`.

The NIC's protocol stack depends on the session type: for an Ethernet session,
the top of the NIC is an Ethernet port of the UE's link layer (see
:doc:`ethernet-sessions`); for an Unstructured session, the application is
attached to the NIC directly.

Other UE parameters
-------------------

``hasRniSupport``
   Adds the statistics collectors that the MEC Radio Network Information
   service reads (see :doc:`mec`).

``extHostAddress``
   In an emulation, the IPv4 address of a real host behind the UE: traffic to
   that address is delivered to this UE (see :doc:`emulation`).

The complete list of parameters is in the NED Reference of :ned:`LteUe` and
:ned:`NrUe`.

Example simulations
-------------------

Every example simulation has UEs. The following show particular aspects of
the UE:

*  ``simulations/nr/standalone``: 5G standalone UEs with IP sessions;

*  ``simulations/nr/dualConnectivity``: dual connectivity UEs;

*  ``simulations/nr/ethernet``: UEs with Ethernet sessions, as hosts and as
   bridges;

*  ``simulations/nr/unstructured``: UEs with Unstructured sessions;

*  ``simulations/nr/cars``: vehicular UEs (:ned:`NrCar`).

Fidelity
--------

*  A UE has exactly one PDU session (PDN connection), whose type is fixed for
   the whole run.

*  Cell selection and handover decisions are based on the received power of
   the base stations' beacons, compared against a minimum (``minRssi``) and
   with a hysteresis margin (``hysteresisFactor``). The measurement
   configuration of 3GPP TS 38.331 (measurement objects, reporting events
   such as A3) is not modeled.

*  An attached UE hands over only to a cell that has an X2 link to its
   serving cell. Handover through the core network (S1-based in LTE, N2-based
   in NR) is not modeled; a detached leg may attach to any cell.
