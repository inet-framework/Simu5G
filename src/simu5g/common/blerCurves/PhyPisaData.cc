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

#include "simu5g/common/LteDefs.h"
#include "simu5g/common/blerCurves/PhyPisaData.h"

namespace simu5g {

using namespace omnetpp;

PhyPisaData::PhyPisaData()
{
    channel_.resize(10000);
    double x, y;

    for (int i = 0; i < 1000; i++) {
        x = normal(getEnvir()->getRNG(0), 0, 0.5);
        y = normal(getEnvir()->getRNG(0), 0, 0.5);
        channel_[i] = (x * x) + (y * y);
    }
}

PhyPisaData::~PhyPisaData()
{
}

double PhyPisaData::getChannel(unsigned int i)
{
    i = i % channel_.size();
    return channel_[i];
}

} //namespace

