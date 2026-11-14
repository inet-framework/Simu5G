Introduction
============

What is Simu5G
--------------

Simu5G is a simulation model library for 5G New Radio (NR) and LTE/LTE-A
networks, built on the `OMNeT++ <https://omnetpp.org>`__ simulation framework
and the `INET Framework <https://inet.omnetpp.org>`__. It is the evolution of
the `SimuLTE <https://simulte.omnetpp.org>`__ 4G simulator, developed by the
same research group, and it simulates pure LTE networks as well as mixed 4G/5G
deployments.

Simu5G models a mobile network after the 3GPP system architecture: the UE, the
base stations (eNB and gNB), the user plane functions and the control plane
of the core network, and the radio interface between them. Because Simu5G
nodes are INET network nodes, a scenario can freely combine cellular elements
with the rest of the INET model library: routers, Ethernet links, wireless
LANs, TCP/UDP applications, mobility models, and so on.

The main features are:

*  The NR and LTE radio protocol stacks at the UE and the base station: SDAP,
   PDCP with header compression (ROHC), RLC in unacknowledged and
   acknowledged mode, MAC with HARQ and a set of pluggable schedulers, and a
   physical layer modeled at the level of transport blocks and resource
   blocks (:doc:`part-radio-stack`)

*  5G standalone and LTE deployments, and dual connectivity between LTE and NR
   in both directions, EN-DC and NE-DC (:doc:`deployments`)

*  PDU sessions of the IPv4, IPv6, IPv4v6, Ethernet and Unstructured types
   (:doc:`pdu-sessions`, :doc:`ethernet-sessions`)

*  QoS flows mapped to data radio bearers through SDAP, with bearer profiles
   from the standardized 5QI and QCI characteristics (:doc:`qos`)

*  The control plane modeled through its entities: attach, session
   establishment, handover over X2, radio link failure and RRC
   re-establishment (:doc:`part-control-plane`)

*  Carrier aggregation, numerologies 0 to 4, FDD and TDD (:doc:`spectrum`)

*  A radio medium with channel models after 3GPP TR 38.901, TR 36.873 and
   TR 36.814 (path loss per scenario, line of sight, shadowing, fading),
   inter-cell interference, and a reception trace for debugging
   (:doc:`radio-medium`)

*  CoMP coordinated scheduling over X2 (:doc:`x2`)

*  An ETSI Multi-access Edge Computing (MEC) model, with a MEC orchestrator,
   MEC hosts and MEC services (:doc:`mec`)

*  Background cells, background UEs and external cells, which load the radio
   resources and create interference at a small cost (:doc:`background`)

*  A real-time emulation mode, in which real applications exchange their
   traffic through a simulated network (:doc:`emulation`)

*  Network-assisted device-to-device (D2D) communication, one-to-one and
   one-to-many, kept as a legacy feature (:doc:`d2d`)

Scope and modeling philosophy
-----------------------------

Simu5G is built for end-to-end performance studies of mobile networks: radio
resource management, scheduling, QoS, interference, mobility, and the
performance of applications over a 5G network. Its level of abstraction is
chosen for that purpose, and it is the same throughout the model:

*  **The user plane is modeled packet by packet**, through every protocol
   layer and every GTP-U tunnel, with the sizes of the headers that the
   protocols add.

*  **The control plane is modeled through its entities and procedures, not
   through its messages.** The RRC of every node, the control plane of the
   core network and the control of the user plane nodes are modules, and the
   procedures run between them, each step at the node that performs it in
   3GPP. The signaling messages (RRC, NAS, NGAP or S1AP, XnAP or X2AP, PFCP)
   are direct calls named after the messages they stand for; where the
   duration of a procedure matters, as for a handover, it is modeled with
   timers (see :doc:`architecture` and :doc:`control-plane`).

*  **The physical layer is an abstraction.** Instead of simulating OFDM
   symbols, Simu5G computes the SINR of every resource block in the radio
   medium and decides the reception of a transport block from BLER curves.
   This reduces the computational cost by orders of magnitude; the SINR
   distribution of the model has been calibrated against the 3GPP guidelines
   of RP-180524 (see :doc:`phy` and :doc:`radio-medium`).

*  **Configuration replaces negotiation.** The bearers, their QoS profiles and
   the classification rules come from the configuration, not from the outcome
   of a signaling exchange, and a UE has one PDU session (see :doc:`qos` and
   :doc:`pdu-sessions`).

Every chapter of this manual ends with a Fidelity section that says, for its
area, what is modeled in detail, what is abstracted, what is left out, and
where the model knowingly deviates from the specifications.

Relationship to OMNeT++ and INET
--------------------------------

Simu5G is not a standalone program: it is a model library that requires
OMNeT++ and builds on INET. Working knowledge of both is assumed throughout
this manual. In particular, new users should first become familiar with:

*  the OMNeT++ *Simulation Manual* and IDE documentation (NED language, INI
   files, running simulations, recording and analyzing results);

*  the INET *User's Guide* (network nodes, applications, transport and
   network protocols, mobility).

This manual concentrates on what Simu5G adds on top: the cellular network
nodes, the NR and LTE protocol stacks, the core network, and the features
built around them.

About this manual
-----------------

This manual is written for users who assemble and run simulations using the
components provided by Simu5G, configuring them via NED and INI files. Its
spine is the 3GPP system architecture, so a reader who knows 3GPP can look a
topic up where the standard puts it. It is organized in parts:

*  **Getting Started**: this introduction, installation and usage, and
   :doc:`building-a-scenario`, a walkthrough that builds a complete simulation
   step by step;

*  **System Architecture**: the architecture as Simu5G models it, the
   deployment options, the UE, the base station, the core network and the
   cellular NIC;

*  **Sessions and QoS**: PDU sessions, Ethernet sessions, QoS flows and
   bearers;

*  **Radio Protocol Stack**: PDCP, RLC, MAC and the PHY abstraction;

*  **Control Plane and Procedures**: the control plane, handover, dual
   connectivity, X2 and CoMP;

*  **Radio**: the spectrum and the radio medium;

*  **Scenario Components**: applications, background traffic, mobility, MEC
   and D2D;

*  **Running and Evaluating**: results and emulation.

The chapters share a structure. The Overview first describes the component as
3GPP defines it, then what Simu5G models of it; the body explains how it works
in Simu5G and how to configure it, with examples taken from the shipped
simulations; and the Fidelity section closes the chapter. The chapters do not
restate the 3GPP specifications, but cite the ones they follow.

The chapters explain the parameters that matter for each task, but they do not
list every parameter: the NED Reference, generated from the source code, is
the complete reference of every module and parameter, and the module names in
this manual link to it. For complete, runnable scenarios, see the example
simulations under the ``simulations/`` directory of the Simu5G source tree,
and the showcases and tutorials on the Simu5G website.
