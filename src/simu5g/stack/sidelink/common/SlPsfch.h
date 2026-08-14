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

#ifndef _SIDELINK_SLPSFCH_H_
#define _SIDELINK_SLPSFCH_H_

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

//
// PSFCH timing and resource math (design decision D19), as pure functions
// (unit-tested in the D13 suite).
//
// Timing: a PSSCH transmission in slot n is acknowledged in the first PSFCH
// slot >= n + psfchMinGap, where PSFCH slots are those with
// slot % psfchPeriod == 0 (pool psfchPeriod in {1,2,4}; 0 keeps PSFCH off).
//
// Resource model (documented abstraction -- no PRB mapping, no sequence-based
// code multiplexing): each PSFCH slot offers psfchResources indices; a
// feedback transmission occupies one, derived deterministically from the
// acknowledged PSSCH's (slot, first subchannel) plus, for groupcast option 2,
// the responding member's index. Two feedbacks on the same
// (slot, resourceIndex) interfere.
//

/// the absolute slot in which the feedback for a PSSCH in psschSlot is sent
inline SlotIndex slPsfchFeedbackSlot(SlotIndex psschSlot, int psfchPeriod, int psfchMinGap)
{
    SlotIndex earliest = psschSlot + psfchMinGap;
    SlotIndex rem = earliest % psfchPeriod;
    return (rem == 0) ? earliest : earliest + (psfchPeriod - rem);
}

/// the PSFCH resource index used by the feedback for a PSSCH transmission;
/// memberIndex differentiates groupcast option-2 responders (0 otherwise)
inline int slPsfchResourceIndex(SlotIndex psschSlot, int firstSubchannel, int poolNumSubchannels,
        int memberIndex, int psfchResources)
{
    return (int)((psschSlot * poolNumSubchannels + firstSubchannel + memberIndex) % psfchResources);
}

} // namespace simu5g

#endif
