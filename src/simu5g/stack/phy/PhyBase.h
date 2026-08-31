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

#ifndef _PHYBASE_H_
#define _PHYBASE_H_

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <math.h>
#include <inet/common/ModuleRefByPar.h>
#include <inet/common/Units.h>

#include "simu5g/world/radio/ChannelAccess.h"
#include "simu5g/world/radio/ChannelControl.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
#include "simu5g/stack/mac/amc/LteAmc.h"
#include "simu5g/stack/phy/channelmodel/RadioBase.h"
#include "simu5g/stack/phy/feedback/LteFeedbackComputationRealistic.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief Physical layer of Lte Nic.
 *
 * This class implements the physical layer of the Lte Nic.
 * It contains methods to manage analog models and the decider.
 *
 * The module receives packets from the LteStack and
 * sends them to the air channel, encapsulated in LteAirFrames.
 *
 * The module receives LteAirFrames from the radioIn gate,
 * filters the received signal using the analog models,
 * processes the received signal using the decider,
 * then decapsulates the inner packet and sends it to the
 * LteStack with LteDeciderControlInfo attached.
 */

class PhyBase : public ChannelAccess
{

  protected:

    /**
     * Defines the scheduling priority of AirFrames.
     *
     * AirFrames use a slightly higher priority than normal to ensure
     * channel consistency. This means that before anything else happens
     * at a time point t every AirFrame which ended at t has been removed and
     * every AirFrame started at t has been added to the channel.
     *
     * An example where this matters is a ChannelSenseRequest which ends at
     * the same time as an AirFrame starts (or ends). Depending on which message
     * is handled first the result of ChannelSenseRequest would differ.
     */
    static short airFramePriority_;
    /**
     * The carriers this PHY leg's one radio endpoint (primaryRadio_)
     * serves (radio endpoint recast E8, §3(c)/§4): channelModel_'s old
     * per-carrier map collapses to this set plus the one endpoint reference
     * below, since one endpoint module now serves every carrier of the leg.
     * A std::set so iteration stays in ascending-frequency order, exactly
     * what range-for over the old std::map<GHz, ...> gave every reader
     * (sendFeedback(), LteMacUe's per-carrier scheduler setup) -- registration
     * order (componentCarrierModules' declaration order) is a separate
     * concern, preserved by RadioBase::getComponentCarriers() instead.
     */
    std::set<GHz> servedCarriers_;
    inet::ModuleRefByPar<RadioBase> primaryRadio_;

    /** The id of the in-data gate from the Stack */
    int upperGateIn_ = -1;
    /** The id of the out-data gate to the Stack */
    int upperGateOut_ = -1;
    /** The id of the radioIn gate to receive LteAirFrames */
    int radioInGate_ = -1;

    /** Statistics */
    unsigned int numAirFrameReceived_ = 0;    /// number of LteAirFrame correctly received
    unsigned int numAirFrameNotReceived_ = 0; /// number of LteAirFrame not received

    /** Local device MacNodeId */
    MacNodeId nodeId_ = NODEID_NONE;

    /** Node type */
    RanNodeType nodeType_ = UNKNOWN_NODE_TYPE;

    /// Reference to Binder
    inet::ModuleRefByPar<Binder> binder_;

    /// Reference to CellInfo
    opp_component_ptr<CellInfo> cellInfo_;

    //Ue  Tx Power
    double ueTxPower_ = NAN;
    // eNodeB Tx Power
    double eNodeBtxPower_ = NAN;
    //Micro eNb Tx Power
    double microTxPower_ = NAN;
    // Tx Power
    double txPower_ = NAN;
    // Tx Direction
    TxDirectionType txDirection_ = OMNI;
    // Tx Angle
    double txAngle_ = NAN;
    // Attenuation array
    AttenuationVector attenuationVector_;
    //Used only for PisaPhy
    LteFeedbackComputation *lteFeedbackComputation_ = nullptr;

    /*
     * NR Support
     */
    bool isNr_ = false;           // this flag is true if this module is part of the NR stack

    //Statistics
    static simsignal_t averageCqiDlSignal_;
    static simsignal_t averageCqiUlSignal_;

    // last time that the node has transmitted (currently, used only by UEs)
    simtime_t lastActive_;

  public:

    const RadioBase *getPrimaryChannelModel()
    {
        return primaryRadio_;
    }

    /*
     * The carrier frequency of this PHY leg's primary carrier -- the
     * concept the three call sites in PhyEnb/PhyUe that mean "this leg's
     * carrier" actually want (radio endpoint recast E7). Flattened onto
     * PhyBase, rather than left as a direct primaryRadio_ read, so
     * a later collapse of the carrier vector (E8) only has to change this
     * one place.
     */
    GHz getPrimaryCarrierFrequency() const
    {
        return primaryRadio_->getCarrierFrequency();
    }

    /** Which carriers this leg's one radio endpoint serves, ascending by frequency (radio endpoint recast E8). */
    const std::set<GHz>& getServedCarriers()
    {
        return servedCarriers_;
    }

