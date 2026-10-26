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

import inet.physicallayer.wireless.common.radio.packetlevel.Radio;

//
// The radio of a cellular PHY leg: the node-side counterpart of the radio
// medium, holding the antenna, the transmitter and the receiver. Every frame
// the PHY transmits passes through it (the PHY says where it goes), and every
// frame that arrives for the node's PHY leg enters through it; the PHY and its
// channel models still decide everything else.
//
// A frame arrives when its transmission starts. The radio holds it until the
// transmission ends and passes it up to the PHY then, at the frame's own
// scheduling priority; a frame that takes no air time (a beacon) is passed up
// on arrival.
//
module CellularRadio extends Radio
{
    parameters:
        bool insideBuilding = default(false); // Whether the radio is inside a building; its distance from the wall (uniformly in [0, 25] m) and the random term of its building penetration loss (standard normal, scaled by the study's sigma_P) are drawn once
        // SPIKE: INET's radio delivers and times the signals, unit-disk (no physics, no draws)
        signalAnalogRepresentation = "unitDisk";
        initialRadioMode = "off"; // INET 4.7: the initial mode listens before the radio is on the medium; switched on at INITSTAGE_LAST
        allowConcurrentTransmissions = true;
        allowConcurrentReceptions = true;
        antenna.typename = "CellularAntenna";
        antenna.mobilityModule = default("^.^.^.mobility");
        transmitter.typename = "CellularTransmitter";
        receiver.typename = "CellularReceiver";
        @class(CellularRadio);
        @display("i=device/antennatower");
}
