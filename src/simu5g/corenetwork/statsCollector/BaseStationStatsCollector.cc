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

#include "simu5g/corenetwork/statsCollector/BaseStationStatsCollector.h"
#include "simu5g/corenetwork/statsCollector/UeStatsCollector.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include <string>

namespace simu5g {

using namespace omnetpp;

Define_Module(BaseStationStatsCollector);

BaseStationStatsCollector::~BaseStationStatsCollector()
{
    cancelAndDelete(pdcpBytes_);
    cancelAndDelete(prbUsage_);
    cancelAndDelete(discardRate_);
    cancelAndDelete(activeUsers_);
    cancelAndDelete(packetDelay_);
    cancelAndDelete(tPut_);
}

void BaseStationStatsCollector::initialize(int stage) {

    if (stage == inet::INITSTAGE_LOCAL) {
        EV << collectorType_ << "::initialize stage: " << stage << endl;
        collectorType_ = par("collectorType").stringValue();
        std::string nodeType = getContainingNode(this)->par("nodeType").stdstringValue();
        nodeType_ = static_cast<RanNodeType>(cEnum::get("simu5g::RanNodeType")->lookup(nodeType.c_str()));
        EV << collectorType_ << "::initialize node type: " << nodeType << endl;
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        EV << collectorType_ << "::initialize stage: " << stage << endl;

        cModule *node = getContainingNode(this);
        ecgi_.plmn.mcc = node->par("mcc").stdstringValue();
        ecgi_.plmn.mnc = node->par("mnc").stdstringValue();

        mac_.reference(this, "macModule", true);
        pdcp_.reference(this, "pdcpModule", true);

        rlc_.reference(this, "rlcUmModule", false);
        if (!rlc_) {
            throw cRuntimeError("%s::initialize - eNodeB statistic collector only works with RLC in UM mode", collectorType_.c_str());
        }

        // PacketFlowManager functionality removed
        cellInfo_.reference(this, "cellInfoModule", true);

        ecgi_.cellId = cellInfo_->getMacCellId(); // at least stage 2

        dl_total_prb_usage_cell.init("dl_total_prb_usage_cell", par("prbUsagePeriods"), par("movingAverage"));
        ul_total_prb_usage_cell.init("ul_total_prb_usage_cell", par("prbUsagePeriods"), par("movingAverage"));

        number_of_active_ue_dl_nongbr_cell.init("number_of_active_ue_dl_nongbr_cell", par("activeUserPeriods"), false);
        number_of_active_ue_ul_nongbr_cell.init("number_of_active_ue_ul_nongbr_cell", par("activeUserPeriods"), false);

        dl_nongbr_pdr_cell.init("dl_nongbr_pdr_cell", par("discardRatePeriods"), false);
        ul_nongbr_pdr_cell.init("ul_nongbr_pdr_cell", par("discardRatePeriods"), false);

        // setup timers
        prbUsage_ = new cMessage("prbUsage_");
        activeUsers_ = new cMessage("activeUsers_");
        discardRate_ = new cMessage("discardRate_");
        packetDelay_ = new cMessage("packetDelay_");
        pdcpBytes_ = new cMessage("pdcpBytes_");
        tPut_ = new cMessage("tPut_");

        prbUsagePeriod_ = par("prbUsagePeriod");
        activeUsersPeriod_ = par("activeUserPeriod");
        discardRatePeriod_ = par("discardRatePeriod");
        delayPacketPeriod_ = par("delayPacketPeriod");
        dataVolumePeriod_ = par("dataVolumePeriod");
        tPutPeriod_ = par("tPutPeriod");

        // start scheduling the l2 measurement
        // PacketFlowManager functionality removed - only scheduling basic stats

        scheduleAt(NOW + prbUsagePeriod_, prbUsage_);
        scheduleAt(NOW + activeUsersPeriod_, activeUsers_);
        // PacketFlowManager-dependent timers disabled
    }
}

void BaseStationStatsCollector::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        EV << collectorType_ << "::handleMessage - get " << msg->getName() << " statistics" << endl;

        if (msg == prbUsage_) {
            add_dl_total_prb_usage_cell();
            add_ul_total_prb_usage_cell();
            scheduleAt(NOW + prbUsagePeriod_, prbUsage_);
        }
        else if (msg == activeUsers_) {
            add_number_of_active_ue_dl_nongbr_cell();
            add_number_of_active_ue_ul_nongbr_cell();
            scheduleAt(NOW + activeUsersPeriod_, activeUsers_);
        }
        else if (msg == tPut_) {
            add_dl_nongbr_throughput_ue_perUser();
            add_ul_nongbr_throughput_ue_perUser();
            scheduleAt(NOW + tPutPeriod_, tPut_);
        }
        // PacketFlowManager-dependent handlers removed
        else {
            delete msg;
        }
    }
    else {
        EV << collectorType_ << "::handleMessage - it is not a self message" << endl;
        delete msg;
    }
}

