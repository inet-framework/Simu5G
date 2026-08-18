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

package simu5g.stack.rrc;

//
// The node's data radio bearer configuration: one descriptor per bearer, holding the
// bearer's SDAP configuration and the RLC/logical-channel configuration of its radio
// bearer (modeled on the DRB-ToAddMod and RLC-BearerConfig records of TS 38.331).
//
// The table holds two collections of descriptors. The configured bearers are the ones
// this node has been told to set up, before and independent of their establishment:
// the core network's session management delivers them (see the staticDrbs parameter of
// ~Smf), RRC takes them in and pushes the SDAP-relevant part of each into the SDAP
// module, so neither this table nor SDAP authors any of it. The established bearers are
// filled in by ~BearerManagement as bearers are established, and dropped together with
// the bearer's PDCP entity.
//
simple DrbTable like IDrbTable
{
    parameters:
        @display("i=block/table");
        @class(DrbTable);
}
