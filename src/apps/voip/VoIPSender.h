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

#ifndef _VOIPSENDER_H_
#define _VOIPSENDER_H_

#include <string.h>
#include <omnetpp.h>

#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include "apps/voip/VoipPacket_m.h"

namespace simu5g {

class VoIPSender : public omnetpp::cSimpleModule
{
    inet::UdpSocket socket;

    //source
    omnetpp::simtime_t durTalk_ = 0;
    omnetpp::simtime_t durSil_ = 0;
    double scaleTalk_ = 0.0;
    double shapeTalk_ = 0.0;
    double scaleSil_ = 0.0;
    double shapeSil_ = 0.0;
    bool isTalk_ = false;
    omnetpp::cMessage *selfSource_ = nullptr;
    //sender
    int iDtalk_ = 0;
    int nframes_ = 0;
    int iDframe_ = 0;
    int nframesTmp_ = 0;
    int size_ = 0;
    omnetpp::simtime_t sampling_time;

    bool silences_ = false;

    unsigned int totalSentBytes_ = 0;
    omnetpp::simtime_t warmUpPer_;

    omnetpp::simsignal_t voIPGeneratedThroughtput_;
    // ----------------------------

    omnetpp::cMessage *selfSender_ = nullptr;

    omnetpp::cMessage *initTraffic_ = nullptr;

    omnetpp::simtime_t timestamp_;
    int localPort_ = 0;
    int destPort_ = 0;
    inet::L3Address destAddress_;

    void initTraffic();
    void talkspurt(omnetpp::simtime_t dur);
    void selectPeriodTime();
    void sendVoIPPacket();

  public:
    ~VoIPSender();
    VoIPSender();

  protected:

    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

};

} //namespace

#endif

