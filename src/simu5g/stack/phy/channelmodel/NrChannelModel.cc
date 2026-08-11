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

#include "simu5g/stack/phy/channelmodel/NrChannelModel.h"

#include "simu5g/stack/phy/channelmodel/Tr36873PathLossModel.h"

namespace simu5g {

Define_Module(NrChannelModel);

void NrChannelModel::initialize(int stage)
{
    LteRealisticChannelModel::initialize(stage);
}

PathLossModel *NrChannelModel::createPathLossModel()
{
    return new Tr36873PathLossModel();
}

void NrChannelModel::computeLosProbability(double d3D, double d2D, const LinkKey& nodeId)
{
    if (!dynamicLos_) {
        losMap_[nodeId] = fixedLos_;
        return;
    }
    double p = pathLoss_->computeLosProbability(d3D, d2D);
    losMap_[nodeId] = (uniform(0.0, 1.0) <= p);
}

double NrChannelModel::computePathLoss(double threeDimDistance, double twoDimDistance, bool los)
{
    return pathLoss_->computePathLoss(threeDimDistance, twoDimDistance, los);
}

bool NrChannelModel::computeExtCellInterference(MacNodeId eNbId, MacNodeId nodeId, Coord coord, bool isCqi, GHz carrierFrequency,
        std::vector<double> *interference)
{
    EV << "**** Ext Cell Interference **** " << endl;

    // get external cell list
    ExtCellList list = binder_->getExtCellList(carrierFrequency);

    Coord c;
    double threeDimDist, // meters
           twoDimDist, // meters
           recvPwr, // watt
           recvPwrDBm, // dBm
           att, // dBm
           angularAtt; // dBm

    std::vector<double> fadingAttenuation;

    // compute distance for each cell
    for (const auto& extCell : list) {
        // get external cell position
        c = extCell->getPosition();
        // compute distance between UE and the ext cell
        threeDimDist = coord.distance(c);
        twoDimDist = getTwoDimDistance(coord, c);

        EV << "\t distance between UE[" << coord.x << "," << coord.y <<
            "] and extCell[" << c.x << "," << c.y << "] is -> "
           << threeDimDist << "\t";

        // compute attenuation according to some path loss model
        att = computeExtCellPathLoss3D(threeDimDist, twoDimDist, nodeId);

        //=============== ANGULAR ATTENUATION =================
        if (extCell->getTxDirection() == OMNI) {
            angularAtt = 0;
        }
        else {
            // compute the angle between uePosition and reference axis, considering the eNb as center
            double ueAngle = computeAngle(c, coord);

            // compute the reception angle between ue and eNb
            double recvAngle = fabs(extCell->getTxAngle() - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(c, coord);

            // compute attenuation due to sectorial tx
            angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);
        }
        //=============== END ANGULAR ATTENUATION =================

        // TODO do we need to use (- cableLoss_ + antennaGainEnB_) in ext cells too?
        // compute and linearize received power
        recvPwrDBm = extCell->getTxPower() - att - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainUe_;
        recvPwr = dBmToLinear(recvPwrDBm);

        int numBands = std::min(numBands_, extCell->getNumBands());
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

double NrChannelModel::computeExtCellPathLoss3D(double threeDimDistance, double twoDimDistance, MacNodeId nodeId)
{
    computeSpeed(nodeId, phy_->getCoord());

    // External-cell interferers are cellular links: degenerate key, as before.
    const LinkKey key(nodeId);

    // Compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losMap_[key];

    if (!enable_extCell_los_)
        los = false;

    // TODO Apply shadowing to each interfering extCell signal
    double attenuation = computePathLoss(threeDimDistance, twoDimDistance, los);

    // Applying shadowing only if it is enabled by configuration
    // Log-normal shadowing
    if (shadowing_) {
        double att;

        att = lastComputedSF_.at(key).second;
        EV << "(" << att << ")";
        attenuation += att;
    }

    return attenuation;
}

} //namespace
