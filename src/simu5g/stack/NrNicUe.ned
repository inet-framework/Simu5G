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

import simu5g.stack.mac.ILteMac;
import simu5g.stack.packetFlowObserver.IPacketFlowObserver;
import simu5g.stack.phy.IPhy;
import simu5g.stack.phy.channelmodel.IChannelModel;
import simu5g.stack.phy.feedback.LteDlFeedbackGenerator;
import simu5g.stack.rlc.IRlcMux;
import simu5g.stack.rrc.IRrc;
import simu5g.stack.sdap.INrSdap;
import simu5g.stack.sdap.common.IReflectiveQosTable;

//
// Defines a User Equipment (UE) network interface card that
// supports both LTE and NR (New Radio) technologies. It extends the
// LteNicUe module by adding additional submodules and parameters to
// support NR-specific functionalities. D2D support is added by the
// ~NrNicUeD2D subtype.
//
module NrNicUe extends LteNicUe
{
    parameters:
        bool hasSdap = default(false);
        bool hasSidelink = default(false);  // enables the NR sidelink (PC5) leg
        bool useReflectiveQos = default(false);
        ip2nic.hasSdap = default(this.hasSdap);
        sdap.nodeRole = default("UE");
        sdap.useReflectiveQos = default(this.useReflectiveQos);
        sdap.reflectiveQosTableModule = default(this.useReflectiveQos ? "^.reflectiveQosTable" : "");
        string nrChannelModelType = default("Tr38901ChannelModel");

        packetFlowObserver.typename = default("PacketFlowObserverUe");

        int numNrCarriers = default(1);

        ip2nic.isNr = default(true);
        rrc.bearerManagement.pdcpEntityModuleType = default("simu5g.stack.pdcp.NrPdcpEntity");

        // sidelink leg: swap in the SL-aware packet classification variants
        handoverPacketHolder.typename = default(hasSidelink ? "simu5g.stack.sidelink.ip2nic.SlHandoverPacketHolderUe" : "HandoverPacketHolderUe");
        technologyDecision.typename = default(hasSidelink ? "simu5g.stack.sidelink.ip2nic.SlTechnologyDecision" : "TechnologyDecision");
        ip2nic.typename = default(hasSidelink ? "simu5g.stack.sidelink.ip2nic.SlIp2Nic" : "Ip2Nic");

        rrc.hasNrHandoverController = true;
        // the sidelink leg is an NR leg: its modules take the node's NR MacNodeId
        slMac.macNodeId = parent.nrMacNodeId;
        slPhy.macNodeId = parent.nrMacNodeId;
        rrc.nrHandoverController.isNr = true;
        nrMac.isNr = true;
        nrPhy.isNr = true;
        nrDlFbGen.isNr = true;
        // the NR leg's modules take their MacNodeId from the node's NR id
        rrc.nrHandoverController.macNodeId = parent.nrMacNodeId;
        nrMac.macNodeId = parent.nrMacNodeId;
        nrPhy.macNodeId = parent.nrMacNodeId;
        nrDlFbGen.macNodeId = parent.nrMacNodeId;
        rrc.nrHandoverController.otherHandoverControllerModule = "^.handoverController";
        nrPhy.channelModelModule = "^.nrChannelModel[0]";
        nrPhy.feedbackGeneratorModule = "^.nrDlFbGen";
        nrPhy.handoverControllerModule = "^.rrc.nrHandoverController";
        rrc.nrHandoverController.macModule = "^.^.nrMac";
        rrc.nrHandoverController.feedbackGeneratorModule = "^.^.nrDlFbGen";
        rrc.handoverController.otherHandoverControllerModule = "^.nrHandoverController";

    submodules:
        sdap: <default("NrSdap")> like INrSdap if hasSdap {
            @display("p=230,155");
        }
        reflectiveQosTable: <default("ReflectiveQosTable")> like IReflectiveQosTable if useReflectiveQos && hasSdap {
            @display("p=80,60,col;g=left");
        }
        // NR RLC submodules (flattened from former LteRlc compound)
        nrRlcMux: <default("RlcMux")> like IRlcMux {
            @display("p=570,430;is=s");
        }
        // MAC Layer
        nrMac: <default("NrMacUe")> like ILteMac {
            @display("p=570,510");
        }
        // PHY Layer
        nrPhy: <default("PhyUe")> like IPhy {
            @display("p=570,600");
        }
        // NR Channel Model
        nrChannelModel[numNrCarriers]: <nrChannelModelType> like IChannelModel {
            @display("p=80,60,col;g=left");
        }

        // Feedback generator submodule
        nrDlFbGen: LteDlFeedbackGenerator {
            @display("p=80,60,col;g=left");
            phyModule = "^.nrPhy";
        }

        nrPacketFlowObserver: <default("NrPacketFlowObserverUe")> like IPacketFlowObserver if hasRniSupport {
            @display("p=80,60,col;g=left");
            macModule = "^.nrMac";
            rlcModule = "^";
            isNrObserver = true;
        }

        //# sidelink (PC5) leg: a third protocol leg beside the LTE and NR Uu legs
        slRrc: <default("simu5g.stack.sidelink.rrc.SlRrc")> like IRrc if hasSidelink {
            @display("p=80,60,col;g=left");
        }
        slRlcMux: <default("RlcMux")> like IRlcMux if hasSidelink {
            @display("p=700,430;is=s");
        }
        slMac: <default("simu5g.stack.sidelink.mac.NrSlMacUe")> like ILteMac if hasSidelink {
            @display("p=700,510");
        }
        slPhy: <default("simu5g.stack.sidelink.phy.NrSlPhyUe")> like IPhy if hasSidelink {
            @display("p=700,600");
        }
        slChannelModel: <default("simu5g.stack.sidelink.phy.SlIdealChannelModel")> like IChannelModel if hasSidelink {
            @display("p=80,60,col;g=left");
        }

    connections:

        //# NR RLC <-> MAC
        nrRlcMux.macOut --> nrMac.upperLayerIn;
        nrRlcMux.macIn <-- nrMac.upperLayerOut;

        //#
        //# Connections from LTE Stack to radio interface
        //#
        nrMac.phyOut --> nrPhy.upperGateIn;
        nrMac.phyIn <-- nrPhy.upperGateOut;

        //# external: lower connection
        nrRadioIn --> nrPhy.radioIn;

        //# sidelink leg
        slRlcMux.macOut --> slMac.upperLayerIn if hasSidelink;
        slRlcMux.macIn <-- slMac.upperLayerOut if hasSidelink;
        slMac.phyOut --> slPhy.upperGateIn if hasSidelink;
        slMac.phyIn <-- slPhy.upperGateOut if hasSidelink;
        slRadioIn --> slPhy.radioIn if hasSidelink;

        //# SDAP layer (optional, spliced between ip2nic and pdcpMux)
        ip2nic.stackOut --> { @reconnect; } --> sdap.upperLayerIn if hasSdap;
        sdap.pdcpOut --> pdcpMux.upperLayerIn if hasSdap;
        pdcpMux.upperLayerOut --> { @reconnect; } --> sdap.pdcpIn if hasSdap;
        sdap.upperLayerOut --> ip2nic.stackIn if hasSdap;
}
