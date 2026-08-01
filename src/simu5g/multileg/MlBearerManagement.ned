//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.rrc.BearerManagement;

//
// BearerManagement for a UE NIC with extra NR legs: the leg lookups answer
// for legs >= 2, bearer establishment is inherited unchanged (see
// MlBearerManagement.h).
//
simple MlBearerManagement extends BearerManagement
{
    parameters:
        @class(MlBearerManagement);

        // per-extra-leg module paths (empty = leg not present)
        string nrMac2Module = default("^.^.nrMac2");
        string nrRlcMux2Module = default("^.^.nrRlcMux2");
        string nrMac3Module = default("");
        string nrRlcMux3Module = default("");
}
