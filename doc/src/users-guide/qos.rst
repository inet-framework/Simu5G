QoS Flows and Bearers
=====================

Overview
--------

In the 5G system (3GPP TS 23.501 5.7), the QoS flow is the finest granularity
of QoS: a flow within a PDU session, identified by its QoS Flow Identifier
(QFI). Each QoS flow has a QoS profile, whose 5QI points to standardized QoS
characteristics -- resource type (GBR or non-GBR), priority level, packet
delay budget, packet error rate -- and which, for a GBR flow, adds the
guaranteed and maximum flow bit rates. Packets are classified onto QoS flows
at the two ends of the session: in the downlink by the UPF, with the packet
detection rules the SMF installs, and in the uplink by the UE, with the QoS
rules it receives at session establishment or derives from the downlink
traffic (reflective QoS). In the radio access network, the QoS flows ride on
data radio bearers (DRBs). The Service Data Adaptation Protocol (SDAP; TS
37.324) sits above PDCP and has one entity per PDU session. Its functions are
the mapping between QoS flows and DRBs, and the marking of packets with their
QFI, in the SDAP header. Several QoS flows, even with different QoS profiles,
can share a DRB, and how many DRBs a UE has and which flows they carry is the
gNB's decision. In LTE (TS 23.401), the unit of QoS is the EPS bearer,
identified by its QCI, and packets are mapped onto bearers directly by the
packet filters of the core network and of the UE; there is no SDAP.

Simu5G models both arrangements. In the 5G one, the downlink is classified at
the anchor UPF's traffic flow filter and the uplink at the UE's QoS flow
classifier, both with rules from the configuration; SDAP maps the QFIs to
DRBs, adds and removes the SDAP header, and supports reflective QoS at the UE.
In the LTE one, packet filters select the bearer directly. The QoS
characteristics of the standardized 5QI and QCI values 1 to 9 are available as
predefined profiles; they set the RLC mode and the logical channel group of a
bearer, and its weight in the QoS-aware scheduler. Not modeled are QoS
characteristics per flow (they belong to the DRB as a whole), the guaranteed
and maximum bit rates of GBR flows (``gbr`` is a flag only), and more than one
PDU session per UE.

The QoS configuration of a network is declarative. The network-level
:ned:`BearerConfigurator` module holds it: the bearer definitions, their QoS
profiles, and the classification rules. In a real network, these are
decisions of the SMF, of the RAN, and of the operator; in Simu5G they are
written once, in the ini file, and the control plane delivers each piece to
the module that acts on it -- the classification rules through the core
network's :ned:`CoreControl`, the bearers through the base station's
connection control (see :doc:`control-plane`). The :ned:`BearerConfigurator`
itself only holds the definitions; it neither delivers nor establishes
anything.

The rest of this chapter follows a packet: how it gets its QoS flow, how the
QoS flow reaches a bearer, where the bearers come from, and what a bearer's
QoS profile decides. The complete syntax of the definitions is in the NED
Reference of :ned:`BearerConfigurator`.

How a packet gets its QoS flow
------------------------------

Each direction is classified once, where the traffic enters the 5G system:

Downlink, at the core network
   The traffic flow filter (:ned:`TrafficFlowFilter`) of the user plane node
   where a downlink packet enters its tunnel -- the anchor :ned:`Upf`, a
   :ned:`PgwStandard`, or the UPF of a MEC host -- assigns the packet's QFI. Its
   rules come from the ``dlQfiRules`` parameter of the
   :ned:`BearerConfigurator`, which :ned:`CoreControl` installs into every user
   plane node at initialization, through the node's
   :ned:`UserPlaneNodeControl`. This stands for the packet detection and QoS
   enforcement rules that the SMF installs into a UPF over N4. A packet that
   matches no rule gets QFI 0, the default QoS flow. The QFI travels with the
   packet through the tunnel to the base station, whose SDAP uses it.

