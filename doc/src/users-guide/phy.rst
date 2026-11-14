The PHY Abstraction
===================

Overview
--------

The physical layer (PHY; 3GPP TS 38.201 and TS 38.211 to TS 38.214 for NR,
TS 36.201 and TS 36.211 to TS 36.213 for LTE) sits below MAC and carries the
MAC's transport blocks over the air, in the resource blocks the scheduler
allocated. Its main functions are channel coding and modulation, HARQ soft
combining, multi-antenna processing (MIMO), and the measurements that drive
link adaptation and mobility. For coding and modulation, the transmitter
protects each transport block with a CRC and a channel code and maps it onto
modulation symbols, at the modulation and coding scheme (MCS) the MAC chose;
the receiver decodes the block and checks its CRC. For HARQ, a receiver that
fails to decode a block keeps what it received and combines it with the
retransmission, so a retransmission is more likely to succeed than the first
attempt. For link adaptation, the UE measures the downlink channel and
reports channel state information (CSI), whose channel quality indicator
(CQI) tells the base station the highest MCS the UE can receive at a target
block error rate. The UE also measures the received power of the cells around
it, which drives cell selection and handover.

Simu5G models the PHY at the level of transport blocks and resource blocks, not
OFDM symbols. The radio medium computes the SINR of each resource block of a
reception (see :doc:`radio-medium`), and the receiver of the node's radio
decides whether the transport block is received, against an error model. The
default error model, :ned:`BlerCurveErrorModel`, takes the error probability of
each resource block from BLER curves at the reception's CQI and SINR, and stands
in for soft combining by reducing the error probability by a fixed factor at
each HARQ retransmission; :ned:`ConfiguredRateErrorModel` uses a configured
error rate instead. The UE sends periodic CQI reports, and the base stations
send beacons, whose SINR the UEs measure for handover. Channel coding,
modulation and the CRC are not simulated, and transmissions are single-layer:
the reported rank is always 1, so spatial multiplexing is not modeled. A known
deviation is that the CQI has no measurement delay, while TS 38.214 5.2.2.5
measures it several slots before the report.

This chapter covers the node's side of the radio link: the radio with its
antenna, transmitter and receiver, the reception decision, the channel quality
feedback, the beacons, and the transmit power. What happens between the
antennas -- path loss, shadowing, fading, interference, and the SINR that
results -- is the radio medium's, and is described in :doc:`radio-medium`.
This division follows INET: the medium models the propagation up to the
receiving antenna, and the node decides what to make of a reception.

.. TODO: figure: a transmission from the MAC of one node to the MAC of the
   other -- PHY, radio (transmitter), medium (channel model, SINR), radio
   (receiver, error model), PHY -- drawn, not a NED screenshot. The old
   phy_processing.png showed the PHY computing the SINR with the Binder's
   list of interferers, which is no longer the case.

The node's radio
----------------

Every stack leg of a node has a radio, :ned:`CellularRadio`: the ``radio``
submodule of the cellular NIC, and at an :ned:`NrUe`, ``nrRadio`` for the NR
leg. The radio is the node-side end of the radio medium, and has three parts:

``antenna`` (:ned:`CellularAntenna`)
   The antenna gain, for transmission and reception, and the antenna's
   direction: omnidirectional, or sectorial towards a boresight angle. By
   default a base station's antenna has 18 dBi of gain and a UE's 0 dBi, and
   both are omnidirectional.

``transmitter`` (:ned:`CellularTransmitter`)
   Turns each frame the PHY sends into the transmission the medium keeps: its
   power, its resource blocks, its carrier, its direction, its start and its
   duration.

``receiver`` (:ned:`CellularReceiver`)
   The noise figure (5 dB at a base station, 7 dB at a UE by default) and the
   cable loss between the antenna and the receiver (2 dB), the reception
   decision, and the SINR statistics. The decision is made against the
   receiver's error model, the ``errorModel`` submodule.

The PHY module of the leg, :ned:`PhyEnb` at a base station and :ned:`PhyUe`
at a UE, sits between the MAC and the radio. It sends every frame through the
radio and tells it where the frame goes, and it receives every frame that
arrives for the leg from the radio.

A transmission, step by step
----------------------------

When the MAC of a node has a transport block to send, it passes it to the PHY
with the transmission's parameters: the destination, the carrier, the resource
blocks, the transmission format and the CQI it was chosen for. The PHY stamps
the transmit power, and sends the frame through its radio's transmitter into
the radio medium.

The medium delivers the frame to the radio of the receiving node when the
transmission starts. The radio holds the frame until the transmission ends,
and only then passes it up to its PHY, so that the interference of every
transmission that overlapped it is known when the reception is decided.

