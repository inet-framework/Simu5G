package simu5g.stack.ip2nic;

//
// Leg selection for the stock LTE/NR leg pair (0 = LTE, 1 = NR): selects the
// stack leg for outgoing packets and attaches the choice to the packet as a
// LegReq tag. Sits above ~Ip2Nic in the outgoing packet path.
//
// When both legs are available, the choice is made by evaluating a
// configurable expression. Two expressions are provided:
// - dcLegPolicy: used when dual connectivity is enabled and both legs are available
// - legPolicy: used when DC is not enabled but both legs are available
//
// Available variables: typeOfService (int), packetOrdinal (int per-flow counter).
// Return: the leg index.
//
simple LteNrLegSelection like ILegSelection
{
    parameters:
        @display("i=block/fork");
        string nodeType;
        string binderModule = default("binder");
        string handoverPacketHolderModule = default("^.handoverPacketHolder");

        // DC case: both legs available, dual connectivity enabled.
        object dcLegPolicy = default(expr(
            typeOfService >= 20 ? packetOrdinal % 2
                                : (typeOfService >= 10 ? 1 : 0)
        ));

        // Non-DC case: both legs available, DC not enabled.
        object legPolicy = default(expr(typeOfService >= 10 ? 1 : 0));

    gates:
        input upperLayerIn;
        output lowerLayerOut;
}
