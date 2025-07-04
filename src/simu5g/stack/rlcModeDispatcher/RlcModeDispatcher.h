//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_RLCMODEDISPATCHER_H_
#define _LTE_RLCMODEDISPATCHER_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

/**
 * @class RlcModeDispatcher
 * @brief RLC Mode Dispatcher for LTE/5G Stack
 *
 * This module handles the dispatch of packets to appropriate RLC modes (AM, UM, TM)
 * based on traffic classification and configuration. It factors out the RLC mode
 * selection logic from the PDCP layer to provide better separation of concerns.
 *
 * The dispatcher performs the following tasks:
 * - Traffic classification based on application type and packet characteristics
 * - RLC type determination based on traffic class and configuration
 * - Packet dispatching to the appropriate RLC SAP (Service Access Point)
 */
class RlcModeDispatcher : public cSimpleModule
{
  protected:
    // RLC type configuration for different traffic classes
    LteRlcType conversationalRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType streamingRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType interactiveRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType backgroundRlc_ = UNKNOWN_RLC_TYPE;

    // Gate references
    cGate *pdcpInGate_ = nullptr;

    /**
     * Initialize the dispatcher with configuration parameters
     */
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    /**
     * Handle incoming messages from PDCP
     */
    void handleMessage(cMessage *msg) override;

    /**
     * Classify traffic based on packet characteristics and application type
     *
     * @param pkt The packet to classify
     * @return The traffic class for the packet
     */
    LteTrafficClass classifyTraffic(cPacket *pkt);

    /**
     * Determine the appropriate RLC type for a given traffic class
     *
     * @param trafficClass The traffic class
     * @return The RLC type to use for this traffic class
     */
    LteRlcType determineRlcType(LteTrafficClass trafficClass);

    /**
     * Set traffic information in the control info, including RLC type
     *
     * @param pkt The packet to process
     * @param lteInfo The control info to update
     * @param direction The traffic direction (UL/DL)
     */
    void setTrafficInformation(cPacket *pkt, inet::Ptr<FlowControlInfo> lteInfo, Direction direction);

    /**
     * Dispatch a packet to the appropriate RLC layer based on its RLC type
     *
     * @param pkt The packet to dispatch
     */
    void dispatchToRlc(inet::Packet *pkt);

  public:
    /**
     * Destructor
     */
    ~RlcModeDispatcher() override = default;
};

} //namespace

#endif
