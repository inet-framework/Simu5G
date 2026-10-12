//
// SPIKE (stage C): CellularRadio is an INET Radio. INET's antenna,
// transmitter and receiver deliver and time the signals (unit-disk: no
// physics, no draws); Simu5G's own antenna, transmitter and receiver keep
// their roles under the cellular* names.
//
package simu5g.stack.phy.radio;

import inet.physicallayer.wireless.common.radio.packetlevel.Radio;

module CellularRadio extends Radio
{
    parameters:
        @class(CellularRadio);
        @display("i=device/antennatower");
        signalAnalogRepresentation = "unitDisk";
        initialRadioMode = "off"; // INET 4.7: the initial mode listens before the radio is on the medium; switched on at INITSTAGE_LAST
        allowConcurrentTransmissions = true;
        allowConcurrentReceptions = true;
        antenna.mobilityModule = default("^.^.^.mobility");
        transmitter.typename = "SpikeTransmitter";
        receiver.typename = "SpikeReceiver";
        receiver.ignoreInterference = true;
        transmitter.analogModel.communicationRange = 1e9m;
        transmitter.analogModel.interferenceRange = 1e9m;
        transmitter.analogModel.detectionRange = 1e9m;
    submodules:
        cellularAntenna: CellularAntenna {
            @display("p=250,100");
        }
        cellularTransmitter: CellularTransmitter {
            @display("p=250,200");
        }
        cellularReceiver: CellularReceiver {
            @display("p=250,300");
        }
}
