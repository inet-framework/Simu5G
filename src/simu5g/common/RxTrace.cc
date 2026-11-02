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

#include "simu5g/common/RxTrace.h"

#include <charconv>
#include <cstring>
#include <fstream>

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

Register_PerRunConfigOption(CFGID_SIMU5G_RXTRACE_FILE, "simu5g-rxtrace-file", CFG_FILENAME, "", "When set, Simu5G writes one line per reception decision and per channel evaluation that feeds a decision (CQI, RSRP, beacon) into this file, for comparing radio-model versions transmission by transmission; see RxTrace.h for the format.");

namespace rxtrace {

namespace {

std::ofstream *file = nullptr;
Evaluation *innermost = nullptr;
Reception *currentReception = nullptr;

class Lifecycle : public cISimulationLifecycleListener
{
  protected:
    void close()
    {
        if (file != nullptr) {
            file->close();
            delete file;
            file = nullptr;
        }
    }

    void lifecycleEvent(SimulationLifecycleEventType eventType, cObject *details) override
    {
        switch (eventType) {
            case LF_PRE_NETWORK_SETUP: {
                close();
                std::string fileName = getEnvir()->getConfig()->getAsFilename(CFGID_SIMU5G_RXTRACE_FILE);
                if (!fileName.empty()) {
                    file = new std::ofstream(fileName);
                    if (!file->is_open())
                        throw cRuntimeError("Cannot open the reception trace file '%s'", fileName.c_str());
                    *file << "# simu5g-rxtrace 1\n";
                }
                break;
            }
            case LF_ON_RUN_END:
            case LF_PRE_NETWORK_DELETE:
                close();
                break;
            default:
                break;
        }
    }
};

EXECUTE_ON_STARTUP(getEnvir()->addLifecycleListener(new Lifecycle()));

std::string format(double value)
{
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

bool sameBits(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

// a run of adjacent bands with one value and one allocation
struct BandRun
{
    unsigned int first, last, allocation;
    double value;
};

std::string formatRuns(const std::vector<BandRun>& runs)
{
    std::string text = "[";
    for (size_t i = 0; i < runs.size(); i++) {
        const BandRun& run = runs[i];
        if (i > 0)
            text += ",";
        text += std::to_string(run.first);
        if (run.last != run.first)
            text += "-" + std::to_string(run.last);
        if (run.allocation != 1)
            text += "x" + std::to_string(run.allocation);
        text += ":" + format(run.value);
    }
    return text + "]";
}

void appendBand(std::vector<BandRun>& runs, unsigned int band, unsigned int allocation, double value)
{
    if (!runs.empty()) {
        BandRun& last = runs.back();
        if (last.last + 1 == band && last.allocation == allocation && sameBits(last.value, value)) {
            last.last = band;
            return;
        }
    }
    runs.push_back({band, band, allocation, value});
}

std::string formatTime(simtime_t t)
{
    return t.str();
}

std::string formatMHz(GHz carrierFrequency)
{
    return format(carrierFrequency.get() * 1000) + "MHz";
}

bool isBackgroundUeLevel(const cModule *module)
{
    const char *type = module->getNedTypeName();
    size_t length = strlen(type);
    auto endsWith = [&] (const char *suffix) {
        size_t n = strlen(suffix);
        return length >= n && strcmp(type + length - n, suffix) == 0 && (length == n || type[length - n - 1] == '.');
    };
    return endsWith("BackgroundUe") || endsWith("BackgroundTrafficGenerator");
}

void write(const std::string& line)
{
    *file << line << '\n';
}

} // namespace

bool isEnabled()
{
    return file != nullptr;
}

bool isCollecting()
{
    return innermost != nullptr && innermost->isCollecting();
}

std::string nameOf(const cModule *module)
{
    std::string inner;
    const cModule *node = module;
    while (node->getParentModule() != nullptr && node->getParentModule()->getParentModule() != nullptr) {
        if (isBackgroundUeLevel(node))
            inner = "/" + std::string(node->getFullName()) + inner;
        node = node->getParentModule();
    }
    return node->getFullName() + inner;
}

std::string nameOf(Binder *binder, MacNodeId id)
{
    if (!binder->nodeExists(id))
        return "node" + std::to_string(num(id));
    return nameOf(binder->getModuleByMacNodeId(id));
}

const char *ratOf(MacNodeId ueId)
{
    return isNrUe(ueId) ? "NR" : "LTE";
}

void addInterferer(const std::string& name)
{
    ASSERT(isCollecting());
    innermost->add(name);
}

Evaluation::Evaluation(bool collectsInterferers) : collects(collectsInterferers)
{
    if (!isEnabled())
        return;
    active = true;
    outer = innermost;
    innermost = this;
}

Evaluation::~Evaluation()
{
    // evaluations are scoped objects, so they end in reverse order
    if (active)
        innermost = outer;
}

std::string Evaluation::interferersText() const
{
    if (!collects)
        return "-";
    std::string text = "{";
    for (const auto& name : interferers) {
        if (text.size() > 1)
            text += ",";
        text += name;
    }
    return text + "}";
}

Reception::Reception(const cModule *receiver, Binder *binder, const UserControlInfo *lteInfo, simtime_t startTime)
    : lteInfo(lteInfo)
{
    if (!active)
        return;
    Direction dir = lteInfo->getDirection();
    MacNodeId ueId = dir == DL ? lteInfo->getDestId() : lteInfo->getSourceId();
    int cw = lteInfo->getCw();
    int cqi = -1;
    if (lteInfo->getUserTxParams() != nullptr) {
        const auto& cqis = lteInfo->getUserTxParams()->readCqiVector();
        if (cqis.size() == 1)
            cw = 0;
        if ((size_t)cw < cqis.size())
            cqi = cqis[cw];
    }
    key = nameOf(binder, lteInfo->getSourceId()) + ">" + nameOf(receiver) + " " + formatMHz(lteInfo->getCarrierFrequency()) + " " + ratOf(ueId)
        + " " + dirToA(dir) + " cw" + std::to_string(cw) + " start=" + formatTime(startTime) + " cqi=" + std::to_string(cqi)
        + " h=" + std::to_string((int)lteInfo->getAcid()) + "/" + std::to_string((int)lteInfo->getTxNumber());
    currentReception = this;
}

Reception::~Reception()
{
    if (active && currentReception == this)
        currentReception = nullptr;
}

void Reception::setSinr(const std::vector<double>& sinrPerBand)
{
    if (!active)
        return;
    std::vector<BandRun> runs;
    for (const auto& [remote, bands] : lteInfo->getGrantedBlocks())
        for (const auto& [band, allocation] : bands)
            if (allocation != 0)
                appendBand(runs, band, allocation, band < sinrPerBand.size() ? sinrPerBand[band] : NAN);
    sinrText = formatRuns(runs);
}

void Reception::setPer(double per)
{
    if (active)
        perText = format(per);
}

void Reception::setDraw(double draw, uint64_t numbersDrawnBefore)
{
    if (!active)
        return;
    drawText = format(draw);
    rngText = std::to_string(numbersDrawnBefore);
}

bool Reception::finish(bool success, const char *reasonWithoutDraw)
{
    if (!active)
        return success;
    std::string outcome = success ? "OK" : "ERR";
    if (reasonWithoutDraw == nullptr)
        reasonWithoutDraw = noDrawReason;
    if (reasonWithoutDraw != nullptr)
        outcome += std::string("(") + reasonWithoutDraw + ")";
    write(formatTime(simTime()) + " RX " + key + " sinr=" + sinrText + " itf=" + interferersText() + " per=" + perText
            + " draw=" + drawText + " rng=" + rngText + " " + outcome);
    return success;
}

Reception *getCurrentReception()
{
    return currentReception;
}

WhatIf::WhatIf(const char *kind, GHz carrierFrequency, Direction dir, bool collectsInterferers)
    : Evaluation(collectsInterferers), kind(kind), carrierFrequency(carrierFrequency), dir(dir)
{
}

void WhatIf::setLink(const std::string& a, const std::string& b, const char *rat)
{
    link = a + ">" + b + " " + formatMHz(carrierFrequency) + " " + rat;
}

void WhatIf::finish(const std::vector<double>& valuePerBand)
{
    if (!active)
        return;
    std::vector<BandRun> runs;
    for (unsigned int band = 0; band < valuePerBand.size(); band++)
        appendBand(runs, band, 1, valuePerBand[band]);
    write(formatTime(simTime()) + " WI " + kind + " " + link + " " + dirToA(dir) + " values=" + formatRuns(runs) + " itf=" + interferersText());
}

} // namespace rxtrace

} // namespace simu5g
