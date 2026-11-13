Mobility
========

.. TODO: This chapter covers: INET mobility as used in Simu5G; trafficLightMobility.

Mobility
--------

UE movement uses INET's mobility framework unchanged: any ``IMobility``
model can be plugged into a UE's ``mobility`` submodule, from stationary
placement through random waypoint to trace-driven models. See the INET
User's Guide for the catalog.

Simu5G adds one vehicular model of its own, *traffic light mobility*:

*  ``TrafficLightMobility`` extends INET's ``LinearMobility`` with roads,
   intersections, and stopping at red lights (``trafficLightsList``,
   ``enableTurns`` parameters);
*  ``TrafficLight`` nodes, placed on the floorplan, combine a position
   with a ``TrafficLightController`` state machine (``greenPeriod``,
   ``yellowPeriod``, ``redPeriod``).

For full-fledged road traffic simulation, the ``LteCar``/``NrCar`` node
types integrate with the Veins framework (:doc:`ue`).
