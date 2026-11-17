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

#include "simu5g/stack/phy/medium/CellularRadioMedium.h"

#include <algorithm>

#include <inet/common/INETMath.h>

#include "simu5g/stack/phy/channelmodel/IRadioEndpoint.h"
#include "simu5g/stack/phy/packet/AirFrame_m.h"
#include "simu5g/stack/phy/channelmodel/ChannelModelBase.h"

namespace simu5g {

Define_Module(CellularRadioMedium);

ChannelModelBase *CellularRadioMedium::getChannelModel(GHz carrierFrequency, bool isNr) const
{
    const char *name = isNr ? "nrChannelModel" : "channelModel";
    for (int i = 0; i < par("numCarriers").intValue(); i++) {
        auto *model = check_and_cast<ChannelModelBase *>(getSubmodule(name, i));
        if (model->getCarrierFrequency() == carrierFrequency)
            return model;
    }
    return nullptr;
}

ChannelModelBase *CellularRadioMedium::getWhatIfChannelModel(IRadioEndpoint *receiver, const CellularTransmission& transmission) const
{
    ChannelModelBase *channelModel = receiver->getChannelModel(transmission.carrierFrequency);
    if (channelModel == nullptr)
        throw cRuntimeError("CellularRadioMedium::getWhatIfChannelModel(): the receiver %d of a what-if does not operate on its carrier %g GHz",
                (int)num(receiver->getMacNodeId()), transmission.carrierFrequency.get());
    return channelModel;
}

std::vector<double> CellularRadioMedium::computeRsrp(IRadioEndpoint *receiver, const CellularTransmission& transmission)
{
    return getWhatIfChannelModel(receiver, transmission)->computeRsrp(receiver, transmission);
}

std::vector<double> CellularRadioMedium::computeSinr(IRadioEndpoint *receiver, const CellularTransmission& transmission)
{
    return getWhatIfChannelModel(receiver, transmission)->computeSinr(receiver, transmission);
}

std::vector<double> CellularRadioMedium::computeReceptionRsrp(IRadioEndpoint *receiver, AirFrame *frame)
{
    GHz carrierFrequency = check_and_cast<UserControlInfo *>(frame->getControlInfo())->getCarrierFrequency();
    ChannelModelBase *channelModel = receiver->getChannelModel(carrierFrequency);
    if (channelModel == nullptr)
        throw cRuntimeError("CellularRadioMedium::computeReceptionRsrp(): the receiver %d does not operate on the frame's carrier %g GHz",
                (int)num(receiver->getMacNodeId()), carrierFrequency.get());
    return channelModel->computeReceptionRsrp(receiver, frame);
}

void CellularRadioMedium::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        // the distance at which a free-space transmission at pMax falls to the sat threshold
        double waveLength = SPEED_OF_LIGHT / par("carrierFrequency").doubleValue();
        double minReceivePower = pow(10.0, par("sat").doubleValue() / 10.0);
        maxInterferenceDistance = pow(waveLength * waveLength * par("pMax").doubleValue()
                / (16.0 * M_PI * M_PI * minReceivePower), 1.0 / par("alpha").doubleValue());
        EV << "max interference distance:" << maxInterferenceDistance << endl;
        if (par("recordReceptionTrace"))
            rxTrace.open(getEnvir()->getConfig()->substituteVariables("${resultdir}/${configname}-${runnumber}.rxtrace"));
    }
}

void CellularRadioMedium::finish()
{
    rxTrace.close();
}

CellularRadioMedium::~CellularRadioMedium()
{
    for (auto& [carrier, carrierTransmissions] : transmissions)
        for (auto transmission : carrierTransmissions)
            delete transmission;
}

void CellularRadioMedium::addRadio(MacNodeId nodeId, IRadioEndpoint *radio, cModule *radioModule)
{
    Enter_Method("addRadio");
    if (nodeId != NODEID_NONE && !radios.emplace(nodeId, radio).second)
        throw cRuntimeError("CellularRadioMedium::addRadio(): a radio is already registered for node %d", (int)num(nodeId));
    if (radioModule != nullptr)
        radioModules.push_back(RadioModule{radio, radioModule->gate("radioIn")->getPathStartGate()});
}

