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
// TX-side leg dispatcher of a multi-leg PDCP entity (see ~PdcpEntity): picks
// the RLC leg for each PDCP PDU and applies that leg's id mapping on the way
// out. The leg descriptors (legs parameter, provided by the enclosing
// PdcpEntity) determine the per-leg adaptation, keyed by their "rat" field:
//  - "lte": local leg, ids already correct; emits pdcpSduSent.
//  - "nr": the UE's local NR stack; rewrites the flow's source/destination to
//    the UE's NR-leg node ids (serving node looked up per packet, so handover
//    is honored); emits pdcpSduSentNr.
//  - "x2": a DC master's remote leg; rewrites the ids to (secondary gNB, NR UE)
//    and adds the X2 addressing tag; the PDU leaves via the DcMux.
//
// Leg selection: in the current phase the splitter follows the TechnologyReq
// steering tag set by TechnologyDecision (leg 1 when useNR). The
// legSelectionRule parameter is the hook for the upcoming pluggable per-packet
// policy (expression over packet/bearer variables, or a replaced module); it is
// not evaluated yet. If the chosen leg is torn down (gate unconnected), the PDU
// falls back to leg 0 with a warning -- steering avoids dead legs on its own.
//
simple PdcpLegSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                                   // number of legs (set by the enclosing PdcpEntity)
        object legs = default([]);                     // per-leg descriptors (see module comment)
        object legSelectionRule = default(expr(0));    // per-packet leg choice (future policy hook)
        string binderModule = default("binder");
        @display("i=block/dispatch");
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        @signal[pdcpSduSentNr];
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k (sized by the enclosing PdcpEntity's connections)
}
