Applications and Traffic Models
===============================

Overview
--------

In a simulation study, the applications are the traffic: they define the
offered load -- its rate, its packet sizes, its timing -- and they measure what
the network delivers end to end. The traffic model decides what a study can
show. Constant bit rate traffic exposes the capacity and the delay of a
scheduler; voice traffic, with its talk spurts and silences, the handling of
small periodic packets and the quality a listener perceives; video, the
delivery of a stream whose layers differ in importance; bursty traffic, the
reaction of the network to load peaks. The end-to-end measures are delay,
jitter, packet loss, throughput, and quality scores such as the Mean Opinion
Score (MOS) of voice.

Simu5G runs any INET application: the ``app[]`` vector of a UE or a server
takes any module that implements INET's :ned:`IApp`, so INET's UDP and TCP
applications work unchanged over the cellular network. Simu5G adds traffic
models of its own, in sender and receiver pairs over UDP: constant bit rate
(:ned:`CbrSender`, :ned:`CbrReceiver`), voice (:ned:`VoipSender`,
:ned:`VoipReceiver`), video on demand from a trace file (:ned:`VodUdpServer`,
:ned:`VodUdpClient`), bursts (:ned:`BurstSender`, :ned:`BurstReceiver`) and
event alerts (:ned:`AlertSender`, :ned:`AlertReceiver`). The receivers record
the end-to-end measures themselves, and the voice receiver computes the MOS
with the E-model. Further applications serve particular features: the MEC
applications (see :doc:`mec`), the D2D multihop application
(:ned:`MultihopD2D`), and the application of an Unstructured session
(:ned:`UnstructuredApp`), which is attached to the cellular NIC directly.
Simu5G's own traffic models run over UDP only; TCP traffic comes from INET's
applications.

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
