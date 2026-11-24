//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.radio;

//
// The radio of a cellular PHY leg: the node-side counterpart of the radio
// medium, holding the transmitter and the receiver; its antenna is the NIC's,
// which the radios of a NIC share (antennaModule). Every frame
// the PHY transmits passes through it (the PHY says where it goes), and every
// frame that arrives for the node's PHY leg enters through it. The radio's
// receiver decides the reception, against its error model, on the SINR the
// radio medium's channel model computes.
//
// A frame arrives when its transmission starts. The radio holds it until the
// transmission ends and passes it up to the PHY then, at the frame's own
// scheduling priority; a frame that takes no air time (a beacon) is passed up
// at an event of its own, at the instant it arrives.
//
module CellularRadio
{
    parameters:
        string radioMediumModule = default("radioMedium"); // The network's radio medium
        string antennaModule = default("^.antenna"); // The antenna the radio transmits and receives through; the radios of a NIC share the NIC's
        @class(CellularRadio);
        @display("i=block/wrxtx");
    gates:
        input radioIn @directIn;    // frames from other radios
        input upperLayerIn;         // frames to transmit, from the PHY
        output upperLayerOut;       // received frames, to the PHY
    submodules:
        transmitter: CellularTransmitter {
            @display("p=100,200");
        }
        receiver: CellularReceiver {
            @display("p=100,300");
        }
}
