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

#ifndef STACK_PHY_RADIO_CELLULARRADIO_H_
#define STACK_PHY_RADIO_CELLULARRADIO_H_

#include <omnetpp.h>
#include <inet/common/ModuleRefByPar.h>
#include <inet/physicallayer/wireless/common/radio/packetlevel/Radio.h>

#include "simu5g/stack/phy/medium/CellularRadioMedium.h"

namespace simu5g {

using namespace omnetpp;

class AirFrame;
class IRadioEndpoint;

/**
 * See the NED documentation of CellularRadio. A received frame is held until
 * the end of its transmission (it is rescheduled as a self-message) and then
 * passed up unchanged; a frame from the PHY carries a RadioTransmissionRequest
 * that says where to send it.
 */
class RadioTransmissionRequest;

class CellularRadio : public inet::physicallayer::Radio
{
  protected:
    opp_component_ptr<CellularRadioMedium> cellularMedium_;
    IRadioEndpoint *endpoint_ = nullptr; // the PHY the radio sends and receives frames for
    bool insideBuilding_ = false; // whether the radio is inside a building, and how far from its wall
    double insideDistance_ = 0.0;
    double penetrationLossDeviate_ = 0.0; // the standard-normal deviate of its building penetration loss
    // SPIKE: the frame being sent, and the reception being ended
    const RadioTransmissionRequest *pendingRequest_ = nullptr;
    AirFrame *pendingFrame_ = nullptr;
    inet::Coord endingArrivalPosition_;
    const AirFrame *endingFrame_ = nullptr;
    simtime_t endingArrivalStart_;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleUpperMessage(cMessage *message) override;
    virtual void startReception(cMessage *timer, inet::physicallayer::IRadioSignal::SignalPart part) override;
    virtual void endReception(cMessage *timer) override;
    virtual void sendUp(inet::Packet *macFrame) override;
    // SPIKE: INET's createSignal() without its ASSERT on a zero duration (beacons take no air time)
    virtual inet::physicallayer::WirelessSignal *createSignal(inet::Packet *packet) const override;

  public:
    IRadioEndpoint *getEndpoint() const { return endpoint_; }
    const RadioTransmissionRequest *getPendingRequest() const { return pendingRequest_; }
    AirFrame *getPendingFrame() const { return pendingFrame_; }
    /** Sets the PHY the radio sends and receives frames for, whose position is the radio's. */
    virtual void setEndpoint(IRadioEndpoint *endpoint) { endpoint_ = endpoint; }

    /** Whether the radio is inside a building. */
    bool isInsideBuilding() const { return insideBuilding_; }
    /** How far inside its building the radio is, in m (drawn once). */
    double getInsideDistance() const { return insideDistance_; }
    /** The standard-normal deviate of its building penetration loss (drawn once). */
    double getPenetrationLossDeviate() const { return penetrationLossDeviate_; }
};

} // namespace simu5g

#endif
