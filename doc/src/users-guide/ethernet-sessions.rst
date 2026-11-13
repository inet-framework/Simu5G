Ethernet Sessions
=================

Overview
--------

An Ethernet PDU session (3GPP TS 23.501 5.6.10.2) carries Ethernet frames,
instead of IP packets, between a UE and a data network. It exists for
industrial networks, where machines, controllers and sensors communicate on an
Ethernet LAN, and it is the basis on which the 5G system joins a Time-Sensitive
Networking (TSN) network as a bridge (TS 23.501 5.28). The frames may come
from the UE itself, or from devices on a LAN behind the UE. The session is
anchored at a UPF, and its main functions are forwarding based on MAC
addresses, the handling of VLAN tags, the reporting of learned MAC addresses,
ARP and Neighbor Discovery proxying, and the 5G LAN-type service. For
forwarding (TS 23.501 5.8.2.5), the UPF learns the source MAC addresses of the
frames that arrive from the sessions and from the N6 interface, delivers each
frame where its destination address was learned, and floods the frames with
unknown, broadcast and multicast destinations. For VLANs, the UPF can add,
remove or check the S-tags and C-tags of the frames on N6, and can restrict a
session to allowed VLANs and MAC addresses. The UPF reports the MAC addresses it
learns to the SMF, and it can answer ARP and IPv6 Neighbor Discovery requests
on behalf of the UEs, so that these broadcasts do not reach the radio access
network. The 5G LAN-type service (TS 23.501 5.29) groups UEs into a 5G Virtual
Network whose members exchange frames as on one LAN. The frame check sequence
(FCS) is not carried over the 5G system: it is removed where a frame enters
the session and added again where it leaves. The QoS flow of a frame is
selected by its headers, for example by its 802.1Q priority (PCP), and the
radio access network can compress the Ethernet headers (EHC, TS 38.323).

Simu5G models the Ethernet session with a UE that is an Ethernet host or an
Ethernet bridge for the devices behind it, frames that cross the radio access
network and the GTP-U tunnel without their FCS, and a learning bridge at the
anchor UPF, whose ports are the UPF's Ethernet sessions and one N6 Ethernet
interface. VLAN tags travel with the frames, the bridge learns addresses per
VLAN, and the QoS rules can classify frames by any of their header fields. The
VLAN tag handling on N6, the allowed VLAN and MAC address lists, MAC address
reporting, ARP and Neighbor Discovery proxying, the 5G LAN-type service,
Ethernet header compression, and the integration of the 5G system into a TSN
network as a bridge are not modeled, and flooding is not scoped to a VLAN.

This chapter follows a frame through an Ethernet session, describes the UE's
two roles and the UPF's bridge, and shows how to configure them. The general
session model is described in :doc:`pdu-sessions`, and QoS flows and bearers
in :doc:`qos`.

The path of a frame
-------------------

Take a device on an Ethernet LAN behind a UE, sending a frame to a station on
the data network's Ethernet LAN, the LAN of the anchor UPF's N6 interface:

1. The frame reaches the UE on one of its Ethernet interfaces. The UE is an
   Ethernet bridge, and forwards the frame to its cellular NIC, which is one of
   the bridge's ports.

2. At the top of the NIC, :ned:`EthernetPortal` takes the frame into the
   stack. The UE's QoS rules classify the full frame onto a QoS flow, and the
   NIC's :ned:`Ip2Nic` removes the FCS before the frame enters SDAP and the
   data radio bearer of its QoS flow.

3. The base station receives the frame and puts it, unchanged, into the
   session's GTP-U tunnel towards the anchor UPF, on the QoS flow the UE's SDAP
   gave it.

4. At the anchor UPF, the GTP-U entity takes the frame out of the tunnel,
   adds an FCS again, and hands it to the UPF's Ethernet bridge,
   :ned:`EthernetSessionBridge`, on the port of the session.

5. The bridge learns the frame's source address on the session's port, and
   forwards the frame by its destination address: here to the N6 Ethernet
   interface ``dnEth``, which sends it onto the data network's LAN.

A frame from the station to the device takes the opposite path. The bridge
forwards it to the port of the session where the device's address was learned;
the UPF classifies the frame onto a QoS flow by the downlink rules, removes the
FCS and puts the frame into the session's tunnel; and the UE's NIC adds the FCS
again and delivers the frame to the UE's bridge, which forwards it to the
device.

When the destination of a frame is a device or UE of another Ethernet session
of the same UPF, the bridge switches the frame from one session's port to the
other directly, without going through N6.

