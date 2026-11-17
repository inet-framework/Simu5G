# Simu5G channel and PHY model: recorded deviations

Deviations of the channel and PHY model from the 3GPP specifications, and defects
of the model, found during the acceptance of the radio medium branch
(radio-medium-acceptance.md). Checked against ETSI TR 138 901 V16.1.0, TR 36.814
V9.2.0, TR 36.873 V12.7.0, TR 36.843 V12.0.0 and TR 37.885 V15.3.0.

They move fingerprints, so they were recorded rather than fixed during the
acceptance, and are addressed after the whole branch has been merged. Until then
the unit tests record a deviation, where one pins it, as a passing check labelled
"PINNED DEFECT:" and a separate test with %expected-failure that asserts the
specification.

"Later in the branch" names the commits of the radio medium branch that touch the
same mechanism, so that nothing is fixed twice.

## Deviations from the specification

1. **The shadowing correlation over distance is wrong.**
   - What the model does: StochasticChannelModel::computeShadowing() updates a moving UE's shadowing as `a*old + sqrt(1-a^2)*N(0,sigma)` with `a = exp(-0.5*d/dcorr)`. It only updates once the UE has moved more than dcorr, and holds the value until then.
   - What the spec says: TR 38.901 7.4.4, eq. (7.4-5): R(dx) = exp(-|dx|/d_cor), with d_cor from Table 7.5-6, "correlation distance in the horizontal plane". TR 36.873 7.2.4 and TR 36.814 B.1.2.1.1 say the same; all three cite ITU-R P.1816 for it. No clause has a 0.5 factor or a hold rule.
   - Effect: the 0.5 doubles the correlation length, and the hold makes R = 1 below d_cor and the process stepwise.
   - Fix shape: `a = exp(-d/dcorr)`. Then decide between updating at every evaluation with that a, or interpolating ("may be interpolated within the coherence length", 7.6.3.2).
   - Later in the branch: "a link's shadowing is redrawn by how far its ends have moved, not by time and speed" keeps the factor "as before", and "a link's shadowing is kept once per link, shared by every channel model evaluating it" makes the shadowing per link. Neither fixes this.
   - Pinned by tests/unit/ChannelModelShadowing.
   - The distance itself was wrong too, and that part is FIXED on topic/corr-distance (2026-11-17): d_cor was the correlationDistance parameter, 50 m by default for every scenario and propagation condition, and the same 50 m decided when a link's LOS state is drawn again. From "the shadowing and the LOS state of every scenario were redrawn at one distance, 50 m" on, the shadowing takes the SF correlation distance of the selected study's table for the link's scenario and condition (TR 36.814 Table B.1.2.2.1-4, TR 36.873 Tables 7.3-6, 7.3-6a and 7.3-6b, TR 38.901 Table 7.5-6; SMa from ITU-R M.2135-1 Table A1-7), and the LOS state takes TR 38.901 Table 7.6.3.1-2. correlationDistance, when set, replaces both. Tests: unit/PathLossCorrelationDistance, unit/ChannelModelCorrelationDistance.
   - What remains of the distance: TR 36.814 and TR 36.873 give no correlation distance for the LOS state, so the model takes TR 38.901's under them too (SMa: 50 m, from TR 38.901 v19.5.0), which is a convention of the model. The LOS state is drawn again independently at each step; TR 38.901 7.6.3.3 correlates it over the distance. The O2I column applies to a UE inside a building only where the study's table has one. The factor and the hold rule above are unchanged.
2. **The thermal noise does not depend on the bandwidth, and the transmit power is not split over the RBs.**
   - What the model does: thermalNoise is a fixed parameter, -104.5 dBm by NED default, which is kTB for 50 RBs at 15 kHz (9 MHz). Every band's SINR gets the node's full transmit power.
   - Effect: with numBands = 6 the physical floor is -113.7 dBm (15 kHz SCS) or -110.7 dBm (30 kHz), so the SINR is 9.2 or 6.2 dB too low. And the uplink SINR does not depend on how many RBs a UE is given.
   - Fix shape: derive N from numBands, the numerology and the RB width; take a decision on the per-RB power (PSD) convention.
   - The physics oracle copies the convention, so it cannot see it.
