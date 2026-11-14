ETSI MEC model in Simu5G
========================

Overview
--------

Multi-access Edge Computing (MEC) brings computing to the edge of the network,
next to the base stations, so that applications can serve UEs with low latency
and with knowledge of the radio network. ETSI specifies the MEC architecture in
GS MEC 003. A MEC system consists of MEC hosts and system-level management:
each MEC host runs MEC applications on a virtualisation infrastructure, and
its MEC platform offers MEC services to them through the Mp1 reference point.
The main functions of the system are the lifecycle management of MEC
applications, MEC service registration and discovery (GS MEC 011), and the MEC
services themselves, among them the Radio Network Information Service (RNIS,
GS MEC 012) and the Location Service (GS MEC 013). For lifecycle management, a
device application on a UE asks the user application lifecycle management proxy
(UALCMP) over the Mx2 reference point (GS MEC 016) to instantiate a MEC
application, and the MEC orchestrator chooses the MEC host that runs it. In a
5G network, a MEC host reaches the UEs' traffic through a UPF of its own.

Simu5G models a MEC system with these entities: the MEC orchestrator
(:ned:`MecOrchestrator`), which chooses a MEC host by a configurable policy
from the requirements in the application's descriptor; the UALCMP
(:ned:`Ualcmp`), with part of the Mx2 API; and MEC hosts (:ned:`MecHost`),
each with its own UPF, a virtualisation infrastructure that accounts for the
RAM, disk and CPU the MEC applications use, and a MEC platform with the Service
Registry, a subset of the RNIS API, and the Location Service. MEC applications
are instantiated on demand and use the services through REST APIs, and the
base stations steer the traffic of the MEC applications to their MEC host.
Of the Mx2 API, the operations to list applications and to create and delete
application contexts are modeled; of the RNIS API, a subset of the layer-2
measurements.

The aim of the model is to give MEC developers a tool for rapid prototyping of
MEC applications that use the MEC services Simu5G provides. Since Simu5G can
run as a real-time network emulator, real MEC applications and real UE
applications can also be interfaced with it, and tested in a framework that
includes the 5G transport, a MEC infrastructure, and its MEC services.

.. figure:: images/mec_architecture.png

   The ETSI MEC reference architecture (GS MEC 003). The operations support
   system (OSS) is not modeled.

Modules
-------

System level:

:ned:`MecOrchestrator`
   The MEC orchestrator. It keeps the onboarded application descriptors,
   selects the MEC host for each new MEC application, and has the host
   instantiate and terminate it.

:ned:`Ualcmp`
   A host that runs the UALCMP application, :ned:`UalcmpApp`, which serves
   the Mx2 API over HTTP and passes the requests to the MEC orchestrator.

Host level, inside :ned:`MecHost`:

``virtualisationInfrastructure`` (:ned:`VirtualisationInfrastructure`)
   The host's TCP/IP stack, on which the MEC applications, the MEC services
   and the Service Registry run.

``vim`` (:ned:`VirtualisationInfrastructureManager`)
   Accounts for the computing resources of the host, creates and deletes the
   MEC application modules, and computes their processing times.

``mecPlatformManager`` (:ned:`MecPlatformManager`)
   The host's interface to the MEC orchestrator.

``mecPlatform`` (:ned:`MecPlatform`)
   The Service Registry (:ned:`ServiceRegistry`) and the MEC services
   (``mecService[]``), such as :ned:`RniService` and :ned:`LocationService`.

``upf_mec`` (:ned:`Upf`)
   The host's UPF, which connects it to the core network.

Applications:

:ned:`DeviceApp`
   The device application on a UE, which requests MEC applications from the
   UALCMP on behalf of the UE's own application.

:ned:`MecAppBase`, :ned:`MultiUeMecApp`
   The base types of MEC applications. Simu5G provides several applications
   built on them, for example :ned:`MecWarningAlertApp` and
   :ned:`MecResponseApp`, with their UE-side counterparts (see :doc:`apps`).

MEC system-level modeling
-------------------------

