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

import simu5g.stack.ip2nic.HandoverPacketHolderUe;

//
// Handover packet holder for sidelink-capable UEs: PC5-destined packets are
// deliverable without any serving node.
//
simple SlHandoverPacketHolderUe extends HandoverPacketHolderUe
{
    parameters:
        @class(SlHandoverPacketHolderUe);

        // the SlIp2Nic owning the shared path decision (D33/G27); its
        // pc5UnicastEnabled/pathSelectionPolicy govern this seam too
        string slIp2NicModule = default("^.ip2nic");
}
