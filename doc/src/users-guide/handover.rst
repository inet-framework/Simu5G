Handover and Mobility
=====================

Overview
--------

Handover (3GPP TS 38.300 9.2.3 for NR, TS 36.300 10.1.2 for LTE) moves a UE in
connected mode from its serving cell to a target cell without interrupting
its sessions. It is network-controlled and UE-assisted: the UE measures the
cells around it and reports, and the serving base station decides. Its main
phases are preparation, execution and completion. In preparation, the source
base station asks the target to admit the UE over Xn (X2 in LTE), and the
target reserves resources and returns the configuration the UE is to use. In
execution, the source commands the UE to the target and forwards the UE's
downlink data to it, while the UE detaches from the source and synchronizes
with the target. In completion, the target has the core network switch the
downlink path to itself, and the source releases the UE's context. Between
base stations without an Xn connection, the handover runs through the core
network instead (N2-based in NR, S1-based in LTE), and NR also defines
conditional handover and dual active protocol stack (DAPS) handover. A UE
that loses its radio link without a handover declares radio link failure
(TS 38.331 5.3.10) and tries to re-establish its RRC connection (TS 38.331
5.3.7).

Simu5G models the Xn-based handover (X2-based in LTE), with each phase at the
entity that performs it in 3GPP. The UE measures the received power of the
base stations' beacons and reports to its serving base station when another
cell is stronger by a hysteresis margin; the serving base station prepares the
handover with the target and commands the UE; the source forwards the UE's
downlink to the target over X2 while the target holds it; and the target has
the core network switch the downlink path. The signaling messages are direct
calls, and the time the UE spends between the two cells is a configurable
latency. Radio link failure, detected by RLC acknowledged mode when it runs
out of retransmissions, and RRC re-establishment with its timers T311 and
T301 are modeled too. Handover through the core network, conditional handover
and DAPS handover are not modeled, and an attached UE hands over only to a
cell that has an X2 link to its serving cell.

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
