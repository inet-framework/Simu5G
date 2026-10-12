// SPIKE (stage C): CellularRadio is an INET Radio
#include "simu5g/stack/phy/radio/CellularRadio.h"

#include <inet/common/packet/chunk/cPacketChunk.h>
#include <inet/common/ProtocolTag_m.h>
#include <inet/common/Protocol.h>
#include <inet/physicallayer/wireless/common/signal/WirelessSignal.h>

#include "simu5g/stack/phy/channelmodel/IRadioEndpoint.h"
#include "simu5g/stack/phy/packet/AirFrame_m.h"
#include "simu5g/stack/phy/radio/CellularTransmitter.h"

namespace simu5g {

using namespace inet;
using namespace inet::physicallayer;

Define_Module(CellularRadio);
Define_Module(SpikeTransmitter);
Define_Module(SpikeReceiver);

static const Protocol cellularAirFrame("cellularAirFrame", "Simu5G air frame");

const ITransmission *SpikeTransmitter::createTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime) const
{
    auto radio = check_and_cast<const CellularRadio *>(transmitter);
    const RadioTransmissionRequest *request = radio->getPendingRequest();
    simtime_t duration = request->duration;
    auto mobility = transmitter->getAntenna()->getMobility();
    auto position = mobility->getCurrentPosition();
    auto orientation = mobility->getCurrentAngularPosition();
    auto analogModel = getAnalogModel()->createAnalogModel(0, 0, duration, Hz(NaN), Hz(NaN), W(NaN));
    auto transmission = new SpikeTransmission(transmitter, packet, startTime, startTime + duration, 0, 0, duration,
            position, position, orientation, orientation, nullptr, nullptr, nullptr, nullptr, analogModel);
    transmission->broadcast = request->broadcast;
    transmission->targets = request->targets;
    transmission->frame = radio->getPendingFrame()->dup();
    transmission->priority = radio->getPendingFrame()->getSchedulingPriority();
    return transmission;
}

void CellularRadio::initialize(int stage)
{
    Radio::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        cellularMedium_ = check_and_cast<CellularRadioMedium *>(getSimulation()->findModuleByPath("radioMedium"));
    else if (stage == INITSTAGE_LAST)
        setRadioMode(RADIO_MODE_TRANSCEIVER);
}

void CellularRadio::handleUpperMessage(cMessage *message)
{
    auto frame = check_and_cast<AirFrame *>(message);
    auto request = check_and_cast<RadioTransmissionRequest *>(frame->removeControlInfo());
    // the medium's own registry, which the channel models read
    auto transmitter = check_and_cast<CellularTransmitter *>(getSubmodule("cellularTransmitter"));
    cellularMedium_->addTransmission(transmitter->createTransmission(*request, frame->getAdditionalInfo()));
    pendingFrame_ = frame;
    auto packet = new Packet(frame->getName(), makeShared<cPacketChunk>(frame->dup()));
    packet->addTag<PacketProtocolTag>()->setProtocol(&cellularAirFrame);
    pendingRequest_ = request;
    handleUpperPacket(packet);
    pendingRequest_ = nullptr;
    pendingFrame_ = nullptr;
    delete frame;
    delete request;
}

void CellularRadio::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    // the reception ends with the frame's scheduling priority, as the frame did
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    timer->setSchedulingPriority(signal->getSchedulingPriority());
    Radio::startReception(timer, part);
}

void CellularRadio::endReception(cMessage *timer)
{
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    endingArrivalPosition_ = signal->getArrival()->getStartPosition();
    endingArrivalStart_ = signal->getArrival()->getStartTime();
    endingFrame_ = check_and_cast<const SpikeTransmission *>(signal->getTransmission())->frame;
    Radio::endReception(timer);
}

void CellularRadio::sendUp(Packet *macFrame)
{
    auto frame = endingFrame_->dup();
    frame->setArrivalPosition(endingArrivalPosition_);
    frame->setReceptionStartTime(endingArrivalStart_);
    delete macFrame;
    send(frame, upperLayerOutGateId);
}

} // namespace simu5g