void BaseStationStatsCollector::resetDiscardCounterPerUe()
{
    // PacketFlowManager functionality removed - method now empty
    EV << collectorType_ << "::resetDiscardCounterPerUe " << endl;
}

void BaseStationStatsCollector::resetDelayCounterPerUe()
{
    EV << collectorType_ << "::resetDelayCounterPerUe " << endl;
    // PacketFlowManager functionality removed - only keeping UE collector reset
    for (auto const& ue : ueCollectors_) {
        ue.second->resetDelayCounter();
    }
}

void BaseStationStatsCollector::resetThroughputCountersPerUe()
{
    // PacketFlowManager functionality removed - method now empty
    EV << collectorType_ << "::resetThroughputCountersPerUe " << endl;
}

void BaseStationStatsCollector::resetBytesCountersPerUe()
{
    // PacketFlowManager functionality removed - method now empty
    EV << collectorType_ << "::resetBytesCountersPerUe " << endl;
}

// BaseStationStatsCollector management methods
void BaseStationStatsCollector::addUeCollector(MacNodeId id, UeStatsCollector *ueCollector)
{
    if (ueCollectors_.find(id) == ueCollectors_.end()) {
        ueCollectors_.insert({id, ueCollector});
    }
    else {
        throw cRuntimeError("%s::addUeCollector - UeStatsCollector already present for UE node id [%hu]", collectorType_.c_str(), num(id));
    }
}

void BaseStationStatsCollector::removeUeCollector(MacNodeId id)
{
    std::map<MacNodeId, UeStatsCollector *>::iterator it = ueCollectors_.find(id);
    if (it != ueCollectors_.end()) {
        ueCollectors_.erase(it);
        EV << "BaseStationStatsCollector::removeUeCollector - removing UE stats for UE with id[" << id << "]" << endl;
        // PacketFlowManager functionality removed
    }
    else {
        throw cRuntimeError("%s::removeUeCollector - UeStatsCollector not present for UE node id [%hu]", collectorType_.c_str(), num(id));
    }
}

UeStatsCollector *BaseStationStatsCollector::getUeCollector(MacNodeId id)
{
    std::map<MacNodeId, UeStatsCollector *>::iterator it = ueCollectors_.find(id);
    if (it != ueCollectors_.end()) {
        return it->second;
    }
    else {
        throw cRuntimeError("%s::removeUeCollector - UeStatsCollector not present for UE node id [%hu]", collectorType_.c_str(), num(id));
    }
}

UeStatsCollectorMap *BaseStationStatsCollector::getCollectorMap()
{
    return &ueCollectors_;
}

bool BaseStationStatsCollector::hasUeCollector(MacNodeId id)
{
    return ueCollectors_.find(id) != ueCollectors_.end();
}

void BaseStationStatsCollector::add_dl_total_prb_usage_cell()
{
    double prb_usage = mac_->getUtilization(DL);
    EV << collectorType_ << "::add_dl_total_prb_usage_cell " << prb_usage << "%" << endl;
    dl_total_prb_usage_cell.addValue((int)prb_usage);
}

void BaseStationStatsCollector::add_ul_total_prb_usage_cell()
{
    double prb_usage = mac_->getUtilization(UL);
    EV << collectorType_ << "::add_ul_total_prb_usage_cell " << prb_usage << "%" << endl;
    ul_total_prb_usage_cell.addValue((int)prb_usage);
}

void BaseStationStatsCollector::add_number_of_active_ue_dl_nongbr_cell()
{
    int users = mac_->getActiveUesNumber(DL);
    EV << collectorType_ << "::add_number_of_active_ue_dl_nongbr_cell " << users << endl;
    number_of_active_ue_dl_nongbr_cell.addValue(users);
}

void BaseStationStatsCollector::add_number_of_active_ue_ul_nongbr_cell()
{
    int users = mac_->getActiveUesNumber(UL);
    EV << collectorType_ << "::add_number_of_active_ue_ul_nongbr_cell " << users << endl;
    number_of_active_ue_ul_nongbr_cell.addValue(users);
}

void BaseStationStatsCollector::add_dl_nongbr_pdr_cell()
{
    // PacketFlowManager functionality removed - setting default value
    dl_nongbr_pdr_cell.addValue(0);
}

void BaseStationStatsCollector::add_ul_nongbr_pdr_cell()
{
    double pdr = 0.0;
    DiscardedPkts pair = { 0, 0 };
    DiscardedPkts temp = { 0, 0 };

    for (auto const& [ueId, ueCollector] : ueCollectors_) {
        temp = ueCollector->getULDiscardedPkt();
        pair.discarded += temp.discarded;
        pair.total += temp.total;
    }

    if (pair.total == 0)
        pdr = 0.0;
    else
        pdr = ((double)pair.discarded * 1000000) / pair.total;
    ul_nongbr_pdr_cell.addValue((int)pdr);
}

//TODO handover management

