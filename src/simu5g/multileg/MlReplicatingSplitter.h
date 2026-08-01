//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLREPLICATINGSPLITTER_H_
#define _MULTILEG_MLREPLICATINGSPLITTER_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/stack/pdcp/PdcpLegSplitter.h"
#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

/**
 * @brief FRER-style replicating TX-side leg dispatcher.
 *
 * Sends a copy of every PDCP PDU on EVERY connected leg instead of picking
 * one, rewriting each copy's source/destination to that leg's node ids. The
 * PDCP SN is assigned upstream (once per SDU), so all copies carry the same
 * SN and the receiver's reorder window discards the duplicates -- the
 * elimination half of FRER needs no code of its own.
 */
class MlReplicatingSplitter : public PdcpLegSplitter
{
  protected:
    // Registered in initialize(), NOT as file-scope statics: a static registration
    // in this project's translation units would allocate signal ids at library load
    // time and shift the ids of the stock signals, which reorders result recording
    // and moves the sz fingerprint of every stock simulation.
    omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    omnetpp::simsignal_t replicaSentSignal_;

    inet::ModuleRefByPar<MlBinder> binder_;

    // this node's own id per leg, and whether this node is a UE
    std::vector<MacNodeId> legNodeIds_;
    bool isUe_ = false;

    // the legs to replicate onto (bearer legs 0..numLegs-1 map to these stack legs)
    std::vector<int> stackLegs_;

    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

    // Rewrite a replica's ids to those of the given bearer leg.
    virtual void adaptIdsForLeg(inet::Packet *pkt, int legIdx);
};

} // namespace simu5g

#endif
