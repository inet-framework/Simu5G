//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.NrNicUe;
import simu5g.stack.mac.ILteMac;
import simu5g.stack.phy.ILtePhy;
import simu5g.stack.phy.channelmodel.ILteChannelModel;
import simu5g.stack.phy.feedback.LteDlFeedbackGenerator;
import simu5g.stack.rlc.IRlcMux;

//
// UE NIC with an extra NR stack leg (leg 2): one more RLC-mux/MAC/PHY/
// feedback-generator column, wired exactly like the stock NR leg but fed
// from the node's nrMacNodeId2/nrServingNodeId2 parameters. The Ml module
// versions (binder-aware leg bookkeeping) are installed via the parametric
// slots.
//
module MlNrNicUe extends NrNicUe
{
    parameters:
        rrc.typename = default("MlRrc");
        ip2nic.typename = default("MlIp2Nic");
        legSelection.typename = default("MlLegSelection");
        rrc.bearerManagement.typename = default("MlBearerManagement");
        rrc.registration.typename = default("MlRegistration");

        // the extra leg's handover controller (pinned: no handover, no dynamic association)
        rrc.hasNrHandoverController2 = true;
        rrc.nrHandoverController2.isNr = true;
        rrc.nrHandoverController2.macNodeId = parent.nrMacNodeId2;
        rrc.nrHandoverController2.macModule = "^.^.nrMac2";
        rrc.nrHandoverController2.feedbackGeneratorModule = "^.^.nrDlFbGen2";

        // the extra leg's modules take their MacNodeId from the node's leg-2 id
        nrMac2.isNr = true;
        nrMac2.macNodeId = parent.nrMacNodeId2;
        nrPhy2.isNr = true;
        nrPhy2.macNodeId = parent.nrMacNodeId2;
        nrPhy2.channelModelModule = "^.nrChannelModel2[0]";
        nrPhy2.feedbackGeneratorModule = "^.nrDlFbGen2";
        nrPhy2.handoverControllerModule = "^.rrc.nrHandoverController2";
        nrDlFbGen2.isNr = true;
        nrDlFbGen2.macNodeId = parent.nrMacNodeId2;

    gates:
        input nrRadioIn2 @loose;    // the extra leg's radio input

    submodules:
        // the extra leg's stack column, mirroring the stock NR leg
        nrRlcMux2: <default("RlcMux")> like IRlcMux {
            @display("p=700,430;is=s");
        }
        nrMac2: <default("NrMacUe")> like ILteMac {
            @display("p=700,510");
        }
        nrPhy2: <default("NrPhyUe")> like ILtePhy {
            @display("p=700,600");
        }
        nrChannelModel2[numNrCarriers]: <nrChannelModelType> like ILteChannelModel {
            @display("p=80,60,col;g=left");
        }
        nrDlFbGen2: LteDlFeedbackGenerator {
            @display("p=80,60,col;g=left");
            phyModule = "^.nrPhy2";
        }

    connections:
        nrRlcMux2.macOut --> nrMac2.upperLayerIn;
        nrRlcMux2.macIn <-- nrMac2.upperLayerOut;
        nrMac2.phyOut --> nrPhy2.upperGateIn;
        nrMac2.phyIn <-- nrPhy2.upperGateOut;
        nrRadioIn2 --> nrPhy2.radioIn;
}
