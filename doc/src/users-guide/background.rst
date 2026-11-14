Background Traffic and Interference
===================================

Overview
--------

In a simulation study, the cells and users around the ones under study matter
through two effects: they load the radio resources of the cells they share
with the users under study, and they interfere with the links of the
neighboring cells. Simulating them with full protocol stacks makes every
background user cost as much as a studied one -- applications, every protocol
layer, every packet -- which makes large deployments slow, and real-time
emulation of them impossible. What a study needs from the background is its
effect: which resource blocks it occupies, when, and with what power from
where.

Simu5G provides three lightweight elements for the background, none of which
sends packets: background UEs inside a real base station, which compete with
the real UEs for the cell's resource blocks; background cells, base stations
with their own background UEs and scheduler, which load their own spectrum and
interfere with the real cells; and external cells, fixed patterns of downlink
resource occupation with a power and a direction. A background UE's traffic is
a buffer that fills at a configured rate, its channel quality is a CQI
computed or drawn for it, and its retransmissions follow a configured
probability. Their transmissions are put on the radio medium, so the same
channel models that evaluate every link compute the interference they cause.
The model is that of G. Nardini, G. Stea, A. Virdis, "Scalable Real-time
Emulation of 5G Networks with Simu5G", IEEE Access, vol. 9, 2021, which uses it
to emulate large 5G deployments in real time.

This chapter describes the three elements, how their interference reaches the
real cells, and how to configure them.

Background UEs in a real cell
-----------------------------

Every base station's cellular NIC has one background traffic generator per
carrier it serves, ``bgTrafficGenerator[]`` (:ned:`BackgroundTrafficGenerator`;
see :doc:`cellular-nic`), with ``numBgUes`` background UEs
(:ned:`BackgroundUe`), none by default. A background UE has a position (its
``mobility``), a traffic generator (``generator``) and a radio
(:ned:`CellularPhantomRadio`) that stands for its transmitter and antenna on
the radio medium. Its traffic generator fills a downlink and an uplink buffer:
a packet of ``packetSizeDl`` (``packetSizeUl``) bytes, plus ``headerLen`` bytes
of headers, every ``periodDl`` (``periodUl``), from ``startTimeDl``
(``startTimeUl``) on; a direction whose start time is negative carries no
traffic.

The base station's own scheduler serves the background UEs together with the
real ones: they compete for the same resource blocks, by their buffers and
their CQIs, as the scheduling policy decides. In the uplink, a background UE
first goes through random access, and the rest of its data is served from six
slots after its first buffer status report on. A served block of a
background UE costs no packet: its buffer is drained by the bytes the block
carries, and with probability ``rtxRateDl`` (``rtxRateUl``) the bytes come back
after ``rtxDelayDl`` (``rtxDelayUl``) as a retransmission. The allocation is
put on the radio medium as a transmission: the base station's in the downlink,
the background UE's in the uplink.

The background UE's CQI is set in one of three ways:

*  With ``enablePeriodicCqiUpdate = true`` (the default), it is updated every
   ``fbPeriod``, from the SINR the radio medium computes for the background
   UE at its position: a what-if reception that takes the interference of the
   moment into account.

*  With ``useProbabilisticCqi = true`` as well, the periodic update draws the
   CQI from a normal distribution (``cqiMeanDl``, ``cqiStddevDl`` and their
   uplink counterparts) instead of computing it.

*  With ``enablePeriodicCqiUpdate = false``, the CQI is drawn once from the
   same distribution at initialization.

Drawing the CQI is cheaper than computing it, and it lets a study reproduce
the channel quality that a full simulation measured: the shipped
``simulations/nr/bgTraffic`` configurations include per-UE CQI and
retransmission parameters calibrated this way.

Background cells
----------------

A :ned:`BackgroundCell` is a lightweight base station: a position, a
:ned:`BackgroundScheduler`, a background traffic generator with its background
UEs, and a radio (:ned:`CellularPhantomRadio`) whose power, direction and
antenna angle are the scheduler's (``txPower``, ``txDirection``, ``txAngle``).
It has no protocol stack, sends and receives no packets, and has no real UEs.

Its scheduler runs every slot of its numerology (``numerologyIndex``) on one
carrier (``carrierFrequency``, ``numBands``): in the uplink, it first grants
random access and retransmissions, then serves the backlogged background UEs
in the order of their CQI (Max C/I); in the downlink, it serves retransmissions
and then the backlogged UEs the same way. The resource blocks it allocates are
put on the radio medium: the downlink blocks as a transmission of the base
station, the uplink blocks of each background UE as a transmission of that UE.
A background cell covers one carrier; a study that needs interference on
several carriers uses one background cell per carrier.

Background cells are part of the reference networks: their ``numBgCells``
parameter instantiates them (``bgCell[]``), and the configuration places them
and their UEs.

External cells
--------------

An :ned:`ExtCell` is the simplest of the three: a fictitious neighboring cell
at a position (``positionX``, ``positionY``, ``positionZ``), with a transmit
power (``txPower``), an omnidirectional or sectorial antenna (``txDirection``,
``txAngle``), and a downlink occupation pattern on a carrier
(``carrierFrequency``, ``numBands``), chosen by ``bandAllocationType``:

``FULL_ALLOC``
   Every band, all the time.

``RANDOM_ALLOC``
   Every band independently, with probability ``bandUtilization``, drawn anew
   every TTI.

``CONTIGUOUS_ALLOC``
   A block of contiguous bands, ``bandUtilization`` of them, from
   ``startingOffset`` on, the same every TTI.

An external cell has no UEs and no uplink: it is a downlink interferer only.
The LTE reference networks :ned:`MultiCell` and :ned:`MultiCell_X2Mesh` have a
``numExtCells`` parameter for them.

