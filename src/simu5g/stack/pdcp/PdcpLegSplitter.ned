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

package simu5g.stack.pdcp;

//
// Generic TX-side leg dispatcher for a multi-leg PDCP entity (see
// ~PdcpEntityBase): sends each PDCP PDU on the leg its LegReq tag names -- the
// per-packet leg decision is owned by the ~ILegSelection module, so this
// module makes no steering decision of its own. If the selected leg does not
// exist or is torn down (gate unconnected), the PDU falls back to leg 0 with
// a warning.
//
// Pure mechanism with no technology assumptions: the PDU's ids are left as
// they are, so it fits legs whose ids need no per-leg rewriting. See
// ~DcPdcpLegSplitter for the stock LTE/NR pair's id mapping.
//
simple PdcpLegSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                        // number of legs (set by the enclosing PdcpEntityBase)
        @display("i=block/dispatch");
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k (sized by the enclosing PdcpEntityBase's connections)
}
