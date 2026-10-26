//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_PHY_RADIO_CELLULARTRANSMITTER_H_
#define STACK_PHY_RADIO_CELLULARTRANSMITTER_H_

#include <omnetpp.h>
#include <inet/physicallayer/wireless/common/base/packetlevel/TransmitterBase.h>
#include <inet/physicallayer/wireless/generic/GenericTransmission.h>

#include "simu5g/stack/phy/medium/CellularTransmission.h"

namespace simu5g {

using namespace omnetpp;

class IRadioEndpoint;
class AirFrame;

// SPIKE: an INET transmission carrying where the PHY sends the frame
class CellularInetTransmission : public inet::physicallayer::GenericTransmission
{
  public:
    using GenericTransmission::GenericTransmission;
    bool broadcast = false;
    std::vector<cGate *> targets;
    AirFrame *frame = nullptr;   // a copy of the frame, delivered to the receivers' PHYs (owned)
    short priority = 0;          // the frame's scheduling priority
    ~CellularInetTransmission();
};
class RadioTransmissionRequest;
class UserControlInfo;

/**
 * See the NED documentation of CellularTransmitter.
 */
class CellularTransmitter : public inet::physicallayer::TransmitterBase
{
  public:
    // SPIKE: INET's transmission of the frame the radio is sending
    virtual const inet::physicallayer::ITransmission *createTransmission(const inet::physicallayer::IRadio *transmitter, const inet::Packet *packet, const simtime_t startTime) const override;

    /**
     * The transmission of a frame with the given control information, sent
     * now as the PHY requested: by its radio, for the cell and the duration
     * given in the request. The caller owns the result.
     */
    virtual CellularTransmission *createTransmission(const RadioTransmissionRequest& request, const UserControlInfo& info) const;

    /**
     * A transmission that stands for no frame (see
     * CellularRadioMedium::addPhantomTransmission()): the sender's, to the
     * given destination (NODEID_NONE for none) for the given cell, in the
     * given direction on the given blocks, starting now where the sender is,
     * at its power and with its antenna, for the given duration. phantomCell
     * is the cell if it is not a node (an external or a background cell),
     * else nullptr. The caller owns the result.
     */
    virtual CellularTransmission *createPhantomTransmission(IRadioEndpoint *sender, MacNodeId destId, MacCellId cellId, Direction direction,
            GHz carrierFrequency, const RbMap& blocks, simtime_t duration, const cModule *phantomCell) const;
};

} // namespace simu5g

#endif
