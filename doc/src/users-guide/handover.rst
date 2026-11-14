Handover
========

Overview
--------

Handover (3GPP TS 38.300 9.2.3 for NR, TS 36.300 10.1.2 for LTE) moves a UE in
connected mode from its serving cell to a target cell without interrupting
its sessions. It is network-controlled and UE-assisted: the UE measures the
cells around it and reports, and the serving base station decides. Its main
phases are preparation, execution and completion. In preparation, the source
base station asks the target to admit the UE over Xn (X2 in LTE), and the
target reserves resources and returns the configuration the UE is to use. In
execution, the source commands the UE to the target and forwards the UE's
downlink data to it, while the UE detaches from the source and synchronizes
with the target. In completion, the target has the core network switch the
downlink path to itself, and the source releases the UE's context. Between
base stations without an Xn connection, the handover runs through the core
network instead (N2-based in NR, S1-based in LTE), and NR also defines
conditional handover and dual active protocol stack (DAPS) handover. A UE
that loses its radio link without a handover declares radio link failure
(TS 38.331 5.3.10) and tries to re-establish its RRC connection (TS 38.331
5.3.7).

Simu5G models the Xn-based handover (X2-based in LTE), with each phase at the
entity that performs it in 3GPP. The UE measures the beacons of the base
stations around it and reports to its serving base station when another cell
is better by a hysteresis margin; the serving base station prepares the
handover with the target and commands the UE; the source forwards the UE's
downlink to the target over X2 while the target holds it; and the target has
the core network switch the downlink path. The signaling messages are direct
calls, and the time the UE spends between the two cells is a configurable
latency. Radio link failure, detected by RLC acknowledged mode when it runs
out of retransmissions, and RRC re-establishment with its timers T311 and
T301 are modeled too. Handover through the core network, conditional handover
and DAPS handover are not modeled, and an attached UE hands over only to a
cell that has an X2 link to its serving cell.

This chapter follows a handover from the first measurement to the release of
the source, describes what happens to the UE's data, how the two legs of a
dual connectivity UE hand over, and what happens when a radio link fails. The
procedures run between the control-plane entities described in
:doc:`control-plane`; how a UE first finds its cell is described in
:doc:`ue`.

Enabling handover
-----------------

Handover is off by default. It is switched on with the ``enableHandover``
parameter, which exists on both sides of the radio link:

*  on the base station's PHY (:ned:`PhyEnb`), where it makes the base station
   a handover target: it broadcasts beacons every ``beaconInterval`` of its
   ``cellInfo`` (1 s by default), from which the UEs measure it;

*  on each UE leg's connection control (:ned:`ConnectionControlUe`), where it
   makes the leg evaluate the beacons it receives.

A UE leg without ``enableHandover`` ignores beacons and stays with its cell,
and a base station with handover enabled but beacons switched off
(``enableBeacons = false``) is an error. The example configurations switch
handover on everywhere with one line:

.. code:: ini

   **.enableHandover = true

A handover also needs an X2 link between the two base stations, with X2
applications at both ends (see :doc:`deployments`): an attached UE never hands
over to a cell that has no X2 link to its serving cell.

Measurement and decision
------------------------

A base station sends its beacons on its primary carrier. A UE leg receives
the beacons of the base stations of its own radio technology, and for each
it computes a measurement: the SINR of the beacon, averaged over the resource
blocks of the carrier, which the model calls the beacon's RSSI.

The leg keeps track of the best cell it has heard. A beacon from a cell other
than the serving one is ignored when its measurement is below ``minRssi``
(by default the lowest SINR of the BLER curves; with ``minRssiDefault =
false``, the ``minRssi`` parameter), and when the cell has no X2 link to the
serving cell. A cell becomes the candidate when its measurement exceeds that
of the best cell so far by the hysteresis threshold, which is the best
measurement divided by ``hysteresisFactor`` (10 by default; 0 switches the
hysteresis off). Beacons of one round arrive at the same instant, so the leg
evaluates them a short moment later, when all have arrived.

When the leg has a candidate, it reports both measurements to its serving
base station (the measurement report). The decision is the base station's:
its connection control (:ned:`ConnectionControlEnb`) selects the target. By
default it accepts the cell the UE reported; a subclass can implement another
policy by overriding the target selection.

If the serving cell's own beacon falls below ``minRssi`` and there is no
candidate, the leg considers the cell lost and detaches from it. A detached
leg attaches again, without a handover, when it next hears a cell that is good
enough (see `When the radio link fails`_).

Preparation, execution and completion
-------------------------------------

