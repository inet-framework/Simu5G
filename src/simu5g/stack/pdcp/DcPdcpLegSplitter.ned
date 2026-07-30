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
// TX-side leg dispatcher for a dual-connectivity split bearer (two legs). Each
// PDCP PDU is steered per its TechnologyReq tag (leg 1 when useNR, else leg 0) --
// the per-packet leg decision is the technology decision, owned by the
// ~TechnologyDecision module, so this module makes no steering decision of its
// own. It then applies that leg's DC-specific id mapping on the way out:
//  - leg 0 (local LTE leg): ids already correct; emits pdcpSduSent.
//  - leg 1 on a UE (the local NR stack): rewrites the flow's source/destination
//    to the UE's NR-leg node ids (serving node looked up per packet, so
//    handover is honored); emits pdcpSduSentNr.
//  - leg 1 on a DC master (remote leg): rewrites the ids to (secondary gNB,
//    NR UE) and adds the X2 addressing tag; the PDU leaves via the DcMux.
//
// If the selected leg is torn down (gate unconnected), the PDU falls back to
// leg 0 with a warning -- steering avoids dead legs on its own.
//
simple DcPdcpLegSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                        // number of legs (set by the enclosing PdcpEntityBase)
        string binderModule = default("binder");
        @display("i=block/dispatch");
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        @signal[pdcpSduSentNr];
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k (sized by the enclosing PdcpEntityBase's connections)
}
