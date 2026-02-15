//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.mac.amc;

//
// NR Adaptive Modulation and Coding (AMC) module.
//
// Extends ~LteAmc to provide 5G NR-specific AMC functionality.
// TBS determination is based on 3GPP TS 38.214 v15.6.0 (June 2019).
//
simple NrAmc extends LteAmc
{
    parameters:
        @class("NrAmc");
}
