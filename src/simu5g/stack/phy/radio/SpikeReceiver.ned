package simu5g.stack.phy.radio;

import inet.physicallayer.wireless.generic.GenericReceiver;

// SPIKE (stage C): attempts every reception the medium delivers (the medium sends only what is addressed to the radio)
module SpikeReceiver extends GenericReceiver
{
    parameters:
        @class(SpikeReceiver);
}
