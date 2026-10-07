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


package simu5g.stack;

import simu5g.stack.ethernet.EthernetPortal;
import simu5g.stack.mac.ILteMac;
import simu5g.stack.packetFlowObserver.IPacketFlowObserver;
import simu5g.stack.phy.IPhy;
import simu5g.stack.phy.channelmodel.IChannelModel;
import simu5g.stack.phy.feedback.LteDlFeedbackGenerator;
import simu5g.stack.rlc.IRlcMux;

//
// The LteNicUe module is a network interface that provides LTE connectivity
// within a UE (User Equipment). It implements the data and signaling planes of the LTE stack,
// which includes PDCP, RLC, MAC, and PHY layers. The parameters and submodules
// allow customization of the UE's behavior.
//
// The PDCP and RLC entities are missing from the submodules on purpose: the RRC
// layer's ~BearerManagement creates them per bearer at run time.
//
module LteNicUe extends LteNicBase like IUeCellularNic
{
    parameters:
        lteMac.typename = default("LteMacUe");
        ltePhy.typename = default("PhyUe");
        handoverPacketHolder.typename = default("HandoverPacketHolderUe");
        ltePacketFlowObserver.typename = default("PacketFlowObserverUe");

        string sessionType;  // The type of the UE's session (see ~LteUe), set by the node

        rrc.hasLteConnectionControl = true;
        rrc.lteConnectionControl.isNr = false;
        rrc.lteConnectionControl.macNodeId = parent.macNodeId;
        rrc.lteConnectionControl.sessionType = this.sessionType;
        // The UE's control-plane entry point is its LTE leg's connection control
        ip2nic.connectionControlModule = "^.rrc.lteConnectionControl";
        rrc.registration.connectionControlModule = "^.lteConnectionControl";
        rrc.bearerManagement.connectionControlModule = "^.lteConnectionControl";
        rrc.registration.sessionType = this.sessionType;
        handoverPacketHolder.sessionType = this.sessionType;
        ip2nic.sessionType = this.sessionType;
        lteDlFbGen.macNodeId = parent.macNodeId;
        // The LTE leg's modules take their MacNodeId from the node
        ltePhy.macNodeId = parent.macNodeId;
        lteMac.macNodeId = parent.macNodeId;
        // The LTE leg's RLC entities attach to the LTE leg's mux and MAC
        rrc.bearerManagement.rlcMuxModule = "^.^.lteRlcMux";
        rrc.bearerManagement.macModule = "^.^.lteMac";

    submodules:
        // RLC submodules (flattened from former LteRlc compound)
        lteRlcMux: <default("RlcMux")> like IRlcMux {
            @display("p=310,580;is=s");
        }
        // MAC Layer
        lteMac: <> like ILteMac {
            @display("p=310,660");
        }
        // PHY submodule
        ltePhy: <> like IPhy {
            @display("p=310,750");
        }

        channelModel[numCarriers]: <channelModelType> like IChannelModel {
            @display("p=80,60,col;g=left");
        }

        //# Modules used to take trace of PDCP pkt flow
        ltePacketFlowObserver: <default("")> like IPacketFlowObserver if hasRniSupport {
            @display("p=80,60,col;g=left");
        }

        // Feedback generator submodule
        lteDlFbGen: LteDlFeedbackGenerator {
            @display("p=80,60,col;g=left");
        }
        // the NIC's top in Ethernet mode, an Ethernet port of the UE's link layer
        ethernetPortal: EthernetPortal if sessionType == "Ethernet" {
            @display("p=310,50;is=s");
        }

    connections allowunconnected:
        //# RLC <-> MAC
        lteRlcMux.macOut --> lteMac.upperLayerIn;
        lteRlcMux.macIn <-- lteMac.upperLayerOut;

        //# Connections from LTE Stack to radio interface
        lteMac.phyOut --> ltePhy.upperGateIn;
        lteMac.phyIn <-- ltePhy.upperGateOut;

        //# external: lower connection
        radioIn --> ltePhy.radioIn;

        upperLayerIn --> { @reconnect; delay = parent.processingDelayIn; } --> ethernetPortal.upperLayerIn if sessionType == "Ethernet";
        ethernetPortal.lowerLayerOut --> handoverPacketHolder.upperLayerIn if sessionType == "Ethernet";
        ip2nic.upperLayerOut --> { @reconnect; } --> ethernetPortal.lowerLayerIn if sessionType == "Ethernet";
        ethernetPortal.upperLayerOut --> { delay = parent.processingDelayOut; } --> upperLayerOut if sessionType == "Ethernet";
}

