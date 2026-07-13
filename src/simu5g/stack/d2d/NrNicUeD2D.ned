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

import simu5g.stack.NrNicUe;

//
// Device-to-Device (D2D) capable NR User Equipment network interface. It extends
// ~NrNicUe by swapping in the D2D-capable NR MAC/PHY leaves and enabling the D2D
// mode controller, allowing direct UE-to-UE communication without routing traffic
// through the gNodeB. It is the NR counterpart of ~LteNicUeD2D.
//
module NrNicUeD2D extends NrNicUe
{
    parameters:
        // NR stack: use the D2D-capable MAC/PHY leaves
        nrMac.typename = default("NrMacUeD2D");
        nrPhy.typename = default("NrPhyUeD2D");

        // LTE leg: keep parity with the pre-separation NrNicUe, whose LTE leg
        // ran the D2D-capable LTE MAC and the then-D2D-capable NrPhyUe (which
        // carried the averageCqiD2D statistic)
        mac.typename = default("LteMacUeD2D");
        phy.typename = default("NrPhyUeD2D");
        ip2nic.typename = default("Ip2NicD2D");

        // both legs previously ran with the NIC-wide D2D support enabled
        rlcMux.typename = default("RlcMuxD2D");
        nrRlcMux.typename = default("RlcMuxD2D");

        // use the D2D-capable channel models on both legs (matching the
        // pre-separation defaults: LteRealisticChannelModel on the LTE leg,
        // NrChannelModel_3GPP38_901 on the NR leg)
        lteChannelModelType = default("D2dRealisticChannelModel");
        nrChannelModelType = default("D2dNrChannelModel_3GPP38_901");

        rrc.typename = default("RrcD2D");
        rrc.handoverController.typename = default("HandoverControllerD2D");
        rrc.nrHandoverController.typename = default("HandoverControllerD2D");
        rrc.bearerManagement.rlcUmTxEntityModuleType = default("simu5g.stack.d2d.rlc.UmTxEntityD2D");
        rrc.bearerManagement.rlcUmRxEntityModuleType = default("simu5g.stack.d2d.rlc.UmRxEntityD2D");
        bool d2dInitialMode = default(false);
}
