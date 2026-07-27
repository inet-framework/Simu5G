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
// Expression-driven TX-side leg dispatcher of a multi-leg PDCP entity (see
// ~PdcpEntity): evaluates the legSelectionRule expression for each PDCP PDU to
// obtain the leg index, then applies that leg's id mapping on the way out:
//  - leg 0 (local LTE leg): ids already correct; emits pdcpSduSent.
//  - leg 1 on a UE (the local NR stack): rewrites the flow's source/destination
//    to the UE's NR-leg node ids (serving node looked up per packet, so
//    handover is honored); emits pdcpSduSentNr.
//  - leg 1 on a DC master (remote leg): rewrites the ids to (secondary gNB,
//    NR UE) and adds the X2 addressing tag; the PDU leaves via the DcMux.
//
// The default rule follows the TechnologyReq tag computed by the
// ~TechnologyDecision module -- the home of technology steering policy -- so by
// default this module makes no steering decision of its own. Expression
// variables: useNR (bool, the packet's TechnologyReq tag), numLegs (int).
//
// If the selected leg is torn down (gate unconnected), the PDU falls back to
// leg 0 with a warning -- steering avoids dead legs on its own.
//
simple ExprBasedPdcpLegSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                                             // number of legs (set by the enclosing PdcpEntity)
        object legSelectionRule = default(expr(useNR ? 1 : 0));  // per-packet leg index (see module comment)
        string binderModule = default("binder");
        @display("i=block/dispatch");
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        @signal[pdcpSduSentNr];
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k (sized by the enclosing PdcpEntity's connections)
}
