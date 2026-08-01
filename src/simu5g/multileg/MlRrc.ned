//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.rrc.Rrc;
import simu5g.stack.rrc.IHandoverController;

//
// RRC with a handover controller slot for an extra NR leg (leg 2). NED
// 'extends' can add submodules, so no stock file is copied; the stock
// submodules (registration, bearerManagement, ...) are re-typed to their Ml
// versions by the enclosing MlNrNicUe.
//
module MlRrc extends Rrc
{
    parameters:
        bool hasNrHandoverController2 = default(false);

    submodules:
        nrHandoverController2: <default("HandoverController")> like IHandoverController if hasNrHandoverController2 {
            @display("p=850,80");
        }
}
