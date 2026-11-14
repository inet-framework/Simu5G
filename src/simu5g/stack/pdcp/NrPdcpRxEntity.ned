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
// The receiving side of the NR PDCP (Packet Data Convergence Protocol) entity of one bearer.
// It extends ~LtePdcpRxEntity with the reception buffer of TS 38.323 5.2.2: NR RLC delivers
// out of order, and so does a bearer with several legs, so the entity stores the SDUs that
// arrive out of sequence and delivers them in COUNT order. A gap closes when the missing SDU
// arrives or when t-Reordering expires; an SDU that arrives after that is discarded, as is a
// duplicate. With outOfOrderDelivery, every SDU goes up on arrival, and duplicates are still
// discarded.
//
simple NrPdcpRxEntity extends LtePdcpRxEntity
{
    parameters:
        @dynamic(true);
        @class("NrPdcpRxEntity");
        @display("i=block/layer");
        bool outOfOrderDelivery = default(false);  // Deliver each SDU on arrival instead of in COUNT order (TS 38.331 outOfOrderDelivery)
        double timeout @unit(s) = default(0.1s);   // t-Reordering; a modeling default, as TS 38.331 applies infinity when the network leaves it out
        int rxWindowSize = default(4096);
}
