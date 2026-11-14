Mobility
========

Overview
--------

In a cellular simulation study, mobility drives the radio link: as a UE moves,
its distance to the base stations changes, its links go in and out of line of
sight, its speed sets the rate at which the channel fades, and when a
neighboring cell becomes stronger, it hands over. Mobility also shapes the
load: how the UEs are spread over the cells, and how that changes over time.
Vehicular studies add the structure of roads, traffic and intersections, which
decides where and how fast the vehicles move, and when they stop.

Simu5G uses INET's mobility framework unchanged: a UE's ``mobility`` submodule
takes any INET mobility model, from a fixed position to random waypoints and
traces. The base stations are stationary. For vehicular scenarios, Simu5G
offers two options: the car nodes :ned:`LteCar` and :ned:`NrCar`, whose
movement comes from the SUMO road traffic simulator through the Veins
framework, and :ned:`TrafficLightMobility`, a model of its own in which
vehicles drive along roads and stop at traffic lights. The radio medium reads
the positions and the velocities from the mobility models when it evaluates a
link, so every movement shows in the channel.

This chapter describes the mobility of UEs and base stations, the two
vehicular options, and how mobility affects the radio link.

UEs
---

A UE's mobility is an INET mobility model, chosen with
``mobility.typename``; the default is :ned:`StationaryMobility`. Every model of
INET's catalog can be used (see the INET User's Guide): for example
:ned:`LinearMobility`, :ned:`RandomWaypointMobility`, :ned:`CircleMobility` or
:ned:`BonnMotionMobility` for traces. The constraint area of the model keeps
the UE in the region of the scenario.

The background UEs (see :doc:`background`) have a mobility submodule of the
same kind, so they can move too.

Base stations
-------------

The base stations' mobility is a :ned:`StationaryMobility`: they are placed by
the configuration (``mobility.initialX``, ``mobility.initialY``,
``mobility.initialZ``) and do not move. The background and external cells are
stationary as well.

Vehicles with Veins and SUMO
----------------------------

:ned:`LteCar` and :ned:`NrCar` are UEs whose mobility is Veins'
:ned:`VeinsInetMobility`: their positions come from the SUMO road traffic
simulator, which Veins runs alongside the simulation over the TraCI protocol.
The network contains Veins' :ned:`VeinsInetManager`, which creates a car node
for every vehicle that enters the road network in SUMO, deletes it when the
vehicle leaves, and updates the positions of the cars every ``updateInterval``.
Because the cars are created during the run, their IP addresses are configured
by INET's :ned:`HostAutoConfigurator` instead of the network's configurator.

Running such a simulation needs the Veins project and its ``veins_inet``
subproject in the workspace, and SUMO with Veins' ``traci-launchd`` running.
The ``simulations/nr/cars`` example, with the :ned:`Highway` network, shows the
setup.

Traffic light mobility
----------------------

:ned:`TrafficLightMobility` extends INET's :ned:`LinearMobility` with roads and
intersections: the vehicle drives at a constant speed along a road, and when
it reaches a traffic light that shows red in its direction, it waits until the
light turns green. With ``enableTurns = true``, it may turn at an
intersection. The model needs :ned:`TrafficLight` nodes in the network, listed
in the vehicle's ``trafficLightsList`` parameter; ``initialRoadIndex`` chooses
the road it starts on.

A :ned:`TrafficLight` is a stationary node whose
:ned:`TrafficLightController` runs the cycle of the light: green for
``greenPeriod``, yellow for ``yellowPeriod``, red for ``redPeriod``, starting
in ``startState`` at ``startTime``. A light with ``bidirectional = true``
controls both directions of its road.

A car that waits at a red light stands still: its speed, and with it the
Doppler shift of its links, is zero while it waits.

.. TODO: the sentence above assumes a pending fix (proposed merge-gate item):
   tests/doc/model-deviations.md item 22, TrafficLightMobility leaves the car's
   velocity unchanged while it waits at a red light, so a waiting car fades as
   if it were driving. Fix shape: the velocity is zero while the car waits.
   Check that it has landed before the guide is published.

How mobility affects the radio link
-----------------------------------

The radio medium reads a node's position and velocity from its mobility model
at the instant it evaluates a link (see :doc:`radio-medium`):

*  the distance between the two ends sets the path loss, and the line of
   sight state of the link is drawn from the distance as the channel model's
   study prescribes;

*  with Jakes fading (the default fading type of the channel models), the
   speed of the link's mobile end sets the Doppler frequency, and so how fast
   the fading changes;

*  the UE's measurements of the base stations' beacons follow its position,
   which drives cell selection and handover (see :doc:`handover`).

Configuration
-------------

**Move the UEs along straight lines.**

.. code:: ini

   *.ue[*].mobility.typename = "LinearMobility"
   *.ue[*].mobility.speed = 10mps
   *.ue[*].mobility.initialMovementHeading = uniform(0deg, 360deg)
   *.ue[*].mobility.constraintAreaMaxX = 2000m
   *.ue[*].mobility.constraintAreaMaxY = 2000m

**Drive cars through traffic lights.** From
``simulations/nr/videostreaming_dataset_generator``, whose
:ned:`UrbanNetwork` contains the traffic lights ``tl1`` to ``tl6``:

.. code:: ini

   *.ue[*].mobility.typename = "TrafficLightMobility"
   *.ue[*].mobility.trafficLightsList = ["tl1", "tl2", "tl3", "tl4", "tl5", \
           "tl6"]
   *.ue[*].mobility.speed = uniform(8.33mps, 16.67mps)   # 30-60 km/h
   *.ue[*].mobility.initialRoadIndex = intuniform(0,3)
   *.ue[*].mobility.enableTurns = true
   *.tl1.mobility.initialX = 400m
   *.tl1.mobility.initialY = 1600m

**Drive cars from SUMO.** From ``simulations/nr/cars``:

.. code:: ini

   *.veinsManager.host = "localhost"
   *.veinsManager.moduleType = "simu5g.nodes.NrCar"
   *.veinsManager.moduleName = "car"
   *.veinsManager.launchConfig = xmldoc("heterogeneous.launchd.xml")
   *.veinsManager.updateInterval = 0.1s

Statistics
----------

The UE's PHY records the distance between the UE and its serving base station
(``distance``), and the base stations record their position (``posX``,
``posY``). INET's mobility models have statistics of their own (see the INET
User's Guide).

.. TODO: the description of ``distance`` assumes a pending fix (proposed
   merge-gate item): tests/doc/model-deviations.md item 25, the statistic is
   emitted on every read of the position, so its mean is weighted by the reads
   rather than by time. Fix shape: record it time-weighted, or emit it on a
   timer. Say which, once it has landed.

Example simulations
-------------------

*  ``simulations/nr/cars`` and ``simulations/lte/cars``: cars driven by SUMO
   through Veins;

*  ``simulations/nr/videostreaming_dataset_generator``: cars with traffic
   light mobility in an urban network;

*  many other examples move their UEs with INET's models, for example the
   handover examples (see :doc:`handover`).

Fidelity
--------

*  The base stations do not move.

*  Known deviation: a car with traffic light mobility moves or waits for the
   whole time since its position was last updated, and every read of its
   position is an update, so where it is and how long it waits at a light
   depend on how often the simulation reads its position
   (tests/doc/model-deviations.md item 23).

*  Known deviation: the Jakes fading's phase is the speed times the absolute
   time, not the accumulated Doppler phase, so every change of speed
   re-randomizes the fading; and only the speed of the link's mobile end
   counts, not that of both ends (tests/doc/model-deviations.md item 24).
