//
//                  Simu5G
//
// Copyright (C) 2012 Antonio Virdis, Daniele Migliorini, Matteo Maria Andreozzi,
//   Giovanni Accongiagioco, Generoso Pagano, Vincenzo Pii (SimuLTE)
// Copyright (C) 2013-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace simu5g;

//
// Packet used by the ~VoPSender and ~VoipReceiver modules.
//
class VoipPacket extends inet::FieldsChunk {
    unsigned int IDtalk;
    unsigned int nframes;
    unsigned int IDframe;
    simtime_t arrivalTime;
    simtime_t playoutTime;
    simtime_t payloadTimestamp;
    unsigned int payloadSize;
}
