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

#ifndef STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_
#define STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
#include "simu5g/stack/phy/LtePhyUe.h"
#include "simu5g/stack/mac/amc/UserTxParams.h"
#include "simu5g/background/trafficGenerator/generators/TrafficGeneratorBase.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/phy/channelmodel/ID2dChannelModel.h"
#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

/**
 * CRTP mixin that adds D2D support to a core channel model.
 *
 * The core channel models (LteRealisticChannelModel and its NR subclasses) carry
 * no D2D code; this mixin layers the ~800 lines of D2D channel math (attenuation,
 * RSRP/SINR, interference and reception decision) on top of any of them, composing
 * cleanly with the NrChannelModel / NrChannelModel_3GPP38_901 inheritance chain
 * without a diamond. It has native protected access to the Base internals it needs.
 *
 * The concrete Define_Module'd channel models are:
 *   D2dRealisticChannelModel       = D2dChannelModel<LteRealisticChannelModel>
 *   D2dNrChannelModel              = D2dChannelModel<NrChannelModel>
 *   D2dNrChannelModel_3GPP38_901   = D2dChannelModel<NrChannelModel_3GPP38_901>
 * (see D2dChannelModel.cc).
 *
 * The rcvdSinrD2D signal stays registered in LteRealisticChannelModel.cc; the mixin
 * reaches its id through the protected static Base::rcvdSinrD2DSignal_.
 */
template<class Base>
class D2dChannelModel : public Base, public ID2dChannelModel
{
  protected:
    // enable/disable the interference computation for D2D connections
    bool enableD2DInterference_ = false;

    /*
     * Compute attenuation for D2D caused by path loss and shadowing (optional)
     */
    double getAttenuation_D2D(MacNodeId nodeId, Direction dir, inet::Coord coord, MacNodeId node2_Id, inet::Coord coord_2, bool cqiDl);

    /*
     * Compute interference coming from neighboring UEs for the D2D/D2D_MULTI direction
     */
    bool computeD2DInterference(MacNodeId eNbId, MacNodeId senderId, inet::Coord senderCoord, MacNodeId destId, inet::Coord destCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference, Direction dir);

    // Route D2D/D2D_MULTI receptions through getSINR_D2D (called from the core
    // isReceptionSuccessful()).
    std::vector<double> getReceptionSinr(LteAirFrame *frame, UserControlInfo *lteInfo) override;

  public:
    void initialize(int stage) override;

    // ---- ID2dChannelModel ----
    std::vector<double> getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId = NODEID_NONE) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector) override;
    bool isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteI, const std::vector<double>& rsrpVector) override;

    virtual bool isD2DInterferenceEnabled() { return enableD2DInterference_; }
    bool recordsUlTransmissionMap() override { return this->isUplinkInterferenceEnabled() || enableD2DInterference_; }
};

template<class Base>
void D2dChannelModel<Base>::initialize(int stage)
{
    Base::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        enableD2DInterference_ = this->par("d2dInterference");
    }
}

template<class Base>
double D2dChannelModel<Base>::getAttenuation_D2D(MacNodeId nodeId, Direction dir, Coord coord, MacNodeId node2_Id, Coord coord_2, bool cqiDl)
{
    double speed = .0;
    double correlationDist = .0;

    //COMPUTE DISTANCE between UE1 and UE2
    double sqrDistance = coord.distance(coord_2);
    speed = this->computeSpeed(nodeId, coord);
    correlationDist = this->computeCorrelationDistance(nodeId, coord);

    // If Euclidean distance since last LOS probability computation is greater than
    // correlation distance the UE could have changed its state and
    // its visibility from eNodeB, hence it is correct to recompute the LOS probability
    if (correlationDist > this->correlationDistance_
        || this->losMap_.find(nodeId) == this->losMap_.end())
    {
        this->computeLosProbability(sqrDistance, nodeId);
    }

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = this->losMap_[nodeId];
    double dbp = 0;
    double attenuation = this->computePathLoss(sqrDistance, dbp, los);

    //    Applying shadowing only if it is enabled by configuration
    //    log-normal shadowing (not available for background UEs)
    if (num(nodeId) < BGUE_MIN_ID && this->shadowing_)
        attenuation += this->computeShadowing(sqrDistance, nodeId, speed, cqiDl);

    // update current user position
    this->updatePositionHistory(nodeId, coord);

    EV << "LteRealisticChannelModel::getAttenuation - computed attenuation at distance " << sqrDistance << " for UE2 is " << attenuation << endl;

    return attenuation;
}