The UE: an Ethernet host or bridge
----------------------------------

A UE whose ``sessionType`` is ``"Ethernet"`` is an Ethernet node: its link
layer is INET's layered Ethernet stack (:ned:`EthernetLayer` with
:ned:`Ieee8021qProtocol`), and its cellular NIC is an Ethernet port of that
link layer, with a MAC address of its own. 3GPP assigns no MAC address to a UE;
the NIC gets an automatically generated one, as any INET Ethernet port does.
The ``ethernetMode`` parameter selects one of two roles:

Host (``ethernetMode = "host"``, the default)
   The UE is an Ethernet host. Raw Ethernet applications on the UE, such as
   INET's :ned:`EthernetSourceApp` and :ned:`EthernetSinkApp`, send and
   receive frames with the NIC's MAC address.

Bridge (``ethernetMode = "bridge"``)
   The UE is an Ethernet bridge (INET's :ned:`BridgingLayer` with a MAC
   forwarding table) between its cellular NIC and its Ethernet interfaces
   ``eth[]``, whose ``ethg[]`` gates connect the devices behind it. The UE's
   Ethernet ports are promiscuous, so frames for the devices pass through.

Nothing configures IP over an Ethernet session: a UE with an Ethernet session
normally has no IP layer (``hasIpv4 = false``). Applications that need IP run
on the devices behind the UE, over their own Ethernet LAN and the session.

The UPF's Ethernet bridge
-------------------------

A UPF that anchors Ethernet sessions has an :ned:`EthernetSessionBridge`,
switched on with its ``hasEthernetBridge`` parameter, and an N6 Ethernet
interface, ``dnEth``, on its ``dnEthg`` gate. A UPF without the bridge stops
the simulation with an error when a UE requests an Ethernet session.

The bridge is INET's :ned:`BridgingLayer` over an interface table and a MAC
forwarding table of its own. Its ports are ``dnEth`` and one port per Ethernet
session: the port is created when the session is established at the UPF, and
deleted when the session is released, after the addresses learned on it are
flushed from the forwarding table. The bridge

*  learns the source address of every frame on the port it arrived on, per
   VLAN;

*  forwards a frame with a known unicast destination to the port where that
   address was learned, and drops it if that is the port it arrived on;

*  floods a frame with an unknown unicast, a broadcast or a multicast
   destination to every port except the one it arrived on.

The session ports and ``dnEth`` belong to the bridge, not to the UPF's network
layer: they have no IP addresses and no routes, and the UPF's IP traffic does
not reach them.

VLAN tags
---------

A frame crosses the session with its 802.1Q tag, if it has one, and the bridge
learns addresses separately per VLAN. The tags are set by the devices and
stations at the ends: in the example simulations, the applications tag their
frames with INET's :ned:`PacketTagger`. Neither the UE nor the UPF adds,
removes or checks tags, and the bridge floods a frame to every port regardless
of its VLAN.

QoS flows for frames
--------------------

The QoS rules of an Ethernet session classify frames, not IP packets: the
uplink rules (``ulQfiRules`` of the :ned:`BearerConfigurator`, evaluated at the
UE) and the downlink rules (``dlQfiRules``, evaluated at the anchor UPF) are
matched against the full frame, so they can select by the 802.1Q priority, the
VLAN, the EtherType or the MAC addresses. A downlink frame that matches no rule
goes on the default QoS flow, an uplink one on the UE's default bearer. A
``dscpAsQfi`` rule does not apply to frames, which have
no DSCP field; scoping one to a UE with an Ethernet session is an error. The
rule syntax, and the mapping of QoS flows to data radio bearers, are described
in :doc:`qos`.

Mobility
--------

An Ethernet session survives a handover like an IP session. The session keeps
its anchor UPF and its port in the UPF's bridge, so the addresses the bridge
learned stay valid; the source base station forwards the downlink it holds
when the UE leaves to the target base station over X2 (see
:doc:`handover`).

Configuration
-------------

**Enabling Ethernet sessions.** The network needs a 5G core whose anchor UPF
has the bridge, with its N6 Ethernet interface connected to the data network's
Ethernet LAN, and stacks with SDAP:

.. code:: ned

   upf.dnEthg <--> Eth1G <--> dnStation.ethg++;

.. code:: ini

   **.cellularNic.hasSdap = true
   *.upf.hasEthernetBridge = true
   *.upf.dnEth.bitrate = 1Gbps

   *.ue[*].sessionType = "Ethernet"
   *.ue[*].hasIpv4 = false

