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

#include "NrRlcUmDataPdu.h"

namespace simu5g {

NrRlcUmDataPdu::NrRlcUmDataPdu() {
    this->setChunkLength(inet::B(RLC_HEADER_UM));
}

NrRlcUmDataPdu::~NrRlcUmDataPdu() {
    // TODO Auto-generated destructor stub
}

} /* namespace simu5g */