The base stations run the handover through their connection controls, each
step as in TS 38.300 9.2.3.2, with the messages modeled as direct calls:

1. **Handover Request.** The source sends the target the UE leg's context:
   its sessions, with their tunnels, and the configuration of its bearers.
   The target sets up the sessions' downlink tunnels at the base station
   where the downlink will enter the radio access network (the target itself,
   or its master under dual connectivity), installs the bearer configuration,
   so that the bearers keep their identities, and starts holding the leg's
   downlink.

2. **Handover Request Acknowledge.** The target returns the downlink tunnel
   ids. The source commands the UE to the target (the RRC Reconfiguration
   with the handover command), and from then on forwards the leg's downlink
   that reaches it to the target over X2-U.

3. **Execution at the UE.** The UE holds its uplink, detaches from the
   source and attaches to the target. This takes ``handoverDetachmentTime``
   plus ``handoverAttachmentTime``, by default half of ``handoverLatency``
   (50 ms) each. A leg that only attaches, or only detaches, takes the one
   part it needs.

4. **Completion.** When the UE arrives (RRC Reconfiguration Complete), the
   target takes the leg on and asks the core network to switch the
   session's downlink path to itself (Path Switch Request). The anchor moves
   the downlink tunnel and closes the old path with an End Marker (TS 23.502
   4.9.1.2.2). The target then releases the leg at the source (UE Context
   Release), which deletes its state for the leg.

If the source loses the leg while the handover is being prepared or executed,
it cancels the handover at the target (Handover Cancel).

The downlink forwarding runs between the base stations' handover packet
holders (:ned:`HandoverPacketHolderEnb`): at the source, the leg's downlink
that arrives from the core network is sent to the target through the
:ned:`HandoverX2Forwarder` and the X2 GTP-U entity; at the target, the
forwarded packets and the packets that already arrive on the new path are
held. At completion, the target delivers the forwarded packets first, and the
packets of the new path once the End Marker of the old path has arrived, so
that no forwarded packet still on its way is overtaken. Packets that reach the
source after the handover has completed are forwarded too. The UE's handover
packet holder (:ned:`HandoverPacketHolderUe`) holds the uplink during the
execution and sends it at completion.

What happens to the data
------------------------

A handover moves the UE's bearers, not their contents. When the source
releases the leg, it deletes the leg's MAC queues, RLC entities and PDCP
entities, with whatever they still held; only the packets that arrive at the
source's NIC after the handover has started are forwarded. The target
creates new entities from the same bearer configuration, so the bearers
keep their identities, QoS and header compression, but their sequence numbers
and ROHC contexts start over (see the Fidelity section of :doc:`pdcp`). A
bearer that was established on demand at the source is established again at
the target when traffic needs it.

The UE's uplink that was waiting in its RLC and MAC queues is lost in the same
way; the uplink that the application sends during the execution is held and
sent at the target.

Handover and dual connectivity
------------------------------

The two legs of a dual connectivity UE hand over separately, each on the
beacons of its own radio technology:

*  The secondary leg can change to another secondary node of the same master.
   It ignores the beacons of a secondary node whose master is not the one its
   other leg is attached to, so it never attaches across masters.

*  When the master leg hands over to another master, the secondary leg is
   forced to detach first, since its secondary node belongs to the old
   master; the master leg's handover waits for that detachment. The secondary
   leg attaches again when it hears a secondary node of the new master.

Because the bearers of a dual connectivity UE terminate at the master, a
secondary leg's handover does not move the downlink path in the core network.
See :doc:`dual-connectivity`.

When the radio link fails
-------------------------

A UE leg can lose its radio link in two ways.

**Beacons.** With handover enabled, a leg whose serving cell's beacon falls
below ``minRssi`` detaches from it, and the base station releases its state
for the leg and tells the core network, which keeps the session and updates
its downlink path. The leg attaches again when it hears a
cell good enough, and the base station it attaches to has the session's
resources set up as at a first attachment.

**Radio link failure.** An RLC entity in acknowledged mode that reaches the
maximum number of retransmissions of a PDU (``maxRtxThreshold``) declares
radio link failure (see :doc:`rlc`). The RRC releases the link at both ends at
once: the failing node's bearer management deletes its MAC, RLC and PDCP
state for the peer and stops the peer's traffic, and tells the peer's control
plane, which does the same at its end. The UE stays attached to its cell.

What follows depends on RRC re-establishment (TS 38.331 5.3.7), which the
``t311`` and ``t301`` parameters of the RRC's :ned:`BearerManagement`
configure:

*  With ``t311 = 0s``, the default, there is no re-establishment: the leg's
   traffic stays stopped as long as the leg remains in that cell.

