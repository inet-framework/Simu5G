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

package simu5g.nodes;

import inet.mobility.static.StationaryMobility;
import simu5g.stack.phy.radio.CellularPhantomRadio;

//
// The ExtCell module represents a fictitious cell that is used to model
// interference from neighboring cells, without actually instantiating them
// completely (i.e., without having actual UEs producing actual traffic). It can be
// configured with physical layer parameters (such as transmission power and
// direction) and the amount of resource occupation.
// Can be replaced by ~BackgroundCell modules, which provide more
// flexibility.
//
// It consists of a stationary mobility at the configured position, a phantom
// radio (~CellularPhantomRadio) with its antenna, and the allocator
// (~ExtCellAllocator), which decides the resource occupation.
//
module ExtCell
{
    parameters:
        @display("i=device/antennatower;bgb=300,300");

        string binderModule = default("binder");
        string radioMediumModule = default("radioMedium");

        // Coordinates
        double positionX @unit("m") = default(0m);
        double positionY @unit("m") = default(0m);
        double positionZ @unit("m") = default(0m);

        // Transmission power
        double txPower @unit(dBm) = default(46dBm);

        // Transmission direction
        string txDirection @enum(ANISOTROPIC,OMNI) = default("OMNI");

        // Transmission angle
        double txAngle @unit(deg) = default(0deg);

        // Operating carrier
        double carrierFrequency @unit(GHz) = default(2GHz);

        int numResourceBlocks = default(25); // Number of resource blocks of the cell, the first ones of its carrier
        int numBands = default(-1); // Renamed to numResourceBlocks; setting it is an error

        // Allocation management
        string bandAllocationType @enum(FULL_ALLOC,RANDOM_ALLOC,CONTIGUOUS_ALLOC) = default("FULL_ALLOC");
        double bandUtilization = default(0.5);
        int startingOffset = default(0);


        //# Deployment of the cell: the radio medium evaluates a link in the deployment of its base station end
        string scenario @enum(INDOOR_HOTSPOT,URBAN_MICROCELL,URBAN_MACROCELL,RURAL_MACROCELL,SUBURBAN_MACROCELL,INDOOR_FACTORY_SL,INDOOR_FACTORY_DL,INDOOR_FACTORY_SH,INDOOR_FACTORY_DH,INDOOR_FACTORY_HH) = default("URBAN_MACROCELL"); // The deployment scenario of the cell: a link is evaluated in the scenario of its base station end
        double buildingHeight @unit(m) = default(scenario == "RURAL_MACROCELL" ? 5m : scenario == "SUBURBAN_MACROCELL" ? 10m : 20m); // Average building height around the base station, for the path loss formulas that take it; by default what the studies give for the scenario: 5 m rural, 10 m suburban, 20 m urban macrocell
        double streetWidth @unit(m) = default(20m); // Average street width around the base station, for the path loss formulas that take it
        double clutterDensity = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 0.6 : 0.2); // Fraction of the floor of an indoor factory hall that the clutter (machinery, storage) covers, below 0.4 in a sparse, 0.4 or more in a dense sub-scenario
        double clutterHeight @unit(m) = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 6m : 2m); // Effective height of the clutter in an indoor factory hall, 0 to 10 m; the sub-scenario says which antennas are below it
        double clutterSize @unit(m) = default(scenario == "INDOOR_FACTORY_DL" || scenario == "INDOOR_FACTORY_DH" ? 2m : 10m); // Typical size of the clutter in an indoor factory hall
        string site = default(""); // The site of the cell: cells that give the same name are co-sited (the sectors of a site), and their links to a UE share one LOS state and one shadowing realization. Empty: the cell is a site of its own
    submodules:
        mobility: StationaryMobility {
            initFromDisplayString = false;
            // an external cell stands where it is configured to, inside or outside the area the nodes move in
            constraintAreaMinX = -inf m;
            constraintAreaMinY = -inf m;
            constraintAreaMinZ = -inf m;
            constraintAreaMaxX = inf m;
            constraintAreaMaxY = inf m;
            constraintAreaMaxZ = inf m;
            initialX = parent.positionX;
            initialY = parent.positionY;
            initialZ = parent.positionZ;
            @display("p=60,60;is=s");
        }
        radio: CellularPhantomRadio {
            radioMediumModule = parent.radioMediumModule;
            txPower = parent.txPower;
            antenna.txDirection = parent.txDirection;
            antenna.txAngle = parent.txAngle;
            antenna.gain = default(18dBi);
            antenna.mobility.typename = default("AntennaMobility");
            antenna.mobility.mobilityModule = default(absPath("^.mobility"));
            antenna.mobility.offsetZ = default(25m); // The height of the antenna above the cell's position
            noiseFigure = default(5dB);
            @display("p=60,150");
        }
        allocator: ExtCellAllocator {
            binderModule = parent.binderModule;
            carrierFrequency = parent.carrierFrequency;
            numResourceBlocks = parent.numResourceBlocks;
            numBands = parent.numBands;
            bandAllocationType = parent.bandAllocationType;
            bandUtilization = parent.bandUtilization;
            startingOffset = parent.startingOffset;
            @display("p=180,60");
        }
}