Uplink, at the UE
   The UE's QoS flow classifier (:ned:`QosFlowClassifier`), which sits where
   uplink traffic enters the UE's NIC, assigns the QFI. Its rules come from
   the ``ulQfiRules`` parameter, which the control plane delivers to every UE
   with SDAP; they stand for the QoS rules that NAS signaling installs into a
   UE at session establishment. A packet that matches no rule is left
   unclassified, and SDAP then puts it on the QoS flow reflective QoS derived
   for it, or on the default QoS flow (QFI 0). An application can also set the
   QFI of its packets itself; the classifier leaves such packets alone. The
   base station passes the uplink QFI on into the tunnel towards the core
   network.

The rules of both tables are evaluated in order, and the first match wins. A
rule has a filter -- a packet name pattern such as ``"*VoIP*"``, or an
expression such as ``"expr(has(udp) && udp.destPort == 3000)"``, or nothing,
which matches every packet -- and assigns either a fixed QFI (``qfi``) or the
packet's DSCP value as the QFI (``dscpAsQfi``). A downlink rule can be scoped
to some of the user plane nodes (``node``), an uplink rule to some of the UEs
(``ue``). The two directions are authored separately: the same filter means
different things at the two ends, since a downlink packet's destination port
is the UE's and an uplink packet's is the server's.

The default of ``dlQfiRules`` is ``[{dscpAsQfi: true}]``: every downlink packet
gets its DSCP value as its QFI, so traffic sources that mark their packets
select their own QoS flows. The default of ``ulQfiRules`` is empty, which
leaves the uplink to reflective QoS and the default flow.

Reflective QoS
~~~~~~~~~~~~~~

With reflective QoS, the UE derives its uplink classification from the
downlink: when a downlink packet arrives with the reflective QoS indication in
its SDAP header, the UE records the QFI for the packet's IP flow, and sends the
uplink packets of the same flow, with source and destination swapped, on that
QFI. The learned mappings expire after a time without traffic (30 seconds by
default, the ``timeout`` of :ned:`ReflectiveQosTable`). Reflective QoS needs
an IP session and a bearer with an SDAP header. It is switched on at both
ends: the gNB's SDAP sets the indication, and the UE's NIC keeps the table of
learned mappings:

.. code:: ini

   *.gnb.cellularNic.sdap.useReflectiveQos = true
   *.ue[*].cellularNic.useReflectiveQos = true

When a packet has both a QFI from the UE's rules and a reflective match, the
rules win, unless ``reflectiveQosOverridesQfi`` is set in the UE's SDAP.

From QoS flow to bearer
-----------------------

SDAP (:ned:`NrSdap`) maps each packet's QFI onto a DRB, using the bearer
definitions: a definition lists the QFIs its bearer carries in its
``mappedQfis`` field. A QFI that no definition maps goes to the UE's *default
DRB*, the definition marked ``isDefault``. If none of a UE's definitions is
marked and the UE has a single static bearer, that bearer is the default; a UE
with several bearers must have one marked explicitly. A packet whose QFI is
neither mapped nor covered by a default bearer stops the simulation with an
error.

The base station's SDAP maps the downlink, the UE's SDAP the uplink, from the
same definitions, so both ends agree on the mapping by construction.

The SDAP header
~~~~~~~~~~~~~~~

The SDAP header carries the packet's QFI over the radio link, so that the
receiver can tell the QoS flows of a bearer apart. It is needed only where the
bearer alone does not determine the QFI: on a bearer that carries more than
one QFI, and on the default bearer, which catches the unmapped ones. Simu5G
puts the header exactly on those bearers, and leaves it off the others.

The default bearer's header can be suppressed with ``suppressSdapHeader``,
which saves the header's byte on a bearer that in fact carries a single QoS
flow. The receiver then assigns every packet the bearer's one QFI, and the
sender checks every packet against it: a second QoS flow on such a bearer
stops the simulation with an error.

Whether a stack has SDAP at all follows from its core network: the networks
with a 5G core set ``hasSdap`` for every cellular NIC, including the NE-DC
network, whose master gNB connects to a 5G core; the networks with an EPC,
including EN-DC, have no SDAP (see :doc:`deployments`).

Where bearers come from
-----------------------

