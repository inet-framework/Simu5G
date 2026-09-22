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

import simu5g.common.LteControlInfoTags;

namespace simu5g;

//
// Everything the receiver of an AirFrame needs to know about the
// transmission, one part per concern; each part has the type of that
// concern's tag in LteControlInfoTags.msg.
//
// The sending PHY builds it from the control-info tags the MAC attached to
// the packet, completes it with what only the PHY knows (its position and
// transmit power), and sends it with the frame; the tags themselves stay in
// the node. The receiving PHY and its channel models compute on it, and the
// receiving PHY turns it back into tags for its MAC.
//
class TransmissionDescriptor extends cObject
{
    NodeIdentificationInd identity;             // source and destination node
    TrafficDirectionInd trafficDirection;
    LogicalConnectionInd logicalConnection;     // LCID, and D2D multicast group
    CarrierConfigurationInd carrier;
    HarqInfoInd harq;
    PhyTransmissionInd phyTransmission;         // TX mode, frame type, TX power, RBs, sender position, feedback request
    UserTransmissionParametersInd txParams;
}
