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

#ifndef _LTE_VOIPRECEIVER_H_
#define _LTE_VOIPRECEIVER_H_

#include <list>
#include <string.h>

#include <omnetpp.h>

#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>

#include "apps/voip/VoipPacket_m.h"

namespace simu5g {

class VoIPReceiver : public omnetpp::cSimpleModule
{
    inet::UdpSocket socket;

    ~VoIPReceiver();

    int emodel_Ie_ = 0;
    int emodel_Bpl_ = 0;
    int emodel_A_ = 0;
    double emodel_Ro_ = 0.0;

    typedef std::list<VoipPacket *> PacketsList;
    PacketsList mPacketsList_;
    PacketsList mPlayoutQueue_;
    unsigned int mCurrentTalkspurt_ = 0;
    unsigned int mBufferSpace_ = 0;
    omnetpp::simtime_t mSamplingDelta_;
    omnetpp::simtime_t mPlayoutDelay_;

    bool mInit_ = false;

    unsigned int totalRcvdBytes_ = 0;
    omnetpp::simtime_t warmUpPer_;

    omnetpp::simsignal_t voIPFrameLossSignal_;
    omnetpp::simsignal_t voIPFrameDelaySignal_;
    omnetpp::simsignal_t voIPPlayoutDelaySignal_;
    omnetpp::simsignal_t voIPMosSignal_;
    omnetpp::simsignal_t voIPTaildropLossSignal_;
    omnetpp::simsignal_t voIPPlayoutLossSignal_;
    omnetpp::simsignal_t voIPJitterSignal_;
    omnetpp::simsignal_t voIPReceivedThroughput_;

    virtual void finish() override;

  protected:

    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    double eModel(omnetpp::simtime_t delay, double loss);
    void playout(bool finish);
};

} //namespace

#endif