A bearer can be established in two ways. It can be established explicitly,
which is how a real network does it: the base station sets up the bearers for
a UE's QoS flows when the UE's session is established or modified. Or it can
be established on demand, when the first packet that needs it finds none. On
demand establishment is a modeling convenience: it gives a simulation bearers
for its traffic without describing each of them in advance. Today, explicit
bearers are the ones listed in the configuration, and they are established
when the UE's session is set up at its base station, normally at
initialization. The control plane is not tied to that moment: it can
establish a bearer from its definition at any time during the run, and it
releases bearers, with their entities and DRB identities, when a UE leaves a
cell.

.. TODO: ScenarioManager commands to establish and release a UE's bearers at
   run time (short-term TODO); describe them here once they exist.

The definitions are in two tables of the :ned:`BearerConfigurator`:

``staticDrbs``
   Bearers established explicitly, when the UE's session is set up: one entry
   per bearer, which names its UE (a module path pattern such as
   ``"ue[*]"``, so one entry can describe the bearer of many UEs) and its DRB
   identity (``drbId``, 1 to 32).

``onDemandDrbs``
   Bearers established when traffic first needs them. The entries have no
   ``drbId``, which is assigned at establishment, and their ``ue`` field is
   optional. A ``"5gc"`` entry is matched by the QFI of the first packet that
   finds no bearer, and the later QFIs of its ``mappedQfis`` join the same
   bearer.

Matching tries the ``staticDrbs`` entries first, then the ``onDemandDrbs``
entries, each in table order, and the first match wins.

The base station that serves the UE establishes the bearers. It reads the
definitions from the :ned:`BearerConfigurator` and installs each bearer's
entities at itself, at the UE, and at a dual connectivity secondary node (see
:doc:`control-plane`). An on-demand bearer is established when the data path
of either end finds no bearer for a packet and asks its node's control plane
for one. Because a definition names its UE by module path, not by node id, it
follows the UE from cell to cell: after a handover, the target base station
establishes the UE's bearers from the same definitions.

On-demand establishment can be switched off, so that the static bearers are
the only ones, and a packet that finds no bearer is an error. This is a check
that a static configuration covers all the traffic:

.. code:: ini

   *.gnb.cellularNic.sdap.establishBearersOnDemand = false
   *.ue[*].cellularNic.sdap.establishBearersOnDemand = false

(For a stack without SDAP, the parameter is that of ``ip2nic``.)

The default of ``onDemandDrbs`` lets a simulation run without any bearer
configuration. For a stack with SDAP, it holds a single default bearer with a
suppressed SDAP header, so an unconfigured run carries the default QoS flow
only; a run whose traffic shows up with several QFIs stops with an error,
asking for configured bearers. For a stack without SDAP, it holds catch-all
bearers selected by packet name: ``"VoIP*"`` packets in LCG 0, ``"VoD*"``
packets in LCG 1, ``"gaming*"`` packets in LCG 2, and everything else in LCG 3,
all in RLC unacknowledged mode. Setting the parameter replaces the whole
default.

What a bearer's QoS profile decides
-----------------------------------

A bearer definition can carry a QoS profile: the resource type (``gbr``), the
priority level (``qosPriorityLevel``), the packet delay budget
(``packetDelayBudget``, in milliseconds) and the packet error rate
(``packetErrorRate``). Profiles that several definitions share are named once
in the ``drbProfiles`` parameter and referenced by the ``profile`` field of a
definition. The rows 1 to 9 of the standardized 5QI table (TS 23.501 Table
5.7.4-1) and of the QCI table (TS 23.203 Table 6.1.7-A) are predefined, as
``"5qi-1"`` to ``"5qi-9"`` and ``"qci-1"`` to ``"qci-9"``, and need no
definition.

The profile decides how the radio access network treats the bearer:

RLC mode
   A bearer whose packet error rate is at most ``amPerThreshold`` (10\ :sup:`-4`
   by default) uses RLC acknowledged mode, whose retransmissions can meet such
   an error rate where HARQ alone cannot; the others use unacknowledged mode.
   A definition can state ``rlcMode`` to override this.

Logical channel group
   The priority level is bucketed into the bearer's logical channel group
   (LCG), 0 to 3, by the bounds in ``lcgPriorityBounds`` (``[25, 50, 75]`` by
   default: priorities below 25 go to LCG 0, and so on). The UE reports its
   buffer per LCG, and Simu5G's MAC serves the lower LCGs first (see
   :doc:`mac`). A definition can state ``lcg`` to override this.

