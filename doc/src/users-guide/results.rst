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
file. The statistics cover the applications and the layers
of the radio protocol stack at the UEs and the base stations, from RLC to the
PHY's SINR and CQI. Throughput statistics use Simu5G's own result recorder,
``simu5g_rateavg``, a time-weighted average of the rate values in which each
value holds for the interval before its emission. Besides statistics, the
radio medium can write a reception trace, one line per reception decision and
per channel evaluation (the ``recordReceptionTrace`` parameter of
:ned:`CellularRadioMedium`). The analysis of the results needs nothing
Simu5G-specific: the OMNeT++ tools apply.

Recording is controlled from the ini file exactly as described in the OMNeT++
Simulation Manual (``**.result-recording-modes``, ``**.scalar-recording``,
``**.vector-recording``, and per-statistic filters). This chapter points out
the statistics most evaluations need, layer by layer; the generated NED
documentation lists them all.

Application level
-----------------

Simu5G's traffic models record end-to-end QoS directly (see
:doc:`apps`): ``cbrFrameDelay``, ``cbrJitter``, ``cbrFrameLoss``,
``cbrReceivedThroughput`` for CBR; ``voipFrameDelay``, ``voipJitter``,
``voipFrameLoss`` and the ``voipMos`` opinion score for VoIP; per-layer
statistics for video streaming. When using INET applications instead,
INET's own end-to-end statistics apply unchanged.

Radio stack
-----------

The most commonly used stack-level statistics:

*  **MAC** (per node): ``macThroughputDl``/``Ul`` and
   ``macDelayDl``/``Ul``; HARQ behavior via ``harqErrorRateDl``/``Ul``
   (plus per-attempt variants ``harqErrorRate_1st_Dl`` ..
   ``harqErrorRate_4th_Dl``) and ``harqTxAttemptsDl``/``Ul``.
*  **Scheduler** (base stations): ``avgServedBlocksDl``/``Ul`` -- the
   resource blocks allocated per TTI, i.e. cell load.
*  **RLC**: ``rlcThroughputDl``/``Ul``, ``rlcDelayDl``/``Ul``.
*  **PHY / channel**: ``averageCqiDl``/``Ul`` at the UE;
   ``rcvdSinrDl``/``Ul`` and ``measuredSinrDl``/``Ul`` at the channel
   model.
*  **Mobility/handover**: ``servingCell`` -- the UE's serving cell id
   over time.
*  **D2D**: D2D-mode counterparts ``macThroughputD2D``, ``macDelayD2D``,
   ``harqErrorRateD2D``, ``rcvdSinrD2D``.
*  **Background traffic**: proxies recorded by background UEs
   (``bgAverageCqiDl``/``Ul``, ``bgHarqErrorRateDl``/``Ul``,
   ``bgMeasuredSinrDl``/``Ul``) and background cells
   (``bgAvgServedBlocksDl``/``Ul``).

Analyzing results
-----------------

Result files (``.sca``, ``.vec``) are processed with the standard OMNeT++
toolchain -- the Analysis Tool of the IDE, ``opp_scavetool``, or the
``omnetpp`` Python package. See the OMNeT++ documentation; nothing about
it is Simu5G-specific.

.. TODO: add a worked example (record CBR delay + cell load in a multicell
   scenario, produce one plot) once the manual gets a tutorial appendix.
