Collecting Results
==================

Overview
--------

In a simulation study, the results are the measurements the runs record: the
performance figures of the study, such as delay, throughput or the use of the
radio resources. They are recorded where they arise -- end to end at the
applications, or per node and per layer in the protocol stack -- as values over
time (vectors), as summaries (scalars), or as distributions (histograms). A
study records the results it needs selectively, because recording everything
costs run time and disk space, and then processes them into plots and tables.

Simu5G records its results through the standard OMNeT++ mechanism: its modules
emit signals, and the statistics declared in their NED files (``@statistic``)
turn them into scalars, vectors and histograms, under the control of the ini
file. The statistics cover the applications and the layers of the radio
protocol stack at the UEs and the base stations, from RLC to the PHY's SINR
and CQI. Throughput statistics use Simu5G's own result recorder,
``simu5g_rateavg``, a time-weighted average of the rate values in which each
value holds for the interval before its emission. Besides statistics, the
radio medium can write a reception trace, one line per reception decision and
per channel evaluation (the ``recordReceptionTrace`` parameter of
:ned:`CellularRadioMedium`). The analysis of the results needs nothing
Simu5G-specific: the OMNeT++ tools apply.

This chapter explains how Simu5G's statistics are recorded and where to find
them, gives a map of the main statistics by layer, describes the radio
network measurements of the MEC services and the reception trace, and ends
with a short worked example of an analysis. Each layer's statistics are
described in more detail in the layer's own chapter.

How statistics are recorded
---------------------------

A Simu5G module emits a signal whenever something measurable happens -- a
packet arrives, a transport block is decoded, a CQI is reported -- and the
module's NED declaration says which statistics are computed from the signal
and how they are recorded: as a scalar (the mean, a count, a sum), as a vector
of every value with its time, or as a histogram. Statistics are recorded in
the module that declares them, so their names carry the module path, for
example ``SingleCell_Standalone.ue[0].cellularNic.mac`` for the MAC statistics
of a UE.

Recording is controlled from the ini file as described in the OMNeT++
Simulation Manual. Typical settings:

.. code:: ini

   # record scalars everywhere, but vectors only for the application delay
   **.vector-recording = false
   **.app[*].cbrFrameDelay:vector.vector-recording = true

   # discard the results of the first two seconds
   warmup-period = 2s

Some statistics exist in more than one copy. The statistics of an RLC entity
are recorded in the entity itself, one copy per bearer, and again, aggregated,
by the NIC's :ned:`RlcMux` (see :doc:`rlc`). On a radio that serves more than
one carrier, the SINR statistics are recorded once more per carrier, under the
statistic's name followed by the carrier frequency, for example
``measuredSinrDl-3500MHz``. These per-carrier statistics, and the D2D SINR
statistic ``rcvdSinrD2D``, are created at run time from templates, so they
appear only in the runs that need them.

Throughput statistics are recorded with ``simu5g_rateavg``, a result recorder
of Simu5G: each emitted value is the rate of the interval that ends at the
emission, and the recorder averages these rates weighted by the length of
their intervals. A NaN value marks an interval to leave out. The result is one
scalar per statistic, for example ``rlcThroughputDl:simu5g_rateavg``.

The main statistics
-------------------

The following map names the statistics most studies need, by layer; the NED
Reference lists them all, with their units and recording modes.

Applications
   Simu5G's traffic models record their end-to-end figures at the receiver:
   ``cbrFrameDelay``, ``cbrJitter``, ``cbrFrameLoss`` and
   ``cbrReceivedThroughput`` for constant bit rate traffic;
   ``voipFrameDelay``, ``voipJitter``, ``voipFrameLoss``, the playout
   statistics and the Mean Opinion Score ``voipMos`` for voice; the delay and
   throughput per video layer (``VoDDelayLayer0`` ...) for video on demand;
   and the delay and counts of the burst and alert applications. The senders
   record what they generate. INET's applications record INET's statistics.
   See :doc:`apps`.

PDCP
   PDCP records no statistics of its own (see :doc:`pdcp`).

RLC
   ``rlcDelayDl``/``Ul`` and ``rlcThroughputDl``/``Ul`` per bearer, as
   delivered to PDCP, and the same per PDU (``rlcPduDelayDl`` ...); the AM
   entities also record their window occupation and their retransmissions.
   See :doc:`rlc`.

