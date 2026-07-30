//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.rlc;

//
// RLC UM entity of an LTE data radio bearer: the ~RlcUmEntityBase compound with
// its TX/RX sides bound to the LTE (FI wire format, TS 36.322) entity profiles.
// Selected by BearerManagement via its lteRlcUmEntityModuleType parameter,
// which defaults to this type.
//
module LteRlcUmEntity extends RlcUmEntityBase
{
    parameters:
        tx.typename = default("simu5g.stack.rlc.um.UmTxEntity");
        rx.typename = default("simu5g.stack.rlc.um.UmRxEntity");
}
