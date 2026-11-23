//
//                  Simu5G
//
// Copyright (C) 2026 Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef TESTS_UNIT_LIB_SCHEDULERHARNESS_H_
#define TESTS_UNIT_LIB_SCHEDULERHARNESS_H_

#include <list>
#include <map>
#include <vector>

#include "MacTestStubs.h"
#include "simu5g/background/trafficGenerator/IBackgroundTrafficManager.h"
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/mac/allocator/LteAllocationModule.h"
#include "simu5g/stack/mac/amc/LteAmc.h"
#include "simu5g/stack/mac/buffer/harq/LteHarqBufferTx.h"
#include "simu5g/stack/mac/buffer/harq/LteHarqProcessTx.h"

namespace simu5g {
namespace unittest {

[[noreturn]] inline void unexpected(const char *method)
{
    throw omnetpp::cRuntimeError("RecordingBgManager: unexpected call to %s()", method);
}

/**
 * A background traffic manager with one backlogged UE that records the
 * allocation maps registered with it. The background UEs waiting for a RAC
 * grant are given by index; index i is the UE firstBgUeId + i.
 */
class RecordingBgManager : public IBackgroundTrafficManager
{
  public:
    unsigned int newDataBytes = 100;
    unsigned int rtxBytes = 8;
    unsigned int bytesPerBlock = 10;
    int registrations = 0;
    RbMap registered;
    MacNodeId firstBgUeId = MacNodeId(BGUE_MIN_ID);
    std::list<int> waitingForRac;
    std::vector<MacNodeId> racServed;

    unsigned int getBackloggedUeBuffer(MacNodeId bgUeId, Direction dir, bool rtx) override { return rtx ? rtxBytes : newDataBytes; }
    unsigned int getBackloggedUeBytesPerBlock(MacNodeId bgUeId, Direction dir) override { return bytesPerBlock; }
    unsigned int consumeBackloggedUeBytes(MacNodeId bgUeId, unsigned int bytes, Direction dir, bool rtx) override { return 0; }
    void registerTransmission(MacNodeId bgUeId, Direction dir, GHz carrierFrequency, const RbMap& allocatedRbMap) override
    {
        registrations++;
        registered = allocatedRbMap;
    }

    void setCarrierFrequency(GHz carrierFrequency) override { unexpected("setCarrierFrequency"); }
    unsigned int getNumBands() override { unexpected("getNumBands"); }
    void notifyBacklog(int index, Direction dir, bool rtx) override { unexpected("notifyBacklog"); }
    Cqi computeCqi(int bgUeIndex, Direction dir) override { unexpected("computeCqi"); }
    Cqi computeCqiFromSinr(double sinr) override { unexpected("computeCqiFromSinr"); }
    CellularPhantomRadio *getBgUeRadio(MacNodeId bgUeId) override { unexpected("getBgUeRadio"); }
    MacNodeId getBgUeId(int bgUeIndex) override { return MacNodeId(num(firstBgUeId) + bgUeIndex); }
    int getBgUeIndex(MacNodeId bgUeId) override { unexpected("getBgUeIndex"); }
    std::vector<TrafficGeneratorBase *>::const_iterator getBgUesBegin() override { unexpected("getBgUesBegin"); }
    std::vector<TrafficGeneratorBase *>::const_iterator getBgUesEnd() override { unexpected("getBgUesEnd"); }
    std::list<int>::const_iterator getBackloggedUesBegin(Direction dir, bool rtx) override { unexpected("getBackloggedUesBegin"); }
    std::list<int>::const_iterator getBackloggedUesEnd(Direction dir, bool rtx) override { unexpected("getBackloggedUesEnd"); }
    std::list<int>::const_iterator getWaitingForRacUesBegin() override { return waitingForRac.begin(); }
    std::list<int>::const_iterator getWaitingForRacUesEnd() override { return waitingForRac.end(); }
    void racHandled(MacNodeId bgUeId) override { racServed.push_back(bgUeId); }
};

/**
 * An AMC that gives every UE the same transmission parameters (one codeword,
 * on the given bands) unless it has its own, and a fixed number of bytes per
 * resource block.
 */
class StubAmc : public LteAmc
{
  public:
    UserTxParams params;
    std::map<MacNodeId, UserTxParams> ueParams;
    unsigned int bytesPerBlock = 10;

    const UserTxParams& computeTxParams(MacNodeId id, const Direction dir, GHz carrierFrequency) override
    {
        auto it = ueParams.find(id);
        return it != ueParams.end() ? it->second : params;
    }
    unsigned int computeBytesOnNRbs(MacNodeId id, Band b, unsigned int blocks, const Direction dir, GHz carrierFrequency) override { return blocks * bytesPerBlock; }
    unsigned int computeBytesOnNRbs(MacNodeId id, Band b, Codeword cw, unsigned int blocks, const Direction dir, GHz carrierFrequency) override { return blocks * bytesPerBlock; }
};

/**
 * A UE's downlink HARQ buffer whose every process has a PDU of the given
 * length to retransmit. Its processes have no units: the scheduler reads the
 * length, not the units.
 */
class StubHarqBufferTx : public LteHarqBufferTx
{
  public:
    unsigned int length;
    StubHarqBufferTx(LteMacBase *owner, unsigned int numProc, unsigned int length)
        : LteHarqBufferTx(nullptr, numProc, owner), length(length)
    {
        for (unsigned int i = 0; i < numProc; i++)
            processes_[i] = new LteHarqProcessTx(nullptr, i, 0, numProc, owner, owner);
    }
    int64_t pduLength(unsigned char acid, Codeword cw) override { return length; }
    void markSelected(UnitList unitIds, unsigned char availableTbs) override {}
};

/**
 * A cell's carriers, registered by hand. Never initialize()d.
 */
class TestCellInfo : public CellInfo
{
  protected:
    void initialize(int stage) override {}
};

/**
 * An eNB MAC in a cell with the given carriers, whose background traffic
 * manager and AMC are the stubs above, with a downlink HARQ buffer per UE on
 * request.
 */
class TestEnbMac : public StubEnbMac
{
  public:
    RecordingBgManager manager;
    TestCellInfo cellInfo;
    StubAmc amc;
    TestEnbMac(std::vector<GHz> carriers, unsigned int numBands)
    {
        for (GHz carrier : carriers)
            cellInfo.registerCarrier(carrier, numBands, 0);
    }
    CellInfo *getCellInfo() override { return &cellInfo; }
    IBackgroundTrafficManager *getBackgroundTrafficManager(GHz carrierFrequency) override { return &manager; }
    LteAmc *getAmc() override { return &amc; }
    void addHarqBuffer(GHz carrier, MacNodeId ueId, unsigned int pduLength)
    {
        harqTxBuffers_[carrier][ueId] = new StubHarqBufferTx(this, 8, pduLength);
    }
};

/**
 * A scheduler of the given direction set up by hand on the given MAC, with its
 * own allocator, its protected seams under test exposed.
 */
template<class Base>
class TestScheduler : public Base
{
  public:
    using Base::allocateRtxBytes;
    using Base::applyAllowedBandLimits;
    using Base::schedulePerAcidRtx;

    void setup(LteMacEnb *mac, Direction dir, unsigned int numBands)
    {
        this->mac_ = mac;
        this->direction_ = dir;
        this->allocator_ = new LteAllocationModule(mac, dir);
        this->allocator_->init(numBands, numBands);
    }
    LteAllocationModule *allocator() { return this->allocator_; }

  protected:
    void initialize(int stage) override {}
};

} //namespace unittest
} //namespace simu5g

#endif
