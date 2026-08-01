//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.common.binder.Binder;

//
// Binder with per-leg id bookkeeping for nodes with more than two legs
// (see MlBinder.h). Installed as the network's 'binder' submodule.
//
simple MlBinder extends Binder
{
    parameters:
        @class(MlBinder);
}