How the interference reaches the real cells
-------------------------------------------

All three put their transmissions on the radio medium. The background and
external cells, and the background UEs, send them through phantom radios
(:ned:`CellularPhantomRadio`): radios with a transmitter and an antenna but no
receiver, which the medium evaluates like any other radio. The channel model
of the receiving node's carrier and technology decides what it counts (see
:doc:`radio-medium`):

*  ``bgCellInterference`` (true by default) counts the background cells and
   their background UEs, in the downlink and the uplink SINR;

*  ``extCellInterference`` (true by default) counts the external cells, in the
   downlink SINR.

The background UEs of a real cell are different: their transmissions are the
real cell's, in the downlink sent by the base station's own radio, so they
count as that cell's interference, which ``downlinkInterference`` and
``uplinkInterference`` govern together with the real UEs'. Both are false by
default (see :doc:`radio-medium`). So with the default switches, a cell's
background UEs load that cell but do not interfere with its neighbors, while
background and external cells interfere with every real cell.

Configuration
-------------

**Load a gNB with background UEs.** From the ``CBR-DL`` configuration of
``simulations/nr/bgTraffic``:

.. code:: ini

   *.gnb.cellularNic.bgTrafficGenerator[0].numBgUes = 15
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].mobility.initialX = \
           uniform(750m,1250m)
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].mobility.initialY = \
           uniform(750m,1250m)
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].generator.typename = \
           "TrafficGeneratorCbr"
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].generator.startTimeDl = \
           uniform(0s,0.05s)
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].generator.packetSizeDl = \
           1000B
   *.gnb.cellularNic.bgTrafficGenerator[0].bgUE[*].generator.periodDl = 0.01s

**Surround the cell with background cells.** In the same configuration, each
background cell gets a position, its UEs around it, and the carrier settings
of the real cell:

.. code:: ini

   *.numBgCells = 3
   *.bgCell[*].bgTrafficGenerator.numBgUes = 15
   *.bgCell[0].mobility.initialX = 750m
   *.bgCell[0].mobility.initialY = 567m
   *.bgCell[0].bgTrafficGenerator.bgUE[*].mobility.initialX = \
           uniform(500m,1000m)
   *.bgCell[0].bgTrafficGenerator.bgUE[*].mobility.initialY = \
           uniform(317m,817m)
   *.bgCell[*].bgScheduler.isNr = true
   *.bgCell[*].bgScheduler.numBands = 100
   *.bgCell[*].bgScheduler.numerologyIndex = 2

**Add an external interferer.** From ``simulations/lte/multicell``:

.. code:: ini

   *.extCell[*].txPower = 20dBm
   *.extCell[*].txDirection = "ANISOTROPIC"
   *.extCell[*].bandUtilization = 0.5
   *.extCell[0].positionX = 100m
   *.extCell[0].positionY = 600m
   *.extCell[0].txAngle = 315deg

**Use calibrated CQIs instead of computed ones.** Set
``useProbabilisticCqi = true`` on the traffic generator, and give each
background UE its ``cqiMeanDl``, ``cqiStddevDl``, ``cqiMeanUl``,
``cqiStddevUl``, and ``rtxRateDl`` and ``rtxRateUl``, as the files under
``simulations/nr/bgTraffic/trafficGeneratorConfigs`` do.

Statistics
----------

*  The background traffic generators record the SINR and the CQI of their
   background UEs (``bgMeasuredSinrDl``/``Ul``, ``bgAverageCqiDl``/``Ul``) and
   their retransmission rate (``bgHarqErrorRateDl``/``Ul``).

*  A background cell's scheduler records the average number of resource
   blocks it serves per slot (``bgAvgServedBlocksDl``/``Ul``).

The real cell's own statistics (see :doc:`mac`) show the load the background
UEs put on it.

Example simulations
-------------------

*  ``simulations/nr/bgTraffic``: a gNB with background UEs, surrounded by 3,
   6 or 9 background cells, in the downlink and the uplink;

*  ``emulation/extclientserver_bgTraffic``: the same background in a
   real-time emulation (see :doc:`emulation`);

*  ``simulations/lte/multicell``: external cells with the three allocation
   types;

*  ``simulations/nr/videostreaming_dataset_generator``: background cells and
   UEs in an urban scenario with moving UEs.

Fidelity
--------

*  The background sends no packets: its traffic is a buffer model, its
   reception a CQI and a retransmission probability. The background UEs record
   no application or protocol statistics.

*  The background UEs of a real cell are served only by the ``MAXCI``
   scheduling policy, the default; the other policies do not see them.

*  A background UE's retransmissions follow its configured probability, not
   the error rate of its SINR, and its uplink data waits a fixed six slots
   after its first buffer status report.

*  An external cell occupies the downlink only, and its pattern does not
   react to load.

*  Known deviation: the links of the background UEs and of the background and
   external cells have no shadowing, while TR 38.901 7.5 gives every link one;
   and a real UE's link to a background cell is shadowed in the uplink
   what-if but not in the downlink. The spread of the background UEs' SINR is
   narrower than it would be (tests/doc/model-deviations.md item 31).

.. TODO: two pending fixes are assumed above (proposed merge-gate items):
   model-deviations.md item 18 (a background UE on a cell's second carrier is
   registered on the medium with the cell-wide band index; fix: subtract the
   carrier's first band) and item 20 (a background UE's uplink allocation is
   put on the medium only if its own cell's channel model has
   uplinkInterference on; fix: register every allocation). The text says
   every allocation is put on the medium, on the right bands. Check that both
   have landed before the guide is published.
