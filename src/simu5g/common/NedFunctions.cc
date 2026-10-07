//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <inet/common/INETDefs.h>
#include "simu5g/common/LteDefs.h"

namespace simu5g {

using namespace omnetpp;

using namespace inet;

// This is a copy of the similar function in development version of INET -- remove once INET is released.

static cNEDValue nedf_seq(cComponent *context, cNEDValue argv[], int argc)
{
    static int handle = cSimulationOrSharedDataManager::registerSharedVariableName("simu5g::NedFunctions::seq");
    auto& seqs = getSimulationOrSharedDataManager()->getSharedVariable<std::map<std::string, int>>(handle);
    auto key = argv[0].stringValue();
    auto it = seqs.find(key);
    if (it == seqs.end())
        seqs[key] = 0;
    else
        seqs[key]++;
    return cNEDValue(seqs[key]);
}

Define_NED_Function2(nedf_seq,
        "int simu5g_seq(string id)",
        "misc",
        "Returns the next integer (starting from 0) from the sequence identified by the first argument as name."
        );

static cNEDValue nedf_renamedParam(cComponent *context, cNEDValue argv[], int argc)
{
    if (argv[0].intValue() != argv[1].intValue())
        throw cRuntimeError(context, "Parameter '%s' was renamed to '%s', set that one instead", argv[2].stringValue(), argv[3].stringValue());
    return argv[4];
}

Define_NED_Function2(nedf_renamedParam,
        "int simu5g_renamedParam(int oldValue, int unsetValue, string oldName, string newName, int value)",
        "misc",
        "Guards a renamed integer parameter: the old parameter stays declared with unsetValue as its default, and the new "
        "parameter's default calls this function, which throws an error if the old parameter was set (oldValue differs "
        "from unsetValue), and returns value otherwise."
        );

} //namespace
