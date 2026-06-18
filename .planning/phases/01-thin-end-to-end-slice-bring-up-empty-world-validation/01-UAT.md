---
status: complete
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
source: [01-VERIFICATION.md]
started: 2026-06-17
updated: 2026-06-18
---

## Current Test

[testing complete]

## Tests

### 1. In-sim path gain = Sionna table value
expected: rcvdSinrDl (or logged getAttenuation) tracks the offline friis_check.py value within ~1 dB and is NOT the analytic 3GPP path loss. Operator observed rcvdSinrDl:mean = 70.14 dB during the 01-04 run, consistent with -83.3604 dB Sionna path gain.
result: pass
evidence: committed 0.sca (ed360465) — SionnaSingleLink.ue[0].cellularNic.nrChannelModel[0] rcvdSinrDl:mean = 70.13959 dB; measuredSinrDl:mean = 56.42577 dB. Tracks the -83.3604 dB Sionna table value, not the ~97.5 dB analytic 3GPP attenuation. Operator confirmed pass.

### 2. Corrupted-manifest negative check aborts with cRuntimeError
expected: Editing manifest carrier_frequency_hz to a wrong value and re-running aborts nonzero with a message naming the mismatch.
result: pass
evidence: run log — "Error: Sionna manifest carrier_frequency_hz mismatch: artifact 2.6e+09 Hz, scenario 3.5e+09 Hz -- in module (simu5g::SionnaManager) SionnaSingleLink.sionnaManager (id=6), during network initialization". Aborts nonzero, no silent fallback. Operator confirmed pass.

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
