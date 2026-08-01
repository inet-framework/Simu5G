//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.pdcp.NrPdcpEntity;

//
// PDCP entity whose multi-leg split point replicates instead of steering
// (FRER). Reordering is auto-enabled for numLegs > 1 by PdcpEntityBase, and
// the stock RX entity discards already-received SNs, so the elimination
// half needs nothing here.
//
module MlPdcpEntity extends NrPdcpEntity
{
    parameters:
        splitter.typename = default("MlReplicatingSplitter");
}
