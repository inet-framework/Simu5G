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


package simu5g.stack.rlc.um;

//
// Generic transmission side of the Unacknowledged Mode (UM) of the RLC layer,
// parametrized for LTE or NR via the soFraming flag (one C++ class, UmTxEntity):
//  - LTE (soFraming=false, TS 36.322): FI framing + concatenation, SN per PDU.
//  - NR  (soFraming=true,  TS 38.322): SI + byte-offset (SO) segmentation, SN per SDU.
// The NR parametrization is provided by the NrUmTxEntity profile. Select via
// BearerManagement.rlcUmTxEntityModuleType. Note: soFraming is the wire-format
// selector and is independent of isNR (which BearerManagement sets to mark the
// NR leg, and which does NOT change the wire format).
//
simple UmTxEntity like simu5g.stack.rlc.IRlcTxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        bool isNR @mutable = default(false);             // NR-leg marker (set programmatically by BearerManagement; not the wire format)
        bool soFraming = default(false);                 // wire format: LTE FI/concat (false) vs NR SI/SO (true)
        string macModule = default("^.^.mac");  // set by the enclosing RlcUmEntityBase compound (per leg)
        int queueSize @unit(B) = default(2MiB);          // RLC TX entity SDU queue size (LTE mode; 0: unlimited)
        int fragmentSize @unit(B) = default(30B);        // accepted for config-compat
        int sn_FieldLength = default(10);                // UM SN field length in bits (NR mode): LTE 5/10, NR 6/12
        @signal[rlcPduCreated];

    gates:
        input in;       // SDUs from upper mux (PDCP)
        input macIn;    // SDU requests from lower mux (MAC)
        output out;     // PDUs and new-data notifications to lower mux (MAC)
}
