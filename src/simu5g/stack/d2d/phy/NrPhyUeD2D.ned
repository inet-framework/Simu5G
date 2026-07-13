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

package simu5g.stack.d2d.phy;

import simu5g.stack.phy.NrPhyUe;

//
// NR-UE PHY with D2D support. Extends ~NrPhyUe with the device-to-device (D2D)
// deltas: it supports the transmission and reception of frames to another UE
// directly (including one-to-many D2D multicast) without going through the
// serving gNodeB. It is the NR counterpart of ~LtePhyUeD2D.
//
simple NrPhyUeD2D extends NrPhyUe
{
    parameters:
        @class("NrPhyUeD2D");
        double d2dTxPower @unit(dBm) = default(26dBm);
        bool d2dMulticastCaptureEffect = default(true);
        string d2dMulticastCaptureEffectFactor = default("RSRP");  // or distance

        //# D2D CQI statistic
        @signal[averageCqiD2D];
        @statistic[averageCqiD2D](title="Average Cqi reported in D2D"; unit="cqi"; source="averageCqiD2D"; record=mean,vector);
}
