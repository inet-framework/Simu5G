Networks and Deployments
========================

Overview
--------

This chapter describes how cellular networks are assembled from Simu5G
components: which modules every network needs, how base stations, UEs and
core network nodes are connected, and how the standard 3GPP deployment
options map to Simu5G network descriptions. The internals of the individual
node types are covered in :doc:`ue`, :doc:`base-station` and :doc:`core-network`.

The example networks referenced throughout this chapter can be found under
``simulations/lte/networks/`` and ``simulations/nr/networks/`` in the Simu5G
source tree; they are good starting points for custom scenarios.

Anatomy of a Simu5G network
---------------------------

Every Simu5G network contains, besides the nodes themselves, a small set of
infrastructure modules instantiated once at the network level:

``binder`` (``Binder``)
   A directory of all cellular nodes in the network: node identifiers, cell
   associations, dual connectivity relations, X2 peerings. Modules of the
   cellular stack look it up via their ``binderModule`` parameter.

``bearerConfigurator`` (``BearerConfigurator``)
   The authority for bearer and QoS-flow configuration (see
   :doc:`qos`).

``carrierAggregation`` (``CarrierAggregation``)
   Describes the component carriers available in the network (see
   :doc:`spectrum`).

``channelControl`` (``LteChannelControl``)
   Keeps track of the positions of radio-capable nodes, so that receivers
   can determine which transmissions affect them.

``configurator`` (INET's ``Ipv4NetworkConfigurator``)
   Assigns IP addresses and routes, exactly as in any INET network.

A typical network definition therefore begins like this:

.. code:: ned

   network SingleCell_Standalone
   {
       submodules:
           channelControl: LteChannelControl;
           binder: Binder;
           bearerConfigurator: BearerConfigurator;
           carrierAggregation: CarrierAggregation;
           configurator: Ipv4NetworkConfigurator;
           ...
   }

The rest of the network consists of Simu5G nodes (UEs, base stations, core
network nodes) and ordinary INET nodes (routers, servers, switches), wired
together with standard INET link types.

5G Standalone (SA) deployment
-----------------------------

In a Standalone deployment (3GPP TR 38.801, option 2), gNBs connect directly
to the 5G core, represented in Simu5G by ``Upf`` modules. The example
network ``SingleCell_Standalone`` (in ``simulations/nr/networks/``) contains
one gNB, an intermediate UPF (``iUpf``) and the UPF that acts as the
entry/exit point of the mobile network (``upf``), which connects to the data
network -- a router and a server:

.. code:: ned

   upf: Upf;
   iUpf: Upf;
   gnb: gNodeB;
   ue[numUe]: NrUe;

User traffic between the gNB and the ``upf`` exit point is carried in GTP
tunnels; any number of intermediate UPFs can be placed along the path, and
they behave like IP routers forwarding the tunneled traffic (see
:doc:`core-network`).

The multicell counterpart, ``MultiCell_Standalone``, instantiates two gNBs
connected to the same core and peered with each other via the X2 interface,
enabling handover between them.

LTE deployment
--------------

Simu5G retains full LTE support inherited from SimuLTE. An LTE network uses
``eNodeB`` base stations, ``LteUe`` terminals, and a ``PgwStandard``
node as the (simplified) EPC PDN Gateway. The ``SingleCell`` and
``MultiCell`` networks in ``simulations/lte/networks/`` are the reference
examples; the multicell versions also demonstrate X2 wiring and, with
``ExtCell`` modules, lightweight external interference sources.

E-UTRA/NR Dual Connectivity (EN-DC) deployment
----------------------------------------------

In a Non-Standalone deployment (3GPP TR 38.801, option 3/3a), the gNB does
not connect to the core network directly: it acts as *secondary node* of an
LTE eNB (the *master node*), which is attached to the EPC. The two base
stations are connected via the X2 interface, and NR user traffic traverses
the eNB.

.. figure:: images/SA-ENDC.png
   :align: center
   :figwidth: 70.0%

The reference example is ``SingleCell_withSecondaryGnb``:

.. code:: ned

   pgw: PgwStandard;
   masterEnb: eNodeB;
   secondaryGnb: gNodeB;
   ue[numUe]: NrUe;

with the X2 connection:

.. code:: ned

   masterEnb.x2++ <--> Eth10G <--> secondaryGnb.x2++;

The master/secondary relationship itself is configured on the secondary
node, by setting its ``masterId`` parameter to the master's node id. UEs are
``NrUe`` modules -- dual-stack terminals that attach to the eNB with
their LTE leg and to the gNB with their NR leg (``servingNodeId`` and
``nrServingNodeId`` parameters, see :doc:`ue`).

How traffic is distributed over the two legs -- MCG, SCG and split bearers
-- is the subject of :doc:`dual-connectivity`. A multicell variant,
``MultiCell_withSecondaryGnb``, combines two EN-DC pairs in an X2 mesh.

NR-E-UTRA Dual Connectivity (NE-DC) deployment
----------------------------------------------

The mirror-image arrangement (option 4: gNB as master, eNB as secondary,
with a 5GC core) is available as the ``SingleCell_withSecondaryEnb`` example
network.

.. TODO: expand when the NE-DC/5GC rework lands; currently the example is
   interim and EPC-flavored in its bearer configuration.

X2 wiring rules
---------------

Base stations that need to communicate with each other -- for handover, dual
connectivity, or inter-cell interference coordination -- must be connected
through their ``x2`` gate vectors. Connections are point-to-point links (an
X2 "mesh" is a set of pairwise links), and each new connection uses the next
gate index on both sides:

.. code:: ned

   eNodeB1.x2++ <--> Eth10G <--> eNodeB2.x2++;

The X2 endpoints find each other automatically at initialization; the
per-peer applications and the SCTP-based X2 protocol stack inside the base
stations are described in :doc:`x2`.
