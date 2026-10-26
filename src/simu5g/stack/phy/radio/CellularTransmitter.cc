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

#include "simu5g/stack/phy/radio/CellularTransmitter.h"

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/phy/channelmodel/IRadioEndpoint.h"
#include "simu5g/stack/phy/radio/RadioTransmissionRequest.h"
#include "simu5g/stack/phy/radio/CellularRadio.h"
#include "simu5g/stack/phy/packet/AirFrame_m.h"
#include <inet/physicallayer/wireless/common/analogmodel/unitdisk/UnitDiskTransmissionAnalogModel.h>

namespace simu5g {

Define_Module(CellularTransmitter);

CellularTransmission *CellularTransmitter::createTransmission(const RadioTransmissionRequest& request, const UserControlInfo& info) const
{
    auto transmission = new CellularTransmission();
    transmission->transmitter = request.sender;
    transmission->sourceId = info.getSourceId();
    transmission->destId = info.getDestId();
    transmission->cellId = request.cellId;
    transmission->direction = (Direction)info.getDirection();
    transmission->frameType = (LtePhyFrameType)info.getFrameType();
    transmission->carrierFrequency = info.getCarrierFrequency();
    transmission->grantedBlocks = info.getGrantedBlocks();
    // a D2D frame, one-to-one or one-to-many, is sent at the UE's D2D power, which its control info carries
    // besides the cellular one
    bool d2d = transmission->direction == D2D || transmission->direction == D2D_MULTI;
    transmission->txPower = d2d ? info.getD2dTxPower() : info.getTxPower();
    transmission->startPosition = request.sender->getCoord();
    transmission->txDirection = request.sender->getTxDirection();
    transmission->txAngle = request.sender->getTxAngle();
    transmission->startTime = simTime();
    transmission->duration = request.duration;
    return transmission;
}

CellularTransmission *CellularTransmitter::createPhantomTransmission(IRadioEndpoint *sender, MacNodeId destId, MacCellId cellId, Direction direction,
        GHz carrierFrequency, const RbMap& blocks, simtime_t duration, const cModule *phantomCell) const
{
    auto transmission = new CellularTransmission();
    transmission->transmitter = sender;
    transmission->phantomCell = phantomCell;
    transmission->sourceId = sender->getMacNodeId();
    transmission->destId = destId;
    transmission->cellId = cellId;
    transmission->direction = direction;
    transmission->frameType = DATAPKT;
    transmission->carrierFrequency = carrierFrequency;
    transmission->grantedBlocks = blocks;
    transmission->txPower = sender->getTxPwr(direction);
    transmission->startPosition = sender->getCoord();
    transmission->txDirection = sender->getTxDirection();
    transmission->txAngle = sender->getTxAngle();
    transmission->startTime = simTime();
    transmission->duration = duration;
    return transmission;
}

CellularInetTransmission::~CellularInetTransmission()
{
    delete frame;
}

const inet::physicallayer::ITransmission *CellularTransmitter::createTransmission(const inet::physicallayer::IRadio *transmitter, const inet::Packet *packet, const simtime_t startTime) const
{
    using namespace inet;
    using namespace inet::physicallayer;
    auto radio = check_and_cast<const CellularRadio *>(transmitter);
    const RadioTransmissionRequest *request = radio->getPendingRequest();
    simtime_t duration = request->duration;
    auto mobility = transmitter->getAntenna()->getMobility();
    auto position = mobility->getCurrentPosition();
    auto orientation = mobility->getCurrentAngularPosition();
    auto analogModel = new UnitDiskTransmissionAnalogModel(0, 0, duration, m(1e9), m(1e9), m(1e9));
    auto transmission = new CellularInetTransmission(transmitter, packet, startTime, startTime + duration, 0, 0, duration,
            position, position, orientation, orientation, nullptr, nullptr, nullptr, nullptr, analogModel);
    transmission->broadcast = request->broadcast;
    transmission->targets = request->targets;
    transmission->frame = radio->getPendingFrame()->dup();
    transmission->priority = radio->getPendingFrame()->getSchedulingPriority();
    return transmission;
}

} // namespace simu5g
