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

#include "simu5g/stack/phy/radio/CellularRadio.h"

#include "simu5g/stack/phy/channelmodel/IRadioEndpoint.h"
#include "simu5g/stack/phy/packet/AirFrame_m.h"
#include "simu5g/stack/phy/radio/CellularTransmitter.h"
#include "simu5g/stack/phy/radio/RadioTransmissionRequest.h"
#include <inet/common/packet/chunk/cPacketChunk.h>
#include <inet/common/ProtocolTag_m.h>
#include <inet/common/Protocol.h>
#include <inet/physicallayer/wireless/common/signal/WirelessSignal.h>

namespace simu5g {

Define_Module(CellularRadio);

using namespace inet;
using namespace inet::physicallayer;

static const Protocol cellularAirFrame("cellularAirFrame", "Simu5G air frame");

void CellularRadio::initialize(int stage)
{
    Radio::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        cellularMedium_ = check_and_cast<CellularRadioMedium *>(medium.get());
        insideBuilding_ = par("insideBuilding");
        if (insideBuilding_) {
            insideDistance_ = uniform(0.0, 25.0);
            penetrationLossDeviate_ = normal(0.0, 1.0);
        }
    }
    else if (stage == INITSTAGE_LAST)
        setRadioMode(RADIO_MODE_TRANSCEIVER);
}

void CellularRadio::handleUpperMessage(cMessage *message)
{
    auto frame = check_and_cast<AirFrame *>(message);
    auto request = check_and_cast<RadioTransmissionRequest *>(frame->removeControlInfo());
    // Simu5G's registry of the transmission, which the channel models read
    auto cellularTransmitter = check_and_cast<CellularTransmitter *>(getSubmodule("transmitter"));
    CellularTransmission *transmission = cellularTransmitter->createTransmission(*request, frame->getAdditionalInfo());
    frame->setStartPosition(transmission->startPosition);
    cellularMedium_->addTransmission(transmission);
    // INET's transmission, carrying the frame
    pendingFrame_ = frame;
    pendingRequest_ = request;
    auto packet = new Packet(frame->getName(), makeShared<cPacketChunk>(frame->dup()));
    packet->addTag<PacketProtocolTag>()->setProtocol(&cellularAirFrame);
    handleUpperPacket(packet);
    pendingRequest_ = nullptr;
    pendingFrame_ = nullptr;
    delete frame;
    delete request;
}

WirelessSignal *CellularRadio::createSignal(Packet *packet) const
{
    encapsulate(packet);
    return check_and_cast<WirelessSignal *>(medium->transmitPacket(this, packet));
}

void CellularRadio::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    // the reception ends with the frame's scheduling priority
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    timer->setSchedulingPriority(signal->getSchedulingPriority());
    Radio::startReception(timer, part);
}

void CellularRadio::endReception(cMessage *timer)
{
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    endingArrivalPosition_ = signal->getArrival()->getStartPosition();
    endingArrivalStart_ = signal->getArrival()->getStartTime();
    endingFrame_ = check_and_cast<const CellularInetTransmission *>(signal->getTransmission())->frame;
    Radio::endReception(timer);
}

void CellularRadio::sendUp(Packet *macFrame)
{
    // the frame the transmission carried, with where and when it arrived
    auto frame = endingFrame_->dup();
    frame->setArrivalPosition(endingArrivalPosition_);
    frame->setReceptionStartTime(endingArrivalStart_);
    delete macFrame;
    send(frame, upperLayerOutGateId);
}

} // namespace simu5g