The UALCMP is a host of its own, with a TCP/IP stack; in the example networks,
it sits in the data network, on the anchor UPF's N6 link. Its application
serves three operations of the Mx2 API over HTTP: ``GET /app_list``, which
lists the available applications, and ``POST /app_contexts`` and
``DELETE /app_contexts/{contextId}``, which request the creation and the
deletion of a MEC application. It passes the requests to the MEC orchestrator
through a pair of gates, which connect the two modules directly.

.. figure:: images/mec_system_level.png
   :figwidth: 70.0%

   The UALCMP and the MEC orchestrator, connected by gates; the orchestrator
   drives the MEC hosts through direct calls.

The MEC orchestrator manages the MEC hosts listed in its ``mecHostList``
parameter, so a network can contain several MEC systems, each with its own
orchestrator. It knows the applications by their application descriptors,
JSON files in the style of ETSI's AppD. A descriptor names the application
(``appDid``, ``appName``), the NED type that implements it (``appProvider``),
the resources it needs (``virtualComputeDescriptor``: ``virtualMemory`` and
``virtualDisk`` in MB, ``virtualCpu`` in millions of instructions per second),
and the MEC services it uses (``appServiceRequired``):

.. code:: json

   {
   "appDid" : "WAMECAPP",
   "appName" : "MecWarningAlertApp",
   "appProvider" : "simu5g.apps.mec.WarningAlert.MecWarningAlertApp",
   "virtualComputeDescriptor" :{
       "virtualDisk": 10,
       "virtualCpu" : 1500,
       "virtualMemory":10
       },
   "appServiceRequired": [
       {
           "ServiceDependency" :{
               "serName" : "LocationService",
               "version" : "v2",
               "serCategory": "Location"
           }
       }
   ]
   }

A descriptor is onboarded either at initialization, when its name is listed
in the orchestrator's ``mecApplicationPackageList`` (the file is then
``ApplicationDescriptors/<name>.json``), or on request, when a device
application sends the path of the descriptor with its creation request.

For each new MEC application, the orchestrator selects a MEC host with the
policy set in its ``selectionPolicy`` parameter:

``"MecServiceBased"`` (the default)
   The first host, in the order of ``mecHostList``, that has the resources the
   application needs and runs the first MEC service its descriptor requires.
   If no such host has the service, the last host with enough resources.

``"AvailableResourcesBased"``
   Among the hosts with enough resources, the one with the most CPU left.

``"MecHostBased"``
   The host at position ``mecHostIndex`` of ``mecHostList``, regardless of its
   resources.

The orchestrator then has the selected host instantiate the application,
through the host's MEC platform manager. The lifecycle operations take time:
``onboardingTime``, ``instantiationTime`` and ``terminationTime``, 50 ms each
by default, delay the orchestrator's answers.

MEC host-level modeling
-----------------------

A MEC host (:ned:`MecHost`) has a fixed amount of computing resources: at most
``maxMecApps`` MEC applications (0 by default, so it must be set), ``maxRam``
of memory, ``maxDisk`` of storage, and ``maxCpuSpeed`` millions of
instructions per second. The VIM accounts for them: an application is
instantiated only if the host has more free memory, storage and CPU than its
descriptor requires, and its resources are released when it is terminated.
Background applications (``bgApp[]``, :ned:`BackgroundApp`) reserve resources
for the whole run without running anything, to model a host that is already
loaded.

.. figure:: images/mec_host_level_2.png
   :figwidth: 70.0%

   The MEC host

The VIM also models the processing time of the MEC applications. An
application that asks it how long a computation takes gets the number of
instructions divided by the CPU speed it is entitled to: with the VIM's
``scheduling = "segregation"`` (the default), the CPU its descriptor
reserved; with ``"fair"``, its reservation scaled up to share all of the
host's CPU among the running applications.

The MEC platform holds the Service Registry and the MEC services, which are
HTTP servers running on the host's TCP/IP stack. The number and the types of
the services are configured per host (``mecPlatform.numMecServices``,
``mecPlatform.mecService[i].typename``). The services are registered with the
Service Registry of their host and with the MEC orchestrator, and MEC
applications discover them through the Service Registry's API
(``GET .../mec_service_mgmt/v1/services``, GS MEC 011). A service serves its
requests one at a time, from a queue, and the time it spends on each is drawn
from an exponential distribution (``requestServiceTime``); with
``loadGenerator = true``, the service also serves the requests of a
configurable number of background applications, which delay the real ones.

