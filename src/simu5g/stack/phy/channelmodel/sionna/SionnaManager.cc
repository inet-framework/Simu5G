//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/phy/channelmodel/sionna/SionnaManager.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/carrierAggregation/ComponentCarrier.h"
#include "simu5g/stack/phy/LtePhyBase.h"
#include "simu5g/mec/utils/httpUtils/json.hpp"

namespace simu5g {

using json = nlohmann::json;
namespace fs = std::filesystem;

Define_Module(SionnaManager);

long long SionnaManager::carrierKey(GHz carrier)
{
    // round the carrier frequency to the nearest Hz for a stable integer key
    return llround(Hz(carrier).get());
}

void SionnaManager::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
        carrierAggregationModulePar_ = par("carrierAggregationModule").stdstringValue();

        sceneFile_ = par("sceneFile").stdstringValue();
        groundPermittivity_ = par("groundPermittivity");
        groundConductivity_ = par("groundConductivity");
        sceneSize_ = par("sceneSize").doubleValue();
        numReflections_ = par("numReflections");
        polarization_ = par("polarization").stdstringValue();

        granularity_ = (std::string(par("granularity").stringValue()) == "wideband")
            ? Granularity::WIDEBAND : Granularity::PER_RB;
        interferenceMode_ = (std::string(par("interferenceMode").stringValue()) == "allPairs")
            ? InterferenceMode::ALL_PAIRS : InterferenceMode::NOISE_LIMITED;

        channelTableFile_ = par("channelTableFile").stdstringValue();
        cacheDir_ = par("cacheDir").stdstringValue();
        forceRegenerate_ = par("forceRegenerate");
        pythonExecutable_ = par("pythonExecutable").stdstringValue();
        sionnaScript_ = par("sionnaScript").stdstringValue();
        backend_ = par("backend").stdstringValue();

        warnOnCouplingGuard();
    }
    else if (stage == inet::INITSTAGE_LAST) {
        // positions are final and all nodes have registered with the Binder
        ensureTable();
    }
}

void SionnaManager::warnOnCouplingGuard()
{
    // Plan A §8: a single wideband value cannot represent RB-selective interference.
    if (granularity_ == Granularity::WIDEBAND && interferenceMode_ == InterferenceMode::ALL_PAIRS)
        EV_WARN << "SionnaManager: 'wideband' granularity with 'allPairs' interference is "
                   "inconsistent - interference will be treated as band-averaged (an "
                   "approximation). Prefer 'perRb' for multi-cell scenarios.\n";
}

void SionnaManager::ensureTable()
{
    if (tableReady_)
        return;
    tableReady_ = true; // set early so a failed build is not retried per lookup

    // ---- enumerate carriers -------------------------------------------------
    cModule *caModule = getModuleByPath(carrierAggregationModulePar_.c_str());
    std::vector<ComponentCarrier *> carriers;
    if (caModule != nullptr) {
        for (int i = 0; ; ++i) {
            cModule *cc = caModule->getSubmodule("componentCarrier", i);
            if (cc == nullptr)
                break;
            carriers.push_back(check_and_cast<ComponentCarrier *>(cc));
        }
    }
    if (carriers.empty())
        throw cRuntimeError("SionnaManager: no component carriers found at '%s'",
                carrierAggregationModulePar_.c_str());

    // ---- enumerate nodes (id, role, position) ------------------------------
    struct NodeRec { MacNodeId id; const char *role; inet::Coord pos; };
    std::vector<NodeRec> nodes;
    auto addNode = [&](MacNodeId id, const char *role, LtePhyBase *phy) {
        if (phy != nullptr)
            nodes.push_back({ id, role, phy->getCoord() });
    };
    for (auto *e : binder_->getEnbList())
        addNode(e->id, "enb", e->phy);
    for (auto *u : binder_->getUeList())
        addNode(u->id, "ue", u->phy);

    if (nodes.empty())
        throw cRuntimeError("SionnaManager: no nodes found via Binder; cannot build channel table");

    // ---- build the request --------------------------------------------------
    json req;
    req["version"] = 1;
    req["backend"] = backend_;
    req["scene"] = {
        { "type", sceneFile_.empty() ? "flatGround" : "sceneFile" },
        { "sceneFile", sceneFile_ },
        { "groundPermittivity", groundPermittivity_ },
        { "groundConductivity", groundConductivity_ },
        { "sizeMeters", sceneSize_ },
        { "numReflections", numReflections_ },
    };
    req["interferenceMode"] =
        (interferenceMode_ == InterferenceMode::ALL_PAIRS) ? "allPairs" : "noiseLimited";
    req["granularity"] =
        (granularity_ == Granularity::WIDEBAND) ? "wideband" : "perRb";
    req["polarization"] = polarization_;

    req["carriers"] = json::array();
    for (auto *cc : carriers)
        req["carriers"].push_back({
            { "carrierFrequencyHz", Hz(cc->getCarrierFrequency()).get() },
            { "numBands", cc->getNumBands() },
            { "numerology", cc->getNumerologyIndex() },
        });

    req["nodes"] = json::array();
    for (auto& n : nodes)
        req["nodes"].push_back({
            { "id", num(n.id) },
            { "role", n.role },
            { "pos", { n.pos.x, n.pos.y, n.pos.z } },
            { "antennaGainDb", 0.0 },
        });

    // explicit ordered links: serving links (both directions) or the full matrix
    req["links"] = json::array();
    auto addLink = [&](MacNodeId tx, MacNodeId rx) {
        req["links"].push_back({ { "tx", num(tx) }, { "rx", num(rx) } });
    };
    if (interferenceMode_ == InterferenceMode::ALL_PAIRS) {
        for (auto& a : nodes)
            for (auto& b : nodes)
                if (a.id != b.id)
                    addLink(a.id, b.id);
    }
    else {
        for (auto& e : nodes)
            if (std::string(e.role) == "enb")
                for (auto& u : nodes)
                    if (std::string(u.role) == "ue") {
                        addLink(e.id, u.id);  // DL
                        addLink(u.id, e.id);  // UL
                    }
    }

    // ---- resolve the table: committed file, cache, or generator -------------
    std::string body = req.dump();
    std::string hash = computeHashHex(body);
    req["requestHash"] = hash;

    std::string tablePath;
    if (!channelTableFile_.empty()) {
        // committed, fingerprint-stable artifact - never spawn
        tablePath = channelTableFile_;
        EV_INFO << "SionnaManager: loading committed channel table '" << tablePath << "'\n";
    }
    else {
        fs::create_directories(cacheDir_);
        std::string cachePath = (fs::path(cacheDir_) / (hash + ".json")).string();
        if (!forceRegenerate_ && fs::exists(cachePath)) {
            tablePath = cachePath;
            EV_INFO << "SionnaManager: cache hit '" << cachePath << "'\n";
        }
        else {
            std::string reqPath = (fs::path(cacheDir_) / (hash + ".request.json")).string();
            std::ofstream(reqPath) << req.dump(2) << "\n";
            runGenerator(reqPath, cachePath);
            tablePath = cachePath;
        }
    }

    loadTableFromFile(tablePath);
}

