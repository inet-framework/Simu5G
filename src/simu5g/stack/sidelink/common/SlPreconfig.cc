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

SlPreconfig::SlPreconfig()
{
    // built-in unicast SLRB template (D17): a single default UM bearer per link
    SlrbConfigEntry def;
    def.castType = SL_UNICAST;
    def.rlcType = UM;
    def.pfi = 1;
    def.isDefault = true;
    unicastSlrbDefaults.push_back(def);
}

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
    if (map->containsKey("t0"))
        t0Ms = (int)map->get("t0").doubleValueInUnit("ms");
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
    if (map->containsKey("psfchMinGap"))
        psfchMinGap = (int)map->get("psfchMinGap").intValue();
    if (map->containsKey("psfchResources"))
        psfchResources = (int)map->get("psfchResources").intValue();

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
            if (num(e.drbId) >= SL_UNICAST_DRB_BASE)
                throw cRuntimeError("SlPreconfig: slrbConfig DRB id %hu is in the dynamic unicast range "
                                    "(static entries must use ids below %hu, see D17)", num(e.drbId), SL_UNICAST_DRB_BASE);
            loadSlrbEntryShape(entry, e);
            slrbConfig.push_back(e);
        }
    }

    // CBR congestion control table (D22)
    if (map->containsKey("cbrConfig")) {
        cbrConfig.clear();
        const cValueArray *arr = check_and_cast<const cValueArray *>(map->get("cbrConfig").objectValue());
        for (int i = 0; i < (int)arr->size(); i++) {
            const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());
            SlCbrLevel level;
            level.cbrUpper = entry->get("cbrUpper").doubleValue();
            if (entry->containsKey("maxMcs"))
                level.maxMcs = (unsigned int)entry->get("maxMcs").intValue();
            if (entry->containsKey("maxNumSubchannels"))
                level.maxNumSubchannels = (int)entry->get("maxNumSubchannels").intValue();
            if (entry->containsKey("maxTxPower"))
                level.maxTxPowerDbm = entry->get("maxTxPower").doubleValueInUnit("dBm");
            if (entry->containsKey("crLimit"))
                level.crLimit = entry->get("crLimit").doubleValue();
            if (!cbrConfig.empty() && level.cbrUpper <= cbrConfig.back().cbrUpper)
                throw cRuntimeError("SlPreconfig: cbrConfig levels must have ascending cbrUpper");
            cbrConfig.push_back(level);
        }
    }

    // PQI priority overrides (WP-J)
    if (map->containsKey("pqiPriorityOverrides")) {
        pqiPriorityOverrides.clear();
        const cValueArray *arr = check_and_cast<const cValueArray *>(map->get("pqiPriorityOverrides").objectValue());
        for (int i = 0; i < (int)arr->size(); i++) {
            const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());
            pqiPriorityOverrides[(int)entry->get("pqi").intValue()] = (int)entry->get("priority").intValue();
        }
    }

    // unicastSlrbDefaults array (D17): per-link SLRB templates, one per PFI
    if (map->containsKey("unicastSlrbDefaults")) {
        unicastSlrbDefaults.clear();
        const cValueArray *arr = check_and_cast<const cValueArray *>(map->get("unicastSlrbDefaults").objectValue());
        for (int i = 0; i < (int)arr->size(); i++) {
            const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());
            if (entry->containsKey("dstL2Id") || entry->containsKey("drb"))
                throw cRuntimeError("SlPreconfig: unicastSlrbDefaults entries are per-link templates -- "
                                    "they carry no dstL2Id/drb (DRB ids are allocated at link establishment, D17)");
            SlrbConfigEntry e;
            e.castType = SL_UNICAST;
            loadSlrbEntryShape(entry, e);
            unicastSlrbDefaults.push_back(e);
        }
        if (unicastSlrbDefaults.empty())
            throw cRuntimeError("SlPreconfig: unicastSlrbDefaults must not be empty (omit the key for the built-in default)");
    }

    // validation
    if (numerologyIndex > 5)
        throw cRuntimeError("SlPreconfig: invalid numerologyIndex %u", numerologyIndex);
    if (subchannelSize <= 0 || numSubchannels <= 0)
        throw cRuntimeError("SlPreconfig: invalid subchannel geometry (%d x %d)", subchannelSize, numSubchannels);
    if (t1 < 0 || t2 <= t1)
        throw cRuntimeError("SlPreconfig: invalid selection window [%d,%d]", t1, t2);
    if (psfchPeriod != 0 && psfchPeriod != 1 && psfchPeriod != 2 && psfchPeriod != 4)
        throw cRuntimeError("SlPreconfig: psfchPeriod %d not in {0,1,2,4}", psfchPeriod);
    if (psfchMinGap < 0 || psfchResources <= 0)
        throw cRuntimeError("SlPreconfig: invalid PSFCH parameters (minGap %d, resources %d)", psfchMinGap, psfchResources);
    for (const auto& e : slrbConfig) {
        if (e.psfchMode != SL_PSFCH_OFF && e.castType != SL_GROUPCAST)
            throw cRuntimeError("SlPreconfig: psfchMode is a groupcast option (unicast SLRBs use ACK/NACK implicitly when the pool has PSFCH)");
        if (e.psfchMode == SL_PSFCH_NACK_ONLY && e.mcrMeters <= 0)
            throw cRuntimeError("SlPreconfig: groupcast option 1 (nackOnly) needs a positive mcr on the SLRB");
        if (e.psfchMode != SL_PSFCH_OFF && psfchPeriod == 0)
            throw cRuntimeError("SlPreconfig: an SLRB requests PSFCH feedback but the pool has psfchPeriod 0");
    }
}

void SlPreconfig::loadSlrbEntryShape(const cValueMap *entry, SlrbConfigEntry& e)
{
    if (entry->containsKey("rlcType"))
        e.rlcType = aToRlcType(entry->get("rlcType").stdstringValue());
    if (entry->containsKey("destAddress"))
        e.destAddress = entry->get("destAddress").stdstringValue();
    if (entry->containsKey("pfi"))
        e.pfi = (int)entry->get("pfi").intValue();
    if (entry->containsKey("pqi"))
        e.pqi = (int)entry->get("pqi").intValue();
    if (entry->containsKey("isDefault"))
        e.isDefault = entry->get("isDefault").boolValue();
    if (entry->containsKey("mcr"))
        e.mcrMeters = entry->get("mcr").doubleValueInUnit("m");
    if (entry->containsKey("psfchMode"))
        e.psfchMode = aToSlPsfchMode(entry->get("psfchMode").stdstringValue());
}

const SlCbrLevel *SlPreconfig::findCbrLevel(double cbr) const
{
    for (const auto& level : cbrConfig)
        if (cbr <= level.cbrUpper)
            return &level;
    // above the last level's cbrUpper: the strictest level keeps applying
    return cbrConfig.empty() ? nullptr : &cbrConfig.back();
}

int SlPreconfig::getPqiPriority(int pqi) const
{
    auto it = pqiPriorityOverrides.find(pqi);
    return it != pqiPriorityOverrides.end() ? it->second : slPqiToPriority(pqi);
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