*  With ``t311`` greater than 0, the UE re-establishes. T311 stands for the
   cell selection, after which the same cell is taken as selected; then T301
   runs, for the re-establishment request and its completion. When T301
   expires, the traffic resumes, and the bearers are established again on
   demand.

Configuration
-------------

**Enable handover between two base stations.** Switch handover on, and give
the base stations an X2 link with X2 applications. From the ``Handover-General``
configuration of ``simulations/lte/test_handover``:

.. code:: ini

   **.enableHandover = true

   *.eNodeB*.numX2Apps = 1
   *.eNodeB*.x2App[*].server.localPort = 5000 + ancestorIndex(1)
   *.eNodeB1.x2App[0].client.connectAddress = "eNodeB2%x2ppp0"
   *.eNodeB2.x2App[0].client.connectAddress = "eNodeB1%x2ppp0"

**Tune the decision and the interruption.** The hysteresis, the minimum
measurement and the time the UE is between cells are parameters of each leg's
connection control:

.. code:: ini

   *.ue[*].cellularNic.rrc.*onnectionControl.hysteresisFactor = 5
   *.ue[*].cellularNic.rrc.*onnectionControl.handoverLatency = 20ms

**Enable RRC re-establishment.** From the ``AM-RLF-Reestablish``
configuration of ``simulations/nr/rlc``:

.. code:: ini

   **.bearerManagement.t311 = 1s
   **.bearerManagement.t301 = 0.1s

**Keep a configured bearer across handovers.** A static bearer is defined for
the UE, not for a cell, so every target establishes it again. From the
``VoIP-DL-StaticDrb`` configuration of ``simulations/lte/test_handover``:

.. code:: ini

   *.bearerConfigurator.staticDrbs = [{coreNetwork: "epc", ue: "ue11", \
           drbId: 1, profile: "qci-1", filters: ["VoIP*"]}]

Statistics
----------

Each UE leg records its serving cell over time in the ``servingCell``
vector, the node id of the cell, or 0 while detached, which is the direct way
to see handovers in the results. The effect of a handover on the traffic shows
in the statistics of the applications, RLC and MAC (see :doc:`results`).

Example simulations
-------------------

*  ``simulations/lte/test_handover``: UEs moving between two eNBs, with voice
   and file transfer in both directions, over RLC UM and AM
   (``VoIP-AM-Handover``), and with a static bearer (``VoIP-DL-StaticDrb``);

*  ``simulations/nr/standalone_multicell``: handover between two gNBs of a
   5G standalone network;

*  ``simulations/nr/dualConnectivity_multicell``: handovers of dual
   connectivity UEs between two EN-DC pairs;

*  ``simulations/nr/rlc``: radio link failure without (``AM-RLF``) and with
   (``AM-RLF-Reestablish``) RRC re-establishment.

Fidelity
--------

*  The handover signaling between the UE, the base stations and the core
   network (RRC, XnAP or X2AP, NGAP or S1AP) is modeled as direct calls,
   which take no time. The only latency of a handover is the UE's detachment
   and attachment time.

*  Only the Xn-based (X2-based) handover is modeled: an attached UE hands over
   only to a cell with an X2 link to its serving cell. The handover through
   the core network, conditional handover and DAPS handover are not modeled.

*  Known deviation: the measurement is not the RSRP of TS 38.215. It is the
   beacon's SINR averaged over the resource blocks of the primary carrier, and
   the hysteresis is proportional to the measured value
   (``hysteresisFactor``), not an offset in dB as in the measurement events
   of TS 38.331 (such as A3). There is no time to trigger, and no measurement
   configuration.

*  The data queued at the source is lost at a handover: PDCP status reports
   and the retransmission of unacknowledged PDCP PDUs at the target, which
   make a 3GPP handover lossless for RLC AM bearers, are not modeled. The
   ``losslessHandover`` parameter of the handover forwarder has no effect.

*  Radio link failure is detected only by RLC acknowledged mode; the radio
   link monitoring of TS 38.331 5.3.10 (T310, N310, N311) is not modeled. A
   detected failure releases both ends of the link at once.

*  RRC re-establishment always returns to the same cell: cell selection
   during T311 is not modeled. Without re-establishment (``t311 = 0s``), the
   leg's traffic stays stopped while it remains in the cell; the transition to
   RRC_IDLE and the reconnection when new data arrives are not modeled.

*  For a D2D-capable UE, the source base station switches the UE's D2D flows
   to the infrastructure path before the handover command, and the target
   evaluates the D2D mode again after the UE arrives (see :doc:`d2d`).
