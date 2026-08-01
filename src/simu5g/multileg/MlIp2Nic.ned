//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.ip2nic.Ip2Nic;

//
// Ip2Nic for nodes aware of extra legs: per-leg local ids and next-hop
// resolution for legs >= 2 (see MlIp2Nic.h).
//
simple MlIp2Nic extends Ip2Nic
{
    parameters:
        @class(MlIp2Nic);
}
