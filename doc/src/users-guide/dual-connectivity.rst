Dual Connectivity
=================

Overview
--------

Multi-radio dual connectivity (MR-DC; 3GPP TS 37.340) lets a UE use the radio
resources of two base stations at once. The master node (MN) holds the UE's
control-plane connection to the core network, and the secondary node (SN) is
connected to the master over Xn, or over X2 when the master is an eNB. Each
node serves the UE with a cell group: the master cell group (MCG) and the
secondary cell group (SCG). The flavors are named after the technologies of
the two nodes and the core network: EN-DC (an LTE master and an NR secondary,
with an EPC), NGEN-DC (the same with a 5G core), NE-DC (an NR master and an
LTE secondary, with a 5G core) and NR-DC (two NR nodes). The main functions
of MR-DC are the bearer types, the bearer termination, the routing of a
split bearer's PDUs over the two legs, and the procedures that add, change and
release the secondary node. A bearer is an MCG bearer, an SCG bearer, or a
split bearer that uses both cell groups, and its PDCP terminates at either node
(MN-terminated or SN-terminated). For a split bearer, the uplink stays on a
primary path until the data waiting in the UE exceeds a split threshold, and
the downlink split is decided by the node that holds the PDCP, with flow
control over Xn (TS 38.425).

Simu5G models EN-DC and NE-DC. The secondary node is paired with its master
by configuration, and the master installs the secondary's part of each bearer.
All three bearer types are available, chosen in the bearer's definition, and
every bearer is MN-terminated: its PDCP entity is at the master, and a relay
entity at the secondary carries its PDUs to and from the master over X2. A
split bearer's uplink follows the primary path and the data split threshold,
and above the threshold uses the less loaded leg or a configurable
expression; its downlink takes the leg a configurable per-PDU expression
chooses, a Simu5G extension, or stays on the primary path. SN-terminated
bearers and the downlink flow control over Xn are not modeled. NR-DC is not
possible, because a UE has a single NR leg, and NGEN-DC has no reference
network.

The deployment side -- example networks, X2 wiring, the ``masterId``
parameter on the secondary node, and the UE's two serving-node parameters
-- is described in :doc:`deployments`. This chapter covers how user
traffic actually uses the two legs.

.. figure:: images/dual_stack.png
   :align: center
   :figwidth: 80.0%

Bearer types
------------

Under dual connectivity, each bearer of a UE is carried in one of three
ways, chosen by the ``legs`` field of its bearer definition
(:doc:`qos`):

*MCG bearer*
   The bearer uses the master's cell group only -- the classic single-leg
   case.

*SCG bearer*
   The bearer's radio leg is the secondary's cell group. The bearer remains
   anchored (PDCP-terminated) at the master, which relays its PDUs to and
   from the secondary over X2.

*Split bearer*
   The bearer may use both legs: its PDCP entity at the master feeds both
   its own RLC and, over X2, the secondary's RLC, and the peer entity at
   the UE merges and reorders the two PDU streams.

How a split bearer distributes traffic is configured per bearer:

*  in the downlink, a *leg selection* policy (``dlLegSelection``) decides
   per PDU which leg to use;
*  in the uplink, the standard's ``ul-DataSplitThreshold`` mechanism
   applies (``ulDataSplitThreshold`` and ``ulLegSelection`` fields): below
   the threshold, PDUs follow the primary path (``primaryPath``); above
   it, the UE may submit to either leg.

.. TODO: document the available leg selection policies and their exact
   field names/values once the leg-steering reconciliation lands.

What the model does under the hood
----------------------------------

At the master, split-bearer PDCP PDUs pass through a leg splitter into
either the local RLC or the ``DcMux``/``DcX2Forwarder`` pair, which
tunnels them over the X2 interface; at the secondary, a lightweight relay
entity stands in for the bearer's PDCP and hands the PDUs to the local
RLC. In the receive direction the same path is walked in reverse, and a
leg joiner feeds the merged stream into the NR PDCP receive entity, whose
reordering window restores sequence (its parameters are described in
:doc:`pdcp`).

The upshot for the user: apart from the bearer definition fields above and
the reordering parameters, dual connectivity needs no per-module
configuration -- establishing a bearer with two legs wires all of this up
automatically.

Example simulations
-------------------

*  ``simulations/nr/dualConnectivity`` -- single EN-DC pair, the place to
   start;
*  ``simulations/nr/dualConnectivity_multicell`` -- two EN-DC pairs in an
   X2 mesh, with handover.

.. TODO: NE-DC example status: the SingleCell_withSecondaryEnb network is
   currently EPC-flavored (EPS bearers, no SDAP); rewrite this section
   after the 5GC-flavored redo.
