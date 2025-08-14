//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import inet.common.packet.chunk.Chunk;

namespace simu5g;

//
// Represents the PDCP header in LTE and NR.
//
class LtePdcpHeader extends inet::FieldsChunk
{
    //TODO set chunk size in the code instead:
    // LTE: sequence number is 7 or 12 bits, resulting in 1 or 2-byte header
    // NR:  sequence number is 12 or 18 bits, resulting in 2 or 3-byte header
    chunkLength = inet::B(1);

    unsigned int sequenceNumber = 0;  // PDCP sequence number (7, 12 or 18 bits)
}
