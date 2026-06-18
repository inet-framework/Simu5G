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

#include "simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h"

#include <inet/common/InitStages.h>

namespace simu5g {

Define_Module(SionnaChannelModel);

void SionnaChannelModel::initialize(int stage)
{
    // Chain the full NrChannelModel/LteRealisticChannelModel init so the entire
    // inherited SINR/RSRP/interference machinery is set up unchanged (MOD-01).
    NrChannelModel::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        // Acquire the loaded, contract-validated table source. Fail loud if absent —
        // never silently fall back to the analytic path loss (CAL-02).
        const char *managerPath = par("sionnaManagerModule").stringValue();
        cModule *mod = getModuleByPath(managerPath);
        if (mod == nullptr)
            throw cRuntimeError("SionnaChannelModel: sionnaManagerModule '%s' not found", managerPath);
        sionnaManager_ = dynamic_cast<SionnaManager *>(mod);
        if (sionnaManager_ == nullptr)
            throw cRuntimeError("SionnaChannelModel: module '%s' is not a SionnaManager", managerPath);

        // v1 invariant: linkKeyFor always returns link index 0, which is only correct
        // when there is exactly one link in the table. Fail loud if the artifact has
        // more than one link so a mis-configured multi-cell scenario is caught before
        // it silently returns link 0's path gain for every interferer (WR-01).
        // v2 will remove this guard once linkKeyFor maps (Tx, Rx) to the correct row.
        const std::size_t tableSize = sionnaManager_->getTable().size();
        if (tableSize != 1)
            throw cRuntimeError("SionnaChannelModel: artifact table has %lu link(s) but "
                                "v1 supports exactly 1 (linkKeyFor is hard-wired to index 0); "
                                "use a single-link artifact or upgrade to v2", (unsigned long)tableSize);
    }
}

std::size_t SionnaChannelModel::linkKeyFor(MacNodeId nodeId, Direction dir, const inet::Coord& coord) const
{
    // v1: a single empty-world Tx/Rx link -> link index 0. v2 maps (Tx, Rx) to its row.
    (void)nodeId;
    (void)dir;
    (void)coord;
    return 0;
}

double SionnaChannelModel::getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl)
{
    (void)cqiDl;

    // Keep the OMNeT++ Euclidean distance computation through the same funnel as the
    // analytic model: this is what proves the TOOL-02 coord transform is correct (the
    // offline Friis cross-check is computed at exactly this distance). The value is not
    // fed into a path-loss formula here; the Sionna table already encodes propagation.
    double threeDimDistance = phy_->getCoord().distance(coord);
    (void)threeDimDistance;

    // Substitute the precomputed Sionna path gain (dB) for the analytic path loss:
    // getSINR consumes attenuation as recvPower -= attenuation, so attenuation must be
    // the POSITIVE loss; a Sionna path gain of e.g. -83 dB -> +83 dB attenuation (MOD-01).
    const std::size_t linkIndex = linkKeyFor(nodeId, dir, coord);
    return -sionnaManager_->getTable().lookup(linkIndex);

    // Assumption A3 (MOD-02, deferred to Phase 3): this override bypasses the analytic
    // path-loss and shadowing computations entirely, but it does not yet suppress the
    // inherited statistical shadowing/fading/LOS draws at the source level. Phase 1
    // keeps them inert via ini config (shadowing=false, fading=false, fixedLos=true);
    // the source-level removal of those terms is Plan MOD-02 in Phase 3.
}

} //namespace
