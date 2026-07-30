//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.pdcp;

//
// Handles the operations related to the NR PDCP (Packet Data Convergence Protocol)
// layer at the transmitting side of a specific connection. It extends the
// ~LtePdcpTxEntity module with the NR flavor of the per-SDU statistics
// (pdcpSduSentNr) and the NR-leg id mapping of a single-leg NR bearer. On a
// multi-leg bearer, leg dispatch, id mapping and per-leg statistics are handled
// by the enclosing compound's splitter instead (see ~PdcpEntityBase).
//
simple NrPdcpTxEntity extends LtePdcpTxEntity
{
    parameters:
        @dynamic(true);
        @class("NrPdcpTxEntity");
        @display("i=block/layer");
        @signal[pdcpSduSentNr];
}

