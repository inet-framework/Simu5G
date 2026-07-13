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

#ifndef _LTE_ID2DMACENB_H_
#define _LTE_ID2DMACENB_H_

#include "simu5g/common/LteCommon.h"

namespace simu5g {

class LteHarqBufferMirrorD2D;
class ConflictGraph;

typedef std::pair<MacNodeId, MacNodeId> D2DPair;
typedef std::map<D2DPair, LteHarqBufferMirrorD2D *> HarqBuffersMirrorD2D;

/*
 * Interface implemented by every D2D-capable eNB/gNB MAC module.
 *
 * D2D code talks to the serving cell's MAC through this interface instead of
 * a concrete class, so that LTE (LteMacEnbD2D) and NR (NrMacGnbD2D) D2D MACs
 * need no common D2D base class. Obtain it with
 * dynamic_cast<ID2dMacEnb *>(mac): a null result means the eNB is not
 * D2D-capable.
 */
class ID2dMacEnb
{
  public:
    virtual ~ID2dMacEnb() {}

    /// Access the mirror image of the D2D HARQ buffers of the served UEs (per carrier).
    virtual HarqBuffersMirrorD2D *getHarqBuffersMirrorD2D(GHz carrierFrequency) = 0;

    /// Delete the mirror D2D HARQ buffer of the given transmitter/receiver flow.
    virtual void deleteHarqBuffersMirrorD2D(MacNodeId txPeer, MacNodeId rxPeer) = 0;

    /// Send a D2D mode switch notification to the transmitter of the given flow.
    virtual void sendModeSwitchNotification(MacNodeId srcId, MacNodeId dstId, LteD2DMode oldMode, LteD2DMode newMode) = 0;

    /// Whether H-ARQ processes of D2D flows are interrupted at mode switch.
    virtual bool isMsHarqInterrupt() = 0;

    /// Whether frequency reuse is enabled for one-to-one / one-to-many D2D communications.
    virtual bool isReuseD2DEnabled() = 0;
    virtual bool isReuseD2DMultiEnabled() = 0;

    /// Access the conflict graph used for D2D frequency reuse (nullptr if reuse is disabled).
    virtual ConflictGraph *getConflictGraph() = 0;
};

} //namespace

#endif
