Handover and Mobility
=====================

Overview
--------

Simu5G models intra-system handover: a moving UE detaches from its serving
cell and re-attaches to a stronger one, with the two base stations
coordinating over the X2 interface. As with bearers, the control-plane
signaling is abstracted -- what is modeled is the measurement process, the
decision, the timing, and the user-plane effects (detachment gap, data
forwarding, bearer re-establishment at the target).

Enabling handover
-----------------

Handover involves both sides of the radio link, and both are governed by
an ``enableHandover`` parameter -- on the base station's PHY (where it
turns on periodic *beacon* transmissions) and on the UE's handover
controller, a submodule of the NIC's RRC module (where beacon measurements
are evaluated and the decision is made). The example configurations simply
set both at once:

.. code:: ini

   **.enableHandover = true

The handover controller's decision parameters:

*  ``hysteresisFactor`` -- how much stronger a candidate cell must be
   before the UE switches, guarding against ping-pong handovers;
*  ``minRssi`` -- the threshold below which the serving cell is considered
   lost;
*  ``handoverLatency`` -- the modeled duration of the procedure, split
   into detachment and attachment phases.

The same controller also implements *dynamic cell association*
(``dynamicCellAssociation = true``): at initialization, the UE measures
the surrounding cells and associates with the strongest one, rather than
staying with the statically configured
``servingNodeId``/``nrServingNodeId`` (which still provides the starting
point).

What happens during handover
----------------------------

When the UE decides to hand over, the source and target base stations
execute the procedure over X2: the source announces the UE, in-flight
downlink data is forwarded to the target through a dedicated GTP tunnel,
and packets arriving during the gap are held and released when the UE
attaches (packet holder modules on both sides). The bearers of the UE are
re-established at the target according to their definitions, and the
``Binder`` records the new association. With ``losslessHandover = true``
on the base stations' handover machinery, forwarding is arranged so that
no in-flight packet is dropped.

A dual-connectivity UE hands over its legs independently -- e.g. an EN-DC
UE can change its secondary gNB while keeping its master eNB.

The UE's serving cell over time is recorded by the ``servingCell``
statistic, which is the standard way to observe handovers in results.

The reference example is ``simulations/lte/test_handover``; the multicell
NR examples also perform handovers.

.. TODO: describe the D2D interaction (flows are switched to
   infrastructure mode for the duration of a handover; see d2d) and
   verify the exact parameter paths for losslessHandover.
