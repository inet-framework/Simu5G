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

#ifndef STACK_PHY_CHANNELMODEL_LTECHANNELMODEL_H_
#define STACK_PHY_CHANNELMODEL_LTECHANNELMODEL_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/carrierAggregation/ComponentCarrier.h"
#include "simu5g/stack/phy/LtePhyBase.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
namespace simu5g {

using namespace inet;
using namespace omnetpp;

class LteAirFrame;
class LtePhyBase;
class Binder;

/**
 * A radio link between two arbitrary endpoints, and the parameters the channel
 * model needs to evaluate it.
 *
 * The channel model used to be phrased as "a link between me (phy_->getCoord())
 * and one remote endpoint", with Direction selecting -- all at once -- which
 * endpoint was mobile, which antenna gains applied, which noise figure applied,
 * and which fading/shadowing map to use. A UE-to-UE link fits neither of those
 * two shapes, which is why the D2D channel model had to re-implement the whole
 * propagation path rather than reuse it.
 *
 * Here those four things are data. `dir` survives only as a tag: it selects the
 * statistic to emit and dispatches the (genuinely cellular-topology-aware)
 * interference computation, but it no longer derives any of the link geometry
 * or the link budget.
 */
struct RadioLink
{
    // ---- geometry ----
    MacNodeId txId = NODEID_NONE;
    MacNodeId rxId = NODEID_NONE;
    inet::Coord txCoord;
    inet::Coord rxCoord;

    // ---- per-node channel state ----
    // stateKey indexes losMap_ / lastComputedSF_ / jakesFadingMap_ /
    // positionHistory_ / lastCorrelationPoint_. The owning instance is derived:
    // with useUeSideMaps the maps are fetched from the UE's own channel model
    // via obtainShadowingMap() / obtainUeJakesMap(), otherwise they are this
    // module's own.
    //
    // NOTE: this ought to be a *link* key, not a node key. Every link type
    // currently sets it to a single node id, so all of a node's links share one
    // slot -- harmless for cellular (one link per UE per instance) but wrong for
    // D2D, where a UE has many peers. Fixing that changes every stored fading
    // and shadowing realization, so it is deliberately not done here.
    MacNodeId stateKey = NODEID_NONE;
    inet::Coord stateCoord;      // position feeding computeSpeed + correlation distance
    bool useUeSideMaps = false;  // the former 'cqiDl' flag

    // ---- link budget ----
    double txAntennaGain = 0.0;
    double rxAntennaGain = 0.0;
    double noiseFigure = 0.0;
    bool txIsBaseStation = false;   // gates angular attenuation

    // The cell this link belongs to. Only the interference computation needs it --
    // that model is genuinely cellular-topology-aware, since it asks which cell an
    // interferer is in. For a cellular link it is the base-station endpoint; for a
    // UE-to-UE link it is the transmitter's serving cell.
    MacNodeId cellId = NODEID_NONE;

    // ---- tag, not a switch ----
    Direction dir = UNKNOWN_DIRECTION;
};

class LteChannelModel : public cSimpleModule
{
  protected:
    // Reference to Binder module
    inet::ModuleRefByPar<Binder> binder_;

    // Reference to cell info module
    inet::ModuleRefByPar<CellInfo> cellInfo_;

    // Reference to the corresponding PHY layer
    opp_component_ptr<LtePhyBase> phy_;

    // Reference to the component carrier
    inet::ModuleRefByPar<ComponentCarrier> componentCarrier_;

    // Carrier Frequency and its base-10 logarithm
    GHz carrierFrequency_;
    double carrierFrequencyHz_;
    double carrierFrequencyGHz_;
    double log10CarrierFrequencyGHz_;

    // Number of bands for this carrier
    unsigned int numBands_ = -1;

  public:

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    /*
     * Returns the carrier frequency
     */
    virtual GHz getCarrierFrequency() const { return GHz(carrierFrequencyGHz_); }

    /*
     * Returns the number of logical bands
     */
    virtual unsigned int getNumBands() const { return numBands_; }

    /*
     * Returns the numerology index
     */
    virtual unsigned int getNumerologyIndex() const { return componentCarrier_->getNumerologyIndex(); }

    virtual void setPhy(LtePhyBase *phy) { phy_ = phy; }

    /*
     * Compute the error probability of the transmitted packet according to CQI used, TX mode, and the received power
     * After that, it generates a random number to check if this packet will be corrupted or not
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual bool isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;

    /*
     * Compute attenuation (path loss + optional shadowing) over a radio link.
     *
     * @param link the two endpoints and the channel-state key to evaluate against
     */
    virtual double getAttenuation(const RadioLink& link) = 0;
    /*
     * Compute the path-loss attenuation according to the selected scenario
     *
     * @param distance between UE and eNodeB
     * @param los line-of-sight flag
     */
    virtual double computePathLoss(double distance, double dbp, bool los) = 0;
    /*
     * Compute SIR for each band for user nodeId according to multipath fading
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getSIR(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;
    /*
     * Compute SINR for each band for user nodeId according to path loss, shadowing (optional), and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getSINR(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;
    /*
     * Compute SINR for each band for a background UE according to path loss
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;

    /*
     * Compute received power for a background UE according to path loss
     *
     */
    virtual double getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId) = 0;

    /*
     * Compute the error probability of the transmitted packet according to CQI used, TX mode, and the received power
     * After that, it generates a random number to check if this packet will be corrupted or not
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     * @param rsrpVector the received signal for each RB, if it has already been computed
     */
    virtual bool isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector) = 0;
    /*
     * Compute received useful signal for each band for user nodeId according to path loss, shadowing (optional), and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;
    /*
     * Compute received useful signal for D2D transmissions
     */
    virtual std::vector<double> getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord) = 0;
    /*
     * Compute SINR (D2D) for each band for user nodeId according to path loss, shadowing (optional), and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId peerUeId, inet::Coord peerUeCoord, MacNodeId enbId = NODEID_NONE) = 0;
    virtual std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector) = 0;

    virtual bool isUplinkInterferenceEnabled() { return false; }
    virtual bool isD2DInterferenceEnabled() { return false; }
};

} //namespace

#endif

