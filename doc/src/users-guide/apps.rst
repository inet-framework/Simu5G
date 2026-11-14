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

This chapter describes how to use INET's applications over the cellular
network, then each of Simu5G's traffic models: what it generates, and what its
receiver measures. The MEC applications are described in :doc:`mec`, the
background traffic, which loads the radio resources without applications, in
:doc:`background`.

Using INET applications
-----------------------

A UE is an INET :ned:`StandardHost` (see :doc:`ue`), and the servers of the data
network are usually :ned:`StandardHost` modules too, so every INET application
runs between them: :ned:`UdpBasicApp` and :ned:`UdpSink`, :ned:`TcpSessionApp`
and :ned:`TcpSinkApp`, :ned:`UdpVideoStreamServer` and
:ned:`UdpVideoStreamClient`, and the rest of INET's catalog. The application
addresses its peer by module name, as in any INET network; the UE's address is
that of its cellular interface (see :doc:`ue` for naming it explicitly with
``%cellular``). For TCP traffic, INET's applications are the only choice:
Simu5G's own traffic models run over UDP.

A UE with an Ethernet session runs INET's raw Ethernet applications, such as
:ned:`EthernetSourceApp` and :ned:`EthernetSinkApp`, instead (see
:doc:`ethernet-sessions`).

Constant bit rate
-----------------

:ned:`CbrSender` sends a packet of ``packetSize`` bytes every ``samplingTime``,
from ``startTime`` to ``finishTime``, to ``destAddress`` and ``destPort``.
:ned:`CbrReceiver` measures the delay of every packet, the throughput, and the
frame loss: the fraction of the packets the sender sends between its start and
finish time that do not arrive. Constant bit rate traffic is the usual choice
for capacity and scheduling studies, because it offers a known, steady load.

Voice
-----

:ned:`VoipSender` models a voice call: it alternates talk spurts and silences,
whose durations it draws from Weibull distributions (``scaleTalk`` and
``shapeTalk``, ``scaleSil`` and ``shapeSil``). During a talk spurt it sends a
frame of ``packetSize`` bytes (40 bytes by default) every ``samplingTime`` (20
ms); during a silence it sends nothing. With ``silences = false`` it talks all
the time.

:ned:`VoipReceiver` models the listener's side. It collects the frames of a
talk spurt and plays them out through a playout buffer of ``dimBuffer`` frames,
starting ``playoutDelay`` after the first frame's arrival: a frame that arrives
after its playout time is lost to the playout, and a frame that finds the
buffer full is lost to tail drop. The playout delay adapts from talk spurt to
talk spurt: it is adjusted by the largest lateness of the current talk spurt's
frames, so it grows after late frames and shrinks when every frame arrived
early. At the end of each talk spurt, the receiver computes the Mean
Opinion Score (MOS) of the call with the E-model of ITU-T G.107, from the
playout delay and the total loss (lost on the network, late and dropped).
The E-model's parameters are ``emodel_Ro`` (the basic signal-to-noise ratio),
``emodel_A`` (the advantage factor), and ``emodel_Ie`` and ``emodel_Bpl``, the
codec's equipment impairment factor and packet-loss robustness factor, which
ITU-T G.113 tabulates per codec.

Video on demand
---------------

:ned:`VodUdpServer` streams a video encoded with Scalable Video Coding (SVC)
from a trace file (``vodTraceFile``): each line of the trace gives a frame's
size and time. The server serves the clients listed in its ``destAddresses``
parameter. :ned:`VodUdpClient` receives the stream and records the throughput
and the delay of each of the first four SVC layers separately, so that a study
can see how the layers of different importance fare.

Bursts
------

:ned:`BurstSender` sends bursts of ``burstSize`` packets, one every
``intraBurstTime`` within the burst, with ``interBurstTime`` between the
bursts. :ned:`BurstReceiver` records the delay of every packet. Bursty traffic
shows how the scheduler and the buffers react to load peaks.

