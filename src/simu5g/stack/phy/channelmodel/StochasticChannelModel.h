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
#include "simu5g/stack/phy/PhyBase.h"
#include "simu5g/stack/phy/channelmodel/ChannelModelBase.h"
#include "simu5g/stack/phy/channelmodel/RadioMedium.h"

namespace simu5g {

using namespace omnetpp;

class Binder;

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
 * getAttenuation, computePathLoss, getSINR, getRSRP, getSINR_bgUe,
 * getReceivedPower_bgUe and isReceptionSuccessful are one-line forwarders to
 * the RadioMedium this endpoint registers with: the medium owns the
 * per-carrier-leg PathLossModel strategy (chosen by the pathLossType
 * parameter -- "Tr36814", "Tr36873" or "Tr38901"), the SINR assembly and the
 * reception decision, and every random draw both make -- including
 * isReceptionSuccessful's BLER draw. Tr36873ChannelModel and Tr38901ChannelModel
 * are NED-level presets of this class (no C++ class of their own) that only
 * override the pathLossType default, to Tr36873 and Tr38901 respectively.
 * The four cellular interference walks live on the medium's interference
 * submodule; the D2D interference walk and the D2D branches of
 * getReceptionSinr/emitRcvdSinr/computeInterferencePlusNoise live in the
 * medium alongside them -- D2dChannelModel does not override any of
 * the three, so none of them are virtual dispatch points here. The
 * per-link/per-node stochastic state lives on the medium;
 * this endpoint caches none of it. The medium computes shadowing,
 * Jakes/Rayleigh fading, LOS probability and speed directly rather than
 * calling back through the endpoint, so this class carries no forwarder for
 * any of them. The ext-cell/background-cell path-loss strategy is
 * likewise one Tr36814PathLossModel per carrier leg, owned by the medium and
 * reached through extCellPathLossFor(); this endpoint holds no instance of
 * its own.
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
 * D2D links are evaluated by the medium too; D2dChannelModel is an
 * endpoint marker (RadioMedium::RadioDescriptor::d2dEndpoint) plus the
 * ID2dChannelModel entry points (getRSRP_D2D/getSINR_D2D) D2D-aware PHY code
 * calls directly.
 */
class StochasticChannelModel : public ChannelModelBase
{
  protected:

    // The medium this endpoint registers with in initialize(), and the module
    // id used to look it up safely from the destructor (it may already be
    // gone by the time this endpoint is torn down).
    inet::ModuleRefByPar<RadioMedium> medium_;
    int mediumModuleId_ = -1;

    // true if the UE is inside a building
    bool inside_building_;

    // distance from the building wall; drawn once at registration
    // rather than in the medium, so it stays resident -- handed to the
    // medium per-call through o2iStateOf() instead of cached there
    double inside_distance_;

    // enable/disable the interference computation for UL connections, for
    // isUplinkInterferenceEnabled()/recordsUlTransmissionMap()
    bool enableUplinkInterference_;

    bool enable_extCell_los_;

    // Antenna gain of eNodeB
    double antennaGainEnB_;

    // Antenna gain of micro node; write-only (set from the antennGainMicro
    // parameter, never read)
    double antennaGainMicro_;

    // Antenna gain of UE
    double antennaGainUe_;

    // Cable loss
    double cableLoss_;

    // UE noise figure
    double ueNoiseFigure_;

    // eNodeB noise figure
    double bsNoiseFigure_;

    // If false, disable the collection of SINR statistics, which might be quite time-consuming
    bool collectSinrStatistics_;

  public:
    // Statistics, public: RadioMedium's getSINR() reads
    // measuredSinr* to emit on a peer endpoint's own signal, and its
    // emitRcvdSinr() reads rcvdSinr* the same way.
    static simsignal_t rcvdSinrDlSignal_;
    static simsignal_t rcvdSinrUlSignal_;
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
    bool isNr() const { return phy_->isNr(); }

    /*
     * The raw per-role antenna gains, noise figures and cable loss, for
     * RadioMedium's getSINR_bgUe()/getReceivedPower_bgUe()/getRSRP():
     * these background-UE paths need both roles' values from the one
     * endpoint that plays the eNB side, not just the value matching this
     * endpoint's own role.
     */
    double getAntennaGainEnB() const { return antennaGainEnB_; }
    double getAntennaGainUe() const { return antennaGainUe_; }
    double getUeNoiseFigure() const { return ueNoiseFigure_; }
    double getBsNoiseFigure() const { return bsNoiseFigure_; }
    double getCableLoss() const { return cableLoss_; }

