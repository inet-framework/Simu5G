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

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

const std::string slCastTypeToA(SlCastType c)
{
    switch (c) {
        case SL_BROADCAST: return "broadcast";
        case SL_GROUPCAST: return "groupcast";
        case SL_UNICAST: return "unicast";
        default: return "unknown";
    }
}

SlCastType aToSlCastType(const std::string& s)
{
    if (s == "broadcast") return SL_BROADCAST;
    if (s == "groupcast") return SL_GROUPCAST;
    if (s == "unicast") return SL_UNICAST;
    throw cRuntimeError("Unknown sidelink cast type: '%s' (expected broadcast/groupcast/unicast)", s.c_str());
}

const std::string slPsfchModeToA(SlPsfchMode m)
{
    switch (m) {
        case SL_PSFCH_OFF: return "off";
        case SL_PSFCH_NACK_ONLY: return "nackOnly";
        case SL_PSFCH_ACK_NACK: return "ackNack";
        default: return "unknown";
    }
}

SlPsfchMode aToSlPsfchMode(const std::string& s)
{
    if (s == "off") return SL_PSFCH_OFF;
    if (s == "nackOnly") return SL_PSFCH_NACK_ONLY;
    if (s == "ackNack") return SL_PSFCH_ACK_NACK;
    throw cRuntimeError("Unknown PSFCH mode: '%s' (expected off/nackOnly/ackNack)", s.c_str());
}

int slPqiToPriority(int pqi)
{
    // abstracted subset of TS 23.287 table 5.4.4-1 (standardized PQIs)
    switch (pqi) {
        case 21: return 1;   // GBR, platooning high (delay budget 20 ms)
        case 22: return 2;   // GBR, sensor sharing
        case 23: return 3;   // GBR, video between vehicles
        case 1: return 3;    // non-GBR, safety broadcast (CAM/DENM class)
        case 2: return 4;
        case 3: return 3;
        case 55: return 3;   // delay-critical GBR
        case 56: return 6;
        case 57: return 5;
        case 58: return 4;
        case 59: return 6;
        case 90: return 25;  // non-GBR best effort
        default: return SL_PQI_PRIORITY_DEFAULT;
    }
}

} // namespace simu5g