Two MEC services are implemented:

:ned:`RniService`
   A subset of the RNIS API (GS MEC 012): the layer-2 measurements query
   (``GET .../rni/v2/queries/layer2_meas``), filtered by cell or by UE. It
   reports on the base stations listed in the MEC host's ``bsList``
   parameter, from the statistics collectors that the base stations and the
   UEs have when their ``hasRniSupport`` parameter is true.

:ned:`LocationService`
   The Location API (GS MEC 013): the query of the UEs' positions
   (``GET .../location/v2/queries/users``), filtered by cell or by UE, and
   the subscription to a circular area (``POST
   .../location/v2/subscriptions/area/circle``), which notifies the
   subscriber when a UE enters or leaves the area, checked every
   ``locationSubscriptionPeriod``.

The life of a MEC application
-----------------------------

A MEC application is created on request of a UE:

1. The UE's own application asks its device application (:ned:`DeviceApp`),
   over UDP, to start a MEC application of a given name.

2. The device application sends a ``POST /app_contexts`` request to the
   UALCMP, over TCP.

3. The UALCMP passes the request to the MEC orchestrator, which onboards the
   descriptor if needed and selects a MEC host.

4. The host's VIM reserves the resources and creates the MEC application
   module inside the host, as an instance of the NED type that the
   descriptor's ``appProvider`` names.

5. The answer travels back: the orchestrator tells the UALCMP the address and
   port of the new application, and the device application tells the UE's
   application, which from then on talks to the MEC application directly.

The MEC application, in turn, finds the MEC services it needs through the
Service Registry and uses them over HTTP. When the UE's application is done,
it has the device application send ``DELETE /app_contexts/{contextId}``, and
the host terminates the MEC application and releases its resources.

A MEC application can also run outside this lifecycle: a host's independent
MEC applications (``independentMecApp[]``) are created at initialization, and
a descriptor with an ``emulatedMecApplication`` field stands for a real
application running outside the simulation (see `Running an emulated system`_
below).

Traffic steering to the MEC host
--------------------------------

A MEC host connects to the core network through its UPF, ``upf_mec``: its
``ppp`` gates link it to a UPF of the core network, typically an intermediate
UPF near the base stations, and its ``gateway`` parameter names the anchor
UPF whose sessions it serves. The control plane sets up an uplink tunnel for
every session of the anchor at each such MEC host UPF, next to the tunnel at
the anchor itself (see :doc:`core-network`).

The base station decides where an uplink packet goes. Every MEC host
registers its address with the :ned:`Binder`; when a UE's uplink packet is
addressed to a MEC host, the base station's traffic flow filter puts it into
the session's tunnel towards that host's UPF, instead of the tunnel towards
the anchor. A MEC host of another operator's core network is reached through
the anchor, as any external destination. The downlink from the MEC
application goes from the host's UPF to the base station that serves the UE.

The QoS flows of the MEC traffic are classified like any other traffic (see
:doc:`qos`); the example simulations scope a downlink classification rule to
the MEC host's UPF with its ``node`` field:

.. code:: ini

   *.bearerConfigurator.dlQfiRules = [
       {node: "**.upf_mec", filter: "WarningAlert*", qfi: 1},
       {node: "**.upf_mec", qfi: 0},
       {dscpAsQfi: true}]
   *.bearerConfigurator.ulQfiRules = [{filter: "WarningAlert*", qfi: 1}]

Configuration
-------------

**Add a MEC system to a network.** The network needs a
:ned:`MecOrchestrator`, a :ned:`Ualcmp` connected to it through its
``toMecOrchestrator`` and ``fromMecOrchestrator`` gates and linked to the data
network, and one or more :ned:`MecHost` nodes linked to UPFs of the core
network. Each MEC host's platform manager must know the orchestrator, and the
orchestrator its hosts:

.. code:: ini

   *.mecOrchestrator.mecHostList = ["mecHost1", "mecHost2"]
   *.mecOrchestrator.mecApplicationPackageList = ["WarningAlertApp"]
   *.mecHost*.mecPlatformManager.mecOrchestrator = "mecOrchestrator"

