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

#ifndef _SIDELINK_SLMCSTABLE_H_
#define _SIDELINK_SLMCSTABLE_H_

namespace simu5g {

/**
 * Sidelink MCS -> (TBS, CQI) mapping (design decision D15): a self-contained
 * abstraction of the TS 38.214 §5.1.3 link-adaptation math, replacing the
 * SL-1 tbSize/mcs-as-CQI stubs.
 *
 *  - MCS index (MCS table 1, TS 38.214 table 5.1.3.1-1, indices 0..28) ->
 *    modulation order Qm and code rate R
 *  - TBS per TX occasion via the §5.1.3.2 N_info algorithm (single layer,
 *    one codeword), re-hosted on the shared nInfoToTbs[] table of NrMcs.cc;
 *    RE/PRB = 12 subcarriers x (14 - overheadSymbols), capped at 156 like
 *    the Uu procedure. The overhead default (see NrSlMacUe.overheadSymbols)
 *    abstracts AGC + TX/RX guard + PSCCH + DMRS symbols.
 *  - a coarse static MCS->CQI map feeding the existing per-CQI BLER curves:
 *    the largest CQI of TS 38.214 table 7.2.3-1 whose spectral efficiency
 *    does not exceed the MCS's (documented approximation until SL-specific
 *    BLER curves exist).
 *
 * Pure static functions, no module or kernel dependencies (unit-tested in
 * the D13 suite).
 */
class SlMcsTable
{
  public:
    static constexpr unsigned int MAX_MCS = 28;

    struct Entry {
        unsigned int qm;         // modulation order (bits per RE)
        double codeRateX1024;    // code rate R x 1024
    };

    /// MCS table 1 entry for the given MCS index (throws on out-of-range)
    static const Entry& entry(unsigned int mcs);

    /// transport block size [bits] for one TX occasion over numPrbs PRBs
    static unsigned int tbsBits(unsigned int mcs, unsigned int numPrbs, unsigned int overheadSymbols);

    /// transport block size [bytes] (tbsBits / 8, rounded down)
    static unsigned int tbsBytes(unsigned int mcs, unsigned int numPrbs, unsigned int overheadSymbols);

    /// coarse CQI equivalent of the MCS, for the per-CQI BLER lookup (1..15)
    static unsigned int cqi(unsigned int mcs);

    /// the TS 38.214 §5.1.3.2 TBS quantization (exposed for unit tests)
    static unsigned int computeTbsFromNinfo(double nInfo, double codeRate);
};

} // namespace simu5g

#endif
