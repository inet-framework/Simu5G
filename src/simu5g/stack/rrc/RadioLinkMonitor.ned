//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.rrc;

//
// UE-side radio link monitoring (RLM) and radio link failure (RLF) detection,
// an abstraction of TS 38.331 5.3.10 built on the in-sync/out-of-sync model.
//
// The serving cell's periodic beacons serve as the stand-in for the RLM
// reference signals: ~HandoverController measures each serving-cell beacon and
// feeds the RSSI to this module. RSSI below qOut counts as an out-of-sync
// indication, at/above qIn as in-sync. n310 consecutive out-of-sync indications
// start T310; n311 consecutive in-sync indications while T310 runs stop it;
// T310 expiry declares RLF and triggers the RRC teardown/re-establishment
// machinery (~BearerManagement). Total beacon silence is covered by a watchdog
// (beaconLossTimeout). RLM works independently of handover being enabled, but
// it requires the serving eNB/gNB to broadcast beacons.
//
simple RadioLinkMonitor
{
    parameters:
        @display("i=block/timer");
        @class(RadioLinkMonitor);

        // whether this instance is associated with the NR or the LTE stack
        bool isNr;

        // enables radio link monitoring (RLF detection)
        bool enabled = default(false);

        // out-of-sync threshold: beacon RSSI below this counts as an out-of-sync
        // indication (~10% hypothetical PDCCH BLER in TS 38.133 terms)
        double qOut @unit("dB") = default(-8dB);

        // in-sync threshold: beacon RSSI at/above this counts as an in-sync
        // indication (~2% hypothetical PDCCH BLER); must be >= qOut
        double qIn @unit("dB") = default(-6dB);

        // consecutive out-of-sync indications that start T310 (TS 38.331 default n1)
        int n310 = default(1);

        // consecutive in-sync indications, while T310 runs, that stop it (TS 38.331 default n1)
        int n311 = default(1);

        // radio problem recovery timer: started after n310 out-of-sync indications;
        // its expiry declares radio link failure (TS 38.331 default ms1000)
        double t310 @unit(s) = default(1s);

        // watchdog for total beacon loss: if no serving-cell beacon arrives for this
        // long, RLF is declared directly (out-of-range beacons still arrive in the
        // simulation, so this covers only the serving eNB vanishing or not beaconing;
        // if no beacon was EVER received, it is a configuration error instead).
        // Should be a small multiple of the serving cell's cellInfo.beaconInterval.
        double beaconLossTimeout @unit(s) = default(3s);

        string binderModule = default("binder");
}
