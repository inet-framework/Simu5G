Dual Connectivity
=================

Overview
--------

Dual connectivity (3GPP TS 37.340) lets a UE exchange data through two base
stations at once: a *master node* (MN) that anchors the connection towards
the core network, and a *secondary node* (SN) reached over the X2
interface. Simu5G supports the E-UTRA/NR flavors of the architecture:

*  *EN-DC* -- LTE eNB as master, gNB as secondary, EPC core: the classic
   Non-Standalone 5G deployment;
*  *NE-DC* -- gNB as master, eNB as secondary.

The deployment side -- example networks, X2 wiring, the ``masterId``
parameter on the secondary node, and the UE's two serving-node parameters
-- is described in :doc:`networks`. This chapter covers how user
traffic actually uses the two legs.

.. figure:: images/dual_stack.png
   :align: center
   :figwidth: 80.0%

Bearer types
------------

Under dual connectivity, each bearer of a UE is carried in one of three
ways, chosen by the ``legs`` field of its bearer definition
(:doc:`bearers`):

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
