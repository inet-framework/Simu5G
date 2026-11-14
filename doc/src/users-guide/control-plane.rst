The Control Plane
=================

Overview
--------

In 3GPP, the control plane is the set of protocols and network functions that
set up and manage the user plane: it attaches UEs to the network, establishes
their sessions and bearers, and moves them between cells (TS 23.501,
procedures in TS 23.502). Its main protocols are RRC between the UE and the
base station (TS 38.331, TS 36.331), NAS between the UE and the AMF or MME,
NGAP or S1AP between the base station and the core network (TS 38.413, TS
36.413), XnAP or X2AP between base stations (TS 38.423, TS 36.423), and PFCP
between the SMF and the UPFs (TS 29.244). Its main procedures are
registration, PDU session establishment, bearer setup by RRC reconfiguration,
handover, and the recovery from a radio link failure. For registration and
session establishment, the UE sets up an RRC connection, the base station
passes its registration to the AMF, the SMF has the UPF install the session's
tunnels, and the base station sets up the session's resources and bearers.
For recovery, the UE detects the failure -- by radio link monitoring on the
physical layer (timer T310), by RLC reaching its maximum number of
retransmissions, or by a failed random access -- and tries to re-establish the
connection at a suitable cell (timers T311 and T301). Each message travels as
a packet over its interface, and its delay is part of the procedure's
duration. A UE without an RRC connection is in RRC_IDLE, where the network
reaches it by paging.

Simu5G models the control plane through its entities: the RRC in every
cellular NIC, the core network's control plane in one network-level module,
and a control-plane entry point in every user plane node. The procedures run
between them, each step at the node that performs it in 3GPP, as direct calls
named after the messages they stand for: attach and session establishment,
bearer establishment, the release of a UE that leaves, handover, dual
connectivity setup, and radio link failure with RRC re-establishment. Where
the duration of a procedure matters, it is modeled with timers. The messages
themselves are not simulated, so they take no time and no capacity. Radio
link failure is detected by RLC only; radio link monitoring on the physical
layer, random access failures, RRC_IDLE and paging are not modeled.

This chapter describes the control-plane entities and how they find each
other, then the procedures in the order a UE meets them. Handover and dual
connectivity have chapters of their own, :doc:`handover` and
:doc:`dual-connectivity`; the X2 interface is described in :doc:`x2`.

.. TODO: figure: the control-plane entities (UE RRC, base station RRC,
   CoreControl, UserPlaneNodeControl) and the calls of an attach between
   them, as a sequence chart. Drawn, not a NED screenshot.

The entities
------------

The control plane of a Simu5G network consists of these modules:

``coreControl`` (:ned:`CoreControl`)
   The control plane of the core network, one per cellular network: the AMF
   and the SMF of a 5G core, or the MME and the gateways' control plane of an
   EPC, in one module. It takes the UEs' registrations, establishes and
   releases their sessions, and programs the tunnels at the user plane nodes
   and the base stations (see :doc:`core-network` and :doc:`pdu-sessions`).

