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

package simu5g.stack.rrc;

//
// The node's data radio bearer configuration: one descriptor per bearer, holding the
// bearer's SDAP configuration and the RLC/logical-channel configuration of its radio
// bearer (modeled on the DRB-ToAddMod and RLC-BearerConfig records of TS 38.331).
//
// The table holds two collections of descriptors. The authored configuration comes
// from the drbConfig parameter and describes the node's bearers before and independent
// of their establishment; RRC pushes the SDAP-relevant part of each entry into the
// SDAP module at initialization (see ~BearerManagement), so SDAP never authors its own
// configuration. The established bearers are filled in by ~BearerManagement as bearers
// are established, and dropped together with the bearer's PDCP entity.
//
// drbConfig fields:
//  - drb (int): DRB identifier (= nrPdcp[]/nrRlc[] array index)
//  - ue (int, optional): UE MacNodeId (gNB only; omit on UE side)
//  - qfiList (int[]): QFI values mapped to this DRB
//  - rlcType (string, optional): RLC mode ("AM", "UM", "TM"); default: "UM"
//  - pduSessionType (string, optional): "IPv4", "IPv6", "Ethernet", "Unstructured";
//    default: "IPv4"
//  - upperProtocol (string, optional): INET protocol name for upper layer dispatch
//    (this is normally only needed for "Unstructured" PDU sessions); default:
//    derived from pduSessionType
//  - isDefault (bool, optional): marks this as the default DRB for the UE;
//    if omitted, the first DRB entry per UE (per nodeId) is automatically used
//    as default. The default DRB is the fallback when no QFI-to-DRB mapping matches.
//
simple DrbTable like IDrbTable
{
    parameters:
        @display("i=block/table");
        @class(DrbTable);
        object drbConfig = default([]);  // JSON array: [{drb, ue, qfiList, rlcType, pduSessionType, upperProtocol, isDefault}, ...]; see banner comment for details
}
