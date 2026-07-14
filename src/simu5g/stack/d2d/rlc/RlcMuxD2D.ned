//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.d2d.rlc;

import simu5g.stack.rlc.RlcMux;

//
// Device-to-Device (D2D) aware version of ~RlcMux. It additionally handles
// D2D mode switch notifications arriving from the MAC layer, dispatching them
// to the TX or RX UM entity of the affected bearer, and declares the D2D
// statistics (emitted by the D2D UM entities). Selected by the D2D-capable
// NICs.
//
simple RlcMuxD2D extends RlcMux
{
    parameters:
        @class("RlcMuxD2D");

        //# D2D statistics (emitted by ~UmRxEntityD2D on this module)
        @signal[rlcDelayD2D];
        @statistic[rlcDelayD2D](title="Delay at the rlc layer D2D"; unit="s"; source="rlcDelayD2D"; record=mean,vector);
        @signal[rlcThroughputD2D];
        @statistic[rlcThroughputD2D](title="Throughput at the rlc layer D2D"; unit="Bps"; source="rlcThroughputD2D"; record=simu5g_rateavg,vector);

        @signal[rlcPduDelayD2D];
        @statistic[rlcPduDelayD2D](title="Delay at the rlc layer D2D"; unit="s"; source="rlcPduDelayD2D"; record=mean,vector);
        @signal[rlcPduThroughputD2D];
        @statistic[rlcPduThroughputD2D](title="Throughput at the rlc layer D2D"; unit="Bps"; source="rlcPduThroughputD2D"; record=simu5g_rateavg,vector);
}
