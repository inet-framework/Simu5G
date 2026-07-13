//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import inet.common.TagBase;
import inet.common.geometry.Geometry;
import inet.common.Units;
import simu5g.common.LteCommon;
import simu5g.stack.phy.packet.AirFrame;

namespace simu5g;

//
// Modeled SCI content + TX metadata of a sidelink transmission (design
// decision D6). SCI-1A/2A bit-exactness is NOT modeled; the frame length
// accounts for the headers via byte constants.
//
class SlAirFrameInfo extends cObject
{
    MacNodeId srcNodeId = NODEID_NONE;    // sender node id (= source L2Pid)
    MacNodeId dstPid = NODEID_NONE;       // destination L2Pid (peer node id or SL group pseudo id)
    uint32_t srcL2Id = 0;                 // 24-bit source Layer-2 ID
    uint32_t dstL2Id = 0;                 // 24-bit destination Layer-2 ID
    unsigned short castType = 0;          // SlCastType
    int64_t slotIndex = -1;               // absolute slot index of the transmission
    int firstSubchannel = 0;              // first subchannel of the used resource
    int numSubchannels = 1;               // width of the used resource
    unsigned int mcs = 0;                 // PSSCH modulation and coding scheme
    int reservationPeriodMs = 0;          // resource reservation period signaled in SCI-1A (0 = none)
    double txPower = 0;                   // transmit power [dBm]
    inet::Coord senderCoord;              // sender position at TX time
    inet::GHz carrierFrequency;
}

//
// A sidelink (PC5) transmission: PSCCH (SCI) + PSSCH (data) in the selected
// subchannels of one slot. Sent slPhy -> slPhy over the dedicated slRadioIn
// gate chain; never touches the Uu airframe handling.
//
packet SlAirFrame extends AirFrame
{
    SlAirFrameInfo info;
}

//
// Tag attached by the SL MAC to the MAC PDU handed to slPhy, carrying the
// grant/SCI parameters of this transmission (the packet also carries a
// UserControlInfo tag with source/dest/direction/carrier as usual).
//
class SlTxInfoTag extends inet::TagBase
{
    uint32_t srcL2Id = 0;
    uint32_t dstL2Id = 0;
    unsigned short castType = 0;
    int firstSubchannel = 0;
    int numSubchannels = 1;
    unsigned int mcs = 0;
    int reservationPeriodMs = 0;
}
