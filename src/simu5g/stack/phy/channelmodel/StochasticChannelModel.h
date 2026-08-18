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

#ifndef STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_
#define STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/phy/channelmodel/ChannelModelBase.h"
#include "simu5g/stack/phy/channelmodel/RadioMedium.h"

namespace simu5g {

using namespace omnetpp;

class Binder;
class PathLossModel;

/**
 * The full PHY link model: everything between a transmitted air frame and the
 * decision on whether it was received.
 *
 * The impairments are modeled statistically -- drawn from the distributions of a
 * 3GPP propagation study, rather than computed from the geometry of an actual
 * environment -- which is what the name refers to, and what sets this model apart
 * from IdealChannelModel, its impairment-free sibling. It is far more than
 * propagation, and covers:
 * - path loss per deployment scenario, LOS/NLOS state, and log-normal shadowing;
 * - multipath fading, Jakes or Rayleigh;
 * - the antenna pattern attenuation and the link budget (antenna gains, cable
 *   loss, noise figures, thermal noise);
 * - interference from other cells -- downlink, uplink, external cells and
 *   background cells;
 * - the assembly of all of the above into a per-band SINR, and the mapping of
 *   that SINR onto a block error probability (with HARQ reduction) that decides
 *   reception;
 * - the per-node mobility state the correlated quantities need: position
 *   history, speed, and the point at which shadowing and LOS were last drawn;
 * - the SINR statistics.
 *
 * getAttenuation, computePathLoss, computeLosProbability, computeShadowing,
 * jakesFading, rayleighFading, computeSpeed, computeCorrelationDistance,
 * getSINR, getSIR, getRSRP, getSINR_bgUe, getReceivedPower_bgUe,
 * computeInterferencePlusNoise and isReceptionSuccessful are one-line
 * forwarders to the RadioMedium this endpoint registers with: the medium
 * owns the per-carrier-leg PathLossModel strategy (chosen by the
 * pathLossType parameter -- "Tr36814", "Tr36873" or "Tr38901"), the SINR
 * assembly and the reception decision, and every random draw both make --
 * including isReceptionSuccessful's BLER draw. Tr36873ChannelModel and
 * Tr38901ChannelModel are NED-level presets of this class (no C++ class of
 * their own) that only override the pathLossType default, to Tr36873 and
 * Tr38901 respectively. The four interference walks stay resident (moving
 * at a later step), and getReceptionSinr, emitRcvdSinr and
 * computeInterferencePlusNoise stay virtual dispatch points so
 * D2dChannelModel's overrides still fire when the medium calls them back
 * on this endpoint.
 *
 * Supported propagation studies:
 * - 3GPP TR 36.814, "Further advancements for E-UTRA physical layer aspects", v9.2.0, March 2017
 * - 3GPP TR 36.873, "Study on 3D channel model for LTE", v12.7.0, December 2017
 * - 3GPP TR 38.901, "Study on channel model for frequencies from 0.5 to 100 GHz", v16.1.0, December 2019
 * - 3GPP TS 36.211, "LTE; Physical channels and modulation", v13.2.0, June 2016
 *
 * covering the Indoor Hotspot (InH), Urban Microcell (UMi), Urban Macrocell
 * (UMa), Rural Macrocell (RMa) and Suburban Macrocell (SMa) deployment
 * scenarios (not every study covers every scenario; see the PathLossModel
 * subclasses).
 *
 * D2D links are not evaluated here. The D2dChannelModel subclass layers them on
 * top of this class.
 */
class StochasticChannelModel : public ChannelModelBase
{
  protected:

    // The medium this endpoint registers with in initialize(), and the module
    // id used to look it up safely from the destructor (it may already be
    // gone by the time this endpoint is torn down).
    inet::ModuleRefByPar<RadioMedium> medium_;
    int mediumModuleId_ = -1;

    // This endpoint's stochastic state, owned by the medium (S8) and cached
    // here at registration; its address is stable for as long as this
    // endpoint stays registered (see RadioMedium::stateOf()).
    PerRadioStochasticState *state_ = nullptr;

    // Information needed about the playground
    bool useTorus_;

    // eNodeB Height
    double hNodeB_;

    // UE Height
    double hUe_;

    // average Building Heights
    double hBuilding_;

    // true if the UE is inside a building
    bool inside_building_;