Scheduling weight
   With the QoS-aware proportional fair scheduler (``"QOS_PF"`` as the
   base station's ``schedulingDisciplineDl`` or ``schedulingDisciplineUl``), a
   bearer's share grows with a weight computed from its profile: inversely
   proportional to its priority level, larger for a GBR bearer, and larger for
   a tight packet delay budget. A bearer without a QoS profile is scheduled as
   in plain proportional fair. Only the ratios of the priority levels matter,
   so the QCI scale (1 to 9) and the 5QI scale (10 to 90) rank alike; the LCG
   bucketing, however, uses the absolute values, so the two scales should not
   be mixed in one network.

Header compression
   A bearer of an IP session gets ROHC header compression if its definition
   says ``rohc: true`` (or names the ROHC profiles to use), or if it uses one of
   the QoS profiles listed in ``rohcForDrbProfiles`` -- by default the voice
   rows, ``"5qi-1"`` and ``"qci-1"``. How ROHC works is described in
   :doc:`pdcp`.

Bearers without SDAP
--------------------

A stack without SDAP -- an LTE UE, or any UE of an EPC network -- has EPS
bearers. Its bearer definitions say ``coreNetwork: "epc"`` and are selected by
packet filters instead of QFIs: the ``filters`` field lists packet name
patterns or ``expr(...)`` expressions, and a definition with ``isDefault``
takes the traffic that no filter matches. The filters are matched by the NIC's
:ned:`Ip2Nic` against the first packet of a flow, and the flow stays on the
bearer that packet selected.

The ``coreNetwork`` field of every definition must match the stack it is
installed into: ``"5gc"`` for a stack with SDAP, ``"epc"`` for one without. A
``ue`` pattern that covers UEs of both kinds skips the incompatible ones, but
an entry whose pattern matches no compatible UE is an error.

Modules
-------

:ned:`BearerConfigurator`
   Holds the bearer definitions, the QoS profiles and the classification
   rules of the network.

:ned:`TrafficFlowFilter`
   Classifies downlink packets onto QoS flows where they enter the tunnels of
   the core network.

:ned:`QosFlowClassifier`
   Classifies a UE's uplink packets onto QoS flows.

:ned:`NrSdap`
   The SDAP layer: maps QoS flows onto DRBs, adds and removes the SDAP header,
   and applies reflective QoS at the UE.

:ned:`ReflectiveQosTable`
   The UE's table of the mappings learned by reflective QoS.

Configuration examples
----------------------

**Four QoS flows on two bearers, marked by the traffic sources.** The
``Standalone`` configuration of ``simulations/nr/standalone_drb`` names two
QoS profiles and maps QFIs 1 and 2 to one bearer, 3 and 4 to the other. The
default downlink rule takes the QFI from the DSCP the servers' applications
set (``*.server.app[0].dscp = 1`` and so on, in ``VoIP-DL-MultiQfi``):

.. code:: ini

   *.bearerConfigurator.drbProfiles = {
       voice:  {gbr: true, packetDelayBudget: 100, packetErrorRate: 1e-2,
                qosPriorityLevel: 2},
       gaming: {gbr: true, packetDelayBudget: 50,  packetErrorRate: 1e-3,
                qosPriorityLevel: 1}
       }
   *.bearerConfigurator.staticDrbs = [
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 1, mappedQfis: [1, 2],
        profile: "voice", isDefault: true},
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 2, mappedQfis: [3, 4],
        profile: "gaming"}
       ]
   *.gnb.cellularNic.mac.schedulingDisciplineDl = "QOS_PF"
   *.gnb.cellularNic.mac.schedulingDisciplineUl = "QOS_PF"

**The same with standardized profiles.** ``VoIP-DL-MultiQfi-5qiProfiles``
uses the predefined 5QI rows instead, and lets the RLC mode and the LCG follow
from them:

.. code:: ini

   *.bearerConfigurator.staticDrbs = [
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 1, mappedQfis: [1, 2],
        profile: "5qi-1", isDefault: true},
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 2, mappedQfis: [3, 4],
        profile: "5qi-3"}
       ]

