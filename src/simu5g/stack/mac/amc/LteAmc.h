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

#ifndef _LTE_LTEAMC_H_
#define _LTE_LTEAMC_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/phy/feedback/LteFeedback.h"
#include "simu5g/stack/phy/feedback/LteSummaryBuffer.h"
#include "simu5g/stack/mac/amc/AmcPilot.h"
#include "simu5g/stack/mac/amc/LteMcs.h"
#include "simu5g/stack/mac/amc/UserTxParams.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

using namespace omnetpp;

/// Forward declaration of AmcPilot class, used by LteAmc.
class AmcPilot;
/// Forward declaration of CellInfo class, used by LteAmc.
class CellInfo;
/// Forward declaration of LteMacEnb class, used by LteAmc.
class LteMacEnb;

typedef std::map<Remote, std::vector<std::vector<LteSummaryBuffer>>> History_;

/**
 * @brief Lte AMC module for Omnet++ simulator
 *
 * TODO
 */
class LteAmc : public cSimpleModule
{
  protected:
    /// Factory for the AMC pilot selected by the amcMode parameter; supported names depend on the AMC implementation.
    virtual AmcPilot *createAmcPilot(const char *amcMode);

    // D2D routing seams: the base services DL/UL directly and routes any other
    // (D2D) direction here. The base implementations reject the direction; the
    // D2D AMC subclasses override them to service the D2D structures.
    virtual void printTxParamsForDirection(Direction dir, GHz carrierFrequency);
    virtual bool existTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency);
    virtual const UserTxParams& setTxParamsForDirection(MacNodeId id, Direction dir, UserTxParams& info, GHz carrierFrequency);
    virtual const UserTxParams& getTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency);
    virtual McsTable *getMcsTableForDirection(Direction dir);
    virtual void rescaleMcsForDirection(double rePerRb, Direction dir);
    virtual void detachUserForDirection(MacNodeId nodeId, Direction dir);
    virtual void attachUserForDirection(MacNodeId nodeId, Direction dir);
    virtual void testUeForDirection(MacNodeId nodeId, Direction dir);

  public:
    void printParameters();
    void printFbhb(Direction dir);
    void printTxParams(Direction dir, GHz carrierFrequency);

  protected:
    opp_component_ptr<LteMacEnb> mac_;
    opp_component_ptr<Binder> binder_;
    opp_component_ptr<CellInfo> cellInfo_;
    AmcPilot *pilot_ = nullptr;
    RbAllocationType allocationType_;
    int numBands_;
    MacNodeId nodeId_;
    MacCellId cellId_;
    McsTable dlMcsTable_;
    McsTable ulMcsTable_;
    double mcsScaleDl_;
    double mcsScaleUl_;
    int numAntennas_;
    RemoteSet remoteSet_;
    ConnectedUesMap dlConnectedUe_;
    ConnectedUesMap ulConnectedUe_;
    std::map<MacNodeId, unsigned int> dlNodeIndex_;
    std::map<MacNodeId, unsigned int> ulNodeIndex_;
    std::vector<MacNodeId> dlRevNodeIndex_;
    std::vector<MacNodeId> ulRevNodeIndex_;

    // one tx param per carrier
    std::map<GHz, std::vector<UserTxParams>> dlTxParams_;
    std::map<GHz, std::vector<UserTxParams>> ulTxParams_;

    int fType_; //CQI synchronization Debugging

    // one History per carrier
    std::map<GHz, History_> dlFeedbackHistory_;
    std::map<GHz, History_> ulFeedbackHistory_;

    unsigned int fbhbCapacityDl_;
    unsigned int fbhbCapacityUl_;
    simtime_t lb_;
    simtime_t ub_;
    double cqiComputationWeight_;

    History_ *getHistory(Direction dir, GHz carrierFrequency);

  public:
    LteAmc() {}
    virtual ~LteAmc();

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override { throw cRuntimeError("LteAmc does not handle messages"); }

  public:
    void setfType(int f)
    {
        fType_ = f;
    }

    int getfType()
    {
        return fType_;
    }

    virtual void pushFeedback(MacNodeId id, Direction dir, LteFeedback fb, GHz carrierFrequency);
    virtual const LteSummaryFeedback& getFeedback(MacNodeId id, Remote antenna, TxMode txMode, const Direction dir, GHz carrierFrequency);

    const RemoteSet *getAntennaSet()
    {
        return &remoteSet_;
    }

    bool existTxParams(MacNodeId id, const Direction dir, GHz carrierFrequency);
    virtual const UserTxParams& getTxParams(MacNodeId id, const Direction dir, GHz carrierFrequency);
    virtual const UserTxParams& setTxParams(MacNodeId id, const Direction dir, UserTxParams& info, GHz carrierFrequency);
    virtual const UserTxParams& computeTxParams(MacNodeId id, const Direction dir, GHz carrierFrequency);
    virtual unsigned int computeBitsOnNRbs(MacNodeId id, Band b, unsigned int blocks, const Direction dir, GHz carrierFrequency);
    virtual unsigned int computeBitsOnNRbs(MacNodeId id, Band b, Codeword cw, unsigned int blocks, const Direction dir, GHz carrierFrequency);
    virtual unsigned int computeBytesOnNRbs(MacNodeId id, Band b, unsigned int blocks, const Direction dir, GHz carrierFrequency);
    virtual unsigned int computeBytesOnNRbs(MacNodeId id, Band b, Codeword cw, unsigned int blocks, const Direction dir, GHz carrierFrequency);

    virtual unsigned int computeBitsPerRbBackground(Cqi cqi, const Direction dir, GHz carrierFrequency);

    // multiband version of the above function. It returns the number of bytes that can fit in the given "blocks" of the given "band"
    virtual unsigned int computeBytesOnNRbs_MB(MacNodeId id, Band b, unsigned int blocks, const Direction dir, GHz carrierFrequency);
    virtual unsigned int computeBitsOnNRbs_MB(MacNodeId id, Band b, unsigned int blocks, const Direction dir, GHz carrierFrequency);
    bool setPilotUsableBands(MacNodeId id, UsableBands usableBands);
    UsableBands *getPilotUsableBands(MacNodeId id);

    // utilities - do not involve pilot invocation
    virtual unsigned int getItbsPerCqi(Cqi cqi, const Direction dir);

    /*
     * Access the correct itbs2tbs conversion table given cqi and layer number
     */
    virtual const unsigned int *readTbsVect(Cqi cqi, unsigned int layers, Direction dir);

    /*
     * given <cqi> and <layers> returns bytes allocable in <blocks>
     */
    virtual unsigned int blockGain(Cqi cqi, unsigned int layers, unsigned int blocks, Direction dir);

    /*
     * given <cqi> and <layers> returns blocks capable of carrying  <bytes>
     */
    virtual unsigned int bytesGain(Cqi cqi, unsigned int layers, unsigned int bytes, Direction dir);

    // ---------------------------
    void writeCqiWeight(double weight);
    virtual Cqi readWbCqi(const CqiVector& cqi);
    virtual void detachUser(MacNodeId nodeId, Direction dir);
    virtual void attachUser(MacNodeId nodeId, Direction dir);
    void testUe(MacNodeId nodeId, Direction dir);
    AmcPilot *getPilot() const
    {
        return pilot_;
    }

    CellInfo *getCellInfo()
    {
        return cellInfo_;
    }

    Binder *getBinder()
    {
        return binder_;
    }

    MacNodeId getMacNodeId()
    {
        return nodeId_;
    }

    // Resolve the next hop (serving node) for the given destination, or the
    // destination itself if this cell is its master.
    MacNodeId getServingNodeOrSelf(MacNodeId dst);

    inet::Coord getUePosition(MacNodeId id)
    {
        return cellInfo_->getUePosition(id);
    }

    virtual std::vector<Cqi> readMultiBandCqi(MacNodeId id, const Direction dir, GHz carrierFrequency);

    int getSystemNumBands() { return numBands_; }

    void setPilotMode(PilotComputationModes mode);
};

} //namespace

#endif
