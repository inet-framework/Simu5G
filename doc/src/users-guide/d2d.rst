Device-to-device (D2D) communications
=====================================

Overview
--------

Device-to-device (D2D) communication lets nearby UEs exchange data directly,
over the sidelink (the PC5 interface), instead of through a base station. 3GPP
specifies it as Proximity Services (ProSe, TS 23.303) and as sidelink
communication in the radio access network (TS 36.300 for LTE, TS 38.300 for
NR). Its main functions are sidelink discovery, sidelink communication in
unicast, groupcast and broadcast, and the allocation of sidelink resources,
either scheduled by the base station or selected by the UEs themselves from
configured resource pools. For scheduled allocation, the base station keeps
control of the direct link: it assigns the radio resources of every sidelink
transmission. Sidelink communication also works out of network coverage,
where only UE-selected resources are possible.

Simu5G models network-assisted D2D, with a research prototype that is not based
on the 3GPP sidelink specifications: two UEs served by the same base station
exchange data over a direct link whose radio resources the base station
schedules, either one-to-one or one-to-many (multicast). The base station also
decides, statically or dynamically, whether a flow takes the direct path or the
conventional path through the base station. All of this happens below the IP
layer, in both the LTE and the NR stack, so applications are unaware of the path
in use. Sidelink discovery, UE-selected resources from resource pools, and
out-of-coverage operation are not modeled. D2D is a legacy feature of Simu5G: it
is kept working, but it gets no new features.

The modeling is described in A. Virdis, G. Nardini, G. Stea, "Modeling unicast
device-to-device communications with SimuLTE" (IWSLS2 2016) and in G. Nardini,
A. Virdis, G. Stea, "Modeling network-controlled device-to-device
communications in SimuLTE" (MDPI Sensors, 2018). Ready-to-run examples are
provided under ``simulations/lte/d2d``, ``simulations/lte/d2d_multicast``,
``simulations/lte/d2d_multihop`` and ``simulations/nr/d2d``.

Enabling D2D
------------

D2D is turned on per node with a single switch. Setting ``**.hasD2D = true`` on
a UE and its serving base station selects the D2D-capable ``cellularNic``
variant on those nodes (an explicit ``cellularNic.typename`` still takes
precedence). The behavior of the direct links is then configured with a few
knobs:

- ``**.amcMode = "D2D"`` activates the D2D AMC pilot (the default ``"AUTO"`` is
  cellular-only).
- ``*.ue*.cellularNic.d2dInitialMode = true`` starts D2D-capable flows in
  direct mode rather than infrastructure mode.
- The CQI used for the direct links is either reported per link
  (``cellularNic.phy.enableD2DCqiReporting = true`` on the base station) or
  fixed (``**.usePreconfiguredTxParams = true`` together with ``**.d2dCqi``).
  One-to-many links always use a fixed CQI.
- Dynamic mode switching is driven from the RRC module of the D2D-capable base
  station NIC, which enables the periodic mode-selection tick
  (``rrc.hasD2DModeSelection = true``); the policy is selected with
  ``cellularNic.rrc.d2dModeSelection.typename`` (for example
  ``"D2DModeSelectionBestCqi"``, which keeps each flow on whichever mode
  currently has the better CQI).
- Frequency reuse among mutually non-interfering D2D pairs is available through
  the best-fit uplink allocator (``cellularNic.mac.schedulingDisciplineUl =
  "ALLOCATOR_BESTFIT"``, ``cellularNic.mac.reuseD2D = true`` and the associated
  conflict-graph parameters).

Module organization and extension points
----------------------------------------

All D2D-specific modules live under the ``simu5g.stack.d2d`` package, kept
separate from the core LTE/NR stack: the D2D-capable NICs (``LteNicUeD2D``,
``LteNicEnbD2D``, ``NrNicUeD2D``, ``NrNicEnbD2D``) and, in sub-packages, the
D2D variants of the MAC, PHY, RLC, IP-to-NIC and RRC layers, the D2D AMC,
and the mode-selection modules. D2D links themselves are evaluated by the
radio medium's channel models, like every other link. The core stack reaches this
code only through a small set of interfaces, which are the natural extension
points for custom D2D behavior: the C++ interfaces ``ID2dMacUe``,
``ID2dMacEnb`` and ``ID2dAmc``, and the NED module
interface ``ID2DModeSelection``. To add a new mode-selection policy, subclass
``D2dModeSelectionBase`` (which implements the periodic evaluation and the
switch-notification machinery) and select it via
``cellularNic.rrc.d2dModeSelection.typename``.
