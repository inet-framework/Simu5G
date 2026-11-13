Carriers, Numerologies and Duplexing
====================================

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
scheduler per carrier, at that carrier's slot rate. TDD is modeled as one
fixed division of every slot's 14 symbols between downlink and uplink, with
the remaining symbols used as guard; it scales how much data a slot carries in
each direction. Slot formats that change from slot to slot, and with them the
delay of waiting for a slot of the right direction, are not modeled, and
neither are bandwidth parts.

The interpretation of the numerology in scheduling is described in
:doc:`mac`, and the channel models of the carriers in :doc:`radio-medium`.

Component carriers
------------------

The ``CarrierAggregation`` module instantiates
``componentCarrier[numComponentCarriers]`` submodules of type
``ComponentCarrier``, each with:

``carrierFrequency``
   The carrier's center frequency. It doubles as the carrier's unique
   identifier throughout the stack, so no two CCs may use the same value.

``numBands``
   The number of resource blocks of the carrier.

``numerologyIndex``
   The NR numerology (0..4). As in the standard, a radio frame is 10 ms
   with 1 ms subframes; numerology :math:`\mu` divides each subframe into
   :math:`2^\mu` slots, which are the scheduling TTIs. Numerology 0
   corresponds to LTE timing; LTE nodes can only use numerology-0 carriers.

``useTdd``, ``tddNumSymbolsDl``, ``tddNumSymbolsUl``
   Duplexing. With ``useTdd = false`` (the default), the carrier is FDD,
   with separate uplink and downlink spectrum of ``numBands`` resource
   blocks each. With TDD, the 14 symbols of each slot are divided into a
   downlink part, an uplink part, and (implicitly) guard symbols -- the
   configured DL and UL symbol counts must sum to at most 14. This models
   the 3GPP slot-format concept (TS 38.213) at slot granularity.

Example:

.. code:: ini

   *.carrierAggregation.numComponentCarriers = 2
   *.carrierAggregation.componentCarrier[0].numerologyIndex = 0
   *.carrierAggregation.componentCarrier[0].carrierFrequency = 2GHz
   *.carrierAggregation.componentCarrier[1].numerologyIndex = 2
   *.carrierAggregation.componentCarrier[1].carrierFrequency = 6GHz

Binding nodes to carriers
-------------------------

Each cellular NIC has a ``numCarriers`` parameter and a matching vector of
channel model submodules, one per carrier the node uses. The binding is
made by pointing each channel model at a component carrier:

.. code:: ini

   *.gnb.cellularNic.numCarriers = 1
   *.gnb.cellularNic.channelModel[0].componentCarrierModule = \
           "carrierAggregation.componentCarrier[1]"

   *.ue[*].cellularNic.channelModel[0].componentCarrierModule = \
           "carrierAggregation.componentCarrier[0]"
   *.ue[*].cellularNic.nrChannelModel[0].componentCarrierModule = \
           "carrierAggregation.componentCarrier[1]"

Note the last two lines: a dual-stack UE binds its LTE leg
(``channelModel[]``) and its NR leg (``nrChannelModel[]``) independently,
which is how an EN-DC UE straddles two carriers of different frequency and
numerology.

A base station with ``numCarriers > 1`` performs carrier aggregation: its
MAC schedules each bound carrier separately, at that carrier's own slot
timing, and a UE bound to the same carriers can receive grants on all of
them.

Example simulations
-------------------

Three example directories exercise this machinery and are the recommended
reference configurations:

*  ``simulations/nr/test_multiCarrier`` -- carrier aggregation;
*  ``simulations/nr/test_numerology`` -- mixed numerologies across
   carriers;
*  ``simulations/nr/test_tdd`` -- TDD slot formats.

The CarrierAggregation module
-----------------------------

The ``CarrierAggregation`` module (``src/simu5g/common/carrierAggregation/``)
describes the radio spectrum of the network as a vector of
``ComponentCarrier`` submodules, each with its own carrier frequency, number
of resource blocks, numerology and duplexing settings. Base stations and UEs
refer to these carriers; per-carrier configuration is described in
:doc:`spectrum`.
