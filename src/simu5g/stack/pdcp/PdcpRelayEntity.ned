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
// Stands in place of the PDCP entity of a data radio bearer at a
// dual-connectivity SECONDARY node. The bearer's real PDCP entity lives at the
// master; the secondary only tunnels the already-processed PDUs between the
// master (over X2, via the DcMux) and its own RLC, doing no PDCP processing of
// its own. Created dynamically by BearerManagement, one per (UE, DRB) pair, in
// place of a ~PdcpEntity.
//
// The RLC-side boundary mirrors ~PdcpEntity's (legOut/legIn), so the RLC entity
// is wired the same way regardless of what sits above it. The two directions are
// independent halves: ~PdcpDownlinkRelay carries DL (master -> RLC),
// ~PdcpUplinkRelay carries UL (RLC -> master).
//
module PdcpRelayEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/wheelbarrow");
    gates:
        output legOut[1];   // DL PDU to the local RLC entity (shared RLC boundary, cf. PdcpEntity)
        input legIn[1];     // UL PDU from the local RLC entity
        input x2In;         // DL PDU tunneled from the master (via the DcMux)
        output x2Out;       // UL PDU to be tunneled to the master (via the DcMux)
    submodules:
        dl: PdcpDownlinkRelay {
            @display("p=60,50");
        }
        ul: PdcpUplinkRelay {
            @display("p=140,50");
        }
    connections:
        x2In --> dl.in;
        dl.out --> { @display("m=s"); } --> legOut[0];
        legIn[0] --> { @display("m=s"); } --> ul.in;
        ul.out --> x2Out;
}
