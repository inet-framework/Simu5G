//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIDELINK_SLGNBRRC_H_
#define _SIDELINK_SLGNBRRC_H_

#include <set>

#include <omnetpp.h>

#include <memory>

#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/common/SlPreconfig.h"
#include "simu5g/stack/sidelink/mac/SlEnbScheduler.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

namespace simu5g {

/**
 * gNB-side sidelink control plane (design decision D25, SL-3): owns the
 * cell's sidelink resource pool configuration and the registry of sidelink
 * UEs served by this cell. Registered in SlBinder by cell id; attached UEs
 * with poolSource="servingCell" resolve their pool from here instead of
 * their local preconfig ("SIB12-equivalent" - genie provisioning, no
 * over-the-air system information transport is modeled, see the plan's D25).
 *
 * The Mode-1 resource allocator (SlEnbScheduler, D29) is owned by this
 * module from WP-O on.
 */
class SlGnbRrc : public omnetpp::cSimpleModule
{
  protected:
    MacNodeId cellId_ = NODEID_NONE;

    // the cell's SL pool configuration: only the *pool section* of the
    // SlPreconfig shape is meaningful here (carrier, numerology, subchannel
    // geometry, mode-2 selection params, PSFCH fields); the bearer-side
    // sections (slrbConfig, unicastSlrbDefaults, ...) stay UE-local, as the
    // real SIB12 carries pools, not app bearers
    SlPreconfig poolConfig_;

    // sidelink UEs provisioned from this cell (per-UE mode/CG registry
    // extended in WP-O/WP-P)
    std::set<MacNodeId> slUes_;

    // the mode-1 allocator (D29) and the SL pool's slot grid
    std::unique_ptr<SlEnbScheduler> slEnbScheduler_;
    SlSlotGrid slotGrid_;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;

  public:
    MacNodeId getCellId() const { return cellId_; }
    const SlPreconfig& getPoolConfig() const { return poolConfig_; }

    /// called by an attached UE's SlRrc at pool-resolution time (D25)
    void registerSlUe(MacNodeId ueId);
    const std::set<MacNodeId>& getSlUes() const { return slUes_; }

    /// an SL-BSR arrived at the gNB MAC (D26/D29): run the allocator and
    /// return the grant spec for NrMacGnbSl to send (invalid = no resources)
    SlEnbScheduler::GrantSpec onSlBsr(MacNodeId ueId, int reportedBytes);
};

} // namespace simu5g

#endif