template<class Base>
std::vector<double> D2dChannelModel<Base>::getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, Coord destCoord)
{
    // AttenuationVector::iterator it;
    // Get Tx power
    double recvPower = lteInfo_1->getD2dTxPower(); // dBm

    // Coordinate of the Sender of the Feedback packet
    Coord sourceCoord = lteInfo_1->getCoord();

    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;
    double noiseFigure = 0.0;
    double speed = 0.0;
    // Get MacId for UE and his peer
    MacNodeId sourceId = lteInfo_1->getSourceId();
    std::vector<double> rsrpVector;

    // True if we use the jakes map in the UE side (D2D is like DL for the receivers)
    bool cqiDl = false;
    // Get the direction
    Direction dir = lteInfo_1->getDirection();
    dir = D2D; //todo[stsc]: dir is overridden? why?

    EV << "------------ GET RSRP D2D----------------" << endl;

    //===================== PARAMETERS SETUP ============================

    // D2D CQI or D2D error computation

    if (dir == UL || dir == DL) {
        //consistency check
        throw cRuntimeError("Direction should neither be UL nor DL");
    }
    else {
        antennaGainTx = antennaGainRx = this->antennaGainUe_;
        //In D2D case the noise figure is the ueNoiseFigure_
        noiseFigure = this->ueNoiseFigure_;
        // use the jakes map in the UE side
        cqiDl = true;
    }
    // Compute speed
    speed = this->computeSpeed(sourceId, sourceCoord);

    EV << "LteRealisticChannelModel::getRSRP_D2D - srcId=" << sourceId
       << " - destId=" << destId
       << " - DIR=" << dirToA(dir)
       << " - frameType=" << ((lteInfo_1->getFrameType() == FEEDBACKPKT) ? "feedback" : "other")
       << endl
       << " - txPwr " << recvPower
       << " - ue1_Coord[" << sourceCoord << "] - ue2_Coord[" << destCoord << "] - ue1_Id[" << sourceId << "] - ue2_Id[" << destId << "]" <<
        endl;
    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS + SHADOWING + FADING =================
    EV << "\t using parameters - noiseFigure=" << noiseFigure << " - antennaGainTx=" << antennaGainTx << " - antennaGainRx=" << antennaGainRx <<
        " - txPwr=" << recvPower << " - for ueId=" << sourceId << endl;

    // attenuation for the desired signal
    double attenuation = getAttenuation_D2D(sourceId, dir, sourceCoord, destId, destCoord, cqiDl); // dB

    //compute attenuation (PATHLOSS + SHADOWING)
    recvPower -= attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm

    //sub cable loss
    recvPower -= this->cableLoss_; // (dBm-dB)=dBm

    // compute and add interference due to fading
    // Apply fading for each band
    // if the phy layer is localized we can assume that for each logical band we have different fading attenuation
    // if the phy layer is distributed the number of logical band should be set to 1
    double fadingAttenuation = 0;
    //for each logical band
    for (unsigned int i = 0; i < this->numBands_; i++) {
        fadingAttenuation = 0;
        //if fading is enabled
        if (this->fading_) {
            //Applying fading
            if (this->fadingType_ == Base::RAYLEIGH)
                fadingAttenuation = this->rayleighFading(sourceId, i);

            else if (this->fadingType_ == Base::JAKES) {
                fadingAttenuation = this->jakesFading(sourceId, speed, i, cqiDl);
            }
        }
        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        EV << " LteRealisticChannelModel::getRSRP_D2D node " << sourceId
           << ((lteInfo_1->getFrameType() == FEEDBACKPKT) ?
            " FEEDBACK PACKET " : " NORMAL PACKET ")
           << " band " << i << " recvPower " << recvPower
           << " direction " << dirToA(dir) << " antenna gain tx "
           << antennaGainTx << " antenna gain rx " << antennaGainRx
           << " noise figure " << noiseFigure
           << " cable loss   " << this->cableLoss_
           << " attenuation (pathloss + shadowing) " << attenuation
           << " speed " << speed << " thermal noise " << this->thermalNoise_
           << " fading attenuation " << fadingAttenuation << endl;

        // Store the calculated receive power
        rsrpVector.push_back(finalRecvPower);
    }
    //============ END PATH LOSS + SHADOWING + FADING ===============

    return rsrpVector;
}

