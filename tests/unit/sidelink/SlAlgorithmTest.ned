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

@namespace(simu5g);

//
// Runs the deterministic sidelink algorithm unit tests in initialize().
//
simple SlAlgorithmTest
{
    parameters:
        @display("i=block/cogwheel");
}

network SlUnitTests
{
    submodules:
        test: SlAlgorithmTest;
}
