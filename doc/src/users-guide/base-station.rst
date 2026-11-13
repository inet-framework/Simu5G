The Base Station
================

.. TODO: This chapter covers: eNodeB and gNodeB; the relay; the X2 applications; the stats collector; the CoMP manager slot.

Base stations
-------------

Base stations are modeled by:

*  ``eNodeB`` -- the 4G base station;
*  ``gNodeB`` -- the 5G base station, which extends ``eNodeB`` and
   differs mainly in the NIC variant it instantiates and in its default
   gateway (``upf`` instead of ``pgw``).

A base station combines three groups of functionality:

**The RAN side.** The ``cellularNic`` module (``LteNicEnb`` /
``NrNicEnb``, or D2D variants) implements the base-station side of the
protocol stack and communicates with all attached UEs over the radio channel.
The ``cellInfo`` submodule holds per-cell information, and the ``mobility``
submodule (stationary by default) gives the cell its position.

**The core network side.** User traffic to and from the core network travels
in GTP tunnels. The ``trafficFlowFilter`` submodule classifies outgoing IP
datagrams and the ``gtpUser`` submodule encapsulates/decapsulates them into
GTP-U packets, which are sent over the ``pppIf`` interface towards the
gateway node (parameter ``gateway``, e.g. ``"upf"`` or ``"pgw"``).

**The X2 side.** For each X2 connection to a peer base station, the node has
a PPP interface (``x2ppp[]``); X2 applications (``x2App[]``, one per peer)
run on top of an SCTP stack and carry the X2 control messages used by
handover and dual connectivity. A separate GTP entity, ``gtpUserX2``,
forwards in-flight user-plane data to the target cell during handover.

Key parameters include ``masterId`` (making the node a secondary node of the
given master, see :doc:`dual-connectivity`), ``txPower``, and ``hasD2D``
(selecting the D2D-capable NIC, see :doc:`d2d`).
