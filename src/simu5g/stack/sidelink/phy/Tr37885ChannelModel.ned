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

package simu5g.stack.sidelink.phy;

import simu5g.stack.phy.channelmodel.ILteChannelModel;

//
// TR 37.885 V2V sidelink channel model: highway/urban pathloss, per-pair
// log-normal shadowing, optional NLOSv vehicle blockage loss, SINR with
// co-slot/co-subchannel interference from the SL transmission map, PSCCH
// threshold decode and PSSCH decode via the per-CQI BLER curves.
//
simple Tr37885ChannelModel like ILteChannelModel
{
    parameters:
        @display("i=block/classifier2");
        string binderModule = default("binder");

        string scenario @enum(highway,urban) = default("highway");
        string losState @enum(LOS,NLOSv,NLOS) = default("LOS");   // NLOS: urban only

        bool shadowing = default(true);           // per-pair log-normal shadowing
        double noiseFigure @unit(dB) = default(9dB);
        double cableLoss @unit(dB) = default(0dB);

        // PSCCH (SCI) decode operating point (design decision D11)
        double pscchSinrThreshold @unit(dB) = default(0dB);
}
