//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/phy/Tr37885ChannelModel.h"

#include <inet/common/InitStages.h>

#include "simu5g/stack/sidelink/common/SlAirFrame_m.h"
#include "simu5g/stack/sidelink/mac/SlMcsTable.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(Tr37885ChannelModel);

int Tr37885ChannelModel::numInitStages() const
{
    return inet::NUM_INIT_STAGES;
}

void Tr37885ChannelModel::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
        slBinder_ = SlBinder::getInstance();

        std::string scenario = par("scenario").stdstringValue();
        if (scenario == "highway")
            scenario_ = HIGHWAY;
        else if (scenario == "urban")
            scenario_ = URBAN;
        else
            throw cRuntimeError("Tr37885ChannelModel: unknown scenario '%s' (expected highway/urban)", scenario.c_str());

        std::string losState = par("losState").stdstringValue();
        if (losState == "LOS")
            losState_ = LOS;
        else if (losState == "NLOSv")
            losState_ = NLOSV;
        else if (losState == "NLOS")
            losState_ = NLOS;
        else
            throw cRuntimeError("Tr37885ChannelModel: unknown losState '%s' (expected LOS/NLOSv/NLOS)", losState.c_str());

        if (scenario_ == HIGHWAY && losState_ == NLOS)
            throw cRuntimeError("Tr37885ChannelModel: the highway scenario has no NLOS state (only LOS/NLOSv)");

        shadowing_ = par("shadowing");
        noiseFigureDb_ = par("noiseFigure").doubleValue();
        pscchSinrThresholdDb_ = par("pscchSinrThreshold").doubleValue();
        cableLossDb_ = par("cableLoss").doubleValue();
        harqReduction_ = par("harqReduction").doubleValue();
    }
}

void Tr37885ChannelModel::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

double Tr37885ChannelModel::computePathLossDb(double d, double fcGHz) const
{
    d = std::max(d, 1.0);  // clamp below the model's validity range

    switch (scenario_) {
        case HIGHWAY:
            // LOS and NLOSv share the pathloss formula (blockage is an extra loss)
            return 32.4 + 20.0 * log10(d) + 20.0 * log10(fcGHz);
        case URBAN:
            if (losState_ == NLOS)
                return 36.85 + 30.0 * log10(d) + 18.9 * log10(fcGHz);
            return 38.77 + 16.7 * log10(d) + 18.2 * log10(fcGHz);
    }
    throw cRuntimeError("unreachable");
}

double Tr37885ChannelModel::getShadowing(MacNodeId txId, MacNodeId rxId, double distance)
{
    if (!shadowing_)
        return 0;

    // per-pair persistent log-normal shadowing (mobility decorrelation is not
    // modeled in SL-1; the value is drawn once per node pair)
    auto key = std::minmax(txId, rxId);
    auto it = shadowingMap_.find(key);
    if (it != shadowingMap_.end())
        return it->second;

    double sigma = (losState_ == NLOS) ? 4.0 : 3.0;
    double value = normal(0.0, sigma);
    shadowingMap_[key] = value;
    return value;
}

double Tr37885ChannelModel::computeAttenuationDb(MacNodeId txId, MacNodeId rxId, const inet::Coord& txCoord, const inet::Coord& rxCoord, double fcGHz)
{
    double d = txCoord.distance(rxCoord);
    double attenuation = computePathLossDb(d, fcGHz) + getShadowing(txId, rxId, d) + cableLossDb_;

    if (losState_ == NLOSV) {
        // vehicle blockage extra loss, drawn per frame (blockers move).
        // TR 37.885 Table 6.2.1-1 case "blocker higher than one antenna"
        // (the typical car-antenna case): mu = 5 + max(0, 15log10(d) - 41),
        // sigma = 4; the both-antennas-below-blocker case (9 / 4.5) and the
        // per-blocker height mix are not modeled.
        double mu = 5.0 + std::max(0.0, 15.0 * log10(std::max(d, 1.0)) - 41.0);
        attenuation += std::max(0.0, normal(mu, 4.0));
    }

    return attenuation;
}

double Tr37885ChannelModel::computeNoiseDbm(const SlBinder::SlCarrierInfo *carrier, int numSubchannels) const
{
    // thermal noise over the frame's bandwidth: numSubchannels x subchannelSize
    // PRBs of 12 subcarriers at 15kHz * 2^numerology
    double scsHz = 15e3 * (1 << carrier->numerologyIndex);
    double bwHz = numSubchannels * carrier->subchannelSize * 12 * scsHz;
    return -174.0 + 10.0 * log10(bwHz) + noiseFigureDb_;
}

