package simu5g.stack.ip2nic;

//
// Module interface for leg selection modules that choose the stack leg
// (e.g. LTE or NR) for outgoing packets.
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