    /*
     * Whether to collect the measured/received-SINR statistics, for
     * RadioMedium's getSINR()/isReceptionSuccessful().
     */
    bool getCollectSinrStatistics() const { return collectSinrStatistics_; }

    /*
     * Building-penetration distance, paired with getInsideBuilding() into
     * the O2iState RadioMedium's o2iStateOf() returns.
     */
    double getInsideDistance() const { return inside_distance_; }

    /*
     * Building-penetration flag, paired with getInsideDistance() into the
     * O2iState PathLossModel::computePathLoss() reads.
     */
    bool getInsideBuilding() const { return inside_building_; }

    /*
     * The Binder, for RadioMedium's rayleighFading(): binder_
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
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord)
    {
        return getAttenuation(cellularLink(nodeId, dir, coord));
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
     * Compute sinr for each band for user nodeId according to pathloss, shadowing (optional) and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteinfo pointer to the user control info
     */
    std::vector<double> getSINR(LteAirFrame *frame, UserControlInfo *lteInfo) override;

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

    bool isUplinkInterferenceEnabled() override { return enableUplinkInterference_; }

    /*
     * Public for RadioMedium's getAttenuation(): a plain,
     * stateless coordinate helper the medium calls back on the radio
     * pointer.
     */
    virtual double getTwoDimDistance(inet::Coord a, inet::Coord b);

    /*
     * Public: RadioMedium's getSINR()/getRSRP()/
     * getSINR_bgUe()/getReceivedPower_bgUe()/isReceptionSuccessful(),
     * resident on the medium, call this plain resident helper back on the
     * radio pointer.
     */
    virtual RadioLink linkFor(UserControlInfo *lteInfo);

    /*
     * One interfering transmitter, normalized across the two kinds the UL
     * transmission map can hold: a real UE (with a PHY) and a background UE
     * (with a traffic generator). Public, with describeInterferer() below,
     * for CellularInterferenceModel's computeUplinkInterference() and
     * computeD2DInterference(), which both call describeInterferer()
     * by explicit qualification.
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
     * physical-fact reads against the medium's registry; the background-UE
     * branch stays unbridged since a traffic generator is not a registered radio.
     */
    static InterfererInfo describeInterferer(const UeAllocationInfo& allocation, RadioMedium *medium, GHz carrierFrequency);

    /*
     * Compute attenuation due to path loss and shadowing, always with the
     * TR 36.814 formulas regardless of pathLossType. Public for
     * CellularInterferenceModel's computeExtCellInterference()/
     * computeBackgroundCellInterference(), which call it back on the
     * radio pointer: it reads the medium's shared LOS state (losStateFor())
     * and shared per-leg Tr36814 strategy (extCellPathLossFor())
     * for radio's own carrier leg, plus this endpoint's own inside_building_/
     * inside_distance_ (not reachable from outside), so it stays resident
     * rather than moving with the walks that call it.
     * @return attenuation expressed in dBm
     */
    virtual double computeExtCellPathLoss(double dist, const LinkKey& key);

  protected:

    /*
     * Build the RadioLink for a UE<->serving-BS link expressed the old way: the
     * local module is one endpoint, 'coord' the other, and 'dir' says which of
     * the two is the UE.
     */
    RadioLink cellularLink(MacNodeId ueId, Direction dir, inet::Coord coord);

    /*
     * Compute received useful signal (RSRP) per band over a radio link.
     * Protected: the only caller is
     * D2dChannelModel::getRSRP_D2D() (a subclass, reached through an
     * unqualified self-call) -- the
     * medium computes RSRP itself rather than calling back through radio.
     */
    virtual std::vector<double> getRSRP(const RadioLink& link, double txPower);

    /*
     * Add noise and interference to an already-computed per-band received-power
     * vector. Protected, for the same reason as
     * getRSRP(const RadioLink&, double) above: only D2dChannelModel's own
     * one-to-many capture-effect path (D2dChannelModel::getSINR_D2D())
     * calls it.
     */
    virtual std::vector<double> getSINR(const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector);
};

} //namespace

#endif /* STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_ */

