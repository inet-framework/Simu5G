//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.nodes.NrUe;

//
// UE with an extra NR stack leg (leg 2), attached to its own gNB. The leg-2
// id is drawn from the same simu5g_seq sequence as the stock NR id, so it is
// collision-free by construction.
//
module MlNrUe extends NrUe
{
    parameters:
        cellularNic.typename = default("MlNrNicUe");

        int nrServingNodeId2 = default(0); // the extra leg's serving gNodeB; 0 if none
        int nrMacNodeId2 = default(2049 + simu5g_seq("NrUe_macNodeId"));

    gates:
        input nrRadioIn2 @directIn;    // the extra leg's connection to its serving gNB

    connections allowunconnected:
        cellularNic.nrRadioIn2 <-- nrRadioIn2;
}
