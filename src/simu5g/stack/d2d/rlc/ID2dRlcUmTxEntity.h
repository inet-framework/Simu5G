//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_ID2DRLCUMTXENTITY_H_
#define _SIMU5G_ID2DRLCUMTXENTITY_H_

namespace simu5g {

/**
 * @brief What a D2D UM TX entity offers the D2D mode controller.
 *
 * The LTE (FI) and the NR (SO) profiles are separate class hierarchies, so a
 * D2D-capable TX entity exists in two unrelated concrete flavours
 * (~LteRlcUmTxEntityD2D, ~NrRlcUmTxEntityD2D). ~D2DModeController has to hold
 * both in one registry, hence this narrow cross-technology interface -- the
 * same idiom as ID2dMacEnb/ID2dMacUe/ID2dAmc elsewhere in this package.
 *
 * Discovery is by dynamic_cast from the RLC TX entity base.
 */
class ID2dRlcUmTxEntity
{
  public:
    virtual ~ID2dRlcUmTxEntity() {}

    /**
     * Release the SDUs parked in the holding buffer into the TX buffer, and
     * resume normal downstream buffering. Called on the entity of the newly
     * selected mode once the old-mode entity has drained.
     */
    virtual void resumeDownstreamInPackets() = 0;

    /**
     * True while this entity is draining its TX buffer for a mode switch, i.e.
     * the peer's new-mode entity must keep holding its incoming SDUs.
     */
    virtual bool isEmptyingBuffer() = 0;

    /**
     * Start parking incoming SDUs instead of buffering them. Used when a peer's
     * old-mode entity is found to be still draining at registration time.
     */
    virtual void startHoldingDownstreamInPackets() = 0;

    /** True while incoming SDUs are being parked in the holding buffer. */
    virtual bool isHoldingDownstreamInPackets() = 0;

    /**
     * Adapt the TX buffers and the sequence numbering to a D2D mode switch.
     * Called by ~RlcMuxD2D on the entities of the old and the new mode.
     */
    virtual void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer) = 0;
};

/**
 * @brief What a D2D UM RX entity offers the D2D-aware RLC mux.
 *
 * The receiving counterpart of ~ID2dRlcUmTxEntity; see there for why a
 * cross-technology interface is needed.
 */
class ID2dRlcUmRxEntity
{
  public:
    virtual ~ID2dRlcUmRxEntity() {}

    /**
     * Adapt the RX buffers and the sequence numbering to a D2D mode switch.
     */
    virtual void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer) = 0;
};

} //namespace

#endif
