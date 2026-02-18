//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sdap.common;

//
// QfiContextManager maintains mappings between QFIs (QoS Flow Identifiers),
// CIDs (Connection IDs), and their associated QoS parameters.
//
// It can load QFI-DRB configurations from a file and provides lookup
// functions for QoS-aware scheduling and SDAP layer operations.
//
simple QfiContextManager like IQfiContextManager
{
    parameters:
        @display("i=block/table2");
        @class(QfiContextManager);
        string qfiContextFile = default("");  // Path to QFI context configuration file
}
