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
// Module interface for the Adaptive Modulation and Coding (AMC) module.
// AMC is responsible for selecting the appropriate modulation and coding scheme
// based on channel quality feedback from UEs.
//
moduleinterface ILteAmc
{
    parameters:
        @display("i=block/mac");
}
