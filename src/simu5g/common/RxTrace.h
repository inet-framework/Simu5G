//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef COMMON_RXTRACE_H_
#define COMMON_RXTRACE_H_

#include <set>
#include <string>
#include <vector>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class Binder;
class UserControlInfo;

/**
 * The reception trace: one text line per reception decision and per channel
 * evaluation that feeds a decision (CQI, RSRP, beacon). It is meant for
 * comparing two versions of the radio model transmission by transmission, so
 * it records the values the decisions consume -- the per-RB SINR the error
 * model sees, the error rate, the random draw and the outcome -- and the set
 * of interferers, in readable form, doubles in their shortest round-trip form
 * (bit-exact).
 *
 * Off by default; enabled by the per-run option simu5g-rxtrace-file. When
 * off, nothing is computed. When on, it only writes the file: it draws no
 * random number and schedules no event, so the fingerprints do not change.
 *
 * Line formats (fields separated by single spaces, a list without spaces):
 *
 *   <t> RX <tx>><rx> <MHz>MHz <RAT> <dir> cw<k> start=<t0> cqi=<q> h=<acid>/<txNumber>
 *        sinr=[<bands>:<dB>,...] itf={<name>,...} per=<p> draw=<u> rng=<n> <outcome>
 *   <t> WI <kind> <a>><b> <MHz>MHz <RAT> <dir> values=[<bands>:<dB>,...] itf={<name>,...}
 *
 * - <t>: the simulation time of the decision or evaluation; start: when the
 *   frame started to arrive.
 * - <tx>, <rx>, <a>, <b>, interferers: node names (the network's submodule),
 *   a background UE as <node>/<generator>/bgUE[k]; a module path is not used,
 *   because it differs between versions of the module tree.
 * - sinr: the bands the transmission occupies, values: every band; runs of
 *   adjacent bands with an equal value (and allocation) are merged: 3-7:12.5;
 *   an allocation other than one RB per band is written as 3x2:12.5.
 * - per: the error rate compared with the draw; draw: the uniform number;
 *   rng: the number of numbers the drawing RNG had drawn before it; the three
 *   are "-" when the decision draws nothing.
 * - outcome: OK, ERR (the draw), or ERR(<reason>) for a decision without a draw.
 * - WI kinds: CQI (the SINR a CQI is computed from), BEACON (the SINR of a
 *   handover beacon), RSRP (initial cell selection), D2D-RSRP (capture
 *   effect), BG-CQI (a background UE's CQI; no interferers recorded).
 */
namespace rxtrace {

/** Whether the trace is being written in this run. */
bool isEnabled();

/** Whether an evaluation is collecting interferers right now. */
bool isCollecting();

/** The name of the node containing the module (a background UE's, see above). */
std::string nameOf(const cModule *module);

/** The name of the node with the given MAC node id. */
std::string nameOf(Binder *binder, MacNodeId id);

/** "NR" or "LTE": the radio technology of the given UE id. */
const char *ratOf(MacNodeId ueId);

/** Adds an interferer to the evaluation being collected; call only when isCollecting(). */
void addInterferer(const std::string& name);
inline void addInterferer(const cModule *module) { addInterferer(module != nullptr ? nameOf(module) : std::string("?")); }

/**
 * An evaluation that collects interferers while it exists. Evaluations nest;
 * an interferer goes to the innermost one.
 */
class Evaluation
{
  protected:
    bool active = false;
    bool collects = true;
    Evaluation *outer = nullptr;
    std::set<std::string> interferers;

  public:
    explicit Evaluation(bool collectsInterferers = true);
    virtual ~Evaluation();
    Evaluation(const Evaluation&) = delete;
    Evaluation& operator=(const Evaluation&) = delete;

    bool isActive() const { return active; }
    void add(const std::string& name) { interferers.insert(name); }
    Evaluation *getOuter() const { return outer; }
    bool isCollecting() const { return collects; }

  protected:
    std::string interferersText() const;
};

/**
 * A reception decision. Construct it where the decision starts; the line is
 * written by finish(), which returns the outcome, so every exit reads
 * `return rec.finish(...)`.
 */
class Reception : public Evaluation
{
  protected:
    std::string key;
    const UserControlInfo *lteInfo;
    std::string sinrText = "[]";
    std::string perText = "-";
    std::string drawText = "-";
    std::string rngText = "-";
    const char *noDrawReason = nullptr;

  public:
    Reception(const cModule *receiver, Binder *binder, const UserControlInfo *lteInfo, simtime_t startTime);
    ~Reception() override;

    /** The per-band SINR of the reception; only the bands the transmission occupies are written. */
    void setSinr(const std::vector<double>& sinrPerBand);
    void setPer(double per);
    void setDraw(double draw, uint64_t numbersDrawnBefore);
    /** Why the decision draws nothing, for code that decides on its behalf (an error model). */
    void setNoDrawReason(const char *reason) { noDrawReason = reason; }
    /** Writes the line; reasonWithoutDraw (or the one set before) marks a decision without a draw. */
    bool finish(bool success, const char *reasonWithoutDraw = nullptr);
};

/** The reception being decided, or nullptr; for code that draws on its behalf. */
Reception *getCurrentReception();

/**
 * A channel evaluation that is not a reception (CQI, RSRP, beacon). Construct
 * it before the evaluation; when isActive(), name its link with setLink();
 * write the line with finish().
 */
class WhatIf : public Evaluation
{
  protected:
    const char *kind;
    GHz carrierFrequency;
    Direction dir;
    std::string link;

  public:
    WhatIf(const char *kind, GHz carrierFrequency, Direction dir, bool collectsInterferers = true);
    void setLink(const std::string& a, const std::string& b, const char *rat);
    void finish(const std::vector<double>& valuePerBand);
};

} // namespace rxtrace

} // namespace simu5g

#endif
