//
// Protocol Test Framework for INET.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Putting the framework's NED types in their own package with an explicit @namespace keeps
// them insulated from a consumer's package namespace -- e.g. an opp_test `.test` case sets
// @namespace on the root package, which would otherwise rewrite the @class of any
// default-package NED type loaded alongside it.
//
// This is Simu5G's vendored copy of the framework (see README-VENDORED.md); the NED
// package is simu5g.protocoltest so it can never collide with a future INET-shipped
// copy, while the C++ namespace stays inet::protocoltest to match upstream.
//
package simu5g.protocoltest;

@namespace(inet::protocoltest);
