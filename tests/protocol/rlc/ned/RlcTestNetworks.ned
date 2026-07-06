//
// Simu5G RLC conformance suite -- test networks.
//
// Thin wrappers that add a `tester: ProtocolTester` submodule to the stock Simu5G
// scenarios, WITHOUT modifying them. The tester subscribes network-wide at
// getSystemModule() and runs the named ProtocolTest program selected by its
// `testName` parameter (set per test in the .test %inifile). Adding a submodule via
// `extends` leaves the scenario's own topology, addressing, and channel model intact.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
package simu5g.tests.protocol.rlc;

import simu5g.protocoltest.ProtocolTester;
import simu5g.simulations.lte.networks.SingleCell;
import simu5g.simulations.nr.networks.SingleCell_Standalone;

network LteRlcTestNet extends SingleCell
{
    submodules:
        tester: ProtocolTester {
            @display("p=50,50;i=block/cogwheel");
        }
}

network NrRlcTestNet extends SingleCell_Standalone
{
    submodules:
        tester: ProtocolTester {
            @display("p=50,50;i=block/cogwheel");
        }
}
