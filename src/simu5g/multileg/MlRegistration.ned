//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.rrc.Registration;

//
// Registration for a UE with extra NR legs: announces each extra leg's
// MacNodeId (node parameter nrMacNodeId<k>) and serving node
// (nrServingNodeId<k>) to the MlBinder (see MlRegistration.h).
//
simple MlRegistration extends Registration
{
    parameters:
        @class(MlRegistration);
}