**Size a MEC host and give it services.**

.. code:: ini

   *.mecHost*.maxMecApps = 100
   *.mecHost*.maxRam = 32GB
   *.mecHost*.maxDisk = 100TB
   *.mecHost1.maxCpuSpeed = 400000
   *.mecHost*.bsList = ["gnb1", "gnb2"]

   *.mecHost2.mecPlatform.numMecServices = 1
   *.mecHost2.mecPlatform.mecService[0].typename = "LocationService"
   *.mecHost2.mecPlatform.mecService[0].localAddress = \
           "mecHost2.virtualisationInfrastructure"
   *.mecHost2.mecPlatform.mecService[0].localPort = 10020
   *.mecHost2.mecPlatform.serviceRegistry.localAddress = \
           "mecHost2.virtualisationInfrastructure"
   *.mecHost2.mecPlatform.serviceRegistry.localPort = 10021

**Run a MEC application for a UE.** The UE runs a device application and the
application that uses the MEC application:

.. code:: ini

   *.ue[*].numApps = 2
   *.ue[*].app[0].typename = "DeviceApp"
   *.ue[*].app[0].localPort = 4500
   *.ue[*].app[0].ualcmpAddress = "ualcmp"
   *.ue[*].app[0].ualcmpPort = 1000
   *.ue[*].app[0].appPackageSource = \
           "ApplicationDescriptors/WarningAlertApp.json"
   *.ue[*].app[1].typename = "UeWarningAlertApp"
   *.ue[*].app[1].deviceAppAddress = "ue["+string(ancestorIndex(1))+"]"
   *.ue[*].app[1].deviceAppPort = 4500

**Choose how the MEC host is selected:**

.. code:: ini

   *.mecOrchestrator.selectionPolicy = "AvailableResourcesBased"

**Use the RNIS.** The base stations and the UEs need the statistics
collectors the service reads:

.. code:: ini

   **.hasRniSupport = true
   *.mecHost.mecPlatform.mecService[0].typename = "RniService"

Deployment scenarios
--------------------

Simu5G runs MEC scenarios fully simulated or as an emulation, in which real
applications exchange their traffic through the simulated network. A user can
develop and test MEC applications in either mode; for example, a real MEC
application can be tested against a simulated UE application, or the other
way around.

Running a fully simulated system
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``MultiMec`` configuration of ``simulations/nr/mec/multiMecHost`` is a
complete example. Its network has two gNBs, each behind an intermediate UPF
with a MEC host of its own, and the UALCMP and the MEC orchestrator on the
anchor UPF. The UEs run a device application and a
:ned:`UeWarningAlertApp`, which has a :ned:`MecWarningAlertApp` instantiated
in the MEC system; the MEC application subscribes to the Location Service and
alerts its UE when the UE enters a danger zone. The application descriptor is
the one shown above, and the configuration is the one of the
`Configuration`_ section. Only ``mecHost2`` runs the Location Service, so the
default ``MecServiceBased`` policy places the MEC applications there; the
configuration also runs the other two policies, as an iteration variable, for
comparison.

Running an emulated system
~~~~~~~~~~~~~~~~~~~~~~~~~~

The examples under ``emulation/mec`` run the same scenario with real
applications: a real UE application (``extUeApp``), a real MEC application
(``extMecApp``), or both (``extUeAppMecApp``), written in Python and running
on the same machine as Simu5G. The real applications reach the simulation
through INET's :ned:`ExtLowerEthernetInterface` modules in the UE and in the
MEC host's virtualisation infrastructure, attached to virtual Ethernet (veth)
interfaces in separate Linux network namespaces:

.. figure:: images/namespaces.png
   :figwidth: 100.0%

   The namespaces of an emulated MEC scenario

.. code:: ini

   *.mecHost1.virtualisationInfrastructure.numExtEthInterfaces = 1
   *.mecHost1.virtualisationInfrastructure.extEth[0].typename = \
           "ExtLowerEthernetInterface"
   *.mecHost1.virtualisationInfrastructure.extEth[0].device = "sim-veth1"
   **.extMeAppsAddress = "192.168.2.0/24"

   *.ue.numEthInterfaces = 1
   *.ue.eth[0].typename = "ExtLowerEthernetInterface"
   *.ue.eth[0].device = "sim-veth2"
   *.ue.extHostAddress = "192.168.3.2"
   *.ue.forwarding = true

