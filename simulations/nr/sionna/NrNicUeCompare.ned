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

package simu5g.simulations.nr.sionna;

import simu5g.stack.NrNicUe;
import simu5g.stack.phy.channelmodel.ILteChannelModel;

//
// NR UE NIC for the Plan A §7 validation: the NR channel model is a
// CompareChannelModel that wraps a 'reference' (built-in) and a 'candidate'
// (Sionna) inner model, declared here as siblings of nrChannelModel. The
// CompareChannelModel.referenceModule / candidateModule paths are set from the ini.
//
module NrNicUeCompare extends NrNicUe
{
    parameters:
        string referenceChannelModelType = default("NrChannelModel_3GPP38_901");
        string candidateChannelModelType = default("SionnaChannelModel");
        nrChannelModelType = default("CompareChannelModel");

    submodules:
        refNrChannelModel[numNrCarriers]: <referenceChannelModelType> like ILteChannelModel {
            @display("p=232,300;is=s");
        }
        candNrChannelModel[numNrCarriers]: <candidateChannelModelType> like ILteChannelModel {
            @display("p=120,300;is=s");
        }
}
