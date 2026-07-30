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

package simu5g.stack.pdcp;

//
// PDCP entity of an LTE data radio bearer: the ~PdcpEntityBase compound with
// its TX/RX sides bound to the LTE entity profiles (TS 36.323). Selected by
// BearerManagement via its pdcpEntityModuleType parameter, which defaults to
// this type.
//
module LtePdcpEntity extends PdcpEntityBase
{
    parameters:
        tx.typename = default("simu5g.stack.pdcp.LtePdcpTxEntity");
        rx.typename = default("simu5g.stack.pdcp.LtePdcpRxEntity");
}
