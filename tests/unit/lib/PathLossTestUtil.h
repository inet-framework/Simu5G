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

#ifndef TESTS_UNIT_LIB_PATHLOSSTESTUTIL_H_
#define TESTS_UNIT_LIB_PATHLOSSTESTUTIL_H_

#include <charconv>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>

#include "simu5g/stack/phy/channelmodel/PathLossModel.h"

namespace simu5g {
namespace unittest {

/**
 * Deployment-scenario parameters of a path-loss model, with the defaults of
 * the Radio/RadioMedium NED parameters they mirror. A test
 * fills in the fields it cares about and hands it to a CaseRecorder, which
 * configures the model with it and reports it as the input of the case,
 * which spares it the five-argument PathLossModel::initialize() call.
 *
 * The carrier frequency and the two antenna heights are not part of
 * initialize() any more (radio endpoint recast E4): they travel per call, in
 * a LinkContext (PathLossModel.h) -- linkContext() extracts the bundle a
 * computePathLoss()/computeLosProbability()/getShadowingStdDev() call needs,
 * the carrier frequency given in GHz only, its Hz value and logarithm derived
 * from it, matching what the owning medium derives from a registered radio's
 * own carrier frequency.
 *
 * insideBuilding/insideDistance are not part of initialize() either --
 * PathLossModel::computePathLoss() takes them per call as an O2iState
 * (PathLossModel.h) -- but they stay here because a test configures them
 * together with the rest of the scenario; o2iState() extracts that bundle.
 */
struct ScenarioParams
{
    DeploymentScenario scenario = UNKNOWN_SCENARIO;
    double hNodeB = 25;                 // base station antenna height [m]
    double hUe = 1.5;                   // UE antenna height [m]
    double hBuilding = 20;              // average building height [m]
    double wStreet = 20;                // average street width [m]
    bool insideBuilding = false;        // whether the UE is indoor
    double insideDistance = 0;          // indoor part of the link length [m]
    double carrierFrequencyGHz = 2.0;   // carrier frequency [GHz]
    bool tolerateMaxDistViolation = false;

    void apply(PathLossModel& model, omnetpp::cComponent *owner) const
    {
        model.initialize(owner, scenario, hBuilding, wStreet, tolerateMaxDistViolation);
    }

    O2iState o2iState() const
    {
        return O2iState{insideBuilding, insideDistance};
    }

    LinkContext linkContext() const
    {
        return LinkContext{carrierFrequencyGHz * 1e9, carrierFrequencyGHz, std::log10(carrierFrequencyGHz), hNodeB, hUe};
    }
};

/**
 * Reports each checked data point as a machine-readable record on stdout, for
 * pathloss_reference.py to grade against its transcription of the
 * specification. A record names the reference formula, the inputs it is to be
 * evaluated at, and what the model returned:
 *
 *   #CASE ref=t901_umi_los scenario=URBAN_MICROCELL hBS=10 hUT=1.5 h=20 W=20
 *         fc=3.5 d3D=21.5 d2D=20 los=1 actual=71.26256854533 tol=1e-06
 *
 * No expected value appears here or in the .test files: the data point is
 * defined once, by the call that produces it, and the reference script derives
 * what it ought to be from the same inputs. The script grades a record by
 * passing it those of the reported parameters that the named formula declares,
 * so a parameter the specification fixes rather than reads -- the average
 * building height of the TR 38.901 rural scenario, say -- is simply not a
 * parameter of the transcribed formula, and what a test sets it to is ignored.
 *
 * Values are written in the shortest form that reads back as the same double,
 * so a record round-trips exactly and the comparison is made on the number the
 * model returned, not on a decimal approximation of it.
 *
 * The measurement wrappers configure the model themselves, which is what makes
 * the reported inputs the inputs the model was given. Where the two must
 * differ -- a case that checks the model clamps its argument, or one whose
 * result is a difference of two calls -- the test calls record() and states
 * the arguments the reference formula is to be evaluated at explicitly.
 */
class CaseRecorder
{
  private:
    omnetpp::cComponent *owner_;
    double tolerance_;
    int modelChecks_ = 0;
    int modelFailures_ = 0;

    // shortest representation that reads back as the same double
    static std::string num(double d)
    {
        char buf[32];
        auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), d);
        return std::string(buf, end);
    }

    void emit(const char *ref, const ScenarioParams *p,
            std::initializer_list<std::pair<const char *, double>> args,
            double actual, double tolerance)
    {
        std::cout << "#CASE ref=" << ref;
        if (p != nullptr)
            std::cout << " scenario=" << DeploymentScenarioToA(p->scenario)
                      << " hBS=" << num(p->hNodeB) << " hUT=" << num(p->hUe)
                      << " h=" << num(p->hBuilding) << " W=" << num(p->wStreet)
                      << " fc=" << num(p->carrierFrequencyGHz);
        for (const auto& arg : args)
            std::cout << " " << arg.first << "=" << num(arg.second);
        std::cout << " actual=" << num(actual) << " tol=" << num(tolerance) << std::endl;
    }

  public:
    /**
     * The tolerance applies to every case the recorder reports, unless one
     * passes its own.
     */
    CaseRecorder(omnetpp::cComponent *owner, double tolerance) :
        owner_(owner), tolerance_(tolerance)
    {
    }