    // distance from the building wall
    double inside_distance_;

    // Average street's width
    double wStreet_;

    // enable/disable the shadowing
    bool shadowing_;

    // enable/disable intercell interference computation
    bool enableBackgroundCellInterference_;
    bool enableExtCellInterference_;
    bool enableDownlinkInterference_;
    bool enableUplinkInterference_;

    bool enable_extCell_los_;

    // Scenario
    DeploymentScenario scenario_;

    // The ext-cell and background-cell interference paths evaluate their path
    // loss with the TR 36.814 formulas regardless of which propagation study
    // the model uses for its own links (see computeExtCellPathLoss); owned
    PathLossModel *extCellPathLoss_ = nullptr;

    // Correlation distance used in shadowing computation and
    // also used to recompute the probability of LOS
    double correlationDistance_;

    // Percentage of error probability reduction for each h-arq retransmission
    double harqReduction_;

    // Antenna gain of eNodeB
    double antennaGainEnB_;

    // Antenna gain of micro node
    double antennaGainMicro_;

    // Antenna gain of UE
    double antennaGainUe_;

    // Thermal noise
    double thermalNoise_;

    // Cable loss
    double cableLoss_;

    // UE noise figure
    double ueNoiseFigure_;

    // eNodeB noise figure
    double bsNoiseFigure_;

    // Enable disable fading
    bool fading_;

    // Number of fading paths in Jakes fading
    int fadingPaths_;

    // Average delay spread in Jakes fading
    double delayRMS_;

    bool tolerateMaxDistViolation_;

    enum FadingType
    {
        RAYLEIGH, JAKES
    };

    // Fading type (JAKES or RAYLEIGH)
    FadingType fadingType_;

    // Enable or disable the dynamic computation of LOS NLOS probability for each user
    bool dynamicLos_;

    // If dynamicLos is false this boolean is initialized to true if all users will be in LOS or false otherwise
    bool fixedLos_;

    // If false, disable the collection of SINR statistics, which might be quite time-consuming
    bool collectSinrStatistics_;

    // Statistics. rcvdSinr* stay protected: emitRcvdSinr(), the only reader,
    // stays resident on the endpoint (it is a D2D-override dispatch point --
    // see the getReceptionSinr/emitRcvdSinr/computeInterferencePlusNoise
    // comment below). measuredSinr* are public: RadioMedium's relocated
    // getSINR() (S10) reads them to emit on a peer endpoint's own signal.
    static simsignal_t rcvdSinrDlSignal_;
    static simsignal_t rcvdSinrUlSignal_;

  public:
    static simsignal_t measuredSinrDlSignal_;
    static simsignal_t measuredSinrUlSignal_;
    ~StochasticChannelModel() override;

    void initialize(int stage) override;

    /*
     * Node identity of the owning PHY, needed by RadioMedium::addRadio() to
     * index the registry.
     */
    MacNodeId getNodeId() const { return phy_->getMacNodeId(); }

    /*
     * Physical facts read through phy_, exposed for RadioMedium's registry
     * accessors (coordOf, txPowerOf, txDirectionOf, txAngleOf): phy_ itself
     * is a protected member, so the medium cannot dereference it directly.
     */
    inet::Coord getCoord() const { return phy_->getCoord(); }
    double getTxPwr(Direction dir = UNKNOWN_DIRECTION) const { return phy_->getTxPwr(dir); }
    TxDirectionType getTxDirection() const { return phy_->getTxDirection(); }
    double getTxAngle() const { return phy_->getTxAngle(); }
    RanNodeType getNodeType() const { return phy_->getNodeType(); }
    bool isNr() const { return phy_->isNr(); }

    /*
     * Role-appropriate antenna gain / noise figure for RadioMedium's
     * antennaGainOf()/noiseFigureOf(): the endpoint carries a parameter for
     * every role (UE, base station), so the medium asks for the one that
     * matches this endpoint's own node type.
     */
    double getAntennaGain() const { return getNodeType() == UE ? antennaGainUe_ : antennaGainEnB_; }
    double getNoiseFigure() const { return getNodeType() == UE ? ueNoiseFigure_ : bsNoiseFigure_; }

