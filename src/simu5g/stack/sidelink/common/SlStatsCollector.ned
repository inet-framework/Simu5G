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

package simu5g.stack.sidelink.common;

//
// Optional network-level collector of the TR 37.885 application-level V2X
// metrics (PRR and PIR per transmitter-receiver distance bin). Declare it as
// a submodule named "slStatsCollector" of the network; sidelink PHYs find it
// by that name and report transmissions and deliveries. Results are recorded
// as scalars at the end of the simulation.
//
simple SlStatsCollector
{
    parameters:
        @display("i=block/cogwheel");
        double binSize @unit(m) = default(20m);
        double maxDistance @unit(m) = default(500m);
}