void CellularRadioMedium::removeRadio(IRadioEndpoint *radio)
{
    Enter_Method("removeRadio");
    bool found = false;
    for (auto it = radios.begin(); it != radios.end(); ++it) {
        if (it->second == radio) {
            radios.erase(it);
            found = true;
            break;
        }
    }
    auto it = std::find_if(radioModules.begin(), radioModules.end(), [radio] (const RadioModule& m) { return m.radio == radio; });
    if (it != radioModules.end()) {
        radioModules.erase(it);
        found = true;
    }
    if (!found)
        throw cRuntimeError("CellularRadioMedium::removeRadio(): the radio is not on the medium");
}

IRadioEndpoint *CellularRadioMedium::findRadio(MacNodeId nodeId) const
{
    auto it = radios.find(nodeId);
    return it == radios.end() ? nullptr : it->second;
}

void CellularRadioMedium::sendToNeighbors(IRadioEndpoint *sender, cSimpleModule *sendingModule, AirFrame *frame, simtime_t duration)
{
    // NOTE: no Enter_Method(): the copies are sent by the sending radio module
    const inet::Coord& senderPosition = sender->getCoord();
    double maxDistanceSquared = maxInterferenceDistance * maxInterferenceDistance;
    for (const auto& radioModule : radioModules) {
        if (radioModule.radio == sender)
            continue;
        if (senderPosition.sqrdist(radioModule.radio->getCoord()) < maxDistanceSquared) {
            EV << "sending message to radio\n";
            // no propagation delay, as for the frames the PHY sends to a single radio
            sendingModule->sendDirect(frame->dup(), 0, duration, radioModule.radioInGate);
        }
    }
    // the radios in range got copies; the original frame can be deleted
    delete frame;
}

simtime_t CellularRadioMedium::getSlotDuration(GHz carrierFrequency)
{
    auto it = slotDurations.find(carrierFrequency);
    if (it == slotDurations.end()) {
        ChannelModelBase *channelModel = getChannelModel(carrierFrequency, false);
        if (channelModel == nullptr)
            channelModel = getChannelModel(carrierFrequency, true);
        if (channelModel == nullptr)
            throw cRuntimeError("CellularRadioMedium: a transmission is on %g GHz, and the medium has no channel model on that carrier", carrierFrequency.get());
        it = slotDurations.emplace(carrierFrequency, SimTime(1, SIMTIME_MS) / (1 << channelModel->getNumerologyIndex())).first;
        carrierNumBands[carrierFrequency] = channelModel->getNumBands();
    }
    return it->second;
}

void CellularRadioMedium::keepTransmission(CellularTransmission *transmission)
{
    transmission->id = nextTransmissionId++;
    auto& carrierTransmissions = transmissions[transmission->carrierFrequency];

    // the slot under way, by the carrier's slot grid; data is sent slot by slot
    simtime_t slot = getSlotDuration(transmission->carrierFrequency);
    simtime_t slotStart = SimTime::fromRaw(transmission->startTime.raw() - transmission->startTime.raw() % slot.raw());
    ASSERT(transmission->frameType != DATAPKT || transmission->startTime == slotStart);

    // its bands are the carrier's own, counted from the carrier's first band
    for (const auto& [antenna, bandBlocks] : transmission->grantedBlocks)
        for (const auto& [band, blocks] : bandBlocks)
            ASSERT(blocks == 0 || band < carrierNumBands[transmission->carrierFrequency]);

    // the slot before it is kept, for the questions about the slot completed last
    simtime_t keptSince = slotStart - slot;
    auto ended = std::remove_if(carrierTransmissions.begin(), carrierTransmissions.end(), [keptSince] (const CellularTransmission *t) {
        if (t->getEndTime() < keptSince) {
            delete t;
            return true;
        }
        return false;
    });
    carrierTransmissions.erase(ended, carrierTransmissions.end());
    carrierTransmissions.push_back(transmission);
}

