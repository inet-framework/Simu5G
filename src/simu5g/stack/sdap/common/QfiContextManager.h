//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
///


#ifndef STACK_SDAP_COMMON_QFICONTEXTMANAGER_H_
#define STACK_SDAP_COMMON_QFICONTEXTMANAGER_H_

#pragma once

#include <omnetpp.h>
#include <string>
#include <map>
#include "QfiContext.h"
#include "simu5g/common/LteCommon.h"

using namespace omnetpp;

namespace simu5g {

class QfiContextManager : public cSimpleModule
{
  protected:
    std::map<int, QfiContext> qfiMap_;           // QFI -> QoS context
    std::map<MacCid, int> cidToQfi_;             // CID -> QFI
    std::map<int, MacCid> qfiToCid_;             // QFI -> CID

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override { throw cRuntimeError("QfiContextManager does not process messages"); }

  public:
    void loadFromFile(const std::string& filename);

    // Register a single QFI ↔ CID pair
    void registerQfiForCid(MacCid cid, int qfi);

    // Accessors
    const QfiContext* getContextByQfi(int qfi) const;
    int getQfiForCid(MacCid cid) const;
    MacCid getCidForQfi(int qfi) const;
    const std::map<int, QfiContext>& getQfiMap() const;
    const std::map<MacCid, int>& getCidToQfiMap() const;

    void dump(std::ostream& os = std::cout) const;
};

} // namespace simu5g

#endif /* STACK_SDAP_COMMON_QFICONTEXTMANAGER_H_ */
