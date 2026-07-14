//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/d2d/mac/scheduler/LcgSchedulerD2D.h"
#include "simu5g/stack/mac/buffer/LteMacBuffer.h"

namespace simu5g {

using namespace omnetpp;

bool LcgSchedulerD2D::checkForPendingAdditionalBsr(Direction grantDir, LteTrafficClass tc)
{
    // FIXME This is a workaround in case a UE has both UL and D2D active connections.
    //       When an UL grant has been received, check if there is data in the D2D connections' buffer
    //       and if the bsrTriggered flag is set. If so, do not schedule any UL connection and use the
    //       grant for sending the BSR related to the D2D connection(s).
    //       A smarter policy should be implemented
    if (grantDir == UL && mac_->bsrTriggered()) {
        const LcgMap& lcgMap = mac_->getLcgMap();
        auto it_pair = lcgMap.equal_range(tc);
        // look for an active D2D connection
        for (auto it = it_pair.first; it != it_pair.second; ++it) {
            // get the Flow descriptor
            MacCid cid = it->second.first;
            const FlowDescriptor& connDesc = mac_->getConnDesc(cid);
            if (connDesc.getDirection() == D2D) {
                // get the connection virtual buffer
                LteMacBuffer *vQueue = it->second.second;
                // get the buffer size
                unsigned int queueLength = vQueue->getQueueOccupancy(); // in bytes
                if (queueLength != 0)
                    return true;
            }
        }
    }
    return false;
}

} //namespace