The receiving PHY first checks that the frame is meant for it. A UE accepts
frames from its serving base station only, and drops a frame that arrives from
its old serving cell during a handover. Control frames -- HARQ feedback,
scheduling grants, random access messages, and the UE's channel quality
reports -- are passed on without a reception decision: they always arrive.
A data frame is decided: the receiver asks the radio medium for the SINR of
each of the frame's resource blocks, asks its error model for the frame's
error rate, and makes one random draw against it. The PHY passes the
transport block to the MAC together with the outcome, and the MAC's HARQ
acknowledges it or asks for a retransmission (see :doc:`mac`).

The reception decision
----------------------

The default error model, :ned:`BlerCurveErrorModel`, works from block error
rate (BLER) curves: tables that give, for each CQI, the probability that a
block transmitted with that CQI's transmission format is received in error at
a given SINR. For a frame, it looks up the error probability of each allocated
resource block at the frame's CQI and at that block's SINR, and combines them:
the frame is received only if every block is, so its error rate is one minus
the product of the blocks' success probabilities. The lookup interpolates
between the tabulated points of the curve, at the block's SINR rounded to a
whole dB, as the SINR is rounded when the CQI is chosen, so the CQI choice and
the reception decision use the same point of the curve. Above a curve's last
tabulated point, the error rate stays at that point's value, the error floor
of the curve. A frame sent with CQI 0, or with a block whose SINR is below the
curves, is lost for certain.

.. TODO: the rounding and the error floor assume two pending fixes of the
   error model (tests/doc/model-deviations.md items 7 and 9: the lookup never
   reads the curves' last column and returns 0 above it; the reception decision
   truncates the SINR while the CQI choice rounds it). Check that both have
   landed before the guide is published.

A HARQ retransmission carries the same transport block again. A real receiver
combines the copies, so the retransmission is more likely to succeed; the
error model stands in for that by multiplying the error rate by
``harqReduction`` (0.2 by default) for every retransmission.

:ned:`ConfiguredRateErrorModel` replaces the computation by a configured error
rate per direction (``perDl``, ``perUl``, ``perD2D``), which the SINR, the CQI
and the allocation do not enter. It is meant for experiments that need a known
loss rate rather than one that emerges from the geometry and the MCS
selection: for example, RLC AM and radio link failure experiments, which want
a lossy downlink and a clean uplink for the status reports. With
``harqReduction`` set to 1, the HARQ attempts are independent, and a block that
exhausts its retransmissions is lost with exactly the configured rate raised
to the number of attempts. A direction's rate is read by the receivers of that
direction, so a downlink error rate takes effect at the UEs.

Channel quality feedback
------------------------

A UE reports the quality of its downlink to its serving base station, which
uses the reports for link adaptation and scheduling. The feedback generator of
each UE leg (:ned:`LteDlFeedbackGenerator`, ``dlFbGen`` and, on the NR leg,
``nrDlFbGen``) runs the reporting cycle: every ``fbPeriod`` (6 TTIs by
default) it takes a measurement, and ``fbDelay`` later (1 TTI) it sends the
report to the base station, on every carrier of the leg. The reports are
control frames: they always arrive, and they take no uplink resources.

The base station computes the CQI from the report. For the downlink, it
evaluates the UE's channel on the radio medium at the instant the report's
reception ends, and for the uplink, it takes the report's own reception. In
both, it chooses for each band the CQI whose error rate at the band's SINR is
closest to the target block error rate, the ``targetBler`` parameter of
:ned:`PhyEnb` (0.001 by default). The ``feedbackType`` of the feedback
generator decides how many values a report carries: a CQI for every band
(``ALLBANDS``, the default), for the best bands only (``PREFERRED``), or one
for the whole carrier (``WIDEBAND``). The reports go to the MAC of the base
station, whose adaptive modulation and coding (AMC) module keeps them and
chooses the transmission format of each transmission from them (see
:doc:`mac`).

The rank of every report is 1: all transmissions are single-layer.

Beacons and cell measurements
-----------------------------

A base station can broadcast beacons, frames that carry no data and are sent
every ``beaconInterval`` (a parameter of the base station's ``cellInfo``, 1
second by default). A UE leg with handover enabled measures every beacon of
its own technology on its primary carrier: the measurement is the beacon's
SINR, averaged over the carrier's resource blocks. It uses the measurements
for two purposes: to hand over when another cell becomes
stronger by a margin, and to detect that the serving cell is lost, when its
beacons fall below a minimum. A leg without handover ignores the beacons. The
decisions themselves are taken by the connection control of the UE leg and of
the base station (see :doc:`ue` and :doc:`handover`). The initial choice of the
strongest cell, with dynamic cell association, does not use beacons: the UE
computes the received power of every base station of its technology at
initialization.

Beacons are switched on with the ``enableBeacons`` parameter of
:ned:`PhyEnb`, which follows ``enableHandover`` by default: a base station that
takes part in handover must send beacons.

Transmit power and antennas
---------------------------

The transmit power of a leg is set at its PHY: ``eNodeBTxPower`` for a base
station (46 dBm by default), or ``microTxPower`` (30 dBm) for a base station
whose ``cellInfo`` marks it as a micro cell, and ``ueTxPower`` for a UE
(26 dBm). The power is fixed: there is no uplink power control.

