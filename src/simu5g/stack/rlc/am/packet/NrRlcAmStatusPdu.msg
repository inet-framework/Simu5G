//
//                  Simu5G
//
// Authors: Esteban Egea Lopez (Universidad Politécnica de Cartagena)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import  inet.common.packet.chunk.Chunk;
import  simu5g.stack.rlc.LteRlcDefs;

namespace simu5g;


class NrRlcAmStatusPdu extends inet::FieldsChunk
{
    chunkLength = inet::B(1);
    unsigned short amType enum(LteAmType);
    StatusPduData data;
}