**A UE as an Ethernet host.** Raw Ethernet applications on the UE send and
receive on the cellular interface, and address their peers by node name, which
resolves to the MAC address of the peer's interface:

.. code:: ini

   *.ue[0].numApps = 2
   *.ue[0].app[0].typename = "EthernetSourceApp"
   *.ue[0].app[0].io.interface = "cellular"
   *.ue[0].app[0].io.remoteAddress = "dnStation"
   *.ue[0].app[1].typename = "EthernetSinkApp"
   *.ue[0].app[1].io.interface = "cellular"
   *.ue[0].app[1].io.localAddress = "ue[0]"
   *.ue[0].app[1].io.steal = true

The stations of the data network and the devices behind a UE are INET hosts
with the layered Ethernet stack, configured as in
``simulations/nr/ethernet/omnetpp.ini``.

**A UE as a bridge for devices.** The devices connect to the UE's ``ethg[]``
gates, and the UE gets one Ethernet interface per device:

.. code:: ned

   device[i].ethg++ <--> Eth1G <--> ue[0].ethg++;

.. code:: ini

   *.ue[0].ethernetMode = "bridge"
   *.ue[0].numEthInterfaces = 1

**Mapping an 802.1Q priority to a QoS flow.** To carry the frames of priority 5
on their own QoS flow and data radio bearer, in both directions:

.. code:: ini

   *.bearerConfigurator.staticDrbs = [
       {coreNetwork: "5gc", ue: "ue[0]", drbId: 1, isDefault: true, lcg: 3,
        rlcMode: "UM"},
       {coreNetwork: "5gc", ue: "ue[0]", drbId: 2, mappedQfis: [5], lcg: 1,
        rlcMode: "UM"}]
   *.bearerConfigurator.ulQfiRules = [{ue: "ue[0]",
       filter: "expr(has(ieee8021qctag) && ieee8021qctag.pcp == 5)", qfi: 5}]
   *.bearerConfigurator.dlQfiRules = [
       {filter: "expr(has(ieee8021qctag) && ieee8021qctag.pcp == 5)", qfi: 5}]

Statistics
----------

Ethernet sessions have no statistics of their own. The applications at the
ends of the session record what they send and receive, and the radio protocol
stack records its statistics per bearer, as for any session (see
:doc:`results`).

Example simulations
-------------------

``simulations/nr/ethernet`` runs Ethernet sessions in the
:ned:`EthernetSessions` network, a 5G standalone network whose UPF has an
Ethernet N6 link to a station, ``dnStation``, and an IP N6 link to a router and
a server:

``Ethernet-Hosts``
   Two UEs in host mode and ``dnStation`` exchange frames through the UPF's
   bridge: from a UE to the station over N6, from one UE to the other,
   switched at the UPF between the two sessions, and broadcasts from the
   station.

``Ethernet-Bridge``
   A UE in bridge mode with a device behind it. The device and ``dnStation``
   exchange VLAN 42 frames of priorities 5 and 0, and the QoS rules put
   priority 5 on its own QoS flow and data radio bearer.

``Ethernet-Handover``
   A UE in host mode moves from one gNB to another while it exchanges frames
   with ``dnStation``.

``Ethernet-Mixed``
   One UPF anchors an Ethernet, an IPv4 and an Unstructured session at the
   same time.

Fidelity
--------

*  The bridge at the UPF is a learning bridge that floods frames across
   VLANs: flooding is not scoped to the frame's VLAN.

*  The UPF neither adds, removes nor checks VLAN tags (the S-tag and C-tag
   handling of TS 23.501 5.8.2.5.3), and does not restrict a session to
   allowed VLANs or MAC addresses.

*  The UPF does not report learned MAC addresses to the SMF, and does not
   proxy ARP or IPv6 Neighbor Discovery: broadcasts reach every session.

*  The 5G LAN-type service, with its 5G VN groups, and the TSN integration of
   TS 23.501 5.28 are not modeled.

*  The anchor UPF of a session does not change (no PSA relocation), and a UPF
   has one N6 Ethernet interface.

*  The Ethernet headers are not compressed over the radio: Ethernet header
   compression (EHC) is not modeled. ROHC compresses IP headers only: a bearer
   definition that states ``rohc`` for an Ethernet session is an error, and the
   ROHC policy of the :ned:`BearerConfigurator` skips such bearers.

*  Ethernet sessions are not supported on a D2D-capable UE, and reflective QoS
   is not supported on them.

*  An Ethernet session needs a 5G core: requesting one at a UE whose anchor is
   a :ned:`PgwStandard` is an error.
