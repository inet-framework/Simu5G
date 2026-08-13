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

#include "simu5g/stack/sidelink/mac/SlMcsTable.h"

#include <algorithm>
#include <cmath>

#include <omnetpp.h>

#include "simu5g/stack/mac/amc/NrMcs.h"  // nInfoToTbs[] (referenced, not copied)

namespace simu5g {

using namespace omnetpp;

// TS 38.214 table 5.1.3.1-1 (MCS index table 1 for PDSCH, also the PSSCH
// table per TS 38.214 §8.1.3.1): MCS index -> (Qm, R x 1024)
static const SlMcsTable::Entry mcsTable1[SlMcsTable::MAX_MCS + 1] = {
    { 2, 120.0 }, { 2, 157.0 }, { 2, 193.0 }, { 2, 251.0 }, { 2, 308.0 },
    { 2, 379.0 }, { 2, 449.0 }, { 2, 526.0 }, { 2, 602.0 }, { 2, 679.0 },
    { 4, 340.0 }, { 4, 378.0 }, { 4, 434.0 }, { 4, 490.0 }, { 4, 553.0 },
    { 4, 616.0 }, { 4, 658.0 },
    { 6, 438.0 }, { 6, 466.0 }, { 6, 517.0 }, { 6, 567.0 }, { 6, 616.0 },
    { 6, 666.0 }, { 6, 719.0 }, { 6, 772.0 }, { 6, 822.0 }, { 6, 873.0 },
    { 6, 910.0 }, { 6, 948.0 },
};

// Coarse MCS->CQI map: for each MCS of table 1, the largest CQI of TS 38.214
// table 7.2.3-1 whose spectral efficiency (Qm x R) does not exceed the MCS's
// (rule-derived, kept static for auditability)
static const unsigned int mcsToCqi[SlMcsTable::MAX_MCS + 1] = {
    2, 2, 3, 3, 4, 4, 5, 5, 6, 6,          // QPSK
    6, 7, 7, 8, 8, 9, 9,                   // 16QAM
    9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15,  // 64QAM
};

const SlMcsTable::Entry& SlMcsTable::entry(unsigned int mcs)
{
    if (mcs > MAX_MCS)
        throw cRuntimeError("SlMcsTable: MCS index %u out of range (table 1 has 0..%u)", mcs, MAX_MCS);
    return mcsTable1[mcs];
}

unsigned int SlMcsTable::cqi(unsigned int mcs)
{
    if (mcs > MAX_MCS)
        throw cRuntimeError("SlMcsTable: MCS index %u out of range (table 1 has 0..%u)", mcs, MAX_MCS);
    return mcsToCqi[mcs];
}

unsigned int SlMcsTable::tbsBits(unsigned int mcs, unsigned int numPrbs, unsigned int overheadSymbols)
{
    if (overheadSymbols >= 14)
        throw cRuntimeError("SlMcsTable: overheadSymbols %u leaves no data symbols in the slot", overheadSymbols);

    const Entry& e = entry(mcs);
    // N'_RE per PRB, capped at 156 like the TS 38.214 §5.1.3.2 procedure
    unsigned int rePerPrb = std::min(12u * (14u - overheadSymbols), 156u);
    double nRe = (double)rePerPrb * numPrbs;
    double codeRate = e.codeRateX1024 / 1024.0;
    double nInfo = nRe * codeRate * e.qm;  // single layer, one codeword

    return computeTbsFromNinfo(std::floor(nInfo), codeRate);
}

unsigned int SlMcsTable::tbsBytes(unsigned int mcs, unsigned int numPrbs, unsigned int overheadSymbols)
{
    return tbsBits(mcs, numPrbs, overheadSymbols) / 8;
}

unsigned int SlMcsTable::computeTbsFromNinfo(double nInfo, double codeRate)
{
    // TS 38.214 §5.1.3.2 steps 3/4 (real-valued arithmetic throughout)
    if (nInfo <= 0)
        return 0;

    if (nInfo <= 3824) {
        int n = std::max(3, (int)std::floor(std::log2(nInfo)) - 6);
        double quantized = std::max(24.0, std::exp2(n) * std::floor(nInfo / std::exp2(n)));
        for (unsigned int j = 0; j < TBSTABLESIZE; j++)
            if (nInfoToTbs[j] >= quantized)
                return nInfoToTbs[j];
        return nInfoToTbs[TBSTABLESIZE - 1];
    }

    int n = (int)std::floor(std::log2(nInfo - 24)) - 5;
    double quantized = std::exp2(n) * std::round((nInfo - 24) / std::exp2(n));
    double C;
    if (codeRate <= 0.25)
        C = std::ceil((quantized + 24) / 3816);
    else if (quantized >= 8424)
        C = std::ceil((quantized + 24) / 8424);
    else
        C = 1;
    return (unsigned int)(8 * C * std::ceil((quantized + 24) / (8 * C)) - 24);
}

} // namespace simu5g
