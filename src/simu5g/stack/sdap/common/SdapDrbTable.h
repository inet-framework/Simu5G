//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
///


#ifndef STACK_SDAP_COMMON_SDAPDRBTABLE_H_
#define STACK_SDAP_COMMON_SDAPDRBTABLE_H_

#include <map>
#include <vector>
#include <iostream>
#include "simu5g/stack/rrc/DrbDesc.h"
#include "simu5g/common/LteCommon.h"

namespace simu5g {

//
// SDAP's working copy of the data radio bearer configuration, indexed by QFI and by node
// so that SDAP can map a QoS flow onto a bearer. SDAP never authors this table: entries
// are pushed by RRC (see NrSdap::configureDrb), from the configuration authored in RRC's
// ~DrbTable (drbConfig parameter).
//
class SdapDrbTable
{
  protected:
    // Primary table: DrbKey(nodeId, drbId) -> DrbDesc (owns DrbDesc objects)
    std::map<DrbKey, DrbDesc> drbMap_;

    // Reverse lookup: (nodeId, qfi) -> DrbDesc*  (UE uses NODEID_NONE as nodeId)
    std::map<std::pair<MacNodeId, Qfi>, const DrbDesc *> qfiToDrb_;

    // Default DRB per nodeId (UE: NODEID_NONE key; gNB: per-UE keys)
    std::map<MacNodeId, const DrbDesc *> defaultDrb_;

    // UE shortcut: default DRB for NODEID_NONE (nullptr if not configured)
    const DrbDesc *ueDefaultDrb_ = nullptr;

  public:
    // Inserts or replaces the entry for drb.key and updates the QFI and default-DRB
    // indexes accordingly. The entry arrives with isDefault already resolved (the
    // authoring side assigns the default DRB).
    void addOrUpdateDrb(const DrbDesc& drb);

    // Primary lookup by DrbKey
    const DrbDesc *getDrb(DrbKey key) const;

    // Reverse lookup: (nodeId, qfi) -> DrbDesc  (UE passes NODEID_NONE)
    const DrbDesc *getDrbForQfi(MacNodeId nodeId, Qfi qfi) const;

    // Default DRB for a given nodeId (gNB: per-UE; UE: NODEID_NONE)
    const DrbDesc *getDefaultDrb(MacNodeId nodeId) const;

    // UE shortcut: default DRB (NODEID_NONE)
    const DrbDesc *getDefaultDrb() const { return ueDefaultDrb_; }

    // Access full DRB map
    const std::map<DrbKey, DrbDesc>& getDrbMap() const { return drbMap_; }

    void dump(std::ostream& os = std::cout) const;
};

} // namespace simu5g

#endif /* STACK_SDAP_COMMON_SDAPDRBTABLE_H_ */