// for each user save stats

void BaseStationStatsCollector::add_dl_nongbr_pdr_cell_perUser()
{
    EV << collectorType_ << "::add_dl_nongbr_pdr_cell_perUser()" << endl;
    // PacketFlowManager functionality removed - setting default value
    for (auto const& ue : ueCollectors_) {
        ue.second->add_dl_nongbr_pdr_ue(0);
    }
}

void BaseStationStatsCollector::add_ul_nongbr_pdr_cell_perUser()
{
    EV << collectorType_ << "::add_ul_nongbr_pdr_cell_perUser()" << endl;
    for (auto const& [ueId, ueCollector] : ueCollectors_) {
        ueCollector->add_ul_nongbr_pdr_ue();
    }
}

void BaseStationStatsCollector::add_ul_nongbr_delay_perUser()
{
    EV << collectorType_ << "::add_ul_nongbr_delay_perUser()" << endl;
    for (auto const& [ueId, ueCollector] : ueCollectors_) {
        ueCollector->add_ul_nongbr_delay_ue();
    }
}

void BaseStationStatsCollector::add_dl_nongbr_delay_perUser()
{
    EV << collectorType_ << "::add_dl_nongbr_delay_perUser()" << endl;
    // PacketFlowManager functionality removed - setting default value
    for (auto const& ue : ueCollectors_) {
        ue.second->add_dl_nongbr_delay_ue(0);
    }
}

void BaseStationStatsCollector::add_ul_nongbr_data_volume_ue_perUser()
{
    EV << collectorType_ << "::add_ul_nongbr_data_volume_ue_perUser" << endl;
    // PacketFlowManager functionality removed - setting default value
    for (auto const& ue : ueCollectors_) {
        ue.second->add_ul_nongbr_data_volume_ue(0);
    }
}

void BaseStationStatsCollector::add_dl_nongbr_data_volume_ue_perUser()
{
    EV << collectorType_ << "::add_dl_nongbr_data_volume_ue_perUser" << endl;
    // PacketFlowManager functionality removed - setting default value
    for (auto const& ue : ueCollectors_) {
        ue.second->add_dl_nongbr_data_volume_ue(0);
    }
}

void BaseStationStatsCollector::add_dl_nongbr_throughput_ue_perUser()
{
    EV << collectorType_ << "::add_dl_nongbr_throughput_ue_perUser" << endl;
    // PacketFlowManager functionality removed - setting default value
    for (auto const& ue : ueCollectors_) {
        ue.second->add_dl_nongbr_throughput_ue(0);
    }
}

void BaseStationStatsCollector::add_ul_nongbr_throughput_ue_perUser()
{
    EV << collectorType_ << "::add_ul_nongbr_throughput_ue_perUser" << endl;
    double throughput;
    for (auto const& [ueId, ueCollector] : ueCollectors_) {
        throughput = rlc_->getUeThroughput(ueId);
        EV << collectorType_ << "::add_ul_nongbr_throughput_ue_perUser - throughput: " << throughput << " for node " << ueId << endl;
        rlc_->resetThroughputStats(ueId);
        if (throughput > 0.0)
            ueCollector->add_ul_nongbr_throughput_ue((int)throughput);
    }
}

/*
 * Getters for RNI service module
 */

int BaseStationStatsCollector::get_ul_nongbr_pdr_cell()
{
    return ul_nongbr_pdr_cell.getMean();
}

int BaseStationStatsCollector::get_dl_nongbr_pdr_cell()
{
    return dl_nongbr_pdr_cell.getMean();
}

// since GBR rab has not been implemented nongbr = total
int BaseStationStatsCollector::get_dl_total_prb_usage_cell() {
    return dl_total_prb_usage_cell.getMean();
}

int BaseStationStatsCollector::get_ul_total_prb_usage_cell() {
    return ul_total_prb_usage_cell.getMean();
}

int BaseStationStatsCollector::get_dl_nongbr_prb_usage_cell() {
    return dl_total_prb_usage_cell.getMean();
}

int BaseStationStatsCollector::get_ul_nongbr_prb_usage_cell() {
    return ul_total_prb_usage_cell.getMean();
}

int BaseStationStatsCollector::get_number_of_active_ue_dl_nongbr_cell() {
    return number_of_active_ue_dl_nongbr_cell.getMean();
}

int BaseStationStatsCollector::get_number_of_active_ue_ul_nongbr_cell() {
    return number_of_active_ue_ul_nongbr_cell.getMean();
}

const mec::Ecgi& BaseStationStatsCollector::getEcgi() const
{
    return ecgi_;
}

MacCellId BaseStationStatsCollector::getCellId() const
{
    return cellInfo_->getMacCellId();
}

void BaseStationStatsCollector::resetStats(MacNodeId nodeId)
{
    auto ue = ueCollectors_.find(nodeId);
    if (ue != ueCollectors_.end())
        ue->second->resetStats();
}

} //namespace
