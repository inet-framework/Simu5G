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
import simu5g.common.LteCommon;

cplusplus {{
#include "simu5g/common/LteControlInfo.h"
}}

namespace simu5g;

//
// This is LteAirFrame.
//
packet LteAirFrame extends AirFrame {
    RemoteUnitPhyData remoteUnitPhyDataVector[] @nopack;
    UserControlInfo additionalInfo @nopack;
}

cplusplus(LteAirFrame) {{
  public:
    void addRemoteUnitPhyDataVector(const RemoteUnitPhyData& data) { appendRemoteUnitPhyDataVector(data); }
}}
