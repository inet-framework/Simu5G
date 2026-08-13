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
        string bearerManagementModule = default("^.rrc.bearerManagement");

        // classify unicast packets to SL-capable peers as PC5 unicast (D16):
        // a static path-selection rule; the Uu/PC5 policy hook is SL-3
        bool pc5UnicastEnabled = default(true);

        // SL-SDAP function (D20, WP-J): route classified SL packets through
        // per-destination SlSdapEntity modules (PFI -> SLRB mapping) wired
        // as a side chain on the gates below; off = SL-1 behavior (every
        // packet rides the destination's first/default SLRB)
        bool hasSlSdap = default(true);
        // PFI resolution fallback: QfiReq tag, else DSCP (tos >> 2), else 0
        bool useDscpAsPfiFallback = default(true);

    gates:
        input sdapTxIn[];
        output sdapTxOut[];
        input sdapRxIn[];
        output sdapRxOut[];
}
