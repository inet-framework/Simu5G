Building a Scenario
===================

Overview
--------

This chapter builds a complete simulation step by step: a 5G standalone cell
with one UE that receives a constant bit rate stream from a server in the data
network. Each step adds one part of the configuration, explains what it does,
and points to the chapter that covers its area in depth. The result is a
working ``omnetpp.ini`` that you can extend toward your own study.

The walkthrough follows the ``Single-UE`` configuration of
``simulations/nr/tutorial/omnetpp.ini``; the same scenario is the first step of
the NR tutorial on the Simu5G website. Steps 1 to 4, 7 and 8 use its lines as
they are. Steps 5 and 6 add a QoS configuration and a channel model choice
that the shipped configuration leaves at their defaults; those lines are
marked as additions.

The ini lines of the steps go into one file. The first step's lines belong to
the ``[General]`` section, which holds what every configuration of the file
shares; the others to a ``[Config Single-UE]`` section.

Step 1: The network
-------------------

The network is :ned:`SingleCell_Standalone`, the reference network of 5G
standalone deployments (see :doc:`deployments`). It contains one gNB
(``gnb``), ``numUe`` UEs of type :ned:`NrUe` (``ue[]``), a 5G core with an
anchor UPF (``upf``) and an intermediate UPF (``iUpf``), and a data network of
a router and a server (``server``). It also contains the network-level modules
that every Simu5G network has: the Binder, the core network's control plane
(``coreControl``), the bearer configuration (``bearerConfigurator``), the
carriers (``carrierAggregation``), the radio medium (``radioMedium``) and INET's
IP address configurator (see :doc:`architecture`).

.. code:: ini

   [General]
   network = simu5g.simulations.nr.networks.SingleCell_Standalone
   sim-time-limit = 10s
   warmup-period = 1s
   seed-set = ${repetition}

   *.configurator.config = xmldoc("./demo.xml")

   **.mobility.constraintAreaMaxX = 1000m
   **.mobility.constraintAreaMaxY = 1000m
   **.mobility.constraintAreaMinX = 0m
   **.mobility.constraintAreaMinY = 0m
   **.mobility.constraintAreaMinZ = 0m
   **.mobility.constraintAreaMaxZ = 0m
   **.mobility.initFromDisplayString = false

The simulation runs for 10 seconds, of which the first second is a warm-up
period that the statistics leave out. The IP address configurator takes its
addressing plan from ``demo.xml``, next to the ini file. The mobility lines
define a 1 km by 1 km floor plan and tell the nodes not to take their
positions from the network's graphical layout: every position is set below.

The tutorial's ``[General]`` section has a few more lines. Two name the
result files (see step 7), and three concern link adaptation:
``**.amcType = "NrAmc"``, ``**.targetBler = 0.01``, the error rate the CQI
choice aims for, and ``**.blerShift = 5``, which shifts the SINR at which the
BLER curves are read by 5 dB (see :doc:`phy` and :doc:`mac`).

Step 2: Placing and attaching the nodes
---------------------------------------

The gNB and the UE get their positions, and the UE's NR leg is attached to the
gNB:

.. code:: ini

   [Config Single-UE]
   *.gnb.mobility.initialX = 500m
   *.gnb.mobility.initialY = 500m

   *.numUe = 1
   *.ue[*].mobility.initialX = 500m
   *.ue[*].mobility.initialY = 900m

   *.ue[*].servingNodeId = 0
   *.ue[*].nrServingNodeId = 1

The UE is 400 m from the gNB. An :ned:`NrUe` has an LTE and an NR leg; in a
standalone network only the NR leg is used, so ``servingNodeId`` (the LTE
leg's cell) is 0, detached, and ``nrServingNodeId`` names the gNB by its node
id. The gNB is the network's first base station, so its node id is 1 (see
:doc:`architecture` for how node ids are assigned, and :doc:`ue` for cell
association).

The UE does not move: a UE's default mobility is stationary. To make it move,
choose any INET mobility model, for example:

.. code:: ini

   *.ue[*].mobility.typename = "LinearMobility"
   *.ue[*].mobility.speed = 10mps

See :doc:`mobility`. A moving UE that may leave its cell also needs handover
(see :doc:`handover`).

Step 3: Carriers and numerology
-------------------------------

The network's spectrum is described by its ``carrierAggregation`` module. The
tutorial keeps its defaults: one component carrier at 2 GHz, with 6 resource
blocks and numerology 0 (15 kHz subcarrier spacing, 1 ms slots). Both the gNB
and the UE use that carrier by default. A wider carrier, or a higher
numerology, is a matter of two lines:

.. code:: ini

   *.carrierAggregation.componentCarrier[0].numBands = 25
   *.carrierAggregation.componentCarrier[0].numerologyIndex = 1

With 6 resource blocks, the cell's capacity is small, which the later
configurations of the tutorial use to show congestion; with more resource
blocks, the same traffic occupies a smaller share of the cell. Several
carriers, carrier aggregation and TDD are described in :doc:`spectrum`.

Step 4: Traffic
---------------

The server sends a constant bit rate stream to the UE: a :ned:`CbrSender` on
the server, and a :ned:`CbrReceiver` on the UE.

