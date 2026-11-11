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

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/phy/packet/AirFrame_m.h"

namespace simu5g {

namespace {

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

} // namespace

void RxTrace::open(const std::string& fileName)
{
    ASSERT(!file.is_open());
    file.open(fileName);
    if (!file.is_open())
        throw cRuntimeError("Cannot open the reception trace file '%s'", fileName.c_str());
    file << "# simu5g-rxtrace 2\n";
}

void RxTrace::close()
{
    ASSERT(innermost == nullptr);
    if (file.is_open())
        file.close();
}

void RxTrace::write(const std::string& line)
{
    file << line << '\n';
}

void RxTrace::addInterferer(const cModule *module)
{
    if (innermost != nullptr && innermost->isCollecting())
        innermost->addInterferer(module != nullptr ? nameOf(module) : std::string("?"));
}

std::string RxTrace::nameOf(const cModule *module)
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

std::string RxTrace::nameOf(Binder *binder, MacNodeId id)
{
    if (!binder->nodeExists(id))
        return "node" + std::to_string(num(id));
    return nameOf(binder->getModuleByMacNodeId(id));
}

const char *RxTrace::ratOf(MacNodeId ueId)
{
    return isNrUe(ueId) ? "NR" : "LTE";
}

RxTrace::Evaluation::Evaluation(RxTrace& trace, bool collectsInterferers) : trace(trace), collects(collectsInterferers)
{
    if (!trace.isEnabled())
        return;
    active = true;
    outer = trace.innermost;
    trace.innermost = this;
}

RxTrace::Evaluation::~Evaluation()
{
    // evaluations are scoped objects, so they end in reverse order
    if (active) {
        ASSERT(trace.innermost == this);
        trace.innermost = outer;
    }
}

std::string RxTrace::Evaluation::interferersText() const
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

RxTrace::Reception::Reception(RxTrace& trace, const cModule *receiver, Binder *binder, const UserControlInfo *lteInfo, const AirFrame *frame)
    : Evaluation(trace, true), lteInfo(lteInfo)
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
        + " " + dirToA(dir) + " cw" + std::to_string(cw) + " start=" + (frame != nullptr ? formatTime(frame->getReceptionStartTime()) : std::string("-")) + " cqi=" + std::to_string(cqi)
        + " h=" + std::to_string((int)lteInfo->getAcid()) + "/" + std::to_string((int)lteInfo->getTxNumber());
}

void RxTrace::Reception::setSinr(const std::vector<double>& sinrPerBand)
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

void RxTrace::Reception::setPer(double per)
{
    if (active)
        perText = format(per);
}

void RxTrace::Reception::setDraw(double draw, uint64_t numbersDrawnBefore)
{
    if (!active)
        return;
    drawText = format(draw);
    rngText = std::to_string(numbersDrawnBefore);
}

bool RxTrace::Reception::finish(bool success)
{
    if (active)
        write(success ? "OK" : "ERR");
    return success;
}

bool RxTrace::Reception::finishWithoutDraw()
{
    if (active)
        write("ERR(nodraw)");
    return false;
}

void RxTrace::Reception::write(const std::string& outcome)
{
    trace.write(formatTime(simTime()) + " RX " + key + " sinr=" + sinrText + " itf=" + interferersText() + " per=" + perText
            + " draw=" + drawText + " rng=" + rngText + " " + outcome);
}

RxTrace::WhatIf::WhatIf(RxTrace& trace, const char *kind, GHz carrierFrequency, Direction dir, bool collectsInterferers)
    : Evaluation(trace, collectsInterferers), kind(kind), carrierFrequency(carrierFrequency), dir(dir)
{
}

void RxTrace::WhatIf::setLink(const std::string& a, const std::string& b, const char *rat)
{
    link = a + ">" + b + " " + formatMHz(carrierFrequency) + " " + rat;
}

void RxTrace::WhatIf::finish(const std::vector<double>& valuePerBand)
{
    if (!active)
        return;
    std::vector<BandRun> runs;
    for (unsigned int band = 0; band < valuePerBand.size(); band++)
        appendBand(runs, band, 1, valuePerBand[band]);
    trace.write(formatTime(simTime()) + " WI " + kind + " " + link + " " + dirToA(dir) + " values=" + formatRuns(runs) + " itf=" + interferersText());
}

} // namespace simu5g
