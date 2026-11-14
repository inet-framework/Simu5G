Running Simu5G as a real-time network emulator
==============================================

Overview
--------

Network emulation runs real software over a simulated network. The simulation
runs in real time and exchanges packets with the real world: it captures the
packets of real hosts and applications from network interfaces of the machine
it runs on, carries them across the modeled network, and injects them into the
real network at the other end. This lets real applications, devices and
frameworks be tested against a network that would be costly or impossible to
build, such as a 5G deployment with many cells and loaded radio resources. The
condition is that the simulation keeps up with real time: a model that needs
more processing time than the time it simulates delays the real packets it
carries.

Simu5G provides emulation on top of INET's emulation support: the simulation
runs with INET's real-time scheduler (``inet::RealTimeScheduler``), and INET's
external interfaces (:ned:`ExtLowerEthernetInterface`) connect nodes of the
simulated network, such as a UE and a router of the data network, to network
interfaces of the host, typically virtual Ethernet (veth) pairs. A UE stands
for a real host behind it with its ``extHostAddress`` parameter. To keep large
scenarios within real time, Simu5G provides background cells and background
UEs, which load the radio resources and create interference without simulating
their protocol stacks (see :doc:`background`). The example emulations are in
the ``emulation/`` directory of the source tree.

Emulation is especially useful for integrating Simu5G with external real-world
applications and frameworks. For example, it allows one to :doc:`make Simu5G
interoperable with the Intel OpenNESS framework <emulation_openness>`. The
emulation model is described in this paper:

-  G. Nardini, G. Stea, A. Virdis, "Scalable Real-time Emulation of 5G
   Networks with Simu5G", IEEE Access, vol. 9, pp. 148504-148520, 2021. doi:
   10.1109/ACCESS.2021.3123873.

.. TODO: the emulation examples are broken on dev: none of the six networks
   under emulation/ has the coreControl module (CoreControl), which every
   base station's connection control requires since 37eb3f4aa ("cplane:
   coreControl,bearerConfigurator,rrc: refactor: the core network's control
   plane becomes its own module, CoreControl"). Fix them, and run each
   example once, before the guide is published.

An emulation demo
-----------------

With reference to the video below, the "sender" PC on the left sends 2.5 kB
packets to the "receiver" laptop on the right every 100 ms, and the PC in the
middle runs Simu5G as an emulator, transporting packets between the two
endpoints. On receipt of a packet, the receiver sends back an ACK. TCP is used
as the transport protocol. The scenario emulated in Simu5G is displayed on the
middle screen, and it contains one UE (the red dot) that moves away from its
serving gNB (the triangle in the central hexagon) and back again. Neighboring
gNBs interfere on the downlink spectrum. The deployment bandwidth is kept
artificially low, to saturate the radio frame. Within Simu5G, the sender is
connected to a remote server (not shown in the picture) and the receiver is
connected to the moving UE. The sender screen displays a moving-window average
of the round-trip time (RTT) of the communication, measured at the application
layer. The RTT climbs up as the UE moves away from the gNB, because its CQI
goes down, hence more radio resources are used for the same transmission, and
then it climbs down again as the UE moves back towards the gNB. Local delay
spikes can be observed, corresponding to HARQ retransmissions.

Large-scale real-time emulation with Simu5G
-------------------------------------------

Real-time emulation rests on the real-time event scheduler of OMNeT++ and on
INET's ability to exchange packets between a running simulation and the
operating system. What remains is to keep the model light enough to run in
real time on a desktop machine, up to fairly large scales (ten cells, a
thousand UEs), which realistic inter-cell interference and resource
contention require.

Simu5G does this by offering two levels of detail for UEs and cells. A
foreground UE is one whose packets or statistics the experiment observes: it
is a full UE, with every protocol layer from the application to the PHY. A
background UE exists only to compete for the radio resources and to
interfere: it has no protocol stack, only a traffic generator, which sizes
its buffers, a mobility model, which gives it a position, and a radio that
stands for it on the radio medium. Likewise, a foreground cell is a real base
station, which serves at least one foreground UE, while a background cell only
serves background UEs, to create interference: it has no protocol stack, only
a scheduler that allocates its resource blocks to its background UEs.

A real base station can serve background UEs too, next to its foreground UEs:
its NIC has a background traffic generator per carrier, whose background UEs
compete with the real ones in the base station's scheduler. The background
models are described in :doc:`background`.

.. TODO: figure: a foreground base station's NIC with its background traffic
   generator, and a background cell with its scheduler, traffic generator
   and background UEs. The old bgTrafficModel.png shows a background channel
   model that no longer exists, and bgTrafficModules.png a full-buffer
   generator that Simu5G does not have. Drawn, not a NED screenshot.

Running an emulation example with Simu5G
----------------------------------------

.. note:: Emulation runs on Linux only, as INET's emulation support does.

The ``emulation/`` directory of the Simu5G source tree has single-machine
emulation examples, which can be used as a basis for an emulation testbed:

*  ``emulation/extserver``: a real server exchanges TCP traffic with two
   simulated UEs, in the downlink and in the uplink;