    /*
     * The raw per-role antenna gains, noise figures and cable loss, for
     * RadioMedium's relocated getSINR_bgUe()/getReceivedPower_bgUe()/getRSRP()
     * (S10): unlike getAntennaGain()/getNoiseFigure() above, these background-UE
     * paths need both roles' values from the one endpoint that plays the eNB
     * side, not just the value matching this endpoint's own role.
     */
    double getAntennaGainEnB() const { return antennaGainEnB_; }
    double getAntennaGainUe() const { return antennaGainUe_; }
    double getUeNoiseFigure() const { return ueNoiseFigure_; }
    double getBsNoiseFigure() const { return bsNoiseFigure_; }
    double getCableLoss() const { return cableLoss_; }

    /*
     * Whether to collect the measured/received-SINR statistics, for
     * RadioMedium's relocated getSINR()/isReceptionSuccessful() (S10).
     */
    bool getCollectSinrStatistics() const { return collectSinrStatistics_; }

    /*
     * Building-penetration distance for RadioMedium's insideDistanceOf().
     */
    double getInsideDistance() const { return inside_distance_; }

    /*
     * Building-penetration flag, paired with getInsideDistance() into the
     * O2iState PathLossModel::computePathLoss() reads.
     */
    bool getInsideBuilding() const { return inside_building_; }

    /*
     * The Binder, for RadioMedium's relocated rayleighFading() (S9b): binder_
     * itself is a protected member, so the medium cannot dereference it directly.
     */
    Binder *getBinder() const { return binder_; }

    /*
     * Compute attenuation (path loss + optional shadowing) over a radio link.
     */
    double getAttenuation(const RadioLink& link) override;

    /*
     * Convenience overload for the cellular callers that still think in
     * (UE, direction, remote coordinate) terms -- the interference helpers and
     * the background-UE path. Builds a cellular link and forwards, so the
     * subclass override of getAttenuation(const RadioLink&) still applies.
     *
     * @param nodeId mac node id of UE
     * @param dir traffic direction
     * @param coord position of end point communication (if dir==UL is the position of UE else is the position of eNodeB)
     */
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl)
    {
        return getAttenuation(cellularLink(nodeId, dir, coord, cqiDl));
    }

    /*
     *  Compute angle between two coordinates
     *
     * @param center first coord
     * @param point second coord
     */
    virtual double computeAngle(Coord center, Coord point);

    /*
     *  Compute vertical angle between two coordinates
     *  The returned angle is the angle with respect to the zenith direction
     *
     * @param center first coord
     * @param point second coord
     * @return angle
     */
    virtual double computeVerticalAngle(Coord center, Coord point);

    /*
     *  Compute Attenuation caused by transmission direction
     *
     * @param angle angle
     */
    virtual double computeAngularAttenuation(double hAngle, double vAngle = 0);

    /*
     * Compute shadowing
     *
     * @param d3D 3D distance between UE and eNodeB
     * @param d2D 2D distance between UE and eNodeB
     * @param nodeid mac node id of UE
     * @param speed speed of UE
     */
    virtual double computeShadowing(double d3D, double d2D, const LinkKey& key, MacNodeId ownerId, double speed, bool cqiDl);

    /*
     * Compute sir for each band for user nodeId according to multipath fading
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     */
    std::vector<double> getSIR(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    /*
     * Compute sinr for each band for user nodeId according to pathloss, shadowing (optional) and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     */
    std::vector<double> getSINR(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    /*
     * Add noise and interference to an already-computed per-band received-power
     * vector. Split out so that a caller which already holds the RSRP (the D2D
     * one-to-many capture-effect path) shares this implementation instead of
     * repeating it.
     */
    virtual std::vector<double> getSINR(const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector);

    /*
     * Compute received useful signal for each band for user nodeId according to pathloss, shadowing (optional) and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     */
    std::vector<double> getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    /*
     * Compute sinr for each band for a background UE according to pathloss
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     */
    std::vector<double> getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    /*
     * Compute received power for a background UE according to pathloss
     *
     */
    double getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId) override;

    /*
     * Compute the error probability of the transmitted packet according to cqi used, txmode, and the received power
     * after that it throws a random number in order to check if this packet will be corrupted or not
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     * @param rsrpVector the received signal for each RB, if it has already been computed
     */
    bool isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteI, const std::vector<double>& rsrpVector) override;

    /*
     * Compute the path-loss attenuation according to the selected scenario
     *
     * @param distance between UE and eNodeB
     * @param los line-of-sight flag
     */
    double computePathLoss(double distance, double dbp, bool los) override;

