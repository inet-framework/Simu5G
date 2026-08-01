//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.mac.ILteMac;
import simu5g.stack.phy.ILtePhy;
import simu5g.stack.phy.channelmodel.ILteChannelModel;
import simu5g.stack.phy.feedback.LteDlFeedbackGenerator;
import simu5g.stack.rlc.IRlcMux;

//
// UE NIC with two extra NR stack legs (legs 2 and 3): one more stack column
// on top of MlNrNicUe, wired the same way from the node's
// nrMacNodeId3/nrServingNodeId3 parameters.
//
module MlNrNicUe3 extends MlNrNicUe
{
    parameters:
        rrc.bearerManagement.nrMac3Module = "^.^.nrMac3";
        rrc.bearerManagement.nrRlcMux3Module = "^.^.nrRlcMux3";

        rrc.hasNrHandoverController3 = true;
        rrc.nrHandoverController3.isNr = true;
        rrc.nrHandoverController3.macNodeId = parent.nrMacNodeId3;
        rrc.nrHandoverController3.macModule = "^.^.nrMac3";
        rrc.nrHandoverController3.feedbackGeneratorModule = "^.^.nrDlFbGen3";

        nrMac3.isNr = true;
        nrMac3.macNodeId = parent.nrMacNodeId3;
        nrPhy3.isNr = true;
        nrPhy3.macNodeId = parent.nrMacNodeId3;
        nrPhy3.channelModelModule = "^.nrChannelModel3[0]";
        nrPhy3.feedbackGeneratorModule = "^.nrDlFbGen3";
        nrPhy3.handoverControllerModule = "^.rrc.nrHandoverController3";
        nrDlFbGen3.isNr = true;
        nrDlFbGen3.macNodeId = parent.nrMacNodeId3;

    gates:
        input nrRadioIn3 @loose;    // the third leg's radio input

    submodules:
        nrRlcMux3: <default("RlcMux")> like IRlcMux {
            @display("p=830,430;is=s");
        }
        nrMac3: <default("NrMacUe")> like ILteMac {
            @display("p=830,510");
        }
        nrPhy3: <default("NrPhyUe")> like ILtePhy {
            @display("p=830,600");
        }
        nrChannelModel3[numNrCarriers]: <nrChannelModelType> like ILteChannelModel {
            @display("p=80,60,col;g=left");
        }
        nrDlFbGen3: LteDlFeedbackGenerator {
            @display("p=80,60,col;g=left");
            phyModule = "^.nrPhy3";
        }

    connections:
        nrRlcMux3.macOut --> nrMac3.upperLayerIn;
        nrRlcMux3.macIn <-- nrMac3.upperLayerOut;
        nrMac3.phyOut --> nrPhy3.upperGateIn;
        nrMac3.phyIn <-- nrPhy3.upperGateOut;
        nrRadioIn3 --> nrPhy3.radioIn;
}
