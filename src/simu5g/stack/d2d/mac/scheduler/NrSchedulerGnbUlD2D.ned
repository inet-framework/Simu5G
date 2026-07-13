//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.d2d.mac.scheduler;

import simu5g.stack.mac.scheduler.NrSchedulerGnbUl;

//
// NR gNB uplink scheduler with device-to-device (D2D) support. It is the
// D2D-capable variant of ~NrSchedulerGnbUl, wired in by the D2D NR MAC via the
// ulSchedulerType parameter.
//
simple NrSchedulerGnbUlD2D extends NrSchedulerGnbUl
{
    parameters:
        @class(NrSchedulerGnbUlD2D);
}
