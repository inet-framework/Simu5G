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

import simu5g.stack.rrc.HandoverController;

//
// D2D-capable variant of the ~HandoverController. It fills in the handover
// lifecycle hooks with D2D behavior: switching active D2D flows back to
// infrastructure mode before a handover, detaching/attaching the D2D AMC
// direction while executing it, and re-selecting the D2D mode once the
// handover has completed. Selected via 'handoverController.typename' /
// 'nrHandoverController.typename' by the D2D-capable UE NICs.
//
simple HandoverControllerD2D extends HandoverController
{
    parameters:
        @class(HandoverControllerD2D);
}
