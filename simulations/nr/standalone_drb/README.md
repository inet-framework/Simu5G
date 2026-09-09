# NR Standalone with QoS Flows, SDAP and Multiple DRBs

This example demonstrates 5G QoS-flow handling in Simu5G: the SDAP layer,
QFI-to-DRB mapping with multiple Data Radio Bearers per UE, and QoS-aware
MAC scheduling driven by per-DRB QoS parameters.

The network is the standard `SingleCell_Standalone` (one gNB, two UEs, a
remote server behind a UPF); everything specific to this example is plain
ini configuration.

## What is configured

- **SDAP layer** is enabled on the gNB and UE NICs (`hasSdap = true`).
  With SDAP enabled, the static `staticDrbs` parameter is the sole authority
  for DRB assignment (Ip2Nic's dynamic per-connection DRB creation is
  bypassed).

- **Two DRBs per UE**, defined in `smf.staticDrbs`: QFIs 1,2 (Voice/Video)
  map to DRB 0, QFIs 3,4 (Gaming/URLLC) map to DRB 1, both in RLC UM mode.
  The configuration is written once for the whole network and names its UE by
  module path (`ue: "ue[*]"`), so one entry describes a bearer of every UE
  and neither end of the radio link is configured directly: the SMF, the core
  network's session management, tells the UE's RRC and the gNB's RRC what to
  set up, and each RRC pushes on what its own layers consume. The two bearer kinds are named once as profiles
  (`smf.drbProfiles`) saying what the bearer *is* (RLC mode, QoS); the
  entries add what *selects* it (its UE and its QFIs). DRB IDs are per-UE, as
  in 3GPP: each UE has a DRB 0 and a DRB 1. The first DRB of each UE acts as
  its *default DRB*, which carries traffic whose QFI has no explicit mapping.

- **QoS-aware scheduling**: `schedulingDiscipline = "QOS_PF"` with the
  per-DRB QoS profile (GBR flag, packet delay budget, packet error rate,
  priority) authored on the same `staticDrbs` entries and pushed by RRC
  into the gNB MAC for the QoS-aware proportional-fair scheduler. DRB 1
  is configured as the more demanding bearer (50 ms budget, 10^-3 PER,
  priority 1).

## How packets get their QFI

- **Downlink**: the sender application marks packets with a DSCP value
  (`dscp` parameter of `VoipSender`). At the core-network ingress the
  `TrafficFlowFilter` reads the DSCP as the QFI (simplified PDR matching),
  the QFI travels in the GTP-U header to the gNB, and the gNB's SDAP maps
  it to the UE's DRB. Unmarked traffic (DSCP 0) becomes QFI 0 and falls
  back to the default DRB.

- **Uplink**: the UE application also marks packets with DSCP, and the UE's
  QoS-flow classifier (fed by the `ulQfiRules` parameter of the
  `bearerConfigurator`, the uplink mirror of the downlink rules) reads the
  DSCP as the QFI where the traffic enters the stack. A packet no rule covers is left unclassified and falls
  back to reflective QoS or the default DRB; an explicit QFI set by the
  application always wins.

## Configurations

- `Standalone`: Topology and DRB/QoS setup only, no traffic. Base for the others. |
- `VoIP-DL`: 4 VoIP flows to each UE, downlink. No DSCP marking, so all traffic is QFI 0 and rides the default DRB — demonstrates the default-flow fallback. |
- `VoIP-DL-MultiQfi`: Same as `VoIP-DL`, but apps 0..3 mark DSCP 1..4, so the four flows use QFI 1..4 and spread across both DRBs of each UE. |
- `VoIP-DL-MultiQfi-NoOnDemand`: Same as `VoIP-DL-MultiQfi`, but on-demand bearer establishment is disabled (`sdap.establishBearersOnDemand = false`): the static DRB configuration, established up front at initialization as always, must cover every flow, and a packet that finds no bearer raises a configuration error. |
- `VoIP-DL-MultiQfi-OnDemandDrb`: Same as `VoIP-DL-MultiQfi`, but the Gaming/URLLC DRB is not configured up front: QFIs 3 and 4 are served by an entry of the SMF's `onDemandDrbs` parameter, so that DRB is created by the first Gaming or URLLC packet (SDAP misses the QFI-to-DRB lookup and asks the SMF) and the other flow joins it. |
- `VoIP-DL-MultiQfi-Heavy`: Same, with heavy traffic (1000 B every 1 ms, no silence periods) to create contention and exercise the QOS_PF scheduler. |
- `VoIP-DL-NoDrbConfig`: Same VoIP-DL traffic with no bearers authored at all (`staticDrbs` emptied, `onDemandDrbs` left at its default). The default `onDemandDrbs` carries a `5gc` catch-all default bearer, so the first packet materializes one default DRB per UE and every flow rides it — the SDAP-stack counterpart of running an SDAP-less stack unauthored. |
- `VoIP-UL`: 4 VoIP flows from each UE to the server, uplink, with DSCP-derived QFIs 1..4. |
- `VoIP-UL-FilterRules`: Same as `VoIP-UL`, but the traffic is unmarked and the classifier assigns QFIs by destination port (`{filter, qfi}` rules) -- classification without the applications' cooperation. |

## What to look at

- SDAP behavior in the event log (Qtenv): `SDAP TX: QFI = ... extracted from
  QfiReq`, the selected DRB, and whether an SDAP header is added (it is only
  added when the QFI cannot be recovered from the DRB alone, i.e. on the
  default DRB or on DRBs carrying multiple QFIs).
- Per-application VoIP quality statistics (`voipMos`, `voipFrameDelay`,
  `voipFrameLoss`, `voipReceivedThroughput`), and how they differ per DRB
  under load in `VoIP-DL-MultiQfi-Heavy`.
