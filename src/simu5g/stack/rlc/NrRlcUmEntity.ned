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
// RLC UM entity of an NR data radio bearer: the RlcUmEntityBase compound with its
// TX/RX sides defaulted to the NR (SI/SO wire format, TS 38.322) entity
// profiles. Selected by BearerManagement for NR bearers via its
// nrRlcUmEntityModuleType parameter.
//
module NrRlcUmEntity extends RlcUmEntityBase
{
    parameters:
        txEntityType = default("simu5g.stack.rlc.um.NrRlcUmTxEntity");
        rxEntityType = default("simu5g.stack.rlc.um.NrRlcUmRxEntity");
}
