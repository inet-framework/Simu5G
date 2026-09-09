Carriers, Numerologies and Duplexing
====================================

Overview
--------

The radio spectrum of a Simu5G network is described centrally, by the
network-level ``CarrierAggregation`` module, as a set of *component
carriers* (CCs). Every carrier-dependent piece of the stack -- schedulers,
HARQ timing, channel models -- refers to these definitions, and a node
participates in a carrier by binding one of its channel model instances to
it. This chapter covers the carrier definitions and the binding mechanism;
the interpretation of numerology in scheduling appears in :doc:`mac`,
and per-carrier channel modeling in :doc:`phy`.

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
