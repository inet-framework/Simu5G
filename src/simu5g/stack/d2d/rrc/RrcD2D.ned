//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.d2d.rrc;

import simu5g.stack.rrc.Rrc;

//
// D2D-capable variant of the ~Rrc layer. It adds the D2D mode controller
// (which coordinates RLC TX-queue draining across a mode switch) and, on the
// eNB/gNB side (hasD2DModeSelection==true), the periodic D2D mode selection
// module. Selected via 'rrc.typename' by the D2D-capable NICs.
//
module RrcD2D extends Rrc
{
    parameters:
        bool hasD2DModeSelection = default(false);

    submodules:
        d2dModeController: D2DModeController {
            @display("p=400,80");
        }
        d2dModeSelection: <default("D2dModeSelectionBase")> like ID2DModeSelection if hasD2DModeSelection {
            @display("p=400,160");
        }
}
