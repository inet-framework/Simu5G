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

#ifndef _RLC_MUX_D2D_H_
#define _RLC_MUX_D2D_H_

#include "simu5g/stack/rlc/RlcMux.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @class RlcMuxD2D
 * @brief D2D-aware lower-layer RLC packet dispatcher.
 *
 * Extends ~RlcMux with the handling of D2D mode switch notifications
 * arriving from the MAC layer: the notification is dispatched to the
 * TX or RX UM entity of the affected bearer, which adapts its buffers
 * and sequence numbering to the new mode. Used by the D2D-capable NICs
 * (LteNicUeD2D, LteNicEnbD2D, NrNicUeD2D, NrNicEnbD2D).
 */
class RlcMuxD2D : public RlcMux
{
  protected:
    void fromMacLayer(cPacket *pkt) override;
};

} // namespace simu5g

#endif
