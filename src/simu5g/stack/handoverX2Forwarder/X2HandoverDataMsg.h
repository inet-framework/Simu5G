//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_X2HANDOVERDATAMSG_H_
#define _LTE_X2HANDOVERDATAMSG_H_

#include "simu5g/x2/packet/LteX2Message.h"
#include "simu5g/common/LteCommon.h"

namespace simu5g {

/**
 * Class derived from LteX2Message
 * It defines the message that encapsulates the datagram to be exchanged between Handover managers
 */
class X2HandoverDataMsg : public LteX2Message
{
  protected:
    // QoS flow of the forwarded datagram (QFI_NONE on bearers without one, i.e. EPC).
    // In 3GPP the forwarding tunnel carries the QFI too (TS 38.425); the receiving
    // side restores it as the QfiReq tag, which the target's SDAP needs to map the
    // datagram back onto a DRB. Carried as a C++ field of the header chunk, like
    // GtpUserMsg's qfi; it does not add to the message length.
    Qfi qfi_ = QFI_NONE;

  public:

    X2HandoverDataMsg() :
        LteX2Message()
    {
        type = X2_HANDOVER_DATA_MSG;
    }

    X2HandoverDataMsg(const X2HandoverDataMsg& other) : LteX2Message() { operator=(other); }

    X2HandoverDataMsg& operator=(const X2HandoverDataMsg& other)
    {
        if (&other == this)
            return *this;
        LteX2Message::operator=(other);
        qfi_ = other.qfi_;
        return *this;
    }

    X2HandoverDataMsg *dup() const override { return new X2HandoverDataMsg(*this); }

    Qfi getQfi() const { return qfi_; }
    void setQfi(Qfi qfi) { qfi_ = qfi; }

};

} //namespace

#endif

