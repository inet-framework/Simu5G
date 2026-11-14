The Radio Medium and Channel Models
===================================

Overview
--------

3GPP specifies the radio channel for system-level evaluations in its channel
model studies: TR 38.901 for frequencies from 0.5 to 100 GHz, TR 36.873 for
the 3D channel of LTE, and TR 36.814 for LTE-Advanced. For each deployment
scenario -- indoor hotspot (InH), urban microcell (UMi), urban macrocell
(UMa), rural macrocell (RMa), and others -- they define how the signal of a
link is attenuated and how the receivers' SINR comes about. Their main
elements are path loss, the line-of-sight probability, shadow fading, fast
fading, outdoor-to-indoor penetration loss, and antenna patterns. The path
loss is a function of the distance, the carrier frequency and the antenna
heights, with one formula for line of sight (LOS) and one for non-line of
sight (NLOS), and the LOS probability, also a function of the distance, says
which applies. Shadow fading is a log-normal variation of the path loss,
spatially correlated over a correlation distance. Fast fading is the
variation caused by multipath propagation and movement, which TR 38.901
models with clusters of rays, or with simpler tapped delay lines. A UE inside
a building takes an additional penetration loss through the wall and inside
the building. The base stations' antennas are sectorial, with a pattern that
attenuates the signal off the boresight; a multi-antenna link has a channel
per antenna pair. At the receiver, the useful signal competes with the
interference of the other transmitters on the same resources and with thermal
noise, and their ratio, the SINR, decides the error rate.

Simu5G models the radio channel in a network-level module, the radio medium
(:ned:`CellularRadioMedium`), which evaluates links with a channel model per
carrier and radio access technology. The default channel model,
:ned:`StochasticChannelModel`, takes the path loss, the LOS probability, the
shadowing and the antenna pattern of the chosen scenario from TR 36.814, TR
36.873 or TR 38.901; adds Jakes or Rayleigh fast fading, the
outdoor-to-indoor penetration loss of TR 38.901, the link budget and the
thermal noise; and computes, for every resource block, the interference of
the other cells, of external and background cells, and the SINR. The
cluster-based fast fading of TR 38.901 and multi-antenna channels are not
modeled, and several known deviations from the studies remain, among them
the shadowing correlation, a thermal noise that does not follow the
bandwidth, and heights that are set per channel model rather than per link
(see `Fidelity`_).

This chapter describes the radio medium and how it evaluates a link, the
interference it counts, how to choose and configure a channel model, and the
reception trace. What happens at the receiving node -- its antenna, receiver,
error model and reception decision -- is described in :doc:`phy`.

The radio medium
----------------

Every network has one radio medium, named ``radioMedium``. As INET's radio
medium does for INET's wireless models, it models the propagation of the
signals up to the receiving antenna, and leaves what to make of a reception
to the node:

*  It keeps the registry of the radios: every PHY leg registers its radio,
   under its node id, so a dual connectivity UE is two radios.

*  It delivers broadcast frames, the base stations' beacons, to the radios in
   range. The range is that of a free-space transmission at the power
   ``pMax`` falling to the threshold ``sat``; these parameters set the reach of
   the beacons only, not the propagation of data.

*  It keeps the transmissions of the current and the previous slot of every
   carrier, from which the interference of a reception is computed.

*  It keeps the channel state of every link, shared by every channel model
   that evaluates it: whether the link is in line of sight, its last
   shadowing sample, and its fast fading paths.

*  It owns the channel models: one per carrier and radio access technology,
   ``channelModel[i]`` for LTE and ``nrChannelModel[i]`` for NR, bound to
   ``carrierAggregation.componentCarrier[i]`` (see :doc:`spectrum`). Their
   types are set with ``channelModelType`` (:ned:`StochasticChannelModel` by
   default, with the TR 36.814 formulas) and ``nrChannelModelType``
   (:ned:`Tr38901ChannelModel` by default).

Every radio of a technology on a carrier is evaluated by the same channel
model, so the parameters of a channel model hold for every link on its
carrier.

How a link is evaluated
-----------------------

When a frame is transmitted, the channel model of the receiver's technology
on the frame's carrier computes what the receiver sees of it, from where the
two radios were when the transmission started:

1. **Path loss.** The formula of the chosen study and scenario
   (``pathLossType``, ``scenario``), for the 2D and 3D distance of the link and
   the carrier frequency. The antenna heights are the channel model's
   ``nodebHeight`` and ``ueHeight`` parameters (25 m and 1.5 m by default);
   ``buildingHeight`` and ``streetWidth`` feed the formulas that use them.
   Beyond the distance a formula is valid for, the simulation stops with an
   error, unless ``tolerateMaxDistViolation`` is set.

2. **LOS or NLOS.** With ``dynamicLos = true``, each link's state is drawn
   from the scenario's LOS probability, and drawn again when either end has
   moved farther than ``correlationDistance`` (50 m). Otherwise every link
   has the state ``fixedLos`` says, which is NLOS by default.

3. **Shadowing.** A log-normal sample, with the standard deviation of the
   scenario and the LOS state, drawn per link and drawn again, correlated with
   the previous sample, when either end has moved farther than
   ``correlationDistance``. It is switched off with ``shadowing = false``.

