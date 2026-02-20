//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//
#include "simu5g/stack/mac/scheduling_modules/QoSAwareScheduler.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerEnb.h"

namespace simu5g {

using namespace omnetpp;

QoSAwareScheduler::QoSAwareScheduler(Binder* binder, double pfAlpha)
    : LteScheduler(binder), pfAlpha_(pfAlpha)
{
}

double QoSAwareScheduler::computeQosWeightFromContext(const DrbContext& ctx)
{
    // NOTE: It would be incorrect to use QFI or 5QI as input here.
    // Removed code: weight *= (10.0 - ctx.fiveQi + 1);  // Lower 5QI = better
    double weight = 1.0;
    if (ctx.gbr) weight *= 2.0;
    weight *= 10.0 / (ctx.priorityLevel + 1);  // Lower priority level = higher weight
    if (ctx.delayBudgetMs <= 10) weight *= 5.0;
    else if (ctx.delayBudgetMs <= 50) weight *= 3.0;
    else if (ctx.delayBudgetMs <= 100) weight *= 1.5;
    EV << NOW << " DRB " << ctx.drbIndex << " lcid=" << ctx.lcid << " Weight: " << weight << endl;
    return weight;
}

const DrbContext* QoSAwareScheduler::getDrbContextForCid(MacCid cid)
{
    // Look up DRB context by LCID (= local DRB index within the UE's DRB set)
    int lcid = (int)cid.getLcid();
    // Search drbMap for a DrbContext whose lcid matches and ueNodeId matches
    MacNodeId ueNodeId = cid.getNodeId();
    const auto& drbMap = qfiContextMgr_->getDrbMap();
    for (const auto& [drb, ctx] : drbMap) {
        if (ctx.lcid == lcid && (ctx.ueNodeId == ueNodeId || ctx.ueNodeId == NODEID_NONE))
            return &ctx;
    }
    EV_WARN << "QoSAwareScheduler: No DRB context for CID " << cid << "\n";
    return nullptr;
}

void QoSAwareScheduler::prepareSchedule()
{
    if (!qfiContextMgr_)
        throw cRuntimeError("QoSAwareScheduler requires QfiContextManager but none was configured. "
                            "Set mac.qfiContextManagerModule parameter to point to a QfiContextManager module.");

    EV << NOW << " QoSAwareScheduler::prepareSchedule" << endl;

    grantedBytes_.clear();
    activeConnectionTempSet_ = *activeConnectionSet_;

    auto compare = [](const ScoredCid& a, const ScoredCid& b) { return a.second < b.second; };
    std::priority_queue<ScoredCid, std::vector<ScoredCid>, decltype(compare)> score(compare);

    for (const auto& cid : carrierActiveConnectionSet_) {
        EV << NOW << " QoSAwareScheduler::CID--->"<< cid << endl;
        MacNodeId nodeId = cid.getNodeId();
        grantedBytes_[cid] = 0;

        if (nodeId == NODEID_NONE || !binder_->nodeExists(nodeId)) {
            activeConnectionSet_->erase(cid);
            activeConnectionTempSet_.erase(cid);
            carrierActiveConnectionSet_.erase(cid);
            continue;
        }

        Direction dir = (direction_ == UL)
                        ? ((cid.getLcid() == D2D_SHORT_BSR) ? D2D
                          : (cid.getLcid() == D2D_MULTI_SHORT_BSR) ? D2D_MULTI
                          : UL)
                        : DL;

	// Filtering Invalid Directions in the QoS Scheduler
	if (dir != UL && dir != DL) continue;

        const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(nodeId, dir, carrierFrequency_);
        if (info.readCqiVector().empty() || info.readBands().empty()) continue;
        if (eNbScheduler_->allocatedCws(nodeId) == info.getLayers().size()) continue;

        bool cqiNull = std::any_of(info.readCqiVector().begin(), info.readCqiVector().end(), [](int cqi) { return cqi == 0; });
        if (cqiNull) continue;

        unsigned int availableBlocks = 0, availableBytes = 0;
        for (auto antenna : info.readAntennaSet()) {
            for (auto band : info.readBands()) {
                unsigned int blocks = eNbScheduler_->readAvailableRbs(nodeId, antenna, band);
                availableBlocks += blocks;
                availableBytes += eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, band, blocks, dir, carrierFrequency_);
            }
        }

        const DrbContext* ctx = getDrbContextForCid(cid);
        double qosWeight = ctx ? computeQosWeightFromContext(*ctx) : 1.0;

        EV << NOW << "QoSAwareScheduler::Cid: "<< cid << " QoS Weight: " << qosWeight<< endl;
        if (!pfRate_.count(cid)) pfRate_[cid] = 0;

        double s = 0.0;
        if (pfRate_[cid] < scoreEpsilon_)
            s = qosWeight / scoreEpsilon_;
        else if (availableBlocks > 0)
            s = qosWeight * ((availableBytes / availableBlocks) / pfRate_[cid])
                + uniform(getEnvir()->getRNG(0), -scoreEpsilon_ / 2.0, scoreEpsilon_ / 2.0);
        else
            s = 0.0;

        score.push({cid, s});
    }

    while (!score.empty()) {
        ScoredCid current = score.top();
        MacCid cid = current.first;
        bool terminate = false, active = true, eligible = true;

        unsigned int granted = requestGrant(cid, UINT32_MAX, terminate, active, eligible);
        grantedBytes_[cid] += granted;

        if (terminate) break;
        if (!active || !eligible) score.pop();
        if (!active) {
            activeConnectionTempSet_.erase(cid);
            carrierActiveConnectionSet_.erase(cid);
        }
    }
}

void QoSAwareScheduler::commitSchedule()
{
    unsigned int total = eNbScheduler_->resourceBlocks_;
    for (const auto& [cid, granted] : grantedBytes_) {
        double shortTermRate = (total > 0) ? static_cast<double>(granted) / total : 0.0;
        double& longTermRate = pfRate_[cid];
        longTermRate = (1.0 - pfAlpha_) * longTermRate + pfAlpha_ * shortTermRate;
    }
    *activeConnectionSet_ = activeConnectionTempSet_;
}

} // namespace simu5g





