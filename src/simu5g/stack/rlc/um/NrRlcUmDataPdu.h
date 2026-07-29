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

#ifndef STACK_RLC_UM_NRRLCUMDATAPDU_H_
#define STACK_RLC_UM_NRRLCUMDATAPDU_H_
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
namespace simu5g {

class NrRlcUmDataPdu : public LteRlcDataPdu {
private:
    void copy(const NrRlcUmDataPdu& other) {

        snoMainPacket=other.snoMainPacket;
        startOffset=other.startOffset;
        endOffset=other.endOffset;
        lengthMainPacket = other.lengthMainPacket;
    }
protected:


    unsigned int snoMainPacket;
    unsigned int startOffset;
    unsigned int endOffset;
    unsigned int lengthMainPacket;

public:
    NrRlcUmDataPdu();
    NrRlcUmDataPdu(const NrRlcUmDataPdu& other) : LteRlcDataPdu(other)
    {
        copy(other);
    }

    NrRlcUmDataPdu& operator=(const NrRlcUmDataPdu& other)
    {
        if (&other == this)
            return *this;
        LteRlcDataPdu::operator=(other);
        copy(other);

        return *this;
    }

    NrRlcUmDataPdu *dup() const override
    {
        return new NrRlcUmDataPdu(*this);
    }

    virtual ~NrRlcUmDataPdu();


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
Register_Class(NrRlcUmDataPdu);
} /* namespace simu5g */

#endif /* STACK_RLC_UM_NRRLCUMDATAPDU_H_ */
