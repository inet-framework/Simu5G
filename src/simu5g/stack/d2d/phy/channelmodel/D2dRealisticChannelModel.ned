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

package simu5g.stack.d2d.phy.channelmodel;

import simu5g.stack.phy.channelmodel.LteRealisticChannelModel;

//
// D2D-capable LTE realistic channel model: ~LteRealisticChannelModel plus the
// device-to-device channel math (RSRP/SINR, D2D interference, D2D reception
// decision). It is the default LTE channel model of the D2D NICs.
//
simple D2dRealisticChannelModel extends LteRealisticChannelModel
{
    parameters:
        @class("D2dRealisticChannelModel");

        // if true, enables the interference computation for D2D connections
        bool d2dInterference = default(true);

        // statistics
        @signal[rcvdSinrD2D];
        @statistic[rcvdSinrD2D](title="SINR measured at packet reception, D2D"; unit="dB"; source="rcvdSinrD2D"; record=mean,vector);
}
