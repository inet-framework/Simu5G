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

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class AirFrame;
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
 * The radio medium owns the trace (CellularRadioMedium::getRxTrace()) and
 * opens it when its recordReceptionTrace parameter is true. When it is not
 * open, nothing is computed. When it is, it only writes the file: it draws no
 * random number and schedules no event, so the fingerprints do not change.
 *
 * The code that evaluates a link or decides a reception marks it with a
 * scoped Reception or WhatIf object constructed on the trace; the line is
 * written by the object's finish().
 *
 * Line formats (fields separated by single spaces, a list without spaces):
 *
 *   <t> RX <tx>><rx> <MHz>MHz <RAT> <dir> cw<k> start=<t0> cqi=<q> h=<acid>/<txNumber>
 *        sinr=[<bands>:<dB>,...] itf={<name>,...} per=<p> draw=<u> rng=<n> <outcome>
 *   <t> WI <kind> <a>><b> <MHz>MHz <RAT> <dir> values=[<bands>:<dB>,...] itf={<name>,...}
 *
 * - <t>: the simulation time of the decision or evaluation; start: when the
 *   frame started to arrive, "-" for a reception decided without a frame.
 * - <tx>, <rx>, <a>, <b>, interferers: node names (the network's submodule),
 *   a background UE as <node>/<generator>/bgUE[k]; a module path is not used,
 *   because it differs between versions of the module tree.
 * - sinr: the bands the transmission occupies, values: every band; runs of
 *   adjacent bands with an equal value (and allocation) are merged: 3-7:12.5;
 *   an allocation other than one RB per band is written as 3x2:12.5.
 * - per: the error rate compared with the draw; draw: the uniform number;
 *   rng: the number of numbers the drawing RNG had drawn before it; the three
 *   are "-" when the decision draws nothing.
 * - outcome: OK or ERR (decided by the draw), or ERR(nodraw): lost for
 *   certain, without a draw, because the error model gave no error rate. With
 *   the BLER-curve error model that is CQI 0 (cqi=0 on the line) or a band
 *   below the curves.
 * - WI kinds: CQI (the SINR a CQI is computed from), BEACON (the SINR of a
 *   handover beacon), RSRP (initial cell selection), D2D-RSRP (capture
 *   effect), BG-CQI (a background UE's CQI; no interferers recorded).
 */
class RxTrace
{
  public:
    /**
     * An evaluation that collects interferers while it exists. Evaluations
     * nest; an interferer goes to the innermost one.
     */
    class Evaluation
    {
      protected:
        RxTrace& trace;
        bool active = false;
        bool collects = true;
        Evaluation *outer = nullptr;
        std::set<std::string> interferers;

      public:
        Evaluation(RxTrace& trace, bool collectsInterferers);
        virtual ~Evaluation();
        Evaluation(const Evaluation&) = delete;
        Evaluation& operator=(const Evaluation&) = delete;

        /** Whether the trace is open, so this evaluation writes a line. */
        bool isActive() const { return active; }
        bool isCollecting() const { return collects; }
        void addInterferer(const std::string& name) { interferers.insert(name); }

      protected:
        std::string interferersText() const;
    };

    /**
     * A reception decision. Construct it where the decision starts; the line
     * is written by finish(), which returns the outcome, so every exit reads
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

      public:
        /** frame may be nullptr (a reception decided without one, as in unit tests) */
        Reception(RxTrace& trace, const cModule *receiver, Binder *binder, const UserControlInfo *lteInfo, const AirFrame *frame);

        /** The per-band SINR of the reception; only the bands the transmission occupies are written. */
        void setSinr(const std::vector<double>& sinrPerBand);
        void setPer(double per);
        void setDraw(double draw, uint64_t numbersDrawnBefore);
        /** Writes the line of a decision by the draw. */
        bool finish(bool success);
        /** Writes the line of a reception lost for certain, without a draw; returns false. */
        bool finishWithoutDraw();

      protected:
        void write(const std::string& outcome);
    };

    /**
     * A channel evaluation that is not a reception (CQI, RSRP, beacon).
     * Construct it before the evaluation; when isActive(), name its link with
     * setLink(); write the line with finish().
     */
    class WhatIf : public Evaluation
    {
      protected:
        const char *kind;
        GHz carrierFrequency;
        Direction dir;
        std::string link;

      public:
        WhatIf(RxTrace& trace, const char *kind, GHz carrierFrequency, Direction dir, bool collectsInterferers = true);
        void setLink(const std::string& a, const std::string& b, const char *rat);
        void finish(const std::vector<double>& valuePerBand);
    };

  protected:
    std::ofstream file;
    Evaluation *innermost = nullptr; // the evaluation interferers go to

  public:
    RxTrace() = default;
    RxTrace(const RxTrace&) = delete;
    RxTrace& operator=(const RxTrace&) = delete;

    /** Opens the trace file; throws if it cannot be opened. */
    void open(const std::string& fileName);
    void close();

    /** Whether the trace is being written. */
    bool isEnabled() const { return file.is_open(); }

    /** Adds the module's node as an interferer to the innermost evaluation, if that collects interferers. */
    void addInterferer(const cModule *module);

    /** The name of the node containing the module (a background UE's, see above). */
    static std::string nameOf(const cModule *module);

    /** The name of the node with the given MAC node id. */
    static std::string nameOf(Binder *binder, MacNodeId id);

    /** "NR" or "LTE": the radio technology of the given UE id. */
    static const char *ratOf(MacNodeId ueId);

  protected:
    void write(const std::string& line);
};

} // namespace simu5g

#endif
