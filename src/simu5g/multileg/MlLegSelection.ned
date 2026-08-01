//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.ip2nic.ILegSelection;

//
// Leg selection for a UE with extra NR legs: an attached leg is available;
// with several available, the legPolicy expression picks one (see
// MlLegSelection.h).
//
// Available variables: typeOfService (int), packetOrdinal (int per-flow counter).
// Return: the leg index.
//
simple MlLegSelection like ILegSelection
{
    parameters:
        @display("i=block/fork");
        string nodeType;
        string binderModule = default("binder");

        // leg picked when more than one leg is available
        object legPolicy = default(expr(typeOfService >= 20 ? 2 : 1));

    gates:
        input upperLayerIn;
        output lowerLayerOut;
}