template<class Base>
std::vector<double> D2dChannelModel<Base>::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, Coord destCoord, MacNodeId enbId)
{
    // AttenuationVector::iterator it;
    // Get Tx power
    double recvPower = lteInfo->getD2dTxPower(); // dBm

    // Get allocated RBs
    RbMap rbmap = lteInfo->getGrantedBlocks();

    // Coordinate of the Sender of the Feedback packet
    Coord sourceCoord = lteInfo->getCoord();

    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;
    double noiseFigure = 0.0;
    double speed = 0.0;
    double extCellInterference = 0;
    // Get MacId for UE and his peer
    MacNodeId sourceId = lteInfo->getSourceId();
    std::vector<double> snrVector;
    snrVector.resize(this->numBands_, 0.0);

    // True if we use the jakes map in the UE side (D2D is like DL for the receivers)
    bool cqiDl = true;
    // Get the direction
    Direction dir = D2D;

    EV << "------------ GET SINR D2D ----------------" << endl;

    //===================== PARAMETERS SETUP ============================

    // antenna gain is antennaGainUe for both tx and rx
    antennaGainTx = antennaGainRx = this->antennaGainUe_;
    // In D2D case the noise figure is the ueNoiseFigure_
    noiseFigure = this->ueNoiseFigure_;

    // Compute speed
    speed = this->computeSpeed(sourceId, sourceCoord);

    EV << "LteRealisticChannelModel::getSINR_d2d - srcId=" << sourceId
       << " - destId=" << destId
       << " - DIR=" << dirToA(dir)
       << " - frameType=" << ((lteInfo->getFrameType() == FEEDBACKPKT) ? "feedback" : "other")
       << endl
       << " - txPwr " << recvPower
       << " - ue1_Coord[" << sourceCoord << "] - ue2_Coord[" << destCoord << "] - ue1_Id[" << sourceId << "] - ue2_Id[" << destId << "]" <<
        endl;
    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS + SHADOWING + FADING =================
    EV << "\t using parameters - noiseFigure=" << noiseFigure << " - antennaGainTx=" << antennaGainTx << " - antennaGainRx=" << antennaGainRx <<
        " - txPwr=" << recvPower << " - for ueId=" << sourceId << endl;

    // attenuation for the desired signal
    double attenuation = getAttenuation_D2D(sourceId, dir, sourceCoord, destId, destCoord, cqiDl); // dB

    //compute attenuation (PATHLOSS + SHADOWING)
    recvPower -= attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm

    //sub cable loss
    recvPower -= this->cableLoss_; // (dBm-dB)=dBm

    // compute and add interference due to fading
    // Apply fading for each band
    // if the phy layer is localized we can assume that for each logical band we have different fading attenuation
    // if the phy layer is distributed the number of logical band should be set to 1
    double fadingAttenuation = 0;
    //for each logical band
    for (unsigned int i = 0; i < this->numBands_; i++) {
        fadingAttenuation = 0;
        //if fading is enabled
        if (this->fading_) {
            //Applying fading
            if (this->fadingType_ == Base::RAYLEIGH)
                fadingAttenuation = this->rayleighFading(sourceId, i);

            else if (this->fadingType_ == Base::JAKES) {
                fadingAttenuation = this->jakesFading(sourceId, speed, i, cqiDl);
            }
        }
        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        EV << " LteRealisticChannelModel::getSINR_d2d node " << sourceId
           << ((lteInfo->getFrameType() == FEEDBACKPKT) ?
            " FEEDBACK PACKET " : " NORMAL PACKET ")
           << " band " << i << " recvPower " << recvPower
           << " direction " << dirToA(dir) << " antenna gain tx "
           << antennaGainTx << " antenna gain rx " << antennaGainRx
           << " noise figure " << noiseFigure
           << " cable loss   " << this->cableLoss_
           << " attenuation (pathloss + shadowing) " << attenuation
           << " speed " << speed << " thermal noise " << this->thermalNoise_
           << " fading attenuation " << fadingAttenuation << endl;

        // Store the calculated receive power
        snrVector[i] = finalRecvPower;
    }
    //============ END PATH LOSS + SHADOWING + FADING ===============

    /*
     * The SINR will be calculated as follows
     *
     *              Pwr
     * SINR = ---------
     *           N  +  I
     *
     * N = thermalNoise_ + noiseFigure (measured in dBm)
     * I = extCellInterference + inCellInterference (measured in mW)
     */
    //============ D2D INTERFERENCE COMPUTATION =================
    /*
     * In calculating a D2D CQI the interference from other UEs discriminates between calculating a CQI
     * following direction D2D_Tx--->D2D_Rx or D2D_Tx<---D2D_Rx (This happens due to the different positions of the
     * interfering UEs relative to the position of the UE for whom we are calculating the CQI). We need that the CQI
     * for the D2D_Tx is the same as the D2D_Rx (This is a help for the simulator because when the eNodeB allocates
     * resources to a D2D_Tx it must refer to the quality channel of the D2D_Rx).
     * To do so here we must check if the ueId is the ID of the D2D_Tx: if it
     * is so we swap the ueId with the one of his Peer (D2D_Rx). We do the same for the coord.
     */
    //vector containing the sum of in-cell interference for each band
    std::vector<double> d2dInterference; // Linear value (mW)
    // prepare data structure
    d2dInterference.resize(this->numBands_, 0);
    if (enableD2DInterference_) {
        computeD2DInterference(enbId, sourceId, sourceCoord, destId, destCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &d2dInterference, dir);
    }

    //===================== SINR COMPUTATION ========================
    if (enableD2DInterference_) {
        // compute and linearize total noise
        double totN = dBmToLinear(this->thermalNoise_ + noiseFigure);

        // denominator expressed in dBm as (N+extCell+inCell)
        double den;
        EV << "LteRealisticChannelModel::getSINR - distance from my Peer = " << destCoord.distance(sourceCoord) << " - DIR=" << dirToA(dir) << endl;

        // Add interference for each band
        for (unsigned int i = 0; i < this->numBands_; i++) {
            // if we are decoding a data transmission and this RB has not been used, skip it
            // TODO fix for multi-antenna case
            if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
                continue;

            //               (      mW            +  mW  +        mW            )
            den = linearToDBm(extCellInterference + totN + d2dInterference[i]);

            EV << "\t ext[" << extCellInterference << "] - in[" << d2dInterference[i] << "] - recvPwr["
               << dBmToLinear(snrVector[i]) << "] - sinr[" << snrVector[i] - den << "]\n";

            // compute final SINR. Subtraction in dB is equivalent to linear division
            snrVector[i] -= den;
        }
    }
    // compute snr with no D2D interference
    else {
        for (unsigned int i = 0; i < this->numBands_; i++) {
            // if we are decoding a data transmission and this RB has not been used, skip it
            // TODO fix for multi-antenna case
            if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
                continue;

            /*
               std::cout<<"SNR "<<i<<" "<<snrVector[i]<<endl;
               std::cout<<"noise figure "<<i<<" "<<noiseFigure<<endl;
               std::cout<<"Thermal noise "<<i<<" "<<thermalNoise_<<endl;
             */
            // compute final SINR
            snrVector[i] -= (noiseFigure + this->thermalNoise_);

            EV << "LteRealisticChannelModel::getSINR_d2d - distance from my Peer = " << destCoord.distance(sourceCoord) << " - DIR=" << dirToA(dir) << " - snr[" << snrVector[i] << "]\n";
        }
    }
    //sender is a UE
    this->updatePositionHistory(sourceId, sourceCoord);

    return snrVector;
}

