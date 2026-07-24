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
// RLC AM entity of an NR data radio bearer: the RlcAmEntityBase compound with its
// TX/RX sides defaulted to the NR (SI/SO wire format, TS 38.322) entity
// profiles. Selected by BearerManagement for NR bearers via its
// nrRlcAmEntityModuleType parameter.
//
module NrRlcAmEntity extends RlcAmEntityBase
{
    parameters:
        tx.typename = default("simu5g.stack.rlc.am.NrRlcAmTxEntity");
        rx.typename = default("simu5g.stack.rlc.am.NrRlcAmRxEntity");
}
