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
// TX-side leg dispatcher for a dual-connectivity split bearer (two legs). On
// top of the generic ~PdcpLegSplitter mechanism (steer per the LegReq tag,
// fall back to leg 0 when the selected leg is torn down), it applies the
// stock LTE/NR pair's DC-specific id mapping on the way out:
//  - leg 0 (local LTE leg): ids already correct; emits pdcpSduSent.
//  - leg 1 on a UE (the local NR stack): rewrites the flow's source/destination
//    to the UE's NR-leg node ids (serving node looked up per packet, so
//    handover is honored); emits pdcpSduSentNr.
//  - leg 1 on a DC master (remote leg): rewrites the ids to (secondary gNB,
//    NR UE) and adds the X2 addressing tag; the PDU leaves via the DcMux.
//
simple DcPdcpLegSplitter extends PdcpLegSplitter
{
    parameters:
        @class(DcPdcpLegSplitter);
        string binderModule = default("binder");
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        @signal[pdcpSduSentNr];
}
