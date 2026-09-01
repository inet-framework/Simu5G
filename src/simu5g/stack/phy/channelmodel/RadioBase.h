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

#ifndef STACK_PHY_CHANNELMODEL_RADIOBASE_H_
#define STACK_PHY_CHANNELMODEL_RADIOBASE_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/carrierAggregation/ComponentCarrier.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
namespace simu5g {

using namespace inet;
using namespace omnetpp;

class LteAirFrame;
class PhyBase;
class Binder;

/**
 * Identifies a radio link for the purpose of indexing its channel state.
 *
 * LOS/NLOS, shadowing and multipath fading are properties of a *link*, not of a
 * node: two links from the same transmitter to peers in different directions and
 * at different distances have independent realizations. Keying that state by node
 * alone is harmless for cellular, where
 * a UE has exactly one link per channel-model instance, but wrong for D2D, where
 * one UE has many peers that would all share a single slot.
 *
 * The pair is stored normalized so that a link is the same key seen from either
 * end. Cellular callers construct the degenerate key {id, id}, keying by node
 * alone -- exactly right since a UE has only one cellular link.
 */
struct LinkKey
{
    MacNodeId a = NODEID_NONE;
    MacNodeId b = NODEID_NONE;

    LinkKey() = default;
    explicit LinkKey(MacNodeId node) : a(node), b(node) {}
    LinkKey(MacNodeId x, MacNodeId y)
        : a(num(x) <= num(y) ? x : y), b(num(x) <= num(y) ? y : x) {}

    bool operator<(const LinkKey& o) const
    {
        return num(a) != num(o.a) ? num(a) < num(o.a) : num(b) < num(o.b);
    }
};

inline std::ostream& operator<<(std::ostream& os, const LinkKey& k)
{
    return num(k.a) == num(k.b) ? (os << k.a) : (os << "[" << k.a << "," << k.b << "]");
}

/**
 * A radio link between two arbitrary endpoints, and the parameters the channel
 * model needs to evaluate it.
 *
 * Geometry, channel-state key and link budget are data. `dir` survives only
 * as a tag: it selects the statistic to emit and dispatches the (genuinely
 * cellular-topology-aware) interference computation, but it does not derive
 * any of the link geometry or the link budget.
 */
struct RadioLink
{
    // ---- geometry ----
    MacNodeId txId = NODEID_NONE;
    MacNodeId rxId = NODEID_NONE;
    inet::Coord txCoord;
    inet::Coord rxCoord;

    // ---- channel state ----
    // stateKey indexes the medium's per-link state: losMap_, lastComputedSF_
    // and jakesFadingMap_, keyed together with the caller's own carrier leg
    // -- one shared entry per physical link, regardless of
    // which end asks.
    LinkKey stateKey;

    // stateNodeId is the *node* the state belongs to -- the UE. It indexes
    // positionHistory_, which is genuinely a node property because it
    // defines the node's speed, and it distinguishes background UEs.
    MacNodeId stateNodeId = NODEID_NONE;

    inet::Coord stateCoord;      // position feeding computeSpeed + correlation distance

    // Which of the calling radio's (possibly several) carriers
    // this link is on -- keys the medium's per-leg state (pathLoss_,
    // losMap_, positionHistory_, ...) alongside stateKey/stateNodeId above.
    // One endpoint can serve several carriers, so this travels with the
    // link rather than being implicit in the endpoint's own identity.
    GHz carrierFrequency = GHz(0);

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

/**
 * Abstract base for the radios a PHY layer can be equipped with.
 *
 * This is not a propagation model: it is the interface to the entire PHY link
 * model. A radio is asked for received power (getRSRP), for per-band
 * SINR (getSINR, getSINR_bgUe), and for the reception decision itself
 * (isReceptionSuccessful), so it owns propagation, fading, interference and
 * the SINR-to-error mapping alike. Path loss (getAttenuation,
 * computePathLoss) is one ingredient among those, not the subject of the
 * class.
 *
 * The class and interface names are RAT-neutral: the concrete radios differ
 * in which 3GPP propagation study supplies their formulas (selected on the
 * medium's matching carrier leg, not on the endpoint), never in whether they
 * serve an LTE or an NR carrier, and any of
 * them can be plugged into the radioType slot of any NIC through the
 * IRadio interface. Everything
 * technology-dependent -- carrier frequency, bandwidth, numerology -- is
 * read from the ComponentCarrier module rather than encoded in the class.
 */
class RadioBase : public cSimpleModule
{
  protected:
    // Reference to Binder module
    inet::ModuleRefByPar<Binder> binder_;

