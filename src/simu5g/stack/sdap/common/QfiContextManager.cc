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

#include "simu5g/stack/sdap/common/QfiContextManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

namespace simu5g {

std::ostream& operator<<(std::ostream& os, const QfiContext& ctx) {
    os << "QfiContext{qfi=" << ctx.qfi
       << ", drbIndex=" << ctx.drbIndex
       << ", fiveQi=" << ctx.fiveQi
       << ", isGbr=" << (ctx.isGbr ? "Yes" : "No")
       << ", delayBudgetMs=" << ctx.delayBudgetMs
       << ", packetErrorRate=" << ctx.packetErrorRate
       << ", priorityLevel=" << ctx.priorityLevel
       << ", description=\"" << ctx.description << "\"}";
    return os;
}

Define_Module(QfiContextManager);

void QfiContextManager::initialize()
{
    std::string configFile = par("qfiContextFile").stdstringValue();
    if (!configFile.empty()) {
        loadFromFile(configFile);
        EV << "QfiContextManager: Loaded " << qfiMap_.size() << " QFI entries from " << configFile << endl;
    }
    WATCH_MAP(qfiMap_);
    WATCH_MAP(cidToQfi_);
    WATCH_MAP(qfiToCid_);
}

void QfiContextManager::registerQfiForCid(MacCid cid, int qfi) {
    EV_INFO << "QfiContextManager::registerQfiForCid - registering CID=" << cid << " with QFI=" << qfi << endl;
    cidToQfi_[cid] = qfi;
    qfiToCid_[qfi] = cid;
}

int QfiContextManager::getQfiForCid(MacCid cid) const {
    auto it = cidToQfi_.find(cid);
    return it != cidToQfi_.end() ? it->second : -1;
}

MacCid QfiContextManager::getCidForQfi(int qfi) const {
    auto it = qfiToCid_.find(qfi);
    return it != qfiToCid_.end() ? it->second : MacCid();
}

const QfiContext* QfiContextManager::getContextByQfi(int qfi) const {
    auto it = qfiMap_.find(qfi);
    return it != qfiMap_.end() ? &it->second : nullptr;
}

const std::map<MacCid, int>& QfiContextManager::getCidToQfiMap() const {
    return cidToQfi_;
}

const std::map<int, QfiContext>& QfiContextManager::getQfiMap() const {
    return qfiMap_;
}

// Placeholder for file loader
void QfiContextManager::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open())
        throw cRuntimeError("QfiContextManager: Failed to open file '%s'", filename.c_str());

    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        QfiContext ctx;
        std::string gbrStr;

        if (iss >> ctx.qfi >> ctx.drbIndex >> ctx.fiveQi >> gbrStr
                >> ctx.delayBudgetMs >> ctx.packetErrorRate
                >> ctx.priorityLevel >> std::ws)
        {
            ctx.isGbr = (gbrStr == "Yes" || gbrStr == "yes");
            std::getline(iss, ctx.description);
            qfiMap_[ctx.qfi] = ctx;
        }
    }

    if (in.bad())
        throw cRuntimeError("QfiContextManager: Failed to read file '%s'", filename.c_str());

    in.close();
}
void QfiContextManager::dump(std::ostream& os) const {
    os << "=== QfiContextManager dump ===" << std::endl;
    os << "QFI Map (" << qfiMap_.size() << " entries):" << std::endl;
    for (const auto& [qfi, ctx] : qfiMap_) {
        os << "  " << qfi << " -> " << ctx << std::endl;
    }
    os << "CID to QFI Map (" << cidToQfi_.size() << " entries):" << std::endl;
    for (const auto& [cid, qfi] : cidToQfi_) {
        os << "  CID=" << cid << " -> QFI=" << qfi << std::endl;
    }
    os << "QFI to CID Map (" << qfiToCid_.size() << " entries):" << std::endl;
    for (const auto& [qfi, cid] : qfiToCid_) {
        os << "  QFI=" << qfi << " -> CID=" << cid << std::endl;
    }
    os << "===============================" << std::endl;
}

} // namespace simu5g

