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

#ifndef _SIDELINK_SLPATHPOLICY_H_
#define _SIDELINK_SLPATHPOLICY_H_

namespace simu5g {

/**
 * Uu/PC5 path-selection policy for unicast traffic (design decision D33,
 * SL-3): the pure decision core, unit-testable (D13). Groupcast/broadcast
 * has no Uu equivalent here and stays PC5 unconditionally under every
 * policy - the policy governs unicast only.
 *
 * One shared decision, three seams (G27): the policy instance lives in
 * SlIp2Nic; SlTechnologyDecision and SlHandoverPacketHolderUe call the
 * same SlIp2Nic::decidePath(), so the three classification points cannot
 * drift apart per packet.
 */
class SlPathPolicy
{
  public:
    enum Policy
    {
        PC5_IF_PEER,   // today's D16 rule: PC5 iff the peer is SL-capable
        UU_IF_SERVED,  // prefer Uu while attached, PC5 otherwise (the
                       // classic mode-switching study baseline)
        PC5_ONLY,      // never fall back to Uu: SL peers via PC5, anything
                       // else is denied (models a PC5-only device)
        CONDITION      // pc5Condition expr() decides (variables: tos,
                       // served, peerSlCapable)
    };

    enum Decision
    {
        PATH_PC5,      // route over the sidelink
        PATH_UU,       // route over the Uu (the base modules' rules apply)
        PATH_DENY      // route over neither: drop at the decision point
    };

    /// parse the pathSelectionPolicy parameter value (throws via caller on nullptr)
    static bool parse(const char *name, Policy& out);

    /// the pure unicast decision: peerSlCapable = destination resolves to a
    /// registered SL-capable UE; served = NR serving node != 0;
    /// conditionResult = the pc5Condition expr() value (CONDITION only)
    static Decision decideUnicast(Policy policy, bool peerSlCapable, bool served, bool conditionResult);
};

} // namespace simu5g

#endif
