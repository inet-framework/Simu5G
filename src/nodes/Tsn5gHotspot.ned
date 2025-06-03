//
//                           Simu5G
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.nodes;

import inet.node.tsn.TsnSwitch;
import simu5g.stack.ICellularNic;


//
// TODO
//
module Tsn5gHotspot extends TsnSwitch
{
    parameters:

    gates:
        input radioIn @directIn;  // connection to the radio interface

    submodules:
        cellularNic: <default("LteNicEnb")> like ICellularNic {
            parameters:
                @display("p=800,750");
                nodeType = "GNODEB";
        }

    connections allowunconnected:
        cellularNic.radioIn <-- radioIn;
		li.out++ --> cellularNic.upperLayerIn;
		li.in++ <-- cellularNic.upperLayerOut;
}