**Classifying the uplink by destination port.** In ``VoIP-UL-FilterRules``,
the applications mark nothing, and each UE classifies its flows by the
server port they go to:

.. code:: ini

   *.bearerConfigurator.ulQfiRules = [
       {ue: "ue[0]", filter: "expr(udp.destPort == 3000)", qfi: 1},
       {ue: "ue[0]", filter: "expr(udp.destPort == 3001)", qfi: 2},
       ...]

**One application's own QoS flow at a MEC host.** The MEC examples
(``simulations/nr/mec/singleMecHost``) give the MEC application's downlink
QFI 1 at the MEC host's UPF, everything else there QFI 0, and keep the DSCP
rule for the other nodes. A table that is set replaces the default, so the
catch-all is restated at the end:

.. code:: ini

   *.bearerConfigurator.dlQfiRules = [
       {node: "**.upf_mec", filter: "WarningAlert*", qfi: 1},
       {node: "**.upf_mec", qfi: 0},
       {dscpAsQfi: true}]
   *.bearerConfigurator.ulQfiRules = [{filter: "WarningAlert*", qfi: 1}]

**A bearer created by its first packet.** ``VoIP-DL-MultiQfi-OnDemandDrb``
keeps the first bearer static and leaves the second to an on-demand
definition, which the first packet of QFI 3 or 4 establishes:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [
       {coreNetwork: "5gc", ue: "ue[*]", mappedQfis: [3, 4], profile: "gaming"}
       ]

**An LTE bearer by destination port.** ``SingleCell-DL-OnDemandDrb`` in
``simulations/lte/tutorial`` gives the traffic to port 3000 its own bearer,
and everything else the default one:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [
       {coreNetwork: "epc", ue: "ue[*]",
        filters: ["expr(udp.destPort == 3000)"], lcg: 0, rlcMode: "UM"},
       {coreNetwork: "epc", isDefault: true, lcg: 3, rlcMode: "UM"}
       ]

Statistics
----------

QoS has no statistics of its own. To compare the treatment of QoS flows,
record the end-to-end statistics of the applications, with one application per
flow, as the ``simulations/nr/standalone_drb`` configurations do.

Example simulations
-------------------

*  ``simulations/nr/standalone_drb``: static and on-demand bearers, downlink
   and uplink classification, standardized profiles, the QoS-aware scheduler,
   and IPv6 sessions (the ``Ipv6`` configurations);

*  ``simulations/nr/mec``: per-application QoS flows at a MEC host;

*  ``simulations/lte/tutorial``: EPS bearers selected by packet filters;

*  ``simulations/nr/ethernet``: classification of Ethernet frames by their
   VLAN priority (see :doc:`ethernet-sessions`).

Fidelity
--------

*  QoS characteristics belong to the bearer, not to the QoS flow: flows with
   different profiles can share a bearer, but the bearer is treated by one
   profile.

*  GBR is a flag. The guaranteed and maximum flow bit rates (GFBR, MFBR) are
   not modeled, and a GBR bearer is not guaranteed any rate; the flag only
   raises its weight in the QoS-aware scheduler.

*  The mapping of QoS flows to bearers comes from the configuration. In 3GPP
   it is the gNB's decision and can change during the session; in Simu5G it
   is fixed by the bearer definitions.

*  On-demand establishment is a simulation shortcut: in 3GPP, the first
   uplink packet of an unmapped QoS flow travels on the default bearer, and
   the gNB may set up a new bearer afterwards. In Simu5G, the bearer is
   established before the packet is sent.

*  A UE has one PDU session, so the QFI mappings and the default bearer are
   per UE, while TS 37.324 scopes them per session.

*  Without SDAP, a flow is identified by its source and destination address
   and its DSCP, and the packet filters are evaluated on its first packet
   only. Two applications between the same pair of hosts with the same DSCP
   share a bearer, even if their ports would match different filters.

*  The signaling that carries the QoS configuration (NAS, NGAP, RRC, PFCP) is
   not simulated: the configuration is delivered by direct calls at
   initialization and at bearer establishment.

*  The QoS-aware scheduler's weights (the GBR factor and the delay budget
   factors) are built into the scheduler and are not parameters.
