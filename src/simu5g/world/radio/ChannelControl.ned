//
//                  Simu5G
//
// Copyright (C) 2004 Telecommunication Networks Group (TKN) at
//              Technische Universitaet Berlin, Germany.
//              Authors: Andras Varga, Steffen Sroka, Daniel Willkomm
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.world.radio;

//
// Utility module that records the position of each radio module in the network.
// Exactly one instance of this module (or its derived extensions) must be present
// in the network.
//
// The parameters pMax, sat, alpha and carrierFrequency are not part of the
// radio propagation model -- that is the channel model of each PHY -- but they
// are used: they set the maximum interference distance, the distance at which a
// free-space transmission at pMax falls to the sat threshold. Base station
// beacons are delivered only to the radios within that distance of the sender,
// and beacons drive handover and the detection of a lost serving cell.
//
simple ChannelControl
{
    parameters:
        double pMax @unit("mW") = default(20mW); // Maximum sending power used for this network (in mW)
        double sat @unit("dBm") = default(-110dBm); // Signal attenuation threshold (in dBm)
        double alpha = default(2); // Path loss coefficient
        double carrierFrequency @unit("Hz") = default(2.4GHz); // Carrier frequency (in Hz)
        @display("i=misc/sun");
        @labels(node);
}