A real MEC application is described by a descriptor with an
``emulatedMecApplication`` field, which holds its address and port. The MEC
orchestrator then reserves the resources on the selected host without
creating a module, and gives the device application the real application's
address. The base stations forward the traffic addressed to the real MEC
applications, whose address range their ``extMeAppsAddress`` parameter
gives, to the MEC host.

Each example directory contains ``setup.sh`` and ``teardown.sh`` scripts,
which create and remove the namespaces and the veth interfaces, and a
``run.sh`` script. In outline, the setup does the following:

.. code:: shell

   sudo ip netns add ns1
   sudo ip netns add ns2
   sudo ip link add veth1 netns ns1 type veth peer name sim-veth1
   sudo ip link add veth2 netns ns2 type veth peer name sim-veth2
   sudo ip netns exec ns1 ip addr add 192.168.2.2/24 dev veth1
   sudo ip netns exec ns2 ip addr add 192.168.3.2/24 dev veth2
   sudo ip netns exec ns1 ip link set veth1 up
   sudo ip netns exec ns2 ip link set veth2 up
   sudo ip link set sim-veth1 up
   sudo ip link set sim-veth2 up
   sudo ip netns exec ns1 route add default dev veth1
   sudo ip netns exec ns2 route add default dev veth2

Running Simu5G as an emulator is described in :doc:`emulation`.

Statistics
----------

The Service Registry, the MEC services and the UALCMP record the response
time of the requests they serve (``responseTime``) and, with the load
generator, the number of background requests
(``loadGeneratorNumBackgroundRequests``). The applications record their own
measures: for example, :ned:`UeRequestApp` records its requests' response
time, split into the uplink, the processing and the downlink parts, and the
real-time video streaming applications record frame and playout statistics.
See :doc:`apps` and :doc:`results`.

Example simulations
-------------------

*  ``simulations/nr/mec/singleMecHost``: one gNB and one MEC host;

*  ``simulations/nr/mec/multiMecHost``: two cells with a MEC host each, and
   the three selection policies;

*  ``simulations/nr/mec/multiOperator``: two operators' core networks, each
   with its MEC host;

*  ``simulations/nr/mec/requestResponseApp``: request-response traffic with
   processing times from the VIM, background MEC applications and loaded
   services;

*  ``simulations/nr/mec/rnisTest``: a MEC application that queries the RNIS
   periodically;

*  ``emulation/mec/extUeApp``, ``extMecApp`` and ``extUeAppMecApp``: real UE
   and MEC applications.

Fidelity
--------

*  The reference points between the MEC orchestrator and the MEC hosts
   (Mm3 and the platform manager's interfaces) are direct calls, and the
   UALCMP is connected to the orchestrator by gates; the lifecycle operations
   take the fixed times ``onboardingTime``, ``instantiationTime`` and
   ``terminationTime``. The OSS and the relocation of MEC applications are not
   modeled.

*  The APIs are subsets: of Mx2, listing applications and creating and
   deleting application contexts; of the RNIS, the layer-2 measurements query,
   without subscriptions; of the Location API, the users query and the
   circular-area subscription; of the Service Registry, the query of the
   services. The Mp1 traffic rules and DNS rules are not modeled.

*  The ``MecServiceBased`` policy considers only the first MEC service a
   descriptor requires, and the ``MecHostBased`` policy does not check the
   host's resources; an application that does not fit is then refused at
   instantiation.

*  Processing times are the number of instructions divided by a CPU speed;
   the host's memory and storage are only accounted for, they do not affect
   the timing.

*  Known deviation: the base station steers the traffic of the MEC
   applications, by destination address, into a tunnel towards the MEC
   host's UPF. In the 5G system (TS 23.501 5.13), the SMF inserts an uplink
   classifier or a branching point UPF into the session's path, and the base
   station is not involved.

.. TODO: figure: the configuration of the fully simulated example (the old
   base_network_simulation2.png was a NED screenshot, which the guide does not
   use).
