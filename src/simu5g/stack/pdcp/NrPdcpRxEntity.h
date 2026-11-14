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

#ifndef _NRPDCPRXENTITY_H_
#define _NRPDCPRXENTITY_H_

#include <map>

#include "simu5g/stack/pdcp/LtePdcpRxEntity.h"
#include "simu5g/stack/dcX2Forwarder/DcX2Forwarder.h"
#include "simu5g/common/timer/TTimer.h"

namespace simu5g {

/*
 * Descriptor of the entity status
 * Taken from 3GPP TS 38.323
 */
struct PdcpRxWindowDesc
{
  public:

    //! Size of the reception window
    unsigned int windowSize_ = 16;
    //! SN of the next SDU expected to be received (RX_NEXT)
    unsigned int rxNext_;
    //! SN of the first SDU not delivered to the upper layer (RX_DELIV)
    unsigned int rxDeliv_;
    //! SN of the SDU following the SDU that triggered t_reordering (RX_REORD)
    unsigned int rxReord_;

    void clear(unsigned int i = 0)
    {
        rxNext_ = i;
        rxDeliv_ = i;
        rxReord_ = i;
    }

    PdcpRxWindowDesc()
    {
        clear();
    }

};

inline std::ostream& operator<<(std::ostream& os, const PdcpRxWindowDesc& w)
{
    os << "windowSize=" << w.windowSize_ << " rxNext=" << w.rxNext_
       << " rxDeliv=" << w.rxDeliv_ << " rxReord=" << w.rxReord_;
    return os;
}

enum PdcpRxTimerType
{
    REORDERING_T = 0
};

/**
 * @brief Entity for New Radio PDCP Layer
 *
 * This is the PDCP entity of LTE/NR Stack.
 *
 */
class NrPdcpRxEntity : public LtePdcpRxEntity
{
  protected:

    // outOfOrderDelivery (TS 38.323): deliver each SDU on arrival instead of in COUNT order
    bool outOfOrderDelivery_;

    // the reception buffer, keyed by COUNT: the stored SDUs not yet delivered, and (as nullptr)
    // the COUNTs above RX_DELIV that were delivered out of order
    std::map<unsigned int, inet::Packet *> reorderingBuffer_;

    // State variables
    PdcpRxWindowDesc rxWindowDesc_;

    // Timer to manage the reordering of the PDUs
    TTimer t_reordering_ = TTimer(this);

    // Timeout for the above timer
    double timeout_;

    // handler for PDCP SDU
    void handlePdcpSdu(Packet *pdcpSdu, unsigned int sequenceNumber) override;

    // deliver the stored SDUs with consecutive COUNTs from RX_DELIV, advancing RX_DELIV past them
    virtual void deliverConsecutiveSdus();

  public:
    ~NrPdcpRxEntity() override;

    void initialize(int stage) override;

    void handleMessage(cMessage *msg) override;

    bool isEmpty() const override;
};

} //namespace

#endif

