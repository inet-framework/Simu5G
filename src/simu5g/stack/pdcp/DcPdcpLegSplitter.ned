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
// PDCP PDU is steered to a leg by the legSelection policy expression, which sees
// the flow's IP marking (typeOfService) and a per-bearer PDU counter
// (packetOrdinal) and returns a leg index. Legs whose peer is not attached are
// not offered to it: with one leg live the choice is made already, so the policy
// only ever decides between legs that can carry the PDU.
// The module then applies the chosen leg's DC-specific id mapping on the way out:
//  - leg 0 (local LTE leg): ids already correct; emits pdcpSduSent.
//  - leg 1 on a UE (the local NR stack): rewrites the flow's source/destination
//    to the UE's NR-leg node ids (serving node looked up per packet, so
//    handover is honored); emits pdcpSduSentNr.
//  - leg 1 on a DC master (remote leg): rewrites the ids to (secondary gNB,
//    NR UE) and adds the X2 addressing tag; the PDU leaves via the DcMux.
//
// If no leg is live at all, the PDU falls back to leg 0 with a warning.
//
simple DcPdcpLegSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                        // number of legs (set by the enclosing PdcpEntityBase)
        string binderModule = default("binder");
        string handoverPacketHolderModule = default("^.^.handoverPacketHolder");  // UEs only; the bearer's PDCP entity is a direct submodule of the NIC
        // Leg index for a PDU, from typeOfService and packetOrdinal; see banner comment.
        // The default policy: a legSelection in the bearer's definition, delivered by RRC
        // through setLegSelection(), overrides this parameter.
        object legSelection = default(expr(
            typeOfService >= 20 ? (packetOrdinal % 2)
                                : (typeOfService >= 10 ? 1 : 0)
        ));
        @display("i=block/dispatch");
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        @signal[pdcpSduSentNr];
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k (sized by the enclosing PdcpEntityBase's connections)
}