Alerts
------

:ned:`AlertSender` sends small messages periodically (``period``) to a unicast
or a multicast address; :ned:`AlertReceiver` joins the multicast group and
records the delay of every message. With a multicast address, the alerts of a
D2D-capable UE travel as D2D multicast to the UEs around it, which makes the
pair the traffic of the D2D multicast examples (see :doc:`d2d`).

Unstructured sessions
---------------------

A UE whose session is of type Unstructured has no IP layer: its application,
:ned:`UnstructuredApp`, is attached to the cellular NIC directly, and its
payload goes over the session to a server in the data network (see
:doc:`pdu-sessions`). The application is built from INET's queueing elements,
like INET's Ethernet applications: a packet source (by default
:ned:`ActivePacketSource`), whose packet length and production interval set the
traffic, and a packet sink (:ned:`PassivePacketSink`).
:ned:`UnstructuredSourceApp` and :ned:`UnstructuredSinkApp` only send and only
receive. A UE has at most one such application, in its ``unstructuredApp``
vector (``numUnstructuredApps``).

D2D multihop dissemination
--------------------------

:ned:`MultihopD2D` disseminates event notifications over several D2D multicast
hops: an :ned:`EventGenerator` module signals events (for example a collision
between vehicles) to the application of a UE, which sends a notification to an
IP multicast address, carried as D2D multicast; a UE that receives it relays it
further. To limit flooding, a message has a time to live (``ttl``) and a target
broadcast radius (``maxBroadcastRadius``), and with ``trickle = true`` the
Trickle suppression mechanism keeps a UE from relaying a message of which it
has already received ``k`` duplicates. A :ned:`MultihopD2DStatistics` module
collects the statistics of the dissemination over the target area of each
message. The model is that of G. Nardini, G. Stea, A. Virdis, "A fast and
reliable broadcast service for LTE-Advanced exploiting multihop
device-to-device transmissions", Future Internet, 2017. See :doc:`d2d`.

MEC applications
----------------

The applications of the ETSI MEC model -- the device application of a UE
(:ned:`DeviceApp`), the MEC applications that run on MEC hosts, and the sample
pairs such as the warning alert, request-response and real-time video
streaming applications -- are described in :doc:`mec`.

Configuration
-------------

Every task below sets the ``app[]`` vectors of the UEs and the server; the
number of applications is ``numApps``, and each application's type is its
``typename``.

**Send constant bit rate traffic from the server to a UE.** From the
``simulations/nr/rlc`` examples:

.. code:: ini

   *.ue[*].numApps = 1
   *.server.numApps = 1

   *.ue[*].app[0].typename = "CbrReceiver"
   *.ue[*].app[0].localPort = 3000

   *.server.app[0].typename = "CbrSender"
   *.server.app[0].destAddress = "ue[0]"
   *.server.app[0].destPort = 3000
   *.server.app[0].packetSize = 40B
   *.server.app[0].samplingTime = 0.01s   # 100 packets per second
   *.server.app[0].startTime = 1s
   *.server.app[0].finishTime = 18s

**Make a voice call in the uplink.** The UE runs the sender, the server the
receiver, as in the ``VoIP-UL`` configuration of ``simulations/nr/standalone``:

.. code:: ini

   *.ue[*].app[*].typename = "VoipSender"
   *.ue[*].app[*].destAddress = "server"
   *.ue[*].app[*].startTime = uniform(0s,0.02s)
   *.server.app[*].typename = "VoipReceiver"

**Transfer bulk data over TCP.** With INET's TCP applications, as in the
``TCP-AM`` configuration of ``simulations/lte/rlc``:

