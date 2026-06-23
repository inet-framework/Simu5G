//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel;

//
// SionnaChannelModel
//
// A channel model that sources its propagation (per-(link, RB) path gain) from
// the ray-traced ~SionnaManager table instead of Simu5G's analytic models. It
// reuses the LteRealisticChannelModel SINR/RSRP aggregation skeleton (interference
// + noise) but overrides the propagation hooks - getAttenuation(), the
// fading/shadowing terms and the angular attenuation - so that every received
// power comes from the Sionna table (Plan A).
//
// Since the Sionna path gain already spans Tx port -> Rx port (it includes antenna
// patterns and beamforming), this model forces the antenna gains, the cable loss
// and the analytic fading/shadowing to zero to avoid double-counting.
//
simple SionnaChannelModel extends LteRealisticChannelModel
{
    parameters:
        @class("SionnaChannelModel");
        string sionnaManagerModule = default("sionnaManager");

        // The Sionna path gain already includes antenna/beamforming and the fades;
        // neutralize the analytic terms so they are not applied a second time.
        antennaGainUe = default(0dBi);
        antennGainEnB = default(0dBi);
        antennGainMicro = default(0dBi);
        cableLoss = default(0dB);
        shadowing = default(false);
}
