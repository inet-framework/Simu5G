package simu5g.stack.phy.radio;

import inet.physicallayer.wireless.common.base.packetlevel.TransmitterBase;
import inet.physicallayer.wireless.common.contract.packetlevel.ITransmitter;

// SPIKE (stage C): a transmission lasting what the PHY asked for
module SpikeTransmitter extends TransmitterBase like ITransmitter
{
    parameters:
        @class(SpikeTransmitter);
}
