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

package simu5g.stack.sidelink.ip2nic;

import simu5g.stack.ip2nic.TechnologyDecision;

//
// Technology decision for sidelink-capable UEs: PC5-destined packets bypass
// the serving-node check (an out-of-coverage UE has none); everything else
// behaves as in ~TechnologyDecision.
//
simple SlTechnologyDecision extends TechnologyDecision
{
    parameters:
        @class(SlTechnologyDecision);
}
