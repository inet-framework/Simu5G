//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.phy.LtePhyEnbD2D;

//
// gNB PHY that delivers frames for an extra leg's id to that leg's own
// radio gate (nrRadioIn2 etc.; see MlPhyGnb.h). Installed on the gNBs via
// the NIC's phy typename.
//
simple MlPhyGnb extends LtePhyEnbD2D
{
    parameters:
        @class(MlPhyGnb);
}
