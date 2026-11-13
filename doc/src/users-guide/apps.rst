Applications and Traffic Models
===============================

Overview
--------

Simu5G nodes accept any INET application: the ``app[]`` vector of UEs,
servers and base stations takes ``IApp``-compatible modules, so INET's
UDP/TCP applications work unchanged over the cellular network. On top of
that, Simu5G provides a set of traffic models typical of cellular
evaluations, most of them coming in sender/receiver pairs and recording
application-level QoS statistics (delay, jitter, loss, throughput) out of
the box.

All of them live under ``src/simu5g/apps/``; parameters below are the
essential ones, see the NED documentation for the full lists.

Traffic generators
------------------

``CbrSender`` / ``CbrReceiver``
   Constant bit rate traffic over UDP: a packet of ``packetSize`` every
   ``samplingTime``. The receiver records frame delay, jitter, loss and
   throughput -- the go-to traffic model for scheduling and capacity
   experiments.

``VoipSender`` / ``VoipReceiver``
   Voice traffic with talk-spurt/silence alternation (Weibull-distributed
   durations, configurable via ``shapeTalk``/``scaleTalk`` etc.). The
   receiver models a playout buffer (``playoutDelay``, ``dimBuffer``) and
   computes a Mean Opinion Score (``voipMos``) using the E-model
   parameters, alongside delay/jitter/loss statistics.

``VodUdpServer`` / ``VodUdpClient``
   Video-on-demand streaming from an SVC trace file (``vodTraceFile``),
   with per-layer reception statistics at the client.

``BurstSender`` / ``BurstReceiver``
   On-off bursty traffic: bursts of ``burstSize`` packets spaced
   ``interBurstTime`` apart.

``AlertSender`` / ``AlertReceiver``
   Small periodic event messages to a unicast or multicast destination;
   designed for D2D and vehicular scenarios (the receiver can join
   multicast groups).

``MultihopD2D``
   Multihop dissemination of event notifications over D2D multicast, with
   TTL and broadcast-radius limiting and optional Trickle suppression;
   paired with an ``EventGenerator`` and a dedicated statistics collector.
   See :doc:`d2d`.

MEC applications
----------------

The applications modeling the ETSI MEC ecosystem -- the UE-side
``DeviceApp``, MEC apps deployed on MEC hosts, and the sample
warning-alert, request-response and RNIS test app pairs -- are described
in :doc:`mec`.