    RadioBase *getRadio(GHz carrierFreq = GHz(0.0))
    {
        if (primaryRadio_ == nullptr)
            return nullptr;
        // when not specified, returns the one radio endpoint (there is only
        // ever one per leg since E8); otherwise it must be one of the
        // carriers that endpoint actually serves
        if (carrierFreq == GHz(0.0) || servedCarriers_.count(carrierFreq))
            return primaryRadio_;
        return nullptr;
    }

    // Compatibility alias for BackgroundCellChannelModel.cc's one remaining
    // caller; remove this together with BackgroundCellChannelModel.* .
    RadioBase *getChannelModel(GHz carrierFreq = GHz(0.0)) { return getRadio(carrierFreq); }

    double getMicroTxPwr()
    {
        return microTxPower_;
    }

    double getMacroTxPwr()
    {
        return eNodeBtxPower_;
    }

    virtual double getTxPwr(Direction dir = UNKNOWN_DIRECTION)
    {
        return txPower_;
    }

    TxDirectionType getTxDirection()
    {
        return txDirection_;
    }

    double getTxAngle()
    {
        return txAngle_;
    }

    /**
     * Delivers a decoded packet to the upper (stack) layer: records the
     * reception outcome, attaches the decider result and sends the packet
     * to #upperGateOut_. A D2D-agnostic core operation used, e.g., by the
     * capture-effect decoding in the D2D UE-PHY helper.
     */
    void sendDecodedPacketUp(inet::Packet *pkt, bool receptionSuccessful);

  protected:

    /**
     * Performs initialization operations to prepare gates' IDs and statistics.
     *
     * In the local stage gets gates' IDs, TX power parameters and initializes
     * statistics to be watched.
     * In the Simu5G registrations stage initializes the channel model(s).
     *
     * @param stage initialization stage
     */
    void initialize(int stage) override;

    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    /**
     * Processes messages received from #radioInGate_ or from the stack (#upperGateIn_).
     *
     * @param msg message received from stack or from air channel
     */
    void handleMessage(cMessage *msg) override;

    /**
     * Sends a frame to all NICs in range.
     *
     * Frames are sent with zero transmission delay.
     */
    virtual void sendBroadcast(LteAirFrame *airFrame);

    /**
     * Sends a frame uniquely to the destination specified in carried control info.
     *
     * Delay is calculated based on sender's and receiver's positions.
     */
    virtual void sendUnicast(LteAirFrame *airFrame);

    /**
     * @brief Called when a mobilityStateChanged signal is received.
     *
     * Emits statistics related to the serving cell
     */
    void emitMobilityStats() override {}

  protected:

    /**
     * Sends the given message to the wireless channel.
     *
     * Called by the handleMessage() method
     * when a message from #upperGateIn_ gate is received.
     *
     * The message is encapsulated into an LteAirFrame to which
     * a Signal object containing info about TX power, bit-rate and
     * movement pattern is attached.
     * The LteAirFrame is then sent to the wireless channel.
     *
     * @param msg packet received from LteStack
     */
    virtual void handleUpperMessage(cMessage *msg);

    /// name of the air frame created for an outgoing upper-layer packet
    virtual const char *airFrameNameFor(const UserControlInfo *info);

    /// scheduling priority of the air frame created for an outgoing upper-layer packet
    virtual short airFramePriorityFor(const UserControlInfo *info) { return airFramePriority_; }

    /// stamps additional per-technology fields on the outgoing control info (called after the Tx power)
    virtual void stampExtraTxControlInfo(UserControlInfo *info) {}

    /// hands the prepared air frame to the channel (default: unicast to the destination)
    virtual void transmitFrame(LteAirFrame *frame, const UserControlInfo *info);

    /**
     * Processes messages received from the wireless channel.
     *
     * Called by the handleMessage() method
     * when a message from #radioInGate_ is received.
     *
     * The channel model prepared during the initialization phase determines
     * whether reception is successful, and the frame's inner packet is then
     * sent out to #upperGateOut_ gate along with the result (attached as
     * control info). Concrete behavior is implemented by subclasses.
     *
     * @param msg LteAirFrame received from the air channel
     */
    virtual void handleAirFrame(cMessage *msg) = 0;

    virtual void handleSelfMessage(cMessage *msg) = 0;

    virtual void handleControlMsg(LteAirFrame *frame, UserControlInfo *userInfo);

    virtual void initializeRadio();

    /**
     * Utility.
     * Shows current statistics above the icon.
     */
    virtual void updateDisplayString();

    /**
     * Determine the radio gate index on the receiving node that serves the
     * destination's stack leg.
     */
    virtual int getReceiverGateIndex(const cModule *receiver, MacNodeId dest) const;

  public:
    /*
     * Returns the current position of the node
     */
    const inet::Coord& getCoord() { return getRadioPosition(); }
    /*
     * Returns the time of the last transmission performed
     */
    simtime_t getLastActive() { return lastActive_; }
    /*
     * Returns the MAC Node Id
     */
    MacNodeId getMacNodeId() { return nodeId_; }
    /*
     * Returns whether this PHY is part of the NR stack (as opposed to LTE);
     * needed by RadioMedium to key its per-leg strategies and state by
     * carrier leg, not just carrier frequency.
     */
    bool isNr() const { return isNr_; }
};

} //namespace

#endif /* _PHYBASE_H_ */
