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
        string scenario @enum(INDOOR_HOTSPOT,URBAN_MICROCELL,URBAN_MACROCELL,RURAL_MACROCELL,SUBURBAN_MACROCELL,INDOOR_FACTORY_SL,INDOOR_FACTORY_DL,INDOOR_FACTORY_SH,INDOOR_FACTORY_DH,INDOOR_FACTORY_HH) = default("URBAN_MACROCELL"); // The deployment scenario of the cell, which counts on a base station's radio: a link is evaluated in the scenario of its base station end
        double buildingHeight @unit(m) = default(scenario == "RURAL_MACROCELL" ? 5m : scenario == "SUBURBAN_MACROCELL" ? 10m : 20m); // Average building height around the base station, for the path loss formulas that take it; by default what the studies give for the scenario: 5 m rural, 10 m suburban, 20 m urban macrocell
        double streetWidth @unit(m) = default(20m); // Average street width around the base station, for the path loss formulas that take it
        double clutterDensity = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 0.6 : 0.2); // Fraction of the floor of an indoor factory hall that the clutter (machinery, storage) covers, which counts on a base station's radio: below 0.4 in a sparse, 0.4 or more in a dense sub-scenario
        double clutterHeight @unit(m) = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 6m : 2m); // Effective height of the clutter in an indoor factory hall, 0 to 10 m; the sub-scenario says which antennas are below it
        double clutterSize @unit(m) = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 2m : 10m); // Typical size of the clutter in an indoor factory hall
        string site = default(""); // The site of the cell, which counts on a base station's radio: cells that give the same name are co-sited (the sectors of a site), and their links to a UE share one LOS state and one shadowing realization. Empty: the cell is a site of its own
        bool insideBuilding = default(false); // Whether the radio is inside a building, which counts on a UE's radio: a link's building penetration loss is its UE end's. Its distance from the wall (uniformly in [0, 25] m) and the random term of its penetration loss (standard normal, scaled by the study's sigma_P) are drawn once
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