    /*
     * Compute Rayleigh fading
     *
     * @param i index in the trace file
     * @param nodeid mac node id of UE
     */
    virtual double rayleighFading(MacNodeId id, unsigned int band);

    /*
     * Compute Jakes fading
     *
     * @param speed speed of UE
     * @param nodeid mac node id of UE
     * @param band logical band id
     * @param cqiDl if true, the jakesMap in the UE side should be used
     * @param isBgUe if true, this is called for a background UE
     */
    virtual double jakesFading(const LinkKey& key, MacNodeId ownerId, double speed, unsigned int band, bool cqiDl, bool isBgUe = false);

    /*
     * Compute LOS probability
     *
     * @param d3D 3D distance between UE and eNodeB
     * @param d2D 2D distance between UE and eNodeB
     * @param nodeid mac node id of UE
     */
    virtual void computeLosProbability(double d3D, double d2D, const LinkKey& key);

    JakesFadingMap *getJakesMap()
    {
        return &jakesState();
    }

    ShadowFadingMap *getShadowingMap()
    {
        return &shadowingState();
    }

    bool isUplinkInterferenceEnabled() override { return enableUplinkInterference_; }
    /*
     * Compute the received useful signal (RSRP) per band over a radio link.
     */
    virtual std::vector<double> getRSRP(const RadioLink& link, double txPower);

    /*
     * Public for RadioMedium's relocated getAttenuation()/computeShadowing()/
     * jakesFading() (S9b), which call these on the radio identifying the
     * caller: getTwoDimDistance() and the position/correlation-distance
     * bookkeeping stay on the endpoint since they are also read from the
     * still-resident getSINR()/getSIR(), and obtainUeJakesMap()/
     * obtainShadowingMap() bridge to a peer endpoint's own state and are
     * deleted, not relocated, at a later step.
     */
    virtual double getTwoDimDistance(inet::Coord a, inet::Coord b);
    virtual void updatePositionHistory(const MacNodeId nodeId, const inet::Coord coord);
    virtual void updateCorrelationDistance(const LinkKey& key, const inet::Coord coord);
    virtual JakesFadingMap *obtainUeJakesMap(MacNodeId id);
    virtual ShadowFadingMap *obtainShadowingMap(MacNodeId id);

    /*
     * Public for RadioMedium's relocated getSINR()/getSIR()/getRSRP()/
     * getSINR_bgUe()/getReceivedPower_bgUe()/isReceptionSuccessful() (S10).
     * linkFor() and the four interference walks below are plain resident
     * helpers the relocated bodies now call back on the radio pointer.
     * computeInterferencePlusNoise(), getReceptionSinr() and emitRcvdSinr()
     * are more than that: they are D2D-override dispatch points
     * (D2dChannelModel substitutes UE-to-UE interference, routes through
     * getSINR_D2D, and reports rcvdSinrD2D respectively), so the relocated
     * bodies must call them on the radio pointer -- never on the medium
     * itself -- for that override to still fire. The four interference
     * walks are S11's; they stay resident only until then.
     */
    virtual RadioLink linkFor(UserControlInfo *lteInfo);
    virtual void emitRcvdSinr(Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr);
    virtual void computeInterferencePlusNoise(const RadioLink& link, UserControlInfo *lteInfo,
            RbMap& rbmap, double totN, std::vector<double>& den);
    virtual std::vector<double> getReceptionSinr(LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector) { return getSINR(frame, lteInfo); }
    virtual bool computeDownlinkInterference(MacNodeId eNbId, MacNodeId ueId, inet::Coord coord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference);
    virtual bool computeUplinkInterference(MacNodeId eNbId, MacNodeId senderId, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference);
    virtual bool computeExtCellInterference(MacNodeId eNbId, MacNodeId nodeId, inet::Coord coord, bool isCqi, GHz carrierFrequency, std::vector<double> *interference);
    virtual bool computeBackgroundCellInterference(MacNodeId nodeId, inet::Coord bsCoord, inet::Coord ueCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, Direction dir, std::vector<double> *interference);

  protected:

    /*
     * Build the RadioLink for a UE<->serving-BS link expressed the old way: the
     * local module is one endpoint, 'coord' the other, and 'dir' says which of
     * the two is the UE.
     */
    RadioLink cellularLink(MacNodeId ueId, Direction dir, inet::Coord coord, bool cqiDl);

