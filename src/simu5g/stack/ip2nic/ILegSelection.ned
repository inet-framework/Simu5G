package simu5g.stack.ip2nic;

//
// Module interface for leg selection modules that choose the stack leg
// (e.g. LTE or NR) for outgoing packets.
//
// Contract: every packet forwarded to lowerLayerOut must carry a LegReq tag
// with the selected leg's index. A packet that cannot be routed on any leg
// (e.g. no leg is attached to a serving node) is discarded here.
// LegSelectionBase provides the packet-path plumbing and the expression
// machinery for implementations.
//
moduleinterface ILegSelection
{
    parameters:
        @display("i=block/fork");
        string nodeType;
    gates:
        input upperLayerIn;
        output lowerLayerOut;
}