.. code:: ini

   *.ue[*].numApps = 1
   *.server.numApps = 1

   *.ue[*].app[*].typename = "CbrReceiver"
   *.ue[*].app[*].localPort = 1000

   *.server.app[*].typename = "CbrSender"
   *.server.app[*].packetSize = 1000B
   *.server.app[*].samplingTime = 0.01s
   *.server.app[*].destAddress = "ue["+string(ancestorIndex(0))+"]"
   *.server.app[*].destPort = 1000
   *.server.app[*].localPort = 3088+ancestorIndex(0)
   *.server.app[*].startTime = uniform(0.01s,0.05s)

The sender sends a 1000-byte packet every 10 ms, 800 kbps. Its destination
address names the UE whose index matches the application's index, so the
same lines serve several UEs, one application per UE, when ``numUe`` grows;
the UE's name resolves to the address of its cellular interface (see
:doc:`ue`). Any INET application works the same way; Simu5G's own traffic
models are described in :doc:`apps`.

Step 5: QoS (an addition)
-------------------------

Without a QoS configuration, the stream rides the UE's default bearer as the
default QoS flow, which is what the tutorial does. To give it a bearer of its
own, with a standardized QoS profile, define two bearers for the UE and a
downlink rule that puts the stream on the QoS flow of the second one:

.. code:: ini

   *.bearerConfigurator.staticDrbs = [
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 1, isDefault: true,
        profile: "5qi-9"},
       {coreNetwork: "5gc", ue: "ue[*]", drbId: 2, mappedQfis: [2],
        profile: "5qi-2"}
       ]
   *.bearerConfigurator.dlQfiRules = [
       {filter: "expr(has(udp) && udp.destPort == 1000)", qfi: 2}
       ]

The anchor UPF classifies every downlink packet with the rules of
``dlQfiRules``: the stream's packets, addressed to UDP port 1000, get QFI 2,
everything else the default QoS flow. The UE's SDAP layer maps QFI 2 to DRB 2,
which carries the QoS characteristics of 5QI 2; the default flow goes on the
default bearer, DRB 1. The RLC mode and the logical channel group of each
bearer follow from its profile. Both bearers are established when the UE's
session is set up. See :doc:`qos`.

Step 6: The channel model (an addition)
---------------------------------------

The radio medium evaluates the NR links with the channel model of TR 38.901
by default, in its urban macrocell scenario (UMa) with a 25 m base station.
For a small cell in a street canyon, choose the urban microcell scenario (UMi),
whose base station is 10 m high in TR 38.901:

.. code:: ini

   *.radioMedium.nrChannelModel[*].scenario = "URBAN_MICROCELL"
   *.radioMedium.nrChannelModel[*].nodebHeight = 10m

Shadowing and fast fading are on by default. The channel models, their
scenarios and the interference they account for are described in
:doc:`radio-medium`.

Step 7: Running the simulation
------------------------------

In a shell where the ``setenv`` scripts of OMNeT++, INET and Simu5G have been
sourced, run the configuration from the directory of the ini file with the
``simu5g`` launcher (see :doc:`usage`):

.. code:: shell

   $ simu5g -u Cmdenv -c Single-UE omnetpp.ini

Without ``-u Cmdenv``, the simulation opens in Qtenv, where you can watch the
packets cross the gNB's NIC and inspect the modules. The results go into the
``results/`` directory. The tutorial's ``[General]`` section names the files
after the configuration and the repetition (``results/Single-UE/0.sca`` for
the scalars):

.. code:: ini

   output-scalar-file = ${resultdir}/${configname}/${repetition}.sca
   output-vector-file = ${resultdir}/${configname}/${repetition}.vec

Step 8: Looking at the results
------------------------------

A few statistics tell whether the cell carries the stream:

``cbrGeneratedThroughput`` (at the server's application)
   The throughput the server sends, in bytes per second.

``cbrReceivedThroughput`` (at the UE's application)
   The throughput the UE receives, in bytes per second, as a time-weighted
   average over the measurement period. With one UE in a lightly loaded cell,
   it equals the generated throughput.

``cbrFrameDelay`` (at the UE's application)
   The end-to-end delay of the stream's packets.

``avgServedBlocksDl`` (at the gNB's MAC)
   The number of resource blocks the gNB allocates in the downlink per slot,
   on average: how much of the cell the stream occupies.

``averageCqiDl`` (at the UE's PHY)
   The channel quality the UE reports, which sets how many bytes a resource
   block carries.

A quick look from the command line, with OMNeT++'s ``opp_scavetool``:

.. code:: shell

   $ cd results/Single-UE
   $ opp_scavetool query -l -f 'name =~ "cbr*Throughput*"' 0.sca

The OMNeT++ IDE and the Python analysis API (``omnetpp.scave``) give plots and
tables of the same results. The statistics of each layer are described in
the layer's chapter, and the ways of recording and analyzing them in
:doc:`results`.

Where to go next
----------------

The tutorial's later configurations extend this scenario the same way:
``BgTraffic`` adds a background cell that interferes with the UE (see
:doc:`background`), ``Multiple-UEs`` adds UEs until the cell congests,
``CarrierAggregation`` gives the gNB a second carrier, and ``Numerology``
varies the numerology. From there, the chapters of this manual are the
reference for every part of the configuration.
