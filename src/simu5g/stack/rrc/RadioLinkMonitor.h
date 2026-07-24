//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_RADIOLINKMONITOR_H_
#define _LTE_RADIOLINKMONITOR_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/common/LteDefs.h"
#include "simu5g/common/LteTypes.h"

namespace simu5g {

using namespace omnetpp;

class Binder;
class BearerManagement;

/**
 * UE-side radio link monitoring (RLF detection; abstraction of TS 38.331 5.3.10).
 *
 * HandoverController measures each serving-cell beacon and feeds the RSSI in via
 * servingCellBeaconReceived(). RSSI below qOut_ counts as an out-of-sync indication,
 * at/above qIn_ as in-sync. n310_ consecutive out-of-sync indications start T310;
 * n311_ consecutive in-sync indications while T310 runs stop it; T310 expiry declares
 * RLF via BearerManagement::scheduleRadioLinkFailure. A watchdog declares RLF directly
 * when serving-cell beacons stop arriving altogether.
 */
class RadioLinkMonitor : public cSimpleModule
{
  protected:
    MacNodeId nodeId_ = NODEID_NONE;
    bool isNr_ = false;

    bool enabled_ = false;
    double qOut_ = 0;                 // dB; below => out-of-sync indication
    double qIn_ = 0;                  // dB; at/above => in-sync indication
    int n310_ = 0;
    int n311_ = 0;
    simtime_t t310_;
    simtime_t beaconLossTimeout_;

    MacNodeId monitoredCell_ = NODEID_NONE;  // serving cell the RLM state refers to
    int oosCounter_ = 0;              // consecutive out-of-sync indications (N310 counter)
    int isCounter_ = 0;               // consecutive in-sync indications while T310 runs (N311 counter)
    bool rlfDeclared_ = false;        // RLF fired; monitoring suspended until link recovers (>= qIn_)
    long numBeaconsSeen_ = 0;         // distinguishes "beacons stopped" from "never beaconing" (config error)
    cMessage *t310Timer_ = nullptr;
    cMessage *beaconLossTimer_ = nullptr;

    inet::ModuleRefByPar<Binder> binder_;
    BearerManagement *bearerManagement_ = nullptr;

  protected:
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;

    // Declare RLF: suspend monitoring and hand over to BearerManagement's teardown machinery
    void declareRadioLinkFailure(MacNodeId servingNodeId, const char *cause);
    // Clear counters/timers (serving cell changed or stale T310)
    void reset();

  public:
    ~RadioLinkMonitor() override;

    bool isEnabled() const { return enabled_; }

    /**
     * Called by HandoverController for every measured serving-cell beacon.
     */
    void servingCellBeaconReceived(MacNodeId servingNodeId, double rssi);
};

} //namespace

#endif /* _LTE_RADIOLINKMONITOR_H_ */