void CellularRadioMedium::addTransmission(CellularTransmission *transmission)
{
    Enter_Method("addTransmission");
    keepTransmission(transmission);

    // where each other radio is as the transmission starts, read from its
    // mobility, in the order the radios were added -- as INET's medium does
    // when it computes the transmission's arrival at every radio; the value
    // itself is not needed, the read is: it moves each mobility to now
    for (const auto& radioModule : radioModules)
        if (radioModule.radio != transmission->transmitter)
            radioModule.radio->getCoord();
}

void CellularRadioMedium::addPhantomTransmission(CellularTransmission *transmission)
{
    Enter_Method("addPhantomTransmission");
    keepTransmission(transmission);
}

std::vector<const CellularTransmission *> CellularRadioMedium::getDataTransmissionsDuring(GHz carrierFrequency, simtime_t from, simtime_t to) const
{
    std::vector<const CellularTransmission *> during;
    auto it = transmissions.find(carrierFrequency);
    if (it == transmissions.end())
        return during;
    for (auto t : it->second)
        if (t->frameType == DATAPKT && t->startTime < to && t->getEndTime() > from)
            during.push_back(t);
    return during;
}

std::vector<const CellularTransmission *> CellularRadioMedium::getUplinkTransmissionsDuring(GHz carrierFrequency, simtime_t from, simtime_t to) const
{
    std::vector<const CellularTransmission *> uplink;
    for (auto t : getDataTransmissionsDuring(carrierFrequency, from, to))
        if (t->direction == UL || t->direction == D2D || t->direction == D2D_MULTI)
            uplink.push_back(t);
    return uplink;
}

IRadioEndpoint *CellularRadioMedium::getRadio(MacNodeId nodeId) const
{
    IRadioEndpoint *radio = findRadio(nodeId);
    if (radio == nullptr)
        throw cRuntimeError("CellularRadioMedium::getRadio(): no radio is registered for node %d", (int)num(nodeId));
    return radio;
}

long CellularRadioMedium::joinSite(const char *name, IRadioEndpoint *cell)
{
    Enter_Method_Silent("joinSite");
    auto it = sites.find(name);
    if (it == sites.end()) {
        // apart from the names of the nodes (module ids) and of the ends without a radio
        Site site;
        site.id = -1000000 - (long)sites.size();
        site.firstCell = cell;
        sites[name] = site;
        return site.id;
    }

    IRadioEndpoint *first = it->second.firstCell;
    inet::Coord a = first->getCoord(), b = cell->getCoord();
    double apart = sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    if (apart > 1.0)
        throw cRuntimeError("the cells of the site '%s' are not at one place: the antennas of nodes %d and %d are %g m apart horizontally. "
                "The cells of a site share one LOS state and one shadowing realization towards a UE, drawn for one place",
                name, (int)num(first->getMacNodeId()), (int)num(cell->getMacNodeId()), apart);
    if (first->getDeploymentScenario() != cell->getDeploymentScenario() || first->getBuildingHeight() != cell->getBuildingHeight()
        || first->getStreetWidth() != cell->getStreetWidth() || first->getClutterDensity() != cell->getClutterDensity()
        || first->getClutterHeight() != cell->getClutterHeight() || first->getClutterSize() != cell->getClutterSize())
        throw cRuntimeError("the cells of the site '%s' are not in one deployment: nodes %d and %d differ in scenario, buildingHeight, streetWidth, "
                "clutterDensity, clutterHeight or clutterSize. "
                "The cells of a site share one LOS state towards a UE, drawn for one scenario",
                name, (int)num(first->getMacNodeId()), (int)num(cell->getMacNodeId()));
    return it->second.id;
}

ChannelState::LosMap& CellularRadioMedium::getLosMap()
{
    return losMap;
}

ChannelState::ShadowFadingMap& CellularRadioMedium::getShadowingMap()
{
    return shadowingMap;
}

ChannelState::JakesFadingMap& CellularRadioMedium::getJakesFadingMap(GHz carrierFrequency)
{
    return jakesFadingMaps[carrierFrequency];
}

} // namespace simu5g