3. **Rayleigh fading is not fading.**
   - What the model does: rayleighFading() is a lookup in the one PhyPisaData table shared by the whole simulation, indexed by band only. Every node on a band gets the same value, the value never changes over time, and no random number is drawn.
   - Used by: channelmodel Rayleigh-38901, and any config with fadingType = RAYLEIGH.
   - Pinned by tests/unit/ChannelModelJakesFading, as "tripwires".
4. **The UE-to-UE (D2D) path loss uses base-station conventions.**
   - What the model does, as the D2dLinkBudget oracle reproduces it: the 38.901 UMi breakpoint is computed with the gNB height (hBS = 10 m, so d'BP = 120 m), and cableLoss (2 dB) is subtracted on a link with no base station.
   - What the specs say: TR 36.843 A.2.1.2 gives h_BS = h_MS = 1.5 m (effective 0.8 m), and TR 37.885 6.2.1 reuses the V2V path loss of Table 6.2.1-1 for P2P ("Pathloss equation of V2V is reused for that of V2P, P2P, V2R, R2R."; urban LOS: 38.77 + 16.7 log10 d + 18.2 log10 fc).
   - At 50 m and 2 GHz: the model gives 74.10 dB, TR 37.885 72.62 dB, and 38.901 UMi with both heights at 1.5 m 90.73 dB.
   - D2D is legacy (keep alive, no features). Decide whether this is worth fixing at all.
   - Since "the path loss took a link's heights from parameters" (item 32), the rule is explicit: a UE-to-UE link takes the base station height the studies give for the scenario (10 m UMi, 25 m UMa, 35 m SMa and RMa, 3 m InH) and the lower of the two UEs' antenna heights, whichever UE transmits; the scenario is that of the receiving UE's serving cell (the receiver is there when the link is evaluated, the transmitter may have left its cell). With the default heights this is what the parameters gave before. Taking the two UEs' own heights was tried and dropped: at hBS = 1.5 m the TR 36.814 UMa NLOS formula gives 236 dB at 100 m, because its (h/hBS)^2 term is outside the formula's range (10 m < hBS < 150 m). The rule is stated in StochasticChannelModel.ned and tested in tests/unit/ChannelModelLinkHeights.
5. **The scenario geometry is outside the formulas' validity.** The physics tests ran URBAN_MICROCELL with a base station at 25 m. TR 38.901 Table 7.4.1-1 UMi-SC gives "hBS=10m", and TR 36.814 Table B.1.2.1-1 UMi gives "hBS = 10 m".
   - Closed for the model by "the path loss took a link's heights from parameters": a link whose antenna heights are outside what the study gives for the scenario is an error, unless the channel model's tolerateHeightViolation is set. The check takes the "Applicability range, antenna height default values" column: the applicability ranges where the table states them, otherwise the heights it gives. It is strict where the table gives one value: TR 38.901 UMa is hBS = 25 m only.
   - The physics tests and one module test now have the microcell base station at 10 m. Six unit harnesses (ChannelModelPhantomGeometry, -PhantomLineOfSight, -PhantomLinkBudget, -PhantomShadowing, -SectorPattern, -UplinkSectorPattern) keep 25 m and set tolerateHeightViolation, because their expected values are written for that geometry: move them to 10 m when they are next touched.
   - The indoor hotspot examples (simulations/channelmodel InH-*, nr/d2d ChannelModel-IndoorD2D) had the base station at 25 m; its antenna is now at 3 m ("hBS = 3-6 m").
6. **The external- and background-cell interference was not evaluated with the model's own path loss.**
   - What the model did: computeExtCellPathLoss always applied TR 36.814 with the 3D distance as d, NLOS by configuration, and antennGainEnB.
   - Later in the branch: "external and background cells' interference takes the model's own path loss" replaces this. Re-check after the merge whether anything remains, e.g. shadowing and LOS for these links (stage B gives phantom links their own LOS state in "a phantom transmission's link has its own LOS state").
   - From piece 5a on, these links take the evaluating model's own study. What remains at the end of piece 5a: no shadowing on these links, and the formulas take the nodebHeight and ueHeight parameters while the distances come from the real positions.
   - Closed: these links are shadowed since "background UEs' and phantom cells' links had no shadowing", and take their own antennas' heights and their own cell's scenario since item 32 was fixed.

## Data and arithmetic defects

7. **The BLER curve ends in a cliff.** GetBLER_TU returns 0 at and above each curve's last tabulated point, and never reads BLER_15_CQI_TU[row][15]. For CQI 11-15 that value is an error floor (e.g. 0.02), so the lookup goes from 0.06 at 20 dB to 0 at 21 dB for CQI 11. Pinned by tests/unit/BlerCqiRoundTrip as the "current contract".
8. **The BLER curves are not monotonic.** BLER_15_CQI_TU rises with SINR at CQI 1 (-1.5 to -0.5 dB), CQI 4 (4 to 5 dB) and CQI 5 (-3 to -2 dB). Recorded in tests/unit/BlerCurveMonotonicity as an expected failure, which counts 2 offenders at 1 dB sampling. Fixing it is a modelling decision about the curves.
9. **The CQI and the error model read the SINR differently.** getCqi rounds the SINR (floor(snr+0.5)), while isReceptionSuccessful truncates it toward zero (int). So they disagree by up to 1 dB about which curve point applies, and -14.9 dB is judged as -14 dB, inside the curves. Pinned by BlerCqiRoundTrip and ChannelModelReception.
11. **The SINR statistic leaves out the worst receptions.** A reception lost to CQI 0, or to a band below the curves, returns before rcvdSinr is emitted, so the statistic and the recorded results never see those receptions.
   - FIXED by "StochasticChannelModel: fix: the received-SINR statistics include the receptions lost without a draw": such a reception records its SINR too. Test: "StochasticChannelModel: tests: test: a reception lost without a draw records its SINR too".

12. **Part of the fading table is never filled (latent).** PhyPisaData's constructor sizes channel_ to 10000 entries and fills only the first 1000. getChannel(i) wraps i modulo 10000, so any index from 1000 to 9999 returns 0, which is -inf dB. The caller, StochasticChannelModel::rayleighFading, indexes by band only, so this is reached only with 1000 or more bands. Found while mutation-checking the Rayleigh test.

## Found in piece 4 (2026-11-06): fix after the full merge

Recorded, not fixed in the branch; addressed after the whole radio branch is
merged, together with the items above. Each fix moves
fingerprints. In the branch they get test hardening only (pins and expected failures).

13. **Withdrawn in piece 7b: a D2D multicast reception does not key its interferers on the wrong link.** The item read a D2D_MULTI flow's destination id as the transmitter's own at the SINR evaluation, but D2dUePhy::frameAccepted() sets a group member's to its own first (lte/d2d_multicast checkMulticastRange, 3 s: 7401 receptions with the receiver's id, none with the transmitter's). The number is kept so that the later items keep theirs.
14. **A UE with two radios has two indoor positions.**
   - What the model does: each CellularRadio draws its own distance from the wall (uniform 0-25 m) and, from "the random term of a UE's building penetration loss is drawn once, the UE's own" on, its own penetration-loss deviate. An NR UE always has two radios (NrNicUe extends the LTE NIC: `radio` and `nrRadio`, the LTE one unused in standalone), and `**.insideBuilding = true` marks both, so every indoor NR UE draws twice; a dual-connectivity UE uses both.
   - What the spec says: TR 38.901 7.4.3.1 generates d2D-in and the O2I loss's random term per UT ("UT-specifically"), the same for all of the UT's links.
   - Effect: the indoor loss is 0.5 dB/m, so the two legs of one UE can differ by up to 12.5 dB for no physical reason; biases anything comparing a DC UE's LTE and NR legs indoors. On dev it was worse: one draw per channel model, i.e. per radio and per carrier.
   - Moves the four channelmodel Penetration rows (one draw fewer per UE on RNG 0).
   - Recorded in tests/module/IndoorTwoRadios (pin) and IndoorOnePosition (expected failure).
   - Fix shape: one draw per UE, shared by its radios.
   - Not changed by the fix of item 27: the LOS state and the shadowing are the UE's since then, but the indoor distance and the penetration-loss deviate are still each radio's.
15. **A D2D link's indoor loss depends on its direction.**
   - What the model does: the penetration loss is the "UE end's", `link.dir == UL ? txRadio : rxRadio` (StochasticChannelModel::getAttenuation), so a D2D link takes the receiver's indoor state: A indoors, B outdoors gives no wall loss on A->B and the full wall loss on B->A. Two indoor UEs get the full outdoor-to-indoor loss.
   - What the spec says: a link's path loss is reciprocal; TR 36.843 A.2.1.2 uses the indoor model between two indoor UEs.
   - TR 36.843 A.2.1.2's NLOS outdoor-to-indoor formula also has a term - 0.8*h_MS, which the model does not.
   - Recorded in tests/unit/ChannelModelD2dIndoorDirection (pin) and ChannelModelD2dIndoorReciprocal (expected failure).
   - Fix shape: one indoor end means one wall; both indoor needs an indoor-to-indoor model. D2D is legacy: decide whether it is worth more than the reciprocal rule.
16. **The medium's per-link maps are never erased.**
   - What the model does: the LOS, shadowing and Jakes maps keep one entry per (UE, cell) pair and carrier, per D2D pair, and from piece 7a on per (phantom transmitter, receiver) pair, for the whole run (CellularRadioMedium.h: "Entries are never removed: node ids are not reused"). On dev the base-station side grew the same way; the UE-side state died with the UE's channel model.
   - Effect: memory grows with the number of UEs ever created (cars, Veins); Jakes entries are bands x paths x 16 B. A user-assigned macNodeId reused after a departure would inherit stale state.
   - Fix shape: erase a radio's links when it unregisters from the medium (fingerprint-inert: no row removes UEs).
17. **The indoor distance is one uniform draw.** TR 38.901 7.4.3.1 takes d2D-in as the minimum of two independent uniforms (0-25 m for UMa/UMi, 0-10 m for RMa); the model draws one uniform(0, 25 m) for every scenario. Pre-existing.

## Found in piece 5a (2026-11-06): fix after the full merge

Recorded, not fixed in the branch, like the items of piece 4; the tests of
piece 5a pin item 19.

## Found in piece 5b (2026-11-07): fix after the full merge

Recorded, not fixed in the branch, like the items of pieces 4 and 5a.

22. **A car halted at a red light keeps its Doppler shift.**
   - What the model does: TrafficLightMobility leaves the car's velocity unchanged while it waits at a red light, and from "the Doppler speed is the mobile end's, from its mobility" on, the Doppler speed is the mobility's velocity. A waiting car fades as if it were driving at its cruising speed. The old two-sample estimate gave 0 there.
   - Reached in nr/videostreaming_dataset_generator (UrbanNetwork).
   - Fix shape: the velocity is zero while the car waits.
23. **A car's trajectory depends on how often its position is read.**
   - What the model does: at each update, TrafficLightMobility either stops the car for the whole time since the last update (light red) or moves it for the whole of it (light green). From "positions are the mobility's, at the instant they are read" on, every read of the position is an update, and positions are read at arbitrary instants (frames sent and received, CQIs, other nodes' evaluations), so the time a car waits, and where it is, depend on the reads.
   - Reached in the same row.
   - Fix shape: compute when the car reaches the stop line and when the light changes, independently of the update instants.
24. **The Jakes phase is the speed times the absolute time, and only one end's speed counts.**
   - What the model does: StochasticChannelModel::jakesFading() sums the paths with phase 2*pi*(cos(aoa)*v*f/c*t - tau*f), with v the speed at the evaluation and t the absolute simulation time. The phase that has accumulated up to t is not the integral of the Doppler frequency, so every change of speed (waypoints, TraCI steps, GaussMarkovMobility) jumps the phase by an amount growing with t, which re-randomizes the fading, and a UE that stops returns to the same static value whatever its path. The speed is that of the link's mobile end, the UE, or the transmitter of a D2D link: a moving base station's and a D2D receiver's are ignored, and the two directions of a D2D pair share one realization at two different speeds. On dev the phase was the same; the speed was an estimate.
   - What the physics says: a path's phase advances by the integral of its Doppler frequency over time, and the Doppler frequency has a term for each moving end.
   - The speed is read when the link is evaluated, while from "a link's Jakes fading is that at the start of the transmission evaluated on it" on the fading is taken at the transmission's start: when the speed changes in between (a reception decoded at its slot end after a waypoint, a TraCI step), a reception's fading depends on when it is evaluated.
   - Fix shape: keep the accumulated phase per link and path, advanced by the Doppler frequency over the time since the last evaluation, up to the transmission's start; take the velocities of both ends.
25. **The distance statistic is weighted by the reads of the position.**
   - What the model does: a UE's PHY emits its distance (and serving cell) statistics on the mobility's mobilityStateChanged signal. From "positions are the mobility's, at the instant they are read" on, the mobility emits that signal whenever anyone reads the position, so the mean of the distance is an average over the reads, not over time, and an unrelated module that reads the position more often changes it.
   - Fix shape: record the statistic time-weighted (timeavg), or emit it on a timer.

## Found in piece 6a (2026-11-07): fix after the full merge

Recorded, not fixed in the branch, like the items of the earlier pieces.

26. **The UMa environment height's probability of 1 m: TR 36.873 leaves out the distance, TR 38.901 draws at 18 m.**
   - What the model does: Tr36873PathLossModel::drawEnvironmentHeight() takes C(d2D, hUT) as ((hUT-13)/10)^1.5, without the distance term g(d2D), so the probability of h_E = 1 m is the same at every distance: 0.52 at hUT = 22.5 m. The same file's UMa LOS probability has the distance term. Tr38901PathLossModel takes g(d2D) = 0 only for d2D < 18 m, so at exactly 18 m it draws, and about 5 links in 1000 get an h_E other than 1 m.
   - What the specification says: TR 36.873 Table 7.2-1 NOTE 4 draws h_E = 1 m with probability 1/(1+C(d2D, hUT)), with the C of Table 7.2-2, which has the distance term: 0.98 at 30 m, 0.63 at 100 m and 0.29 at 200 m for hUT = 22.5 m, and 1 at 18 m or less. TR 38.901 Table 7.4.1-1 NOTE 1 has g(d2D) = 0 for d2D <= 18 m.
   - Reached in channelmodel TallUe-36873 (the only row with UEs above 13 m under TR 36.873); the 18 m boundary in no row.
   - Recorded in tests/unit/Tr36873EnvironmentHeightDistance and Tr38901EnvironmentHeightBoundary as expected failures; PathLossEnvironmentHeight pins the TR 36.873 distribution.
   - Fix shape: the C of the LOS probability in the TR 36.873 draw; d2D <= 18 m in the TR 38.901 one.
27. **A link's LOS state and h_E are drawn per cell and per carrier, not per site and UE.**
   - What the model does: the medium keeps a link's LOS sample, and with it h_E, per (cell, UE) pair and per carrier (CellularRadioMedium::getLosMap() per carrier frequency). A UE served on two carriers of one cell has two independent LOS states and h_E values, and the cells of one site (sectors) draw independently.
   - What the specification says: TR 38.901 7.6.5, "Propagation conditions generated in Step 2 are the same for all the frequencies"; Table 7.6.3.4-1 makes the LOS/NLOS state site-specific, and NOTE 1 of Table 7.4.1-1 determines h_E "for every link between BS sites and UTs".
   - Reached in the rows that serve a UE on two carriers of one cell (e.g. nr/test_multiCarrier), and in any network with co-sited cells.
   - Fix shape: key the LOS sample by site and UE, shared by every carrier.
   - FIXED on topic/site-state (2026-11-16), together with the same fault of the shadowing (findings P1 to P3 of reports/simu5g-2026-11-09_propagation-modeling-errors.md). The LOS state with h_E and the shadowing sample are kept by the two ends of the channel, as the radios name them (IRadioEndpoint::getChannelEndId()): the radios of one UE name the UE ("the two radios of a UE were two UTs to the channel state"), there is one table for all carriers ("a link had a LOS state and a shadowing realization per carrier"), and the cells that give one site name (the `site` parameter of a base station's radio) name the site ("the sectors of a site could not share their LOS state and shadowing"). The Jakes paths stay per radio pair and per carrier, as 7.6.5 has the fast fading. Tests: unit/ChannelModelStateAcrossCarriers (which replaces ChannelModelPerCarrierState, the pin of the wrong reading), unit/ChannelModelSiteState, module/Site*, physics/ThreeSectorSite.
   - What remains:
     - The first channel model to evaluate a pair of ends draws their state, and a model of another study takes it: an eNB and a gNB of one site share one LOS state and one shadowing value, drawn with the LOS probability and the sigma of whichever evaluated first (TR 36.814 and TR 38.901 differ: 4 dB against 7.82 dB at UMi NLOS).
     - The cells of a site must stand within 1 m of each other horizontally and be in one scenario; a site is named explicitly, never inferred from equal positions, so co-located cells without a common name still draw independently.
     - An end whose radio has left the medium (the sender of a frame still in the air) gets a state of its own from then on.
     - The azimuth from a mast to a UE straight below or above it is 0 degrees (atan2(0, 0)): the UE is then on the boresight of a sector pointing along the x axis.
     - The per-link tables are still never erased (item 16).

## Found in piece 6b (2026-11-08): fix after the full merge

Recorded, not fixed in the branch, like the items of the earlier pieces.

28. **The CQI describes the channel of the slot it is first used for: it has no measurement delay.**
   - What the model does: the base station computes a UE's DL CQI, and a D2D pair's CQI, from the UE's report as a what-if on the medium at the instant the report's reception ends (from "a CQI's what-if is evaluated where the radios are now" on), so it takes the positions and the Jakes fading of the DL slot that starts then. Before that commit it took the report's start, one slot earlier. The UL CQI of the same report is the report's own reception, faded at its start, so the two halves of one report are a slot apart.
   - What the specification says: TS 38.214 V19.5.0 5.2.2.5: "In the time domain, the CSI reference resource for a CSI reporting in uplink slot n' [...] is defined by a single downlink slot n-nCSI_ref-Koffset[...]", "where for periodic and semi-persistent CSI reporting - if a single CSI-RS/SSB resource is configured for channel measurement nCSI_ref is the smallest value greater than or equal to 4*2^muDL, such that it corresponds to a valid downlink slot". TS 36.213 V19.5.0 7.2.3 defines the LTE CSI reference resource the same way, as "a single downlink subframe/slot/subslot [...] n-nCQI_ref".
   - Effect: where the channel changes within a few slots (moving UEs, Jakes fading), the CQI is more accurate than a real UE's, and link adaptation fares better than it would.
   - Reached in every row with moving UEs and CQI reporting.
   - Fix shape: evaluate the CQI at its CSI reference resource -- the positions and the fading nCSI_ref slots before the report -- for the DL, UL and D2D CQI alike.
29. **A UE inside a building has no penetration loss under TR 36.814, nor under TR 36.873 except in UMa.**
   - What the model does: only Tr36873PathLossModel's UMa and Tr38901PathLossModel's UMa, UMi and RMa add a building penetration loss. Under every other study and scenario, a link whose UE end has `insideBuilding = true` is rejected when it is first evaluated; where the study defines an outdoor-to-indoor loss (TR 36.873 UMi, TR 36.814 UMi), the error says that it is not implemented.
   - What the specification says: TR 36.873 7.2.3: "3D-UMi O2I PL = PLb + PLtw + PLin", "PLb = PL3D-UMi (d3D-out + d3D-in)", "PLtw = 20", "PLin = 0.5d2D-in", "0m < d2D-in < 25m". TR 36.814 Table B.1.2.1-1, UMi outdoor-to-indoor: "PLtw = 20, other values remain the same", "0 m < din< 25 m". The other scenarios of the two studies have no outdoor-to-indoor model.
   - Reached in no row: the Penetration rows use TR 38.901 UMa and TR 36.873 UMa.
   - Recorded in tests/module/IndoorRejected_NoPenetrationLossInStudy and IndoorRejected_PenetrationLossNotImplemented.
   - Fix shape: the O2I loss in TR 36.873 UMi and TR 36.814 UMi.
30. **A moving base station interferes from where it is when the interference is evaluated, not from where its transmission started.**
   - What the model does: the DL interference walk evaluates another cell's link through StochasticChannelModel::cellularLink(), which reads the interfering cell's current position; the angular attenuation of the same walk and the uplink interference take the transmission's start position.
   - What the medium's model says: from "every radio's position is read as a transmission starts" on, a transmission's arrival is computed from where it started, as on INET's medium.
   - Reached in no row: no base station moves.
   - Fix shape: build the interfering cell's link from the transmission's start position.

## Found in piece 7a (2026-11-08): fix after the full merge

Recorded, not fixed in the branch, like the items of the earlier pieces.

## Found in piece 7b (2026-11-09): fix after the full merge

32. **A link's heights and scenario are one value per carrier and RAT, and the heights do not follow the positions.**
   - What the model does: scenario, nodebHeight and ueHeight are parameters of the channel model, and from "the radio medium owns the channel models, one per carrier and RAT" on there is one model per carrier and RAT, shared by every cell and UE on it. The formulas take the heights from these parameters; the distances come from the mobility positions, and nothing ties the two together. Most examples constrain z to 0, so d3D equals d2D although the breakpoint is computed with 25 m and 1.5 m. On dev each NIC had its own models, which allowed values per node but not per link: DL took the UE's model, UL and the DL CQI the base station's.
   - What the specification says: TR 38.901 7.5 Step 3: "Calculate pathloss with formulas in Table 7.4.1-1 for each BS-UT link to be modelled", whose h_BS and h_UT are that link's. TR 36.873 7.2.3 has a UT's height by its floor: "hUT=3(nfl - 1) + 1.5". A macro cell and a small cell are two scenarios.
   - Effect: a macro and a small cell on one carrier, UEs at different heights, and a background cell of its own kind cannot be expressed. A network where all cells and all UEs have the same height gives the same results as before.
   - Reached in every row as a mismatch of positions and heights; as a lost capability, in no row, which is why no suite showed it. Item 6's residue and item 4 are parts of this.
   - Fix shape: both heights from the link's positions; the scenario a property of the cell or site. The examples then need real heights, which moves results widely.
   - FIXED on topic/link-heights (2026-11-16). An antenna has a position ("an antenna has a position: its height above the node": antenna.mobility.offsetZ, 25 m at a base station and 1.5 m at a UE by default); the formulas take the heights of the link's two antennas ("the path loss took a link's heights from parameters"); the scenario, building height and street width are the base station radio's ("a link is evaluated in the deployment of its base station"). The channel model's five old parameters stay declared for one release and throw when set. Tests: unit/ChannelModelLinkHeights, unit/ChannelModelCellDeployment, unit/AntennaPosition, module/AntennaHeight*, module/CellDeploymentRejected_*.
   - What remains: the study (pathLossType) is still one per carrier and RAT. The h_E of a link is drawn with the UE height of that moment and kept (a UE that changes its height keeps its h_E). The TR 38.901 RMa formulas took h = 5 m and W = 20 m as constants; fixed by "the TR 38.901 rural formulas ignored the building height and street width", which also makes buildingHeight default to the studies' value for the scenario. The applicability ranges of h and W (5 to 50 m) are not checked. Flat ground at z = 0 is assumed: there is no terrain.

## Found 2026-11-12: fix after the full merge

## Fixed later in the branch, re-check only

- **The O2I loss's random term redrawn at every evaluation:** accepted with piece 6a. Drawn once per UE from "the random term of a UE's building penetration loss is drawn once, the UE's own"; see item 14 for the per-radio residue.

- **Shadowing per direction (LinkReciprocity):** made per link by "a link's shadowing is kept once per link, shared by every channel model evaluating it".
- **Jakes fading evaluated at now - 1 ms:** accepted with piece 6a, fixed by "a link's Jakes fading is that at the start of the transmission evaluated on it"; see item 24 for the speed read at the evaluation. The background cells' channel model keeps now - 1 ms until it is deleted later in the branch.
- **The Doppler speed from a two-sample estimate:** accepted with piece 5b. The estimate becomes the real speed with "positions are the mobility's, at the instant they are read", and the speed is the mobility's from "the Doppler speed is the mobile end's, from its mobility"; see items 22 and 24 for what remains.

## Not a deviation

- **ModuleOrderPermutation** asserts per-realisation invariance under relabelling of UEs. That is a common-random-numbers (RNG design) property, not physics. Keep it as a reproducibility goal, not as a model defect.