A base station's antenna is sectorial when its ``txDirection`` is
``"ANISOTROPIC"``; ``txAngle`` is then its boresight. The radiation pattern is
the one of the channel model's propagation study (see :doc:`radio-medium`).

Modules
-------

:ned:`PhyEnb`, :ned:`PhyUe`
   The PHY of a base station and of a UE leg; their common base is
   :ned:`PhyBase`.

:ned:`CellularRadio`
   The radio of a leg, with :ned:`CellularAntenna`, :ned:`CellularTransmitter`
   and :ned:`CellularReceiver`.

:ned:`BlerCurveErrorModel`, :ned:`ConfiguredRateErrorModel`
   The error models, which implement :ned:`ICellularErrorModel`.

:ned:`LteDlFeedbackGenerator`
   The channel quality reporting of a UE leg.

Configuration
-------------

**Make the base stations sectorial.** Turn their antennas towards different
directions, as ``simulations/lte/multicell`` does:

.. code:: ini

   *.eNodeB*.cellularNic.radio.antenna.txDirection = "ANISOTROPIC"
   *.eNodeB1.cellularNic.radio.antenna.txAngle = 45deg
   *.eNodeB2.cellularNic.radio.antenna.txAngle = 135deg

**Change the transmit power.** The parameters are on every PHY, so a pattern
covers all legs:

.. code:: ini

   **.eNodeBTxPower = 40dBm
   **.ueTxPower = 26dBm

**Use a fixed error rate.** Replace the error model of every receiver, and
make the HARQ attempts independent, as ``simulations/lte/rlc`` does to drive
its RLC experiments at a known loss rate:

.. code:: ini

   **.receiver.errorModel.typename = "ConfiguredRateErrorModel"
   **.harqReduction = 1

**Report channel quality less often.** Set the period of the reports, as
``simulations/lte/d2d_multihop`` does:

.. code:: ini

   **.fbPeriod = 500tti

**Enable the beacons for handover.** Handover needs the beacons, which the base
stations send when handover is enabled at their PHY:

.. code:: ini

   **.enableHandover = true

Statistics
----------

The receiver of a UE records the SINR of the UE's links, in both directions:

``rcvdSinrDl``, ``rcvdSinrUl``
   The SINR at the reception of each data frame, in the downlink at the UE and
   in the uplink at the base station.

``measuredSinrDl``, ``measuredSinrUl``
   The SINR at the computation of each channel quality report.

A base station's receiver records none of them, so the figures stay per UE.
On a radio with more than one carrier, each is also recorded per carrier,
under its name followed by the carrier frequency (``measuredSinrDl-3500MHz``,
for example). The PHY of a UE records the CQI of the transmissions it receives
(``averageCqiDl``) and sends (``averageCqiUl``), and its distance from its
serving base station (``distance``). See :doc:`results`.

Example simulations
-------------------

*  ``simulations/lte/multicell`` and ``simulations/lte/tutorial``: sectorial
   base stations;

*  ``simulations/channelmodel``: the transmit power and the antenna, varied
   for the channel model studies;

*  ``simulations/lte/rlc`` and ``simulations/nr/rlc``: the configured error
   rate;

*  ``simulations/lte/test_handover`` and ``simulations/lte/cars``: beacons and
   handover.

Fidelity
--------

*  The PHY is modeled at the level of transport blocks and resource blocks:
   channel coding, modulation, the CRC and the OFDM symbols are not
   simulated, and the reception of a transport block is one random draw
   against its error rate.

*  HARQ soft combining is approximated by a fixed reduction of the error rate
   per retransmission.

*  Transmissions are single-layer: the rank is always 1, and MIMO and spatial
   multiplexing are not modeled.

*  Control frames -- HARQ feedback, scheduling grants, random access
   messages, channel quality reports -- are always received and take no radio
   resources.

*  The transmit power is fixed: there is no uplink power control.

*  Known deviation: the CQI is the one whose error rate at the SINR is
   closest to ``targetBler`` (0.001 by default). TS 38.214 5.2.2.1 defines it
   as the highest CQI at which a transport block would be received with an
   error probability not exceeding 0.1, for the default CQI tables.

*  Known deviation: the CQI has no measurement delay. The base station
   evaluates the downlink channel at the instant a report's reception ends,
   while TS 38.214 5.2.2.5 measures it on a reference resource several slots
   before the report, so where the channel changes within a few slots (moving
   UEs, fast fading) the CQI is more accurate than a real UE's.

*  Known deviation: the BLER curves are not monotonic at a few points (CQI 1,
   4 and 5), where the error rate rises with the SINR.

*  Known deviation: the handover decision compares the beacons' SINR,
   averaged over the resource blocks, where TS 38.215 measures the reference
   signal received power (RSRP). And that SINR counts the interference of
   background and external cells, but not that of the real cells.

*  Known deviation: a sectorial base station's antenna pattern applies only
   when it transmits; in the uplink, its own UEs and their interference
   arrive at full gain.
