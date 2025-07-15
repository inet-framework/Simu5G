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

import simu5g.stack.phy.packet.AirFrame;
import simu5g.common.LteControlInfo;

cplusplus {{
#include "simu5g/common/LteControlInfo.h"
}}

cplusplus {{
#include "simu5g/common/LteCommon.h"  // for RemoteUnitPhyDataVector
}}

namespace simu5g;

//
// Represents a transport block (TB) carrying a MAC PDU, as transmitted
// in the PDSCH / PUSCH channel. In the simulation, a MAC PDU
// (LteMacPdu) travels encapsulated within an LteAirFrame.
//
packet LteAirFrame extends AirFrame {
    @customize(true);
    RemoteUnitPhyData remoteUnitPhyDataVector[] @nopack;
    UserControlInfo additionalInfo @nopack;
}

cplusplus(LteAirFrame) {{
  public:
    void addRemoteUnitPhyDataVector(const RemoteUnitPhyData& data) { appendRemoteUnitPhyDataVector(data); }
}}
