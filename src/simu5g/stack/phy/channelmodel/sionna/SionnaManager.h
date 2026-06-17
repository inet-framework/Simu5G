//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef SIONNAMANAGER_H_
#define SIONNAMANAGER_H_

#include <omnetpp.h>

#include "simu5g/stack/phy/channelmodel/sionna/ManifestReader.h"
#include "simu5g/stack/phy/channelmodel/sionna/SionnaTable.h"

namespace simu5g {

using namespace omnetpp;

//
// Loads the offline Sionna artifact (manifest + binary path-gain table) once and
// asserts the parameter contract (carrier frequency, SCS/numerology, band count,
// coord transform, request hash, ...) against the live scenario, failing loud on any
// mismatch. SionnaChannelModel instances acquire the loaded table from this manager.
//
// This is a compilable skeleton; the artifact load and fail-loud contract assertions
// are filled in by Plan 01-03.
//
class SionnaManager : public cSimpleModule
{
  public:
    // Schema version this build understands. A manifest produced by Plan 01-01
    // carries schema_version == 1; any other value aborts (CAL-02).
    static const int EXPECTED_SCHEMA_VERSION = 1;

    // Live-scenario contract values the manifest must match. Kept as a small
    // value type so the fail-loud assertion is unit-testable without a running
    // module (the offline tests construct one directly).
    struct LiveContract {
        double carrier_frequency_hz = 0.0;
        double subcarrier_spacing_hz = 0.0;
        int num_bands = 0;
    };

  protected:
    Manifest manifest_;
    SionnaTable table_;

  public:
    void initialize(int stage) override;

    /*
     * @return the loaded, contract-validated path-gain table.
     */
    const SionnaTable& getTable() const { return table_; }

    /*
     * Fail-loud assertion of the full manifest contract against the live scenario.
     * Throws cRuntimeError on the FIRST mismatching field — schema_version, carrier
     * frequency, subcarrier spacing, band count, identity coord_transform, or an
     * empty request_hash. No silent fallback to the analytic model (CAL-02, Pitfall 5).
     * Pure (no module/par access) so it is unit-testable in isolation.
     *
     * @param m parsed manifest
     * @param live live-scenario contract values
     */
    static void assertContractMatchesLiveScenario(const Manifest& m, const LiveContract& live);
};

} //namespace

#endif /* SIONNAMANAGER_H_ */
