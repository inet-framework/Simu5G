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

package simu5g.world.radio;

//
// The name under which the shipped networks instantiate ~ChannelControl. It
// adds nothing to it: without @class, it is implemented by ChannelControl's
// C++ class. Exactly one instance of this module (or of ~ChannelControl) must
// be present in the network, as a submodule named "channelControl".
//
simple LteChannelControl extends ChannelControl
{
}
