#ifndef STACK_PHY_RADIO_CELLULARRADIO_H_
#define STACK_PHY_RADIO_CELLULARRADIO_H_

#include <inet/physicallayer/wireless/common/radio/packetlevel/Radio.h>
#include <inet/physicallayer/wireless/generic/GenericTransmission.h>
#include <inet/physicallayer/wireless/common/base/packetlevel/TransmitterBase.h>
#include <inet/physicallayer/wireless/generic/GenericReceiver.h>

#include "simu5g/stack/phy/medium/CellularRadioMedium.h"
#include "simu5g/stack/phy/radio/RadioTransmissionRequest.h"

namespace simu5g {

using namespace omnetpp;

class AirFrame;
class IRadioEndpoint;

// SPIKE (stage C): an INET transmission carrying where the PHY sends the frame
class SpikeTransmission : public inet::physicallayer::GenericTransmission
{
  public:
    using GenericTransmission::GenericTransmission;
    bool broadcast = false;
    std::vector<cGate *> targets;
    AirFrame *frame = nullptr;   // a copy of the frame, delivered to the receivers' PHYs (owned)
    short priority = 0;          // the frame's scheduling priority
    ~SpikeTransmission() { delete frame; }
};

class SpikeTransmitter : public inet::physicallayer::TransmitterBase
{
  public:
    const inet::physicallayer::ITransmission *createTransmission(const inet::physicallayer::IRadio *transmitter, const inet::Packet *packet, const simtime_t startTime) const override;
};

class SpikeReceiver : public inet::physicallayer::GenericReceiver
{
  public:
    bool computeIsReceptionAttempted(const inet::physicallayer::IListening *listening, const inet::physicallayer::IReception *reception, inet::physicallayer::IRadioSignal::SignalPart part, const inet::physicallayer::IInterference *interference) const override
    {
        return computeIsReceptionPossible(listening, reception, part);
    }
};

// SPIKE (stage C): CellularRadio is an INET Radio
class CellularRadio : public inet::physicallayer::Radio
{
  protected:
    opp_component_ptr<CellularRadioMedium> cellularMedium_;
    IRadioEndpoint *endpoint_ = nullptr; // the PHY the radio sends and receives frames for
    const RadioTransmissionRequest *pendingRequest_ = nullptr; // the request of the frame being transmitted
    AirFrame *pendingFrame_ = nullptr; // the frame being transmitted
    inet::Coord endingArrivalPosition_; // where the reception being ended arrived
    const AirFrame *endingFrame_ = nullptr; // the frame of the reception being ended
    simtime_t endingArrivalStart_;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleUpperMessage(cMessage *message) override;
    virtual void startReception(cMessage *timer, inet::physicallayer::IRadioSignal::SignalPart part) override;
    virtual void endReception(cMessage *timer) override;
    virtual void sendUp(inet::Packet *macFrame) override;

  public:
    virtual void setEndpoint(IRadioEndpoint *endpoint) { endpoint_ = endpoint; }
    IRadioEndpoint *getEndpoint() const { return endpoint_; }
    const RadioTransmissionRequest *getPendingRequest() const { return pendingRequest_; }
    AirFrame *getPendingFrame() const { return pendingFrame_; }
};

} // namespace simu5g

#endif
