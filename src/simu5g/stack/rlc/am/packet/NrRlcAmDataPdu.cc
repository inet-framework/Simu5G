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

#include "NrRlcAmDataPdu.h"

namespace simu5g {

NrRlcAmDataPdu::NrRlcAmDataPdu() : LteRlcDataPdu() {
    // this should be RLC_HEADER_AM
    this->setChunkLength(inet::B(RLC_HEADER_AM));
}

NrRlcAmDataPdu::~NrRlcAmDataPdu() {
    // TODO Auto-generated destructor stub
}

} /* namespace simu5g */
