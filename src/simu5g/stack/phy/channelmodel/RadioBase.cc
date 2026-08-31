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

#include "simu5g/stack/phy/channelmodel/RadioBase.h"

#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/phy/PhyBase.h"

namespace simu5g {


void RadioBase::initialize(int stage)
{
    if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        binder_.reference(this, "binderModule", true);

        // radio endpoint recast E8 (§3(c)): a space-separated list, resolved
        // in declaration order -- the order the registration sweeps below,
        // and PhyBase::initializeRadio()'s binder_->registerCarrierUe
        // sweep, register in.
        cStringTokenizer tokenizer(par("componentCarrierModules").stringValue());
        while (tokenizer.hasMoreTokens())
            componentCarriers_.push_back(check_and_cast<ComponentCarrier *>(getModuleByPath(tokenizer.nextToken())));
        if (componentCarriers_.empty())
            throw cRuntimeError("%s: componentCarrierModules must name at least one ComponentCarrier module", getFullPath().c_str());

        // risk 9: numCarriers/numNrCarriers still sizes bgTrafficGenerator[]
        // (LteNicEnb.ned) and no longer sizes this radio's own carrier list --
        // catch the two drifting apart loudly instead of leaving it to
        // silently misbehave.
        const char *countParName = (std::string(getName()) == "nrRadio") ? "numNrCarriers" : "numCarriers";
        cModule *nic = getParentModule();
        if (nic->hasPar(countParName)) {
            int declaredCount = nic->par(countParName);
            if ((int)componentCarriers_.size() != declaredCount)
                throw cRuntimeError("%s: %s=%d does not match the %zu carrier(s) named in componentCarrierModules",
                        getFullPath().c_str(), countParName, declaredCount, componentCarriers_.size());
        }

        // PRIMARY (first-declared) carrier's frequency, cached the same way
        // the single-carrier module used to -- TODO fix this for UEs' radio
        // (probably it's not used)
        numBands_ = componentCarriers_[0]->getNumBands();
        carrierFrequency_ = componentCarriers_[0]->getCarrierFrequency();
        carrierFrequencyGHz_ = GHz(carrierFrequency_).get();
        carrierFrequencyHz_ = Hz(carrierFrequency_).get();
        log10CarrierFrequencyGHz_ = log10(carrierFrequencyGHz_);
    }
    if (stage == INITSTAGE_SIMU5G_REGISTRATIONS) {
        // register every carrier this radio serves to the cellInfo module,
        // in componentCarrierModules' declaration order (E8, §3(c)):
        // CellInfo::carriersVector_ is a push_back, so registration order is
        // observable
        cellInfo_.reference(this, "cellInfoModule", false);
        if (cellInfo_ != nullptr) { // cellInfo is NULL on UEs
            for (auto *cc : componentCarriers_)
                cellInfo_->registerCarrier(cc->getCarrierFrequency(), cc->getNumBands(), cc->getNumerologyIndex());
        }
    }
}

} //namespace

