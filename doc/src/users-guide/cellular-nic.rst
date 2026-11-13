The Cellular NIC
================

Overview
--------

3GPP describes the radio interface of the UE and the base station as a
protocol stack (TS 38.300 for NR, TS 36.300 for LTE). In the user plane, from
the top: SDAP, in NR only, maps QoS flows to radio bearers; PDCP handles
header compression, ciphering and in-order delivery; RLC segments and
retransmits; MAC multiplexes the logical channels into transport blocks,
schedules them and runs HARQ; and the physical layer transmits them. In the
control plane, the RRC configures these layers and controls the connection.
A UE in dual connectivity has a MAC and a physical layer for each of its two
cell groups, below a shared PDCP.

Simu5G packages the stack of a node into one compound module, the cellular
NIC: the ``cellularNic`` submodule of every UE and base station. To the rest of
the node, the NIC is a network interface like any other INET interface, named
``cellular``. It contains every layer of the user plane and the RRC, and the
radio that connects the node to the radio medium; the PDCP and RLC entities of
the bearers are created at run time. The NIC of a dual connectivity UE has two
legs below PDCP, one per radio technology. The RRC configures the layers of
its node through direct calls, and the RRC messages between the UE and the
base station are not simulated.

This chapter describes the NIC as a whole: the NIC types, the submodules they
contain, the path a packet takes through them, and the parts that can be
replaced. The layers themselves are described in their own chapters:
:doc:`qos` for SDAP, then :doc:`pdcp`, :doc:`rlc`, :doc:`mac` and
:doc:`phy`.

.. TODO: figure: the submodules of NrNicUe and NrNicEnb with the two legs of
   the UE and the per-bearer entities. The old NR_NIC.png placed the channel
   models inside the NIC; they are in the radio medium now. Drawn, not a NED
   screenshot.

NIC types
---------

There is a NIC type for each node role and technology. The node chooses its
NIC by default, and every NIC type can be replaced through
``cellularNic.typename``: the base stations' NICs implement the
:ned:`ICellularNic` module interface, and the UEs' NICs :ned:`IUeCellularNic`.

:ned:`LteNicEnb`
   The NIC of an eNB: one LTE stack.

:ned:`NrNicEnb`
   The NIC of a gNB: one NR stack. It extends :ned:`LteNicEnb`.

:ned:`LteNicUe`
   The NIC of an :ned:`LteUe`: one LTE stack.

:ned:`NrNicUe`
   The NIC of an :ned:`NrUe`: two stacks, an LTE *leg* and an NR leg, above
   which a single PDCP layer is shared. Each leg has its own RLC, MAC, PHY and
   radio, and attaches to a base station of its own technology. It extends
   :ned:`LteNicUe`.

:ned:`LteNicEnbD2D`, :ned:`NrNicEnbD2D`, :ned:`LteNicUeD2D`, :ned:`NrNicUeD2D`
   The D2D-capable variants, which the nodes select when ``hasD2D`` is true
   (see :doc:`d2d`).

:ned:`LteNicBase` is the common base of all of them.

Submodules
----------

Every NIC contains the following submodules, from the top of the stack to the
bottom:

``handoverPacketHolder``
   Holds the traffic of a UE in handover: at a UE, the uplink while the UE is
   between cells (:ned:`HandoverPacketHolderUe`); at a base station, the
   downlink of a UE that is leaving, which it forwards to the target over X2,
   and the downlink of a UE that is arriving, which it holds until the UE is
   there (:ned:`HandoverPacketHolderEnb`). See :doc:`handover`.

``ip2nic`` (:ned:`Ip2Nic`)
   The boundary between the node and the stack. It extracts the flow
   information of a packet from its headers, finds the destination UE at a
   base station, and finds the bearer that carries the packet. Without SDAP,
   it also classifies the traffic onto bearers.

``rrc`` (:ned:`Rrc`)
   The control plane of the NIC, a compound of its own: ``registration``
   registers the node with the :ned:`Binder` and sets up the network
   interface; ``drbTable`` holds the configuration of the node's data radio
   bearers; ``bearerManagement`` creates and tears down the per-bearer
   entities of PDCP and RLC; ``connectionControl`` is the control-plane entry
   point of the base station or of the UE's LTE leg, and
   ``nrConnectionControl`` that of the UE's NR leg. See
   :doc:`control-plane`.

``pdcpMux``
   Dispatches the packets coming from above to the PDCP entity of their
   bearer, and collects the packets the PDCP entities deliver upwards.

``rlcMux``
   The same for RLC, towards the MAC: it dispatches the MAC's packets to the
   RLC entities and collects the RLC entities' packets for the MAC.

``mac``
   The MAC layer: :ned:`LteMacEnb` or :ned:`NrMacGnb` at a base station, with
   the schedulers; :ned:`LteMacUe` at a UE. See :doc:`mac`.

``phy``
   The physical layer: :ned:`PhyEnb` at a base station, :ned:`PhyUe` at a UE.
   See :doc:`phy`.

``radio`` (:ned:`CellularRadio`)
   The node-side end of the radio medium: the antenna, the transmitter and
   the receiver. Every frame the PHY transmits passes through it, and every
   frame that arrives for the node enters through it. See :doc:`phy` and
   :doc:`radio-medium`.

The PDCP and RLC entities do not appear among the submodules of the NIC type:
the RRC creates them at run time, one PDCP entity and one RLC entity per leg
for every data radio bearer, as the bearer is established, and deletes them
when it is torn down. In a running simulation, they appear in the NIC next to
the static submodules.

A NIC with SDAP has two more submodules (see :doc:`qos`):