template<class Base>
std::vector<double> D2dChannelModel<Base>::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector)
{
    std::vector<double> snrVector = rsrpVector;

    MacNodeId sourceId = lteInfo_1->getSourceId();
    Coord sourceCoord = lteInfo_1->getCoord();

    // Get allocated RBs
    RbMap rbmap = lteInfo_1->getGrantedBlocks();

    // Get the direction
    Direction dir = D2D;

    double noiseFigure = 0.0;
    double extCellInterference = 0.0;

    // In D2D case the noise figure is the ueNoiseFigure_
    noiseFigure = this->ueNoiseFigure_;

    EV << "------------ GET SINR D2D----------------" << endl;

    /*
     * The SINR will be calculated as follows
     *
     *              Pwr
     * SINR = ---------
     *           N  +  I
     *
     * N = thermalNoise_ + noiseFigure (measured in dBm)
     * I = extCellInterference + inCellInterference (measured in mW)
     */
    //============ IN CELL D2D INTERFERENCE COMPUTATION =================
    /*
     * In calculating a D2D CQI, the interference from other D2D UEs discriminates between calculating a CQI
     * in the direction D2D_Tx--->D2D_Rx or D2D_Tx<---D2D_Rx (This happens due to the different positions of the
     * interfering UEs relative to the position of the UE for whom we are calculating the CQI). We need that the CQI
     * for the D2D_Tx is the same as the D2D_Rx (This is a help for the simulator because when the eNodeB allocates
     * resources to a D2D_Tx it must refer to the quality channel of the D2D_Rx).
     * To do so, here we must check if the ueId is the ID of the D2D_Tx: if it
     * is so we swap the ueId with the one of his Peer (D2D_Rx). We do the same for the coord.
     */
    // vector containing the sum of inCell interference for each band
    std::vector<double> d2dInterference; // Linear value (mW)
    // prepare data structure
    d2dInterference.resize(this->numBands_, 0);
    if (enableD2DInterference_) {
        computeD2DInterference(enbId, sourceId, sourceCoord, destId, destCoord, (lteInfo_1->getFrameType() == FEEDBACKPKT), lteInfo_1->getCarrierFrequency(), rbmap, &d2dInterference, dir);
    }

    //===================== SINR COMPUTATION ========================
    if (enableD2DInterference_) {
        // compute and linearize total noise
        double totN = dBmToLinear(this->thermalNoise_ + noiseFigure);

        // denominator expressed in dBm as (N+extCell+inCell)
        double den;
        EV << "LteRealisticChannelModel::getSINR - distance from my Peer = " << destCoord.distance(sourceCoord) << " - DIR=" << dirToA(dir) << endl;

        // Add interference for each band
        for (unsigned int i = 0; i < this->numBands_; i++) {
            // if we are decoding a data transmission and this RB has not been used, skip it
            // TODO fix for multi-antenna case
            if (lteInfo_1->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
                continue;

            //               (      mW            +  mW  +        mW            )
            den = linearToDBm(extCellInterference + totN + d2dInterference[i]);

            EV << "\t ext[" << extCellInterference << "] - in[" << d2dInterference[i] << "] - recvPwr["
               << dBmToLinear(snrVector[i]) << "] - sinr[" << snrVector[i] - den << "]\n";

            // compute final SINR. Subtraction in dB is equivalent to linear division
            snrVector[i] -= den;
        }
    }
    // compute snr with no D2D interference
    else {
        for (unsigned int i = 0; i < this->numBands_; i++) {
            // if we are decoding a data transmission and this RB has not been used, skip it
            // TODO fix for multi-antenna case
            if (lteInfo_1->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
                continue;

            // compute final SINR
            snrVector[i] -= (noiseFigure + this->thermalNoise_);

            EV << "LteRealisticChannelModel::getSINR_D2D - distance from my Peer = " << destCoord.distance(sourceCoord) << " - DIR=" << dirToA(dir) << " - snr[" << snrVector[i] << "]\n";
        }
    }

    // sender is a UE
    this->updatePositionHistory(sourceId, sourceCoord);

    return snrVector;
}

template<class Base>
std::vector<double> D2dChannelModel<Base>::getReceptionSinr(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    if (lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI) {
        MacNodeId destId = lteInfo->getDestId();
        Coord destCoord = this->phy_->getCoord();
        MacNodeId enbId = this->binder_->getServingNodeOrSelf(lteInfo->getSourceId());
        return getSINR_D2D(frame, lteInfo, destId, destCoord, enbId);
    }
    return Base::getReceptionSinr(frame, lteInfo);
}

template<class Base>
bool D2dChannelModel<Base>::isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector)
{
    EV << "LteRealisticChannelModel::error_D2D" << endl;

    // get codeword
    unsigned char cw = lteInfo->getCw();
    // get number of codewords
    int size = lteInfo->getUserTxParams()->readCqiVector().size();

    // get position associated with the packet
    // Coord coord = lteInfo->getCoord();

    // if total number of codewords is equal to 1 the cw index should be only 0
    if (size == 1)
        cw = 0;

    // Get CQI used to transmit this cw
    Cqi cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
    EV << "LteRealisticChannelModel:: CQI: " << cqi << endl;

    MacNodeId id;
    Direction dir = lteInfo->getDirection();

    // Get MacNodeId of UE
    if (dir == DL)
        id = lteInfo->getDestId();
    else // UL or D2D
        id = lteInfo->getSourceId();

    EV << NOW << "LteRealisticChannelModel::FROM: " << id << endl;
    // Get Number of RTX
    unsigned char nTx = lteInfo->getTxNumber();

    // consistency check
    if (nTx == 0)
        throw cRuntimeError("Transmissions counter should not be 0");

    // Get txmode
    TxMode txmode = (TxMode)lteInfo->getTxMode();

    // SINR vector(one SINR value for each band)
    std::vector<double> snrV;
    if (lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI) {
        MacNodeId peerUeMacNodeId = lteInfo->getDestId();
        Coord peerCoord = this->phy_->getCoord();
        MacNodeId enbId = MacNodeId(1); // TODO get an appropriate way to get EnbId

        if (lteInfo->getDirection() == D2D) {
            snrV = getSINR_D2D(frame, lteInfo, peerUeMacNodeId, peerCoord, enbId);
        }
        else { // D2D_MULTI
            snrV = getSINR_D2D(frame, lteInfo, peerUeMacNodeId, peerCoord, enbId, rsrpVector);
        }
    }
    // ROSSALI-------END------------------------------------------------
    else snrV = this->getSINR(frame, lteInfo);                                           // Take SINR

    // Get the resource Block id used to transmit this packet
    RbMap rbmap = lteInfo->getGrantedBlocks();

    // Get txmode
    unsigned int itxmode = txModeToIndex[txmode];

    double bler = 0;
    std::vector<double> totalbler;
    double finalSuccess = 1;

    // for statistical purposes
    double sumSnr = 0.0;
    int usedRBs = 0;

    // for each Remote unit used to transmit the packet
    for (const auto& [remoteUnitId, resourceBlocks] : rbmap) {
        // for each logical band used to transmit the packet
        for (const auto& [band, allocation] : resourceBlocks) {
            // this Rb is not allocated
            if (allocation == 0) continue;

            // Get the Bler
            if (cqi == 0)
                return false; // CQI 0 means channel below usable quality (e.g. after handover) — loss
            if (cqi > 15)
                throw cRuntimeError("A packet has been transmitted with a cqi greater than 15 cqi:%d txmode:%d dir:%d rb:%d cw:%d rtx:%d", cqi, lteInfo->getTxMode(), dir, band, cw, nTx);

            // for statistical purposes
            sumSnr += snrV[band];
            usedRBs++;

            int snr = snrV[band];// XXX because band is a Band (=unsigned short)
            if (snr < 1)                           // XXX it was < 0
                return false;
            else if (snr > this->binder_->phyPisaData.maxSnr())
                bler = 0;
            else
                bler = this->binder_->phyPisaData.getBler(itxmode, cqi, snr);

            EV << "\t bler computation: [itxMode=" << itxmode << "] - [cqi=" << cqi
               << "] - [snr=" << snr << "]" << endl;

            double success = 1 - bler;
            // compute the success probability according to the number of RB used
            double successPacket = pow(success, (double)allocation);

            // compute the success probability according to the number of LB used
            finalSuccess *= successPacket;

            EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
               << " node " << id << " remote unit " << dasToA(remoteUnitId)
               << " Band " << band << " SNR " << snr << " CQI " << cqi
               << " BLER " << bler << " success probability " << successPacket
               << " total success probability " << finalSuccess << endl;
        }
    }
    // Compute total error probability
    double per = 1 - finalSuccess;
    // Harq Reduction
    double totalPer = per * pow(this->harqReduction_, nTx - 1);

    double er = this->uniform(0.0, 1.0);

    EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
       << " node " << id << " total ERROR probability  " << per
       << " per with H-ARQ error reduction " << totalPer
       << " - CQI[" << cqi << "]- random error extracted[" << er << "]" << endl;

    // emit SINR statistic
    if (this->collectSinrStatistics_ && usedRBs > 0)
        this->emit(Base::rcvdSinrD2DSignal_, sumSnr / usedRBs);

    if (er <= totalPer) {
        EV << "This is NOT your lucky day (" << er << " < " << totalPer << ") -> do not receive." << endl;

        // Signal too weak, we can't receive it
        return false;
    }
    // Signal is strong enough, receive this Signal
    EV << "This is your lucky day (" << er << " > " << totalPer << ") -> Receive AirFrame." << endl;

    return true;
}

template<class Base>
bool D2dChannelModel<Base>::computeD2DInterference(MacNodeId eNbId, MacNodeId senderId, Coord senderCoord, MacNodeId destId, Coord destCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
        std::vector<double> *interference, Direction dir)
{
    EV << "**** D2D Interference for cellId[" << eNbId << "] node[" << destId << "] ****" << endl;

    // get the D2D view of the eNodeB's MAC
    ID2dMacEnb *macEnb = check_and_cast<ID2dMacEnb *>(this->binder_->getMacFromMacNodeId(eNbId));

    const std::vector<std::vector<UeAllocationInfo>> *ulTransmissionMap;
    const std::vector<UeAllocationInfo> *allocatedUes;

    if (isCqi) {// check slot occupation for this TTI
        ulTransmissionMap = this->binder_->getUlTransmissionMap(carrierFrequency, CURR_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            for (unsigned int i = 0; i < this->numBands_; i++) {
                // get the UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    MacNodeId ueId = ue_it.nodeId;
                    MacCellId cellId = ue_it.cellId;
                    Direction dir = ue_it.dir;
                    double txPwr;
                    inet::Coord ueCoord;
                    LtePhyUe *uePhy = nullptr;
                    TrafficGeneratorBase *trafficGen = nullptr;
                    if (ue_it.phy != nullptr) {
                        uePhy = check_and_cast<LtePhyUe *>(ue_it.phy);
                        txPwr = uePhy->getTxPwr(dir);
                        ueCoord = uePhy->getCoord();
                    }
                    else { // this is a backgroundUe
                        trafficGen = check_and_cast<TrafficGeneratorBase *>(ue_it.trafficGen);
                        txPwr = trafficGen->getTxPwr();
                        ueCoord = trafficGen->getCoord();
                    }

                    // no self-interference
                    if (ueId == senderId || ueId == destId)
                        continue;

                    // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                    if (dir == UL && cellId == eNbId)
                        continue;

                    // no interference from D2D connections of the same cell when reuse is disabled (otherwise, computation of CQI is misleading)
                    if (cellId == eNbId && (!macEnb->isReuseD2DEnabled() && !macEnb->isReuseD2DMultiEnabled()))
                        continue;

                    EV << NOW << " LteRealisticChannelModel::computeD2DInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get tx power and attenuation from this UE
                    double rxPwr = txPwr - this->cableLoss_ + 2 * this->antennaGainUe_;
                    double att = getAttenuation_D2D(ueId, D2D, ueCoord, destId, destCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }
    else { // Error computation. We need to check the slot occupation of the previous TTI
        ulTransmissionMap = this->binder_->getUlTransmissionMap(carrierFrequency, PREV_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            // For each band we have to check if the Band in the previous TTI was occupied by the interferingId
            for (unsigned int i = 0; i < this->numBands_; i++) {
                // get the UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    MacNodeId ueId = ue_it.nodeId;
                    MacCellId cellId = ue_it.cellId;
                    Direction dir = ue_it.dir;
                    double txPwr;
                    inet::Coord ueCoord;
                    LtePhyUe *uePhy = nullptr;
                    TrafficGeneratorBase *trafficGen = nullptr;
                    if (ue_it.phy != nullptr) {
                        uePhy = check_and_cast<LtePhyUe *>(ue_it.phy);
                        txPwr = uePhy->getTxPwr(dir);
                        ueCoord = uePhy->getCoord();
                    }
                    else { // this is a backgroundUe
                        trafficGen = check_and_cast<TrafficGeneratorBase *>(ue_it.trafficGen);
                        txPwr = trafficGen->getTxPwr();
                        ueCoord = trafficGen->getCoord();
                    }

                    // no self-interference
                    if (ueId == senderId || ueId == destId)
                        continue;

                    // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                    if (dir == UL && cellId == eNbId)
                        continue;

                    // no interference from D2D connections of the same cell when reuse is disabled
                    if (cellId == eNbId && (!macEnb->isReuseD2DEnabled() && !macEnb->isReuseD2DMultiEnabled()))
                        continue;

                    EV << NOW << " LteRealisticChannelModel::computeD2DInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get tx power and attenuation from this UE
                    double rxPwr = txPwr - this->cableLoss_ + 2 * this->antennaGainUe_;
                    double att = getAttenuation_D2D(ueId, D2D, ueCoord, destId, destCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }

    // Debug Output
    EV << NOW << " LteRealisticChannelModel::computeD2DInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < this->numBands_; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

} //namespace

#endif /* STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_ */
