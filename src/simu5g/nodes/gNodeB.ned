//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.nodes;

import simu5g.nodes.eNodeB;

//
// Implements a gNodeB for a 5G network. It extends the ~eNodeB module
// by providing 5G connectivity to ~NrUe modules. It can be used in either StandAlone (SA) or
// Non-StandAlone (NSA) 5G deployments. In the former case, it is directly connected to a ~Upf
// module. In the latter case, this module can be configured as a secondary node of
// a ~eNodeB module, thus enabling E-UTRA Dual Connectivity (EN-DC).
//
module gNodeB extends eNodeB
{
    parameters:
        @networkNode();
        @display("i=device/antennatower;bgb=1260,600");
        collector.typename = default("GNodeBStatsCollector");
        gateway = default("upf"); // change this path if the gateway node is inside a compound module
        nodeType = "GNODEB";        // DO NOT CHANGE

        //# gNB-side sidelink support (SL-3): selects the SL-aware NIC variant
        //# (cell pool owner + Mode-1 scheduling). A combined D2D+SL gNB is not
        //# supported (hasD2D takes precedence; SlGnbRrc rejects it at init).
        //# An explicit cellularNic.typename in the ini file still overrides this.
        bool hasSidelink = default(false);
        cellularNic.typename = default(hasD2D ? "NrNicEnbD2D" : (hasSidelink ? "simu5g.stack.sidelink.NrNicEnbSl" : "NrNicEnb"));
}
