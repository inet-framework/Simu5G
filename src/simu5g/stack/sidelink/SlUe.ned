//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sidelink;

import simu5g.nodes.NrUe;

//
// An out-of-coverage sidelink UE (design decision D14): a ~NrUe with the
// sidelink leg enabled and no serving cell. The Uu legs are present but
// dormant (servingNodeId and nrServingNodeId already default to 0 = none).
//
module SlUe extends NrUe
{
    parameters:
        hasSidelink = default(true);
        @display("i=device/pocketpc");
}
