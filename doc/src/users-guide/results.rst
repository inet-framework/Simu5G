Collecting Results
==================

Overview
--------

Simu5G records results through OMNeT++'s standard signal/statistic
mechanism: modules declare ``@statistic`` properties in their NED files,
and recording is controlled from the INI file exactly as described in the
OMNeT++ Simulation Manual (``**.result-recording-modes``,
``**.scalar-recording``, ``**.vector-recording``, and per-statistic
filters). The model library declares over two hundred statistics; this
chapter points out the ones most evaluations need, layer by layer. The
generated NED documentation lists them all.

One Simu5G-specific detail: throughput statistics use a custom result
recorder, ``simu5g_rateavg``, which averages the received bits over the
measurement time -- keep this in mind when post-processing raw vectors
yourself.

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