*  ``emulation/extclientserver``: two real hosts, one behind a UE and one in
   the data network, exchange TCP traffic across the emulated 5G network;

*  ``emulation/extclientserver_bgTraffic``: the same with background cells
   and background UEs around the serving cell, whose interference raises the
   round-trip time of the real traffic;

*  ``emulation/mec/extUeApp``, ``emulation/mec/extMecApp`` and
   ``emulation/mec/extUeAppMecApp``: a real UE application, a real MEC
   application, or both, using the MEC model (see :doc:`mec`).

Each example has a README with its description and instructions, a
``setup.sh`` script that prepares the host's network, a ``run.sh`` script that
starts the simulation, and a ``teardown.sh`` script that cleans up.

Configuring the emulated scenario
---------------------------------

The following refers to the ``emulation/extclientserver_bgTraffic`` example.
Its network has one gNB with a UPF and a router behind it, one UE, and a
configurable number of background cells. Two real hosts run on the same
machine as the simulation, each in a network namespace of its own, connected
to the simulation through a veth pair: the one in ``ns1`` (192.168.2.2) is
attached to the router, as a host of the data network, and the one in ``ns2``
(192.168.3.2) to the UE, as the host behind it:

.. code:: text

   ns1 [veth1] <--> [sim-veth1] router .. gnb .. ue [sim-veth2] <--> [veth2] ns2

The router and the UE each get an :ned:`ExtLowerEthernetInterface`, bound to
the simulation's end of a veth pair. The UE stands for the real host behind
it through ``extHostAddress``: traffic to that address is delivered to the UE,
which forwards it to its external interface.

.. code:: ini

   [General]
   scheduler-class = "inet::RealTimeScheduler"

   [Config ExtClientServer-bgTraffic]
   *.router.numEthInterfaces = 1
   *.router.eth[0].typename = "ExtLowerEthernetInterface"
   *.router.eth[0].device = "sim-veth1"

   *.ue.numEthInterfaces = 1
   *.ue.eth[0].typename = "ExtLowerEthernetInterface"
   *.ue.eth[0].device = "sim-veth2"
   *.ue.extHostAddress = "192.168.3.2"
   *.ue.forwarding = true

Real packets carry real checksums, so the example has every simulated node
compute them (``**.fcsMode``, ``**.checksumMode`` and ``**.crcMode`` set to
``"computed"``). The routes of the simulated nodes come from routing files
(``routing/*.mrt``), which also lead to the real hosts' subnets. The background
cells and their background UEs are configured in the ``MultiCellBackground``
configuration, which ``ExtClientServer-bgTraffic`` extends.

The host side is prepared by ``setup.sh``: it creates the namespaces ``ns1``
and ``ns2``, the veth pairs, the addresses and the default routes, and turns
off checksum offloading on the namespaces' interfaces, so that the real hosts
compute the checksums the simulation checks.

Building Simu5G for real-time emulation
---------------------------------------

#. Make sure that the emulation feature is enabled in the INET project:

   -  *via the IDE*: right-click on the 'inet' folder in the Project
      Explorer, select Properties, then OMNeT++ > Project Features, and tick
      the box "Network emulation support";
   -  *via the command line*: in the root INET folder, type
      ``opp_featuretool enable NetworkEmulationSupport``.

   If the feature was disabled, rebuild INET (``make`` in the root INET
   folder).

#. Give the simulation the right to send and receive packets through raw
   sockets. Simu5G's ``simu5g`` launcher runs ``opp_run``, so set the
   capabilities of the OMNeT++ runners:

   .. code:: shell

      sudo setcap cap_net_raw,cap_net_admin=eip path/to/opp_run
      sudo setcap cap_net_raw,cap_net_admin=eip path/to/opp_run_dbg
      sudo setcap cap_net_raw,cap_net_admin=eip path/to/opp_run_release

#. Build Simu5G as described in :doc:`usage`: in the root Simu5G folder,

   .. code:: shell

      $ . setenv
      $ make

Running the emulation
---------------------

In the example's folder:

#. Prepare the host's network:

   .. code:: shell

      ./setup.sh

#. Start the simulation:

   .. code:: shell

      ./run.sh -r <RUN_NUMBER>

   where run 0 has 3 background cells, run 1 has 6, and run 2 has 9.

#. In another terminal, start the real traffic, for example a ping from the
   host of the data network to the host behind the UE:

   .. code:: shell

      sudo ip netns exec ns1 ping 192.168.3.2

   The round-trip time grows with the number of background cells, through
   their interference.

#. Clean up the host's network:

   .. code:: shell

      ./teardown.sh

Fidelity
--------

*  The simulation must keep up with real time. When it falls behind, the real
   packets it carries are delayed by the processing time, not only by the
   modeled network, and the measured figures no longer describe the network.
   Background cells and background UEs exist to keep large scenarios within
   real time.

*  Background UEs and background cells have no protocol stack: they occupy
   resource blocks and interfere, but carry no packets (see
   :doc:`background`).

*  The examples carry IPv4 traffic between the real hosts; the packets must be
   smaller than the MTU of the veth interfaces (1500 bytes by default).
