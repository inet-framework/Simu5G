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

import simu5g.stack.ip2nic.Ip2Nic;

//
// IP-to-NIC bridge for sidelink-capable UEs: PC5-destined packets are
// classified as SL flows and their SLRB chains are genie-established on
// first use; everything else behaves as in ~Ip2Nic.
//
simple SlIp2Nic extends Ip2Nic
{
    parameters:
        @class(SlIp2Nic);
        string slRrcModule = default("^.slRrc");
}
