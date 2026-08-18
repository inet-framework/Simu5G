//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/phy/channelmodel/CellularInterferenceModel.h"

#include <algorithm>

#include "simu5g/background/cell/BackgroundScheduler.h"
#include "simu5g/background/trafficGenerator/generators/TrafficGeneratorBase.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/nodes/ExtCell.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/phy/PhyBase.h"
#include "simu5g/stack/phy/channelmodel/RadioMedium.h"
#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"

namespace simu5g {

Define_Module(CellularInterferenceModel);

void CellularInterferenceModel::initialize()
{
    // this module's own parent, the S2 submodule slot; purely structural, so
    // resolvable regardless of init-stage ordering
    medium_ = check_and_cast<RadioMedium *>(getParentModule());
    binder_.reference(this, "binderModule", true);
}

void CellularInterferenceModel::handleMessage(cMessage *msg)
{
    throw cRuntimeError("unexpected message '%s': CellularInterferenceModel has no gates and schedules no self-messages", msg->getName());
}

bool CellularInterferenceModel::computeDownlinkInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId ueId,
        inet::Coord coord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference)
{
    EV << "**** Downlink Interference ****" << endl;

    const auto& enbList = binder_->getEnbList();
    for (auto& enbInfo : enbList) {
        MacNodeId id = enbInfo->id;

        if (id == eNbId)
            continue;

        // initialize eNB data structures
        if (!enbInfo->init) {
            // obtain a reference to eNB phy and obtain tx power
            enbInfo->phy = check_and_cast<PhyBase *>(binder_->getPhyByNodeId(id));

            // txPwr/txDirection/txAngle below are never read again in this file
            // (this method uses medium_->txPowerOf/txDirectionOf/txAngleOf instead),
            // but do not remove them: BackgroundCellChannelModel.cc reads
            // enb->txPwr/txDirection/txAngle under this same shared enbInfo->init
            // flag, so whichever module runs first fills them for the other. Keep
            // both writers until the fork in BackgroundCellChannelModel is deleted.
            enbInfo->txPwr = enbInfo->phy->getTxPwr();//dBm

            // get tx direction
            enbInfo->txDirection = enbInfo->phy->getTxDirection();

            // get tx angle
            enbInfo->txAngle = enbInfo->phy->getTxAngle();

            //get reference to mac layer
            enbInfo->mac = check_and_cast<LteMacEnb *>(binder_->getMacByNodeId(id));

            enbInfo->init = true;
        }

        StochasticChannelModel *interfChanModel = dynamic_cast<StochasticChannelModel *>(enbInfo->phy->getChannelModel(carrierFrequency));

        // if the eNB does not use the selected carrier frequency, skip it
        if (interfChanModel == nullptr)
            continue;

        // compute attenuation using data structures within the cell
        double att = interfChanModel->getAttenuation(ueId, UL, coord, isCqi);
        EV << "EnbId [" << id << "] - attenuation [" << att << "]";

        //=============== ANGULAR ATTENUATION =================
        double angularAtt = 0;
        if (medium_->txDirectionOf(id, carrierFrequency) == ANISOTROPIC) {
            //get tx angle
            double txAngle = medium_->txAngleOf(id, carrierFrequency);

            // compute the angle between uePosition and reference axis, considering the eNB as center
            double ueAngle = radio->computeAngle(medium_->coordOf(id, carrierFrequency), coord);

            // compute the reception angle between ue and eNB
            double recvAngle = fabs(txAngle - ueAngle);
            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = radio->computeVerticalAngle(medium_->coordOf(id, carrierFrequency), coord);

            // compute attenuation due to sectorial tx
            angularAtt = radio->computeAngularAttenuation(recvAngle, verticalAngle);

            EV << "angular attenuation [" << angularAtt << "]";
        }
        // else, antenna is omni-directional
        //=============== END ANGULAR ATTENUATION =================

        double txPwr = medium_->txPowerOf(id, carrierFrequency) - angularAtt - radio->getCableLoss() + radio->getAntennaGainEnB() + radio->getAntennaGainUe();

        unsigned int numBands = std::min(radio->getNumBands(), interfChanModel->getNumBands());
        EV << " - shared bands [" << numBands << "]" << endl;

        if (isCqi) {// check slot occupation for this TTI
            for (unsigned int i = 0; i < numBands; i++) {
                // compute the number of occupied slot (unnecessary)
                int temp = enbInfo->mac->getDlBandStatus(i);
                if (temp != 0)
                    (*interference)[i] += dBmToLinear(txPwr - att); //(dBm-dB)=dBm

                EV << "\t band " << i << " occupied " << temp << "/pwr[" << txPwr << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
        else { // error computation. We need to check the slot occupation of the previous TTI
            for (unsigned int i = 0; i < numBands; i++) {
                // if we are decoding a data transmission and this RB has not been used, skip it
                // TODO fix for multi-antenna case
                if (!rbmap.empty() && rbmap.at(MACRO).at(i) == 0)
                    continue;

                // compute the number of occupied slot (unnecessary)
                int temp = enbInfo->mac->getDlPrevBandStatus(i);
                if (temp != 0)
                    (*interference)[i] += dBmToLinear(txPwr - att); //(dBm-dB)=dBm

                EV << "\t band " << i << " occupied " << temp << "/pwr[" << txPwr << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
    }

    return true;
}

bool CellularInterferenceModel::computeUplinkInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId senderId,
        bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference)
{
    EV << "**** Uplink Interference for cellId[" << eNbId << "] node[" << senderId << "] ****" << endl;

    const std::vector<std::vector<UeAllocationInfo>> *ulTransmissionMap;
    const std::vector<UeAllocationInfo> *allocatedUes;

    unsigned int numBands = radio->getNumBands();

    if (isCqi) {// check slot occupation for this TTI
        ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, CURR_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            for (unsigned int i = 0; i < numBands; i++) {
                // get the set of UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    const auto interferer = StochasticChannelModel::describeInterferer(ue_it, medium_, carrierFrequency);
                    const MacNodeId ueId = interferer.nodeId;
                    const MacCellId cellId = interferer.cellId;
                    const Direction dir = interferer.dir;
                    const double txPwr = interferer.txPwr;
                    const inet::Coord ueCoord = interferer.coord;

                    // no self-interference
                    if (ueId == senderId)
                        continue;

                    // no interference from UL/D2D connections of the same cell  (no D2D-UL reuse allowed)
                    if (cellId == eNbId)
                        continue;

                    EV << NOW << " CellularInterferenceModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get rx power and attenuation from this UE
                    double rxPwr = txPwr - radio->getCableLoss() + radio->getAntennaGainUe() + radio->getAntennaGainEnB();
                    double att = radio->getAttenuation(ueId, UL, ueCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }
    else { // Error computation. We need to check the slot occupation of the previous TTI
        ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, PREV_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            // For each band we have to check if the Band in the previous TTI was occupied by the interferingId
            for (unsigned int i = 0; i < numBands; i++) {
                // if we are decoding a data transmission and this RB has not been used, skip it
                // TODO fix for multi-antenna case
                if (!rbmap.empty() && rbmap.at(MACRO).at(i) == 0)
                    continue;

                // get the set of UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    const auto interferer = StochasticChannelModel::describeInterferer(ue_it, medium_, carrierFrequency);
                    const MacNodeId ueId = interferer.nodeId;
                    const MacCellId cellId = interferer.cellId;
                    const Direction dir = interferer.dir;
                    const double txPwr = interferer.txPwr;
                    const inet::Coord ueCoord = interferer.coord;

                    // no self-interference
                    if (ueId == senderId)
                        continue;

                    // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                    if (cellId == eNbId)
                        continue;

                    EV << NOW << " CellularInterferenceModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get tx power and attenuation from this UE
                    double rxPwr = txPwr - radio->getCableLoss() + radio->getAntennaGainUe() + radio->getAntennaGainEnB();
                    double att = radio->getAttenuation(ueId, UL, ueCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }

    // Debug Output
    EV << NOW << " CellularInterferenceModel::computeUplinkInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < numBands; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

bool CellularInterferenceModel::computeExtCellInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId nodeId,
        inet::Coord coord, bool isCqi, GHz carrierFrequency, std::vector<double> *interference)
{
    EV << "**** Ext Cell Interference **** " << endl;

    // get external cell list
    ExtCellList list = binder_->getExtCellList(carrierFrequency);

    double dist, // meters
           recvPwr, // watt
           recvPwrDBm, // dBm
           att, // dBm
           angularAtt; // dBm

    //compute distance for each cell
    for (auto& extCell : list) {
        // get external cell position
        inet::Coord c = extCell->getPosition();
        // compute distance between UE and the ext cell
        dist = coord.distance(c);

        EV << "\t distance between UE[" << coord.x << "," << coord.y <<
            "] and extCell[" << c.x << "," << c.y << "] is -> "
           << dist << "\t";

        // compute attenuation according to some path loss model
        att = radio->computeExtCellPathLoss(dist, LinkKey(nodeId));

        //=============== ANGULAR ATTENUATION =================
        if (extCell->getTxDirection() == OMNI) {
            angularAtt = 0;
        }
        else {
            // compute the angle between uePosition and reference axis, considering the eNb as center
            double ueAngle = radio->computeAngle(c, coord);

            // compute the reception angle between ue and eNb
            double recvAngle = fabs(extCell->getTxAngle() - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = radio->computeVerticalAngle(c, coord);

            // compute attenuation due to sectorial tx
            angularAtt = radio->computeAngularAttenuation(recvAngle, verticalAngle);
        }
        //=============== END ANGULAR ATTENUATION =================

        // TODO do we need to use (- cableLoss_ + antennaGainEnB_) in ext cells too?
        // compute and linearize received power
        recvPwrDBm = extCell->getTxPower() - att - angularAtt - radio->getCableLoss() + radio->getAntennaGainEnB() + radio->getAntennaGainUe();
        recvPwr = dBmToLinear(recvPwrDBm);

        unsigned int numBands = std::min(radio->getNumBands(), extCell->getNumBands());
        EV << " - shared bands [" << numBands << "]" << endl;

        // add interference in those bands where the ext cell is active
        for (unsigned int i = 0; i < numBands; i++) {
            int occ;
            if (isCqi) { // check slot occupation for this TTI
                occ = extCell->getBandStatus(i);
            }
            else {      // error computation. We need to check the slot occupation of the previous TTI
                occ = extCell->getPrevBandStatus(i);
            }

            // if the ext cell is active, add interference
            if (occ) {
                (*interference)[i] += recvPwr;
            }
        }
    }

    return true;
}

bool CellularInterferenceModel::computeBackgroundCellInterference(StochasticChannelModel *radio, MacNodeId nodeId,
        inet::Coord bsCoord, inet::Coord ueCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
        Direction dir, std::vector<double> *interference)
{
    EV << "**** Background Cell Interference **** " << endl;

    // get bg schedulers list
    const auto& list = binder_->getBackgroundSchedulerList(carrierFrequency);

    inet::Coord c;
    double dist, // meters
           txPwr, // dBm
           recvPwr, // watt
           recvPwrDBm, // dBm
           att, // dBm
           angularAtt; // dBm

    //compute distance for each cell
    for (auto& bgScheduler : list) {
        if (dir == DL) {
            // compute interference with respect to the background base station

            // get external cell position
            c = bgScheduler->getPosition();
            // compute distance between UE and the ext cell
            dist = ueCoord.distance(c);

            EV << "\t distance between UE[" << ueCoord.x << "," << ueCoord.y <<
                "] and backgroundCell[" << c.x << "," << c.y << "] is -> "
               << dist << "\t";

            // compute attenuation according to some path loss model
            att = radio->computeExtCellPathLoss(dist, LinkKey(nodeId));

            txPwr = bgScheduler->getTxPower();

            //=============== ANGULAR ATTENUATION =================
            if (bgScheduler->getTxDirection() == OMNI) {
                angularAtt = 0;
            }
            else {
                // compute the angle between uePosition and reference axis, considering the eNB as center
                double ueAngle = radio->computeAngle(c, ueCoord);

                // compute the reception angle between ue and eNB
                double recvAngle = fabs(bgScheduler->getTxAngle() - ueAngle);

                if (recvAngle > 180)
                    recvAngle = 360 - recvAngle;

                double verticalAngle = radio->computeVerticalAngle(c, ueCoord);

                // compute attenuation due to sectorial tx
                angularAtt = radio->computeAngularAttenuation(recvAngle, verticalAngle);
            }
            //=============== END ANGULAR ATTENUATION =================

            // TODO do we need to use (- cableLoss_ + antennaGainEnB_) in ext cells too?
            // compute and linearize received power
            recvPwrDBm = txPwr - att - angularAtt - radio->getCableLoss() + radio->getAntennaGainEnB() + radio->getAntennaGainUe();
            recvPwr = dBmToLinear(recvPwrDBm);
            EV << " recvPwr[" << recvPwr << "]\t";

            unsigned int numBands = std::min(radio->getNumBands(), bgScheduler->getNumBands());
            EV << " - shared bands [" << numBands << "]\t";
            EV << " - interfering bands[";

            // add interference in those bands where the ext cell is active
            for (unsigned int i = 0; i < numBands; i++) {
                int occ = 0;
                if (isCqi) { // check slot occupation for this TTI
                    occ = bgScheduler->getBandStatus(i, DL);
                }
                else if (!rbmap.empty() && rbmap.at(MACRO).at(i) != 0) {     // error computation. We need to check the slot occupation of the previous TTI (only if the band has been used by the UE)
                    occ = bgScheduler->getPrevBandStatus(i, DL);
                }

                // if the ext cell is active, add interference
                if (occ > 0) {
                    EV << i << ",";
                    (*interference)[i] += recvPwr;
                }
            }
            EV << "]" << endl;
        }
        else { // dir == UL
            // for each RB occupied in the background cell, compute interference with respect to the
            // background UE that is using that RB
            TrafficGeneratorBase *bgUe;

            double antennaGainBgUe = radio->getAntennaGainUe();  // TODO get this from the bgUe

            angularAtt = 0;  // we assume OMNI directional UEs

            unsigned int numBands = std::min(radio->getNumBands(), bgScheduler->getNumBands());
            EV << " - shared bands [" << numBands << "]" << endl;

            // add interference in those bands where a UE in the background cell is active
            for (unsigned int i = 0; i < numBands; i++) {
                int occ = 0;

                if (isCqi) { // check slot occupation for this TTI
                    occ = bgScheduler->getBandStatus(i, UL);
                    if (occ)
                        bgUe = bgScheduler->getBandInterferingUe(i);
                }
                else if (rbmap.at(MACRO).at(i) != 0) {     // error computation. We need to check the slot occupation of the previous TTI (only if the band has been used by the UE)
                    occ = bgScheduler->getPrevBandStatus(i, UL);
                    if (occ)
                        bgUe = bgScheduler->getPrevBandInterferingUe(i);
                }

                // if the ext cell is active, add interference
                if (occ) {
                    txPwr = bgUe->getTxPwr();

                    c = bgUe->getCoord();
                    dist = bsCoord.distance(c);

                    EV << "\t distance between BgBS[" << bsCoord.x << "," << bsCoord.y <<
                        "] and backgroundUE[" << c.x << "," << c.y << "] is -> "
                       << dist << "\t";

                    // compute attenuation according to some path loss model
                    att = radio->computeExtCellPathLoss(dist, LinkKey(nodeId));

                    recvPwrDBm = txPwr - att - angularAtt - radio->getCableLoss() + radio->getAntennaGainEnB() + antennaGainBgUe;
                    recvPwr = dBmToLinear(recvPwrDBm);

                    (*interference)[i] += recvPwr;
                }
            }
        }
    }

    return true;
}

bool CellularInterferenceModel::computeD2DInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId senderId,
        inet::Coord senderCoord, MacNodeId destId, inet::Coord destCoord, bool isCqi, GHz carrierFrequency,
        std::vector<double> *interference, Direction dir)
{
    EV << "**** D2D Interference for cellId[" << eNbId << "] node[" << destId << "] ****" << endl;

    // get the D2D view of the eNodeB's MAC
    ID2dMacEnb *macEnb = check_and_cast<ID2dMacEnb *>(binder_->getMacFromMacNodeId(eNbId));

    const std::vector<std::vector<UeAllocationInfo>> *ulTransmissionMap;
    const std::vector<UeAllocationInfo> *allocatedUes;

    unsigned int numBands = radio->getNumBands();

    // CQI computation checks the slot occupation of the current TTI;
    // error computation checks the occupation of the previous TTI
    ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, isCqi ? CURR_TTI : PREV_TTI);
    if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
        for (unsigned int i = 0; i < numBands; i++) {
            // get the UEs transmitting on the same band
            allocatedUes = &(ulTransmissionMap->at(i));

            for (auto& ue_it : *allocatedUes) {
                const auto interferer = StochasticChannelModel::describeInterferer(ue_it, medium_, carrierFrequency);
                const MacNodeId ueId = interferer.nodeId;
                const MacCellId cellId = interferer.cellId;
                const Direction dir = interferer.dir;
                const double txPwr = interferer.txPwr;
                const inet::Coord ueCoord = interferer.coord;

                // no self-interference
                if (ueId == senderId || ueId == destId)
                    continue;

                // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                if (dir == UL && cellId == eNbId)
                    continue;

                // no interference from D2D connections of the same cell when reuse is disabled (otherwise, computation of CQI is misleading)
                if (cellId == eNbId && (!macEnb->isReuseD2DEnabled() && !macEnb->isReuseD2DMultiEnabled()))
                    continue;

                EV << NOW << " CellularInterferenceModel::computeD2DInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                // get tx power and attenuation from this UE
                double rxPwr = txPwr - radio->getCableLoss() + 2 * radio->getAntennaGainUe();
                // interferer -> our receiver; the eNB-side maps are used for interferers
                double att = medium_->getAttenuation(radio, medium_->d2dLink(radio, ueId, ueCoord, destId, destCoord, false));
                (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
    }

    // Debug Output
    EV << NOW << " CellularInterferenceModel::computeD2DInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < numBands; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

} //namespace
