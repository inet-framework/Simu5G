//
//                  Simu5G
//
// Copyright (C) 2012 Antonio Virdis, Daniele Migliorini, Matteo Maria Andreozzi,
//   Giovanni Accongiagioco, Generoso Pagano, Vincenzo Pii (SimuLTE)
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_RLCTMRXENTITY_H_
#define _SIMU5G_RLCTMRXENTITY_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/rlc/RlcRxEntityBase.h"

namespace simu5g {

/**
 * @brief Receiver entity for Transparent Mode (TM).
 *
 * Simply forwards received PDUs to the upper layer without
 * reassembly or reordering.
 */
class RlcTmRxEntity : public RlcRxEntityBase
{
  protected:
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
};

} // namespace simu5g

#endif