    // Reference to cell info module
    inet::ModuleRefByPar<CellInfo> cellInfo_;

    // Reference to the corresponding PHY layer
    opp_component_ptr<PhyBase> phy_;

    // The component carriers this radio serves, resolved from
    // componentCarrierModules in DECLARATION order -- the order
    // cellInfo_->registerCarrier and PhyBase's binder_->registerCarrierUe
    // sweep register in.
    std::vector<ComponentCarrier *> componentCarriers_;

    /*
     * getNumBands(GHz)/getNumerologyIndex(GHz)'s shared lookup: a real scan
     * among the carriers this radio actually serves (componentCarriers_),
     * throwing rather than silently answering for the wrong one.
     */
    ComponentCarrier *carrierFor(GHz carrierFrequency) const
    {
        for (auto *cc : componentCarriers_)
            if (cc->getCarrierFrequency() == carrierFrequency)
                return cc;
        throw cRuntimeError("%s: carrier %gGHz is not served by this radio", getFullPath().c_str(), carrierFrequency.get());
    }

    // Carrier Frequency of the PRIMARY (first-declared) carrier, and its
    // base-10 logarithm. Single-valued because most readers -- every
    // single-carrier leg, 181 of 187 fingerprint rows -- have exactly one
    // carrier to mean; CA-aware callers use the explicit-carrier overloads
    // below instead of this cached value.
    GHz carrierFrequency_;
    double carrierFrequencyHz_;
    double carrierFrequencyGHz_;
    double log10CarrierFrequencyGHz_;

    // Number of bands of the PRIMARY carrier -- kept single-valued for
    // IdealRadio/D2dRadio, which read this member directly
    // rather than through getNumBands(GHz) and are never configured with
    // more than one carrier in tree.
    unsigned int numBands_ = -1;

  public:

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    /*
     * Returns the carrier frequency
     */
    virtual GHz getCarrierFrequency() const { return GHz(carrierFrequencyGHz_); }

    /*
     * Returns the number of logical bands for the given carrier: a real
     * lookup among the carriers this radio actually serves
     * (componentCarrierModules), throwing rather than silently answering
     * for the wrong one.
     */
    virtual unsigned int getNumBands(GHz carrierFrequency) const
    {
        return carrierFor(carrierFrequency)->getNumBands();
    }

    /*
     * Legacy no-argument form, kept only for BackgroundCellChannelModel.cc's
     * one remaining caller: that file is out of scope for this step (it is
     * slated for deletion by parent follow-up 1) and must not be touched.
     * Remove this overload when that caller goes away. Answers for the
     * PRIMARY carrier, exactly as the old single-carrier cache did.
     */
    virtual unsigned int getNumBands() const { return numBands_; }

    /*
     * Returns the numerology index for the given carrier (see getNumBands
     * for why the argument is required).
     */
    virtual unsigned int getNumerologyIndex(GHz carrierFrequency) const
    {
        return carrierFor(carrierFrequency)->getNumerologyIndex();
    }

    /*
     * The carriers this radio serves, in componentCarrierModules'
     * declaration order: what PhyBase's initializeRadio() iterates
     * for the binder_->registerCarrierUe sweep.
     */
    const std::vector<ComponentCarrier *>& getComponentCarriers() const { return componentCarriers_; }

    virtual void setPhy(PhyBase *phy) { phy_ = phy; }

    /*
     * Compute the error probability of the transmitted packet according to CQI used, TX mode, and the received power
     * After that, it generates a random number to check if this packet will be corrupted or not
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     * @param rsrpVector the per-band received power captured for this frame, when the
     *        caller already has it (the D2D one-to-many capture-effect path). Empty
     *        otherwise; models that do not need it ignore it.
     */
    virtual bool isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector = {}) = 0;

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
     * Compute received useful signal for each band for user nodeId according to path loss, shadowing (optional), and multipath fading
     *
     * @param frame pointer to the packet
     * @param lteInfo pointer to the user control info
     */
    virtual std::vector<double> getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo) = 0;

    virtual bool isUplinkInterferenceEnabled() { return false; }

    /// Whether transmissions must be recorded in the Binder's UL transmission map
    /// (used by interference computation on the receive path).
    virtual bool recordsUlTransmissionMap() { return isUplinkInterferenceEnabled(); }
};

} //namespace

#endif