.. code:: ini

   **.ue[*].app[0].typename = "TcpSinkApp"
   **.ue[*].app[0].localPort = 1000
   **.server.app[0].typename = "TcpSessionApp"
   **.server.app[0].connectAddress = "ue[0]"
   **.server.app[0].connectPort = 1000
   **.server.app[0].active = true
   **.server.app[0].tOpen = 0.5s
   **.server.app[0].tSend = 1s
   **.server.app[0].sendBytes = 1GiB

**Send alerts to a multicast group over D2D.** From
``simulations/lte/d2d_multicast``; the receiving UEs join the group
``224.0.0.10`` through the network's configurator:

.. code:: ini

   *.ueD2D[0].app[*].typename = "AlertSender"
   *.ueD2D[0].app[*].destAddress = "224.0.0.10"
   *.ueD2D[0].app[*].destPort = 1000

Statistics
----------

The receivers of Simu5G's traffic models record the end-to-end measures
themselves; the senders record what they generate:

*  :ned:`CbrReceiver`: the delay of each packet (``cbrFrameDelay``), the
   received throughput (``cbrReceivedThroughput``) and the frame loss
   (``cbrFrameLoss``). The frame loss is computed at the end of the run from
   the number of packets the sender sends between its ``startTime`` and
   ``finishTime``, so it is meaningful only when ``finishTime`` is set and the
   run lasts until then.

*  :ned:`VoipReceiver`: the delay of each frame (``voipFrameDelay``), the loss
   on the network (``voipFrameLoss``), the playout delay
   (``voipPlayoutDelay``), the loss to late arrival (``voipPlayoutLoss``) and
   to tail drop (``voipTaildropLoss``), the lateness of the late frames
   (``voipJitter``), the MOS (``voipMos``), and the received throughput.

*  :ned:`VodUdpClient`: the throughput and the delay per SVC layer
   (``VoDTptLayer0`` to ``3``, ``VoDDelayLayer0`` to ``3``).

*  :ned:`BurstReceiver` and :ned:`AlertReceiver`: the delay of each packet
   (``burstPktDelay``, ``alertDelay``) and the received packets.

*  :ned:`UnstructuredApp`: the packets sent and received, counted by its
   ``io`` submodule (:ned:`UnstructuredIo`).

*  :ned:`MultihopD2D`: the generated, sent, received, duplicate and suppressed
   messages, with the dissemination statistics of
   :ned:`MultihopD2DStatistics`.

Throughputs are recorded with Simu5G's ``simu5g_rateavg`` recorder (see
:doc:`results`).

Example simulations
-------------------

*  ``simulations/nr/standalone``: voice in the downlink and the uplink;

*  ``simulations/nr/rlc`` and ``simulations/lte/rlc``: constant bit rate
   traffic, and TCP over RLC AM;

*  ``simulations/lte/d2d_multicast``: alerts over D2D multicast;

*  ``simulations/lte/d2d_multihop``: multihop dissemination;

*  ``simulations/nr/unstructured``: the application of an Unstructured
   session.

No shipped simulation uses :ned:`VodUdpServer` or :ned:`BurstSender`.

Fidelity
--------

*  The traffic models are application models, not codecs or protocols: a
   voice frame, a video frame or a burst is a packet of a given size, and its
   content is not simulated.

*  The talk spurt and silence durations follow Weibull distributions with
   configurable parameters; no particular 3GPP or ITU traffic model is
   implemented.

*  The E-model's delay impairment uses the common simplified form, a linear
   function of the delay with a knee at 177.3 ms, rather than the full delay
   formulas of ITU-T G.107; the default ``emodel_Ie`` and ``emodel_Bpl`` do not
   correspond to a particular codec of ITU-T G.113.

*  Known deviation: the MOS takes the playout delay as the delay of the call,
   leaving out the delay of the network. ITU-T G.107's delay impairment is a
   function of the mouth-to-ear delay, which includes the network's, so the
   MOS is too optimistic where the network delay is large.

*  The playout delay adapts by one sample: the delay of the next talk spurt
   follows from the largest lateness of the current one, with no smoothing.
