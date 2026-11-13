The UE
======

.. TODO: This chapter covers: LteUe, NrUe, LteCar, NrCar; the dual-stack NICs; sessionType; cell association.

User equipment
--------------

Terminals are modeled by two node types, defined in ``src/simu5g/nodes/``:

*  ``LteUe`` -- an LTE-only terminal;
*  ``NrUe`` -- extends ``LteUe`` into a dual-stack terminal that can
   attach to an eNB, to a gNB, or to both at once (dual connectivity).

A UE is essentially a mobile INET host. Its submodules include:

*  a vector of applications (``app[numApps]``), which can be any INET
   ``IApp``-compatible module -- INET's own applications and Simu5G's traffic
   models (see :doc:`apps`) alike;
*  UDP, TCP and SCTP transport protocols and an IPv4 (optionally IPv6)
   network layer, all from INET;
*  the ``cellularNic`` module implementing the LTE/NR protocol stack --
   ``LteNicUe`` for ``LteUe``, ``NrNicUe`` for ``NrUe`` (or their
   D2D-capable variants when ``hasD2D = true``);
*  a ``mobility`` submodule, accepting any INET mobility model;
*  optional loopback and Ethernet interfaces; the latter
   (``eth[numEthInterfaces]``) defaults to ``ExtLowerEthernetInterface`` and
   is chiefly used in emulation setups (:doc:`emulation`).

Cell association
~~~~~~~~~~~~~~~~

Which base station serves a UE is configured through node-level parameters:

*  ``servingNodeId`` -- the node id of the serving eNB (the LTE leg);
*  ``nrServingNodeId`` -- the node id of the serving gNB (the NR leg,
   ``NrUe`` only).

Either leg can be left unattached. An ``NrUe`` with both parameters set is
a dual-connectivity UE; one with only ``nrServingNodeId`` set is a
standalone-5G terminal. Automatic, measurement-based initial cell selection
is available via the ``dynamicCellAssociation`` parameter of the UE's
handover controller (a submodule of the NIC's RRC module), in which case
the UE re-associates with the strongest cell at initialization; the
statically configured serving node id still serves as the starting point.

Vehicular variants
~~~~~~~~~~~~~~~~~~

``LteCar`` and ``NrCar`` extend the two UE types with mobility driven
by the `Veins <https://veins.car2x.org/>`__ vehicular simulation framework
(``VeinsInetMobility``). They are used in the ``cars`` example simulations.
For traffic-light-controlled vehicular mobility without Veins, see
:doc:`handover`.
