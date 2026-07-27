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
// PDCP entity of an NR data radio bearer: the PdcpEntity compound with its
// TX/RX sides defaulted to the NR entity profiles (TS 38.323), i.e. NR-flavored
// statistics on the TX side and the multi-leg reorder window on the RX side.
// Selected by BearerManagement via its pdcpEntityModuleType parameter (set by
// NrNicUe/NrNicEnb, and on EN-DC master eNBs from the configuration).
//
module NrPdcpEntity extends PdcpEntity
{
    parameters:
        tx.typename = default("simu5g.stack.pdcp.NrTxPdcpEntity");
        rx.typename = default("simu5g.stack.pdcp.NrRxPdcpEntity");
}
