Bearers, QoS and SDAP
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

Since Simu5G does not simulate control-plane signaling, bearers are not
negotiated through message exchanges. Instead, the network-level
:ned:`BearerConfigurator` module owns the complete bearer and QoS
configuration and stands in for the corresponding control-plane functions
(the SMF's session management, the RAN's bearer setup decisions, and the
MME's EPS bearer handling). The configuration is declarative: a set of
JSON-style object parameters in the INI file describes the bearers and the
classification rules, and the control plane delivers each piece of it to the
module that acts on it.

The authoritative reference for every field mentioned in this chapter is the
banner comment of
``src/simu5g/corenetwork/bearerConfigurator/BearerConfigurator.ned``.

Bearer definitions
------------------

Bearers are described by two parameters of the ``BearerConfigurator``:

``staticDrbs``
   Bearers established up-front, at the end of network initialization. Each
   entry names the UE it belongs to and carries a fixed bearer id
   (``drbId``, 1..32).

``onDemandDrbs``
   Bearer *templates*: a bearer is created from the matching template when
   traffic first requires it. Entries follow the same schema as
   ``staticDrbs`` minus the ``drbId`` (ids are assigned at creation), and
   the ``ue`` field is optional -- a template without it applies to any UE.

A definition is a JSON-style object; the most important fields are:

``coreNetwork``
   ``"epc"`` or ``"5gc"`` -- whether the entry describes an EPS bearer or a
   5G DRB. This must match the kind of core the UE is attached to.

``filters``
   A list of packet name patterns; traffic matching any of them is carried
   by (or, for on-demand entries, triggers the creation of) this bearer.

``isDefault``
   Marks the catch-all entry for traffic not matched by any other filter.

``profile``
   The name of an entry in ``drbProfiles`` supplying QoS characteristics
   (see below); alternatively, the QoS fields (``gbr``,
   ``packetDelayBudget``, ``packetErrorRate``, ``qosPriorityLevel``) can be
   given inline.

``rlcMode``, ``lcg``
   RAN-side treatment: the RLC mode (``"UM"`` or ``"AM"``) used by the
   bearer's RLC entities, and the logical channel group used in buffer
   status reporting. Both are optional -- when absent, they are derived from
   the QoS profile (see `RAN defaults derived from QoS`_).

``mappedQfis``, ``suppressSdapHeader``
   5G entries only: which QoS flows the DRB carries, and whether SDAP
   headers are omitted on it (see `SDAP`_).

``legs``
   For dual-connectivity UEs, which leg or legs of the UE carry the bearer;
   split bearers and leg selection are described in
   :doc:`dual-connectivity`.

Traffic that reaches the stack without any matching bearer -- no static
bearer, no applicable on-demand template, and no default entry -- is a
configuration error and stops the simulation, in keeping with the fail-fast
philosophy of the model library.

The default value of ``onDemandDrbs`` contains catch-all entries for both
core network kinds plus a few filter-based examples, so simple simulations
run without any bearer authoring at all; any serious QoS study should
replace it.

Bearer profiles: QCI and 5QI
----------------------------

The ``drbProfiles`` parameter is a map of named QoS field groups that
bearer definitions can reference via their ``profile`` field. Profiles keep
the per-UE bearer lists short and make the QoS characteristics reusable.

Simu5G ships with predefined profiles named ``"qci-1"`` .. ``"qci-9"`` and
``"5qi-1"`` .. ``"5qi-9"``, carrying the standardized characteristics of
the corresponding 3GPP QoS classes (TS 23.203 for QCI, TS 23.501 for 5QI):
resource type (GBR or non-GBR), packet delay budget, packet error rate, and
priority level. User-defined profiles may add to these; redefining a
predefined name is an error.

Note that the profiles carry *specification-level* QoS characteristics
only. How the RAN acts on them -- RLC mode, logical channel group,
scheduling weight -- is operator policy, configured separately (see the
next section and :doc:`mac`).

RAN defaults derived from QoS
-----------------------------

When a bearer definition does not state ``rlcMode`` or ``lcg`` explicitly,
the ``BearerConfigurator`` derives them from the QoS profile:

*  ``rlcMode``: a profile whose ``packetErrorRate`` is at or below the
   ``amPerThreshold`` parameter (default ``1e-4``) gets acknowledged mode
   (``"AM"``), others get unacknowledged mode (``"UM"``) -- i.e., loss
   targets that plainly require retransmission get them.
*  ``lcg``: the ``qosPriorityLevel`` is bucketed into logical channel
   groups using the ``lcgPriorityBounds`` parameter (default
   ``[25, 50, 75]``): priority below the first bound maps to LCG 0, and so
   on; everything above the last bound maps to LCG 3.

Both knobs are deliberately simple, documented policies rather than 3GPP
mandates; override per-bearer values where they do not fit your scenario.

QoS flow classification
-----------------------

In 5G networks, packets must be assigned a QFI before SDAP can map them to
a DRB. Classification rules are also owned by the ``BearerConfigurator``
and delivered to the two kinds of evaluation sites:

``dlQfiRules``
   Downlink rules, evaluated where traffic enters the mobile network -- at
   the traffic flow filters of core-network tunnel entry points (UPFs, and
   the MEC host's UPF). Each rule is ``{node, filter, qfi}`` or
   ``{node, filter, dscpAsQfi}``; the first matching rule at a site wins.
   The default, ``[{dscpAsQfi: true}]``, simply adopts the packet's DSCP
   value as the QFI -- convenient for experiments where the traffic source
   marks its own packets.

``ulQfiRules``
   Uplink rules, evaluated by the QoS flow classifier in the UE. Entries
   are ``{ue, filter, qfi}`` (or ``dscpAsQfi``); the default is the empty
   list.

SDAP
----

The SDAP layer (3GPP TS 37.324) sits on top of PDCP and performs the
QFI-to-DRB mapping at the transmitter, using the ``mappedQfis`` field of
the bearer definitions. Whether a node has an SDAP layer at all is
determined by the ``hasSdap`` parameter of the cellular NIC, and follows
the standard: *standalone implies SDAP*. Every example network with a 5G
core sets it; EN-DC networks, whose bearers are EPS bearers, have no SDAP
layer.

Per-bearer, the presence of the SDAP header on the user plane can be
switched off with the ``suppressSdapHeader`` field -- mirroring the
standard's provision for header-less DRBs. The catch: a DRB without SDAP
headers cannot carry more than its one configured QoS flow, and the model
verifies this at transmission time.

D2D flows are a special case: they travel directly between UEs below the
IP layer and bypass SDAP entirely, in both infrastructure and direct mode
(see :doc:`d2d`).

Examples
--------

A static 5G bearer with an explicit profile and QoS flow mapping:

.. code:: ini

   *.bearerConfigurator.staticDrbs = [ \
       { coreNetwork: "5gc", ue: "ue[0]", drbId: 1, profile: "5qi-1", \
         mappedQfis: [1] }, \
       { coreNetwork: "5gc", ue: "ue[0]", drbId: 2, isDefault: true } ]

On-demand VoIP treatment for every UE in an LTE network:

.. code:: ini

   *.bearerConfigurator.onDemandDrbs = [ \
       { coreNetwork: "epc", filters: ["VoIP*"], profile: "qci-1" }, \
       { coreNetwork: "epc", isDefault: true } ]

Ready-to-run configurations using authored bearers can be found under
``simulations/nr/standalone_drb/``.

.. TODO: flesh out with a downlink QFI classification example (dlQfiRules
   with explicit node/filter/qfi entries) once the qfiRules examples settle.

The BearerConfigurator
----------------------

The ``BearerConfigurator`` (``src/simu5g/corenetwork/bearerConfigurator/``)
is the single authority for bearer and QoS configuration: which data radio
bearers exist or may be created on demand, what their QoS characteristics
are, and how packets are classified onto QoS flows. Its configuration is
expressed in a few JSON-style object parameters (``staticDrbs``,
``onDemandDrbs``, ``drbProfiles``, ``dlQfiRules``, ``ulQfiRules``) and is
delivered to the evaluation sites in the network at initialization. It is
the subject of :doc:`qos`.
