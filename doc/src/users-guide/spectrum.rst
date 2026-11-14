Spectrum
========

Overview
--------

In 3GPP, the spectrum a cell uses is a set of carriers (TS 38.211, TS 38.213
and TS 38.300 for NR; TS 36.211 and TS 36.300 for LTE). A carrier has a
frequency and a bandwidth, divided into resource blocks of 12 subcarriers. Its
main properties are the numerology, the duplexing mode, the bandwidth parts,
and carrier aggregation. For the numerology (TS 38.211 4.2), the index
:math:`\mu` sets the subcarrier spacing, :math:`15 \cdot 2^\mu` kHz, and with
it the slot length, :math:`2^{-\mu}` ms; LTE uses the equivalent of
numerology 0. For duplexing, a carrier is operated in frequency division
duplex (FDD), with separate downlink and uplink spectrum, or in time division
duplex (TDD), where a slot format divides the symbols of each slot between
downlink, uplink and flexible use, and can differ from slot to slot (TS
38.213 11.1). Bandwidth parts let a UE operate on a part of a wide carrier,
with a numerology of its own. With carrier aggregation (TS 38.300 5.4), a base
station serves a UE on several component carriers at once, a primary cell and
secondary cells, each scheduled separately and with a HARQ entity of its own.

Simu5G models the carriers of a network in one place, the
:ned:`CarrierAggregation` module, as component carriers
(:ned:`ComponentCarrier`) with a carrier frequency, a number of resource
blocks, a numerology from 0 to 4, and FDD or TDD. The PHY of each base station
and UE leg names the carriers it operates on, the primary one first, and a
base station with several carriers performs carrier aggregation: it runs a
scheduler per carrier, at that carrier's slot rate. TDD is modeled, on the NR
legs, as one fixed division of every slot's 14 symbols between downlink and
uplink, with the remaining symbols used as guard; it scales how much data a
slot carries in each direction. Slot formats that change from slot to slot,
and with them the delay of waiting for a slot of the right direction, are not
modeled, and neither are bandwidth parts.

This chapter describes how carriers are defined, how nodes are bound to them,
and what numerology and duplexing change in a simulation. How the MAC
schedules on several carriers is described in :doc:`mac`, and the channel
models of the carriers in :doc:`radio-medium`.

Modules
-------

:ned:`CarrierAggregation`
   The network-level module that holds the network's carriers, as the
   ``componentCarrier[]`` vector of its submodules; ``numComponentCarriers``
   sets their number. Every network has exactly one, named
   ``carrierAggregation``.

:ned:`ComponentCarrier`
   One carrier: its frequency, its number of resource blocks, its numerology
   and its duplexing.

Component carriers
------------------

A carrier is described by a few values. Its frequency
(``carrierFrequency``) is the center frequency; it is also the carrier's
identity throughout the model, so every carrier of a network needs a
frequency of its own. Its bandwidth is given as a number of resource blocks
(``numBands``). Its numerology (``numerologyIndex``, 0 to 4) sets the slot
length. Its duplexing is FDD by default, or TDD with ``useTdd`` (see
`Duplexing`_ below).

.. code:: ini

   *.carrierAggregation.numComponentCarriers = 2
   *.carrierAggregation.componentCarrier[0].carrierFrequency = 2GHz
   *.carrierAggregation.componentCarrier[0].numBands = 6
   *.carrierAggregation.componentCarrier[0].numerologyIndex = 0
   *.carrierAggregation.componentCarrier[1].carrierFrequency = 6GHz
   *.carrierAggregation.componentCarrier[1].numBands = 6
   *.carrierAggregation.componentCarrier[1].numerologyIndex = 2

The radio medium has one channel model per carrier and radio access
technology, ``radioMedium.channelModel[i]`` for LTE and
``radioMedium.nrChannelModel[i]`` for NR, each bound to
``carrierAggregation.componentCarrier[i]``. The reference networks therefore
set ``radioMedium.numCarriers`` from ``carrierAggregation.numComponentCarriers``
(see :doc:`deployments` and :doc:`radio-medium`).

Numerology and slot timing
--------------------------

The numerology sets a carrier's slot length: 1 ms divided by
:math:`2^\mu`, so 1 ms with numerology 0, 0.5 ms with numerology 1, and
62.5 us with numerology 4. The slot is the unit of scheduling: a base station
schedules each of its carriers once per slot of that carrier, so a carrier
with a higher numerology is scheduled more often, with less data per slot. The
MAC of a base station or a UE runs on the slot of the highest numerology among
its carriers, and serves each carrier at its own rate within that cycle (see
:doc:`mac`). A frame's transmission lasts one slot of its carrier.

LTE uses the timing of numerology 0, and the carriers of LTE legs should be
numerology-0 carriers; the model does not check it.