    /*
     * Compute speed (m/s) for a given node
     * @param nodeid mac node id of UE
     * @return the speed in m/s
     */
    virtual double computeSpeed(const MacNodeId nodeId, const inet::Coord coord);

    /*
     * Compute the euclidean distance between the current position and the
     * last position used to calculate the LOS probability
     */
    virtual double computeCorrelationDistance(const LinkKey& key, const inet::Coord coord);

    /*
     * One interfering transmitter, normalized across the two kinds the UL
     * transmission map can hold: a real UE (with a PHY) and a background UE
     * (with a traffic generator).
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
     * medium/carrierFrequency are needed only to bridge the real-UE branch's
     * physical-fact reads against the medium's registry (S4); the background-UE
     * branch stays unbridged since a traffic generator is not a registered radio.
     */
    static InterfererInfo describeInterferer(const UeAllocationInfo& allocation, RadioMedium *medium, GHz carrierFrequency);

    /*
     * Compute attenuation due to path loss and shadowing
     * @return attenuation expressed in dBm
     */
    virtual double computeExtCellPathLoss(double dist, const LinkKey& key);

  private:
    /*
     * Accessors for the six per-radio stochastic-state containers (S7 of the
     * radio-medium plan): every touch of losMap, lastComputedSF,
     * jakesFadingMap, jakesFadingMapBgUe, positionHistory and
     * lastCorrelationPoint goes through one of these, so relocating the
     * containers needed no hunting for scattered touches. As of S8 the
     * containers themselves live in the medium's PerRadioStochasticState for
     * this endpoint (state_, cached at registration); each accessor still
     * reproduces the exact std::map::operator[] semantics the call sites
     * relied on (or, where a call site must not auto-vivify an entry, an
     * explicit createIfMissing flag). No RNG attribution changes: this
     * endpoint still computes and still draws, only the storage moved.
     *
     * shadowingState()/jakesState()/jakesStateBgUe() return the whole
     * container, not a single link's entry: today's code selects between
     * this local container and a *remote* endpoint's map
     * (obtainShadowingMap()/obtainUeJakesMap(), left untouched here -- they
     * are deleted, not relocated, at S13) through one raw map pointer, so
     * the seam has to be the same shape.
     */

    /** Auto-vivifying access to state_->losMap[key]; existed, if given, reports whether the entry was already present. */
    bool& losState(const LinkKey& key, bool *existed = nullptr)
    {
        auto result = state_->losMap.try_emplace(key, false);
        if (existed != nullptr)
            *existed = !result.second;
        return result.first->second;
    }

    /** This endpoint's shadowing-state container (whole-container seam; see the comment above). */
    ShadowFadingMap& shadowingState() { return state_->lastComputedSF; }

    /** This endpoint's (non-background-UE) Jakes-fading-state container. */
    JakesFadingMap& jakesState() { return state_->jakesFadingMap; }

    /** jakesFadingMap's background-UE twin: same key type, selected instead of it when isBgUe is true. */
    JakesFadingMap& jakesStateBgUe() { return state_->jakesFadingMapBgUe; }

    /**
     * Access to state_->positionHistory[nodeId]. createIfMissing=true
     * auto-vivifies an empty queue like std::map::operator[]
     * (updatePositionHistory()); createIfMissing=false returns nullptr
     * instead of inserting a placeholder for a node with no history yet
     * (computeSpeed(), which must not manufacture an entry it would then
     * read as non-empty).
     */
    std::queue<Position> *positionHistory(MacNodeId nodeId, bool createIfMissing)
    {
        if (createIfMissing)
            return &state_->positionHistory[nodeId];
        auto it = state_->positionHistory.find(nodeId);
        return it == state_->positionHistory.end() ? nullptr : &it->second;
    }

    /** Auto-vivifying access to state_->lastCorrelationPoint[key]; existed, if given, reports whether the entry was already present. */
    Position& correlationPoint(const LinkKey& key, bool *existed = nullptr)
    {
        auto result = state_->lastCorrelationPoint.try_emplace(key);
        if (existed != nullptr)
            *existed = !result.second;
        return result.first->second;
    }
};

} //namespace

#endif /* STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_ */

