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

package simu5g.stack.d2d;

import simu5g.stack.NrNicEnb;

//
// Device-to-Device (D2D) capable NR gNodeB network interface. It extends
// ~NrNicEnb by swapping in the D2D-capable NR MAC leaf and the D2D-capable eNB
// PHY, and by enabling the D2D mode controller and mode selection. It is the NR
// counterpart of ~LteNicEnbD2D.
//
module NrNicEnbD2D extends NrNicEnb
{
    parameters:
        mac.typename = default("NrMacGnbD2D");
        phy.typename = default("LtePhyEnbD2D");
        ip2nic.typename = default("Ip2NicD2D");
        rlcMux.typename = default("RlcMuxD2D");

        // use the D2D-capable NR channel model (matching the NrNicEnb default of
        // NrChannelModel_3GPP38_901 on the gNodeB's channelModel slot)
        lteChannelModelType = default("D2dNrChannelModel_3GPP38_901");
        rrc.hasD2DModeController = true;
        rrc.hasD2DModeSelection = true;
        rrc.bearerManagement.rlcUmTxEntityModuleType = default("simu5g.stack.d2d.rlc.UmTxEntityD2D");
        rrc.bearerManagement.rlcUmRxEntityModuleType = default("simu5g.stack.d2d.rlc.UmRxEntityD2D");
}
