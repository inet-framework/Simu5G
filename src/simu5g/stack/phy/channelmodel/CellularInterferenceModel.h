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

#ifndef STACK_PHY_CHANNELMODEL_CELLULARINTERFERENCEMODEL_H_
#define STACK_PHY_CHANNELMODEL_CELLULARINTERFERENCEMODEL_H_

#include <vector>

#include <omnetpp.h>
#include <inet/common/ModuleRefByPar.h>
#include <inet/common/geometry/common/Coord.h>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class Binder;
class RadioMedium;
class StochasticChannelModel;

/**
 * Computes interference for the radio medium: the received power contributed
 * by transmissions other than the one being evaluated.
 *
 * The five walks (four cellular, plus computeD2DInterference)
 * read the Binder (binder_, resolved here since the endpoint's own binder_ is
 * out of reach from an unrelated class) for allocation facts -- which eNB used
 * which RB this TTI, who is transmitting on which band, the ext-cell and
 * background-cell lists, all produced by the MAC scheduler -- and the medium's
 * registry (medium_, this module's own parent) for the physical facts of other
 * registered radios. Each walk also takes radio, the
 * calling endpoint, for facts that stay genuinely per-radio (antenna gain,
 * cable loss) and for the still-resident endpoint methods (computeAngle,
 * computeAngularAttenuation, computeExtCellPathLoss) it calls back on -- the
 * same one-radio-pointer shape RadioMedium's own computation
 * uses. Never draws: every random draw the interference contributions
 * depend on (an interferer's own attenuation, via getAttenuation) is made by
 * the medium's functions, reached through radio or through the
 * interfering endpoint's own channel model, not here.
 */
class CellularInterferenceModel : public cSimpleModule
{
  protected:
    // This module's own parent; resolved once at
    // initialize(), read live thereafter like the rest of the medium's
    // accessor family.
    RadioMedium *medium_ = nullptr;

    // Allocation facts come from the Binder, populated by the MAC
    // scheduler; not available through the medium's registry, which only
    // knows physical facts.
    inet::ModuleRefByPar<Binder> binder_;

  public:
    void initialize() override;

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /*
     * Compute total interference due to eNB coexistence for the DL direction
     * @param eNbId id of the considered eNb
     * @param isCqi if we are computing a CQI
     */
    virtual void computeDownlinkInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId ueId,
            inet::Coord coord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference);

    /*
     * Compute interference coming from neighboring cells for the UL direction
     */
    virtual void computeUplinkInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId senderId,
            bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference);

    /*
     * Evaluates total interference from external cells seen from the spot given by coord
     */
    virtual void computeExtCellInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId nodeId,
            inet::Coord coord, bool isCqi, GHz carrierFrequency, std::vector<double> *interference);

    /*
     * Evaluates total interference from external cells seen from the spot given by coord
     */
    virtual void computeBackgroundCellInterference(StochasticChannelModel *radio, MacNodeId nodeId,
            inet::Coord bsCoord, inet::Coord ueCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
            Direction dir, std::vector<double> *interference);

    /*
     * Compute interference coming from neighboring UEs for the D2D/D2D_MULTI
     * direction (interference-walk-shaped
     * like its four cellular siblings above). radio's own D2D marker
     * (RadioMedium::descriptorFor) is what the caller already resolved to decide
     * this walk should run at all.
     */
    virtual void computeD2DInterference(StochasticChannelModel *radio, MacNodeId eNbId, MacNodeId senderId,
            inet::Coord senderCoord, MacNodeId destId, inet::Coord destCoord, bool isCqi, GHz carrierFrequency,
            std::vector<double> *interference, Direction dir);

  private:
    /*
     * One interfering transmitter, normalized across the two kinds the UL
     * transmission map can hold: a real UE (with a PHY) and a background UE
     * (with a traffic generator). Returned by describeInterferer() below.
     */
    struct InterfererInfo
    {
        MacNodeId nodeId;
        MacCellId cellId;
        Direction dir;
        double txPwr;
        inet::Coord coord;
    };

    /*
     * Unpack a UeAllocationInfo into the properties every interference
     * computation needs. Shared by the uplink and D2D interference loops, which
     * otherwise differ in their exclusion rules and antenna-gain terms.
     *
     * carrierFrequency is needed only to bridge the real-UE branch's
     * physical-fact reads against the medium's registry (medium_); the
     * background-UE branch stays unbridged since a traffic generator is not
     * a registered radio.
     */
    InterfererInfo describeInterferer(const UeAllocationInfo& allocation, GHz carrierFrequency) const;
};

} //namespace

#endif