    /**
     * Report a case whose result the test computed itself, to be graded
     * against ref evaluated at the reported scenario parameters and args.
     */
    void record(const char *ref, const ScenarioParams& p,
            std::initializer_list<std::pair<const char *, double>> args,
            double actual)
    {
        emit(ref, &p, args, actual, tolerance_);
    }

    void record(const char *ref, const ScenarioParams& p,
            std::initializer_list<std::pair<const char *, double>> args,
            double actual, double tolerance)
    {
        emit(ref, &p, args, actual, tolerance);
    }

    /**
     * Report a case that does not depend on the deployment scenario.
     */
    void record(const char *ref,
            std::initializer_list<std::pair<const char *, double>> args,
            double actual)
    {
        emit(ref, nullptr, args, actual, tolerance_);
    }

    void pathLoss(const char *ref, PathLossModel& model, const ScenarioParams& p,
            double d3D, double d2D, bool los)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d3D", d3D }, { "d2D", d2D }, { "los", static_cast<double>(los) } },
                model.computePathLoss(d3D, d2D, los, p.o2iState(), p.linkContext()));
    }

    /**
     * The TR 36.814 formulas are functions of a single distance, which the
     * model reads from the 3D argument; these overloads pass the one distance
     * as both and report it under the name the formulas use.
     */
    void pathLoss(const char *ref, PathLossModel& model, const ScenarioParams& p,
            double d, bool los)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d", d }, { "los", static_cast<double>(los) } },
                model.computePathLoss(d, d, los, p.o2iState(), p.linkContext()));
    }

    void losProbability(const char *ref, PathLossModel& model, const ScenarioParams& p,
            double d3D, double d2D)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d3D", d3D }, { "d2D", d2D } },
                model.computeLosProbability(d3D, d2D, p.linkContext()));
    }

    void losProbability(const char *ref, PathLossModel& model, const ScenarioParams& p, double d)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d", d } }, model.computeLosProbability(d, d, p.linkContext()));
    }

    void shadowingStdDev(const char *ref, PathLossModel& model, const ScenarioParams& p,
            double d3D, double d2D, bool los)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d3D", d3D }, { "d2D", d2D }, { "los", static_cast<double>(los) } },
                model.getShadowingStdDev(d3D, d2D, los, p.linkContext()));
    }

    void shadowingStdDev(const char *ref, PathLossModel& model, const ScenarioParams& p,
            double d, bool los)
    {
        p.apply(model, owner_);
        record(ref, p, { { "d", d }, { "los", static_cast<double>(los) } },
                model.getShadowingStdDev(d, d, los, p.linkContext()));
    }

    /**
     * The antenna pattern is a property of the model alone, so this one takes
     * no deployment scenario and leaves the model unconfigured.
     */
    void angularAttenuation(const char *ref, PathLossModel& model, double phi, double theta)
    {
        record(ref, { { "phi", phi }, { "theta", theta } },
                model.computeAngularAttenuation(phi, theta));
    }

    /**
     * Verify that evaluating the model outside its validity range is
     * rejected. Takes the call as a lambda.
     */
    template<typename Fn>
    void expectRejected(const std::string& what, Fn fn)
    {
        modelChecks_++;
        try {
            fn();
            modelFailures_++;
            std::cout << "model FAILED: " << what << ": expected an error, but none was thrown" << std::endl;
        }
        catch (omnetpp::cRuntimeError& e) {
            std::cout << "model ok: " << what << " threw as expected" << std::endl;
        }
    }

    /**
     * Verify a value that comes from a convention of the model rather than
     * from a specification, and so has no reference formula to be graded
     * against.
     */
    void expectValue(const std::string& what, double actual, double expected)
    {
        modelChecks_++;
        if (std::fabs(actual - expected) > tolerance_) {
            modelFailures_++;
            std::cout << "model FAILED: " << what << ": actual=" << num(actual)
                      << " expected=" << num(expected) << std::endl;
        }
        else
            std::cout << "model ok: " << what << " = " << num(actual) << std::endl;
    }

    /**
     * Reports the checks decided here; the graded cases are counted by the
     * reference script, which is the only side that knows what they should
     * have produced.
     */
    void summary() const
    {
        std::cout << "modelchecks=" << modelChecks_ << " modelfailures=" << modelFailures_ << std::endl;
    }
};

/**
 * Checks a computed value against a literal expected value baked into the
 * test, rather than against pathloss_reference.py's transcription of a
 * formula -- for a test that pins down a documented deviation from the
 * specification: the expected value is what the specification would
 * produce, the model is known to produce something else, and the file
 * declares %expected-failure so the mismatch is the point of the test.
 */
class NumericChecker
{
  private:
    double tolerance_;
    int checks_ = 0;
    int failures_ = 0;

  public:
    explicit NumericChecker(double tolerance) : tolerance_(tolerance) {}

    void check(const std::string& what, double actual, double expected)
    {
        check(what, actual, expected, tolerance_);
    }

    void check(const std::string& what, double actual, double expected, double tolerance)
    {
        checks_++;
        if (std::fabs(actual - expected) > tolerance) {
            failures_++;
            std::cout << "FAILED: " << what << ": actual=" << actual
                      << " expected=" << expected << std::endl;
        }
        else
            std::cout << "ok: " << what << " = " << actual << std::endl;
    }

    void summary() const
    {
        std::cout << "checks=" << checks_ << " failures=" << failures_ << std::endl;
    }
};

} //namespace unittest
} //namespace simu5g

#endif