4. **Penetration loss.** For a UE whose radio is inside a building (the
   radio's ``insideBuilding`` parameter), the outdoor-to-indoor loss of TR
   38.901: the low-loss model by default, the high-loss model with
   ``useBuildingPenetrationHighLossModel``. The distance from the wall and the
   random part of the loss are drawn once per radio.

5. **Antenna pattern.** A base station whose antenna is sectorial (the node's
   ``radio.antenna.txDirection = "ANISOTROPIC"``, pointing at ``txAngle``)
   attenuates the signal by the study's pattern at the angle of the link,
   measured on the horizontal plane. The antenna gains, the receiver's noise
   figure and its cable loss are the node's (see :doc:`phy`).

6. **Fast fading.** Jakes fading by default (``fadingType = "JAKES"``), a sum
   of ``numFadingPaths`` paths (6) with a delay spread of ``delayRms``, whose
   Doppler shift follows the speed of the link's mobile end; or a Rayleigh
   fading (``"RAYLEIGH"``). It is switched off with ``fading = false``.

7. **SINR per resource block.** The received power on each resource block
   the frame occupies, against the thermal noise (``thermalNoise``) plus the
   interference on that block (see below).

.. TODO: the horizontal-plane angle in step 5 assumes the pending fix of
   tests/doc/model-deviations.md item 10 (computeAngle() takes asin(dy/d) of
   the 3D distance; the fix is atan2 on the horizontal components). Check that
   it has landed before the guide is published.

The same evaluation, without a frame, answers what-if questions: the CQI a UE
would report, and the received power of a cell during cell selection.

Interference
------------

The interference of a reception is the received power of the other
transmissions on the same carrier, in the same slot, on the same resource
blocks. Which transmissions count is set by switches of the channel model:

``downlinkInterference``
   The downlink transmissions of the other cells, at a UE. Off by default.

``uplinkInterference``
   The uplink transmissions of the UEs of other cells, at a base station.
   Off by default.

``extCellInterference``
   The external cells (:ned:`ExtCell`), in the downlink. On by default.

``bgCellInterference``
   The background cells and their background UEs (:ned:`BackgroundCell`), in
   both directions. On by default.

``d2dInterference``
   Other device-to-device transmissions, at a D2D receiver. On by default.

Since the interference between real cells is off by default, a multicell
study has to turn it on; most shipped multicell configurations do. The
external and background cells, which stand in for the load of neighboring
cells at a fraction of the cost, are described in :doc:`background`. A
background UE's uplink allocation is on the medium in every case, and counts
wherever a channel model evaluates uplink interference.

.. TODO: the last sentence assumes the pending fix of model-deviations.md item
   20 (a background UE's UL allocation is registered with the medium only when
   its own cell's channel model has uplinkInterference on). Check that it has
   landed before the guide is published.

Channel models
--------------

:ned:`StochasticChannelModel`
   The statistical channel model described above. Its ``pathLossType``
   selects the study whose formulas supply the path loss, the LOS
   probability, the shadowing and the antenna pattern: ``"Tr36814"`` (the
   default), ``"Tr36873"`` or ``"Tr38901"``. Everything else is the same for
   every study.

:ned:`Tr38901ChannelModel` and :ned:`Tr36873ChannelModel`
   :ned:`StochasticChannelModel` with ``pathLossType`` preset to TR 38.901
   and TR 36.873. :ned:`Tr38901ChannelModel` is the default for NR.

:ned:`IdealChannelModel`
   No propagation at all: no path loss, shadowing, fading or interference,
   and a SINR so high that the CQI is always the highest. Combined with a
   :ned:`ConfiguredRateErrorModel` at the receivers, the only loss is a
   configured error rate. It is meant for protocol validation, such as
   driving RLC retransmissions at a known loss rate, not for performance
   studies.

The scenarios are ``INDOOR_HOTSPOT``, ``URBAN_MICROCELL``,
``URBAN_MACROCELL`` (the default), ``RURAL_MACROCELL`` and
``SUBURBAN_MACROCELL``. TR 38.901 has no suburban macrocell scenario: with
it, the formulas of TR 36.814 are used.

The reception trace
-------------------

For debugging, and for comparing two versions of the model reception by
reception, the radio medium can write a reception trace: one text line per
reception decision, with the per-resource-block SINR, the interferers, the
error rate, the random draw and the outcome, and one line per channel
evaluation that feeds a decision, such as a CQI or a beacon's received
power. It is switched on with ``recordReceptionTrace = true`` and written to
``${resultdir}/${configname}-${runnumber}.rxtrace``. Writing it draws no
random number and schedules no event, so it does not change the simulation.

Configuration
-------------

**Choose the propagation study and the scenario.** The channel models of a
technology are configured together:

.. code:: ini

   *.radioMedium.nrChannelModelType = "Tr36873ChannelModel"
   **.nrChannelModel[*].scenario = "URBAN_MICROCELL"
   **.nrChannelModel[*].nodebHeight = 10m

**Turn on the interference between cells.** A multicell scenario needs both
switches:

.. code:: ini

   **.downlinkInterference = true
   **.uplinkInterference = true

**Draw the LOS state from the scenario.** Without this, every link is NLOS:

.. code:: ini

   **.nrChannelModel[*].dynamicLos = true

**Put the UEs inside buildings.** The setting is the UE radio's:

.. code:: ini

   **.ue[*].cellularNic.nrRadio.insideBuilding = true

**Use a channel without impairments,** with a fixed packet error rate (see
:doc:`phy` for the error model):

.. code:: ini

   *.radioMedium.nrChannelModelType = "IdealChannelModel"

**Record the reception trace:**

.. code:: ini

   *.radioMedium.recordReceptionTrace = true

Statistics
----------

The radio medium records no statistics of its own. The SINR of the receptions
is recorded by the receivers (see :doc:`phy`); ``collectSinrStatistics =
false`` on a channel model switches that recording off for its links, which
saves run time.

Example simulations
-------------------

*  ``simulations/channelmodel``: one configuration per propagation study and
   scenario (``UMa-38901``, ``UMi-36873``, ``InH-36814`` and so on), with the
   LOS state drawn along a moving UE's path, plus configurations for the
   fixed LOS state, penetration loss (``Penetration*``), Rayleigh fading,
   a sectorial antenna (``Anisotropic-36873``) and tall UEs. These are
   regression tests of the formulas as much as examples.

*  The multicell examples, such as ``simulations/nr/standalone_multicell``
   and ``simulations/lte/``, turn on the interference between cells.

Calibration
-----------

Simu5G's PHY model has been validated in compliance with the calibration
guidelines of 3GPP RP-180524: in the Urban Macro scenario prescribed there,
the SINR distribution obtained with Simu5G matches the reference
distribution.

.. figure:: images/SINR.png
   :align: center
   :figwidth: 70.0%

.. TODO: the calibration predates the radio medium rework (the channel models
   moved into the medium, the per-link state, the deviations recorded in
   tests/doc/model-deviations.md); re-run it before the guide is published.

Fidelity
--------

*  The fast fading is a Jakes sum of paths, or Rayleigh: the cluster-based
   models and the tapped delay lines of TR 38.901 are not modeled, nor is
   spatial consistency beyond the correlation distance of the LOS state and
   the shadowing.

*  Every link is single-antenna: there is no channel per antenna pair, and
   no MIMO.

*  The antenna heights and the scenario are parameters of a channel model,
   one per carrier and technology, not properties of the link: a macro cell
   and a small cell on one carrier, or UEs at different heights, cannot be
   expressed, and the heights do not follow the nodes' positions, which most
   examples keep at z = 0 (model-deviations.md item 32).

*  The interference of the external and background cells, and of background
   UEs, is evaluated without shadowing, and their links take the channel
   model's heights (items 6 and 31).

*  Known deviation: the shadowing correlation. A link's shadowing is drawn
   again only when an end has moved farther than the correlation distance,
   and correlated with factor :math:`e^{-d/(2 d_{cor})}`; TR 38.901 7.4.4
   correlates it continuously, with :math:`e^{-d/d_{cor}}` (item 1).

*  Known deviation: the thermal noise is a fixed parameter, -104.5 dBm by
   default, which is the noise of 50 resource blocks at 15 kHz; it does not
   follow the carrier's bandwidth or numerology, and every resource block
   receives the full transmit power, which is not split over the resource
   blocks (item 2).

*  Known deviation: Rayleigh fading is a fixed value per resource block,
   shared by every link and constant in time; it is not fading (item 3).

*  Known deviation: the phase of the Jakes paths is the speed times the
   absolute time, so a change of speed re-randomizes the fading, and only the
   UE end's speed counts (item 24).

*  Known deviation: a link's LOS state is drawn per cell and per carrier,
   while TR 38.901 draws it per site and the same for every frequency (item
   27).

*  Known deviation: the outdoor-to-indoor loss is that of TR 38.901, and of
   TR 36.873 in the urban macrocell only; under TR 36.814, and in the other
   TR 36.873 scenarios, a UE inside a building takes no penetration loss
   (item 29).

*  Known deviation: a UE's distance from the wall is one uniform draw from
   0 to 25 m, while TR 38.901 7.4.3.1 takes the smaller of two draws, and 0
   to 10 m in the rural macrocell (item 17); and an NR UE's two radios draw
   their indoor positions independently, while the standard has one per UE
   (item 14).

*  Known deviation: a sectorial base station's pattern applies only when it
   transmits; in the uplink, it is ignored (item 33, see :doc:`phy`).

*  The D2D links use the base station conventions for their path loss and
   take the receiver's indoor state; D2D is a legacy feature (items 4 and 15,
   see :doc:`d2d`).

.. TODO: written as if fixed (pending fixes, preconditions for publishing):
   model-deviations.md item 10 (azimuth on the horizontal plane), item 20
   (background UE uplink allocations always on the medium), item 26 (the UMa
   environment height draw: TR 36.873's distance term, TR 38.901's 18 m
   boundary), item 30 (a moving base station interferes from its
   transmission's start position).