Duplexing
---------

With FDD, the default, a carrier has its ``numBands`` resource blocks in the
downlink and, separately, in the uplink, and every slot can carry data in both
directions.

With TDD (``useTdd = true``), the 14 symbols of every slot are divided between
the two directions: ``tddNumSymbolsDl`` symbols for the downlink,
``tddNumSymbolsUl`` for the uplink (7 and 7 by default), and the remaining
symbols, if any, as a guard. Their sum must not exceed 14. The division is the
same in every slot. It scales how much data the resource blocks of a slot
carry in each direction: the NR link adaptation sizes a transport block from
the symbols of its direction.

.. code:: ini

   *.carrierAggregation.componentCarrier[0].useTdd = true
   *.carrierAggregation.componentCarrier[0].tddNumSymbolsDl = 10
   *.carrierAggregation.componentCarrier[0].tddNumSymbolsUl = 2

The TDD settings take effect on the NR legs only; an LTE leg on a TDD carrier
is sized as on an FDD one.

Binding nodes to carriers
-------------------------

Every PHY names the carriers it operates on in its ``componentCarrierModules``
parameter: module paths of component carriers, separated by spaces, the
primary carrier first. A base station's PHY is ``cellularNic.phy``; a UE has
one PHY per leg, ``cellularNic.phy`` for the LTE leg and
``cellularNic.nrPhy`` for the NR leg, and each is bound on its own. A UE leg
must be bound to the carriers of the base station it attaches to.

.. code:: ini

   *.gnb.cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0]"
   *.ue[*].cellularNic.nrPhy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0]"

A dual connectivity UE binds its two legs to the carriers of its two base
stations, which can differ in frequency and numerology; this is how an EN-DC
UE uses an LTE carrier and an NR carrier at once:

.. code:: ini

   *.masterEnb.cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0]"
   *.secondaryGnb.cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[1]"
   *.ue[*].cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0]"
   *.ue[*].cellularNic.nrPhy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[1]"

A base station's NIC also has a ``numCarriers`` parameter, the number of
carriers it serves, which sizes its background traffic generators (one per
carrier, see :doc:`background`). It must match the number of carriers in the
PHY's ``componentCarrierModules``.

The primary carrier has a role of its own: a base station sends its beacons
on its primary carrier, and a UE measures the beacons of its primary carrier
for cell selection and handover (see :doc:`phy`).

Carrier aggregation
-------------------

A base station whose PHY lists several carriers performs carrier
aggregation. Its MAC runs a scheduler for each carrier, at that carrier's
slot rate, and a HARQ entity for each; a UE whose leg lists the same carriers
can be scheduled on all of them in the same slot, and reports a CQI per
carrier. Each carrier is evaluated by its own channel model, so the path loss
and the interference depend on the carrier's frequency.

.. code:: ini

   *.carrierAggregation.numComponentCarriers = 2
   *.carrierAggregation.componentCarrier[0].carrierFrequency = 2GHz
   *.carrierAggregation.componentCarrier[1].carrierFrequency = 6GHz

   *.gnb.cellularNic.numCarriers = 2
   *.gnb.cellularNic.phy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0] \
            carrierAggregation.componentCarrier[1]"
   *.ue[*].cellularNic.nrPhy.componentCarrierModules = \
           "carrierAggregation.componentCarrier[0] \
            carrierAggregation.componentCarrier[1]"

Statistics
----------

The carriers record no statistics of their own. The per-carrier variants of
the SINR statistics are described in :doc:`phy`, and the MAC's statistics per
cell and UE in :doc:`mac`.

Example simulations
-------------------

*  ``simulations/nr/test_multiCarrier``: a gNB with one carrier
   (``SingleCarrier``) and with two (``MultiCarrier``), with downlink and
   uplink CBR traffic;

*  ``simulations/nr/test_numerology``: EN-DC with the LTE leg on a
   numerology-0 carrier and the NR leg on a second carrier whose numerology
   ranges over 0 to 4;

*  ``simulations/nr/test_tdd``: one carrier, run with FDD and with TDD.

Fidelity
--------

*  The TDD division of a slot is the same in every slot: slot formats that
   vary from slot to slot (TS 38.213 11.1), and the waiting for a slot of the
   right direction, are not modeled; a slot carries both directions, in
   proportion to their symbols. TDD affects the NR legs only.

*  Bandwidth parts are not modeled: a UE uses its carriers whole.

*  The model does not check that the carriers of an LTE leg have numerology 0.

*  The subcarrier spacing changes only the slot length. A resource block
   carries the same number of resource elements per slot in every numerology,
   and the thermal noise does not follow the bandwidth (see
   :doc:`radio-medium`).
