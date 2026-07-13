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

#include "simu5g/stack/sidelink/common/SlPreconfig.h"

#include <omnetpp/cvaluearray.h>
#include <omnetpp/cvaluemap.h>

using namespace omnetpp;

namespace simu5g {

void SlPreconfig::loadFromJson(const cValueMap *map)
{
    // pool geometry
    if (map->containsKey("carrierFrequency"))
        carrierFrequencyGHz = map->get("carrierFrequency").doubleValueInUnit("GHz");
    if (map->containsKey("numerologyIndex"))
        numerologyIndex = (unsigned int)map->get("numerologyIndex").intValue();
    if (map->containsKey("subchannelSize"))
        subchannelSize = (int)map->get("subchannelSize").intValue();
    if (map->containsKey("numSubchannels"))
        numSubchannels = (int)map->get("numSubchannels").intValue();
    if (map->containsKey("slotBitmap"))
        slotBitmap = map->get("slotBitmap").stdstringValue();

    // mode-2 selection parameters
    if (map->containsKey("t1"))
        t1 = (int)map->get("t1").intValue();
    if (map->containsKey("t2"))
        t2 = (int)map->get("t2").intValue();
    if (map->containsKey("rsrpThresholdDbm"))
        rsrpThresholdDbm = map->get("rsrpThresholdDbm").doubleValue();
    if (map->containsKey("reservationPeriodsMs")) {
        reservationPeriodsMs.clear();
        const cValueArray *arr = check_and_cast<const cValueArray *>(map->get("reservationPeriodsMs").objectValue());
        for (int i = 0; i < (int)arr->size(); i++)
            reservationPeriodsMs.push_back((int)arr->get(i).intValue());
    }
    if (map->containsKey("blindRetx"))
        blindRetx = (int)map->get("blindRetx").intValue();
    if (map->containsKey("psfchPeriod"))
        psfchPeriod = (int)map->get("psfchPeriod").intValue();

    // slrbConfig array (D12)
    if (map->containsKey("slrbConfig")) {
        slrbConfig.clear();
        const cValueArray *arr = check_and_cast<const cValueArray *>(map->get("slrbConfig").objectValue());
        for (int i = 0; i < (int)arr->size(); i++) {
            const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());
            SlrbConfigEntry e;
            e.dstL2Id = (SlL2Id)entry->get("dstL2Id").intValue();
            if (entry->containsKey("castType"))
                e.castType = aToSlCastType(entry->get("castType").stdstringValue());
            e.drbId = DrbId(entry->get("drb").intValue());
            if (entry->containsKey("rlcType"))
                e.rlcType = aToRlcType(entry->get("rlcType").stdstringValue());
            if (entry->containsKey("destAddress"))
                e.destAddress = entry->get("destAddress").stdstringValue();
            slrbConfig.push_back(e);
        }
    }

    // validation
    if (numerologyIndex > 5)
        throw cRuntimeError("SlPreconfig: invalid numerologyIndex %u", numerologyIndex);
    if (subchannelSize <= 0 || numSubchannels <= 0)
        throw cRuntimeError("SlPreconfig: invalid subchannel geometry (%d x %d)", subchannelSize, numSubchannels);
    if (t1 < 0 || t2 <= t1)
        throw cRuntimeError("SlPreconfig: invalid selection window [%d,%d]", t1, t2);
}

const SlrbConfigEntry *SlPreconfig::findSlrbForDstL2Id(SlL2Id dstL2Id) const
{
    for (const auto& e : slrbConfig)
        if (e.dstL2Id == dstL2Id)
            return &e;
    return nullptr;
}

const SlrbConfigEntry *SlPreconfig::findSlrbForDestAddress(const std::string& addr) const
{
    for (const auto& e : slrbConfig)
        if (e.destAddress == addr)
            return &e;
    return nullptr;
}

} // namespace simu5g