``sdap`` (:ned:`NrSdap`)
   The SDAP layer, between ``ip2nic`` and ``pdcpMux``: it maps QoS flows to
   data radio bearers and adds and removes the SDAP header.

``qosFlowClassifier`` (:ned:`QosFlowClassifier`, at a UE only)
   Classifies the UE's uplink traffic onto QoS flows, with the QoS rules the
   core network installed. It sits above ``ip2nic``.

A UE's NIC also has the downlink feedback generator, ``dlFbGen``, which
produces the channel quality reports for its base station (see :doc:`phy`).
The NR leg of :ned:`NrNicUe` has its own copies of the leg's modules:
``nrRlcMux``, ``nrMac``, ``nrPhy``, ``nrRadio`` and ``nrDlFbGen``.

A base station's NIC has the modules that work with the X2 links (see
:doc:`x2`): ``x2Manager`` (:ned:`LteX2Manager`), which dispatches the X2
messages; ``handoverX2Forwarder``, which forwards the downlink of a UE in
handover; ``dcX2Forwarder`` and ``pdcpDcMux``, which carry the data of the
dual connectivity bearers between the master and the secondary node; and
``compManager``, the CoMP manager, present with ``compEnabled``. It also has
one background traffic generator per carrier, ``bgTrafficGenerator[]`` (see
:doc:`background`).

The path of a packet
--------------------

Downlink, at the base station: a datagram from the relay enters the NIC and
passes ``handoverPacketHolder``, ``ip2nic`` and, if present, ``sdap``. The
``pdcpMux`` hands it to the PDCP entity of its bearer, which passes the PDCP
PDU to the bearer's RLC entity. The MAC's scheduler decides when the UE is
served; the MAC then requests data from the RLC entities through ``rlcMux``,
builds a transport block, and passes it to ``phy``, which transmits it
through ``radio``.

Downlink, at the UE: the transmission arrives through the radio medium at the
UE's ``radio``, whose receiver decides whether it is received correctly. The
``phy`` passes a successful transport block to the ``mac``, which hands its
contents to the RLC entities through ``rlcMux``. From there the data goes up
through the PDCP entity, ``pdcpMux``, ``sdap`` and ``ip2nic`` to the UE's
network layer.

The uplink takes the same path in the opposite direction, with one addition at
a UE with SDAP: the ``qosFlowClassifier`` assigns each uplink packet its QoS
flow before it enters ``ip2nic``. At the base station, the NIC hands the
uplink data to the relay, which tunnels it into the core network.

For a UE with an Ethernet session, the top of the NIC is an Ethernet port of
the UE's link layer (``ethernetPortal``); for a UE with an Unstructured
session, the application is attached to the NIC directly. See
:doc:`ethernet-sessions` and :doc:`pdu-sessions`.

Dual connectivity
-----------------

The two legs of :ned:`NrNicUe` share the PDCP layer: a bearer has one PDCP
entity, and one RLC entity on each leg that carries it. At the master base
station, the PDCP entity of a split bearer has a leg on the master's own RLC
and a leg to the secondary node, over X2; at the secondary, a PDCP relay
entity takes the place of the PDCP entity and tunnels the PDUs to and from the
master. Dual connectivity is enabled in every NIC with the
``dualConnectivityEnabled`` parameter. See :doc:`dual-connectivity`.

Replaceable parts
-----------------

Most submodules of the NIC are declared with a module interface, and their
types can be changed in the configuration. The ones a model developer most
often replaces are:

*  the MAC (``mac.typename``, ``nrMac.typename``), for example to add a new
   scheduler;

*  the PHY (``phy.typename``, ``nrPhy.typename``);

*  the RRC submodules, for example ``rrc.connectionControl.typename``;

*  ``sdap``, ``qosFlowClassifier``, ``pdcpMux``, ``rlcMux``, ``ip2nic`` and
   ``handoverPacketHolder``;

*  the CoMP manager (``compManager.typename``).

The per-bearer entities are created by type name, so their types are
parameters of the RRC's :ned:`BearerManagement`: ``pdcpEntityModuleType`` for
the PDCP entity, ``pdcpRelayEntityModuleType`` for the PDCP relay entity of a
dual connectivity secondary, and ``rlcTmEntityModuleType``,
``lteRlcUmEntityModuleType``, ``lteRlcAmEntityModuleType``,
``nrRlcUmEntityModuleType`` and ``nrRlcAmEntityModuleType`` for the RLC
entities of each mode and technology. For example, the master eNB of an EN-DC
deployment uses NR PDCP entities:

.. code:: ini

   *.masterEnb.cellularNic.rrc.bearerManagement.pdcpEntityModuleType = \
           "simu5g.stack.pdcp.NrPdcpEntity"

Other NIC parameters
--------------------

``processingDelayIn``, ``processingDelayOut``
   An additional delay for the packets entering and leaving the NIC at the top,
   0 by default.

``hasSdap``
   Whether the stack has an SDAP layer (:ned:`NrNicEnb` and :ned:`NrNicUe`).
   The networks set it according to their core network: true with a 5G core,
   false with an EPC (see :doc:`deployments`).

``useReflectiveQos``
   Whether the UE uses reflective QoS (:ned:`NrNicUe`, see :doc:`qos`).

``compEnabled``
   Adds the CoMP manager (base stations, see :doc:`x2`).

``numCarriers``
   The number of carriers a base station serves, which sizes
   ``bgTrafficGenerator[]``.

The complete list of parameters is in the NED Reference of each NIC type.

Fidelity
--------

*  The RRC configures the layers of its own node through direct calls; the RRC
   messages between a UE and its base station are not simulated (see
   :doc:`control-plane`).