std::string SionnaManager::computeHashHex(const std::string& s)
{
    // FNV-1a 64-bit: portable and stable across runs/machines
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream os;
    os << std::hex << h;
    return os.str();
}

void SionnaManager::runGenerator(const std::string& requestPath, const std::string& outPath)
{
    if (sionnaScript_.empty())
        throw cRuntimeError("SionnaManager: no cached/committed channel table and no "
                "'sionnaScript' set to generate one (request at '%s')", requestPath.c_str());

    std::string cmd = pythonExecutable_ + " " + sionnaScript_ + " "
            + requestPath + " " + outPath;
    EV_INFO << "SionnaManager: running generator: " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    if (rc != 0)
        throw cRuntimeError("SionnaManager: generator failed (exit %d): %s", rc, cmd.c_str());
}

void SionnaManager::loadTableFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
        throw cRuntimeError("SionnaManager: cannot open channel table '%s'", path.c_str());

    json table;
    in >> table;

    // trust the artifact's own granularity/interference declaration for lookups
    if (table.contains("granularity"))
        granularity_ = (table["granularity"] == "wideband")
            ? Granularity::WIDEBAND : Granularity::PER_RB;
    if (table.contains("interferenceMode"))
        interferenceMode_ = (table["interferenceMode"] == "allPairs")
            ? InterferenceMode::ALL_PAIRS : InterferenceMode::NOISE_LIMITED;

    int nLinks = 0;
    for (auto& carrier : table.at("carriers")) {
        long long key = llround(carrier.at("carrierFrequencyHz").get<double>());
        LinkTable& lt = table_[key];
        for (auto& link : carrier.at("links")) {
            MacNodeId tx = static_cast<MacNodeId>(link.at("tx").get<int>());
            MacNodeId rx = static_cast<MacNodeId>(link.at("rx").get<int>());
            lt[{ tx, rx }] = link.at("pathGainDb").get<std::vector<double>>();
            ++nLinks;
        }
    }
    EV_INFO << "SionnaManager: loaded " << nLinks << " link(s) across "
            << table_.size() << " carrier(s) from '" << path << "'\n";
}

bool SionnaManager::hasPair(MacNodeId tx, MacNodeId rx, GHz carrier)
{
    ensureTable();
    auto cit = table_.find(carrierKey(carrier));
    if (cit == table_.end())
        return false;
    return cit->second.count({ tx, rx }) > 0;
}

double SionnaManager::getPathGainDb(MacNodeId tx, MacNodeId rx, GHz carrier, unsigned int band)
{
    ensureTable();
    auto cit = table_.find(carrierKey(carrier));
    if (cit == table_.end())
        throw cRuntimeError("SionnaManager: no channel table for carrier %g Hz",
                Hz(carrier).get());

    auto lit = cit->second.find({ tx, rx });
    if (lit == cit->second.end())
        throw cRuntimeError("SionnaManager: no path gain for link (tx=%hu, rx=%hu) on carrier %g Hz",
                num(tx), num(rx), Hz(carrier).get());

    const std::vector<double>& gains = lit->second;
    if (gains.empty())
        throw cRuntimeError("SionnaManager: empty path-gain vector for link (tx=%hu, rx=%hu)",
                num(tx), num(rx));

    // wideband tables carry a single value; perRb tables are indexed by band
    unsigned int idx = (gains.size() == 1) ? 0 : band;
    if (idx >= gains.size())
        throw cRuntimeError("SionnaManager: band %u out of range (have %zu) for link (tx=%hu, rx=%hu)",
                band, gains.size(), num(tx), num(rx));
    return gains[idx];
}

} //namespace