MAC
   Per UE: ``macDelayDl``/``Ul``, ``macThroughputDl``/``Ul``, the HARQ error
   rates ``harqErrorRateDl``/``Ul`` overall and per attempt, the number of
   attempts ``harqTxAttemptsDl``/``Ul``, and the loss after the last HARQ
   attempt ``macPacketLossDl``/``Ul``. Per cell, at the base station: the
   resource blocks allocated per slot, ``avgServedBlocksDl``/``Ul``, which
   measure the cell's load. See :doc:`mac`.

PHY
   At the UE: the SINR of each reception and of each channel quality
   measurement, ``rcvdSinrDl``/``Ul`` and ``measuredSinrDl``/``Ul``, recorded
   by the radio's receiver (:ned:`CellularReceiver`), the uplink figures as
   measured at the base station; the CQI, ``averageCqiDl``/``Ul``; and the
   distance from the serving base station, ``distance``. A base station's
   receiver records no SINR statistics, so per-cell SINR figures are
   aggregated from the UEs. See :doc:`phy`.

Handover
   ``servingCell``, a vector of the serving cell id of each UE leg over time,
   recorded by its connection control (see :doc:`handover`).

Background traffic and CoMP
   The background UEs record their average CQI, HARQ error rate and SINR
   (``bgAverageCqiDl`` ...), the background cells their served blocks
   (``bgAvgServedBlocksDl``/``Ul``), and the CoMP manager the blocks it
   reserves (``compReservedBlocks``). See :doc:`background` and :doc:`x2`.

D2D
   The MAC, RLC and SINR statistics have D2D counterparts
   (``macThroughputD2D``, ``rlcThroughputD2D``, ``rcvdSinrD2D`` ...); see
   :doc:`d2d`.

Radio network information for MEC
---------------------------------

The Radio Network Information Service of a MEC host (see :doc:`mec`) reports
layer-2 measurements of the cells and the UEs: resource block usage, data
volumes, packet delays, discard and loss rates, throughputs. Simu5G computes
them in two kinds of modules, present in a node when its ``hasRniSupport``
parameter is true:

*  the statistics collector of the node (:ned:`BaseStationStatsCollector`,
   :ned:`GNodeBStatsCollector`, :ned:`UeStatsCollector`,
   :ned:`NrUeStatsCollector`), which keeps the measurements over configurable
   periods, as averages or moving averages;

*  the packet flow observer of the NIC (``packetFlowObserver``), which follows
   the packets through PDCP, RLC and MAC by listening to their signals, and
   derives the delay, discard and loss figures from them.

These measurements are not OMNeT++ statistics: they are kept for the MEC
service, which reports them to the MEC applications that request them.

The reception trace
-------------------

For debugging, and for comparing two versions of the radio model transmission
by transmission, the radio medium can write a reception trace. With

.. code:: ini

   **.radioMedium.recordReceptionTrace = true

the medium writes ``${resultdir}/${configname}-${runnumber}.rxtrace``, a text
file with one line per reception decision -- transmitter, receiver, carrier,
direction, CQI, HARQ process and attempt, the SINR of every resource block,
the interferers, the error rate, the random draw and the outcome -- and one
line per channel evaluation that feeds a decision (a CQI, a beacon, the
received power of the initial cell selection). The format is documented in
the ``RxTrace.h`` source file. Writing the trace draws no random number and
schedules no event, so a run with the trace produces the same results as one
without it.

Analyzing results
-----------------

The result files are the standard OMNeT++ ``.sca`` and ``.vec`` files, so the
OMNeT++ tools apply: the Analysis Tool of the IDE, the ``opp_scavetool``
command, and the ``omnetpp.scave`` Python package. As an example, the
``CBR-DL`` configuration of ``simulations/nr/standalone_multicell`` runs two
cells with 1, 10 and 30 UEs (the ``numUEs`` iteration variable). After the
runs,

.. code:: shell

   opp_scavetool export -F CSV-R -o cbr-delay.csv \
       -f 'name =~ "cbrFrameDelay:mean"' results/CBR-DL/*.sca

exports the mean frame delay of every UE into a CSV file. The same in Python,
averaged over the UEs of each run:

.. code:: python

   from omnetpp.scave import results

   df = results.read_result_files(
       "results/CBR-DL/*.sca",
       filter_expression='name =~ "cbrFrameDelay:mean"')
   scalars = results.get_scalars(df, include_itervars=True)
   print(scalars.groupby("numUEs")["value"].mean())

The cell load of the same runs is the ``avgServedBlocksDl:mean`` scalar of the
two base stations' MACs.