SlReceptionResult Tr37885ChannelModel::computeReception(const SlAirFrameInfo& info, const inet::Coord& rxCoord, MacNodeId rxNodeId)
{
    Enter_Method_Silent("computeReception()");

    GHz carrierFrequency = info.getCarrierFrequency();
    double fcGHz = carrierFrequency.get();  // inet::GHz carries the value in GHz
    const SlBinder::SlCarrierInfo *carrier = slBinder_->getSlCarrier(carrierFrequency);
    if (carrier == nullptr)
        throw cRuntimeError("Tr37885ChannelModel: SL carrier %f GHz not registered in SlBinder", fcGHz);
    ASSERT(fcGHz > 0.1 && fcGHz < 100);  // sanity: the model expects fc in GHz

    SlReceptionResult result;

    // desired signal power
    double attenuationDb = computeAttenuationDb(info.getSrcNodeId(), rxNodeId, info.getSenderCoord(), rxCoord, fcGHz);
    result.rsrpDbm = info.getTxPower() - attenuationDb;

    // interference: co-slot transmissions overlapping the frame's subchannels
    // (from the SL transmission map, D9), weighted by the overlap fraction
    int frameFirst = info.getFirstSubchannel();
    int frameLast = frameFirst + info.getNumSubchannels() - 1;
    double interferenceMw = 0;
    const auto *records = slBinder_->getSlTransmissions(carrierFrequency, info.getSlotIndex());
    if (records != nullptr) {
        for (const auto& rec : *records) {
            if (rec.txNodeId == info.getSrcNodeId() || rec.txNodeId == rxNodeId)
                continue;
            int overlapFirst = std::max(frameFirst, rec.firstSubchannel);
            int overlapLast = std::min(frameLast, rec.firstSubchannel + rec.numSubchannels - 1);
            if (overlapFirst > overlapLast)
                continue;
            double overlapFraction = (double)(overlapLast - overlapFirst + 1) / info.getNumSubchannels();
            double intfAttenuationDb = computeAttenuationDb(rec.txNodeId, rxNodeId, rec.txCoord, rxCoord, fcGHz);
            double intfDbm = rec.txPower - intfAttenuationDb;
            interferenceMw += pow(10.0, intfDbm / 10.0) * overlapFraction;

            EV << "Tr37885ChannelModel::computeReception - interferer node " << rec.txNodeId
               << " overlap " << overlapFraction << " power " << intfDbm << " dBm" << endl;
        }
    }

    double noiseMw = pow(10.0, computeNoiseDbm(carrier, info.getNumSubchannels()) / 10.0);
    double signalMw = pow(10.0, result.rsrpDbm / 10.0);
    result.sinrDb = 10.0 * log10(signalMw / (interferenceMw + noiseMw));

    // PSCCH: SCI decodes iff the SINR is above the configured operating point (D11)
    result.sciDecoded = (result.sinrDb >= pscchSinrThresholdDb_);

    if (result.sciDecoded) {
        // PSSCH: per-CQI BLER curves, indexed by the coarse CQI equivalent of
        // the grant's MCS (SlMcsTable, D15 -- documented approximation until
        // SL-specific BLER curves exist). The decode decision itself
        // (including blind-HARQ combining) is drawn by the PHY from
        // tbErrorProb.
        int cqi = std::min(std::max((int)SlMcsTable::cqi(info.getMcs()), 1), 15);
        int snrInt = (int)floor(result.sinrDb);
        double bler;
        if (snrInt < binder_->phyPisaData.minSnr())
            bler = 1.0;
        else if (snrInt > binder_->phyPisaData.maxSnr())
            bler = 0.0;
        else
            bler = binder_->phyPisaData.getBler(0, cqi, snrInt);

        int numPrbs = info.getNumSubchannels() * carrier->subchannelSize;
        result.tbErrorProb = 1.0 - pow(1.0 - bler, numPrbs);

        EV << "Tr37885ChannelModel::computeReception - rsrp " << result.rsrpDbm << " dBm, sinr "
           << result.sinrDb << " dB, cqi " << cqi << ", bler " << bler << " over " << numPrbs
           << " PRBs -> TB error probability " << result.tbErrorProb << endl;
    }
    else {
        result.tbErrorProb = 1.0;
        EV << "Tr37885ChannelModel::computeReception - sinr " << result.sinrDb
           << " dB below PSCCH threshold -> SCI LOST" << endl;
    }

    return result;
}

} // namespace simu5g