``rrc`` (:ned:`Rrc`) in every cellular NIC
   The RRC of the node, a compound with one submodule per function:

   ``connectionControl`` and ``nrConnectionControl``
      The control-plane entry point of the node: :ned:`ConnectionControlEnb`
      at a base station, :ned:`ConnectionControlUe` for each leg of a UE (the
      NR leg's is ``nrConnectionControl``). The control planes of other nodes
      talk to this module only. At a base station, it owns the node's tunnel
      endpoint ids, sets up the sessions' tunnels, establishes the bearers of
      the UEs it serves, and runs the network side of handover. At a UE, it
      measures the cells and runs the UE side of attach and handover.

   ``registration`` (:ned:`Registration`)
      Registers the node with the :ned:`Binder` and sets up its ``cellular``
      network interface.

   ``drbTable`` (:ned:`DrbTable`)
      The configuration of the node's data radio bearers.

   ``bearerManagement`` (:ned:`BearerManagement`)
      Creates and deletes the per-bearer PDCP and RLC entities, and handles
      radio link failure and RRC re-establishment.

``nodeControl`` (:ned:`UserPlaneNodeControl`) in every UPF and P-GW
   The node's end of N4 (or, in an EPC, of GTP-C): it installs the sessions'
   tunnels and the downlink classification rules into the node's user-plane
   relay, on the calls of :ned:`CoreControl`.

The entities find each other through the :ned:`Binder`: every node's
``registration`` registers the node under its node id, and every
``nodeControl`` registers itself as a user plane node, which is how
:ned:`CoreControl` learns the UPFs and P-GWs of the network. A base station
finds a UE's connection control, or another base station's, by node id.
:ned:`CoreControl` and the :ned:`BearerConfigurator` are reached through the
``coreControlModule`` and ``bearerConfiguratorModule`` parameters, like the
Binder (see :doc:`architecture`).

Inside a node, the RRC configures the other layers through direct calls, and
it pushes its state to them: the modules of the data path keep small copies
of what they need from the RRC (for example the node ids of the serving
cells), which the RRC updates whenever they change. The data path never asks
the RRC for its state; it only asks for an action, such as the establishment
of a bearer.

Attach and session establishment
--------------------------------

A UE leg attaches to a cell at initialization, when its serving cell is
configured (``servingNodeId``, ``nrServingNodeId``; see :doc:`ue`), or during
the run, when a detached leg finds a cell. The steps are those of an RRC
connection setup followed by registration and PDU session establishment:

1. The UE leg requests an RRC connection from the base station
   (``connectionSetupRequest()``), stating the type of the session the UE
   requests. At initialization, the UEs do this in module order, at the last
   initialization stage.

2. The base station takes the leg on and announces it to :ned:`CoreControl`
   with the INITIAL UE MESSAGE of NGAP or S1AP (``initialUeMessage()``).

3. At the UE's first registration, :ned:`CoreControl` establishes the UE's
   session: it checks that the core network can carry the requested type,
   chooses the anchor, and has the anchor's :ned:`UserPlaneNodeControl` set
   up the session's uplink tunnel (PFCP Session Establishment). A UE attached
   to a base station without a core network link gets no session.

4. :ned:`CoreControl` asks the base station to set up the session's
   resources (``sessionResourceSetup()``, the PDU Session Resource Setup of
   NGAP). The base station sets up its end of the tunnels, delivers the UE's
   uplink QoS rules to the UE when its stack has SDAP, and establishes the
   UE's static bearers on the leg that carries them.

5. :ned:`CoreControl` points the anchor's downlink at the base station
   (PFCP Session Modification).

A UE whose second leg attaches later -- for example the NR leg of a dual
connectivity UE -- goes through steps 1 and 2 again, but keeps its session.

Bearer establishment
--------------------

A data radio bearer is established by the base station that serves the UE,
from a definition of the :ned:`BearerConfigurator` (see :doc:`qos`). The base
station installs the bearer's entities at itself, at the UE, and at a dual
connectivity secondary node, through their connection controls; this stands
for the RRC reconfiguration that carries the bearer's configuration to the
UE.

Static bearers are established at step 4 of the attach. An on-demand bearer is
established when the data path of either end finds no bearer for a packet: at
the UE, the data path asks the UE's connection control, which passes the
request to the serving base station; at the base station, the data path asks
the base station's connection control directly. Either way, the bearer is in
place before the packet is sent.

Leaving the network
-------------------

When a UE leaves the simulation -- its module is deleted during the run --
each attached leg tells its base station that it is gone
(``connectionLost()``), and the base station releases its state for the leg:
the MAC queues, the RLC entities and the PDCP entities. When the UE's node ids
are unregistered from the :ned:`Binder`, :ned:`CoreControl` releases the UE's
session: the anchor and the base stations forget its tunnels (PFCP Session
Deletion, and the release of the session's resources).

A leg that is only detached from its cell, for example by a handover to no
cell, also tells its base station, but the session stays: :ned:`CoreControl`
only updates the downlink path.

Radio link failure and RRC re-establishment
-------------------------------------------

An RLC entity in acknowledged mode that reaches the maximum number of
retransmissions of a PDU (``maxRtxThreshold``, see :doc:`rlc`) declares radio
link failure. The node's :ned:`BearerManagement` releases the link -- the
peer's MAC, RLC and PDCP state, and the traffic towards it at the top of the
NIC -- and tells the peer, through the connection controls, to release its
end the same way. Either end can detect the failure.

What happens next depends on the re-establishment timer T311 (``t311``):

*  With ``t311 = 0s``, the default, there is no re-establishment: the link
   stays released, and the UE's traffic stops for good.

*  With a positive ``t311``, RRC re-establishment (TS 38.331 5.3.7) runs on
   timers. When T311 expires, a suitable cell is assumed to have been
   selected -- the same cell -- and the re-establishment request starts T301
   (``t301``). When T301 expires, the re-establishment is complete: the link
   is restored, and the bearers are established again on demand, so
   re-establishment needs on-demand bearer establishment, which is on by
   default (see :doc:`qos`).

The procedure in the context of mobility is described in :doc:`handover`.

Handover and dual connectivity
------------------------------

**Handover.** The serving base station decides on a UE leg's measurement
report (``measurementReport()``) and prepares the handover with the target
over Xn: the Handover Request carries the leg's context, and the target sets
up the session's tunnels and the leg's bearers and starts holding its
downlink. On the Handover Request Acknowledge, the source commands the UE
(``handoverCommand()``) and forwards the leg's downlink to the target over X2.
The UE spends a configurable time between the cells, and when it arrives
(``reconfigurationComplete()``), the target has :ned:`CoreControl` switch the
session's downlink path (PATH SWITCH REQUEST) and releases the leg at the
source (UE CONTEXT RELEASE). See :doc:`handover`.

**Dual connectivity.** A secondary node is paired with its master by
configuration (``masterId``). The master establishes each bearer's part at the
secondary together with its own, and sets up the X2 user-plane tunnels of the
bearer, one per direction, each allocated at its receiving end. See
:doc:`dual-connectivity`.

Configuration
-------------

The control plane needs no configuration of its own beyond the network-level
modules (see :doc:`deployments`) and the bearer definitions (see :doc:`qos`).
Two tasks touch it directly.

**Enable RRC re-establishment after a radio link failure.** Set the two
timers of the RRC's bearer management. This example, the
``AM-RLF-Reestablish`` configuration of ``simulations/nr/rlc``, restores a
link a little over a second after it failed:

.. code:: ini

   **.bearerManagement.t311 = 1s
   **.bearerManagement.t301 = 0.1s

**Let UEs attach during the run.** A leg without a configured serving cell
attaches when it finds one, if its connection control measures the cells (see
:doc:`ue`):

.. code:: ini

   **.enableHandover = true

Statistics
----------

The control plane records one statistic: ``servingCell``, the node id of the
serving cell of each UE leg over time, recorded by
:ned:`ConnectionControlUe` at initialization and at every handover. The
effects of the procedures show in the statistics of the user plane, for
example the gap in an application's throughput during a handover.

Example simulations
-------------------

*  Every example simulation runs attach and session establishment at
   initialization.

*  ``simulations/nr/rlc`` and ``simulations/lte/rlc``: the ``AM-RLF`` and
   ``AM-RLF-Reestablish`` configurations, a radio link failure without and
   with re-establishment.

*  ``simulations/lte/test_handover``, ``simulations/nr/standalone_multicell``:
   handover.

*  ``simulations/nr/dualConnectivity``: dual connectivity setup.

*  ``tests/module``: the session configurations the control plane rejects
   (an Unstructured session at an EPC anchor, an Ethernet session without a
   bridge, two sessions per UE, and others), each with its error message.

Fidelity
--------

*  The signaling is not simulated: RRC, NAS, NGAP or S1AP, XnAP or X2AP for
   handover preparation, and PFCP are direct calls. They take no time, use no
   capacity, and are never lost. The durations that matter are modeled with
   timers: the handover latency, and T311 and T301 of re-establishment.

*  The core network's control plane is one module; the network functions
   beyond the AMF and the SMF (PCF, UDM, AUSF), and the authentication and
   security procedures, are not modeled.

*  Radio link failure is detected only by RLC acknowledged mode. Radio link
   monitoring on the physical layer (T310 and the out-of-sync indications) and
   random access failures are not modeled, so a link that carries only RLC UM
   bearers never fails.

*  Known deviation: a radio link failure releases the link at both ends at
   once. In 3GPP, each end detects the failure on its own.

*  Known deviation: RRC re-establishment always succeeds, at the same cell.
   T311 is a fixed delay, not a cell search, and the radio conditions are not
   checked.

*  There is no RRC_IDLE or RRC_INACTIVE state and no paging: a UE whose link
   was released without re-establishment does not come back, even when new
   data arrives for it.
