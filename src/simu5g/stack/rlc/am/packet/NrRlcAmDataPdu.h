//
//                  Simu5G
//
// Authors: Esteban Egea Lopez (Universidad Politecnica de Cartagena)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_RLC_AM_PACKET_NRRLCAMDATAPDU_H_
#define STACK_RLC_AM_PACKET_NRRLCAMDATAPDU_H_
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"

namespace simu5g {


class NrRlcAmDataPdu: public LteRlcDataPdu {
private:
    void copy(const NrRlcAmDataPdu& other) {
        pollStatus_ = other.pollStatus_;
        snoMainPacket=other.snoMainPacket;
        startOffset=other.startOffset;
        endOffset=other.endOffset;
        lengthMainPacket = other.lengthMainPacket;
    }
protected:

    bool pollStatus_;
    unsigned int snoMainPacket;
    unsigned int startOffset;
    unsigned int endOffset;
    unsigned int lengthMainPacket;

public:
    NrRlcAmDataPdu();
    NrRlcAmDataPdu(const NrRlcAmDataPdu& other) : LteRlcDataPdu(other)
    {
        copy(other);
    }

    NrRlcAmDataPdu& operator=(const NrRlcAmDataPdu& other)
    {
        if (&other == this)
            return *this;
        LteRlcDataPdu::operator=(other);
        copy(other);

        return *this;
    }

    NrRlcAmDataPdu *dup() const override
    {
        return new NrRlcAmDataPdu(*this);
    }

    virtual ~NrRlcAmDataPdu();
    void setPollStatus(bool p) { pollStatus_ = p; }
    bool getPollStatus() const { return pollStatus_; }

    unsigned int  getSnoMainPacket() const {return snoMainPacket;}
    void setSnoMainPacket(unsigned int n) { snoMainPacket=n;}

    unsigned int getEndOffset() const {
        return endOffset;
    }

    void setEndOffset(unsigned int endOffset) {
        this->endOffset = endOffset;
    }

    unsigned int getLengthMainPacket() const {
        return lengthMainPacket;
    }

    void setLengthMainPacket(unsigned int lengthMainPacket) {
        this->lengthMainPacket = lengthMainPacket;
    }

    unsigned int getStartOffset() const {
        return startOffset;
    }

    void setStartOffset(unsigned int startOffset) {
        this->startOffset = startOffset;
    }
};
Register_Class(NrRlcAmDataPdu);
} /* namespace simu5g */

#endif /* STACK_RLC_AM_PACKET_NRRLCAMDATAPDU_H_ */
