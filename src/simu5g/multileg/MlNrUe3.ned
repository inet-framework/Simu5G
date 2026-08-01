//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

//
// UE with two extra NR stack legs (legs 2 and 3). Used by the FRER scenario,
// where all three NR legs attach to the same gNB and carry replicas of the
// same bearer.
//
module MlNrUe3 extends MlNrUe
{
    parameters:
        cellularNic.typename = default("MlNrNicUe3");

        int nrServingNodeId3 = default(0); // the third leg's serving gNodeB; 0 if none
        int nrMacNodeId3 = default(2049 + simu5g_seq("NrUe_macNodeId"));

    gates:
        input nrRadioIn3 @directIn;    // the third leg's connection to its serving gNB

    connections allowunconnected:
        cellularNic.nrRadioIn3 <-- nrRadioIn3;
}
